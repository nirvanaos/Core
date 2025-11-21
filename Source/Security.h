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
#ifndef NIRVANA_CORE_SECURITY_H_
#define NIRVANA_CORE_SECURITY_H_
#pragma once

#include <Port/Security.h>

namespace Nirvana {
namespace Core {

class Security : private Port::Security
{
	typedef Port::Security Base;

public:
	// Implementation - specific methods must be called explicitly.
	Base& port () noexcept
	{
		return *this;
	}

	class Context : private Port::Security::Context
	{
		typedef Port::Security::Context Base;

	public:
		typedef Base::ABI ABI;

		static const Context& current () noexcept;

		// Implementation - specific methods must be called explicitly.
		Base& port () noexcept
		{
			return *this;
		}

		const Base& port () const noexcept
		{
			return *this;
		}

		Context () = default;
		
		explicit Context (ABI abi) noexcept :
			Base (abi)
		{}
		
		Context (const Context& src) = default;

		Context (Context&& src) noexcept = default;

		~Context () = default;

		Context& operator = (const Context& src) = default;
		Context& operator = (Context&& src) noexcept = default;

		void clear () noexcept
		{
			Base::clear ();
		}

		bool empty () const noexcept
		{
			return Base::empty ();
		}

		SecurityId security_id () const
		{
			return Base::security_id ();
		}

		ABI abi () const noexcept
		{
			return Base::abi ();
		}

		void detach () noexcept
		{
			Base::detach ();
		}

		friend class Security;
	};

	/// \returns Current protection domain owner.
	static const Context& prot_domain_context () noexcept
	{
		return static_cast <const Context&> (Base::prot_domain_context ());
	}

	/// \brief Check unmarshalled security context ABI for validity.
	static bool is_valid_context (Context::ABI context) noexcept
	{
		return Base::is_valid_context (context);
	}

	/// \brief Get display name of the principal.
	///
	/// \param id Principal ID.
	/// \returns Principal name.
	static IDL::String get_name (const SecurityId& id)
	{
		return Base::get_name (id);
	}

	static void get_entropy (void* buf, size_t size)
	{
		Base::get_entropy (buf, size);
	}
};

}
}


#endif
