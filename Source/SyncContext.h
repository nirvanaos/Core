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
#ifndef NIRVANA_CORE_SYNCCONTEXT_H_
#define NIRVANA_CORE_SYNCCONTEXT_H_
#pragma once

#include "CoreInterface.h"
#include "Thread.h"
#include "StaticallyAllocated.h"
#include <Port/config.h>

namespace Nirvana {
namespace Core {

class Heap;
class SyncDomain;
class Module;

/// Synchronization context pure interface.
/// Synchronization context may be associated with:
///   - Synchronization domain
///   - Free synchronization context for core stateless objects
///   - Free synchronization context for class library
///   - Executable
class NIRVANA_NOVTABLE SyncContext
{
DECLARE_CORE_INTERFACE

public:
	/// \returns Current synchronization context.
	static SyncContext& current () noexcept;

	/// Synchronization and execution context type.
	/// See Nirvana::System::ContextType - it must be equivalent.
	enum class Type
	{
		/// Execution context is a Nirvana process.
		/// Global variables are read-write.
		/// Object creation is prohibited.
		PROCESS,

		/// This is a synchronization domain.
		/// Global variables are read-only.
		SYNC_DOMAIN,

		/// Free synchronization context.
		/// Global variables are read-only.
		FREE,

		/// Class library initialization code executed in the free synchronization context.
		/// Global variables are read-write.
		FREE_MODULE_INIT,

		/// Class library termination code executed in the free synchronization context.
		/// Global variables are read-write.
		/// Object creation is prohibited.
		/// Object binding is prohibited.
		FREE_MODULE_TERM,

		/// This is synchronization domain in singleton module.
		/// Global variables are read-write.
		SYNC_DOMAIN_SINGLETON,

		/// Singleton termination code executed in the synchronization domain.
		/// Global variables are read-write.
		/// Object creation is prohibited.
		/// Object binding is prohibited.
		SINGLETON_TERM
	};

	virtual Type sync_context_type () const noexcept = 0;

	/// \returns SyncDomain associated with this context.
	///   Returns `nullptr` if this context is not a synchronization domain.
	/// 
	/// This method is implemented inline for the performance reasons.
	SyncDomain* sync_domain () noexcept;

	/// \returns `true` if this context is free sync context.
	bool is_free_sync_context () const noexcept
	{
		Type t = sync_context_type ();
		return Type::FREE <= t && t < Type::SYNC_DOMAIN_SINGLETON;
	}

	/// \returns Free sync context returns pointer to the stateless objects heap.
	///          Other sync contexts return `nullptr`.
	virtual Heap* stateless_memory ();

	/// Returns the Module object associated with this context.
	/// 
	/// \returns Pointer to the Module object.
	///          Core and process executable contexts return `nullptr`.
	virtual Module* module () noexcept = 0;

	/// \returns `true` if current execution context is process.
	bool is_process () const noexcept
	{
		return sync_context_type () == SyncContext::Type::PROCESS;
	}

protected:
	SyncContext (bool sync_domain) :
		sync_domain_ (sync_domain)
	{}

private:
	const bool sync_domain_;
};

/// Free (not synchronized) sync context.
class NIRVANA_NOVTABLE SyncContextFree :
	public SyncContext
{
public:
	virtual Heap* stateless_memory () override = 0;
	virtual Type sync_context_type () const noexcept override;

protected:
	SyncContextFree () :
		SyncContext (false)
	{}
};

/// Core free sync context.
class SyncContextCore :
	public SyncContextFree
{
public:
	virtual Heap* stateless_memory () noexcept override;
	virtual Module* module () noexcept override;
};

extern StaticallyAllocated <ImplStatic <SyncContextCore> > g_core_free_sync_context;

}
}

#endif
