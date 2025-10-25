/// \file
/// Platform domain environment specific inter-ORB protocol.
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
#ifndef NIRVANA_ORB_CORE_ESIOP_H_
#define NIRVANA_ORB_CORE_ESIOP_H_
#pragma once

#include <CORBA/CORBA.h>
#include "IDL/ORB/IOP.h"
#include <Port/ESIOP.h>
#include <Port/config.h>
#include "../Security.h"

namespace CORBA {
namespace Core {
class StreamOutSM;
}
}

/// Environment-specific inter-ORB protocol.
/// Implements communication between protection domains
/// in one system domain.
namespace ESIOP {

inline bool is_system_domain () noexcept
{
	if (Nirvana::Core::SINGLE_DOMAIN)
		return true;
	else if (Nirvana::Core::BUILD_NO_SYS_DOMAIN)
		return false;
	else
		return sys_domain_id () == current_domain_id ();
}

/// Message type
enum MessageType
{
	REQUEST, ///< GIOP Request or LocateRequest message
	REPLY, ///< GIOP Reply message
	REPLY_IMMEDIATE, ///< GIOP Reply with small data
	REPLY_SYSTEM_EXCEPTION, ///< GIOP Reply with system exception
	CANCEL_REQUEST, ///< GIOP CancelRequest
	LOCATE_REPLY, ///< GIOP LocateReply - currently unused
	SHUTDOWN, ///< Shutdown domain
	CLOSE_CONNECTION, ///< Close connection

	/// Number of the ESIOP messages.
	/// Host system may add own message types.
	MESSAGES_CNT
};

/// A message header
struct MessageHeader
{
	uint8_t message_type;  ///< Message type
	uint8_t flags; ///< Flags

	static const uint8_t FLAG_LITTLE_ENDIAN = 0x80;

	MessageHeader (uint8_t type, uint8_t flags = 0) noexcept :
		message_type (type),
		flags ((PLATFORMS_ENDIAN_DIFFERENT && Nirvana::endian::native == Nirvana::endian::little ?
			FLAG_LITTLE_ENDIAN : 0) | flags)
	{}

	bool other_endian () const noexcept
	{
		if (PLATFORMS_ENDIAN_DIFFERENT)
			return (flags & FLAG_LITTLE_ENDIAN) ? Nirvana::endian::native != Nirvana::endian::little
			: Nirvana::endian::native == Nirvana::endian::little;
		else
			return false;
	}
};

/// GIOP Request
struct Request : MessageHeader
{
	/// The GIOP message in the recipient memory.
	SharedMemPtr GIOP_message;

	/// Sender protection domain id.
	ProtDomainId client_domain;

	/// The request id.
	uint32_t request_id;

	Request (ProtDomainId client, CORBA::Core::StreamOutSM& stream, uint32_t rq_id);

	static Request& receive (MessageHeader& hdr) noexcept
	{
		assert (hdr.message_type == MessageType::REQUEST);
		Request& msg = static_cast <Request&> (hdr);
		if (hdr.other_endian ()) {
			Nirvana::byteswap (msg.client_domain);
			Nirvana::byteswap (msg.request_id);
		}
		return msg;
	}
};

/// GIOP Reply
struct Reply : MessageHeader
{
	/// The GIOP message in the recipient memory.
	SharedMemPtr GIOP_message;

	/// The request id.
	uint32_t request_id;

	Reply (CORBA::Core::StreamOutSM& stream, uint32_t rq_id);

	static Reply& receive (MessageHeader& hdr) noexcept
	{
		assert (hdr.message_type == MessageType::REPLY);
		Reply& msg = static_cast <Reply&> (hdr);
		if (hdr.other_endian ())
			Nirvana::byteswap (msg.request_id);
		return msg;
	}
};

/// If GIOP Reply contains system exception, it can be sent without allocation of shared memory.
struct ReplySystemException : MessageHeader
{
	/// Completion status
	uint8_t completed;

	/// The exception code
	int16_t code;

	/// The exception minor code
	uint32_t minor;

	/// The request id
	uint32_t request_id;

	ReplySystemException (uint32_t rq_id, const CORBA::SystemException& ex) noexcept :
		MessageHeader (REPLY_SYSTEM_EXCEPTION),
		completed ((uint8_t)ex.completed ()),
		code ((int16_t)ex.__code ()),
		minor (ex.minor ()),
		request_id (rq_id)
	{}

	ReplySystemException (uint32_t rq_id, CORBA::SystemException::Code code) noexcept :
		MessageHeader (REPLY_SYSTEM_EXCEPTION),
		completed ((uint8_t)CORBA::CompletionStatus::COMPLETED_MAYBE),
		code ((int16_t)code),
		minor (0),
		request_id (rq_id)
	{}

	static ReplySystemException& receive (MessageHeader& hdr) noexcept
	{
		assert (hdr.message_type == MessageType::REPLY_SYSTEM_EXCEPTION);
		ReplySystemException& msg = static_cast <ReplySystemException&> (hdr);
		if (hdr.other_endian ()) {
			Nirvana::byteswap (msg.code);
			Nirvana::byteswap (msg.minor);
			Nirvana::byteswap (msg.request_id);
		}
		return msg;
	}
};

/// GIOP CancelRequest
struct CancelRequest : MessageHeader
{
	/// Sender protection domain id.
	ProtDomainId client_domain;

	/// The request id
	uint32_t request_id;

	CancelRequest (ProtDomainId sender, uint32_t rq_id) noexcept :
		MessageHeader (CANCEL_REQUEST),
		client_domain (sender),
		request_id (rq_id)
	{}

	static CancelRequest& receive (MessageHeader& hdr) noexcept
	{
		assert (hdr.message_type == MessageType::CANCEL_REQUEST);
		CancelRequest& msg = static_cast <CancelRequest&> (hdr);
		if (hdr.other_endian ()) {
			Nirvana::byteswap (msg.client_domain);
			Nirvana::byteswap (msg.request_id);
		}
		return msg;
	}
};

/// Close connection
struct CloseConnection : MessageHeader
{
	/// Sender protection domain id.
	ProtDomainId sender_domain;

	CloseConnection (ProtDomainId sender) noexcept :
		MessageHeader (CLOSE_CONNECTION),
		sender_domain (sender)
	{
	}

	static CloseConnection& receive (MessageHeader& hdr) noexcept
	{
		assert (hdr.message_type == MessageType::CLOSE_CONNECTION);
		CloseConnection& msg = static_cast <CloseConnection&> (hdr);
		if (hdr.other_endian ())
			Nirvana::byteswap (msg.sender_domain);
		return msg;
	}
};

struct Shutdown : MessageHeader
{
	Nirvana::Core::Security::Context::ABI security_context;

	Shutdown (Nirvana::Core::Security::Context::ABI sc, unsigned flags) noexcept :
		MessageHeader (SHUTDOWN, (uint8_t)flags),
		security_context (sc)
	{}

	static Shutdown& receive (MessageHeader& hdr) noexcept
	{
		assert (hdr.message_type == MessageType::SHUTDOWN);
		Shutdown& msg = static_cast <Shutdown&> (hdr);
		if (hdr.other_endian ())
			Nirvana::byteswap (msg.security_context);
		return msg;
	}

	unsigned flags () const noexcept
	{
		return MessageHeader::flags & 0x7F;
	}
};

/// The message buffer enough for any message
union MessageBuffer
{
	Request request;
	Reply reply;
	ReplySystemException system_exception;
	CancelRequest cancel_request;
	CloseConnection close_connection;
	Shutdown shutdown;
};

/// If reply data size is zero or small, the reply can be sent without
/// allocation of the shared memory.
struct ReplyImmediate : MessageHeader
{
	static const size_t MAX_DATA_SIZE = sizeof (MessageBuffer) - sizeof (MessageHeader) - sizeof (uint32_t);
	static_assert (MAX_DATA_SIZE <= 127, "MAX_DATA_SIZE <= 127");

	uint8_t data [MAX_DATA_SIZE];
	uint32_t request_id;

	ReplyImmediate (uint32_t rq_id, const void* p, size_t size) noexcept :
		MessageHeader (REPLY_IMMEDIATE, (uint8_t)size),
		request_id (rq_id)
	{
		assert (size <= MAX_DATA_SIZE);
		Nirvana::real_copy ((const uint8_t*)p, (const uint8_t*)p + size, data);
	}

	static ReplyImmediate& receive (MessageHeader& hdr) noexcept
	{
		assert (hdr.message_type == MessageType::REPLY_IMMEDIATE);
		ReplyImmediate& msg = static_cast <ReplyImmediate&> (hdr);
		if (hdr.other_endian ()) {
			Nirvana::byteswap (msg.request_id);
		}
		return msg;
	}
};

static_assert (sizeof (MessageBuffer) == sizeof (ReplyImmediate), "sizeof (MessageBuffer) == sizeof (ReplyImmediate)");

/// Message dispatch function.
/// Called by the postman from portability layer.
void dispatch_message (MessageHeader& message) noexcept;

/// Other domain platform properties.
struct PlatformSizes
{
	size_t allocation_unit; ///< Shared memory ALLOCATION_UNIT.
	size_t block_size; ///< Stream block size. >= allocation_unit.
	size_t sizeof_pointer; ///< sizeof (void*) on target platform.
	size_t sizeof_size; ///< sizeof (size_t) on target platform.
	size_t max_size; ///< maximal size_t value.
};

/// Nirvana ORB type. TODO: Obtain from OMG.
///
/// The TAG_ORB_TYPE component has an associated value of type unsigned long, encoded as a CDR
/// encapsulation, designating an ORB type ID allocated by the OMG for the ORB type of the
/// originating ORB. Anyone may register any ORB types by submitting a short (one - paragraph)
/// description of the ORB type to the OMG, and will receive a new ORB type ID in return.
/// A list of ORB type descriptions and values will be made available on the OMG web server.
const uint32_t ORB_TYPE = 0xFFFFFFFF;

/// Vendor service context ID. TODO: Obtain from OMG.
const uint32_t VSCID = 0xFFFFFF;

///@{ Service context IDs

/// Local deadline.
/// context_data contains the local DeadlineTime, 8 bytes.
/// Used only for calls inside the local system domain, so we don't have to register it with OMG.
const uint32_t CONTEXT_ID_DEADLINE = (VSCID << 8) | 0;

///@}

ProtDomainId get_prot_domain_id (const IOP::ObjectKey& object_key);

inline
void erase_prot_domain_id (IOP::ObjectKey& object_key)
{
	if (!Nirvana::Core::SINGLE_DOMAIN) {
		assert (get_prot_domain_id (object_key) == current_domain_id ());
		object_key.erase (object_key.begin (), object_key.begin () + sizeof (ProtDomainId));
	}
}

}

#endif
