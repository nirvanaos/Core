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
#include "File.h"

namespace Nirvana {
namespace Core {

void File::check_exist ()
{
	if (type () == FileType::not_found)
		throw CORBA::OBJECT_NOT_EXIST (make_minor_errno (ENOENT));
}

void File::etherealize ()
{
	Base::etherealize ();
	_default_POA ()->deactivate_object (id ());
}

void File::check_flags (unsigned flags) const
{
	unsigned acc_mode = access_->flags () & O_ACCMODE;
	if ((acc_mode == O_WRONLY || acc_mode == O_RDONLY) && acc_mode != (flags & O_ACCMODE))
		throw_NO_PERMISSION (make_minor_errno (EACCES)); // File is unaccessible for this mode
}

}
}
