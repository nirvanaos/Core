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
#ifndef NIRVANA_CORE_TIMERASYNCCALL_H_
#define NIRVANA_CORE_TIMERASYNCCALL_H_
#pragma once

#include "Timer.h"
#include "SyncContext.h"
#include "Runnable.h"
#include "Chrono.h"

namespace Nirvana {
namespace Core {

/// Timer to make asynchronous calls.
/// 
class TimerAsyncCall : 
	public Timer
	// Derived class must derive memory allocation class such as UserObject or SharedObject
{
	DECLARE_CORE_INTERFACE

public:
	int overrun () const noexcept
	{
		return overrun_;
	}

	const DeadlineTime& deadline () const noexcept
	{
		return deadline_;
	}

	void deadline (const DeadlineTime& deadline) noexcept
	{
		deadline_ = deadline;
	}

protected:
	TimerAsyncCall (SyncContext& sync_context, const DeadlineTime& deadline);

	virtual void run (const TimeBase::TimeT& signal_time) noexcept = 0;

private:
	void signal () noexcept override;

	void run_complete () noexcept
	{
		enqueued_.clear ();
	}

	class Runnable : public Core::Runnable
	{
	public:
		Runnable (TimerAsyncCall& timer) :
			timer_ (&timer),
			signal_time_ (Chrono::steady_clock ())
		{}

		~Runnable ()
		{
			timer_->run_complete ();
		}

	private:
		void run () noexcept override;

	private:
		Ref <TimerAsyncCall> timer_;
		TimeBase::TimeT signal_time_;
	};

private:
	Ref <SyncContext> sync_context_;
	DeadlineTime deadline_;
	std::atomic_flag enqueued_;
	std::atomic <int> overrun_;
};

}
}

#endif
