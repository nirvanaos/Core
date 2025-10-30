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
#include "RequestOut.h"
#include "OutgoingRequests.h"
#include "DomainRemote.h"
#include "remarshal_output.h"
#include <Port/Debugger.h>

using namespace Nirvana;
using namespace Nirvana::Core;

namespace CORBA {

using namespace Internal;

namespace Core {

std::atomic <RequestOut::IdGenType> RequestOut::last_id_;

RequestOut::RequestId RequestOut::get_new_id (IdPolicy id_policy) noexcept
{
	IdGenType id;
	switch (id_policy) {
		case IdPolicy::ANY:
			id = last_id_.fetch_add (1) + 1;
			break;

		case IdPolicy::EVEN: {
			IdGenType last = last_id_.load (std::memory_order_relaxed);
			do {
				id = last + 2 - (last & 1);
			} while (!last_id_.compare_exchange_weak (last, id));
		} break;

		case IdPolicy::ODD: {
			IdGenType last = last_id_.load (std::memory_order_relaxed);
			do {
				id = last + 1 + (last & 1);
			} while (!last_id_.compare_exchange_weak (last, id));
		} break;
	}
	return (RequestId)id;
}

RequestOut::RequestOut (unsigned response_flags, Domain& target_domain,
	const Internal::Operation& metadata, ReferenceRemote* ref) :
	RequestGIOP (target_domain.GIOP_minor (), response_flags, &target_domain),
	metadata_ (metadata),
	ref_ (ref),
	id_ (0),
	status_ (Status::IN_PROGRESS),
	request_id_offset_ (0),
	system_exception_code_ (SystemException::EC_NO_EXCEPTION),
	system_exception_data_{ 0, COMPLETED_NO }
{
	ExecDomain& ed = ExecDomain::current ();
	if (response_flags & RESPONSE_EXPECTED)
		deadline_ = ed.deadline ();
	else
		deadline_ = ed.get_request_deadline (true);

	if (INFINITE_DEADLINE == deadline_)
		deadline_ = Nirvana::Core::Chrono::make_deadline (DEFAULT_TIMEOUT);
}

RequestOut::~RequestOut ()
{}

void RequestOut::write_header (const IOP::ObjectKey& object_key, IOP::ServiceContextList& context)
{
	stream_out_->write_message_header (GIOP_minor_, GIOP::MsgType::Request);

	if (GIOP_minor_ <= 1) {
		GIOP::RequestHeader_1_1 hdr;

		// Calculate request id offset
		size_t off = sizeof (GIOP::RequestHeader_1_1) + 4;
		for (const IOP::ServiceContext& ctx : context) {
			off += 8 + ctx.context_data ().size ();
		}
		request_id_offset_ = off;

		hdr.service_context ().swap (context);
		hdr.request_id (id_);
		hdr.response_expected (response_flags_ & RESPONSE_EXPECTED);
		hdr.object_key (object_key);
		hdr.operation (metadata_.name);
		Type <GIOP::RequestHeader_1_1>::marshal_in (hdr, _get_ptr ());
		hdr.service_context ().swap (context);
	} else {
		GIOP::RequestHeader_1_2 hdr;
		request_id_offset_ = sizeof (GIOP::RequestHeader_1_2);
		hdr.request_id (id_);
		hdr.response_flags ((Octet)(response_flags_ & (RESPONSE_EXPECTED | RESPONSE_DATA)));
		hdr.target ().object_key (object_key);
		hdr.operation (metadata_.name);
		hdr.service_context ().swap (context);
		Type <GIOP::RequestHeader_1_2>::marshal_in (hdr, _get_ptr ());
		hdr.service_context ().swap (context);

		// In GIOP version 1.2 and 1.3, the Request Body is always aligned on an 8-octet boundary.
		size_t unaligned = stream_out_->size () % 8;
		if (unaligned) {
			Octet zero [8] = { 0 };
			stream_out_->write_c (1, 8 - unaligned, zero);
		}
	}
}

void RequestOut::id (RequestId id)
{
	if (request_id_offset_) {
		size_t offset = request_id_offset_;
		Octet* hdr = (Octet*)stream_out_->header (offset + 4);
		*(uint32_t*)(hdr + offset) = id;
	}
	id_ = id;
}

void RequestOut::set_reply (unsigned status, IOP::ServiceContextList&& context,
	servant_reference <StreamIn>&& stream)
{
	switch (status_ = (Status)status) {
		case Status::NO_EXCEPTION:
			// If target domain does not support DGC, we make passed DGC references as owned to it.
			if (!marshaled_DGC_references_.empty () && !(domain ()->flags () & Domain::GARBAGE_COLLECTION))
				static_cast <DomainRemote&> (*domain ()).add_DGC_objects (marshaled_DGC_references_);

			if (!(response_flags_ & RESPONSE_DATA)) {
				// Data is not expected
				finalize ();
				break;
			}
			stream_in_ = std::move (stream);

			finalize ();
			break;

		case Status::USER_EXCEPTION:
		case Status::SYSTEM_EXCEPTION:
			if (response_flags_ & RESPONSE_EXPECTED)
				stream_in_ = std::move (stream);
			finalize ();
			break;

		default:
			throw UNKNOWN (); // Unexpected reply
	}
}

bool RequestOut::marshal_op ()
{
	return true;
}

void RequestOut::pre_invoke (IdPolicy id_policy)
{
	if (!stream_out_)
		throw BAD_INV_ORDER ();
	Base::invoke ();
	bool response = (response_flags_ & RESPONSE_EXPECTED) != 0;
	if (!response && !marshaled_DGC_references_.empty ()) {
		// For oneway request with DGC references we have to wait response before the references releasing.
		// Offset of the response_flags is immediately after the request_id
		size_t offset = request_id_offset_ + 4;
		Octet* hdr = (Octet*)stream_out_->header (offset + 1);
		assert (0 == hdr [offset]);
		hdr [offset] = 1;
		response = true;
	}
	if (metadata_.context.size != 0) {
		IDL::Sequence <IDL::String> context;
		ExecDomain::current ().get_context (metadata_.context.p, metadata_.context.size, context);
		Type <IDL::Sequence <IDL::String> >::marshal_out (context, _get_ptr ());
	}
	if (response) {
		OutgoingRequests::new_request (*this, id_policy);

#ifndef NDEBUG
		if (!Nirvana::Core::Port::Debugger::is_debugger_present ())
#endif
		timer_.set (std::max (Nirvana::Core::Chrono::deadline_to_time (
			deadline_ - Nirvana::Core::Chrono::deadline_clock ()), (int64_t)MIN_TIMEOUT), id (), memory ().heap ());
	} else
		OutgoingRequests::new_request_oneway (*this, id_policy);
}

void RequestOut::set_exception (Any& e)
{
	throw BAD_INV_ORDER ();
}

void RequestOut::set_system_exception (SystemException::Code code, uint32_t minor, CompletionStatus completed) noexcept
{
	status_ = Status::SYSTEM_EXCEPTION;
	system_exception_code_ = code;
	system_exception_data_.minor = minor;
	system_exception_data_.completed = completed;

	finalize ();
}

void RequestOut::success ()
{
	throw BAD_INV_ORDER ();
}

bool RequestOut::cancel_internal ()
{
	if (!(response_flags_ & RESPONSE_EXPECTED))
		throw BAD_INV_ORDER ();

	if (CORBA::Core::OutgoingRequests::remove_request (id_)) {
		status_ = Status::CANCELLED;
		return true;
	}
	return false;
}

bool RequestOut::get_exception (Any& e)
{
	uint32_t unknown_minor = 0;
	switch (status_) {
	case Status::NO_EXCEPTION:
		return false;

	case Status::SYSTEM_EXCEPTION: {
		const ExceptionEntry* ee = nullptr;
		if (SystemException::EC_NO_EXCEPTION != system_exception_code_)
			ee = SystemException::_get_exception_entry (system_exception_code_);
		else {
			IDL::String id;
			stream_in_->read_string (id);
			stream_in_->read (4, 8, 8, 1, &system_exception_data_);
			if (stream_in_->other_endian ())
				Type <SystemException::_Data>::byteswap (system_exception_data_);

			ee = SystemException::_get_exception_entry (id);
			if (!ee) {
				ee = SystemException::_get_exception_entry (SystemException::EC_UNKNOWN);
				system_exception_data_.minor = MAKE_OMG_MINOR (2); // Non-standard System Exception not supported.
			}
		}
		std::aligned_storage <sizeof (SystemException), alignof (SystemException)>::type buf;
		(ee->construct) (&buf);
		SystemException& se = reinterpret_cast <SystemException&> (buf);
		se.minor (system_exception_data_.minor);
		se.completed (system_exception_data_.completed);
		unmarshal_end (true);
		e <<= se;
	}
	return true;

	case Status::USER_EXCEPTION: {
		IDL::String id;
		stream_in_->read_string (id);
		TypeCode::_ptr_type tc;
		for (auto p = metadata_.user_exceptions.p, end = p + metadata_.user_exceptions.size; p != end;
			++p)
		{
			TypeCode::_ptr_type tcp = (*p) ();
			if (RepId::compatible (tcp->id (), id)) {
				tc = tcp;
				break;
			}
		}
		if (tc) {
			Type <Any>::unmarshal (tc, _get_ptr (), e);
			unmarshal_end (true);
			return true;
		} else
			unknown_minor = MAKE_OMG_MINOR (1); // Unlisted user exception received by client.
	} break;

	default:
		assert (false);
		// If status_ is other than NO_EXCEPTION, SYSTEM_EXCEPTION or USER_EXCEPTION
		// we return UNKNOWN with minor = 0
	}
	unmarshal_end (true);
	e <<= UNKNOWN (unknown_minor);
	return true;
}

class RequestOut::Timeout : public Runnable
{
public:
	Timeout (RequestId id) :
		id_ (id)
	{}

	virtual void run () noexcept override
	{
		OutgoingRequests::set_system_exception (id_, TIMEOUT (MAKE_OMG_MINOR (3)));
	}

private:
	RequestId id_;
};

void RequestOut::Timer::signal () noexcept
{
	try {
		ExecDomain::async_call <Timeout> (Nirvana::Core::Chrono::make_deadline (timeout_),
			g_core_free_sync_context, (Heap*)heap_, id_);
	} catch (...) {}
}

}
}
