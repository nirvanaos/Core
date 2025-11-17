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
#include "Reference.h"

using namespace Nirvana::Core;
using namespace CORBA;
using namespace CORBA::Internal;
using namespace CORBA::Core;

namespace CORBA {
namespace Core {

const TimeBase::TimeT Reference::DEADLINE_MAX;

IDL::String Reference::_repository_id ()
{
	if (has_primary_interface ())
		return ProxyManager::_repository_id ();
	else {
		IORequest::_ref_type rq = create_request (_make_op_idx ((UShort)ObjectOp::REPOSITORY_ID), 3,
			nullptr);
		rq->invoke ();
		IDL::String _ret;
		IDL::Type <IDL::String>::unmarshal (rq, _ret);
		rq->unmarshal_end ();
		return _ret;
	}
}

Policy::_ref_type Reference::_get_policy (PolicyType policy_type)
{
	Policy::_ref_type policy;
	if (policies_)
		policy = policies_->get_policy (policy_type);
	return policy;
}

Object::_ref_type Reference::_get_component ()
{
	IORequest::_ref_type rq = create_request (_make_op_idx ((UShort)ObjectOp::GET_COMPONENT), 3,
		nullptr);
	rq->invoke ();
	Object::_ref_type _ret;
	IDL::Type <Object>::unmarshal (rq, _ret);
	rq->unmarshal_end ();
	return _ret;
}

bool Reference::is_local_object_op (Internal::OperationIndex op) const noexcept
{
	if (ProxyManager::is_local_object_op (op)) {
		switch ((ObjectOp)Internal::operation_idx (op)) {
		case ObjectOp::REPOSITORY_ID:
			return has_primary_interface ();

		case ObjectOp::GET_DOMAIN_MANAGERS:
		case ObjectOp::GET_COMPONENT:
			return false;

		default:
			return true;
		}
	} else
		return false;
}

}
}
