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
#ifndef NIRVANA_ORB_CORE_REQUESTOUTSYNC_H_
#define NIRVANA_ORB_CORE_REQUESTOUTSYNC_H_
#pragma once

#include "RequestOut.h"
#include <atomic>

namespace CORBA {
namespace Core {

class RequestOutSyncBase
{
protected:
	RequestOutSyncBase (RequestOut& base) :
		base_ (base),
		source_exec_domain_ (&Nirvana::Core::ExecDomain::current ()),
		wait_op_ (std::ref (*this)),
		state_ (State::INVOKE)
	{}

	void pre_invoke ();

	void post_invoke () noexcept
	{
		Nirvana::Core::ExecContext::run_in_neutral_context (wait_op_);
	}

	void post_finalize () noexcept;

	void post_unmarshal_end ()
	{
		// Here we must enter the target sync domain, if any.
		servant_reference <Nirvana::Core::SyncDomain> sd;
		if ((sd = std::move (sync_domain_after_unmarshal_)))
			source_exec_domain_->schedule_return (*sd);
	}

private:
	RequestOut& base_;

	// Source execution domain to resume.
	Nirvana::Core::ExecDomain* source_exec_domain_;

	// Source synchronization domain to enter after unmarshal.
	servant_reference <Nirvana::Core::SyncDomain> sync_domain_after_unmarshal_;

	class WaitOp : public Nirvana::Core::Runnable
	{
	public:
		WaitOp (RequestOutSyncBase& obj) :
			obj_ (obj)
		{}

	private:
		virtual void run () noexcept override;

	private:
		RequestOutSyncBase& obj_;
	};

	WaitOp wait_op_;

	enum class State
	{
		INVOKE,
		SUSPENDED,
		FINALIZED
	};

	std::atomic <State> state_;
};

/// \brief Synchronous outgoing request.
/// 
/// \typeparam Rq Base request class, derived from RequestOut.
template <class Rq>
class RequestOutSync :
	public Rq,
	public RequestOutSyncBase
{
	typedef Rq Base;

public:
	RequestOutSync (unsigned response_flags, Domain& target_domain,
		const IOP::ObjectKey& object_key, const Internal::Operation& metadata, ReferenceRemote* ref) :
		Base (response_flags, target_domain, object_key, metadata, ref),
		RequestOutSyncBase (static_cast <RequestOut&> (*this))
	{}

	virtual void invoke () override
	{
		RequestOutSyncBase::pre_invoke ();
		// Base::invoke () performs the actual send message.
		// SyncDomain is leaved here so message sending does not block other request processing.
		Base::invoke ();
		RequestOutSyncBase::post_invoke ();
	}

protected:
	virtual void finalize () noexcept override
	{
		Base::finalize ();
		RequestOutSyncBase::post_finalize ();
	}

	virtual void unmarshal_end (bool no_check_size) override
	{
		Base::unmarshal_end (no_check_size);
		RequestOutSyncBase::post_unmarshal_end ();
	}

};

}
}

#endif
