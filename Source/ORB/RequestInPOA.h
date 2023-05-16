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
#ifndef NIRVANA_ORB_CORE_REQUESTINPOA_H_
#define NIRVANA_ORB_CORE_REQUESTINPOA_H_
#pragma once

#include "../CoreInterface.h"
#include <CORBA/IOP.h>
#include <CORBA/Proxy/IOReference.h>

namespace Nirvana {
namespace Core {

class MemContext;

}
}

namespace CORBA {
namespace Core {

class ServantProxyObject;

/// \brief Request for the POA processing.
class NIRVANA_NOVTABLE RequestInPOA
{
	DECLARE_CORE_INTERFACE
public:
	/// \returns Target object key.
	virtual const IOP::ObjectKey& object_key () const NIRVANA_NOEXCEPT = 0;

	/// \returns Operation name.
	virtual Internal::StringView <Char> operation () const NIRVANA_NOEXCEPT = 0;

	/// Serve the request.
	/// 
	/// \param proxy The servant proxy.
	virtual void serve (ServantProxyObject& proxy, Internal::IOReference::OperationIndex op_idx) = 0;

	/// Return exception to caller.
	/// Operation has move semantics so \p e may be cleared.
	virtual void set_exception (Any& e) = 0;

	void set_exception (Exception&& e) NIRVANA_NOEXCEPT;
	void set_unknown_exception () NIRVANA_NOEXCEPT;

	virtual bool is_cancelled () const NIRVANA_NOEXCEPT = 0;

	virtual Nirvana::Core::MemContext* memory () const NIRVANA_NOEXCEPT = 0;

protected:
	RequestInPOA ();
	~RequestInPOA ();
};

}
}

#endif
