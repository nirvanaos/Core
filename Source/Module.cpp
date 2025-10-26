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
#include "Module.h"

namespace Nirvana {
namespace Core {

Module::Module (int32_t id, Port::Module&& bin, const ModuleStartup& startup_entry) :
	Binary (std::move (bin)),
	startup_entry_ (startup_entry),
	entry_point_ (ModuleInit::_check (startup_entry.startup)),
	release_time_ (0),
	ref_cnt_ (0),
	initial_ref_cnt_ (0),
	id_ (id)
{}

void Module::_remove_ref () noexcept
{
	AtomicCounter <false>::IntegralType rcnt = ref_cnt_.decrement_seq ();
	// Storing the release_time_ may be not atomic, but it is not critical.
	if (rcnt == initial_ref_cnt_)
		release_time_ = Chrono::steady_clock ();
}

void Module::initialize ()
{
	if (entry_point_)
		entry_point_->initialize ();
}

void Module::terminate () noexcept
{
	if (entry_point_) {
		try {
			entry_point_->terminate ();
		} catch (...) {
			// TODO: Log
		}
		entry_point_ = nullptr;
	}
}

void Module::raise_exception (CORBA::SystemException::Code code, unsigned minor)
{
	CORBA::Internal::Bridge <ModuleInit>* br = static_cast <CORBA::Internal::Bridge <ModuleInit>*> (&entry_point_);
	if (br)
		br->_epv ().epv.raise_exception (br, (short)code, (unsigned short)minor, nullptr);
}

}
}
