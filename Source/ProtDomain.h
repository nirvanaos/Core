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
#ifndef NIRVANA_CORE_PROTDOMAIN_H_
#define NIRVANA_CORE_PROTDOMAIN_H_
#pragma once

#include "ORB/SysServantStatic.h"
#include "IDL/CoreDomains_s.h"
#include <Port/ProtDomain.h>
#include "Binder.h"
#include "ORB/Services.h"
#include "ORB/system_services.h"
#include "ProcessImpl.h"

namespace Nirvana {
namespace Core {

/// Protection domain.
class ProtDomain :
	public CORBA::Core::SysServantStaticImpl <ProtDomain, ProtDomainCore, Nirvana::ProtDomain>,
	private Port::ProtDomain
{
public:
	static SecurityId user ()
	{
		return Security::prot_domain_context ().security_id ();
	}

	static CORBA::Object::_ref_type bind (const IDL::String& name)
	{
		return Binder::bind_interface <CORBA::Object> (name);
	}

	static Nirvana::SysDomain::_ref_type sys_domain ()
	{
		return Nirvana::SysDomain::_narrow (
			CORBA::Core::Services::bind (CORBA::Core::Services::SysDomain));
	}

	static void shutdown (unsigned flags)
	{
		Scheduler::shutdown (flags);
	}

	static CORBA::Object::_ref_type load_and_bind (int32_t mod_id, AccessDirect::_ptr_type file,
		const IDL::String& name)
	{
		return Binder::load_and_bind <CORBA::Object> (mod_id, file, name);
	}

	static void get_module_bindings (Nirvana::AccessDirect::_ptr_type binary,
		PM::ModuleBindings& bindings)
	{
		Binder::get_module_bindings (binary, bindings);
	}

	static Process::_ref_type spawn (AccessDirect::_ptr_type file, StringSeq& argv, const SpawnFiles& files)
	{
		auto process = CORBA::make_reference <ProcessImpl> (file, std::ref (argv));
		Core::MemContext::current ().set_spawn_files (files);
		try {
			ExecDomain::start_process (*process);
		} catch (...) {
			process->on_exception ();
			throw;
		}
		return process->_this ();
	}

	static IDL::String binary_dir ();
};

}
}

#endif
