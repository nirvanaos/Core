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
#include "POA_Activator.h"
#include "POA_Root.h"

using namespace CORBA;
using namespace CORBA::Internal;
using namespace CORBA::Core;

namespace PortableServer {
namespace Core {

const TimeBase::TimeT POA_Activator::ACTIVATION_TIMEOUT;

inline
Object::_ref_type POA_Activator::incarnate (Type <ObjectId>::C_in oid)
{
	// IDL-generated client code returns ServantBase.
	// We need Object instead, so use custom client call.
	Bridge <ServantActivator>* bridge = static_cast <Bridge <ServantActivator>*>
		(&ServantActivator::_ptr_type (activator_));
	EnvironmentEx <ForwardRequest> env;
	Type <Object>::C_ret ret ((bridge->_epv ().epv.incarnate) (bridge,
		&Type <ObjectId>::C_in (oid),
		&POA::_ptr_type (_this ()), &env));
	env.check ();
	return ret;
}

void POA_Activator::etherialize (Type <ObjectId>::C_in oid, Object::_ptr_type serv,
	bool cleanup_in_progress,
	bool remaining_activations) noexcept
{
	Bridge <ServantActivator>* bridge = static_cast <Bridge <ServantActivator>*>
		(&ServantActivator::_ptr_type (activator_));
	(activator_->_epv ().epv.etherealize) (bridge, &oid, &POA::_ptr_type (_this ()),
		&serv, cleanup_in_progress, remaining_activations,
		nullptr); // Ignore exceptions
}

void POA_Activator::serve_default (Request& request, const ObjectId& oid, ReferenceLocal* reference)
{
	if (!activator_)
		throw OBJECT_NOT_EXIST (MAKE_OMG_MINOR (2));
	
	Object::_ref_type servant;
	ReferenceLocalRef ref = reference;

	auto ins = activation_map_.emplace (oid, ACTIVATION_TIMEOUT);
	ActivationMap::reference entry = *ins.first;
	if (ins.second) {
		auto wait_list = entry.second.wait_list ();
		try {
			try {
				servant = incarnate (oid);
			} catch (const ForwardRequest&) {
				// ForwardRequest behaviour is not supported for incoming request.
				// TODO: Log to let an user understand the restriction.
				throw OBJECT_NOT_EXIST (MAKE_OMG_MINOR (2));
			}

			if (!servant) // User code returned nil servant
				throw OBJECT_NOT_EXIST (MAKE_OMG_MINOR (2));

			try {
				if (ref)
					ref->activate (*object2proxy (servant));
				else
					ref = activate_object (ObjectId (oid), false, *object2proxy (servant),
						Reference::GARBAGE_COLLECTION);
			} catch (const ServantAlreadyActive&) {
				etherialize (oid, servant, false, true);
				throw OBJ_ADAPTER ();
			}
			wait_list->finish_construction (servant);
			activation_map_.erase (entry.first);
		} catch (...) {
			wait_list->on_exception ();
			activation_map_.erase (entry.first);
			throw;
		}
	} else {
		servant = entry.second.get ();
		if (!ref)
			ref = root ().find_reference (request.object_key ());
	}

	POA_Base::serve_request (request, oid, ref, *object2proxy (servant));
}

ServantManager::_ref_type POA_Activator::get_servant_manager ()
{
	check_exist ();

	return activator_;
}

void POA_Activator::set_servant_manager (ServantManager::_ptr_type imgr)
{
	check_exist ();

	if (activator_)
		throw BAD_INV_ORDER (MAKE_OMG_MINOR (6));
	activator_ = ServantActivator::_narrow (imgr);
	if (!activator_)
		throw OBJ_ADAPTER (MAKE_OMG_MINOR (4));
}

void POA_Activator::deactivate_objects (bool etherealize) noexcept
{
	POA_Retain::deactivate_objects (etherealize);
	activator_ = nullptr;
}

ReferenceLocalRef POA_Activator::create_reference (ObjectId&& oid, CORBA::Internal::String_in iid)
{
	check_exist ();

	return POA_Base::create_reference (std::move (oid), iid, Reference::GARBAGE_COLLECTION);
}

void POA_Activator::etherialize (const ReferenceLocal& ref, ServantProxyObject& proxy,
	bool cleanup_in_progress) noexcept
{
	if (activator_) {
		try {
			etherialize (ObjectKey::get_object_id (ref.object_key ()), proxy.get_proxy (),
				cleanup_in_progress, proxy.is_active ());
		} catch (...) {
			// TODO: Log
		}
	}
}

}
}
