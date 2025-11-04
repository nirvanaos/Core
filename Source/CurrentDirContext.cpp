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
#include "CurrentDirContext.h"
#include "ORB/Services.h"
#include "NameService/FileSystem.h"

namespace Nirvana {
namespace Core {

void CurrentDirContext::chdir (const IDL::String& path)
{
	if (path.empty ()) {
		current_dir_.clear ();
		return;
	}

	CosNaming::Name new_dir = get_name_from_path (path);

	// Check that new directory exists
	Dir::_ref_type dir = Dir::_narrow (name_service ()->resolve (new_dir));
	if (!dir)
		throw_INTERNAL (make_minor_errno (ENOTDIR));

	if (dir->_non_existent ())
		throw_OBJECT_NOT_EXIST (make_minor_errno (ENOENT));

	current_dir_ = std::move (new_dir);
}

CosNaming::Name CurrentDirContext::default_dir ()
{
	CosNaming::Name home (2);
	home.back ().id ("home");
	return home;
}

CosNaming::Name CurrentDirContext::get_name_from_path (const IDL::String& path) const
{
	IDL::String translated;
	const IDL::String* ppath;
	if (Port::FileSystem::translate_path (path, translated))
		ppath = &translated;
	else
		ppath = &path;

	CosNaming::Name name;
	if (!FileSystem::is_absolute (*ppath)) {
		if (!current_dir_.empty ())
			name = current_dir_;
		else
			name = default_dir ();
	}
	FileSystem::append_path (name, *ppath);
	return name;
}

CosNaming::NamingContext::_ref_type CurrentDirContext::name_service ()
{
	return CosNaming::NamingContext::_narrow (CORBA::Core::Services::bind (CORBA::Core::Services::NameService));
}

}
}
