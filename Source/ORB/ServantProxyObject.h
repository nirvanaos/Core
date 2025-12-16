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
#ifndef NIRVANA_ORB_CORE_SERVANTPROXYOBJECT_H_
#define NIRVANA_ORB_CORE_SERVANTPROXYOBJECT_H_
#pragma once

#include "ServantProxyLocal.h"
#include "../TaggedPtr.h"
#include "../MapUnorderedUnstable.h"
#include "../HeapAllocator.h"
#include "../LockablePtr.h"
#include "Reference.h"
#include "ObjectKey.h"
#include "Services.h"

namespace CORBA {
namespace Core {

class ReferenceLocal;
typedef servant_reference <ReferenceLocal> ReferenceLocalRef;

/// \brief Server-side Object proxy.
class NIRVANA_NOVTABLE ServantProxyObject :
	public ServantProxyBase
{
	typedef ServantProxyBase Base;

public:
	typedef PortableServer::ServantBase ServantInterface;

	virtual void _add_ref () override;

	///@{
	/// Called from the POA synchronization domain.
	/// So calls to activate (), deactivate () and is_active () are always serialized.
	void activate (ReferenceLocal& reference)
	{
		ReferenceLocal* old = reference_.exchange (&reference);
		// If the set is empty, object can have one reference stored in reference_.
		// If the set is not empty, it must contain all the references include stored in reference_.
		if (old) {
			references_.insert (old);
			references_.insert (&reference);
		}
	}

	void deactivate (ReferenceLocal& reference) noexcept
	{
		references_.erase (&reference);
		ReferenceLocal* p = references_.empty () ? nullptr : *references_.begin ();
		reference_.cas (&reference, p);
	}

	bool is_active () const noexcept
	{
		return static_cast <ReferenceLocal*> (reference_.load ()) != nullptr;
	}
	///@}

	/// Returns user ServantBase implementation
	PortableServer::Servant servant () const
#ifdef NDEBUG
		noexcept
#endif
	{
#ifndef NDEBUG
		return PortableServer::ServantBase::_check (&Base::servant ());
#else
		return static_cast <PortableServer::ServantBase*> (&Base::servant ());
#endif
	}

	virtual PortableServer::ServantBase::_ref_type _get_servant () const override
	{
		if (&sync_context () != &Nirvana::Core::SyncContext::current ())
			throw OBJ_ADAPTER ();
		return servant ();
	}

	virtual Boolean _is_equivalent (Object::_ptr_type other_object) const noexcept override;

	virtual Boolean non_existent () override;
	virtual ReferenceLocalRef get_local_reference (const PortableServer::Core::POA_Base&) override;
	virtual ReferenceRef marshal (StreamOut& out) override;

	virtual Policy::_ref_type _get_policy (PolicyType policy_type) override;
	virtual DomainManagersList _get_domain_managers () override;

	void delete_servant (bool from_destructor);

protected:
	ServantProxyObject (PortableServer::Servant servant, Object::_ptr_type comp) :
		ServantProxyBase (servant, comp),
		adapter_context_ (&get_adapter_context ()),
		reference_ (nullptr),
		references_ (adapter_context_->sync_domain ()->mem_context ().heap ())
	{}

	// This constructor is used in custom scenarios (e. g. event channels)
	ServantProxyObject (PortableServer::Servant servant, Internal::String_in interface_id);

	ReferenceLocalRef get_local_reference () const noexcept;

private:
	static Nirvana::Core::SyncContext& get_adapter_context ()
	{
		Object::_ref_type root = Services::bind (Services::RootPOA);
		// If shutdown was initiated, root_poa is nil
		if (!root)
			throw OBJ_ADAPTER ();
		return local2proxy (root)->sync_context ();
	}

protected:
	servant_reference <Nirvana::Core::SyncContext> adapter_context_;

	static const size_t REF_SIZE = sizeof (Reference) + 3 * sizeof (void*) + sizeof (Nirvana::Core::LockablePtrImpl);
	static const size_t REF_ALIGN = Nirvana::Core::core_object_align (REF_SIZE);
	typedef Nirvana::Core::LockablePtrT <ReferenceLocal, 0, REF_ALIGN> RefPtr;

	mutable RefPtr reference_;

	// The set of references.
	// If the set is empty, object can have one reference stored in reference_.
	// If the set is not empty, it contains all the references include stored in reference_.
	// This set is always changed in the POA sync context, so we use the POA sync domain heap.
	typedef Nirvana::Core::SetUnorderedUnstable <ReferenceLocal*, std::hash <void*>,
		std::equal_to <void*>, Nirvana::Core::HeapAllocator> References;
	References references_;
};

CORBA::Object::_ptr_type servant2object (PortableServer::Servant servant) noexcept;

/// Get proxy for object.
/// 
/// \param obj Object pointer.
///   Ensure that it is really object, not a reference or local object.
/// \returns Proxy pointer.
inline ServantProxyObject* object2proxy (CORBA::Object::_ptr_type obj) noexcept
{
	return const_cast <ServantProxyObject*> (
		static_cast <const ServantProxyObject*> (object2proxy_base (obj)));
}

/// \brief Obtain servant from object proxy.
/// 
/// Must be called from the servant synchronization context, otherwise exception will thrown.
/// 
/// \param obj Object proxy.
/// \returns PortableServer::Servant
/// \throws OBJ_ADAPTER
PortableServer::Servant object2servant (Object::_ptr_type obj);

PortableServer::Servant proxy2servant (ServantProxyObject* proxy);

inline
Internal::Bridge <PortableServer::ServantBase>* proxy2servant_base (ServantProxyObject* proxy)
{
	return static_cast <Internal::Bridge <PortableServer::ServantBase>*> (&proxy2servant (proxy));
}

}
}

#endif
