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
#ifndef NIRVANA_ORB_CORE_OBJECTKEY_H_
#define NIRVANA_ORB_CORE_OBJECTKEY_H_
#pragma once

#include <CORBA/Server.h>
#include "IDL/ORB/IOP.h"
#include "StreamOutEncap.h"

namespace CORBA {
namespace Core {

class StreamIn;

}
}

namespace PortableServer {
namespace Core {

typedef IDL::Sequence <IDL::String> AdapterPath;

class POA_Base;

/// Object key internal structure.
class ObjectKey
{
public:
	ObjectKey ()
	{}

	ObjectKey (POA_Base& adapter);
	ObjectKey (const POA_Base& adapter, const ObjectId& oid);
	ObjectKey (const POA_Base& adapter, ObjectId&& oid);
	explicit ObjectKey (const IOP::ObjectKey& object_key);

	explicit operator IOP::ObjectKey () const;

	const AdapterPath& adapter_path () const noexcept
	{
		return adapter_path_;
	}

	const ObjectId& object_id () const noexcept
	{
		return object_id_;
	}

	ObjectId& object_id () noexcept
	{
		return object_id_;
	}

	static ObjectId get_object_id (const IOP::ObjectKey& object_key);
	static AdapterPath get_adapter_path (const IOP::ObjectKey& object_key);

private:
	AdapterPath adapter_path_;
	ObjectId object_id_;
};

}
}

#endif
