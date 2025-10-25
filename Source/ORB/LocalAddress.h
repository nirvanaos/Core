/// \file
/*
* Nirvana IDL support library.
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
#ifndef NIRVANA_ORB_CORE_LOCALADDRESS_H_
#define NIRVANA_ORB_CORE_LOCALADDRESS_H_
#pragma once

#include "../SharedAllocator.h"
#include "../StaticallyAllocated.h"
#include "IDL/ORB/IIOP.h"

namespace CORBA {
namespace Core {

class LocalAddress
{
public:
	LocalAddress () :
		port_ (0)
	{}

	LocalAddress (const IIOP::ListenPoint& lp) :
		host_ (lp.host ().data (), lp.host ().size ()),
		port_ (lp.port ())
	{}

	const Nirvana::Core::SharedString& host () const noexcept
	{
		return host_;
	}

	UShort port () const noexcept
	{
		return port_;
	}

	LocalAddress& operator = (const IIOP::ListenPoint& lp)
	{
		host_.assign (lp.host ().data (), lp.host ().size ());
		port_ = lp.port ();
		return *this;
	}

	bool operator == (const IIOP::ListenPoint& lp) const noexcept
	{
		return port_ == lp.port () &&
			host_.compare (0, host_.npos, lp.host ().data (), lp.host ().size ()) == 0;
	}

	static LocalAddress& singleton () noexcept
	{
		return singleton_;
	}

	static void initialize () noexcept
	{
		singleton_.construct ();
	}

	static void initialize (const IIOP::ListenPoint& lp) noexcept
	{
		singleton_.construct (std::ref (lp));
	}

	static void terminate () noexcept
	{
		singleton_.destruct ();
	}

private:
	Nirvana::Core::SharedString host_;
	UShort port_;

	static Nirvana::Core::StaticallyAllocated <LocalAddress> singleton_;
};

}
}

#endif
