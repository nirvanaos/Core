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
#include "SysManager.h"
#include "SysDomain.h"
#include "ORB/ORB.h"
#include "IDL/CoreDomains.h"

namespace Nirvana {
namespace Core {

StaticallyAllocated <LockablePtrT <CORBA::Core::ServantProxyObject> > SysManager::proxy_;

CORBA::Object::_ref_type SysManager::prot_domain_ref (ESIOP::ProtDomainId domain_id)
{
	const char prefix [] = "corbaloc::1.1@/";
	IDL::String s;
	s.reserve (std::size (prefix) - 1 + (sizeof (ESIOP::ProtDomainId) + 1) * 3);
	s = prefix;
	ESIOP::ProtDomainId id = domain_id;
	if (ESIOP::PLATFORMS_ENDIAN_DIFFERENT && endian::native == endian::little)
		Nirvana::byteswap (id);
	for (const uint8_t* p = (const uint8_t*)&id, *end = p + sizeof (ESIOP::ProtDomainId); p != end; ++p) {
		s += '%';
		uint8_t oct = *p;
		s += CORBA::Static_the_orb::to_hex (oct >> 4);
		s += CORBA::Static_the_orb::to_hex (oct & 0xf);
	}
	s += "%01";
	return CORBA::Static_the_orb::string_to_object (s, CORBA::Internal::RepIdOf <Nirvana::ProtDomainCore>::id);
}

bool SysManager::get_call_context (AsyncCallContext& ctx)
{
	if (CORBA::Core::Services::get_if_constructed (CORBA::Core::Services::SysDomain)) {
		ctx.proxy_ = proxy_->lock ();
		proxy_->unlock ();
		if (ctx.proxy_) {
			ctx.implementation_ = static_cast <SysManager*> (
				static_cast <CORBA::Internal::Bridge <PortableServer::ServantBase>*> (
					&ctx.proxy_->servant ()));
			return true;
		}
	}
	return false;
}

void SysManager::shutdown (unsigned flags) noexcept
{
	if (shutdown_started_)
		return;
	shutdown_started_ = true;
	shutdown_flags_ = flags;
	try {
		if (domains_.empty ())
			final_shutdown ();
		else
			domains_.shutdown (flags); // final_shutdown will be called on the last domain destruction
	} catch (...) {
		shutdown_started_ = false;
	}
}

void SysManager::ReceiveShutdown::run () noexcept
{
	try {
		sys_manager ().shutdown (flags_);
	} catch (...) {}
}

}
}
