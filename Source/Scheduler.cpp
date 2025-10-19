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
#include "Scheduler.h"
#include "ExecDomain.h"
#include "initterm.h"
#include "Chrono.h"
#include "ORB/ESIOP.h"
#include "SysManager.h"

// If we use INFINITE_DEADLINE, the new background thread will be created for shutdown.
//#define SHUTDOWN_DEADLINE Chrono::make_deadline (1 * TimeBase::MINUTE)
#define SHUTDOWN_DEADLINE INFINITE_DEADLINE

namespace Nirvana {
namespace Core {

void Scheduler::Shutdown::run ()
{
	do_shutdown ();
}

void Scheduler::Terminator2::run ()
{
	// Terminate
	NIRVANA_TRACE ("Core::terminate2 ()");
	Core::terminate2 ();
}

void Scheduler::Terminator1::run ()
{
	// Terminate
	// Ensure that all asynchronous GC is completed.
	const auto& activity_cnt = global_->activity_cnt;
	for (BackOff bo; activity_cnt.load () > 1;) {
		bo ();
	}
#ifdef DEBUG_SHUTDOWN
	the_debugger->debug_event (Debugger::DebugEvent::DEBUG_INFO, "Core::terminate1 ()", __FILE__, __LINE__);
#endif
	Core::terminate1 ();
}

StaticallyAllocated <Scheduler::GlobalData> Scheduler::global_;

bool Scheduler::async_GC_allowed () noexcept
{
	return state () < State::TERMINATE1
		&& ExecDomain::current ().restricted_mode () != ExecDomain::RestrictedMode::SUPPRESS_ASYNC_GC;
}

void Scheduler::do_shutdown ()
{
#ifdef DEBUG_SHUTDOWN
	the_debugger->debug_event (Debugger::DebugEvent::DEBUG_INFO, "Core::shutdown ()", __FILE__, __LINE__);
#endif

	Core::shutdown ();

#ifdef DEBUG_SHUTDOWN
	the_debugger->debug_event (Debugger::DebugEvent::DEBUG_INFO, "Core::shutdown () end", __FILE__, __LINE__);
#endif

	// If no activity - toggle it.
	toggle_activity ();
}

void Scheduler::toggle_activity () noexcept
{
	global_->activity_cnt.increment ();
	activity_end ();
}

void Scheduler::shutdown (unsigned flags)
{
	if (!ESIOP::is_system_domain () || !SysManager::async_call <SysManager::ReceiveShutdown> (INFINITE_DEADLINE, flags))
		internal_shutdown (flags);
}

void Scheduler::internal_shutdown (unsigned flags)
{
	State state = State::RUNNING;
	if (flags & SHUTDOWN_FLAG_FORCE)
		start_shutdown (state);
	else if (global_->state.compare_exchange_strong (state, State::SHUTDOWN_PLANNED))
		toggle_activity ();
}

bool Scheduler::start_shutdown (State& from_state) noexcept
{
	if (global_->state.compare_exchange_strong (from_state, State::SHUTDOWN_STARTED)) {
		try {
			// If called from worker thread and not from the invocation context dispatched from some POA,
			// do shutdown in the current execution domain. Otherwise start async call.
			Thread* th = Thread::current_ptr ();
			if (th && th->executing ()) {
				ExecDomain* ed = th->exec_domain ();
				if (ed && !ed->TLS_get (CoreTLS::CORE_TLS_PORTABLE_SERVER)) {
					do_shutdown ();
					return true;
				}
			}
			ExecDomain::async_call <Shutdown> (SHUTDOWN_DEADLINE, g_core_free_sync_context, nullptr);
			return true;
		} catch (...) {
			global_->state = from_state;
		}
	}
	return false;
}

void Scheduler::activity_end () noexcept
{
	if (0 == global_->activity_cnt.decrement_seq ()) {
		State state = global_->state.load ();
		switch (state) {
			case State::SHUTDOWN_PLANNED:
				start_shutdown (state);
				break;

			case State::SHUTDOWN_STARTED:
				if (global_->state.compare_exchange_strong (state, State::TERMINATE2)) {
#ifdef DEBUG_SHUTDOWN
					the_debugger->debug_event (Debugger::DebugEvent::DEBUG_INFO, "ExecDomain::async_call <Terminator2>", __FILE__, __LINE__);
#endif
					try {
						ExecDomain::async_call <Terminator2> (SHUTDOWN_DEADLINE, g_core_free_sync_context, nullptr);
					} catch (...) {
						// Fallback
						toggle_activity ();
					}
				}
				break;

			case State::TERMINATE2:
				if (global_->state.compare_exchange_strong (state, State::TERMINATE1)) {
#ifdef DEBUG_SHUTDOWN
					the_debugger->debug_event (Debugger::DebugEvent::DEBUG_INFO, "ExecDomain::async_call <Terminator1>", __FILE__, __LINE__);
#endif
					try {
						ExecDomain::async_call <Terminator1> (SHUTDOWN_DEADLINE, g_core_free_sync_context, nullptr);
					} catch (...) {
						// Fallback
						toggle_activity ();
					}
				}
				break;

			case State::TERMINATE1:
				if (global_->state.compare_exchange_strong (state, State::SHUTDOWN_FINISH))
					Port::Scheduler::shutdown ();
				break;
		}
	}
}

}
}
