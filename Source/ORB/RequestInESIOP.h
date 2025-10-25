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
#ifndef NIRVANA_ORB_CORE_REQUESTINESIOP_H_
#define NIRVANA_ORB_CORE_REQUESTINESIOP_H_
#pragma once

#include "RequestIn.h"
#include "ESIOP.h"
#include "system_services.h"
#include "IDL/ORB/CSI.h"

namespace CORBA {
namespace Core {

/// ESIOP incoming request.
class RequestInESIOP : public RequestIn
{
	typedef RequestIn Base;

public:
	RequestInESIOP (ESIOP::ProtDomainId client_id, unsigned GIOP_minor,
		CORBA::servant_reference <CORBA::Core::StreamIn>&& in) :
		Base (client_id, GIOP_minor)
	{
		final_construct (std::move (in));

		if (!Nirvana::Core::SINGLE_DOMAIN && !object_key_.empty ()
			&& !(ESIOP::is_system_domain ()
				&& CORBA::Core::system_service (object_key_) < CORBA::Core::Services::SERVICE_COUNT)) {

			if (ESIOP::get_prot_domain_id (object_key_) != ESIOP::current_domain_id ())
				throw CORBA::OBJECT_NOT_EXIST (MAKE_OMG_MINOR (2));
			ESIOP::erase_prot_domain_id (object_key_);
		}

		Nirvana::Core::Security::Context security_context;
		auto context = binary_search (service_context_, IOP::SecurityAttributeService);
		if (context) {
			Nirvana::Core::ImplStatic <StreamInEncap> encap (std::ref (context->context_data ()));
			CSI::MsgType msg_type;
			encap.read_one (msg_type);
			if (CSI::MTMessageInContext == msg_type) {
				CSI::MessageInContext msg_body;
				encap.read_one (msg_body);
				if (encap.end ())
					throw BAD_PARAM ();
				if (msg_body.client_context_id () <=
					std::numeric_limits <Nirvana::Core::Security::Context::ABI>::max ()) {

					Nirvana::Core::Security::Context::ABI sc = (Nirvana::Core::Security::Context::ABI)
						msg_body.client_context_id ();

					if (Nirvana::Core::Security::is_valid_context (sc))
						security_context = Nirvana::Core::Security::Context (sc);
				}
			}
		}

		if (security_context.empty ())
			throw NO_PERMISSION ();

		Nirvana::Core::ExecDomain::set_impersonation_context (std::move (security_context));
	}

protected:
	virtual CORBA::servant_reference <CORBA::Core::StreamOut> create_output () override;
	virtual void set_exception (CORBA::Any& e) override;
	virtual void success () override;
};

}
}

#endif
