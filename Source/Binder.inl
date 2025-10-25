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
#ifndef NIRVANA_CORE_BINDER_INL_
#define NIRVANA_CORE_BINDER_INL_
#pragma once

#include "Binder.h"
#include "ClassLibrary.h"
#include "Singleton.h"
#include "Executable.h"
#include "ORB/RequestLocalBase.h"
#include <Nirvana/BindErrorUtl.h>
#include <Nirvana/OLF_Iterator.h>
#include <CORBA/Proxy/ProxyBase.h>

namespace Nirvana {
namespace Core {

class Binder::Request : public CORBA::Core::RequestLocalBase
{
public:
	static Ref <Request> create ()
	{
		return Ref <Request>::create <CORBA::Core::RequestLocalImpl <Request> > ();
	}

protected:
	Request () : RequestLocalBase (&BinderMemory::heap (),
		CORBA::Internal::IORequest::RESPONSE_EXPECTED | CORBA::Internal::IORequest::RESPONSE_DATA)
	{}
};

inline
Main::_ptr_type Binder::bind (Executable& exe)
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

	BindResult ret;
	Ref <Request> rq = Request::create ();
	rq->invoke ();
	SYNC_BEGIN (singleton_->sync_domain_, nullptr);
	try {
		singleton_->module_bind (exe._get_ptr (), exe.metadata (), nullptr);
		try {
			singleton_->binary_map_.add (exe);
		} catch (...) {
			release_imports (exe._get_ptr (), exe.metadata ());
			throw;
		}
		rq->success ();
	} catch (CORBA::UserException& ex) {
		rq->set_exception (std::move (ex));
	}
	SYNC_END ();
	CORBA::Internal::ProxyRoot::check_request (rq->_get_ptr ());
	return startup;
}

inline
void Binder::unbind (Executable& mod) noexcept
{
	SYNC_BEGIN (singleton_->sync_domain_, nullptr);
	singleton_->binary_map_.remove (mod);
	SYNC_END ();
	release_imports (mod._get_ptr (), mod.metadata ());
}

}
}

#endif
