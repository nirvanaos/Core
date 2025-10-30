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
#include "RequestIn.h"
#include "IncomingRequests.h"
#include "IDL/ORB/IIOP.h"
#include "ServantProxyObject.h"
#include "DomainRemote.h"
#include "tagged_seq.h"
#include "Messaging_policies.h"

using namespace Nirvana;
using namespace Nirvana::Core;

namespace CORBA {

using namespace Internal;

namespace Core {

RequestIn::RequestIn (const DomainAddress& client, unsigned GIOP_minor) :
	RequestGIOP (GIOP_minor, 0, nullptr),
	key_ (client),
	map_iterator_ (nullptr),
	reply_status_offset_ (0),
	reply_header_end_ (0),
	cancelled_ (false),
	has_context_ (false)
{}

void RequestIn::final_construct (servant_reference <StreamIn>&& in)
{
	stream_in_ = std::move (in);

	switch (GIOP_minor_) {
	case 0: {
		typedef GIOP::RequestHeader_1_0 Hdr;
		Hdr hdr;
		Type <Hdr>::unmarshal (_get_ptr (), hdr);
		key_.request_id = hdr.request_id ();
		object_key_ = std::move (hdr.object_key ());
		operation_ = std::move (hdr.operation ());
		service_context_ = std::move (hdr.service_context ());
		response_flags_ = hdr.response_expected () ? (RESPONSE_EXPECTED | RESPONSE_DATA) : 0;
	} break;

	case 1: {
		typedef GIOP::RequestHeader_1_1 Hdr;
		Hdr hdr;
		Type <Hdr>::unmarshal (_get_ptr (), hdr);
		key_.request_id = hdr.request_id ();
		object_key_ = std::move (hdr.object_key ());
		operation_ = std::move (hdr.operation ());
		service_context_ = std::move (hdr.service_context ());
		response_flags_ = hdr.response_expected () ? (RESPONSE_EXPECTED | RESPONSE_DATA) : 0;
	} break;

	default: {
		typedef GIOP::RequestHeader_1_2 Hdr;
		Hdr hdr;
		Type <Hdr>::unmarshal (_get_ptr (), hdr);
		key_.request_id = hdr.request_id ();
		operation_ = std::move (hdr.operation ());
		service_context_ = std::move (hdr.service_context ());
		response_flags_ = hdr.response_flags ();
		if ((response_flags_ & (RESPONSE_EXPECTED | RESPONSE_DATA)) == RESPONSE_DATA)
			throw INV_FLAG ();

		switch (hdr.target ()._d ()) {
		case GIOP::KeyAddr:
			object_key_ = std::move (hdr.target ().object_key ());
			break;

		case GIOP::ProfileAddr:
			get_object_key (hdr.target ().profile ());
			break;

		case GIOP::ReferenceAddr: {
			const GIOP::IORAddressingInfo& ior = hdr.target ().ior ();
			const IOP::TaggedProfileSeq& profiles = ior.ior ().profiles ();
			if (profiles.size () <= ior.selected_profile_index ())
				throw OBJECT_NOT_EXIST ();
			get_object_key (profiles [ior.selected_profile_index ()]);
		} break;
		}

		// In GIOP version 1.2 and 1.3, the Request Body is always aligned on an 8-octet boundary.
		size_t unaligned = stream_in_->position () % 8;
		if (unaligned)
			stream_in_->read (1, 1, 1, 8 - unaligned, nullptr);
	}
	}

	sort (service_context_);

	// Obtain invocation policies
	auto context = binary_search (service_context_, IOP::INVOCATION_POLICIES);
	if (context) {
		invocation_policies_.insert (context->context_data ());
		
		// Erase from context to conserve memory
		service_context_.erase (service_context_.begin () + (context - service_context_.data ()));
	}

	// Obtain the request deadline
	if (key_.family == DomainAddress::Family::ESIOP) {
		auto context = binary_search (service_context_, ESIOP::CONTEXT_ID_DEADLINE);
		if (context) {
			ImplStatic <StreamInEncap> encap (std::ref (context->context_data ()));
			DeadlineTime dl;
			encap.read_one (dl);
			if (encap.end () != 0)
				throw MARSHAL (StreamIn::MARSHAL_MINOR_MORE);
			deadline_ = dl;
		}
	} else {
		TimeBase::UtcT end_time;
		if (invocation_policies_.get_value <Messaging::REQUEST_END_TIME_POLICY_TYPE> (end_time))
			deadline_ = Nirvana::Core::Chrono::deadline_from_UTC (end_time);
		else {
			Messaging::PriorityRange priority_range;
			if (invocation_policies_.get_value <Messaging::REQUEST_PRIORITY_POLICY_TYPE> (priority_range))
				deadline_ = Nirvana::Core::Chrono::deadline_from_priority ((priority_range.min () + priority_range.max ()) / 2);
			else {
				context = binary_search (service_context_, IOP::RTCorbaPriority);
				if (context) {
					ImplStatic <StreamInEncap> encap (std::ref (context->context_data ()));
					int16_t priority;
					encap.read_one (priority);
					if (encap.end ())
						throw BAD_PARAM ();
					deadline_ = Nirvana::Core::Chrono::deadline_from_priority (priority);
				}
			}
		}
	}
}

void RequestIn::get_object_key (const IOP::TaggedProfile& profile)
{
	if (IOP::TAG_INTERNET_IOP == profile.tag ()) {
		ImplStatic <StreamInEncap> stm (ref (profile.profile_data ()));

		// Skip IIOP version
		stm.read (IDL::Type <IIOP::Version>::CDR_align, sizeof (IIOP::Version),
			IDL::Type <IIOP::Version>::CDR_size, 1, nullptr);

		// Skip host name
		size_t host_len = stm.read_size ();
		stm.read (1, 1, 1, host_len + 1, nullptr);

		// Skip port number
		stm.read (2, 2, 2, 1, nullptr);

		stm.read_seq (object_key_);
	}
	throw OBJECT_NOT_EXIST ();
}

RequestIn::~RequestIn ()
{}

void RequestIn::_add_ref () noexcept
{
	RequestGIOP::_add_ref ();
}

void RequestIn::_remove_ref () noexcept
{
	RequestGIOP::_remove_ref ();
}

Heap* RequestIn::heap () const noexcept
{
	return &RequestGIOP::memory ().heap ();
}

void RequestIn::switch_to_reply (GIOP::ReplyStatusType status)
{
	if (!stream_out_) {
		// Leave the object synchronization domain, if any.
		ExecDomain::current ().switch_to_free_sync_context ();
		stream_in_ = nullptr;
		if (response_flags_ & RESPONSE_EXPECTED) {
			stream_out_ = create_output ();
			stream_out_->write_message_header (GIOP_minor_, GIOP::MsgType::Reply);

			// Before marshaling the reply header we must set RESPONSE_DATA flag
			// to prevent bypass.
			unsigned flags = response_flags_;
			response_flags_ |= RESPONSE_DATA;
			try {
				if (GIOP_minor_ <= 1) {
					GIOP::ReplyHeader_1_0 hdr;
					// hdr.service_context (move (context_)); TODO: decide
					hdr.request_id (request_id ());
					hdr.reply_status (status);
					Type <GIOP::ReplyHeader_1_0>::marshal_out (hdr, _get_ptr ());
					reply_header_end_ = stream_out_->size ();
					reply_status_offset_ = reply_header_end_ - 4;
				} else {
					GIOP::ReplyHeader_1_2 hdr;
					// hdr.service_context (move (context_)); TODO: decide
					hdr.request_id (request_id ());
					hdr.reply_status (status);
					reply_status_offset_ = stream_out_->size () + 4;
					Type <GIOP::ReplyHeader_1_2>::marshal_out (hdr, _get_ptr ());
					reply_header_end_ = stream_out_->size ();
				}
			} catch (...) {
				response_flags_ = flags;
				throw;
			}
			response_flags_ = flags;
		}
	}
}

bool RequestIn::marshal_op ()
{
	switch_to_reply ();
	return (response_flags_ & RESPONSE_DATA) != 0;
}

void RequestIn::serve (const ServantProxyBase& proxy)
{
	if (is_cancelled ())
		return;

	Internal::OperationIndex op = proxy.find_operation (operation ());

	SyncContext* sync_context = &proxy.get_sync_context (op);
	SyncDomain* sync_domain = sync_context->sync_domain ();
	const Operation& op_md = proxy.operation_metadata (op);
	has_context_ = op_md.context.size != 0;

	if (sync_domain && (op_md.flags & Operation::FLAG_IN_CPLX)) {
		// Do not enter synchronization domain immediately.
		// We push memory context now and will enter sync_domain_
		// in unmarshal_end () when all input objects unmarshaled.
		ExecDomain& ed = ExecDomain::current ();
		SyncContext& ret_context = ed.sync_context ();
		ed.mem_context_push (&sync_domain->mem_context ());
		sync_domain_after_unmarshal_ = sync_domain;
		proxy.invoke (op, _get_ptr ());
		if (sync_domain_after_unmarshal_) {
			// Exception was occur before schedule_call_no_push_mem,
			// we must pop unused memory context from stack.
			sync_domain_after_unmarshal_ = nullptr;
			ed.mem_context_pop ();
		} else
			ed.schedule_return (ret_context);
	} else {
		// Enter the target synchronization context now.
		// We don't need to handle exceptions here, because invoke () does not throw exceptions.
		Nirvana::Core::Synchronized _sync_frame (*sync_context, sync_domain ? nullptr : heap ());

		// 
		if (!is_cancelled ())
			proxy.invoke (op, _get_ptr ());
	}
}

void RequestIn::unmarshal_end (bool no_check_size)
{
	if (has_context_) {
		IDL::Sequence <IDL::String> context;
		Type <IDL::Sequence <IDL::String> >::unmarshal (_get_ptr (), context);
		ExecDomain::current ().set_context (context);
	}

	RequestGIOP::unmarshal_end (no_check_size);

	// Here we must enter the target sync domain, if any.
	servant_reference <SyncDomain> sd;
	if ((sd = sync_domain_after_unmarshal_)) {
		try {
			ExecDomain::current ().schedule_call_no_push_mem (*sd);
		} catch (...) {
			sync_domain_after_unmarshal_ = sd;
			throw;
		}
	}
}

void RequestIn::success ()
{
	switch_to_reply ();
}

void RequestIn::set_exception (Any& e)
{
	TypeCode::_ptr_type type = e.type ();
	if (type->kind () != TCKind::tk_except)
		throw BAD_PARAM (MAKE_OMG_MINOR (21));

	GIOP::ReplyStatusType status = e.is_system_exception () ?
		GIOP::ReplyStatusType::SYSTEM_EXCEPTION
		:
		GIOP::ReplyStatusType::USER_EXCEPTION;

	if (!stream_out_)
		switch_to_reply (status);
	else {
		assert (reply_header_end_);
		stream_out_->rewind (reply_header_end_);
		*(GIOP::ReplyStatusType*)((Octet*)stream_out_->header (reply_header_end_) + reply_status_offset_) = status;
	}
	unsigned rf = response_flags_;
	if (rf & RESPONSE_EXPECTED) {
		response_flags_ |= RESPONSE_DATA; // To marshal exception data.
		try {
			stream_out_->write_string_c (type->id ());
			type->n_marshal_out (e.data (), 1, _get_ptr ());
		} catch (...) {}
		response_flags_ = rf;
	}
}

bool RequestIn::finalize () noexcept
{
	if (map_iterator_) {
		bool ret = IncomingRequests::finalize (map_iterator_);
		map_iterator_ = nullptr;
		return ret;
	}

	return false;
}

void RequestIn::cancel () noexcept
{
	// Oneway requests (response_flags_ == 0) are not cancellable.
	if (response_flags_ && !cancelled_.exchange (true, std::memory_order_release)) {
		response_flags_ = 0; // Prevent marshaling
		if (map_iterator_) {
			IncomingRequests::release_iterator (map_iterator_);
			map_iterator_ = nullptr;
		}
	}
}

bool RequestIn::is_cancelled () const noexcept
{
	return cancelled_.load (std::memory_order_acquire);
}

void RequestIn::invoke ()
{
	throw BAD_OPERATION ();
}

bool RequestIn::get_exception (Any& e)
{
	return false;
}

void RequestIn::post_send_success () noexcept
{
	if (!marshaled_DGC_references_.empty ()) {
		assert (domain_);
		if (!(domain_->flags () & Domain::GARBAGE_COLLECTION))
			static_cast <DomainRemote&> (*domain_).add_DGC_objects (marshaled_DGC_references_);
		else {
			// For DGC-enabled target domain we need to delay request release.
			// Caller must have enough time to confirm DGC references.
			try {
				new (&delayed_release_timer_) DelayedReleaseTimer (std::ref (*this));
			} catch (...) {
				return;
			}
			_add_ref ();
			try {
				((DelayedReleaseTimer*)&delayed_release_timer_)->set (0, domain_->request_latency () * 
					Domain::DGC_IN_REQUEST_DELAY_RELEASE_MUL, 0);
			} catch (...) {
				((DelayedReleaseTimer*)&delayed_release_timer_)->DelayedReleaseTimer::~DelayedReleaseTimer ();
				_remove_ref ();
				return;
			}
		}
	}
}

inline void RequestIn::delayed_release () noexcept
{
	((DelayedReleaseTimer*)&delayed_release_timer_)->DelayedReleaseTimer::~DelayedReleaseTimer ();
	_remove_ref ();
}

void RequestIn::DelayedReleaseTimer::signal () noexcept
{
	try {
		DeadlineTime deadline =
			GC_DEADLINE == INFINITE_DEADLINE ?
			INFINITE_DEADLINE : Nirvana::Core::Chrono::make_deadline (GC_DEADLINE);
		ExecDomain::async_call <DelayedRelease> (deadline, g_core_free_sync_context, nullptr, std::ref (request_));
	} catch (...) {
		assert (false);
		try {
			// Retry 5 seconds later
			set (0, TimeBase::SECOND * 5, 0);
		} catch (...) {
			assert (false);
		}
	}
}

void RequestIn::DelayedRelease::run () noexcept
{
	request_.delayed_release ();
}

}
}
