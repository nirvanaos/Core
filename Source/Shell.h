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

namespace Nirvana {

class Static_the_shell :
	public CORBA::servant_traits <Shell>::ServantStatic <Static_the_shell>
{
public:
	static int spawn (AccessDirect::_ptr_type file,
		StringSeq& argv, IDL::String& work_dir, FileDescriptors& files)
	{
		Core::Executable executable (file);

		int ret = -1;
		SYNC_BEGIN (executable, &Core::Heap::user_heap ());
		Core::MemContext& mc = Core::MemContext::current ();
		mc.file_descriptors ().set_inherited_files (files);
		if (!work_dir.empty ())
			mc.current_dir ().chdir (work_dir);
		ret = executable.main (argv);
		SYNC_END ();

		return ret;
	}

	static int run_cmdlet (const IDL::String& name, StringSeq& argv,
		IDL::String& work_dir, FileDescriptors& files)
	{
		auto cmdlet = Core::Binder::bind_interface (name, CORBA::Internal::RepIdOf <Cmdlet>::id);

		int ret = -1;
		SYNC_BEGIN (*cmdlet.sync_context, &Core::Heap::user_heap ());
		Core::MemContext& mc = Core::MemContext::current ();
		mc.file_descriptors ().set_inherited_files (files);
		if (!work_dir.empty ())
			mc.current_dir ().chdir (work_dir);
		ret = cmdlet.itf.template downcast <Cmdlet> ()->run (argv);
		SYNC_END ();

		return ret;
	}

	static void create_pipe (AccessChar::_ref_type& pipe_out, AccessChar::_ref_type& pipe_in)
	{
		throw_NO_IMPLEMENT ();
	}

};

}

#endif
