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
#include "Domain.h"
#include "../Binder.h"
#include "POA_Root.h"
#include <CORBA/Proxy/ProxyBase.h>

using namespace Nirvana;
using namespace Nirvana::Core;

namespace CORBA {
using namespace Internal;
namespace Core {

const Operation Domain::op_heartbeat_ = {"FT_HB"};
const Operation Domain::DGC_Request::operation_ = {"ping"};

Domain::Domain (unsigned flags, TimeBase::TimeT request_latency, TimeBase::TimeT heartbeat_interval,
	TimeBase::TimeT heartbeat_timeout) :
	flags_ (flags),
	last_ping_in_time_ (Nirvana::Core::Chrono::steady_clock ()),
	last_ping_out_time_ (Nirvana::Core::Chrono::steady_clock ()),
	request_latency_ (request_latency),
	heartbeat_interval_ (heartbeat_interval),
	heartbeat_timeout_ (heartbeat_timeout),
	DGC_scheduled_ (false)
{}

Domain::~Domain ()
{}

void Domain::_add_ref () NIRVANA_NOEXCEPT
{
	ref_cnt_.increment ();
}

void Domain::_remove_ref () NIRVANA_NOEXCEPT
{
	if (!ref_cnt_.decrement_seq ()) {
		SyncContext& sc = Binder::singleton ().sync_domain ();
		if (&SyncContext::current () == &sc)
			destroy ();
		else
			GarbageCollector::schedule (*this, sc);
	}
}

void Domain::add_owned_objects (IDL::Sequence <IOP::ObjectKey>& keys, ReferenceLocalRef* objs)
{
	PortableServer::Core::POA_Root::get_DGC_objects (keys, objs);
	for (auto it = keys.begin (); it != keys.end (); ++it, ++objs) {
		bool first = local_objects_.empty ();
		local_objects_.emplace (std::move (*it), std::move (*objs));
		if (first)
			_add_ref ();
	}
}

void Domain::erase_remote_key (DGC_RefKey& key) noexcept
{
	if (remote_objects_.erase (key) && remote_objects_.empty ())
		_remove_ref ();
}


void Domain::confirm_DGC_references (size_t cnt, ReferenceRemoteRef* refs)
{
	if (cnt) {
		ReferenceRemoteRef* end = refs + cnt;
		std::sort (refs, end,
			[](const ReferenceRemoteRef& l, const ReferenceRemoteRef& r)
			{ return &l->domain () < &r->domain (); });

		SYNC_BEGIN (Binder::sync_domain (), nullptr)
			try {
				call_DGC_function (&confirm_DGC_ref_start, refs, end, false);
				call_DGC_function (&confirm_DGC_ref_finish, refs, end, false);
			} catch (...) {
				call_DGC_function (&confirm_DGC_ref_finish, refs, end, true);
				throw;
			}
		SYNC_END ()
	}
}

void Domain::call_DGC_function (DGC_Function func, const ReferenceRemoteRef* begin,
	const ReferenceRemoteRef* end, bool no_throw)
{
	do {
		Domain* domain = &(*begin)->domain ();
		const ReferenceRemoteRef* domain_end;
		for (domain_end = begin + 1; domain_end != end; ++domain_end) {
			if (&(*domain_end)->domain () != domain) {
				break;
			}
		}
		(domain->*func) (begin, domain_end, no_throw);
		begin = domain_end;
	} while (begin != end);
}

void Domain::confirm_DGC_ref_start (const ReferenceRemoteRef* begin,
	const ReferenceRemoteRef* end, bool)
{
	DGC_RequestRef rq;
	for (const ReferenceRemoteRef* ref = begin; ref != end; ++ref) {
		DGC_RefKey* key = (*ref)->DGC_key ();
		assert (key);
		key->complete_deletion ();
		if (!key->request ()) {
			if (!rq)
				rq = DGC_RequestRef::create <DGC_Request> (std::ref (*this));
			rq->add (*key);
		}
	}

	// If new request was created and we have keys to delete, add them to request.
	if (rq)
		append_del (*rq);

	// Invoke new request, if any
	if (rq)
		rq->invoke ();
}

void Domain::confirm_DGC_ref_finish (const ReferenceRemoteRef* begin,
	const ReferenceRemoteRef* end, bool no_throw)
{
	// Complete all requests
	for (const ReferenceRemoteRef* ref = begin; ref != end; ++ref) {
		DGC_RefKey* key = (*ref)->DGC_key ();
		assert (key);
		if (key->request ())
			key->request ()->complete (no_throw);
		else if (!no_throw)
			key->check ();
		assert (no_throw || key->added ());
	}
}

void Domain::append_del (DGC_Request& rq)
{
	while (!remote_objects_del_.empty ()) {
		DGC_RefKey& key = remote_objects_del_.front ();
		key.complete_addition ();
		if (key.added ())
			rq.del (key);
		key.remove ();
	}
}

void Domain::schedule_del () noexcept
{
	if (!DGC_scheduled_) {
		DGC_scheduled_ = true;
		DeadlineTime deadline =
			PROXY_GC_DEADLINE == INFINITE_DEADLINE ?
			INFINITE_DEADLINE : Chrono::make_deadline (PROXY_GC_DEADLINE);
		try {
			ExecDomain::async_call <GC> (deadline, Binder::sync_domain (), nullptr, std::ref (*this));
		} catch (...) {
			DGC_scheduled_ = false;
			// TODO: Log
		}
	}
}

inline void Domain::send_del ()
{
	if (!remote_objects_del_.empty ()) {
		try {
			DGC_RequestRef rq = DGC_RequestRef::create <DGC_Request> (std::ref (*this));
			append_del (*rq);
			rq->invoke ();
			rq->complete ();
		} catch (...) {
			// TODO: Log
		}
	}
}

void Domain::GC::run ()
{
	domain_->DGC_scheduled_ = false;
	domain_->send_del ();
}

Domain::DGC_Request::DGC_Request (Domain& domain) :
	domain_ (domain),
	request_ (domain.create_request (IOP::ObjectKey (), operation_,
		IORequest::RESPONSE_EXPECTED | IOReference::REQUEST_ASYNC)),
	add_cnt_ (0)
{
}

void Domain::DGC_Request::invoke ()
{
	request_->marshal_seq_begin (add_cnt_);
	auto it = keys_.begin ();
	auto add_end = it + add_cnt_;
	try {
		for (; it != add_end; ++it) {
			Internal::Type <IOP::ObjectKey>::marshal_in (**it, request_);
			(*it)->request (static_cast <DGC_Request&> (*this));
		}
		request_->marshal_seq_begin (keys_.size () - add_cnt_);
		for (; it != keys_.end (); ++it) {
			Internal::Type <IOP::ObjectKey>::marshal_in (**it, request_);
			(*it)->request (static_cast <DGC_Request&> (*this));
		}
		request_->invoke ();
	} catch (...) {
		while (it != keys_.begin ()) {
			(*--it)->request_failed ();
			if (it >= add_end)
				domain_.remote_objects_del_.push_back (**it);
		}
		throw;
	}
}

void Domain::DGC_Request::complete (bool no_throw)
{
	DGC_RequestRef hold (this);
	request_->wait (std::numeric_limits <uint64_t>::max ());
	if (!keys_.empty ()) { // Not yet completed
		try {
			Internal::ProxyRoot::check_request (request_);
		} catch (...) {
			std::exception_ptr ex = std::current_exception ();
			for (DGC_RefKey* key : keys_) {
				assert (key->request () == this);
				key->request_failed ();
			}
			for (auto it = keys_.begin () + add_cnt_; it != keys_.end (); ++it) {
				domain_.remote_objects_del_.push_back (**it);
			}
			if (!no_throw)
				throw;
		}
		for (DGC_RefKey* key : keys_) {
			assert (key->request () == this);
			key->request_completed ();
		}
		for (auto it = keys_.begin () + add_cnt_; it != keys_.end (); ++it) {
			DGC_RefKey* key = *it;
			if (!key->reference_cnt ())
				domain_.erase_remote_key (*key);
		}
		keys_.clear ();
		add_cnt_ = 0;
	}
}

}
}
