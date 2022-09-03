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
#ifndef NIRVANA_ORB_CORE_MAPUNORDEREDUNSTABLE_H_
#define NIRVANA_ORB_CORE_MAPUNORDEREDUNSTABLE_H_
#pragma once

#pragma push_macro ("verify")
#undef verify
#include "parallel-hashmap/parallel_hashmap/phmap.h"
#pragma pop_macro ("verify")

namespace Nirvana {
namespace Core {

/// Unordered map without the pointer stability.
template <class Key, class T, class Hash = std::hash <Key>, class KeyEqual = std::equal_to <Key>,
	class Allocator = std::allocator <std::pair <const Key, T> > >
	using MapUnorderedUnstable = phmap::flat_hash_map <Key, T, Hash, KeyEqual, Allocator>;

template <class Key, class Hash = std::hash <Key>, class KeyEqual = std::equal_to <Key>,
	class Allocator = std::allocator <Key> >
	using SetUnorderedUnstable = phmap::flat_hash_set <Key, Hash, KeyEqual, Allocator>;

}
}

#endif
