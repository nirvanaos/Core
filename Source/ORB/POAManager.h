/// \file
/*
* Nirvana Core.
*
* This is a part of the Nirvana project.
*
* Author: Igor Popov
*
* Copyright (c) 2021 Igor Popov.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation; either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*
* Send comments and/or bug reports to:
*  popov.nirvana@gmail.com
*/
#ifndef NIRVANA_ORB_CORE_POAMANAGER_H_
#define NIRVANA_ORB_CORE_POAMANAGER_H_
#pragma once

#include "POA_Base.h"
#include <queue>

namespace PortableServer {
namespace Core {

class POAManagerFactory;

class POAManager :
	public IDL::String,
	public CORBA::servant_traits <PortableServer::POAManager>::Servant <POAManager>
{
public:
	inline
	POAManager (POAManagerFactory& factory, const IDL::String& id, const CORBA::PolicyList& policies);

	~POAManager ()
	{
		NIRVANA_TRACE ("~POAManager ()");
	}

	CORBA::Core::PolicyMapShared* policies () const noexcept
	{
		return policies_;
	}

	inline void _delete_object () noexcept;

	void activate ()
	{
		switch (state_) {
			case State::HOLDING:
				state_ = State::ACTIVE;
				if (!queue_.empty ()) {
					Nirvana::Core::SyncContext& sc = CORBA::Core::local2proxy (_this ())->sync_context ();
					do {
						const QElem& top = queue_.top ();
						auto& request = *top.request;
						if (!request.is_cancelled ()) {
							POA_Base& adapter = *top.adapter;
							if (adapter.destroy_called ())
								request.set_exception (CORBA::OBJ_ADAPTER (MAKE_OMG_MINOR (1)));
							else {
								adapter.on_request_start ();
								try {
									Nirvana::Core::ExecDomain::async_call <ServeRequest> (
										top.deadline, sc, top.request->heap (), std::ref (top));
								} catch (CORBA::Exception& e) {
									top.request->set_exception (std::move (e));
									adapter.on_request_finish ();
								}
							}
						}
						queue_.pop ();
					} while (!queue_.empty ());
				}
				break;

			case State::DISCARDING:
				state_ = State::ACTIVE;
#ifdef NIRVANA_C17
				[[fallthrough]];
#endif
			case State::ACTIVE:
				break;

			default:
				throw AdapterInactive ();
		}
	}

	void hold_requests (bool wait_for_completion)
	{
		if (State::INACTIVE == state_)
			throw AdapterInactive ();

		if (wait_for_completion)
			POA_Base::check_wait_completion ();

		state_ = State::HOLDING;

		if (wait_for_completion)
			requests_completed_.wait ();
	}

	void discard_requests (bool wait_for_completion)
	{
		if (State::INACTIVE == state_)
			throw AdapterInactive ();

		if (wait_for_completion)
			POA_Base::check_wait_completion ();

		if (state_ != State::DISCARDING) {
			state_ = State::DISCARDING;
			discard_queued_requests ();
		}

		if (wait_for_completion)
			requests_completed_.wait ();
	}

	void deactivate (bool etherealize_objects, bool wait_for_completion)
	{
		if (wait_for_completion)
			POA_Base::check_wait_completion ();

		if (State::INACTIVE != state_) {
			state_ = State::INACTIVE;
			discard_queued_requests ();
			if (etherealize_objects)
				for (auto p : associated_adapters_) {
					p->deactivate_objects (true);
				}
		}

		if (wait_for_completion)
			requests_completed_.wait ();
	}

	State get_state () const noexcept
	{
		return state_;
	}

	const IDL::String& get_id () const noexcept
	{
		return *this;
	}

	static POAManager* get_implementation (const CORBA::Core::ServantProxyLocal* proxy)
	{
		if (proxy) {
			POAManager* impl = static_cast <POAManager*> (
				static_cast <Bridge <CORBA::LocalObject>*> (&proxy->servant ()));
			if (impl->signature_ == SIGNATURE)
				return impl;
			else
				throw CORBA::INV_OBJREF (); // User try to create own POAManager implementation?
		}
		return nullptr;
	}

	void invoke (POA_Base& adapter, Request& request)
	{
		switch (state_) {
			case State::HOLDING:
				queue_.emplace (adapter, request);
				break;
			case State::ACTIVE:
				adapter.on_request_start ();
				try {
					adapter.serve_request (request);
				} catch (...) {
					adapter.on_request_finish ();
					throw;
				}
				adapter.on_request_finish ();
				break;
			case State::DISCARDING:
				throw CORBA::TRANSIENT (MAKE_OMG_MINOR (1));
				break;
			default:
				throw CORBA::OBJ_ADAPTER (MAKE_OMG_MINOR (1));
		}
	}

	void on_adapter_create (POA_Base& adapter)
	{
		associated_adapters_.push_back (&adapter);
	}

	void on_adapter_destroy (POA_Base& adapter) noexcept
	{
		// POA are created and destroyed rarely so we use simple linear search.
		associated_adapters_.erase (
			std::find (associated_adapters_.begin (), associated_adapters_.end (), &adapter));
	}

	void on_request_start () noexcept
	{
		if (1 == ++request_cnt_)
			requests_completed_.reset ();
	}

	void on_request_finish () noexcept
	{
		if (0 == --request_cnt_)
			requests_completed_.signal ();
	}

private:
	void discard_queued_requests ();

private:
	struct QElem
	{
		Nirvana::DeadlineTime deadline;
		CORBA::servant_reference <POA_Base> adapter;
		RequestRef request;

		QElem (POA_Base& a, Request& r) :
			deadline (Nirvana::Core::ExecDomain::current ().deadline ()),
			adapter (&a),
			request (&r)
		{}

		bool operator < (const QElem& rhs) const noexcept
		{
			return deadline < rhs.deadline;
		}
	};

	class ServeRequest : public Nirvana::Core::Runnable
	{
	public:
		ServeRequest (const QElem& qelem) :
			adapter_ (qelem.adapter),
			request_ (qelem.request)
		{}

	private:
		virtual void run () noexcept override;
		virtual void on_crash (const siginfo& signal) noexcept override;

	private:
		CORBA::servant_reference <POA_Base> adapter_;
		RequestRef request_;
	};

	CORBA::servant_reference <POAManagerFactory> factory_;
	CORBA::servant_reference <CORBA::Core::PolicyMapShared> policies_;
	std::vector <POA_Base*, Nirvana::Core::UserAllocator <POA_Base*> > associated_adapters_;
	std::priority_queue <QElem, std::vector <QElem, Nirvana::Core::UserAllocator <QElem> > > queue_;
	State state_;
	unsigned int request_cnt_;
	Nirvana::Core::EventSync requests_completed_;
	static const int32_t SIGNATURE = 'POAM';
	const int32_t signature_;
};

inline
PortableServer::POAManager::_ref_type POA_Base::the_POAManager () const
{
	check_exist ();

	return the_POAManager_->_this ();
}

inline
void POA_Base::invoke (Request& request)
{
	if (destroy_called_)
		throw CORBA::OBJ_ADAPTER (MAKE_OMG_MINOR (1));
	the_POAManager_->invoke (*this, request);
}

}
}

#endif
