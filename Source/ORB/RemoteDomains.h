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
#ifndef NIRVANA_ORB_CORE_REMOTEDOMAINS_H_
#define NIRVANA_ORB_CORE_REMOTEDOMAINS_H_
#pragma once

#include "DomainRemote.h"
#include "../MapUnorderedStable.h"
#include "IDL/ORB/IIOP.h"
#include <list>

namespace std {

template <>
struct hash <IIOP::ListenPoint>
{
	size_t operator () (const IIOP::ListenPoint& lp) const noexcept;
};

template <>
struct equal_to <IIOP::ListenPoint>
{
	bool operator () (const IIOP::ListenPoint& l, const IIOP::ListenPoint& r) const noexcept
	{
		return l.port () == r.port () && l.host () == r.host ();
	}
};

}

namespace CORBA {
namespace Core {

class RemoteDomains
{
public:
	servant_reference <DomainRemote> get (const IIOP::ListenPoint& lp);

	void erase (const IIOP::ListenPoint& lp) noexcept
	{
		listen_points_.erase (lp);
	}

	bool housekeeping (const TimeBase::TimeT& cur_time) noexcept
	{
		for (auto it = domains_.begin (); it != domains_.end (); ++it) {
			if (it->is_garbage (cur_time))
				it = domains_.erase (it);
			else
				++it;
		}
		return !domains_.empty ();
	}

	void shutdown () noexcept
	{
		for (auto it = domains_.begin (); it != domains_.end (); ++it) {
			it->shutdown ();
		}
	}

private:
	typedef std::list <DomainRemote, Nirvana::Core::BinderMemory::Allocator <DomainRemote> > Domains;

	typedef Nirvana::Core::MapUnorderedStable <IIOP::ListenPoint, DomainRemote*, std::hash <IIOP::ListenPoint>,
		std::equal_to <IIOP::ListenPoint>, Nirvana::Core::BinderMemory::Allocator> ListenPoints;

	ListenPoints listen_points_;
	Domains domains_;
};

}
}

#endif
