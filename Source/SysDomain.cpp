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
#include "SysDomain.h"
#include "ORB/ORB.h"
#include "SysManager.h"
#include "NameService/FileSystem.h"

using namespace CORBA;

namespace Nirvana {
namespace Core {

const char* const SysDomain::sys_module_names_ [MODULE_SHELL] = {
	"DecCalc.olf",
	"SFloat.olf",
	"dbc.olf",
	"SQLite.olf",
	"PacMan.olf",
	"shell.olf"
};

inline
SysDomain::SysDomain (CORBA::Object::_ref_type& obj)
{
	CORBA::Object::_ref_type component = _this ();

	// Create SysManager
	SYNC_BEGIN (g_core_free_sync_context, &MemContext::current ());
	manager_ = make_reference <SysManager> (component)->_this ();
	SYNC_END ();

	// Create Packages object
	Binder::BindResult br = Binder::load_and_bind (MODULE_PACKAGES,
		open_sys_binary (PLATFORM, MODULE_PACKAGES),
		"Nirvana/PM/pac_factory", CORBA::Internal::RepIdOf <PM::PacFactory>::id);
	PM::PacFactory::_ref_type pf = std::move (br.itf).template downcast <PM::PacFactory> ();
	SYNC_BEGIN (*br.sync_context, &MemContext::current ());
	packages_ = pf->create (component);
	SYNC_END ();

	obj = std::move (component);
}

SysDomain::~SysDomain ()
{}

Object::_ref_type create_SysDomain ()
{
	if (ESIOP::is_system_domain ()) {
		CORBA::Object::_ref_type obj;
		make_stateless <SysDomain> (std::ref (obj));
		return obj;
	} else
		return CORBA::Static_the_orb::string_to_object (
			"corbaloc::1.1@/%00", CORBA::Internal::RepIdOf <SysDomain::PrimaryInterface>::id);
}

AccessDirect::_ref_type SysDomain::open_sys_binary (unsigned platform, SysModuleId module_id)
{
	return open_binary (get_sys_binary_path (platform, sys_module_names_ [module_id - 1]));
}

bool SysDomain::get_sys_binding (CORBA::Internal::String_in name, unsigned platform, Binding& binding)
{
	static const char dbc [] = "IDL:NDBC/";
	static const char sfloat_4 [] = "CORBA/Internal/sfloat_4";
	static const char sfloat_8 [] = "CORBA/Internal/sfloat_8";
	static const char sfloat_16 [] = "CORBA/Internal/sfloat_16";

	struct SysObject
	{
		const char* name;
		SysModuleId module_id;
	};

	static const SysObject sys_objects [] = {
		{ "NDBC/the_manager", MODULE_DBC},
		{ "Nirvana/dec_calc", MODULE_DECCALC },
		{ "Nirvana/pacman", MODULE_SHELL },
		{ "SQLite/driver", MODULE_SQLITE }
	};

	SysModuleId module_id;

	if (name.size () >= sizeof (dbc) && std::equal (dbc, dbc + sizeof (dbc) - 1, name.data ()))
		module_id = MODULE_DBC;
	else if (
		(!std::is_same <float, CORBA::Float>::value && name == sfloat_4)
		||
		(!std::is_same <double, CORBA::Double>::value && name == sfloat_8)
		||
		(!std::is_same <long double, CORBA::LongDouble>::value && name == sfloat_16)
		) {
		module_id = MODULE_SFLOAT;
	} else {
		const SysObject* p = sys_objects;
		for (; p != std::end (sys_objects); ++p) {
			if (name == p->name)
				break;
		}
		if (p != std::end (sys_objects))
			module_id = p->module_id;
		else
			return false;
	}

	ModuleLoad& module_load = binding.module_load ();
	module_load.binary (open_sys_binary (platform, module_id));
	module_load.module_id (module_id);
	return true;
}

}
}
