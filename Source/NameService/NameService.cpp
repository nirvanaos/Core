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
#include "NameService.h"
#include "../ORB/ESIOP.h"
#include "../ORB/ORB.h"
#include "../ORB/ServantProxyObject.h"
#include "FileSystem.h"

using namespace CORBA;

namespace CosNaming {
namespace Core {

Object::_ref_type create_NameService ()
{
	if (ESIOP::is_system_domain ())
		return make_reference <NameService> ()->_this ();
	else
		return CORBA::Static_the_orb::string_to_object (
			"corbaloc::1.1@/NameService", CORBA::Internal::RepIdOf <CosNaming::NamingContextExt>::id);
}

bool NameService::is_file_system (const NameComponent& nc)
{
	return nc.id ().empty () && nc.kind ().empty ();
}

inline void NameService::check_no_fs (const Name& n)
{
	if (is_file_system (n.front ()))
		throw NamingContext::AlreadyBound ();
}

void NameService::bind1 (Name& n, Object::_ptr_type obj)
{
	check_no_fs (n);
	Base::bind1 (n, obj);
}

void NameService::rebind1 (Name& n, Object::_ptr_type obj)
{
	check_no_fs (n);
	return Base::rebind1 (n, obj);
}

void NameService::bind_context1 (Name& n, NamingContext::_ptr_type nc)
{
	check_no_fs (n);
	Base::bind_context1 (n, nc);
}

void NameService::rebind_context1 (Name& n, NamingContext::_ptr_type nc)
{
	check_no_fs (n);
	Base::rebind_context1 (n, nc);
}

Object::_ref_type NameService::resolve1 (Name& n)
{
	assert (!n.empty ());
	Object::_ref_type ret;

	if (is_file_system (n.front ())) {
		ret = file_system_.get_if_constructed ();
		if (!ret) {
			file_system_.initialize (FS_CREATION_DEADLINE);
			auto wait_list = file_system_.wait_list ();
			try {
				ret = Nirvana::Core::FileSystem::create ();
			} catch (...) {
				wait_list->on_exception ();
				throw;
			}
			wait_list->finish_construction (ret);
		}
	} else
		ret = Base::resolve1 (n);

	return ret;
}

NamingContext::_ref_type NameService::bind_new_context1 (Name& n)
{
	check_no_fs (n);
	return Base::bind_new_context1 (n);
}

void NameService::get_bindings (IteratorStack& iter) const
{
	iter.reserve (bindings_.size ());
	iter.push (NameComponent (), BindingType::ncontext);
	Base::get_bindings (iter);
}

Name NameService::to_name (const StringName& sn)
{
	Name n;
	size_t begin = 0;
	for (size_t end = 0; (end = sn.find ('/', end)) != StringName::npos;) {
		if (end > 0 && sn [end - 1] == '\\') { // Escaped, search next
			++end;
			continue;
		}

		n.push_back (to_component (sn.substr (begin, end - begin)));
		begin = ++end;
	}
	n.push_back (to_component (sn.substr (begin)));
	return n;
}

NameService::StringName NameService::to_string_unchecked (const Name& n)
{
	StringName sn;
	size_t size = n.size () - 1;
	for (const NameComponent& nc : n) {
		size += nc.id ().size ();
		if (!nc.kind ().empty ())
			size += nc.kind ().size () + 1;
	}
	sn.reserve (size);
	Name::const_iterator it = n.begin ();
	append_string (sn, *it++);
	while (it != n.end ()) {
		sn += '/';
		append_string (sn, *it++);
	}
	return sn;
}

}
}
