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

#include "Startup.h"
#include <CORBA/Server.h>
#include "ExecDomain.h"
#include "ProtDomain.h"
#include "initterm.h"
#include <Nirvana/Shell_s.h>
#include <NameService/FileSystem.h>
#include "open_binary.h"

namespace Nirvana {
namespace Core {

Startup::Startup (int argc, char* argv []) noexcept :
	argc_ (argc),
	argv_ (argv),
	ret_ (0)
{}

void Startup::launch (DeadlineTime deadline) noexcept
{
	try {
		ExecDomain::async_call (deadline, *this, g_core_free_sync_context, &Heap::shared_heap ());
	} catch (...) {
		on_exception ();
	}
}

void Startup::run_command ()
{
	if (argc_ > 1) {

		bool cmdlet = false;
		const char* prog = argv_ [1];
		char** args = argv_ + 2, ** end = argv_ + argc_;

		if (prog [0] == '-' && prog [1] == 'c' && prog [2] == 0 && args != end) {
			prog = *(args++);
			cmdlet = true;
		}
		
		std::vector <std::string> argv;
		argv.reserve (end - args);
		for (char** arg = args; arg != end; ++arg) {
			argv.emplace_back (*arg);
		}

		FileDescriptors files = MemContext::current ().get_inherited_files (6);

		if (cmdlet) {
			ret_ = (int)the_shell->cmdlet (prog, argv, "/sbin", files);
		} else {

			IDL::String exe_path = prog;
			IDL::String translated;
			if (FileSystem::translate_path (exe_path, translated))
				exe_path = std::move (translated);

			if (!FileSystem::is_absolute (exe_path))
				exe_path = ProtDomain::binary_dir () + '/' + exe_path;
			AccessDirect::_ref_type binary = open_binary (exe_path);

			ret_ = (int)the_shell->process (binary, argv, "/sbin", files);
		}
		Scheduler::shutdown (0);
	}
}

bool Startup::initialize () noexcept
{
	try {
		Nirvana::Core::initialize ();
	} catch (const CORBA::Exception& ex) {
		on_exception ();
		return false;
	}
	return true;
}

void Startup::on_exception () noexcept
{
	exception_ = std::current_exception ();
	Scheduler::shutdown (0);
}

}
}
