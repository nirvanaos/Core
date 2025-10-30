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
#include "ServantProxyBase.h"
#include "RequestLocal.h"

using namespace Nirvana::Core;

namespace CORBA {

using namespace Internal;

namespace Core {

class ServantProxyBase::GC :
	public Runnable
{
public:
	GC (Interface::_ptr_type servant) :
		servant_ (servant)
	{}

private:
	virtual void run () noexcept override;

private:
	Interface::_ptr_type servant_;
};

void ServantProxyBase::GC::run () noexcept
{
	collect_garbage (servant_);
}

ServantProxyBase::~ServantProxyBase ()
{
	assert (&Nirvana::Core::SyncContext::current () == &sync_context ());
/* Temporary disabled: assert (!servant_); */
	assert (0 == ref_cnt_);
}

RefCntProxy::IntegralType ServantProxyBase::add_ref ()
{
	RefCntProxy::IntegralType cnt = ref_cnt_.increment_seq ();
	try {
		if (1 == cnt) {
			if (servant_)
				add_ref_servant ();
		} else if (2 == cnt && component_)
			interface_duplicate (&component_);
	} catch (...) {
		ref_cnt_.decrement ();
		throw;
	}
	return cnt;
}

void ServantProxyBase::_remove_ref ()
{
	RefCntProxy::IntegralType cnt = ref_cnt_.decrement ();
	if (0 == cnt) {
		if (servant_)
			run_garbage_collector ();
		else {
			// This is not a normal situation.
			// Usually it means that the servant implementation has a bug with _add_ref()/_remove_ref() calls.
			assert (false);

			// To use correct heap we must enter the sync context.
			Nirvana::Core::Synchronized _sync_frame (*sync_context_, nullptr);

			delete this;
		}
	} else if (1 == cnt && component_)
		interface_release (&component_);
}

void ServantProxyBase::reset_servant () noexcept
{
	servant_ = nullptr;
	for (InterfaceEntry* ie = metadata_.interfaces.begin (); ie != metadata_.interfaces.end (); ++ie) {
		ie->implementation = nullptr;
	}
}

void ServantProxyBase::add_ref_servant () const
{
	// Called in the servant synchronization context on getting the first reference to proxy.
	if (&SyncContext::current () == sync_context_)
		interface_duplicate (&servant_);
	else {
		SYNC_BEGIN (*sync_context_, nullptr)
			interface_duplicate (&servant_);
		SYNC_END ()
	}
}

void ServantProxyBase::run_garbage_collector () const noexcept
{
	using namespace Nirvana::Core;

	if (Scheduler::async_GC_allowed ()) {
		try {
			Nirvana::DeadlineTime deadline =
				Nirvana::Core::GC_DEADLINE == Nirvana::INFINITE_DEADLINE ?
				Nirvana::INFINITE_DEADLINE : Nirvana::Core::Chrono::make_deadline (
					Nirvana::Core::GC_DEADLINE);

			ExecDomain::async_call <GC> (deadline, sync_context (), nullptr, servant_);
			return;
		} catch (...) {
			// Async call failed, maybe resources are exausted.
			// Fallback to collect garbage synchronous.
		}
	}
	if (&sync_context () != &SyncContext::current ()) {
		try {
			SYNC_BEGIN (sync_context (), nullptr)
				collect_garbage (servant_);
			SYNC_END ()
		} catch (...) {
				assert (false);
				// Swallow exceptions.
			}
	} else
		collect_garbage (servant_);
}

IORequest::_ref_type ServantProxyBase::create_request (OperationIndex op, unsigned flags,
	CallbackRef&& callback)
{
	if (is_local_object_op (op))
		return ProxyManager::create_request (op, flags, std::move (callback));

	check_create_request (op, flags);

	Heap* mem;
	SyncDomain* sd = sync_context ().sync_domain ();
	if (sd)
		mem = &sd->mem_context ().heap ();
	else
		mem = nullptr;

	if (callback) {
		if (!(flags & IORequest::RESPONSE_EXPECTED))
			throw BAD_PARAM ();
		return make_pseudo <RequestLocalImpl <RequestLocalAsync> > (std::ref (*this), op,
			mem, flags, std::move (callback));
	} else if (flags & IORequest::RESPONSE_EXPECTED) {
		return make_pseudo <RequestLocalImpl <RequestLocal> > (std::ref (*this), op,
			mem, flags);
	} else {
		return make_pseudo <RequestLocalImpl <RequestLocalOneway> > (std::ref (*this), op,
			mem, flags);
	}
}

Object::_ref_type ServantProxyBase::_get_component ()
{
	return component_;
}

}
}
