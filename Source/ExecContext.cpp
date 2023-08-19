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
#include "ExecDomain.h"
#include "Thread.h"

namespace Nirvana {
namespace Core {

void ExecContext::run_in_neutral_context (Runnable& runnable) noexcept
{
	ExecContext& neutral_context = Thread::current ().neutral_context ();
	neutral_context.runnable_ = &runnable;
	neutral_context.switch_to ();
}

void ExecContext::neutral_context_loop (Thread& worker) noexcept
{
	assert (&current () == &worker.neutral_context ());
	while (worker.neutral_context ().runnable_) {
		worker.neutral_context ().run ();
		ExecDomain* ed = worker.exec_domain ();
		if (ed) {
			assert (ed->runnable ());
			ed->switch_to ();
		}
	}
}

}
}
