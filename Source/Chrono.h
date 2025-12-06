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
#ifndef NIRVANA_CORE_CHRONO_H_
#define NIRVANA_CORE_CHRONO_H_
#pragma once

#include <Port/Chrono.h>
#include <Nirvana/rescale.h>
#include "unrecoverable_error.h"

namespace Nirvana {
namespace Core {

/// Core time service.
class Chrono : private Port::Chrono
{
public:
	static void initialize () noexcept
	{
		if (!Port::Chrono::initialize ())
			unrecoverable_error (0);
	}

	static void terminate () noexcept
	{
		Port::Chrono::terminate ();
	}

	/// Current system time.
	static TimeBase::UtcT system_clock () noexcept
	{
		return Port::Chrono::system_clock ();
	}

	/// Current UTC time.
	static TimeBase::UtcT UTC () noexcept
	{
		return Port::Chrono::UTC ();
	}

	/// Set current UTC time.
	/// \param t UTC time.
	static void set_UTC (const TimeBase::TimeT& t)
	{
		Port::Chrono::set_UTC (t);
	}

	/// System clock resolution.
	static const TimeBase::TimeT& system_clock_resolution () noexcept
	{
		return Port::Chrono::system_clock_resolution ();
	}

	/// Duration since system startup in 100 ns intervals.
	static SteadyTime steady_clock () noexcept
	{
		return Port::Chrono::steady_clock ();
	}

	/// Steady clock resolution.
	static const SteadyTime& steady_clock_resolution () noexcept
	{
		return Port::Chrono::steady_clock_resolution ();
	}

	/// Duration since system startup with maximal possible precision.
	static DeadlineTime deadline_clock () noexcept
	{
		return Port::Chrono::deadline_clock ();
	}

	/// Deadline clock frequency, Hz.
	static const DeadlineTime& deadline_clock_frequency () noexcept
	{
		return Port::Chrono::deadline_clock_frequency ();
	}

	/// Convert UTC time to the local deadline time.
	/// 
	/// \param utc UTC time.
	/// \returns Local deadline time.
	static DeadlineTime deadline_from_UTC (const TimeBase::UtcT& utc) noexcept
	{
		TimeBase::UtcT cur = UTC ();
		return deadline_clock () + time_to_deadline (utc.time () - cur.time () + inacc_max (utc, cur));
	}

	/// Convert local deadline time to UTC time.
	/// 
	/// \param deadline Local deadline time.
	/// \returns UTC time.
	static TimeBase::UtcT deadline_to_UTC (DeadlineTime deadline) noexcept
	{
		TimeBase::UtcT utc = UTC ();
		utc.time (utc.time () + deadline_to_time (deadline));
		return utc;
	}

	/// Convert deadline time interval to 100ns interval.
	/// 
	/// \param deadline Deadline time interval.
	/// \returns Time interval in 100ns units.
	static int64_t deadline_to_time (int64_t deadline) noexcept
	{
		return rescale64 (deadline, 10000000,
			deadline_clock_frequency () - 1, deadline_clock_frequency ());
	}

	/// Convert 100ns interval to deadline interval.
	/// 
	/// \param Time Time interval in 100ns units.
	/// \returns Deadline time interval.
	static int64_t time_to_deadline (int64_t time) noexcept
	{
		return rescale64 (time, deadline_clock_frequency (), 0, 10000000);
	}

	/// Make deadline.
	/// 
	/// NOTE: If deadline_clock_frequency () is too low (1 sec?), Port library can implement advanced
	/// algorithm to create diffirent deadlines inside one clock tick, based on atomic counter.
	///
	/// \param timeout A timeout from the current time.
	/// \return Deadline time as local deadline clock value.
	static DeadlineTime make_deadline (TimeBase::TimeT timeout) noexcept
	{
		return Port::Chrono::make_deadline (timeout);
	}

	/// Convert CORBA RT request priority to the request deadline.
	/// 
	/// \param priority See IOP::RTCorbaPriority service context.
	/// \return Deadline time as local deadline clock value.
	static DeadlineTime deadline_from_priority (int16_t priority) noexcept
	{
		assert (priority >= 0);
		return deadline_clock () + deadline_clock_frequency () * ((int32_t)std::numeric_limits <int16_t>::max () - (int32_t)priority + 1) / TimeBase::MILLISECOND;
	}

	/// Convert request deadline to the CORBA RT request priority.
	/// 
	/// \param deadline The request deadline.
	/// \return CORBA RT priority. See IOP::RTCorbaPriority service context.
	static int16_t deadline_to_priority (DeadlineTime deadline) noexcept
	{
		int64_t rem = deadline - deadline_clock ();
		if (rem < 0)
			rem = 0;
		int64_t ms = rescale64 (rem, TimeBase::MILLISECOND, deadline_clock_frequency () - 1, deadline_clock_frequency ());
		if (ms > std::numeric_limits <int16_t>::max ())
			ms = std::numeric_limits <int16_t>::max ();
		return std::numeric_limits <int16_t>::max () - (int16_t)ms;
	}

private:
	static uint64_t inacc_max (const TimeBase::UtcT& t1, const TimeBase::UtcT& t2) noexcept
	{
		if (t1.inacchi () > t2.inacchi ())
			return ((uint64_t)t1.inacchi () << 32) | t1.inacclo ();
		else if (t1.inacchi () < t2.inacchi ())
			return ((uint64_t)t2.inacchi () << 32) | t2.inacclo ();
		else
			return ((uint64_t)t1.inacchi () << 32) | std::max (t1.inacclo (), t2.inacclo ());
	}
};

}
}

#endif
