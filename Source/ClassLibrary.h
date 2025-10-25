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
#ifndef NIRVANA_CORE_CLASSLIBRARY_H_
#define NIRVANA_CORE_CLASSLIBRARY_H_
#pragma once

#include "Module.h"
#include "SyncContext.h"

namespace Nirvana {
namespace Core {

/// Class library.
/// Static objects live in the free synchronization domain.
class ClassLibrary : 
	public Module,
	public SyncContextFree
{
public:
	ClassLibrary (int32_t id, Port::Module&& bin, const ModuleStartup& startup_entry) :
		Module (id, std::move (bin), startup_entry),
		sync_context_type_ (Type::FREE)
	{
		get_data_sections (data_sections_);
	}

	// Module::

	virtual SyncContext& sync_context () noexcept override
	{
		return *this;
	}

	virtual void atexit (AtExitFunc f) override;
	virtual void execute_atexit () noexcept override;

	virtual void initialize () override;
	virtual void terminate () noexcept override;

	virtual MemContext* initterm_mem_context () const noexcept override
	{
		return initterm_mem_context_;
	}

	// SyncContext::

	virtual Type sync_context_type () const noexcept override;
	virtual Heap* stateless_memory () override;
	virtual Module* module () noexcept override;

private:
	void _add_ref () noexcept override
	{
		Module::_add_ref ();
	}

	void _remove_ref () noexcept override
	{
		Module::_remove_ref ();
	}

private:
	DataSections data_sections_;
	AtExitAsync at_exit_;

	// Initialization/termination memory context may be nil.
	Ref <MemContext> initterm_mem_context_;

	SyncContext::Type sync_context_type_;
};

}
}

#endif
