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
#ifndef NIRVANA_CORE_PROCESSIMPL_H_
#define NIRVANA_CORE_PROCESSIMPL_H_
#pragma once

#include "Runnable.h"
#include "EventSyncTimeout.h"
#include "Executable.h"
#include "Scheduler.h"
#include <Nirvana/Shell_s.h>

namespace Nirvana {
namespace Core {

class ProcessImpl : 
	public CORBA::servant_traits <Nirvana::Process>::Servant <ProcessImpl>,
	public Runnable
{
public:
	ProcessImpl (AccessDirect::_ptr_type binary, StringSeq& argv) :
		executable_ (binary),
		argv_ (std::move (argv)),
		sync_context_ (&SyncContext::current ()),
		exit_code_ (-1)
	{
		_add_ref ();
		// While the Process object exists we have to keep domain running.
		Scheduler::activity_begin ();
	}

	~ProcessImpl ()
	{
		Scheduler::activity_end ();
	}

	int32_t id () const noexcept
	{
		return 0;
	}

	bool get_exit_code (int32_t& ret)
	{
		if (!event_.wait (0))
			return false;
		ret = exit_code_;
		return true;
	}

	bool wait (const TimeBase::TimeT& timeout)
	{
		return event_.wait (timeout);
	}

	void signal (int sig)
	{
		throw_NO_IMPLEMENT ();
	}

	void run () noexcept override;
	void on_crash (const siginfo& signal) noexcept override;

	SyncContext& sync_context () noexcept
	{
		return executable_;
	}

	void on_exception () noexcept
	{
		finish (-1);
	}

private:
	void finish (int exit_code) noexcept;

private:
	Executable executable_;
	StringSeq argv_;
	EventSyncTimeout event_;
	Ref <SyncContext> sync_context_;
	int32_t exit_code_;
};

inline void ExecDomain::start_process (ProcessImpl& process)
{
	Ref <ExecDomain> exec_domain = create (INFINITE_DEADLINE, Ref <MemContext> (&MemContext::current ()));
	exec_domain->runnable_ = &process;
	exec_domain->spawn (process.sync_context ());
}

}
}

#endif
