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
#ifndef NIRVANA_ORB_CORE_POLLABLE_H_
#define NIRVANA_ORB_CORE_POLLABLE_H_
#pragma once

#include <CORBA/Server.h>
#include <CORBA/Pollable_s.h>
#include <CORBA/Proxy/IOReference_s.h>
#include "../EventSyncTimeout.h"
#include "../MapUnorderedUnstable.h"
#include "../Synchronized.h"
#include "../SyncDomain.h"
#include "../UserObject.h"
#include "IDL/ORB/RequestCallback_s.h"
#include "GarbageCollector.h"
#include "RefCntProxy.h"

namespace CORBA {
namespace Core {

class PollableSet;

class NIRVANA_NOVTABLE Pollable :
	public Internal::ValueBaseAbstract <Pollable>,
	public Internal::ValueImpl <Pollable, CORBA::Pollable>,
	public Internal::ValueImplBase <Pollable, RequestCallback>,
	public SyncGC,
	public Nirvana::Core::UserObject
{
public:
	typedef CORBA::Pollable PrimaryInterface;

	virtual void _add_ref () noexcept override;
	virtual void _remove_ref () noexcept override;

	uint32_t _refcount_value () const noexcept
	{
		return (uint32_t)ref_cnt_;
	}

	static Pollable* cast (CORBA::Pollable::_ptr_type ptr) noexcept
	{
		return static_cast <Pollable*> (
			static_cast <Internal::Bridge <CORBA::Pollable>*> (&ptr));
	}

	virtual Internal::Interface::_ptr_type _query_valuetype (Internal::String_in id)
	{
		return Internal::FindInterface <CORBA::Pollable>::find (*this, id);
	}

	// Outline, called also in Poller
	bool is_ready (ULong timeout);

	CORBA::PollableSet::_ref_type create_pollable_set () const;

	void cur_set (PollableSet* ps) noexcept
	{
		SYNC_BEGIN (*sync_domain_, nullptr)
		if (cur_set_ && ps)
			throw BAD_PARAM (MAKE_OMG_MINOR (43));
		cur_set_ = ps;
		SYNC_END ()
	}

	Internal::Interface::_ptr_type callback () noexcept
	{
		return RequestCallback::_ptr_type (this);
	}

	void completed (Internal::IORequest::_ptr_type rq)
	{
		SYNC_BEGIN (*sync_domain_, nullptr)
			on_complete (rq);
		SYNC_END ()
	}

	virtual ~Pollable ();

	static TimeBase::TimeT ms2time (uint32_t ms) noexcept;

protected:
	Pollable ();
	Pollable (const Pollable&) = delete;
	Pollable (Pollable&&) = delete;
	Pollable& operator = (const Pollable&) = delete;
	Pollable& operator = (Pollable&&) = delete;

	virtual void on_complete (Internal::IORequest::_ptr_type reply);

	Nirvana::Core::SyncDomain& sync_domain () noexcept
	{
		return *sync_domain_;
	}

private:
	servant_reference <Nirvana::Core::SyncDomain> sync_domain_;
	Nirvana::Core::EventSyncTimeout event_;
	PollableSet* cur_set_;
	RefCntProxy ref_cnt_;
	bool ready_;
};

class DIIPollable :
	public Pollable,
	public Internal::ServantTraits <DIIPollable>,
	public Internal::ValueImpl <DIIPollable, CORBA::DIIPollable>,
	public Internal::LifeCycleRefCnt <DIIPollable>
{
public:
	using CORBA::Internal::ServantTraits <DIIPollable>::_wide_val;
	using CORBA::Internal::ServantTraits <DIIPollable>::_implementation;
	using CORBA::Internal::LifeCycleRefCnt <DIIPollable>::__duplicate;
	using CORBA::Internal::LifeCycleRefCnt <DIIPollable>::__release;
	using CORBA::Internal::LifeCycleRefCnt <DIIPollable>::_duplicate;
	using CORBA::Internal::LifeCycleRefCnt <DIIPollable>::_release;

	virtual Internal::Interface::_ptr_type _query_valuetype (Internal::String_in id) override
	{
		return Internal::FindInterface <CORBA::DIIPollable, CORBA::Pollable>::find (*this, id);
	}
};

class PollableSet : public servant_traits <CORBA::PollableSet>::Servant <PollableSet>
{
public:
	static CORBA::DIIPollable::_ref_type create_dii_pollable ()
	{
		return make_reference <DIIPollable> ();
	}

	void add_pollable (CORBA::Pollable::_ptr_type potential)
	{
		servant_reference <Pollable> p = Pollable::cast (potential);
		if (!p)
			throw BAD_PARAM ();
		p->cur_set (this);
		try {
			set_.insert (p);
		} catch (...) {
			p->cur_set (nullptr);
			throw;
		}
	}

	CORBA::Pollable::_ref_type get_ready_pollable (uint32_t timeout)
	{
		if (event_.wait (Pollable::ms2time (timeout))) {
			for (Set::iterator it = set_.begin (); it != set_.end (); ++it) {
				Pollable* p = *it;
				if (p->is_ready (0)) { // Does not cause context switch
					CORBA::Pollable::_ref_type ret (p);
					set_.erase (it);
					return ret;
				}
			}
		}
		return nullptr;
	}

	void remove (CORBA::Pollable::_ptr_type potential)
	{
		servant_reference <Pollable> p = Pollable::cast (potential);
		if (!p || !set_.erase (p))
			throw UnknownPollable ();
		if (p->is_ready (0))
			event_.reset_one ();
		p->cur_set (nullptr);
	}

	uint16_t number_left () const
	{
		return (uint16_t)std::max (set_.size (), (size_t)std::numeric_limits <uint16_t>::max ());
	}

	void pollable_ready ();

private:
	typedef Nirvana::Core::SetUnorderedUnstable <servant_reference <Pollable>,
		std::hash <void*>, std::equal_to <void*>, Nirvana::Core::UserAllocator> Set;

	Set set_;
	Nirvana::Core::EventSyncTimeout event_;
};

inline
CORBA::PollableSet::_ref_type Pollable::create_pollable_set () const
{
	CORBA::PollableSet::_ref_type ret;
	SYNC_BEGIN (*sync_domain_, nullptr)
		ret = make_reference <PollableSet> ()->_this ();
	SYNC_END ();
	return ret;
}

}
}

#endif
