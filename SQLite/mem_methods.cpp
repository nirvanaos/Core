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
#include "mem_methods.h"
#include <Nirvana/c_heap_dbg.h>

using namespace Nirvana;

namespace SQLite {

extern "C" void* xMalloc (int size)
{
	return c_malloc <HeapBlockHdrType> (alignof (std::max_align_t), size);
}

extern "C" void xFree (void* p)
{
	c_free <HeapBlockHdrType> (p);
}

extern "C" void* xRealloc (void* p, int size)
{
	return c_realloc <HeapBlockHdrType> (p, size);
}

extern "C" int xSize (void* p)
{
	return (int)c_usable_size <HeapBlockHdrType> (p);
}

extern "C" int xRoundup (int size)
{
	if (size < (int)(std::numeric_limits <size_t>::max () / 4))
		size = (int)(clp2 (size + sizeof (HeapBlockHdrType)) - sizeof (HeapBlockHdrType));
	return size;
}

extern "C" int xInit (void*) noexcept
{
	return SQLITE_OK;
}

extern "C" void xShutdown (void*) noexcept
{}

const struct sqlite3_mem_methods mem_methods = {
	xMalloc,
	xFree,
	xRealloc,
	xSize,
	xRoundup,
	xInit,
	xShutdown
};

}
