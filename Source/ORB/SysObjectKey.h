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
#ifndef NIRVANA_ORB_CORE_SYSOBJECTKEY_H_
#define NIRVANA_ORB_CORE_SYSOBJECTKEY_H_
#pragma once

#include <CORBA/CORBA.h>
#include "IDL/ORB/IOP.h"
#include <algorithm>
#include <iterator>

namespace CORBA {
namespace Core {

template <class Servant> struct SysObjectKey
{
	static const Octet key [];

	static bool equal (const Octet* obj_key, size_t len) noexcept
	{
		return len == std::size (key)
			&& std::equal (key, std::end (key), obj_key);
	}

	static bool equal (const IOP::ObjectKey& object_key) noexcept
	{
		return equal (object_key.data (), object_key.size ());
	}

	static IOP::ObjectKey object_key ()
	{
		return IOP::ObjectKey (std::begin (key), std::end (key));
	}
};

}
}
#endif
