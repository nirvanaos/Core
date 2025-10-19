/*
* Nirvana SQLite driver.
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
#include "Global.h"
#include "mem_methods.h"
#include "mutex_methods.h"
#include "filesystem.h"
#include <Nirvana/System.h>
#include <Nirvana/POSIX.h>
#include <Nirvana/posix_defs.h>
#include <Nirvana/signal_defs.h>
#include <Nirvana/Module.h>

namespace SQLite {

inline
Global::Global () :
	file_system_ (Nirvana::FileSystem::_narrow (CosNaming::NamingContext::_narrow (
		CORBA::the_orb->resolve_initial_references ("NameService"))->resolve (CosNaming::Name (1)))),
	cs_key_ (-1)
{
	sqlite3_config (SQLITE_CONFIG_PAGECACHE, nullptr, 0, 0);
	sqlite3_config (SQLITE_CONFIG_MALLOC, &mem_methods);

//	sqlite3_config (SQLITE_CONFIG_MUTEX, &mutex_methods);

	sqlite3_vfs_register (&vfs, 1);
	sqlite3_initialize ();

	cs_key_ = Nirvana::the_module->CS_alloc (wsd_deleter);
}

inline
Global::~Global ()
{
	sqlite3_shutdown ();
	Nirvana::the_module->CS_free (cs_key_);
}

Global global;

Nirvana::AccessDirect::_ref_type Global::open_file (const char* path, uint_fast16_t flags) const
{
	// SQLite guarantees that the zFilename parameter to xOpen is either a NULL pointer or string
	// obtained from xFullPathname() with an optional suffix added.
	// If a suffix is added to the zFilename parameter, it will consist of a single "-" character
	// followed by no more than 11 alphanumeric and/or "-" characters.
	// SQLite further guarantees that the string will be valid and unchanged until xClose() is called.
	// Because of the previous sentence, the sqlite3_file can safely store a pointer to the filename if
	// it needs to remember the filename for some reason. If the zFilename parameter to xOpen is
	// a NULL pointer then xOpen must invent its own temporary name for the file.
	// Whenever the xFilename parameter is NULL it will also be the case that the flags parameter will
	// include SQLITE_OPEN_DELETEONCLOSE.

	uint_fast16_t open_flags = O_DIRECT;
	if (flags & SQLITE_OPEN_READWRITE)
		open_flags |= O_RDWR;
	if (flags & SQLITE_OPEN_CREATE)
		open_flags |= O_CREAT;
	if (flags & SQLITE_OPEN_EXCLUSIVE)
		open_flags |= O_EXCL;

	Nirvana::Access::_ref_type access;

	if (path) {
		// Convert to name
		CosNaming::Name name = Nirvana::the_system->to_name (path);

		// Remove filesystem root
		name.erase (name.begin ());

		// Open file
		size_t missing_directories = 0;
		try {
			access = file_system_->open (name, open_flags, 0);
		} catch (CosNaming::NamingContext::NotFound& not_found) {
			if ((flags & SQLITE_OPEN_CREATE) &&
				not_found.why () == CosNaming::NamingContext::NotFoundReason::missing_node
				&& not_found.rest_of_name ().size () > 1
			)
				missing_directories = not_found.rest_of_name ().size () - 1;
			else
				throw;
		}

		if (missing_directories) {
			for (CosNaming::Name dir_name (name.begin (), name.begin () + name.size () - missing_directories);;) {
				file_system_->mkdir (dir_name, 0);
				if (dir_name.size () == name.size () - 1)
					break;
				dir_name.push_back (name [dir_name.size ()]);
			}
			
			access = file_system_->open (name, open_flags, 0);
		}

	} else {
		
		// Create temporary file
		CosNaming::Name tmp_dir_name (2);
		tmp_dir_name [0].id ("var");
		tmp_dir_name [1].id ("tmp");
		Nirvana::Dir::_ref_type tmp_dir = Nirvana::Dir::_narrow (file_system_->resolve (tmp_dir_name));

		IDL::String file_name = "sqliteXXXXXX.tmp";
		access = tmp_dir->mkostemps (file_name, 4, open_flags, S_IRWXU | S_IRWXG | S_IRWXO);
	}

	assert (access);
	CORBA::Object::_ref_type obj = access->_to_object ();
	assert (obj);
	return Nirvana::AccessDirect::_narrow (obj);
}

void Global::wsd_deleter (void* p)
{
	delete reinterpret_cast <WritableStaticData*> (p);
}

WritableStaticData& Global::static_data ()
{
	if (cs_key_ < 0) {
		// Initialization stage
		return initial_static_data_;
	} else {
		void* p = Nirvana::the_module->CS_get (cs_key_);
		if (!p) {
			p = new WritableStaticData (initial_static_data_);
			Nirvana::the_module->CS_set (cs_key_, p);
		}
		return *reinterpret_cast <WritableStaticData*> (p);
	}
}

}

extern "C" int sqlite3_wsd_init (int N, int J)
{
	return SQLITE_OK;
}

extern "C" void* sqlite3_wsd_find (void* K, int L)
{
	try {
		return SQLite::global.static_data ().get (K, L);
	} catch (...) {
		Nirvana::the_posix->raise (SIGABRT);
	}
	return nullptr;
}

