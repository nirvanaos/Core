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
#include "RequestLocal.h"
#include "../ExecDomain.h"

using namespace Nirvana::Core;

namespace CORBA {
namespace Core {

RequestLocal::RequestLocal (ProxyManager& proxy, Internal::OperationIndex op_idx,
	Nirvana::Core::Heap* callee_memory, unsigned response_flags) noexcept :
	RequestLocalBase (callee_memory, response_flags),
	proxy_ (&proxy),
	op_idx_ (op_idx)
{}

void RequestLocal::invoke ()
{
	RequestLocalBase::invoke ();
	// We don't need to handle exceptions here, because invoke_sync ()
	// does not throw exceptions.
	Nirvana::Core::Synchronized _sync_frame (proxy ().get_sync_context (op_idx ()), target_heap ());
	invoke_sync ();
}

void RequestLocal::invoke_sync () noexcept
{
	assert (State::CALL == state ()); // RequestLocalBase::invoke () must be called
	proxy ().invoke (op_idx (), _get_ptr ());
}

void RequestLocalOneway::invoke ()
{
	RequestLocalBase::invoke (); // rewind
	ExecDomain::async_call <Runnable> (ExecDomain::current ().get_request_deadline (!response_flags ()),
		proxy ().get_sync_context (op_idx ()), target_heap (), std::ref (*this));
}

void RequestLocalOneway::Runnable::run () noexcept
{
	request_->run ();
}

void RequestLocalAsync::cancel () noexcept
{
	if (set_cancelled ())
		response_flags_ = 0; // Prevent marshaling
}

void RequestLocalAsync::finalize () noexcept
{
	Base::finalize ();
	callback_.call (proxy ().operation_metadata (op_idx ()), _get_ptr ());
}

}
}
