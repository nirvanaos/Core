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
#ifndef NIRVANA_ORB_CORE_DOMAIN_H_
#define NIRVANA_ORB_CORE_DOMAIN_H_
#pragma once

#include <CORBA/CORBA.h>
#include "IDL/ORB/IOP.h"
#include <CORBA/Proxy/InterfaceMetadata.h>
#include <Nirvana/SimpleList.h>
#include "../AtomicCounter.h"
#include "../MapUnorderedUnstable.h"
#include "../MapUnorderedStable.h"
#include "../BinderMemory.h"
#include "../Chrono.h"
#include "../BinderObject.h"
#include "HashOctetSeq.h"
#include "ReferenceLocal.h"
#include "RequestEvent.h"
#include "SystemExceptionHolder.h"
#include <array>

namespace CORBA {
namespace Core {

class ReferenceRemote;
typedef servant_reference <ReferenceRemote> ReferenceRemoteRef;

/// Other domain
class NIRVANA_NOVTABLE Domain : public Nirvana::Core::BinderObject
{
	template <class D> friend class CORBA::servant_reference;

	// For DGC add requests, deadline policy is request_latency () / DGC_DEADLINE_ADD_DIV.
	static const unsigned DGC_DEADLINE_ADD_DIV = 20;

	// For DGC del requests, deadline policy is request_latency () * DGC_DEADLINE_DEL_MUL.
	static const unsigned DGC_DEADLINE_DEL_MUL = 10;

public:
	// If an incoming request returns DGC references, it must be holded until the reference
	// confirmation. Destruction delayed on request_latency () * DGC_IN_REQUEST_DELAY_RELEASE_MUL;
	// Must be > 2 to let the incoming request passed to caller and then pass the DGC ping request
	// to the object owners.
	static const unsigned DGC_IN_REQUEST_DELAY_RELEASE_MUL = 8;

	virtual Internal::IORequest::_ref_type create_request (unsigned response_flags,
		const IOP::ObjectKey& object_key, const Internal::Operation& metadata, ReferenceRemote* ref,
		CallbackRef&& callback) = 0;

	/// Domain flag bits
	enum
	{
		/// Domain supports distributed garbage collection
		///
		/// If this flag is set, flags HEARTBEAT_IN and HEARTBEAT_OUT are set also
		GARBAGE_COLLECTION = 0x0001,

		/// Domain requires FT heartbeat ping
		HEARTBEAT_IN = 0x0002,

		/// Domain sends FT heartbeat ping
		HEARTBEAT_OUT = 0x0002
	};

	unsigned flags () const noexcept
	{
		return flags_;
	}

	unsigned GIOP_minor () const noexcept
	{
		return GIOP_minor_;
	}

	// Estimated maximal time of the request delivery.
	// If it is too small, the DGC will fail with OBJECT_NOT_EXIST.
	const TimeBase::TimeT request_latency () const noexcept
	{
		return request_latency_;
	}

	void request_latency (TimeBase::TimeT latency) noexcept;

	Nirvana::DeadlineTime request_add_deadline () const noexcept
	{
		return request_add_deadline_;
	}

	Nirvana::DeadlineTime request_del_deadline () const noexcept
	{
		return request_del_deadline_;
	}

	void simple_ping () noexcept
	{
		last_ping_in_time_ = Nirvana::Core::Chrono::steady_clock ();
	}

	void complex_ping (Internal::IORequest::_ptr_type rq)
	{
		last_ping_in_time_ = Nirvana::Core::Chrono::steady_clock ();
		IDL::Sequence <IOP::ObjectKey> add, del;
		IDL::Type <IDL::Sequence <IOP::ObjectKey> >::unmarshal (rq, add);
		IDL::Type <IDL::Sequence <IOP::ObjectKey> >::unmarshal (rq, del);
		rq->unmarshal_end ();
		complex_ping (add, del);
		rq->success ();
	}

	bool is_garbage (const TimeBase::TimeT& cur_time) noexcept
	{
		// TODO: Check for connection is alive and make zomby if not.
		return ref_cnt_.load () == 0;
	}

	class DGC_RefKey;

private:
	// DGC request
	class DGC_Request;
	typedef Nirvana::Core::ImplDynamicSync <DGC_Request> DGC_RequestImpl;
	typedef servant_reference <DGC_RequestImpl> DGC_RequestRef;

	class DGC_Request
	{
	public:
		static DGC_RequestRef create (Domain& domain);

		void add (DGC_RefKey& key)
		{
			keys_.push_back (&key);
			++add_cnt_;
		}

		void del (DGC_RefKey& key)
		{
			keys_.push_back (&key);
		}

		void invoke ();

		void complete (bool no_throw = false);

	protected:
		DGC_Request (Domain& domain);

	private:
		Domain& domain_;
		Internal::IORequest::_ref_type request_;
		servant_reference <RequestEvent> event_;
		std::vector <DGC_RefKey*, Nirvana::Core::BinderMemory::Allocator <DGC_RefKey*> > keys_;
		size_t add_cnt_;

		static const Internal::Operation operation_;
	};

public:
	// DGC-enabled remote reference owned by the current domain
	class DGC_RefKey :
		public IOP::ObjectKey,
		public Nirvana::SimpleList <DGC_RefKey>::Element
	{
	public:
		DGC_RefKey (const IOP::ObjectKey& object_key) :
			IOP::ObjectKey (object_key),
			reference_cnt_ (1),
			added_ (false)
		{}

		// Add remote reference with this key
		void reference_add () noexcept
		{
			++reference_cnt_;
		}

		// Remove remote reference with this key
		unsigned reference_remove () noexcept
		{
			return --reference_cnt_;
		}

		unsigned reference_cnt () const noexcept
		{
			return reference_cnt_;
		}

		bool unconfirmed () const noexcept
		{
			return !added_ || request_;
		}

		bool added () const noexcept
		{
			return added_;
		}

		void complete_deletion () noexcept
		{
			if (request_ && added_)
				request_->complete (true);
		}

		void complete_addition () noexcept
		{
			if (request_ && !added_)
				request_->complete (true);
		}

		void request_completed () noexcept
		{
			assert (request_);
			request_ = nullptr;
			added_ = !added_;
		}

		void request_failed (const SystemException& ex) noexcept
		{
			assert (request_);
			request_ = nullptr;
			if (!added_)
				exception_.set_exception (ex);
		}

		DGC_Request* request () const noexcept
		{
			return request_;
		}

		void request (DGC_Request& rq) noexcept
		{
			exception_.reset ();
			request_ = &static_cast <DGC_RequestImpl&> (rq);
		}

		void check ()
		{
			exception_.check ();
		}

	private:
		unsigned reference_cnt_;
		bool added_;
		DGC_RequestRef request_;
		SystemExceptionHolder exception_;
	};

	DGC_RefKey* on_DGC_reference_unmarshal (const IOP::ObjectKey& object_key)
	{
		if (zombie_)
			return nullptr;
		bool first = remote_objects_.empty ();
		auto ins = remote_objects_.emplace (object_key);
		if (first)
			_add_ref ();
		DGC_RefKey& key = const_cast <DGC_RefKey&> (*ins.first);
		if (!ins.second)
			key.reference_add ();
		return &key;
	}

	void on_DGC_reference_delete (DGC_RefKey& key) noexcept
	{
		if (zombie_)
			return;

		if (0 == key.reference_remove ()) {
			if (key.added () || key.request ()) {
				remote_objects_del_.push_back (key);
				schedule_del ();
			} else
				erase_remote_key (key);
		}
	}

	static void confirm_DGC_references (size_t cnt, CORBA::Core::ReferenceRemoteRef* refs);

	void make_zombie () noexcept;

	bool zombie () const noexcept
	{
		return zombie_;
	}

	void _add_ref () noexcept
	{
		ref_cnt_.increment ();
	}

	void _remove_ref () noexcept
	{
		ref_cnt_.decrement ();
	}

protected:
	Domain (unsigned flags, unsigned GIOP_minor, TimeBase::TimeT request_latency, TimeBase::TimeT heartbeat_interval,
		TimeBase::TimeT heartbeat_timeout);
	~Domain ();

private:
	typedef void (Domain::* DGC_Function) (const ReferenceRemoteRef*, const ReferenceRemoteRef*, bool);
	static void call_DGC_function (DGC_Function func, const ReferenceRemoteRef* begin,
		const ReferenceRemoteRef* end, bool no_throw);
	void confirm_DGC_ref_start (const ReferenceRemoteRef* begin,
		const ReferenceRemoteRef* end, bool dummy);
	void confirm_DGC_ref_finish (const ReferenceRemoteRef* begin,
		const ReferenceRemoteRef* end, bool no_throw);

	void complex_ping (IDL::Sequence <IOP::ObjectKey>& add, IDL::Sequence <IOP::ObjectKey>& del)
	{
		if (!Nirvana::Core::SINGLE_DOMAIN) {
			erase_domain_id (add);
			erase_domain_id (del);
		}
		static const size_t STATIC_ADD_CNT = 4;
		if (add.size () <= STATIC_ADD_CNT) {
			std::array <ReferenceLocalRef, STATIC_ADD_CNT> refs;
			add_owned_objects (add, refs.data ());
		} else {
			std::vector <ReferenceLocalRef> refs (add.size ());
			add_owned_objects (add, refs.data ());
		}

		for (const IOP::ObjectKey& obj_key : del) {
			if (local_objects_.erase (obj_key) && local_objects_.empty ())
				_remove_ref ();
		}
	}

	void add_owned_objects (IDL::Sequence <IOP::ObjectKey>& keys, ReferenceLocalRef* objs);

	void send_heartbeat ()
	{
		Internal::IORequest::_ref_type rq = create_request (0, IOP::ObjectKey (), op_heartbeat_,
			nullptr, nullptr);
		rq->invoke ();
		last_ping_out_time_ = Nirvana::Core::Chrono::steady_clock ();
	}

	void schedule_del () noexcept;
	void send_del () noexcept;
	void append_del (DGC_Request& rq);
	void erase_remote_key (DGC_RefKey& key) noexcept;
	static void erase_domain_id (IDL::Sequence <IOP::ObjectKey>& keys);

private:
	class GC : public Nirvana::Core::Runnable
	{
	public:
		GC (Domain& domain) :
			domain_ (&domain)
		{}

	private:
		virtual void run () noexcept override;

	private:
		servant_reference <Domain> domain_;
	};

	class RefCnt : public Nirvana::Core::AtomicCounter <false>
	{
	public:
		RefCnt () :
			Nirvana::Core::AtomicCounter <false> (1)
		{}

		RefCnt (const RefCnt&) = delete;
		RefCnt& operator = (const RefCnt&) = delete;
	};

	RefCnt ref_cnt_;

	unsigned flags_;
	const unsigned GIOP_minor_;

	// DGC-enabled local objects owned by this domain
	typedef Nirvana::Core::MapUnorderedUnstable <IOP::ObjectKey, ReferenceLocalRef,
		std::hash <IOP::ObjectKey>, std::equal_to <IOP::ObjectKey>, 
		Nirvana::Core::BinderMemory::Allocator> LocalObjects;

	LocalObjects local_objects_;

	// DGC-enabled references to the domain objects owned by the current domain
	typedef Nirvana::Core::SetUnorderedStable <DGC_RefKey,
		std::hash <IOP::ObjectKey>, std::equal_to <IOP::ObjectKey>,
		Nirvana::Core::BinderMemory::Allocator> RemoteObjects;

	// DGC-enabled references to the domain objects owned by the current domain
	RemoteObjects remote_objects_;
	Nirvana::SimpleList <DGC_RefKey> remote_objects_del_;

	TimeBase::TimeT last_ping_in_time_;
	TimeBase::TimeT last_ping_out_time_;
	TimeBase::TimeT request_latency_;
	Nirvana::DeadlineTime request_add_deadline_;
	Nirvana::DeadlineTime request_del_deadline_;
	TimeBase::TimeT heartbeat_interval_;
	TimeBase::TimeT heartbeat_timeout_;

	bool DGC_scheduled_;
	bool zombie_;

	static const Internal::Operation op_heartbeat_;
};

}
}

#endif
