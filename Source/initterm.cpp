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
#include "initterm.h"
#include "Chrono.h"
#include "Binder.h"
#include "Scheduler.h"
#include "ExecDomain.h"
#include "ThreadBackground.h"
#include "Timer.h"
#include "ORB/ORB_initterm.h"
#include "ORB/Services.h"
#include "ORB/LocalAddress.h"
#include "ORB/SysAdapterActivator.h"
#include "ORB/POA_Root.h"
#include <Port/PostOffice.h>
#include "ProtDomain.h"
#include "NameService/NameService.h"
#include "CoreModule.h"

namespace Nirvana {
namespace Core {

void initialize0 () noexcept
{
	Chrono::initialize ();
	g_core_free_sync_context.construct ();
	CoreModule::initialize ();
	Timer::initialize ();
	ThreadBackground::initialize ();
	ExecDomain::initialize ();
	Scheduler::initialize ();
	Heap::initialize ();
}

void initialize ()
{
	// CORBA::Core::Services must be initialized before Binder
	CORBA::Core::initialize ();

	Binder::initialize ();

	ProtDomain::initialize ();
	// Bind ProtDomain service to hold proxy
	CORBA::Core::Services::bind (CORBA::Core::Services::ProtDomain);

	if (ESIOP::is_system_domain ())
		PortableServer::Core::SysAdapterActivator::initialize ();

	// Start receiving messages from other domains
	Port::PostOffice::initialize (CORBA::Core::LocalAddress::singleton ().host (), CORBA::Core::LocalAddress::singleton ().port ());

	ObjectPoolBase::housekeeping_start ();
}

void shutdown () noexcept
{
	ObjectPoolBase::housekeeping_stop ();

	// Deactivate all managers to reject incoming requests.
	PortableServer::Core::POA_Root::deactivate_all ();
	
	// Block incoming requests and complete currently executing ones.
	PortableServer::Core::POA_Root::shutdown ();
}

void terminate2 () noexcept
{
	if (ESIOP::is_system_domain ())
		CosNaming::Core::NameService::terminate ();

	// Release all service proxies.
	CORBA::Core::Services::terminate ();

	// Clear remote references, if any
	Binder::clear_remote_references ();

	// Disable all timers
	Timer::terminate ();

	if (ESIOP::is_system_domain ())
		PortableServer::Core::SysAdapterActivator::terminate ();
}

void terminate1 () noexcept
{
	// Stop receiving messages. But we can still send messages.
	Port::PostOffice::terminate ();

	ProtDomain::terminate ();
	Binder::terminate ();
	CORBA::Core::terminate ();
}

void terminate0 () noexcept
{
	Heap::terminate ();
	Scheduler::terminate ();
	ExecDomain::terminate ();
	ThreadBackground::terminate ();
	CoreModule::terminate ();
#ifndef NDEBUG
	g_core_free_sync_context.destruct ();
#endif
	Chrono::terminate ();
}

}
}
