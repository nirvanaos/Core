/*
* Nirvana Core.
*
* This is a part of the Nirvana project.
*
* Author: Igor Popov
*
* Copyright (c) 2025 Igor Popov.
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
#include "ProcessImpl.h"

namespace Nirvana {
namespace Core {

void ProcessImpl::finish (int exit_code) noexcept
{
	assert (sync_context_);
	Synchronized _sync_frame (*sync_context_, nullptr);
	exit_code_ = exit_code;
	event_.signal_all ();
	sync_context_ = nullptr;
	_remove_ref ();
}

void ProcessImpl::run () noexcept
{
	finish (executable_.main (argv_));
}

void ProcessImpl::on_crash (const siginfo& signal) noexcept
{
	int exit_code;
	if (SIGABRT == signal.si_signo)
		exit_code = 3;
	else
		exit_code = 128 + signal.si_signo;
	finish (exit_code);
}

}
}
