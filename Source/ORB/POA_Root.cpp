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
#include "POA_Root.h"
#include "RqHelper.h"

using namespace Nirvana;
using namespace Nirvana::Core;
using namespace CORBA;
using namespace CORBA::Core;

namespace PortableServer {
namespace Core {

bool POA_Root::shutdown_;

Object::_ref_type POA_Root::create ()
{
	auto manager_factory = CORBA::make_reference <POAManagerFactory> ();
	auto manager = manager_factory->create ("RootPOAManager");
	manager->activate ();

	return CORBA::make_reference <POA_Root> (std::move (manager),
		std::move (manager_factory))->_this ();
}

POA_Root::~POA_Root ()
{
	assert (is_destroyed ());
	root_ = nullptr;
}

std::pair <POA_Root::References::iterator, bool> POA_Root::emplace_reference (const POA_Base& adapter, ObjectId&& oid)
{
	return local_references_.emplace (IOP::ObjectKey (ObjectKey (adapter, std::move (oid))), Reference::DEADLINE_MAX);
}

template <typename Param>
CORBA::Core::ReferenceLocalRef POA_Root::get_or_create (std::pair <References::iterator, bool>& ins,
	POA_Base& adapter, bool unique, Param& param, unsigned flags, CORBA::Core::PolicyMapShared* policies)
{
	References::reference entry = *ins.first;
	if (ins.second) {
		auto wait_list = entry.second.wait_list ();
		RefPtr p;
		try {
			p = new CORBA::Core::ReferenceLocal (entry.first, adapter, param, flags, policies);
		} catch (...) {
			wait_list->on_exception ();
			local_references_.erase (entry.first);
			throw;
		}
		PortableServer::Servant_var <CORBA::Core::ReferenceLocal> ret (p); // Adopt reference count ownership
		wait_list->finish_construction (p);
		return ret;
	} else if (unique)
		return nullptr;
	else
		return entry.second.get ();
}

ReferenceLocalRef POA_Root::emplace_reference (POA_Base& adapter, ObjectId&& oid, bool unique,
	CORBA::Internal::String_in primary_iid, unsigned flags, CORBA::Core::PolicyMapShared* policies)
{
	auto ins = emplace_reference (adapter, std::move (oid));
	ReferenceLocalRef p = get_or_create (ins, adapter, unique, primary_iid, flags, policies);
	if (p)
		p->check_primary_interface (primary_iid);
	return p;
}

ReferenceLocalRef POA_Root::emplace_reference (POA_Base& adapter, ObjectId&& oid, bool unique,
	ServantProxyObject& proxy, unsigned flags, CORBA::Core::PolicyMapShared* policies)
{
	auto ins = emplace_reference (adapter, std::move (oid));
	ReferenceLocalRef p = get_or_create (ins, adapter, unique, proxy, flags, policies);
	if (p)
		p->check_primary_interface (proxy.primary_interface_id ());
	return p;
}

ReferenceLocalRef POA_Root::find_reference (const IOP::ObjectKey& key) noexcept
{
	References::iterator it = local_references_.find (key);
	ReferenceLocalRef ref;
	if (it != local_references_.end ())
		ref = it->second.get ();
	return ref;
}

POA_Ref POA_Root::find_child (const AdapterPath& path, bool activate_it)
{
	POA_Ref adapter = &root ();
	for (const auto& name : path) {
		if (activate_it && adapter->is_destroyed ())
			throw TRANSIENT (MAKE_OMG_MINOR (4));

		adapter = adapter->find_child (name, activate_it);
		if (!adapter)
			break;
	}
	return adapter;
}

void POA_Root::invoke (RequestRef request, bool async) noexcept
{
	try {
		if (Scheduler::shutdown_started ())
			throw CORBA::OBJ_ADAPTER (MAKE_OMG_MINOR (1));

		Object::_ref_type root = get_root (); // Hold root POA reference
		SYNC_BEGIN (local2proxy (root)->sync_context (), nullptr);

		invoke_sync (*request);

		if (async) // Do not reschedule exec domain, it will be released immediately.
			_sync_frame.return_to_caller_context ();

		SYNC_END ();

	} catch (Exception& e) {
		request->set_exception (std::move (e));
	} catch (...) {
		request->set_unknown_exception ();
	}
}

void POA_Root::invoke_sync (Request& request)
{
	POA_Base* adapter;
	POA::_ref_type adapter_holder;
	{
		POA_Ref adapter_ref;
		try {
			adapter_ref = find_child (ObjectKey::get_adapter_path (request.object_key ()), true);
		} catch (...) {
			throw OBJ_ADAPTER (MAKE_OMG_MINOR (1));
		}
		if (!adapter_ref)
			throw OBJECT_NOT_EXIST (MAKE_OMG_MINOR (2));
		adapter = adapter_ref;
		adapter_holder = adapter_ref->_this ();

		// Sync context may be changed during the adapter->invoke() so we release adapter_ref here
	}
	adapter->invoke (request);
}

class POA_Root::InvokeAsync :
	public Runnable
{
public:
	InvokeAsync (POA_Base* root, RequestRef&& request) :
		root_ (root),
		request_ (std::move (request))
	{}

private:
	virtual void run () noexcept;
	virtual void on_crash (const siginfo& signal) noexcept;

private:
	POA_Ref root_;
	RequestRef request_;
};

void POA_Root::invoke_async (RequestRef request, DeadlineTime deadline)
{
	if (Scheduler::shutdown_started ())
		throw CORBA::OBJ_ADAPTER (MAKE_OMG_MINOR (1));

	const ServantProxyLocal* adapter_proxy = CORBA::Core::local2proxy (get_root ());
	POA_Base* adapter = get_implementation (adapter_proxy);
	assert (adapter);

	ExecDomain::async_call <InvokeAsync> (deadline, adapter_proxy->sync_context (), request->heap (),
		adapter, std::move (request));
}

void POA_Root::InvokeAsync::run () noexcept
{
	try {
		invoke_sync (*request_);
	} catch (Exception& e) {
		request_->set_exception (std::move (e));
	} catch (...) {
		request_->set_unknown_exception ();
	}
}

void POA_Root::InvokeAsync::on_crash (const siginfo& signal) noexcept
{
	Any a = RqHelper::signal2exception (signal);
	request_->set_exception (a);
}

}
}
