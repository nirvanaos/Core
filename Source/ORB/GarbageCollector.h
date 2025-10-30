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
#ifndef NIRVANA_ORB_CORE_GARBAGECOLLECTOR_H_
#define NIRVANA_ORB_CORE_GARBAGECOLLECTOR_H_
#pragma once

#include "../Runnable.h"

namespace Nirvana {
namespace Core {
class SyncContext;
}
}

namespace CORBA {
namespace Core {

class NIRVANA_NOVTABLE SyncGC
{
	template <class> friend class CORBA::servant_reference;
	virtual void _add_ref () noexcept = 0;
	virtual void _remove_ref () noexcept = 0;
};

class GarbageCollector :
	public Nirvana::Core::Runnable
{
public:
	static void schedule (SyncGC& garbage, Nirvana::Core::SyncContext& sync_context) noexcept;

	GarbageCollector (SyncGC& garbage) :
		ref_ (&garbage)
	{}

private:
	virtual void run () noexcept override;

private:
	servant_reference <SyncGC> ref_;
};

}
}

#endif
