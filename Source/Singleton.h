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
#ifndef NIRVANA_CORE_SINGLETON_H_
#define NIRVANA_CORE_SINGLETON_H_
#pragma once

#include "Module.h"
#include "SyncDomain.h"

namespace Nirvana {
namespace Core {

/// Singleton module.
/// Static objects live in the common synchronization domain.
class Singleton :
	public Module,
	public SyncDomain
{
public:
	Singleton (int32_t id, Port::Module&& bin, const ModuleStartup& startup_entry) :
		Module (id, std::move (bin), startup_entry),
		SyncDomain (MemContext::create ()),
		sync_context_type_ (SyncContext::Type::SYNC_DOMAIN_SINGLETON)
	{}

	void _add_ref () noexcept override
	{
		Module::_add_ref ();
	}

	void _remove_ref () noexcept override
	{
		Module::_remove_ref ();
	}

	// Module::

	virtual SyncContext& sync_context () noexcept override;
	virtual void atexit (AtExitFunc f) override;
	virtual void execute_atexit () noexcept override;
	virtual MemContext* initterm_mem_context () const noexcept override;
	virtual void terminate () noexcept override;

	// SyncContext::

	virtual SyncContext::Type sync_context_type () const noexcept override;
	virtual Module* module () noexcept override;

private:
	AtExitSync <UserAllocator> at_exit_;

	SyncContext::Type sync_context_type_;
};

}
}

#endif
