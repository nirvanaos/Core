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
#ifndef NIRVANA_CORE_SCHEDULER_H_
#define NIRVANA_CORE_SCHEDULER_H_
#pragma once

#include <Port/Scheduler.h>
#include "AtomicCounter.h"
#include "Runnable.h"
#include <atomic>
#include "StaticallyAllocated.h"

namespace ESIOP {

struct MessageHeader;
void dispatch_message (MessageHeader& message) noexcept;

}

namespace Nirvana {
namespace Core {

class SysManager;

class Scheduler
{
public:
	enum State
	{
		RUNNING = 0,
		SHUTDOWN_PLANNED, // Start shutdown on the last activity end

		// Disabled object creation, periodic timers and wait events
		SHUTDOWN_STARTED,

		TERMINATE2,

		// Disabled async GC
		TERMINATE1,
		SHUTDOWN_FINISH
	};

	static void initialize ()
	{
		global_.construct ();
	}

	static void terminate () noexcept
	{
		global_.destruct ();
	}

	static State state () noexcept
	{
		return global_->state.load (std::memory_order_acquire);
	}

	static bool shutdown_started () noexcept
	{
		return state () >= State::SHUTDOWN_STARTED;
	}

	static bool async_GC_allowed () noexcept;

	/// Start new activity.
	/// Called on creation of the execution domain.
	static void activity_begin () noexcept
	{
		assert (global_->state < State::SHUTDOWN_FINISH);
		global_->activity_cnt.increment ();
	}

	/// End activity.
	/// Called on completion of the execution domain.
	static void activity_end () noexcept;

	/// Initiates shutdown.
	/// Shutdown will be completed when activity count became zero.
	static void shutdown (unsigned flags);

	/// Reserve space for an active item.
	/// \throws CORBA::NO_MEMORY
	static void create_item (bool with_reschedule)
	{
		Port::Scheduler::create_item (with_reschedule);
	}

	/// Release active item space.
	static void delete_item (bool with_reschedule) noexcept
	{
		Port::Scheduler::delete_item (with_reschedule);
	}

	/// \summary Schedule execution.
	/// 
	/// \param deadline Deadline.
	/// \param executor Executor.
	static void schedule (const DeadlineTime& deadline, Executor& executor) noexcept
	{
		Port::Scheduler::schedule (deadline, executor);
	}

	/// \summary Re-schedule execution.
	/// 
	/// \param deadline New deadline.
	/// \param executor Executor.
	/// \param old Old deadline.
	/// \returns `true` if the executor was found and rescheduled. `false` if executor with old deadline was not found.
	static bool reschedule (const DeadlineTime& deadline, Executor& executor, const DeadlineTime& old) noexcept
	{
		return Port::Scheduler::reschedule (deadline, executor, old);
	}

private:
	friend class SysManager;
	friend void ESIOP::dispatch_message (ESIOP::MessageHeader& message) noexcept;

	static void internal_shutdown (unsigned flags);

	static bool start_shutdown (State& from_state) noexcept;

	static void do_shutdown () noexcept;

	static void toggle_activity () noexcept;

	class Shutdown : public Runnable
	{
	private:
		virtual void run () noexcept override;
	};

	class Terminator2 : public Runnable
	{
	private:
		virtual void run () noexcept override;
	};

	class Terminator1 : public Runnable
	{
	private:
		virtual void run () noexcept override;
	};

	struct GlobalData
	{
		std::atomic <State> state;
		AtomicCounter <false> activity_cnt;

		GlobalData () :
			state (State::RUNNING),
			activity_cnt (0)
		{}
	};

	static StaticallyAllocated <GlobalData> global_;
};

}
}

#endif
