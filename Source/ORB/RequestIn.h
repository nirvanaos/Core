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
#ifndef NIRVANA_ORB_CORE_REQUESTIN_H_
#define NIRVANA_ORB_CORE_REQUESTIN_H_
#pragma once

#include "DomainAddress.h"
#include "RequestGIOP.h"
#include "RequestInPOA.h"
#include "PolicyMap.h"
#include "../ExecDomain.h"
#include "../Timer.h"

namespace CORBA {
namespace Core {

/// Unique id of an incoming request.
struct RequestKey : DomainAddress
{
	RequestKey (const DomainAddress& addr, uint32_t rq_id) :
		DomainAddress (addr),
		request_id (rq_id)
	{}

	RequestKey (const DomainAddress& addr) :
		DomainAddress (addr)
	{}

	bool operator < (const RequestKey& rhs) const noexcept
	{
		if (request_id < rhs.request_id)
			return true;
		else if (request_id > rhs.request_id)
			return false;
		else
			return DomainAddress::operator < (rhs);
	}

	uint32_t request_id;
};

/// Implements server-side IORequest for GIOP.
class NIRVANA_NOVTABLE RequestIn :
	public RequestGIOP,
	public RequestInPOA
{
protected:
	template <class T> friend class CORBA::servant_reference;

	// RequestInPOA
	virtual void _add_ref () noexcept override;
	virtual void _remove_ref () noexcept override;
	virtual Nirvana::Core::Heap* heap () const noexcept override;

public:
	const RequestKey& key () const noexcept
	{
		return key_;
	}

	/// \returns The request id.
	uint32_t request_id () const noexcept
	{
		return key_.request_id;
	}

	/// \returns Service context sorted in the ascending order.
	const IOP::ServiceContextList& service_context () const noexcept
	{
		return service_context_;
	}

	/// \returns Invocation policies.
	const PolicyMap& invocation_policies () const noexcept
	{
		return invocation_policies_;
	}

	virtual const IOP::ObjectKey& object_key () const noexcept override
	{
		return object_key_;
	}

	virtual Internal::StringView <Char> operation () const noexcept override
	{
		return operation_;
	}

	virtual void serve (const ServantProxyBase& proxy) override;

	/// Cancel the request.
	/// Called from the IncomingRequests class.
	/// Request is already removed from map on this call.
	virtual void cancel () noexcept override;
	virtual bool is_cancelled () const noexcept override;

	/// Return exception to caller.
	/// Operation has move semantics so \p e may be cleared.
	/// Must be overridden in the derived class to send reply.
	virtual void set_exception (Any& e) override = 0;

	/// Marks request as successful.
	/// Must be overridden in the derived class to send reply.
	virtual void success () override = 0;

	void inserted_to_map (void* iter) noexcept
	{
		map_iterator_ = iter;
	}

	/// Finalizes the request.
	/// 
	/// \returns `true` if the reply must be sent.
	bool finalize () noexcept;

protected:
	RequestIn (const DomainAddress& client, unsigned GIOP_minor);
	~RequestIn ();

	void final_construct (servant_reference <StreamIn>&& in);

	/// Output stream factory.
	/// Must be overridden in a derived class.
	/// 
	/// \returns The output stream reference.
	virtual servant_reference <StreamOut> create_output () = 0;

	void post_send_success () noexcept;

private:
	void get_object_key (const IOP::TaggedProfile& profile);

	// Caller operations throw BAD_OPERATION or just return `false`
	virtual void invoke () override;
	virtual bool get_exception (Any& e) override;

	virtual void unmarshal_end (bool no_check_size) override;
	virtual bool marshal_op () override;

	void switch_to_reply (GIOP::ReplyStatusType status = GIOP::ReplyStatusType::NO_EXCEPTION);

	void delayed_release () noexcept;

protected:
	RequestKey key_;
	IOP::ObjectKey object_key_;
	IOP::ServiceContextList service_context_;
	PolicyMap invocation_policies_;

private:
	class DelayedRelease : public Nirvana::Core::Runnable
	{
	public:
		DelayedRelease (RequestIn& request) :
			request_ (request)
		{}

	private:
		virtual void run () noexcept override;

	private:
		RequestIn& request_;
	};

	class DelayedReleaseTimer : public Nirvana::Core::Timer
	{
	public:
		DelayedReleaseTimer (RequestIn& request) :
			request_ (request)
		{}

	private:
		virtual void signal () noexcept override;

	private:
		RequestIn& request_;
	};

	IDL::String operation_;
	void* map_iterator_;

	/// SyncDomain of the target object to enter after unmarshal_end.
	servant_reference <Nirvana::Core::SyncDomain> sync_domain_after_unmarshal_;

	size_t reply_status_offset_;
	size_t reply_header_end_;
	// We should not use StaticallyAllocated here because the constructed_ value will be undefined.
	std::aligned_storage <sizeof (DelayedReleaseTimer), alignof (DelayedReleaseTimer)>::type delayed_release_timer_;
	std::atomic <bool> cancelled_;
	bool has_context_;
};

}
}

#endif
