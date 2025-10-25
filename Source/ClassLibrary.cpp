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
#include "ClassLibrary.h"
#include "MemContext.h"

namespace Nirvana {
namespace Core {

void ClassLibrary::initialize ()
{
	sync_context_type_ = SyncContext::Type::FREE_MODULE_INIT;
	try {
		Module::initialize ();
	} catch (...) {
		sync_context_type_ = SyncContext::Type::FREE;
		throw;
	}
	sync_context_type_ = SyncContext::Type::FREE;

	initterm_mem_context_ = MemContext::current_ptr ();

	if (Port::Memory::FLAGS & Memory::ACCESS_CHECK) {

		// Make global data read-only

		if (initterm_mem_context_)
			initterm_mem_context_->heap ().change_protection (true);

		for (auto it = data_sections_.cbegin (); it != data_sections_.cend (); ++it) {
			void* p = const_cast <void*> (it->address);
			size_t cb = it->size;
			Port::Memory::copy (p, p, cb, Memory::READ_ONLY | Memory::EXACTLY);
		}
	}
}

void ClassLibrary::terminate () noexcept
{
	if (Port::Memory::FLAGS & Memory::ACCESS_CHECK) {
		try {

			// Make global data read-write

			if (initterm_mem_context_)
				initterm_mem_context_->heap ().change_protection (false);

			for (auto it = data_sections_.cbegin (); it != data_sections_.cend (); ++it) {
				void* p = const_cast <void*> (it->address);
				size_t cb = it->size;
				Port::Memory::copy (p, p, cb, Memory::READ_WRITE | Memory::EXACTLY);
			}
		} catch (...) {
			// TODO: Log
			return;
		}
	}
	sync_context_type_ = SyncContext::Type::FREE_MODULE_TERM;
	Module::terminate ();
	sync_context_type_ = SyncContext::Type::FREE;
}

SyncContext::Type ClassLibrary::sync_context_type () const noexcept
{
	return sync_context_type_;
}

Heap* ClassLibrary::stateless_memory ()
{
	if (!initterm_mem_context_)
		initterm_mem_context_ = MemContext::create (true);
	return &initterm_mem_context_->heap ();
}

Module* ClassLibrary::module () noexcept
{
	return this;
}

void ClassLibrary::atexit (AtExitFunc f)
{
	if (!initterm_mem_context_) {
		if (SyncContext::Type::FREE_MODULE_INIT == sync_context_type_)
			initterm_mem_context_ = MemContext::create (true);
		else
			initterm_mem_context_ = &MemContext::current ();
	}
	at_exit_.atexit (initterm_mem_context_->heap (), f);
}

void ClassLibrary::execute_atexit () noexcept
{
	at_exit_.execute ();
}

}
}
