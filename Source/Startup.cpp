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
#include "ExecDomain.h"
#include "initterm.h"
#include <Nirvana/Shell.h>

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
		char** args = argv_ + 1, ** end = argv_ + argc_;
		const char* first = *args;

		if (first [0] == '-' && first [1] == 'c' && first [2] == 0) {
			++args;
			if (args == end)
				throw_BAD_PARAM ();
			cmdlet = true;
		}
		
		std::vector <std::string> argv;
		argv.reserve (end - args);
		for (char** arg = args; arg != end; ++arg) {
			argv.emplace_back (*arg);
		}

		SpawnFiles files;
		the_shell->get_spawn_files (files);
		files.work_dir ("/sbin");

		if (cmdlet)
			ret_ = the_shell->cmdlet (argv, files);
		else {
			Process::_ref_type process = the_shell->spawn (argv, files);
			process->wait (std::numeric_limits <TimeBase::TimeT>::max ());
			int32_t ret;
			process->get_exit_code (ret);
			ret_ = (int)ret;
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
