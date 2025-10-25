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
#ifndef NIRVANA_ORB_CORE_POA_ROOT_H_
#define NIRVANA_ORB_CORE_POA_ROOT_H_
#pragma once

#include "POA_Implicit.h"
#include "POAManagerFactory.h"
#include "HashOctetSeq.h"
#include "../MapUnorderedStable.h"
#include "IDL/CoreDomains.h"
#include <CORBA/Servant_var.h>
#include <random>

namespace IOP {
typedef CORBA::OctetSeq ObjectKey;
}

namespace PortableServer {
namespace Core {

class POA_Root :
	public virtual POA_Base,
	public POA_ImplicitUnique
{
	typedef std::conditional_t <(sizeof (size_t) > 4), std::mt19937_64, std::mt19937> RandomGen;

public:
	POA_Root (CORBA::servant_reference <POAManager>&& manager,
		CORBA::servant_reference <POAManagerFactory>&& manager_factory) :
		POA_Base (nullptr, nullptr, std::move (manager), CORBA::servant_reference <CORBA::Core::PolicyMapShared> ()),
		manager_factory_ (std::move (manager_factory)),
		random_gen_ (RandomGen::result_type (Nirvana::Core::Chrono::UTC ().time () / TimeBase::SECOND))
	{
		root_ = this;
	}

	~POA_Root ();

	virtual IDL::String the_name () const override
	{
		return "RootPOA";
	}

	static void invoke (RequestRef request, bool async) noexcept;
	static void invoke_async (RequestRef request, Nirvana::DeadlineTime deadline);

	static POA_Ref find_child (const AdapterPath& path, bool activate_it);

	static CORBA::Object::_ref_type unmarshal (CORBA::Internal::String_in iid, const IOP::ObjectKey& object_key)
	{
		CORBA::Object::_ref_type proxy = get_root (); // Hold root POA proxy reference
		CORBA::Object::_ref_type ret;

		SYNC_BEGIN (CORBA::Core::local2proxy (proxy)->sync_context (), nullptr)

		CORBA::Core::ReferenceLocalRef ref = root ().find_reference (object_key);
		if (ref)
			ref->check_primary_interface (iid);
		else {
			ObjectKey core_key (object_key);
			POA_Ref adapter = find_child (core_key.adapter_path (), true);
			if (!adapter || adapter->is_destroyed ())
				throw CORBA::OBJECT_NOT_EXIST (MAKE_OMG_MINOR (2));
			ref = adapter->create_reference (std::move (core_key.object_id ()), iid);
		}

		ret = CORBA::Object::_ref_type (ref->get_proxy ());

		SYNC_END ();

		return ret;
	}

	static void get_DGC_objects (const IDL::Sequence <IOP::ObjectKey>& keys, CORBA::Core::ReferenceLocalRef* refs)
	{
		if (shutdown_)
			return;

		CORBA::Object::_ref_type proxy = get_root (); // Hold root POA proxy reference
		SYNC_BEGIN (CORBA::Core::local2proxy (proxy)->sync_context (), nullptr)
		for (const auto& iop_key : keys) {
			CORBA::Core::ReferenceLocalRef ref = root ().find_reference (iop_key);
			if (ref)
				if (ref->flags () & CORBA::Core::Reference::GARBAGE_COLLECTION)
					*refs = std::move (ref);
				else
					throw CORBA::INV_OBJREF ();
			else
				throw CORBA::OBJECT_NOT_EXIST ();
			++refs;
		}
		SYNC_END ();
	}

	POAManagerFactory& manager_factory () noexcept
	{
		return *manager_factory_;
	}

	ObjectId generate_persistent_id ()
	{
		struct
		{
			TimeBase::TimeT time;
			RandomGen::result_type random;
		} id = { Nirvana::Core::Chrono::UTC ().time (), dist_ (random_gen_)};

		const CORBA::Octet* p = (const CORBA::Octet*)&id;
		const CORBA::Octet* end = p + 8 + sizeof (RandomGen::result_type);
		return ObjectId (p, end);
	}

	static void check_persistent_id (const ObjectId& oid)
	{
		if (oid.size () != 8 + sizeof (RandomGen::result_type))
			throw CORBA::BAD_PARAM (MAKE_OMG_MINOR (14));
	}

	typedef CORBA::Core::ReferenceLocal* RefPtr;
	typedef Nirvana::Core::WaitableRef <RefPtr> RefVal;
	typedef Nirvana::Core::MapUnorderedStable <IOP::ObjectKey, RefVal, std::hash <IOP::ObjectKey>,
		std::equal_to <IOP::ObjectKey>, Nirvana::Core::UserAllocator> References;

	std::pair <References::iterator, bool> emplace_reference (const POA_Base& adapter, ObjectId&& oid);

	template <typename Param>
	CORBA::Core::ReferenceLocalRef get_or_create (std::pair <References::iterator, bool>& ins,
		POA_Base& adapter, bool unique, Param& param, unsigned flags,
		CORBA::Core::PolicyMapShared* policies);

	CORBA::Core::ReferenceLocalRef emplace_reference (POA_Base& adapter, ObjectId&& oid, bool unique,
		CORBA::Internal::String_in primary_iid, unsigned flags, CORBA::Core::PolicyMapShared* policies);

	CORBA::Core::ReferenceLocalRef emplace_reference (POA_Base& adapter, ObjectId&& oid, bool unique,
		CORBA::Core::ServantProxyObject& proxy, unsigned flags, CORBA::Core::PolicyMapShared* policies);

	void remove_reference (const IOP::ObjectKey& key) noexcept
	{
		local_references_.erase (key);
	}

	CORBA::Core::ReferenceLocalRef find_reference (const IOP::ObjectKey& key) noexcept;

	CORBA::Core::PolicyMapShared* default_DGC_policies () const noexcept
	{
		return DGC_policies_;
	}

	static void deactivate_all ()
	{
		// Deactivate all managers to reject incoming requests.
		CORBA::Object::_ref_type root_adapter = CORBA::Core::Services::get_if_constructed (CORBA::Core::Services::RootPOA);
		if (root_adapter) {
			const CORBA::Core::ServantProxyLocal* proxy = CORBA::Core::local2proxy (root_adapter);
			SYNC_BEGIN (proxy->sync_context (), nullptr);
			root ().manager_factory ().deactivate_all ();
			SYNC_END ();
		}
	}

	static void shutdown ()
	{
		shutdown_ = true;

		// Block incoming requests and complete currently executed ones.
		CORBA::Object::_ref_type root_adapter = CORBA::Core::Services::get_if_constructed (CORBA::Core::Services::RootPOA);
		if (root_adapter)
			PortableServer::POA::_narrow (root_adapter)->destroy (true, true);
	}

	static bool shutdown_started () noexcept
	{
		return shutdown_;
	}

	static CORBA::Object::_ref_type create ();

private:
	class InvokeAsync;

	static void invoke_sync (Request& request);

private:
	References local_references_; // All local references
	CORBA::servant_reference <POAManagerFactory> manager_factory_;
	RandomGen random_gen_;
	std::uniform_int_distribution <RandomGen::result_type> dist_;

	static bool shutdown_;
};

inline
PortableServer::POAManagerFactory::_ref_type POA_Base::the_POAManagerFactory () noexcept
{
	return root ().manager_factory ()._this ();
}

inline
void POA_Base::implicit_activate (POA::_ptr_type adapter, CORBA::Core::ServantProxyObject& proxy)
{
	const CORBA::Core::ServantProxyLocal* adapter_proxy = CORBA::Core::local2proxy (adapter);
	POA_Base* adapter_impl = get_implementation (adapter_proxy);
	if (!adapter_impl)
		throw CORBA::OBJ_ADAPTER ();
	SYNC_BEGIN (adapter_proxy->sync_context (), nullptr);
	adapter_impl->activate_object (std::ref (proxy), CORBA::Core::Reference::GARBAGE_COLLECTION);
	SYNC_END ();
}

inline
POA::_ref_type POA_Base::create_POA (const IDL::String& adapter_name,
	PortableServer::POAManager::_ptr_type a_POAManager, const CORBA::PolicyList& policies)
{
	check_exist ();

#ifndef NDEBUG
	for (size_t i = 1; i < FACTORY_COUNT; ++i) {
		assert (factories_ [i - 1].policies < factories_ [i].policies);
	}
#endif

	auto ins = children_.emplace (adapter_name, POA_Ref ());
	if (!ins.second && ins.first->second.is_constructed ())
		throw AdapterAlreadyExists ();

	try {

		CORBA::Core::PolicyMapRef object_policies;
		POA_Policies pols = POA_Policies::default_;
		pols.set_values (policies, object_policies);

		CORBA::servant_reference <POAManager> manager = POAManager::get_implementation (
			CORBA::Core::local2proxy (a_POAManager));

		if (!manager)
			manager = root ().manager_factory ().create_auto (adapter_name);
		else if (manager->policies ()) {
			if (!object_policies)
				object_policies = manager->policies ();
			else
				object_policies->merge (*manager->policies ());
		}

		POA_Ref ref;
		const POA_FactoryEntry* pf = std::lower_bound (factories_, factories_ + FACTORY_COUNT, pols);
		if (pf != factories_ + FACTORY_COUNT && !(pols < *pf))
			ref = (pf->factory) (this, &ins.first->first, std::move (manager), std::move (object_policies));
		else
			throw InvalidPolicy (); // Do not return the index, it's too complex to calculate it.

		ins.first->second = std::move (ref);

	} catch (...) {
		children_.erase (ins.first);
		throw;
	}

	return ins.first->second.get ()->_this ();
}

}
}

#endif
