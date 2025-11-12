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
#ifndef NIRVANA_CORE_DEBUGGER_H_
#define NIRVANA_CORE_DEBUGGER_H_

#include "RuntimeSupport.h"
#include "Signals.h"
#include "SharedAllocator.h"
#include <Nirvana/signal_defs.h>
#include <Port/Debugger.h>

namespace Nirvana {
namespace Core {

class Debugger :
	public CORBA::servant_traits <Nirvana::Debugger>::ServantStatic <Debugger>,
	public RuntimeSupport
{
public:
	static void debug_event (DebugEvent evt, const IDL::String& msg, const IDL::String& file_name, int32_t line_number)
	{
		// Use shared string to avoid possible problems with the memory context.
		SharedString s;

		if (!file_name.empty ()) {
			s.assign (file_name.c_str (), file_name.size ());
			if (line_number > 0) {
				s += '(';
				size_t len = s.length ();
				do {
					unsigned d = line_number % 10;
					line_number /= 10;
					s.insert (len, 1, '0' + d);
				} while (line_number);
				s += ')';
			}
			s += ": ";
		}

		static const char* const ev_prefix [(size_t)DebugEvent::DEBUG_ERROR + 1] = {
			"INFO: ",
			"WARNING: ",
			"Assertion failed: ",
			"ERROR: "
		};

		s += ev_prefix [(unsigned)evt];
		s.append (msg.c_str (), msg.size ());
		s += '\n';
		Port::Debugger::output_debug_string (evt, s.c_str ());
		if (evt >= DebugEvent::DEBUG_WARNING && !Port::Debugger::debug_break (evt)) {
			if (evt >= DebugEvent::DEBUG_ASSERT) {
				Signals::raise (SIGABRT);
				unrecoverable_error (SIGABRT);
			}
		}
	}
};

}
}

#endif
