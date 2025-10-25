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
#include "Module.h"
#include <Nirvana/OLF_Iterator.h>
#include "ClassLibrary.h"
#include "Singleton.h"
#include "Binder.h"
#include "ExecDomain.h"
#include <Nirvana/BindErrorUtl.h>

namespace Nirvana {
namespace Core {

Module::Module (int32_t id, Port::Module&& bin, const ModuleStartup& startup_entry) :
	Binary (std::move (bin)),
	startup_entry_ (startup_entry),
	entry_point_ (ModuleInit::_check (startup_entry.startup)),
	ref_cnt_ (0),
	initial_ref_cnt_ (0),
	id_ (id)
{}

void Module::_remove_ref () noexcept
{
	AtomicCounter <false>::IntegralType rcnt = ref_cnt_.decrement_seq ();
	// Storing the release_time_ may be not atomic, but it is not critical.
	if (rcnt == initial_ref_cnt_)
		release_time_ = Chrono::steady_clock ();
}

void Module::initialize ()
{
	if (entry_point_)
		entry_point_->initialize ();
}

void Module::terminate () noexcept
{
	if (entry_point_) {
		try {
			entry_point_->terminate ();
		} catch (...) {
			// TODO: Log
		}
		entry_point_ = nullptr;
	}
}

void Module::raise_exception (CORBA::SystemException::Code code, unsigned minor)
{
	CORBA::Internal::Bridge <ModuleInit>* br = static_cast <CORBA::Internal::Bridge <ModuleInit>*> (&entry_point_);
	if (br)
		br->_epv ().epv.raise_exception (br, (short)code, (unsigned short)minor, nullptr);
}

Module* Module::create (int32_t id, AccessDirect::_ptr_type file)
{
	// Load module into memory
	Port::Module bin (file);

	// Find module entry point
	const ModuleStartup* startup = nullptr;
	const Section& metadata = bin.metadata ();
	for (OLF_Iterator <> it (metadata.address, metadata.size); !it.end (); it.next ()) {
		if (!it.valid ())
			BindError::throw_invalid_metadata ();
		if (OLF_MODULE_STARTUP == *it.cur ()) {
			if (startup)
				BindError::throw_message ("Duplicated OLF_MODULE_STARTUP entry");
			startup = reinterpret_cast <const ModuleStartup*> (it.cur ());
		}
	}

	if (!startup)
		BindError::throw_message ("OLF_MODULE_STARTUP not found");

	Module* ret;
	if (startup->flags & OLF_MODULE_SINGLETON)
		ret = new Singleton (id, std::move (bin), *startup);
	else
		ret = new ClassLibrary (id, std::move (bin), *startup);

	return ret;
}

}
}
