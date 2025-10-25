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
#ifndef NIRVANA_ORB_CORE_CALLBACKREF_H_
#define NIRVANA_ORB_CORE_CALLBACKREF_H_
#pragma once

#include <CORBA/CORBA.h>
#include <CORBA/Proxy/OperationIndex.h>
#include <CORBA/Proxy/InterfaceMetadata.h>
#include "IDL/ORB/RequestCallback.h"

namespace CORBA {
namespace Core {

class ProxyManager;

class CallbackRef
{
public:
	CallbackRef (Internal::Interface::_ptr_type callback, const ProxyManager& proxy, Internal::OperationIndex op);
	
	CallbackRef (RequestCallback::_ptr_type callback) noexcept :
		callback_ (callback),
		handler_op_idx_ (0)
	{}

	CallbackRef () :
		handler_op_idx_ (0)
	{}

	CallbackRef (nullptr_t) :
		handler_op_idx_ (0)
	{}

	operator bool () const noexcept
	{
		return callback_.operator bool ();
	}

	bool is_handler () const noexcept
	{
		return handler_op_idx_ != 0;
	}

	void call (const Internal::Operation& metadata, Internal::IORequest::_ptr_type rq) noexcept;

private:
	Internal::Interface::_ref_type callback_;
	Internal::OperationIndex handler_op_idx_;
};

}
}

#endif
