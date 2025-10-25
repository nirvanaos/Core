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
#ifndef NIRVANA_ORB_CORE_REFERENCEREMOTE_H_
#define NIRVANA_ORB_CORE_REFERENCEREMOTE_H_
#pragma once

#include "Domain.h"
#include "IDL/ORB/IOP.h"
#include "ESIOP.h"
#include "StreamInEncap.h"
#include "tagged_seq.h"

namespace CORBA {
namespace Core {

/// Base for remote references.
class ReferenceRemote :
	public Reference,
	public Nirvana::Core::BinderObject // Allocated from Binder memory
{
public:
	using Nirvana::Core::BinderObject::operator new;
	using Nirvana::Core::BinderObject::operator delete;

	ReferenceRemote (const OctetSeq& addr, servant_reference <Domain>&& domain,
		const IOP::ObjectKey& object_key, Internal::String_in primary_iid, ULong ORB_type,
		const IOP::TaggedComponentSeq& components);

	~ReferenceRemote ();

	void request_repository_id ()
	{
		assert (!has_primary_interface ());
		ProxyManager::set_primary_interface (_repository_id ());
	}

	virtual ReferenceRef marshal (StreamOut& out) override;
	
	virtual Internal::IORequest::_ref_type create_request (Internal::OperationIndex op, unsigned flags,
		CallbackRef&& callback) override;

	const Char* set_object_name (const Char* name)
	{
		object_name_ = name;
		return object_name_.c_str ();
	}

	Domain& domain () const noexcept
	{
		return *domain_;
	}

	const IOP::ObjectKey& object_key () const noexcept
	{
		return object_key_;
	}

	bool unconfirmed () const noexcept
	{
		return DGC_key_ && DGC_key_->unconfirmed ();
	}

	Domain::DGC_RefKey* DGC_key () const noexcept
	{
		return DGC_key_;
	}

	virtual Boolean _is_equivalent (Object::_ptr_type other_object) const noexcept override;
	virtual DomainManagersList _get_domain_managers () override;

	virtual void _add_ref () noexcept override;
	virtual void _remove_ref () noexcept override;

#ifndef NDEBUG
	void dbg_terminate () noexcept
	{
		dbg_terminate_ = true;
	}
#endif

private:
	const OctetSeq& address_;
	servant_reference <Domain> domain_;
	const IOP::ObjectKey object_key_;
	IDL::String object_name_;
	Domain::DGC_RefKey* DGC_key_;

#ifndef NDEBUG
	bool dbg_terminate_;
#endif
};

typedef servant_reference <ReferenceRemote> ReferenceRemoteRef;

}
}

#endif
