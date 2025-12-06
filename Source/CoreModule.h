/*
* Nirvana Core.
*
* This is a part of the Nirvana project.
*
* Author: Igor Popov
*
* Copyright (c) 2025 Igor Popov.
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
#include <CORBA/Server.h>
#include <Nirvana/Module_s.h>
#include "TLS.h"
#include "StaticallyAllocated.h"

namespace Nirvana {
namespace Core {

/// Module interface implementation for core static objects.
class CoreModule :
	public CORBA::servant_traits <Nirvana::Module>::ServantStatic <CoreModule>
{
public:
	static void initialize () noexcept
	{
		tls_.construct ();
	}

	static void terminate () noexcept
	{
		tls_.destruct ();
	}
	
	static const void* base_address () noexcept
	{
		return nullptr;
	}

	static int32_t id () noexcept
	{
		return 0;
	}

	template <class Itf>
	static CORBA::Internal::Interface* __duplicate (CORBA::Internal::Interface* itf, CORBA::Internal::Interface*) noexcept
	{
		return itf;
	}

	template <class Itf>
	static void __release (CORBA::Internal::Interface*) noexcept
	{}

	void atexit (AtExitFunc f)
	{
		throw_NO_IMPLEMENT ();
	}

	unsigned CS_alloc (Deleter deleter)
	{
		return tls_->CS_alloc (deleter);
	}

	void CS_free (unsigned idx)
	{
		tls_->CS_free (idx);
	}

	void CS_set (unsigned idx, void* p)
	{
		tls_->CS_set (idx, p);
	}

	void* CS_get (unsigned idx) noexcept
	{
		return tls_->CS_get (idx);
	}

private:
	static StaticallyAllocated <TLS> tls_;
};

}
}
