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

#ifndef NIRVANA_CORE_SHELL_H_
#define NIRVANA_CORE_SHELL_H_
#pragma once

#include <CORBA/Server.h>
#include <Nirvana/Shell_s.h>
#include "Executable.h"
#include "Binder.h"
#include "open_binary.h"

namespace Nirvana {

class Static_the_shell :
	public CORBA::servant_traits <Shell>::ServantStatic <Static_the_shell>
{
public:
	static int cmdlet (const StringSeq& argv, const SpawnFiles& files)
	{
		if (argv.empty ())
			throw_BAD_PARAM ();
		auto cmdlet = Core::Binder::bind_interface (argv [0], CORBA::Internal::RepIdOf <Cmdlet>::id);

		int ret = -1;
		SYNC_BEGIN (*cmdlet.sync_context, &Core::Heap::user_heap ());
		Core::MemContext::current ().set_spawn_files (files);
		ret = cmdlet.itf.template downcast <Cmdlet> ()->run (argv);
		SYNC_END ();

		return ret;
	}

	static int spawn (const StringSeq& argv, const SpawnFiles& files)
	{
		if (argv.empty ())
			throw_BAD_PARAM ();
		AccessDirect::_ref_type binary = open_binary (argv [0]);
		PlatformId platform = get_binary_platform (binary);
		if (!Core::Binary::is_supported_platform (platform))
			BindError::throw_unsupported_platform (platform);

		int32_t ret;
		if (Core::SINGLE_DOMAIN || PLATFORM == platform) {
			ret = (int)Nirvana::ProtDomainCore::_narrow (
				CORBA::Core::Services::bind (CORBA::Core::Services::ProtDomain))
				->spawn (binary, argv, files);
		} else {
			Nirvana::ProtDomainCore::_ref_type domain = ProtDomainCore::_narrow (
				Nirvana::SysDomain::_narrow (CORBA::Core::Services::bind (CORBA::Core::Services::SysDomain))
				->provide_manager ()->create_prot_domain (platform));
			ret = domain->spawn (binary, argv, files);
			domain->shutdown (0);
		}
		return (int)ret;
	}

	static void create_pipe (AccessChar::_ref_type& pipe_out, AccessChar::_ref_type& pipe_in)
	{
		throw_NO_IMPLEMENT ();
	}

	static void get_spawn_files (SpawnFiles& files)
	{
		return Core::MemContext::current ().get_spawn_files (files);
	}

	static AccessDirect::_ref_type open_binary (const IDL::String& path)
	{
		return Core::open_binary (path);
	}

	static PlatformId get_binary_platform (AccessDirect::_ptr_type binary)
	{
		return Core::Binary::get_platform (binary);
	}

};

}

#endif
