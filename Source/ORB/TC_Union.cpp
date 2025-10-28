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
#include "TC_Union.h"

namespace CORBA {
namespace Core {

TC_Union::TC_Union (IDL::String&& id, IDL::String&& name, TypeCode::_ptr_type discriminator_type, Long default_index) :
	Impl (TCKind::tk_union, std::move (id), std::move (name)),
	discriminator_type_ (discriminator_type),
	default_index_ (default_index)
{
	discriminator_size_ = discriminator_type_->n_size ();
}

void TC_Union::set_members (Members&& members)
{
	members_ = std::move (members);
	size_t align = discriminator_type_->n_align ();
	size_t size = discriminator_size_;
	for (auto& m : members_) {
		size_t m_align = m.type->n_align ();
		if (align < m_align)
			align = m_align;
		size_t off = Nirvana::round_up (discriminator_size_, m_align);
		m.offset = off;
		off += m.type->n_size ();
		if (size < off)
			size = off;
	}
	size_ = Nirvana::round_up (size, align);
	align_ = align;
}

bool TC_Union::mark () noexcept
{
	if (!TC_ComplexBase::mark ())
		return false;
	for (auto& m : members_) {
		m.type.mark ();
	}
	return true;
}

bool TC_Union::set_recursive (const IDL::String& id, const TC_Ref& ref) noexcept
{
	for (auto& m : members_) {
		m.type.replace_recursive_placeholder (id, ref);
	}
	return false;
}

}
}
