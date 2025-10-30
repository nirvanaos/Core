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
#include "POAManager.h"
#include "POA_Root.h"
#include "RqHelper.h"

using namespace CORBA;
using namespace CORBA::Core;
using namespace Nirvana::Core;

namespace PortableServer {
namespace Core {

void POAManager::ServeRequest::run () noexcept
{
	try {
		// Check that adapter is not destroyed.
		if (adapter_->destroy_called ())
			throw OBJ_ADAPTER (MAKE_OMG_MINOR (1));
		adapter_->serve_request (*request_);
	} catch (Exception& e) {
		request_->set_exception (std::move (e));
	}
	adapter_->on_request_finish ();
}

void POAManager::ServeRequest::on_crash (const siginfo& signal) noexcept
{
	Any ex = CORBA::Core::RqHelper::signal2exception (signal);
	try {
		request_->set_exception (ex);
	} catch (...) {}
}

void POAManager::discard_queued_requests ()
{
	while (!queue_.empty ()) {
		if (!queue_.top ().request->is_cancelled ()) {
			queue_.top ().request->set_exception (TRANSIENT (MAKE_OMG_MINOR (1)));
		}
		queue_.pop ();
	}
}

}
}
