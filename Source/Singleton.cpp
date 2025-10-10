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
#include "Singleton.h"

namespace Nirvana {
namespace Core {

SyncContext& Singleton::sync_context () noexcept
{
	return *this;
}

SyncContext::Type Singleton::sync_context_type () const noexcept
{
	return sync_context_type_;
}

void Singleton::atexit (AtExitFunc f)
{
	at_exit_.atexit (f);
}

void Singleton::execute_atexit () noexcept
{
	at_exit_.execute ();
}

MemContext* Singleton::initterm_mem_context () const noexcept
{
	return &SyncDomain::mem_context ();
}

void Singleton::terminate () noexcept
{
	sync_context_type_ = SyncContext::Type::SINGLETON_TERM;
	Module::terminate ();
	sync_context_type_ = SyncContext::Type::SYNC_DOMAIN_SINGLETON;
}

Module* Singleton::module () noexcept
{
	return this;
}

}
}
