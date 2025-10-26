/*
* Nirvana Core.
*
* This is a part of the Nirvana project.
*
* Author: Igor Popov
*
* Copyright (c) 2025 Igor Popov.
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
#ifndef NIRVANA_CORE_CALL_WITH_USER_EXCEPTIONS_H_
#define NIRVANA_CORE_CALL_WITH_USER_EXCEPTIONS_H_
#pragma once

#include "InternalRequest.h"

namespace Nirvana {
namespace Core {

template <class Callable, typename Memory>
void call_with_user_exceptions (SyncContext& target, Memory memory, Callable&& callable)
{
	Ref <InternalRequest> rq = InternalRequest::create ();
	rq->invoke ();
	SYNC_BEGIN (target, memory);
	try {
		rq->unmarshal_end ();
		callable ();
		rq->success ();
	} catch (CORBA::UserException& e) {
		rq->set_exception (std::move (e));
	}
	SYNC_END ();
	CORBA::Internal::ProxyRoot::check_request (rq->_get_ptr ());
}

}
}

#endif
