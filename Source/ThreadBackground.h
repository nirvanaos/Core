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
#ifndef NIRVANA_CORE_THREADBACKGROUND_H_
#define NIRVANA_CORE_THREADBACKGROUND_H_
#pragma once

#include "Heap.h"
#include "ConditionalCreator.h"
#include "ExecDomain.h"
#include <Port/ThreadBackground.h>
#include <Port/config.h>

namespace Nirvana {
namespace Core {

class ThreadBackground :
	public SharedObject,
	public Port::ThreadBackground,
	public ConditionalObjectBase <BACKGROUND_THREAD_POOLING>
{
	friend class Port::ThreadBackground;
	typedef Port::ThreadBackground Base;
	friend class ObjectCreator <ThreadBackground*>;
	
	using Creator = ConditionalCreator <ThreadBackground*, BACKGROUND_THREAD_POOLING>;

public:
	static void initialize () noexcept
	{
		Creator::initialize (BACKGROUND_THREAD_POOL_MIN);
	}

	static void terminate () noexcept
	{
		Creator::terminate ();
	}

	/// Create a new thread.
	/// @param ed ExecDomain assigned to the new background thread.
	static ThreadBackground* spawn (ExecDomain& ed)
	{
		ThreadBackground* thr = Creator::create ();
		thr->exec_domain (ed);
		return thr;
	}

	/// Terminate current thread.
	void stop () noexcept
	{
		assert (!dbg_exec_);
		cleanup ();
		if (BACKGROUND_THREAD_POOLING)
			Creator::release (this);
		else
			Base::stop ();
	}

	/// Switch context to ExecDomain and execute.
	void execute () noexcept
	{
		assert (!dbg_exec_.exchange (true));
		Base::execute ();
	}

	/// Yield execution to another thread that is ready to run on the current processor.
	void yield () noexcept
	{
		Base::yield ();
	}

protected:
	ThreadBackground ()
#ifndef NDEBUG
		: dbg_exec_ (false)
#endif
	{
		Base::start ();
	}

	~ThreadBackground ();

private:
	// Called from port.
	inline void run () noexcept
	{
		assert (this == current_ptr ());

		// Always called in the neutral context.
		assert (context () == &neutral_context ());

		assert (dbg_exec_.exchange (false));

		execute_begin ();

		ExecDomain* ed = exec_domain ();
		assert (ed);
		ed->execute (*this, Ref <Executor> (ed));
	}

	// Called from port.
	void on_thread_proc_end () noexcept
	{
		if (!BACKGROUND_THREAD_POOLING)
			Creator::release (this);
	}

private:
#ifndef NDEBUG
	std::atomic <bool> dbg_exec_;
#endif

};

}
}

#endif
