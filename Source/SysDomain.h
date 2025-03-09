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
#ifndef NIRVANA_CORE_SYSDOMAIN_H_
#define NIRVANA_CORE_SYSDOMAIN_H_
#pragma once

#include <CORBA/Server.h>
#include <Nirvana/CoreDomains_s.h>
#include <Nirvana/BindErrorUtl.h>
#include "ORB/SysServant.h"
#include "ORB/system_services.h"
#include "ORB/Services.h"
#include <Port/SystemInfo.h>
#include <Port/SysDomain.h>
#include <Nirvana/File.h>
#include "NameService/FileSystem.h"
#include "open_binary.h"
#include "Binary.h"
#include "../PacMan/factory.h"

namespace Nirvana {
namespace Core {

/// System domain
class SysDomain :
	public CORBA::Core::SysServantImpl <SysDomain, Nirvana::SysDomainCore, Nirvana::SysDomain,
		Components::Navigation, Components::Receptacles, Components::Events, Components::CCMObject>
{
public:
	using CORBA::Internal::CCM_ObjectFeatures <SysDomain, Nirvana::SysDomain>::provide_facet;

	SysDomain (CORBA::Object::_ref_type& obj);
	~SysDomain ();

	static Version version () noexcept
	{
		return { 0, 0, 0, 0 };
	}

	static Platforms supported_platforms ()
	{
		return Platforms (Port::SystemInfo::supported_platforms (),
			Port::SystemInfo::supported_platforms () + Port::SystemInfo::SUPPORTED_PLATFORM_CNT);
	}

	Nirvana::SysDomain::_ref_type login (const IDL::String& user, const IDL::String& password)
	{
		return _this ();
	}

	static Nirvana::ProtDomain::_ref_type prot_domain ()
	{
		return Nirvana::ProtDomain::_narrow (
			CORBA::Core::Services::bind (CORBA::Core::Services::ProtDomain));
	}

	void get_binding (const IDL::String& name, unsigned platform, Binding& binding) const
	{
		if (get_sys_binding (name, platform, binding))
			return;

		PM::Binding pm_binding;
		packages_->get_binding (name, platform, pm_binding);
		if (pm_binding.module_flags () & PM::MODULE_FLAG_SINGLETON) {
			throw CORBA::NO_IMPLEMENT (); // Singletons are not supported yet
		}
		ModuleLoad& module_load = binding.module_load ();
		module_load.binary (open_binary (pm_binding.binary_path ()));
		module_load.module_id (pm_binding.module_id ());
	}

	static bool get_sys_binding (CORBA::Internal::String_in name, unsigned platform, Binding& binding);

	PM::Packages::_ref_type provide_packages () const noexcept
	{
		return packages_;
	}

	Nirvana::SysManager::_ref_type provide_manager () const noexcept
	{
		return manager_;
	}

	int32_t exec (const IDL::String& path, const StringSeq& argv, const IDL::String& work_dir,
		const FileDescriptors& files)
	{
		AccessDirect::_ref_type binary = open_binary (path);
		PlatformId platform = Binary::get_platform (binary);
		if (!Binary::is_supported_platform (platform))
			BindError::throw_unsupported_platform (platform);

		if (SINGLE_DOMAIN)
			return the_shell->process (binary, argv, work_dir, files);
		else {
			Nirvana::ProtDomain::_ref_type domain = manager_->create_prot_domain (platform);
			Nirvana::Shell::_ref_type shell = Nirvana::Shell::_narrow (domain->bind ("Nirvana/the_shell"));
			int32_t ret = shell->process (binary, argv, work_dir, files);
			domain->shutdown (0);
			return ret;
		}
	}

	BinaryInfo get_module_bindings (const IDL::String& path, PM::ModuleBindings& bindings)
	{
		AccessDirect::_ref_type binary = open_binary (path);

		BinaryInfo info;
		info.platform (Binary::get_platform (binary));
		if (!Binary::is_supported_platform (info.platform ()))
			Nirvana::BindError::throw_unsupported_platform (info.platform ());

		Nirvana::ProtDomain::_ref_type domain;
		if (SINGLE_DOMAIN)
			domain = prot_domain ();
		else
			domain = manager_->create_prot_domain (info.platform ());

		if (SINGLE_DOMAIN)
			info.module_flags (ProtDomainCore::_narrow (domain)->get_module_bindings (binary, bindings));
		else {
			try {
				info.module_flags (ProtDomainCore::_narrow (domain)->get_module_bindings (binary, bindings));
			} catch (...) {
				domain->shutdown (SHUTDOWN_FLAG_FORCE);
				throw;
			}
			domain->shutdown (SHUTDOWN_FLAG_FORCE);
		}

		return info;
	}

	CORBA::Object::_ref_type get_service (const IDL::String& id)
	{
		return CORBA::Core::Services::bind (id);
	}

private:
	static SysDomain* get_implementation (const CORBA::Core::ServantProxyLocal* proxy) noexcept
	{
		return static_cast <SysDomain*> (SysObjectLink::get_implementation (proxy));
	}

	static const IDL::String get_sys_binary_path (unsigned platform, const char* module_name)
	{
		IDL::String path = Port::SysDomain::get_platform_dir (platform);
		{
			IDL::String translated;
			if (FileSystem::translate_path (path, translated))
				path = std::move (translated);
		}
		path += '/';
		path += module_name;
		return path;
	}

	enum SysModuleId
	{
		MODULE_DECCALC = 1,
		MODULE_SFLOAT,
		MODULE_DBC,
		MODULE_SQLITE,
		MODULE_PACKAGES,
		MODULE_SHELL
	};

	static AccessDirect::_ref_type open_sys_binary (unsigned platform, SysModuleId module_id);

private:
	Nirvana::SysManager::_ref_type manager_;
	PM::Packages::_ref_type packages_;

	static const char* const sys_module_names_ [];
};

}
}

#endif
