/*
* Nirvana package manager.
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
#include "PacMan.h"
#include "Manager.h"

PacMan::Lock::Lock (PacMan& obj) :
	obj_ (obj)
{
	if (!obj.connection ())
		Nirvana::throw_TRANSACTION_UNAVAILABLE ();

	if (obj.busy_)
		Nirvana::throw_INVALID_TRANSACTION ();
	obj.busy_ = true;
}

PacMan::~PacMan ()
{
	if (manager_)
		manager_->on_complete ();
}

long PacMan::compare_name (const Nirvana::PM::ObjBinding& l, const Nirvana::PM::ObjBinding& r) noexcept
{
	long cmp = l.name ().compare (r.name ());
	if (cmp != 0)
		return cmp;

	return version (r.major (), r.minor ()) - version (l.major (), l.minor ());
}

void PacMan::complete () noexcept
{
	manager_->on_complete ();
	manager_ = nullptr;
}
