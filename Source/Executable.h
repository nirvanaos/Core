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
#ifndef NIRVANA_LEGACY_CORE_EXECUTABLE_H_
#define NIRVANA_LEGACY_CORE_EXECUTABLE_H_
#pragma once

#include <CORBA/Server.h>
#include "SyncContext.h"
#include "Binary.h"
#include "Binder.h"
#include <Nirvana/Module_s.h>
#include <Nirvana/Main.h>
#include "ORB/LifeCycleStack.h"
#include "AtExit.h"

namespace Nirvana {
namespace Core {

class Executable :
	public Binary,
	public ImplStatic <SyncContext>,
	public CORBA::servant_traits <Nirvana::Module>::Servant <Executable>,
	public CORBA::Core::LifeCycleStack
{
public:
	Executable (AccessDirect::_ptr_type file) :
		Binary (file),
		ImplStatic <SyncContext> (false),
		entry_point_ (Binder::bind (*this))
	{}

	~Executable ()
	{
		Binder::unbind (*this);
	}

	int main (Main::Strings& argv)
	{
		int ret = entry_point_->main (argv);
		at_exit_.execute ();
		entry_point_->cleanup ();
		return ret;
	}

	void atexit (AtExitFunc f)
	{
		at_exit_.atexit (f);
	}

	static int32_t id () noexcept
	{
		return 0;
	}

	// SyncContext::

	virtual SyncContext::Type sync_context_type () const noexcept override;
	virtual Nirvana::Core::Module* module () noexcept override;
	virtual void raise_exception (CORBA::SystemException::Code code, unsigned minor) override;

private:
	Main::_ptr_type entry_point_;
	AtExitSync <UserAllocator> at_exit_;
};

inline Main::_ptr_type Binder::bind (Executable& exe)
{
	// Find module entry point
	const ProcessStartup* startup_entry = nullptr;
	const Section& metadata = exe.metadata ();
	for (OLF_Iterator <> it (metadata.address, metadata.size); !it.end (); it.next ()) {
		if (!it.valid ())
			BindError::throw_invalid_metadata ();
		if (OLF_PROCESS_STARTUP == *it.cur ()) {
			if (startup_entry)
				BindError::throw_message ("Duplicated OLF_PROCESS_STARTUP entry");
			startup_entry = reinterpret_cast <const ProcessStartup*> (it.cur ());
		}
	}

	if (!startup_entry)
		BindError::throw_message ("OLF_PROCESS_STARTUP not found");

	Main::_ptr_type startup = Main::_check (startup_entry->startup);

	call_with_user_exceptions (singleton_->sync_domain_, nullptr, [&exe]() {
		singleton_->module_bind (exe._get_ptr (), exe.metadata (), nullptr);
		try {
			singleton_->binary_map_.add (exe);
		} catch (...) {
			release_imports (exe._get_ptr (), exe.metadata ());
			throw;
		}
		});

	return startup;
}

inline void Binder::unbind (Executable& mod) noexcept
{
	SYNC_BEGIN (singleton_->sync_domain_, nullptr);
	singleton_->binary_map_.remove (mod);
	SYNC_END ();
	release_imports (mod._get_ptr (), mod.metadata ());
}

}
}

#endif
