/*
* Nirvana package manager.
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
#include "Packages.h"
#include "PacFactory_s.h"

namespace Nirvana {
namespace PM {

class Static_pac_factory :
	public CORBA::servant_traits <Nirvana::PM::PacFactory>::ServantStatic <Static_pac_factory>
{
public:
	static Nirvana::PM::Packages::_ref_type create (CORBA::Object::_ptr_type comp)
	{
		return CORBA::make_stateless <::Packages> (comp)->_this ();
	}
};

}
}

NIRVANA_EXPORT_PSEUDO (_exp_Nirvana_PM_pac_factory, Nirvana::PM::Static_pac_factory)
