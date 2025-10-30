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
#include "GarbageCollector.h"
#include "../ExecDomain.h"
#include "../Chrono.h"
#include "../Synchronized.h"

using namespace Nirvana;
using namespace Nirvana::Core;

namespace CORBA {
namespace Core {

void GarbageCollector::schedule (SyncGC& garbage, Nirvana::Core::SyncContext& sync_context)
noexcept
{
	assert (sync_context.sync_domain ());
	if (Scheduler::async_GC_allowed ()) {
		try {
			DeadlineTime deadline =
				GC_DEADLINE == INFINITE_DEADLINE ?
				INFINITE_DEADLINE : Nirvana::Core::Chrono::make_deadline (GC_DEADLINE);

			ExecDomain::async_call <GarbageCollector> (deadline, sync_context, nullptr, std::ref (garbage));
			return;
		} catch (...) {
			// Async call failed, maybe resources are exausted.
			// Fallback to collect garbage synchronous.
		}
	}
	try {
		servant_reference <SyncGC> ref;
		SYNC_BEGIN (sync_context, nullptr)
			ref = nullptr;
		SYNC_END ()
	} catch (...) {
		assert (false);
	}
}

void GarbageCollector::run () noexcept
{
	try {
		ref_ = nullptr;
	} catch (...) {
		assert (false);
	}
}

}
}
