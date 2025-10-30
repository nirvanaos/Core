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
#include "RequestOutSync.h"

namespace CORBA {
namespace Core {

void RequestOutSyncBase::WaitOp::run () noexcept
{
	obj_.source_exec_domain_->suspend_prepared ();
	if (obj_.state_.exchange (State::SUSPENDED) == State::FINALIZED)
		obj_.source_exec_domain_->resume ();
}

void RequestOutSyncBase::pre_invoke ()
{
	Nirvana::Core::SyncDomain* sync_domain = source_exec_domain_->sync_context ().sync_domain ();
	if (sync_domain && base_.metadata ().flags & Internal::Operation::FLAG_OUT_CPLX) {
		source_exec_domain_->mem_context_push (Nirvana::Core::MemContext::create (base_.memory ().heap ()));
		sync_domain_after_unmarshal_ = sync_domain;
		try {
			source_exec_domain_->suspend_prepare (&Nirvana::Core::g_core_free_sync_context, true);
		} catch (...) {
			source_exec_domain_->mem_context_pop ();
			sync_domain_after_unmarshal_ = nullptr;
			throw;
		}
	} else
		source_exec_domain_->suspend_prepare ();
}

void RequestOutSyncBase::post_finalize () noexcept
{
	if (state_.exchange (State::FINALIZED) == State::SUSPENDED)
		source_exec_domain_->resume ();
}

}
}
