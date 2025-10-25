/*
* Nirvana package manager.
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
#ifndef PACMAN_MANAGER_H_
#define PACMAN_MANAGER_H_
#pragma once

#include "PacMan.h"
#include "PacFactory_s.h"

class Manager :
	public CORBA::servant_traits <Nirvana::PM::Manager>::Servant <Manager>
{
public:
	Manager () noexcept :
		in_progress_ (false)
	{}

	~Manager ()
	{}

	void on_complete () noexcept
	{
		assert (in_progress_);
		in_progress_ = false;
	}

	Nirvana::PM::PacMan::_ref_type allocate ()
	{
		Nirvana::PM::PacMan::_ref_type obj;
		if (!in_progress_) {
			obj = CORBA::make_reference <PacMan> (this)->_this ();
			if (in_progress_)
				obj = nullptr;
			in_progress_ = true;
		}
		return obj;
	}

private:
	bool in_progress_;
};

#endif
