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
#ifndef NIRVANA_ORB_CORE_DOMAINREMOTE_H_
#define NIRVANA_ORB_CORE_DOMAINREMOTE_H_
#pragma once

#include "Domain.h"
#include <CORBA/IIOP.h>

namespace Nirvana {
namespace Core {
template <class> class HeapAllocator;
}
}

namespace CORBA {
namespace Core {

/// Other protection domain on the same system.
class DomainRemote :
	public Domain
{
public:
	DomainRemote (const IIOP::ListenPoint& listen_point) :
		Domain (0, 1 * TimeBase::SECOND, 1 * TimeBase::MINUTE, 2 * TimeBase::MINUTE),
		listen_point_ (listen_point)
	{}

	~DomainRemote ()
	{}

	virtual Internal::IORequest::_ref_type create_request (const IOP::ObjectKey& object_key,
		const Internal::Operation& metadata, unsigned flags) override;

	// Add DGC references owned by this domain.
	// Used only in the connection-oriented GC.
	// Not used if the domain is DGC-enabled.
	void add_DGC_objects (ReferenceSet <Nirvana::Core::HeapAllocator>& references) NIRVANA_NOEXCEPT;

protected:
	virtual void destroy () NIRVANA_NOEXCEPT override;

private:
	const IIOP::ListenPoint& listen_point_;

	// DGC references owned by this domain.
	// Used only in the connection-oriented GC.
	// Not used if the domain is DGC-enabled.
	ReferenceSet <Nirvana::Core::UserAllocator> owned_references_;
};

}
}

#endif
