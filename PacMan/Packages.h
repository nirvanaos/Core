/*
* Nirvana package manager.
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
#ifndef PACMAN_PACKAGES_H_
#define PACMAN_PACKAGES_H_
#pragma once

#include <CORBA/Server.h>
#include <Nirvana/Domains_s.h>
#include "Connection.h"
#include "PacFactory.h"

class Packages :
	public CORBA::servant_traits <Nirvana::PM::Packages>::Servant <Packages>
{
	typedef CORBA::servant_traits <Nirvana::PM::Packages>::Servant <Packages> Servant;

	static const unsigned POOL_MAX_CACHE = 10;
	static const unsigned POOL_MAX_CREATE = 1000;

public:
	Packages (CORBA::Object::_ptr_type component);

	~Packages ()
	{}

	void get_binding (const IDL::String& name, Nirvana::PlatformId platform,
		Nirvana::PM::Binding& binding) const
	{
		Connection (pool_).get_binding (name, platform, binding);
	}

	Nirvana::PM::ModuleInfo get_module_info (Nirvana::ModuleId id) const
	{
		return Connection (pool_).get_module_info (id);
	}

	void get_module_bindings (Nirvana::ModuleId id, Nirvana::PM::ModuleBindings& metadata) const
	{
		Connection (pool_).get_module_bindings (id, metadata);
	}

	Nirvana::PM::Packages::Modules get_module_dependencies (Nirvana::ModuleId id) const
	{
		return Connection (pool_).get_module_dependencies (id);
	}

	Nirvana::PM::Packages::Modules get_dependent_modules (Nirvana::ModuleId id) const
	{
		return Connection (pool_).get_dependent_modules (id);
	}

	Nirvana::PM::Packages::Modules get_modules (const IDL::String& name_filter) const
	{
		return Connection (pool_).get_modules (name_filter);
	}

	Nirvana::PM::PacMan::_ref_type manage () const
	{
		return manager_->allocate ();
	}

private:
	static void create_database ();

private:
	NDBC::ConnectionPool::_ref_type pool_;
	Nirvana::PM::Manager::_ref_type manager_;

	static const char* const db_script_ [];
};

#endif
