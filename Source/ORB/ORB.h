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
#ifndef NIRVANA_ORB_CORE_ORB_H_
#define NIRVANA_ORB_CORE_ORB_H_
#pragma once

#include <CORBA/Server.h>
#include <CORBA/ORB_s.h>
#include "IDL/ORB/TC_Factory.h"
#include "Services.h"
#include "PolicyFactory.h"
#include "unmarshal_object.h"
#include "RefCnt.h"
#include "../Binder.h"
#include "TypedEventChannel.h"

namespace CORBA {

/// Implementation of the CORBA::ORB interface.
class Static_the_orb : public servant_traits <CORBA::ORB>::ServantStatic <Static_the_orb>
{
public:
	static const UShort IIOP_DEFAULT_PORT = 2809;

	static IDL::Type <ORBid>::ABI_ret _s_id (Internal::Bridge <CORBA::ORB>*, Internal::Interface*) noexcept
	{
		return IDL::Type <ORBid>::ret ();
	}

	static IDL::String object_to_string (Object::_ptr_type obj)
	{
		Nirvana::Core::ImplStatic <Core::StreamOutEncap> stm;
		Core::ProxyManager::cast (obj)->marshal (stm);
		IDL::String str;
		str.reserve (4 + stm.data ().size () * 2);
		str += "ior:";
		for (unsigned oct : stm.data ()) {
			str += to_hex (oct >> 4);
			str += to_hex (oct & 0xf);
		}
		return str;
	}

	static Object::_ref_type string_to_object (Internal::String_in str, Internal::String_in iid = nullptr);

	void create_list (Long count, NVList::_ref_type& new_list)
	{
		throw NO_IMPLEMENT ();
	}

	void create_operation_list (OperationDef::_ptr_type oper, NVList::_ref_type& new_list)
	{
		throw NO_IMPLEMENT ();
	}

	void get_default_context (Context::_ref_type& ctx)
	{
		throw NO_IMPLEMENT ();
	}

	void send_multiple_requests_oneway (const RequestSeq& req)
	{
		throw NO_IMPLEMENT ();
	}

	void send_multiple_requests_deferred (const RequestSeq& req)
	{
		throw NO_IMPLEMENT ();
	}

	bool poll_next_response ()
	{
		throw NO_IMPLEMENT ();
	}

	void get_next_response (Request::_ref_type& req)
	{
		throw NO_IMPLEMENT ();
	}

	bool get_service_information (ServiceType service_type, ServiceInformation& service_information)
	{
		throw NO_IMPLEMENT ();
	}

	static ObjectIdList list_initial_services ()
	{
		return Core::Services::list_initial_services ();
	}

	static Object::_ref_type resolve_initial_references (const Internal::StringView <Char>& identifier)
	{
		return Core::Services::bind (identifier);
	}

	static TypeCode::_ref_type create_struct_tc (const RepositoryId& id,
		const Identifier& name, const StructMemberSeq& members)
	{
		return tc_factory ()->create_struct_tc (id, name, members);
	}

	static TypeCode::_ref_type create_union_tc (const RepositoryId& id,
		const Identifier& name, TypeCode::_ptr_type discriminator_type, const UnionMemberSeq& members)
	{
		return tc_factory ()->create_union_tc (id, name, discriminator_type, members);
	}

	static TypeCode::_ref_type create_enum_tc (const RepositoryId& id,
		const Identifier& name, const EnumMemberSeq& members)
	{
		return tc_factory ()->create_enum_tc (id, name, members);
	}

	static TypeCode::_ref_type create_alias_tc (const RepositoryId& id,
		const Identifier& name, TypeCode::_ptr_type original_type)
	{
		return tc_factory ()->create_alias_tc (id, name, original_type);
	}

	static TypeCode::_ref_type create_exception_tc (const RepositoryId& id,
		const Identifier& name, const StructMemberSeq& members)
	{
		return tc_factory ()->create_exception_tc (id, name, members);
	}

	static TypeCode::_ref_type create_interface_tc (const RepositoryId& id,
		const Identifier& name)
	{
		return tc_factory ()->create_interface_tc (id, name);
	}

	static TypeCode::_ref_type create_string_tc (ULong bound)
	{
		return tc_factory ()->create_string_tc (bound);
	}

	static TypeCode::_ref_type create_wstring_tc (ULong bound)
	{
		return tc_factory ()->create_wstring_tc (bound);
	}

	static TypeCode::_ref_type create_fixed_tc (UShort digits, Short scale)
	{
		return tc_factory ()->create_fixed_tc (digits, scale);
	}

	static TypeCode::_ref_type create_sequence_tc (ULong bound,
		TypeCode::_ptr_type element_type)
	{
		return tc_factory ()->create_sequence_tc (bound, element_type);
	}

	static TypeCode::_ref_type create_array_tc (ULong length,
		TypeCode::_ptr_type element_type)
	{
		return tc_factory ()->create_array_tc (length, element_type);
	}

	static TypeCode::_ref_type create_value_tc (const RepositoryId& id,
		const Identifier& name, ValueModifier type_modifier,
		TypeCode::_ptr_type concrete_base, const ValueMemberSeq& members)
	{
		return tc_factory ()->create_value_tc (id, name, type_modifier, concrete_base, members);
	}

	static TypeCode::_ref_type create_value_box_tc (const RepositoryId& id,
		const Identifier& name, TypeCode::_ptr_type boxed_type)
	{
		return tc_factory ()->create_value_box_tc (id, name, boxed_type);
	}

	static TypeCode::_ref_type create_native_tc (const RepositoryId& id,
		const Identifier& name)
	{
		return tc_factory ()->create_native_tc (id, name);
	}

	static TypeCode::_ref_type create_recursive_tc (const RepositoryId& id)
	{
		return tc_factory ()->create_recursive_tc (id);
	}

	static TypeCode::_ref_type create_abstract_interface_tc (const RepositoryId& id,
		const Identifier& name)
	{
		return tc_factory ()->create_abstract_interface_tc (id, name);
	}

	static TypeCode::_ref_type create_local_interface_tc (const RepositoryId& id,
		const Identifier& name)
	{
		return tc_factory ()->create_local_interface_tc (id, name);
	}

	static TypeCode::_ref_type create_component_tc (const RepositoryId& id,
		const Identifier& name)
	{
		return tc_factory ()->create_component_tc (id, name);
	}

	static TypeCode::_ref_type create_home_tc (const RepositoryId& id,
		const Identifier& name)
	{
		return tc_factory ()->create_home_tc (id, name);
	}

	static TypeCode::_ref_type create_event_tc (const RepositoryId& id,
		const Identifier& name, ValueModifier type_modifier,
		TypeCode::_ptr_type concrete_base, const ValueMemberSeq& members)
	{
		return tc_factory ()->create_event_tc (id, name, type_modifier, concrete_base, members);
	}

	// Thread related operations

	bool work_pending ()
	{
		return false;
	}

	void perform_work ()
	{}

	void run ()
	{}

	void shutdown (bool wait_for_completion)
	{
		throw NO_IMPLEMENT ();
	}

	void destroy ()
	{}

	static Policy::_ref_type create_policy (PolicyType type, const Any& val)
	{
		return Core::PolicyFactory::create (type, val);
	}

	// Value factory operations

	static ValueFactoryBase::_ref_type register_value_factory (const RepositoryId&, ValueFactoryBase::_ptr_type)
	{
		throw NO_IMPLEMENT ();
	}

	static void unregister_value_factory (const RepositoryId&)
	{
		throw NO_IMPLEMENT ();
	}

	static ValueFactoryBase::_ref_type lookup_value_factory (const RepositoryId& id)
	{
		try {
			return Nirvana::Core::Binder::bind_interface <ValueFactoryBase> (id);
		} catch (...) {
			throw BAD_PARAM (MAKE_OMG_MINOR (1));
		}
	}

	struct TC_Recurse
	{
		const TC_Recurse* prev;
		TypeCode::_ptr_type tc;
	};

	static TypeCode::_ref_type get_compact_typecode (TypeCode::_ptr_type tc, const TC_Recurse* parent = nullptr);

	struct TC_Pair
	{
		const TC_Pair* prev;
		TypeCode::_ptr_type left, right;
	};

	static bool tc_equal (TypeCode::_ptr_type left, TypeCode::_ptr_type right, const TC_Pair* parent = nullptr);
	static bool tc_equivalent (TypeCode::_ptr_type left, TypeCode::_ptr_type right, const TC_Pair* parent = nullptr);

	static Internal::RefCnt::_ref_type create_ref_cnt (Internal::DynamicServant::_ptr_type deleter)
	{
		return make_pseudo <Core::RefCnt> (deleter);
	}

	static CosEventChannelAdmin::EventChannel::_ref_type create_event_channel ()
	{
		CosEventChannelAdmin::EventChannel::_ref_type ret;
		SYNC_BEGIN (Nirvana::Core::g_core_free_sync_context, nullptr)
			ret = make_reference <Core::EventChannel> ()->_this ();
		SYNC_END ();
		return ret;;
	}

	static CosTypedEventChannelAdmin::TypedEventChannel::_ref_type create_typed_channel ()
	{
		CosTypedEventChannelAdmin::TypedEventChannel::_ref_type ret;
		SYNC_BEGIN (Nirvana::Core::g_core_free_sync_context, nullptr)
			ret = make_reference <Core::TypedEventChannel> ()->_this ();
		SYNC_END ();
		return ret;;
	}

	static Char to_hex (unsigned d) noexcept
	{
		return d < 10 ? '0' + d : 'a' + d - 10;
	}

private:

	static Core::TC_Factory::_ptr_type tc_factory ()
	{
		return Core::TC_Factory::_narrow (Core::Services::bind (Core::Services::TC_Factory));
	}

	static ULongLong get_union_label (TypeCode::_ptr_type tc, ULong i);

	static unsigned from_hex (int x);
	static bool schema_eq (const Char* schema, const Char* begin, const Char* end) noexcept;

	static OctetSeq unescape_object_key (const Char* begin, const Char* end);
};

}

#endif
