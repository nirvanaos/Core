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
#ifndef NIRVANA_ORB_CORE_REMOTEREFERENCES_H_
#define NIRVANA_ORB_CORE_REMOTEREFERENCES_H_
#pragma once

#include "ProtDomains.h"
#include "RemoteDomains.h"
#include "ReferenceRemote.h"
#include "HashOctetSeq.h"
#include "DomainAddress.h"
#include "PolicyMap.h"
#include <CORBA/I_var.h>
#include "../Chrono.h"

namespace CORBA {
namespace Core {

class RemoteReferences
{
	typedef ReferenceRemote* RefPtr;
	typedef Nirvana::Core::WaitableRef <RefPtr> RefVal;
	typedef OctetSeq RefKey;
	typedef Nirvana::Core::MapUnorderedStable <RefKey, RefVal, std::hash <RefKey>,
		std::equal_to <RefKey>, Nirvana::Core::BinderMemory::Allocator> References;

public:
	RemoteReferences ()
	{}

	~RemoteReferences ()
	{
		for (auto& r : references_) {
			RefPtr p = r.second.get_if_constructed ();
#ifndef NDEBUG
			if (p)
				p->dbg_terminate ();
#endif
			delete p;
		}
	}

	template <class EndPoint>
	Object::_ref_type unmarshal (const EndPoint& domain, Internal::String_in iid,
		const IOP::TaggedProfileSeq& addr, const IOP::ObjectKey& object_key, ULong ORB_type,
		const IOP::TaggedComponentSeq& components, ReferenceRemoteRef& unconfirmed)
	{
		Nirvana::Core::ImplStatic <StreamOutEncap> stm (true);
		stm.write_tagged (addr);
		auto ins = emplace_reference (stm.data ());
		if (ins.second) {
			auto wait_list = ins.first->second.wait_list ();
			try {
				RefPtr p (make_reference (ins.first->first, get_domain (domain),
					object_key, iid, ORB_type, components));
				if (iid.empty ())
					p->request_repository_id ();
				if (p->unconfirmed ())
					unconfirmed = p;
				Internal::I_var <Object> ret (p->get_proxy ()); // Use I_var to avoid reference counter increment
				wait_list->finish_construction (p);
				return ret;
			} catch (...) {
				wait_list->on_exception ();
				references_.erase (ins.first->first);
				throw;
			}
		} else {
			auto p = ins.first->second.get ();
			p->check_primary_interface (iid);
			return p->get_proxy ();
		}
	}

	ProtDomains& prot_domains () noexcept
	{
		return prot_domains_;
	}

	RemoteDomains& remote_domains () noexcept
	{
		return remote_domains_;
	}

	void erase (const RefKey& ref) noexcept
	{
		references_.erase (ref);
	}

	servant_reference <Domain> get_domain (const DomainAddress& domain);

	servant_reference <DomainProt> get_domain (ESIOP::ProtDomainId domain)
	{
		return prot_domains_.get (domain);
	}

	servant_reference <DomainRemote> get_domain (const IIOP::ListenPoint& lp)
	{
		return remote_domains_.get (lp);
	}

	servant_reference <PolicyMapShared> unmarshal_policies (const OctetSeq& policies)
	{
		servant_reference <PolicyMapShared> ref;
		auto ins = policies_.emplace (policies, nullptr);
		if (ins.second) {
			ref = CORBA::make_reference <Policies> (std::ref (ins.first->first));
			ins.first->second = ref;
		} else
			ref = ins.first->second;
		return ref;
	}

	void heartbeat (const DomainAddress& domain) noexcept
	{
		auto p = find_domain (domain);
		if (p)
			p->simple_ping ();
	}

	void complex_ping (const DomainAddress& domain, Internal::IORequest::_ptr_type rq)
	{
		get_domain (domain)->complex_ping (rq);
	}

	void close_connection (ESIOP::ProtDomainId domain_id) noexcept
	{
		servant_reference <DomainProt> domain = prot_domains_.find (domain_id);
		if (domain)
			domain->make_zombie ();
	}

	bool housekeeping () noexcept
	{
		Nirvana::SteadyTime t = Nirvana::Core::Chrono::steady_clock ();
		bool p = prot_domains_.housekeeping (t);
		bool r = remote_domains_.housekeeping (t);
		return p || r;
	}

	void shutdown () noexcept // Called on terminate()
	{
		prot_domains_.shutdown ();
		remote_domains_.shutdown ();
	}

private:
	std::pair <typename References::iterator, bool> emplace_reference (const OctetSeq& addr);
	static ReferenceRemote* make_reference (const OctetSeq& addr,
		servant_reference <Domain>&& domain, const IOP::ObjectKey& object_key,
		Internal::String_in primary_iid, ULong ORB_type, const IOP::TaggedComponentSeq& components);

	servant_reference <Domain> find_domain (const DomainAddress& domain) const noexcept
	{
		if (domain.family == DomainAddress::Family::ESIOP)
			return prot_domains_.find (domain.address.esiop);
		else
			return nullptr; // TODO: Implement
	}

private:
	ProtDomains prot_domains_;
	RemoteDomains remote_domains_;
	References references_;

	/// Many remote references may have the same policy set.
	/// To conserve memory, we store policies in dictionary.
	
	class Policies :
		public PolicyMapShared,
		public Nirvana::Core::BinderObject
	{
	public:
		using Nirvana::Core::BinderObject::operator new;
		using Nirvana::Core::BinderObject::operator delete;

		Policies (const OctetSeq& policies) :
			PolicyMapShared (std::ref (policies)),
			key_ (policies)
		{}

		virtual ~Policies () override;

	private:
		const OctetSeq& key_;
	};
	
	typedef Nirvana::Core::MapUnorderedStable <OctetSeq, PolicyMapShared*,
		std::hash <OctetSeq>, std::equal_to <OctetSeq>,
		Nirvana::Core::BinderMemory::Allocator> PoliciesDictionary;

	PoliciesDictionary policies_;
};

}
}

#endif
