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
#include "TimerAsyncCall.h"
#include "ExecDomain.h"

namespace Nirvana {
namespace Core {

volatile bool Timer::initialized_;

TimerAsyncCall::TimerAsyncCall (SyncContext& sync_context, const DeadlineTime& deadline) :
	sync_context_ (&sync_context),
	deadline_ (deadline),
	enqueued_ ATOMIC_FLAG_INIT,
	overrun_ (0)
{}

void TimerAsyncCall::signal () noexcept
{
	if (!enqueued_.test_and_set ()) {
		try {
			ExecDomain::async_call <Runnable> (deadline_, *sync_context_, nullptr, std::ref (*this));
		} catch (...) {
			++overrun_;
		}
	} else
		++overrun_;
}

void TimerAsyncCall::Runnable::run () noexcept
{
	timer_->run (signal_time_);
}

}
}
