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
#include "POA_Base.h"
#include "POA_Root.h"
#include "PortableServer_Context.h"
#include "PortableServer_policies.h"
#include <CORBA/Servant_var.h>

using namespace CORBA;
using namespace CORBA::Internal;
using namespace CORBA::Core;
using namespace Nirvana::Core;

namespace PortableServer {
namespace Core {

const TimeBase::TimeT POA_Base::ADAPTER_ACTIVATION_TIMEOUT;

POA_Root* POA_Base::root_;

Interface* POA_Base::_s_get_servant (Bridge <POA>* _b, Interface* _env) noexcept
{
	try {
		return Type <Object>::ret (_implementation (_b).get_servant ());
	} catch (Exception& e) {
		set_exception (_env, e);
	} catch (...) {
		set_unknown_exception (_env);
	}
	return Type <Object>::ret ();
}

void POA_Base::_s_set_servant (Bridge <POA>* _b,
	Interface* p_servant,
	Interface* _env) noexcept
{
	try {
		_implementation (_b).set_servant (Type <Object>::in (p_servant));
	} catch (Exception& e) {
		set_exception (_env, e);
	} catch (...) {
		set_unknown_exception (_env);
	}
}

Type <ObjectId>::ABI_ret POA_Base::_s_activate_object (Bridge <POA>* _b,
	Interface* p_servant,
	Interface* env) noexcept
{
	try {
		return Type <ObjectId>::ret (_implementation (_b).
			activate_object (Type <Object>::in (p_servant)));
	} catch (Exception& e) {
		set_exception (env, e);
	} catch (...) {
		set_unknown_exception (env);
	}
	return Type <ObjectId>::ret ();
}

void POA_Base::_s_activate_object_with_id (Bridge <POA>* _b,
	Type <ObjectId>::ABI_in oid, Interface* p_servant,
	Interface* env) noexcept
{
	try {
		_implementation (_b).activate_object_with_id (Type <ObjectId>::in (oid), Type <Object>::in (p_servant));
	} catch (Exception& e) {
		set_exception (env, e);
	} catch (...) {
		set_unknown_exception (env);
	}
}

Type <ObjectId>::ABI_ret POA_Base::_s_servant_to_id (Bridge <POA>* _b,
	Interface* p_servant,
	Interface* env) noexcept
{
	try {
		return Type <ObjectId>::ret (_implementation (_b).
			servant_to_id (Type <Object>::in (p_servant)));
	} catch (Exception& e) {
		set_exception (env, e);
	} catch (...) {
		set_unknown_exception (env);
	}
	return Type <ObjectId>::ret ();
}

Interface* POA_Base::_s_servant_to_reference (Bridge <POA>* _b,
	Interface* p_servant,
	Interface* env) noexcept
{
	try {
		return Type <Object>::ret (_implementation (_b).
			servant_to_reference (Type <Object>::in (p_servant)));
	} catch (Exception& e) {
		set_exception (env, e);
	} catch (...) {
		set_unknown_exception (env);
	}
	return Type <Object>::ret ();
}

Interface* POA_Base::_s_reference_to_servant (Bridge <POA>* _b,
	Interface* reference,
	Interface* env) noexcept
{
	try {
		return Type <Object>::ret (_implementation (_b).
			reference_to_servant (Type <Object>::in (reference)));
	} catch (Exception& e) {
		set_exception (env, e);
	} catch (...) {
		set_unknown_exception (env);
	}
	return Type <Object>::ret ();
}

Interface* POA_Base::_s_id_to_servant (Bridge <POA>* _b,
	Type <ObjectId>::ABI_in oid,
	Interface* env) noexcept
{
	try {
		return Type <Object>::ret (_implementation (_b).
			id_to_reference (Type <ObjectId>::in (oid))); // Really call to id_to_reference()
	} catch (Exception& e) {
		set_exception (env, e);
	} catch (...) {
		set_unknown_exception (env);
	}
	return Type <Object>::ret ();
}

POA_Base::POA_Base (POA_Base* parent, const IDL::String* name,
	servant_reference <Core::POAManager>&& manager,
	PolicyMapRef&& object_policies) :
	the_POAManager_ (std::move (manager)),
	object_policies_ (std::move (object_policies)),
	base_flags_ (),
	parent_ (parent),
	name_ (name),
#ifndef NDEBUG
	dbg_name_ (name ? *name : "RootPOA"),
#endif
	request_cnt_ (0),
	destroyed_ (false),
	destroy_called_ (false),
	signature_ (SIGNATURE)
{
	if (object_policies_) {
		LifespanPolicyValue lifespan;
		if (object_policies_->get_value <LIFESPAN_POLICY_ID> (lifespan)
			&& LifespanPolicyValue::PERSISTENT == lifespan)
			base_flags_ |= Reference::PERSISTENT;
	}

	the_POAManager_->on_adapter_create (*this);
	the_POAManager_->_this (); // Create proxy forward
	NIRVANA_TRACE ("POA_Base (%s)", dbg_name_.c_str ());
}

POA_Base::~POA_Base ()
{
	the_POAManager_->on_adapter_destroy (*this);
	NIRVANA_TRACE ("~POA_Base (%s)", dbg_name_.c_str ());
}

void POA_Base::check_exist () const
{
	if (destroyed_)
		throw OBJECT_NOT_EXIST (MAKE_OMG_MINOR (4));
}

void POA_Base::activate_object (CORBA::Core::ServantProxyObject& proxy, ObjectId& oid, unsigned flags)
{
	for (;;) {
		oid = generate_object_id ();
		ReferenceLocalRef ref = activate_object (std::move (oid), true, proxy, flags);
		if (ref)
			return;
	}
}

ReferenceLocalRef POA_Base::activate_object (CORBA::Core::ServantProxyObject& proxy, unsigned flags)
{
	for (;;) {
		ReferenceLocalRef ref = activate_object (generate_object_id (), true, proxy, flags);
		if (ref)
			return ref;
	}
}

ReferenceLocalRef POA_Base::activate_object (ObjectId&&, bool unique, ServantProxyObject&, unsigned flags)
{
	throw WrongPolicy ();
}

void POA_Base::activate_object (ReferenceLocal& ref, ServantProxyObject& proxy)
{
	NIRVANA_UNREACHABLE_CODE ();
}

bool POA_Base::implicit_activation () const noexcept
{
	return false;
}

void POA_Base::deactivate_object (ObjectId& oid)
{
	throw WrongPolicy ();
}

servant_reference <CORBA::Core::ServantProxyObject> POA_Base::deactivate_reference (
	ReferenceLocal& ref, bool etherealize, bool cleanup_in_progress)
{
	NIRVANA_UNREACHABLE_CODE ();
	return nullptr;
}

void POA_Base::implicit_deactivate (CORBA::Core::ReferenceLocal& ref,
	CORBA::Core::ServantProxyObject& proxy) noexcept
{
	NIRVANA_UNREACHABLE_CODE ();
}

Object::_ref_type POA_Base::create_reference (CORBA::Internal::String_in iid)
{
	check_exist ();

	return create_reference (iid, 0)->get_proxy ();
}

ReferenceLocalRef POA_Base::create_reference (CORBA::Internal::String_in iid, unsigned flags)
{
	for (;;) {
		// The ObjectKey constructor calls generate_object_id to generate a new unique id.
		// If SYSTEM_ID policy is not present, the WrongPolicy exception will be thrown.
		ReferenceLocalRef ref = root ().emplace_reference (*this, generate_object_id (), true, iid,
			get_flags (flags), get_policies (flags));
		if (ref)
			return ref;
		// Unique ID collision.
		// Hardly ever it can cause, but we support it just for case.
		assert (false);
		// Jus go to generate a new id.
	}
}

ReferenceLocalRef POA_Base::create_reference (ObjectId&& oid, CORBA::Internal::String_in iid)
{
	return create_reference (std::move (oid), iid, 0);
}

ReferenceLocalRef POA_Base::create_reference (ObjectId&& oid, CORBA::Internal::String_in iid,
	unsigned flags)
{
	return root ().emplace_reference (*this, std::move (oid), false, std::ref (iid), get_flags (flags),
		get_policies (flags));
}

ObjectId POA_Base::servant_to_id (ServantProxyObject& proxy)
{
	return servant_to_id_default (proxy, false);
}

ObjectId POA_Base::servant_to_id_default (ServantProxyObject& proxy, bool not_found)
{
	if (not_found)
		throw ServantNotActive ();
	else
		throw WrongPolicy ();
}

Object::_ref_type POA_Base::servant_to_reference (ServantProxyObject& proxy)
{
	return servant_to_reference_default (proxy, false);
}

Object::_ref_type POA_Base::servant_to_reference_default (ServantProxyObject& proxy, bool not_found)
{
	if (not_found) {
		const Context* ctx = (const Context*)Nirvana::Core::ExecDomain::current ().TLS_get (
			CoreTLS::CORE_TLS_PORTABLE_SERVER);
		if (ctx && (&ctx->servant == &proxy.get_proxy ()))
			return ctx->servant;
		throw ServantNotActive ();
	} else
		throw WrongPolicy ();
}

Object::_ref_type POA_Base::reference_to_servant (Object::_ptr_type reference)
{
	check_exist ();

	return reference_to_servant_default (false);
}

Object::_ref_type POA_Base::reference_to_servant_default (bool not_active)
{
	if (not_active)
		throw ObjectNotActive ();
	else
		throw WrongPolicy ();
}

ObjectId POA_Base::reference_to_id (Object::_ptr_type reference)
{
	if (!reference)
		throw BAD_PARAM ();

	ReferenceLocalRef ref = ProxyManager::cast (reference)->get_local_reference (*this);
	if (!ref)
		throw WrongAdapter ();
	return ObjectKey::get_object_id (ref->object_key ());
}

Object::_ref_type POA_Base::id_to_servant (ObjectId& oid)
{
	check_exist ();

	return id_to_servant_default (false);
}

Object::_ref_type POA_Base::id_to_servant_default (bool not_active)
{
	if (not_active)
		throw ObjectNotActive ();
	else
		throw WrongPolicy ();
}

Object::_ref_type POA_Base::id_to_reference (ObjectId& oid)
{
	throw WrongPolicy ();
}

POA_Base* POA_Base::get_implementation (const CORBA::Core::ServantProxyLocal* proxy)
{
	if (proxy) {
		POA_Base* impl = static_cast <POA_Base*> (
			static_cast <Bridge <CORBA::LocalObject>*> (&proxy->servant ()));
		if (impl->signature_ == SIGNATURE)
			return impl;
		else
			throw CORBA::OBJ_ADAPTER (); // User try to create own POA implementation?
	}
	return nullptr;
}

void POA_Base::get_path (AdapterPath& path, size_t size) const
{
	if (parent_) {
		parent_->get_path (path, size + 1);
		path.push_back (*name_);
	} else {
		path.clear ();
		path.reserve (size);
	}
}

bool POA_Base::check_path (const AdapterPath& path, AdapterPath::const_iterator it)
	const noexcept
{
	if (parent_) {
		assert (name_);
		if (it == path.begin ())
			return false;
		--it;
		if (name_->size () != it->size ())
			return false;
		if (*name_ != *it)
			return false;
		return parent_->check_path (path, it);
	} else
		return it == path.begin ();
}

POA_Ref POA_Base::find_child (const IDL::String& adapter_name, bool activate_it)
{
	Children::iterator it;
	if (!activate_it || !the_activator_)
		it = children_.find (adapter_name);
	else {
		auto ins = children_.emplace (adapter_name, ADAPTER_ACTIVATION_TIMEOUT);
		if (ins.second) {
			Children::reference entry (*ins.first);
			auto wait_list = entry.second.wait_list ();
			try {
				bool created = false;
				try {
					created = the_activator_->unknown_adapter (_this (), adapter_name);
				} catch (const AdapterAlreadyExists&) {
					// Possible raise where the first wins, ignore it.
				}

				if (created)
					it = children_.find (adapter_name);
			} catch (...) {
				wait_list->on_exception ();
				children_.erase (entry.first);
				throw;
			}
			if (it == children_.end ())
				children_.erase (entry.first);
		} else
			it = ins.first;
	}
	if (it != children_.end ())
		return it->second.get ();
	else
		return nullptr;
}

ObjectId POA_Base::generate_object_id ()
{
	throw WrongPolicy ();
}

void POA_Base::check_object_id (const ObjectId& oid)
{}

void POA_Base::serve_request (Request& request)
{
	ReferenceLocalRef ref = root ().find_reference (request.object_key ());
	
	// Reference may be created by the destroyed adapter
	if (ref && &ref->adapter () != this) {
		assert (ref->adapter ().destroy_called ());
		ref->adapter_ = this;
	}

	ObjectId oid = ObjectKey::get_object_id (request.object_key ());
	serve_request (request, oid, ref);
}

void POA_Base::serve_request (Request& request, const ObjectId& oid, ReferenceLocal* reference)
{
	serve_default (request, oid, reference);
}

void POA_Base::serve_default (Request& request, const ObjectId& oid, ReferenceLocal* reference)
{
	throw OBJECT_NOT_EXIST (MAKE_OMG_MINOR (2));
}

void POA_Base::serve_request (Request& request, const ObjectId& oid, ReferenceLocal* reference, ServantProxyObject& proxy)
{
	ExecDomain& ed = ExecDomain::current ();
	Context* ctx_prev = (Context*)ed.TLS_get (CoreTLS::CORE_TLS_PORTABLE_SERVER);
	Context ctx (_this (), oid, reference ? reference->get_proxy () : proxy.get_proxy (), proxy);
	ed.TLS_set (CoreTLS::CORE_TLS_PORTABLE_SERVER, &ctx);
	try {
		request.serve (proxy);
	} catch (...) {
		ed.TLS_set (CoreTLS::CORE_TLS_PORTABLE_SERVER, ctx_prev);
		throw;
	}
	ed.TLS_set (CoreTLS::CORE_TLS_PORTABLE_SERVER, ctx_prev);
}

void POA_Base::on_request_start () noexcept
{
	++request_cnt_;
	the_POAManager_->on_request_start ();
}

void POA_Base::on_request_finish () noexcept
{
	the_POAManager_->on_request_finish ();
	if (!--request_cnt_ && destroyed_)
		destroy_completed_.signal ();
}

void POA_Base::check_wait_completion ()
{
	// If wait_for_completion is TRUE and the current thread is in an invocation context dispatched
	// from some POA belonging to the same ORB as this POA, the BAD_INV_ORDER system exception with
	// standard minor code 3 is raised and POA destruction does not occur.
	if (ExecDomain::current ().TLS_get (CoreTLS::CORE_TLS_PORTABLE_SERVER))
		throw CORBA::BAD_INV_ORDER (MAKE_OMG_MINOR (3));
}

bool POA_Policies::operator < (const POA_Policies& rhs) const noexcept
{
	typedef ABI_enum Int;
	return std::lexicographical_compare (
		(const Int*)this, (const Int*)(this + 1),
		(const Int*)&rhs, (const Int*)(&rhs + 1));
}

bool POA_Policies::add_object_policy (PolicyMapRef& object_policies, Policy::_ptr_type policy)
{
	if (!object_policies)
		object_policies = make_reference <PolicyMapShared> ();
	return object_policies->insert (policy);
}

}
}
