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
#include "ReferenceLocal.h"
#include "RequestLocalPOA.h"
#include "POA_Root.h"
#include "LocalAddress.h"
#include "StreamOutEncap.h"
#include "IDL/ORB/IIOP.h"
#include "ESIOP.h"
#include <CORBA/Servant_var.h>
#include "system_services.h"

using namespace Nirvana::Core;
using namespace PortableServer::Core;

namespace CORBA {

using namespace Internal;

namespace Core {

ReferenceLocal::ReferenceLocal (const IOP::ObjectKey& object_key, POA_Base& adapter,
	Internal::String_in primary_iid, unsigned flags, PolicyMapShared* policies) :
	Reference (primary_iid, flags | LOCAL),
	object_key_ (object_key),
	adapter_ (&adapter),
	adapter_context_ (&local2proxy (POA_Base::get_root ())->sync_context ()),
	servant_ (nullptr)
{
	policies_ = policies;
}

ReferenceLocal::ReferenceLocal (const IOP::ObjectKey& object_key, POA_Base& adapter,
	ServantProxyObject& proxy, unsigned flags, PolicyMapShared* policies) :
	Reference (proxy, flags | LOCAL),
	object_key_ (object_key),
	adapter_ (&adapter),
	adapter_context_ (&local2proxy (POA_Base::get_root ())->sync_context ()),
	servant_ (nullptr)
{
	policies_ = policies;
}

ReferenceLocal::~ReferenceLocal ()
{
	assert (&SyncContext::current () == adapter_context_);
	assert (0 == ref_cnt_);
}

void ReferenceLocal::_add_ref () noexcept
{
	if (flags_ & GARBAGE_COLLECTION) {
		if (1 == ref_cnt_.increment_seq ()) {
			ServantProxyObject* proxy = servant_.lock ();
			if (proxy)
				proxy->_add_ref ();
			servant_.unlock ();
		}
	} else
		ref_cnt_.increment ();
}

void ReferenceLocal::_remove_ref () noexcept
{
	if (0 == ref_cnt_.decrement ()) {
		if (flags_ & GARBAGE_COLLECTION) {
			ServantProxyObject* proxy = servant_.load ();
			if (proxy) {
				proxy->_remove_ref ();
				return;
			}
		} else if (servant_.load ()) // Servant is still active
			return;

		// Need GC
		if (&SyncContext::current () == adapter_context_) {
			// Do GC
			POA_Root* root = POA_Base::root_ptr ();
			if (root)
				root->remove_reference (object_key_);
			delete this;
		} else {
			// Schedule GC
			GarbageCollector::schedule (*this, *adapter_context_);
		}
	}
}

void ReferenceLocal::on_servant_destruct () noexcept
{
	// Called on the active weak reference servant destruction.
	assert (&SyncContext::current () == adapter_context_);
	assert (0 == ref_cnt_);
	ServantProxyObject* proxy = servant_.exchange (nullptr);
	assert (proxy);
	POA_Root* root = POA_Base::root_ptr ();
	if (root) {
		root->remove_reference (object_key_);
		adapter_->implicit_deactivate (*this, *proxy);
	}
	delete this;
}

void ReferenceLocal::activate (ServantProxyObject& proxy)
{
	// Caller must hold both references.
	assert (_refcount_value ());
	assert (proxy._refcount_value ());

	if (servant_.load ())
		throw PortableServer::POA::ObjectAlreadyActive ();

	check_primary_interface (proxy.primary_interface_id ());
	proxy.activate (*this);
	proxy._add_ref ();
	servant_ = &proxy;
}

servant_reference <ServantProxyObject> ReferenceLocal::deactivate () noexcept
{
	// Caller must hold reference.
	assert (_refcount_value ());

	// ServantProxyObject is always add-refed there so we use Servant_var.
	PortableServer::Servant_var <ServantProxyObject> proxy (servant_.exchange (nullptr));

	if (proxy)
		proxy->deactivate (*this);
	return proxy;
}

servant_reference <ServantProxyObject> ReferenceLocal::get_active_servant () const noexcept
{
	// This method is always called from the POA sync context, so we need not lock the pointer.
	assert (&SyncContext::current () == adapter_context_);
	return servant_reference <ServantProxyObject> (servant_.load ());
}

servant_reference <ServantProxyObject> ReferenceLocal::get_active_servant_with_lock () const noexcept
{
	servant_reference <ServantProxyObject> proxy (servant_.lock ());
	servant_.unlock ();
	return proxy;
}

PortableServer::ServantBase::_ref_type ReferenceLocal::_get_servant () const
{
	servant_reference <ServantProxyObject> proxy = get_active_servant_with_lock ();
	if (!proxy) {
		// Attempt to pass an unactivated (unregistered) value as an object reference.
		throw OBJECT_NOT_EXIST (MAKE_OMG_MINOR (1));
	}
	return proxy->_get_servant ();
}

inline
void ReferenceLocal::marshal_object_key (const CORBA::Octet* obj_key, size_t obj_key_size,
	CORBA::Core::StreamOut& stream)
{
	if (Nirvana::Core::SINGLE_DOMAIN
		|| (ESIOP::is_system_domain () && system_service (obj_key, obj_key_size) < Services::SERVICE_COUNT)
		) {
		// Do not prefix system domain objects
		stream.write_size (obj_key_size);
	} else {
		// Prefix object key with domain id
		ESIOP::ProtDomainId id = ESIOP::current_domain_id ();
		// If platform endians are different, always use big endian format.
		if (ESIOP::PLATFORMS_ENDIAN_DIFFERENT && Nirvana::endian::native == Nirvana::endian::little)
			Nirvana::byteswap (id);
		stream.write_size (sizeof (ESIOP::ProtDomainId) + obj_key_size);
		stream.write_one (id);
	}
	stream.write_c (1, obj_key_size, obj_key);
}

void ReferenceLocal::marshal (const ProxyManager& proxy, const Octet* obj_key, size_t obj_key_size,
	const PolicyMap* policies, StreamOut& out)
{
	out.write_string_c (proxy.primary_interface_id ());
	ImplStatic <StreamOutEncap> encap;
	{
		IIOP::Version ver (1, 1);
		encap.write_one (ver);
	}
	encap.write_string_c (LocalAddress::singleton ().host ());
	UShort port = LocalAddress::singleton ().port ();
	encap.write_one (port);
	
	marshal_object_key (obj_key, obj_key_size, encap);

	uint32_t ORB_type = ESIOP::ORB_TYPE;

	size_t component_cnt = 2;

	if (policies) {
		if (!policies->empty ())
			++component_cnt;
		else
			policies = nullptr;
	}

	IOP::TaggedComponentSeq components;
	components.reserve (component_cnt);

	{ // IOP::TAG_ORB_TYPE
		ImplStatic <StreamOutEncap> encap;
		encap.write_c (4, 4, &ORB_type);
		components.emplace_back (IOP::TAG_ORB_TYPE, std::move (encap.data ()));
	}

	{ // IOP::TAG_FT_HEARTBEAT_ENABLED
		ImplStatic <StreamOutEncap> encap;
		uint8_t enabled = 1;
		encap.write_c (1, 1, &enabled);
		components.emplace_back (IOP::TAG_FT_HEARTBEAT_ENABLED, std::move (encap.data ()));
	}
	
	if (policies) {
		ImplStatic <StreamOutEncap> encap;
		encap.write_size (policies->size ());
		policies->write (encap);
		components.emplace_back (IOP::TAG_POLICIES, std::move (encap.data ()));
	}

	encap.write_tagged (components);

	IOP::TaggedProfileSeq addr {
		IOP::TaggedProfile (IOP::TAG_INTERNET_IOP, std::move (encap.data ()))
	};
	out.write_tagged (addr);
}

ReferenceRef ReferenceLocal::marshal (StreamOut& out)
{
	marshal (*this, object_key_.data (), object_key_.size (), policies_, out);
	return this;
}

ReferenceLocalRef ReferenceLocal::get_local_reference (const PortableServer::Core::POA_Base& adapter)
{
	// Called from the POA sync context
	assert (&SyncContext::current () == adapter_context_);
	if (adapter_ == &adapter)
		return this;
	else
		return nullptr;
}

IORequest::_ref_type ReferenceLocal::create_request (OperationIndex op, unsigned flags,
	CallbackRef&& callback)
{
	if (is_local_object_op (op))
		return ProxyManager::create_request (op, flags, std::move (callback));

	// If servant is active, create direct request for performance.
	// We can't use get_active_servant here because the arbitrary sync context.
	// So we lock the proxy pointer here.
	servant_reference <ServantProxyObject> proxy = get_active_servant_with_lock ();
	if (proxy)
		return proxy->create_request (op, flags, std::move (callback));

	// Create POA request.
	check_create_request (op, flags);

	if (callback) {
		if (!(flags & Internal::IORequest::RESPONSE_EXPECTED))
			throw BAD_PARAM ();
		return make_pseudo <RequestLocalImpl <RequestLocalAsyncPOA> > (std::ref (*this), op, flags, std::move (callback));
	} else if (flags & Internal::IORequest::RESPONSE_EXPECTED) {
		return make_pseudo <RequestLocalImpl <RequestLocalPOA> > (std::ref (*this), op, flags);
	} else {
		return make_pseudo <RequestLocalImpl <RequestLocalOnewayPOA> > (std::ref (*this), op, flags);
	}
}

DomainManagersList ReferenceLocal::_get_domain_managers ()
{
	return DomainManagersList (); // TODO: Implement
}

Boolean ReferenceLocal::_is_equivalent (Object::_ptr_type other_object) const noexcept
{
	if (!other_object)
		return false;

	if (ProxyManager::_is_equivalent (other_object))
		return true;

	servant_reference <ServantProxyObject> servant = get_active_servant_with_lock ();

	if (servant)
		return servant->_is_equivalent (other_object);
	else
		return false;
}

IDL::String ReferenceLocal::_repository_id ()
{
	if (has_primary_interface ())
		return ProxyManager::_repository_id ();

	servant_reference <ServantProxyObject> active_servant = get_active_servant_with_lock ();

	if (active_servant)
		return active_servant->_repository_id ();

	return Reference::_repository_id ();
}

Object::_ref_type ReferenceLocal::_get_component ()
{
	servant_reference <ServantProxyObject> active_servant = get_active_servant_with_lock ();

	if (active_servant)
		return active_servant->_get_component ();

	return Reference::_get_component ();
}

}
}
