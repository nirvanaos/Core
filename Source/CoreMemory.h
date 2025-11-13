/*
* Nirvana Core.
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
#ifndef NIRVANA_CORE_COREMEMORY_H_
#define NIRVANA_CORE_COREMEMORY_H_
#pragma once

#include "ExecDomain.h"
#include "HeapCustom.h"

namespace Nirvana {
namespace Core {

/// Implementation of the Nirvana::Memory interface.
class CoreMemory :
	public CORBA::servant_traits <Nirvana::Memory>::ServantStatic <CoreMemory>
{
public:
	// Memory::
	static void* allocate (void* dst, size_t& size, unsigned flags)
	{
		return heap ().allocate (dst, size, flags);
	}

	static void release (void* p, size_t size)
	{
		return heap ().release (p, size);
	}

	static void commit (void* p, size_t size)
	{
		return heap ().commit (p, size);
	}

	static void decommit (void* p, size_t size)
	{
		return heap ().decommit (p, size);
	}

	static void* copy (void* dst, void* src, size_t& size, unsigned flags)
	{
		return heap ().copy (dst, src, size, flags);
	}

	static bool is_private (const void* p, size_t size)
	{
		return heap ().is_private (p, size);
	}

	static intptr_t query (const void* p, Memory::QueryParam q)
	{
		return heap ().query (p, q);
	}

	static Nirvana::Memory::_ref_type create_heap (size_t granularity)
	{
		return HeapCustom::create (granularity);
	}

private:
	static Heap& heap ()
	{
		ExecDomain* ed = ExecDomain::current_ptr ();
		if (ed)
			return ed->mem_context ().heap ();
		else
			return Heap::shared_heap ();
	}
};

}
}

#endif
