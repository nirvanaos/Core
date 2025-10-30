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
#include "ESIOP.h"
#include "RequestInESIOP.h"
#include "StreamInSM.h"
#include "OutgoingRequests.h"
#include "IncomingRequests.h"
#include "../Runnable.h"
#include "../Chrono.h"
#include "../Binder.h"
#include "../SysManager.h"

using namespace CORBA;
using namespace CORBA::Core;

using namespace Nirvana;
using namespace Nirvana::Core;

namespace ESIOP {

/// Receive request Runnable
class ReceiveRequest :
	public Runnable
{
public:
	ReceiveRequest (const Request& msg) noexcept :
		data_ ((void*)msg.GIOP_message),
		timestamp_ (Nirvana::Core::Chrono::deadline_clock ()),
		client_id_ (msg.client_domain),
		request_id_ (msg.request_id)
	{}

private:
	virtual void run () noexcept override;
	virtual void on_crash (const siginfo& signal) noexcept override;

private:
	void* data_;
	uint64_t timestamp_;
	ProtDomainId client_id_;
	uint32_t request_id_;
};

void ReceiveRequest::run () noexcept
{
	try {
		// Create input stream
		servant_reference <StreamIn> in;
		try {
			in = servant_reference <StreamIn>::create <ImplDynamic <StreamInSM> > (data_);
		} catch (...) {
			// Not enough memory?
			// Create and destruct object in stack to release stream memory.
			ImplStatic <StreamInSM> tmp (data_);
			throw;
		}

		// Read GIOP message header
		GIOP::MessageHeader_1_1 msg_hdr;
		in->read_message_header (msg_hdr);

		// We do not use GIOP 1.0 in ESIOP.
		assert ((msg_hdr.GIOP_version ().major () == 1) && (msg_hdr.GIOP_version ().minor () > 0));
		assert (GIOP::MsgType::Request == (GIOP::MsgType)msg_hdr.message_type ());
		assert ((msg_hdr.flags () & 2) == 0); // Framentation is not allowed in ESIOP.

		// In the ESIOP we do not use the message size to allow > 4GB data transferring.
		// in->set_size (msg_hdr.message_size ());

		// Create and receive the request
		unsigned minor = msg_hdr.GIOP_version ().minor ();
		servant_reference <CORBA::Core::RequestIn> rq = servant_reference <CORBA::Core::RequestIn>::create <RequestInESIOP> (
			client_id_, minor, std::move (in));
		IncomingRequests::receive (std::move (rq), timestamp_);
	} catch (const SystemException& ex) {
		if (request_id_) {
			// Responce expected
			ReplySystemException reply (request_id_, ex);
			send_error_message (client_id_, &reply, sizeof (reply));
		}
	}
}

void ReceiveRequest::on_crash (const siginfo& signal) noexcept
{
	if (request_id_) {
		// Responce expected
		Exception::Code ec;
		if (signal.si_excode != Exception::EC_NO_EXCEPTION)
			ec = (Exception::Code)signal.si_excode;
		else
			ec = SystemException::EC_UNKNOWN;
		ReplySystemException reply (request_id_, ec);
		send_error_message (client_id_, &reply, sizeof (reply));
	}
}

/// Cancel request Runnable
class ReceiveCancel :
	public Runnable
{
public:
	ReceiveCancel (const CancelRequest& msg) noexcept :
		timestamp_ (Nirvana::Core::Chrono::deadline_clock ()),
		client_id_ (msg.client_domain),
		request_id_ (msg.request_id)
	{}

private:
	virtual void run () noexcept override;

private:
	uint64_t timestamp_;
	ProtDomainId client_id_;
	uint32_t request_id_;
};

void ReceiveCancel::run () noexcept
{
	IncomingRequests::cancel (RequestKey (client_id_, request_id_), timestamp_);
}

/// Receive reply Runnable
class ReceiveReply :
	public Runnable
{
public:
	ReceiveReply (const Reply& msg) noexcept :
		data_ ((void*)msg.GIOP_message),
		request_id_ (msg.request_id)
	{}

private:
	virtual void run () noexcept override;

private:
	void* data_;
	uint32_t request_id_;
};

void ReceiveReply::run () noexcept
{
	try {
		// Create input stream
		servant_reference <StreamIn> in;
		try {
			in = servant_reference <StreamIn>::create <ImplDynamic <StreamInSM> > (data_);
		} catch (...) {
			// Not enough memory?
			// Create and destruct object in stack to release stream memory.
			ImplStatic <StreamInSM> tmp (data_);
			throw;
		}

		// Read GIOP message header
		GIOP::MessageHeader_1_1 msg_hdr;
		in->read_message_header (msg_hdr);

		// We do not use GIOP 1.0 in ESIOP.
		assert ((msg_hdr.GIOP_version ().major () == 1) && (msg_hdr.GIOP_version ().minor () > 0));
		assert (GIOP::MsgType::Reply == (GIOP::MsgType)msg_hdr.message_type ());
		assert ((msg_hdr.flags () & 2) == 0); // Framentation is not allowed in ESIOP.

		// In the ESIOP we do not use the message size to allow > 4GB data transferring.
		// in->set_size (msg_hdr.message_size ());

		// Create and receive the request
		unsigned minor = msg_hdr.GIOP_version ().minor ();
		OutgoingRequests::receive_reply (minor, std::move (in));
	} catch (const SystemException& ex) {
		OutgoingRequests::set_system_exception (request_id_, ex);
	}
}

/// Receive reply immediate Runnable
class ReceiveReplyImmediate :
	public Runnable
{
public:
	ReceiveReplyImmediate (const ReplyImmediate& msg) noexcept :
		request_id_ (msg.request_id),
		size_and_flag_ (msg.flags)
	{
		real_copy (msg.data, msg.data + size (), data_);
	}

	size_t size () const noexcept
	{
		if (PLATFORMS_ENDIAN_DIFFERENT)
			return size_and_flag_ & ~MessageHeader::FLAG_LITTLE_ENDIAN;
		else
			return size_and_flag_;
	}

	unsigned little_endian () const noexcept
	{
		return size_and_flag_ & MessageHeader::FLAG_LITTLE_ENDIAN;
	}

	const uint8_t* data () const noexcept
	{
		return data_;
	}

private:
	virtual void run () noexcept override;

private:
#if UINTPTR_MAX <= UINT16_MAX
	uint16_t size_and_flag_;
#endif
	uint32_t request_id_;
	alignas (8) uint8_t data_ [ReplyImmediate::MAX_DATA_SIZE];
#if UINTPTR_MAX > UINT16_MAX
	uint16_t size_and_flag_;
#endif
};

class StreamInImmediate :
	public StreamInEncap,
	public SharedObject
{
protected:
	StreamInImmediate (ReceiveReplyImmediate& runnable) :
		StreamInEncap (data_, data_ + runnable.size (), true)
	{
		real_copy (runnable.data (), runnable.data () + runnable.size (), data_);
		if (PLATFORMS_ENDIAN_DIFFERENT)
			little_endian (runnable.little_endian ());
	}

private:
	alignas (8) uint8_t data_ [ReplyImmediate::MAX_DATA_SIZE];
};

void ReceiveReplyImmediate::run () noexcept
{
	OutgoingRequests::receive_reply_immediate (request_id_,
		servant_reference <StreamIn>::create <ImplDynamic <StreamInImmediate> > (std::ref (*this)));
}

/// Receive system exception Runnable
class ReceiveSystemException :
	public Runnable
{
public:
	ReceiveSystemException (const ReplySystemException& msg) noexcept :
		completed_ ((CompletionStatus)msg.completed),
		code_ (msg.code),
		minor_ (msg.minor),
		request_id_ (msg.request_id)
	{}

private:
	virtual void run () noexcept override;

private:
	CompletionStatus completed_;
	Exception::Code code_;
	uint32_t minor_;
	uint32_t request_id_;
};

void ReceiveSystemException::run () noexcept
{
	OutgoingRequests::set_system_exception (request_id_, code_, minor_, completed_);
}

/// Receive close connection Runnable
class ReceiveCloseConnection :
	public Runnable
{
public:
	ReceiveCloseConnection (const CloseConnection& msg) noexcept :
		domain_id_ (msg.sender_domain)
	{}

private:
	virtual void run () noexcept override;

private:
	ESIOP::ProtDomainId domain_id_;
};

void ReceiveCloseConnection::run () noexcept
{
	Nirvana::Core::Binder::singleton ().remote_references ().close_connection (domain_id_);
}

/// Receive shutdown Runnable
class ReceiveShutdownSys : public Nirvana::Core::SysManager::ReceiveShutdown
{
	typedef Nirvana::Core::SysManager::ReceiveShutdown Base;

public:
	ReceiveShutdownSys (Nirvana::Core::SysManager::AsyncCallContext&& ctx,
		const Shutdown& msg) noexcept :
		Base (std::move (ctx), 0),
		security_context_ (msg.security_context)
	{}

private:
	virtual void run () noexcept override;

private:
	Security::Context security_context_;
};

void ReceiveShutdownSys::run () noexcept
{
	try {
		ExecDomain::set_impersonation_context (std::move (security_context_));
	} catch (...) {
		assert (false);
	}

	Base::run ();
}

void dispatch_message (MessageHeader& message) noexcept
{
	switch (message.message_type) {
		case MessageType::REQUEST: {
			const auto& msg = Request::receive (message);
			try {
				ExecDomain::async_call <ReceiveRequest> (
					Nirvana::Core::Chrono::make_deadline (INITIAL_REQUEST_DEADLINE_LOCAL), g_core_free_sync_context, nullptr,
					std::ref (msg));
			} catch (const SystemException& ex) {
				// async_call failed.
				// Create and destruct object in stack to release stream memory.
				{
					ImplStatic <StreamInSM> tmp ((void*)msg.GIOP_message);
				}
				// Highly likely we are out of resources here, so we shouldn't use the asyncronous call.
				ReplySystemException reply (msg.request_id, ex);
				send_error_message (msg.client_domain, &reply, sizeof (reply));
			}
		} break;

		case MessageType::REPLY: {
			const auto& msg = Reply::receive (message);
			try {
				ExecDomain::async_call <ReceiveReply> (
					Nirvana::Core::Chrono::make_deadline (INITIAL_REQUEST_DEADLINE_LOCAL), g_core_free_sync_context, nullptr,
					std::ref (msg));
			} catch (const SystemException& ex) {
				// async_call failed.
				// Create and destruct object in stack to release stream memory.
				{
					ImplStatic <StreamInSM> tmp ((void*)msg.GIOP_message);
				}
				OutgoingRequests::set_system_exception (msg.request_id, ex);
			}
		} break;

		case MessageType::REPLY_IMMEDIATE: {
			const auto& msg = ReplyImmediate::receive (message);
			try {
				ExecDomain::async_call <ReceiveReplyImmediate> (
					Nirvana::Core::Chrono::make_deadline (INITIAL_REQUEST_DEADLINE_LOCAL), g_core_free_sync_context, nullptr,
					std::ref (msg));
			} catch (const SystemException& ex) {
				OutgoingRequests::set_system_exception (msg.request_id, ex);
			}
		} break;

		case MessageType::REPLY_SYSTEM_EXCEPTION: {
			const auto& msg = ReplySystemException::receive (message);
			try {
				ExecDomain::async_call <ReceiveSystemException> (
					Nirvana::Core::Chrono::make_deadline (INITIAL_REQUEST_DEADLINE_LOCAL), g_core_free_sync_context, nullptr,
					std::ref (msg));
			} catch (...) {
				OutgoingRequests::set_system_exception (msg.request_id, msg.code, msg.minor,
					(CompletionStatus)msg.completed);
			}
		} break;

		case MessageType::CANCEL_REQUEST: {
			const auto& msg = CancelRequest::receive (message);
			try {
				ExecDomain::async_call <ReceiveCancel> (
					Nirvana::Core::Chrono::make_deadline (CANCEL_REQUEST_DEADLINE), g_core_free_sync_context, nullptr,
					std::ref (msg));
			} catch (...) {
			}
		} break;

		case MessageType::SHUTDOWN: {
			const auto& msg = Shutdown::receive (message);
			if (!Security::is_valid_context (msg.security_context))
				break;
			if (is_system_domain ()) {
				try {
					Nirvana::Core::SysManager::async_call <ReceiveShutdownSys> (INFINITE_DEADLINE, std::ref (msg));
				} catch (...) {
				}
			} else {
				Security::Context tmp (msg.security_context);
				Scheduler::shutdown (msg.flags ());
			}
		} break;

		case MessageType::CLOSE_CONNECTION: {
			const auto& msg = CloseConnection::receive (message);
			try {
				ExecDomain::async_call <ReceiveCloseConnection> (
					Nirvana::Core::Chrono::make_deadline (CLOSE_CONNECTION_DEADLINE), Nirvana::Core::Binder::sync_domain (),
					nullptr, std::ref (msg));
			} catch (...) {
			}
		} break;

		default:
			assert (false);
	}
}

ProtDomainId get_prot_domain_id (const IOP::ObjectKey& object_key)
{
	assert (!Nirvana::Core::SINGLE_DOMAIN);
	if (object_key.size () <= sizeof (ProtDomainId))
		throw CORBA::OBJECT_NOT_EXIST (MAKE_OMG_MINOR (2));
	ProtDomainId id = *(const ProtDomainId*)object_key.data ();

	// If platform endians are different, always use big endian format.
	if (PLATFORMS_ENDIAN_DIFFERENT && endian::native == endian::little)
		Nirvana::byteswap (id);
	return id;
}

}
