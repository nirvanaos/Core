/// \file
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
#ifndef NIRVANA_CORE_TIMEOUT_H_
#define NIRVANA_CORE_TIMEOUT_H_
#pragma once

#include "TimerAsyncCall.h"
#include "CoreInterface.h"
#include "UserObject.h"

namespace Nirvana {
namespace Core {

class TimeoutBase
{
	static const TimeBase::TimeT TIMEOUT_DEADLINE = 1 * TimeBase::MILLISECOND;

protected:
	static const TimeBase::TimeT NO_EXPIRE = std::numeric_limits <TimeBase::TimeT>::max ();

	TimeoutBase ();
	~TimeoutBase ();

	class TimerBase :
		public TimerAsyncCall,
		public UserObject
	{
	public:
		void disconnect () noexcept
		{
			object_ = nullptr;
			TimerAsyncCall::cancel ();
		}

	protected:
		TimerBase (void* obj);

	protected:
		void* object_;
	};

protected:
	Ref <TimerBase> timer_;
	TimeBase::TimeT nearest_expire_time_;
};

template <class T>
class Timeout : public TimeoutBase
{
private:
	class Timer : public TimerBase
	{
	protected:
		Timer (T* obj) :
			TimerBase (obj)
		{}

	private:
		void run (const TimeBase::TimeT&) noexcept override
		{
			if (object_)
				((Timeout*)object_)->call_on_timer ();
		}
	};

protected:
	TimeBase::TimeT nearest_expire_time () const noexcept
	{
		return nearest_expire_time_;
	}

	void nearest_expire_time (TimeBase::TimeT t) noexcept;

private:
	void call_on_timer () noexcept
	{
		nearest_expire_time_ = NO_EXPIRE;
		static_cast <T&> (*this).on_timer (Chrono::steady_clock ());
	}
};

template <class T>
void Timeout <T>::nearest_expire_time (TimeBase::TimeT t) noexcept
{
	if (nearest_expire_time_ != t) {
		nearest_expire_time_ = t;
		if (NO_EXPIRE == t)
			timer_->cancel ();
		else {
			try {

				if (!timer_)
					timer_ = Ref <TimerBase>::create <ImplDynamic <Timer> > (static_cast <T*> (this));
				
				TimeBase::TimeT cur_t = Chrono::steady_clock ();
				if (cur_t >= t)
					t = 0;
				else
					t -= cur_t;
				timer_->set (0, t, 0);
			} catch (const CORBA::Exception& exc) {
				static_cast <T&> (*this).on_exception (exc);
			}
		}
	}
}

}
}

#endif
