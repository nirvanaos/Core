/// \file Tagged sequense sort/search.
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
#ifndef NIRVANA_ORB_CORE_TAGGED_SEQ_H_
#define NIRVANA_ORB_CORE_TAGGED_SEQ_H_
#pragma once

#include <CORBA/CORBA.h>
#include "IDL/ORB/IOP.h"

namespace CORBA {
namespace Core {

void sort (IOP::TaggedComponentSeq& seq) noexcept;

const IOP::TaggedComponent* binary_search (const IOP::TaggedComponentSeq& seq,
	IOP::ComponentId id) noexcept;

inline
void sort (IOP::ServiceContextList& seq) noexcept
{
	sort (reinterpret_cast <IOP::TaggedComponentSeq&> (seq));
}

inline
const IOP::ServiceContext* binary_search (const IOP::ServiceContextList& seq,
	IOP::ServiceId id) noexcept
{
	return reinterpret_cast <const IOP::ServiceContext*> (
		binary_search (reinterpret_cast <const IOP::TaggedComponentSeq&> (seq), id));
}

inline
void sort (IOP::TaggedProfileSeq& seq) noexcept
{
	sort (reinterpret_cast <IOP::TaggedComponentSeq&> (seq));
}

inline
const IOP::TaggedProfile* binary_search (const IOP::TaggedProfileSeq& seq,
	IOP::ProfileId id) noexcept
{
	return reinterpret_cast <const IOP::TaggedProfile*> (
		binary_search (reinterpret_cast <const IOP::TaggedComponentSeq&> (seq), id));
}

}
}

#endif
