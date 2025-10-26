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
#ifndef NIRVANA_CORE_MODULE_H_
#define NIRVANA_CORE_MODULE_H_
#pragma once

#include <CORBA/Server.h>
#include "Binary.h"
#include <Nirvana/Module_s.h>
#include <Nirvana/ModuleInit.h>
#include "AtomicCounter.h"
#include "BinderObject.h"
#include "AtExit.h"

namespace Nirvana {
namespace Core {

class SyncContext;
class Binder;

/// Loadable module
class NIRVANA_NOVTABLE Module :
	public BinderObject,
	public Binary,
	public CORBA::servant_traits <Nirvana::Module>::Servant <Module>,
	public CORBA::Internal::LifeCycleRefCnt <Module>
{
public:
	/// Derived ClassLibrary and Singleton classes must have virtual destructors.
	virtual ~Module ()
	{}

	/// \returns Synchronization context.
	///   If module is ClassLibrary, returned context is free context.
	///   If module is Singleton, returned context is the singleton synchronization domain.
	virtual SyncContext& sync_context () noexcept = 0;

	/// Called on the_module reference _duplicate().
	void _add_ref () noexcept
	{
		ref_cnt_.increment ();
	}

	/// Called on the_module reference _release().
	void _remove_ref () noexcept;

	/// \returns Current reference count.
	AtomicCounter <false>::IntegralType _refcount_value () const noexcept
	{
		return ref_cnt_;
	}

	/// \returns Module name.
	const char* name () const noexcept
	{
		return startup_entry_.name;
	}

	/// \returns `true` if module is singleton library.
	bool is_singleton () const noexcept
	{
		return flags () & OLF_MODULE_SINGLETON;
	}

	/// \returns Module metadata flags.
	unsigned flags () const noexcept
	{
		return (unsigned)startup_entry_.flags;
	}

	/// Called when module is loaded completely
	void on_load_complete () noexcept
	{
		initial_ref_cnt_ = ref_cnt_.load ();
	}

	/// If bound () returns `false`, the module may be safely unloaded.
	/// 
	/// \returns `true` if some objects of this module are bound from other modules.
	bool bound () const
	{
		return initial_ref_cnt_ < ref_cnt_;
	}

	bool can_be_unloaded (const SteadyTime& t) const
	{
		return !bound () && release_time_ <= t;
	}

	virtual void initialize ();
	virtual void terminate () noexcept;

	virtual void raise_exception (CORBA::SystemException::Code code, unsigned minor) override;

	virtual void atexit (AtExitFunc f) = 0;
	virtual void execute_atexit () noexcept = 0;

	virtual MemContext* initterm_mem_context () const noexcept = 0;

	int32_t id () const noexcept
	{
		return id_;
	}

protected:
	Module (int32_t id, Port::Module&& bin, const ModuleStartup& startup_entry);

protected:
	friend class Binder;

	const ModuleStartup& startup_entry_;
	ModuleInit::_ptr_type entry_point_;
	SteadyTime release_time_;
	AtomicCounter <false> ref_cnt_;
	AtomicCounter <false>::IntegralType initial_ref_cnt_;
	int32_t id_;
};

}
}

#endif
