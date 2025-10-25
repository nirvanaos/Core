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
#ifndef NIRVANA_ORB_CORE_REQUESTOUTESIOP_H_
#define NIRVANA_ORB_CORE_REQUESTOUTESIOP_H_
#pragma once

#include "RequestOut.h"
#include "StreamOutSM.h"
#include "OutgoingRequests.h"
#include "IDL/ORB/CSI.h"

namespace CORBA {
namespace Core {

class RequestOutESIOP : public RequestOut
{
	typedef RequestOut Base;

public:
	RequestOutESIOP (unsigned response_flags, Domain& target_domain,
		const IOP::ObjectKey& object_key, const Internal::Operation& metadata, ReferenceRemote* ref) :
		Base (response_flags, target_domain, metadata, ref)
	{
		if ((response_flags & 3) == 1)
			GIOP_minor_ = 2;

		security_context_ = domain ()->create_security_context ();
		
		stream_out_ = servant_reference <StreamOut>::create <Nirvana::Core::ImplDynamic <StreamOutSM> >
			(std::ref (static_cast <DomainProt&> (target_domain)), true);
		
		id (get_new_id (IdPolicy::ANY));
		
		IOP::ServiceContextList context;

		Nirvana::DeadlineTime dl = deadline ();
		if (Nirvana::INFINITE_DEADLINE != dl) {
			Nirvana::Core::ImplStatic <StreamOutEncap> encap;
			encap.write_one (dl);
			context.emplace_back ();
			context.back ().context_id (ESIOP::CONTEXT_ID_DEADLINE);
			context.back ().context_data (std::move (encap.data ()));
		}

		{
			CSI::MsgType msg_type = CSI::MTMessageInContext;
			CSI::MessageInContext msg_body (security_context_.abi (), true);

			Nirvana::Core::ImplStatic <StreamOutEncap> encap;
			encap.write_one (msg_type);
			encap.write_one (msg_body);
			context.emplace_back ();
			context.back ().context_id (IOP::SecurityAttributeService);
			context.back ().context_data (std::move (encap.data ()));
		}

		write_header (object_key, context);
	}

	DomainProt* domain () const noexcept
	{
		return static_cast <DomainProt*> (Base::domain ());
	}

protected:
	virtual void invoke () override;
	virtual void cancel () override;

private:
	Nirvana::Core::Security::Context security_context_;
};

}
}

#endif
