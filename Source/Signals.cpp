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
#include "Signals.h"
#include <Nirvana/signal_defs.h>
#include <algorithm>
#include "ExecDomain.h"

namespace Nirvana {
namespace Core {

const int Signals::supported_signals_ [] = {
	SIGINT,
	SIGABRT,
	SIGTERM
};

const Signals::SigToExc Signals::sig2exc_ [] = {
	{ SIGILL, CORBA::SystemException::EC_ILLEGAL_INSTRUCTION },
	{ SIGFPE, CORBA::SystemException::EC_ARITHMETIC_ERROR },
	{ SIGSEGV, CORBA::SystemException::EC_ACCESS_VIOLATION }
};

inline int Signals::signal_index (int signal) noexcept
{
	const int* p = std::lower_bound (supported_signals_, std::end (supported_signals_), signal);
	if (p != std::end (supported_signals_) && *p == signal)
		return (int)(p - supported_signals_);
	return -1;
}

void Signals::raise (int signo)
{
	if (signal_index (signo) >= 0) {
		siginfo_t signal { 0 };
		signal.si_signo = signo;
		signal.si_excode = CORBA::Exception::EC_NO_EXCEPTION;
		ExecDomain::on_signal (signal);
	} else
		throw_BAD_PARAM ();
}

CORBA::Exception::Code Signals::signal2ex (int signal) noexcept
{
	for (const SigToExc* p = sig2exc_; p != std::end (sig2exc_); ++p) {
		if (p->signal == signal) {
			return p->ec;
		}
	}

	return CORBA::Exception::EC_NO_EXCEPTION;
}

}
}
