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
#ifndef NIRVANA_ORB_CORE_TC_UNION_H_
#define NIRVANA_ORB_CORE_TC_UNION_H_
#pragma once

#include "TC_IdName.h"
#include "TC_Impl.h"
#include "TC_Ref.h"
#include "ORB.h"
#include "../Array.h"
#include "../UserAllocator.h"

namespace CORBA {
namespace Core {

class TC_Union :
	public TC_Impl <TC_Union, TC_Complex <TC_IdName> >
{
	typedef TC_Impl <TC_Union, TC_Complex <TC_IdName> > Impl;

public:
	using Servant::_s_id;
	using Servant::_s_name;
	using Servant::_s_discriminator_type;
	using Servant::_s_default_index;
	using Servant::_s_member_count;
	using Servant::_s_member_name;
	using Servant::_s_member_type;
	using Servant::_s_member_label;
	using Servant::_s_get_compact_typecode;

	struct Member
	{
		Any label;
		IDL::String name;
		TC_Ref type;
		size_t offset;
	};

	typedef Nirvana::Core::Array <Member, Nirvana::Core::UserAllocator> Members;

	TC_Union (IDL::String&& id, IDL::String&& name, TypeCode::_ptr_type discriminator_type, Long default_index);

	TC_Union (IDL::String&& id, IDL::String&& name, TypeCode::_ptr_type discriminator_type, Long default_index,
		Members&& members) :
		TC_Union (std::move (id), std::move (name), discriminator_type, default_index)
	{
		set_members (std::move (members));

		TC_Ref self (_get_ptr (), this);
		for (auto& m : members_) {
			m.type.replace_recursive_placeholder (id_, self);
		}
	}

	void set_members (Members&& members);

	bool equal (TypeCode::_ptr_type other)
	{
		return Static_the_orb::tc_equal (Impl::_get_ptr (), other);
	}

	bool equivalent (TypeCode::_ptr_type other)
	{
		return Static_the_orb::tc_equivalent (Impl::_get_ptr (), other);
	}

	TypeCode::_ref_type discriminator_type () const noexcept
	{
		return discriminator_type_;
	}

	Long default_index () const noexcept
	{
		return default_index_;
	}

	ULong member_count () const noexcept
	{
		return (ULong)members_.size ();
	}

	IDL::String member_name (ULong i) const
	{
		if (i >= members_.size ())
			throw Bounds ();
		return members_ [i].name;
	}

	TypeCode::_ref_type member_type (ULong i) const
	{
		if (i >= members_.size ())
			throw Bounds ();
		return members_ [i].type;
	}

	Any member_label (ULong i) const
	{
		if (i >= members_.size ())
			throw Bounds ();
		return members_ [i].label;
	}

	TypeCode::_ref_type get_compact_typecode ()
	{
		return Static_the_orb::get_compact_typecode (_get_ptr ());
	}

	size_t n_size () const noexcept
	{
		return size_;
	}

	size_t n_align () const noexcept
	{
		return align_;
	}

	void n_construct (void* p) const
	{
		Internal::check_pointer (p);
		if (default_index_ >= 0) {
			const Member& m = members_ [default_index_];
			discriminator_type_->n_copy (p, m.label.data ());
			m.type->n_construct ((Octet*)p + m.offset);
		} else {
			discriminator_type_->n_construct (p);
		}
	}

	void n_destruct (void* p) const
	{
		int idx = find_index (p);
		if (idx >= 0) {
			const Member& m = members_ [idx];
			m.type->n_destruct ((Octet*)p + m.offset);
		}
	}

	void n_copy (void* dst, const void* src) const
	{
		n_destruct (dst);
		discriminator_type_->n_copy (dst, src);
		int idx = find_index (src);
		if (idx >= 0) {
			const Member& m = members_ [idx];
			m.type->n_copy ((Octet*)dst + m.offset, (const Octet*)src + m.offset);
		}
	}

	void n_move (void* dst, void* src) const
	{
		n_destruct (dst);
		discriminator_type_->n_copy (dst, src);
		int idx = find_index (src);
		if (idx >= 0) {
			const Member& m = members_ [idx];
			m.type->n_move ((Octet*)dst + m.offset, (Octet*)src + m.offset);
		}
	}

	void n_marshal_in (const void* src, size_t count, Internal::IORequest_ptr rq) const
	{
		discriminator_type_->n_marshal_in (src, 1, rq);
		int idx = find_index (src);
		if (idx >= 0) {
			const Member& m = members_ [idx];
			m.type->n_marshal_in ((const Octet*)src + m.offset, 1, rq);
		}
	}

	void n_marshal_out (void* src, size_t count, Internal::IORequest_ptr rq) const
	{
		discriminator_type_->n_marshal_out (src, 1, rq);
		int idx = find_index (src);
		if (idx >= 0) {
			const Member& m = members_ [idx];
			m.type->n_marshal_out ((Octet*)src + m.offset, 1, rq);
		}
	}

	void n_unmarshal (Internal::IORequest_ptr rq, size_t count, void* dst) const
	{
		discriminator_type_->n_unmarshal (rq, 1, dst);
		int idx = find_index (dst);
		if (idx >= 0) {
			const Member& m = members_ [idx];
			m.type->n_unmarshal (rq, 1, (Octet*)dst + m.offset);
		}
	}

protected:
	virtual bool mark () noexcept override;
	virtual bool set_recursive (const IDL::String& id, const TC_Ref& ref) noexcept override;

private:
	int find_index (const void* p) const
	{
		Internal::check_pointer (p);

		for (ULong i = 0, cnt = (ULong)members_.size (); i < cnt; ++i) {
			if (i != default_index_ && !memcmp (p, members_ [i].label.data (), discriminator_size_))
				return i;
		}
		return default_index_;
	}

private:
	const TypeCode::_ref_type discriminator_type_;
	Members members_;
	size_t align_;
	size_t size_;
	size_t discriminator_size_;
	const Long default_index_;
};

}
}

#endif
