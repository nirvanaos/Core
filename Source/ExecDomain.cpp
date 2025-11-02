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
#include "ExecDomain.h"
#include "Binder.h"
#include "Chrono.h"
#include "MemContext.h"
#include "DeadlinePolicy.h"
#include "ThreadBackground.h"

namespace Nirvana {
namespace Core {

StaticallyAllocated <ExecDomain::Deleter> ExecDomain::deleter_;
StaticallyAllocated <ExecDomain::Reschedule> ExecDomain::reschedule_;

void ExecDomain::initialize () noexcept
{
	Creator::initialize (EXEC_DOMAIN_POOL_MIN);

	deleter_.construct ();
	reschedule_.construct ();
}

void ExecDomain::terminate () noexcept
{
	Creator::terminate ();
}

ExecDomain::~ExecDomain ()
{
	assert (!sync_context_);
	assert (mem_context_stack_.empty ());
	assert (!background_worker_);
}

Ref <ExecDomain> ExecDomain::create (const DeadlineTime deadline, Ref <MemContext>&& mem_context)
{
	Ref <ExecDomain> ed = Creator::create ();
	Scheduler::activity_begin ();
	try {
		ed->deadline_ = deadline;
		assert (ed->mem_context_stack_.empty ());

		MemContext* pmc = mem_context;
		ed->mem_context_stack_.push (std::move (mem_context));
		ed->mem_context_ = pmc;

		Thread* th = Thread::current_ptr ();
		if (th && th->executing ()) {
			ExecDomain* parent = th->exec_domain ();
			if (parent)
				ed->impersonation_context_ = parent->impersonation_context_;
		}

#ifndef NDEBUG
		assert (!ed->dbg_mem_context_stack_size_++);
#endif
	} catch (...) {
		ed->cleanup ();
		throw;
	}
	return ed;
}

Ref <MemContext> ExecDomain::get_mem_context (SyncContext& target, Heap* heap)
{
	Ref <MemContext> mem_context;
	SyncDomain* sd = target.sync_domain ();
	if (sd) {
		assert (!heap || heap == &sd->mem_context ().heap ());
		mem_context = &sd->mem_context ();
	} else if (heap)
		mem_context = MemContext::create (*heap);

	return mem_context;
}

void ExecDomain::async_call (const DeadlineTime& deadline, Runnable& runnable,
	SyncContext& target, Heap* heap)
{
	Ref <ExecDomain> exec_domain = create (deadline, target, heap);
	exec_domain->runnable_ = &runnable;
	exec_domain->spawn (target);
}

void ExecDomain::spawn (SyncContext& sync_context)
{
	assert (ExecContext::current_ptr () != this);
	assert (runnable_);
	_add_ref ();
	try {
		Ref <SyncContext> tmp (&sync_context);
		schedule (tmp);
	} catch (...) {
		_remove_ref ();
		throw;
	}
}

void ExecDomain::execute (Thread& thread, Ref <Executor> holder) noexcept
{
	thread.exec_domain (*this);
	if (!impersonation_context_.empty ())
		Thread::impersonate (impersonation_context_);
	switch_to ();

	// Perform possible neutral context calls, then return.
	neutral_context_loop (thread);
}

inline void ExecDomain::neutral_context_loop (Thread& worker) noexcept
{
	assert (&ExecContext::current () == &worker.neutral_context ());
	while (worker.neutral_context ().runnable ()) {
		worker.neutral_context ().run ();
		if (worker.executing ()) {
			ExecDomain* ed = worker.exec_domain ();
			if (ed) {
				assert (ed->runnable ());
				ed->switch_to ();
			}
		}
	}
}

void ExecDomain::cleanup () noexcept
{
	assert (!runnable_);

	leave_sync_domain ();

	// Just for case when SYNC_BEGIN called in the memory context destruction
	sync_context_ = &g_core_free_sync_context;

	assert (!mem_context_stack_.empty ());
	assert (1 == dbg_mem_context_stack_size_);
	do {
		mem_context_pop ();
	} while (!mem_context_stack_.empty ());

	sync_context_ = nullptr;

#ifndef NDEBUG
	dbg_mem_context_stack_size_ = 0;
#endif

	suspend_state_.store (SuspendState::NOT_SUSPENDED, std::memory_order_relaxed);
	restricted_mode_ = RestrictedMode::NO_RESTRICTIONS;

	ret_qnodes_clear ();

	if (scheduler_item_created_) {
		Scheduler::delete_item (false);
		scheduler_item_created_ = false;
	}
	
	std::fill_n (tls_, CoreTLS::CORE_TLS_COUNT, nullptr);

	impersonation_context_.clear ();

	resume_exception_.reset ();

	ExecContext::run_in_neutral_context (deleter_);
}

void ExecDomain::final_release () noexcept
{
	Creator::release (this);
	Scheduler::activity_end ();
}

void ExecDomain::Deleter::run () noexcept
{
	Thread& th = Thread::current ();
	ExecDomain* ed = th.exec_domain ();
	assert (ed);

	th.execute_end ();
	if (!BACKGROUND_THREAD_DISABLE && ed->background_worker_) {
		ed->background_worker_->stop ();
		ed->background_worker_ = nullptr;
	}

	ed->_remove_ref ();
}

MemContext& ExecDomain::mem_context ()
{
	if (!mem_context_) {
		mem_context_ =
			mem_context_stack_.top () = MemContext::create (
				sync_context ().sync_context_type () == SyncContext::Type::FREE_MODULE_INIT);
	}
	return *mem_context_;
}

void ExecDomain::mem_context_push (Ref <MemContext>&& mem_context)
{
	// Ensure that memory context is not temporary replaced
	assert ((mem_context_ == nullptr && mem_context_stack_.empty ()) || mem_context_ == mem_context_stack_.top ());

	MemContext* p = mem_context;
	mem_context_stack_.emplace (std::move (mem_context));
	mem_context_ = p;
#ifndef NDEBUG
	++dbg_mem_context_stack_size_;
#endif
}

void ExecDomain::mem_context_pop () noexcept
{
	// Ensure that memory context is not temporary replaced
	assert (mem_context_ == mem_context_stack_.top ());

	{ // On releasing the memory context reference, stack must be consistent.
		Ref <MemContext> tmp (std::move (mem_context_stack_.top ()));
		mem_context_ = tmp;
		mem_context_stack_.pop ();
	}
#ifndef NDEBUG
	--dbg_mem_context_stack_size_;
#endif
	if (!mem_context_stack_.empty ())
		mem_context_ = mem_context_stack_.top ();
	else
		mem_context_ = nullptr;
}

void ExecDomain::create_background_worker ()
{
	if (!BACKGROUND_THREAD_DISABLE && !background_worker_)
		background_worker_ = ThreadBackground::spawn (*this);
}

void ExecDomain::schedule (Ref <SyncContext>& target, bool ret)
{
	// Called in neutral execution context
	assert (ExecContext::current_ptr () != this);

	Ref <SyncDomain> sync_domain = target->sync_domain ();
	bool background = false;
	if (!sync_domain) {
		if (!BACKGROUND_THREAD_DISABLE && INFINITE_DEADLINE == deadline ()) {
			background = true;
			assert (!ret || background_worker_);
			if (!ret)
				create_background_worker ();
		} else if (!scheduler_item_created_) {
			Scheduler::create_item (false);
			scheduler_item_created_ = true;
		}
	}

	sync_context_.swap (target);
	try {
		if (sync_domain) {
			if (ret)
				sync_domain->schedule (ret_qnode_pop (), deadline (), *this);
			else
				sync_domain->schedule (deadline (), *this);
		} else {
			if (!BACKGROUND_THREAD_DISABLE && background)
				background_worker_->execute ();
			else
				Scheduler::schedule (deadline (), *this);
		}
	} catch (...) {
		sync_context_.swap (target);
		throw;
	}
}

void ExecDomain::schedule_call_no_push_mem (SyncContext& target)
{
	// If old context is a synchronization domain, we
	// allocate queue node to perform return without a risk
	// of memory allocation failure.
	{
		SyncDomain* sd = sync_context_->sync_domain ();
		if (sd)
			ret_qnode_push (*sd);
		else if (deadline () == INFINITE_DEADLINE)
			create_background_worker (); // Prepare to return to background
	}

	leave_sync_domain ();

	if (BACKGROUND_THREAD_DISABLE || target.sync_domain () ||
		!(deadline () == INFINITE_DEADLINE && &Thread::current () == background_worker_)) {
		// Need to schedule

		// Call schedule() in the neutral context
		schedule_.call (sync_context (), target);

	} else
		sync_context (target);
}

void ExecDomain::schedule_return (SyncContext& target, bool no_reschedule) noexcept
{
	mem_context_pop ();

	SyncDomain* target_sd = target.sync_domain ();
	if (no_reschedule && (&sync_context () == &target) && (target_sd == execution_sync_domain_))
		return;

	leave_sync_domain ();

	if (BACKGROUND_THREAD_DISABLE || target_sd ||
		!(deadline () == INFINITE_DEADLINE && &Thread::current () == background_worker_)) {

		schedule_.ret (target);
	} else
		sync_context (target);
}

DeadlineTime ExecDomain::get_request_deadline (bool oneway) const noexcept
{
	const DeadlinePolicyContext* deadline_context = nullptr;
	{
		MemContext* mc = mem_context_ptr ();
		if (mc)
			deadline_context = mc->deadline_policy_ptr ();
	}

	Nirvana::System::DeadlinePolicy deadline_policy = deadline_context ?
		(oneway ? deadline_context->policy_oneway () : deadline_context->policy_async ())
		:
		(oneway ? DeadlinePolicyContext::ONEWAY_DEFAULT : DeadlinePolicyContext::ASYNC_DEFAULT);

	DeadlineTime dl;
	if (deadline_policy == Nirvana::System::DEADLINE_POLICY_INHERIT)
		dl = deadline ();
	else if (deadline_policy == Nirvana::System::DEADLINE_POLICY_INFINITE)
		dl = INFINITE_DEADLINE;
	else
		dl = Chrono::make_deadline (deadline_policy);

	return dl;
}

void ExecDomain::Schedule::run () noexcept
{
	Thread& th = Thread::current ();
	ExecDomain* ed = th.exec_domain ();
	assert (ed);
	try {
		{
			Ref <SyncContext> sc (sync_context_);
			ed->schedule (sc, ret_);
		}
		th.execute_end ();
	} catch (const CORBA::SystemException& ex) {
		exception_ = ex.__code ();
	}
}

inline void ExecDomain::Schedule::call (SyncContext& source, SyncContext& target)
{
	sync_context_ = &target;
	ret_ = false;
	ExecContext::run_in_neutral_context (*this);
	// Handle possible schedule() exceptions
	if (CORBA::SystemException::EC_NO_EXCEPTION != exception_) {
		CORBA::SystemException::Code exc = exception_;
		exception_ = CORBA::SystemException::EC_NO_EXCEPTION;
		// We leaved old sync domain so we must enter into prev synchronization domain back
		// before throwing the exception.
		ret (source);
		CORBA::SystemException::_raise_by_code (exc);
	}
}

void ExecDomain::Schedule::ret (SyncContext& target) noexcept
{
	sync_context_ = &target;
	ret_ = true;
	ExecContext::run_in_neutral_context (*this);
	// schedule() can not throw exception in the return mode.
}

bool ExecDomain::suspend_prepare_no_leave (SyncContext* resume_context, bool push_qnode)
{
	assert (Thread::current ().exec_domain () == this);
	assert (suspend_state_ == SuspendState::NOT_SUSPENDED);

	{ // If a background worker will need for resume, create it now.
		SyncContext* res_ctx = resume_context;
		if (!res_ctx)
			res_ctx = sync_context_;
		if (!res_ctx->sync_domain () && INFINITE_DEADLINE == deadline ())
			create_background_worker ();
	}

	bool qnode = false;

	{ // Prepare return to the current sync domain
		SyncDomain* sync_domain = sync_context_->sync_domain ();
		if (sync_domain && (!resume_context || push_qnode)) {
			ret_qnode_push (*sync_domain);
			qnode = true;
		}
	}

	if (resume_context)
		sync_context_ = resume_context;

	suspend_state_.store (SuspendState::PREPARED, std::memory_order_release);

	return qnode;
}

void ExecDomain::suspend_unprepare (bool pop_qnode) noexcept
{
	suspend_state_.store (SuspendState::NOT_SUSPENDED, std::memory_order_release);
	if (pop_qnode)
		ret_qnode_pop ()->release ();
}

void ExecDomain::suspend_prepared () noexcept
{
	assert (Thread::current ().exec_domain () == this);
	assert (&ExecContext::current () == &Thread::current ().neutral_context ());
	assert (suspend_state_ == SuspendState::PREPARED);

	assert (!execution_sync_domain_);

	suspend_state_.store (SuspendState::SUSPENDED, std::memory_order_release);

	Thread::current ().execute_end ();
}

void ExecDomain::resume () noexcept
{
	assert (ExecContext::current_ptr () != this);
	assert (sync_context_);
	assert (suspend_state_ == SuspendState::SUSPENDED);

	suspend_state_.store (SuspendState::NOT_SUSPENDED, std::memory_order_release);

	// Hold reference to sync_context_ to avoid it's destruction out of context.
	Ref <SyncContext> tmp (sync_context_);

	// schedule with ret = true does not throw exceptions
	schedule (tmp, true);
}

void ExecDomain::reschedule () noexcept
{
	Thread& thr = Thread::current ();
	ExecDomain* ed = thr.exec_domain ();
	assert (ed);
	if (BACKGROUND_THREAD_DISABLE || &thr != ed->background_worker_) {
		try {
			ed->suspend_prepare ();
			ExecContext::run_in_neutral_context (reschedule_);
		} catch (...) {}
	} else
		ed->background_worker_->yield ();
}

void ExecDomain::leave_sync_domain () noexcept
{
	if (execution_sync_domain_)
		Ref <SyncDomain> (std::move (execution_sync_domain_))->leave ();
}

void ExecDomain::suspend ()
{
	assert (suspend_state_ == SuspendState::PREPARED);
	ExecContext::run_in_neutral_context (suspend_);
	resume_exception_.check ();
}

bool ExecDomain::suspended () const noexcept
{
	for (BackOff bo;; bo ()) {
		SuspendState state = suspend_state_.load (std::memory_order_acquire);
		switch (state) {
			case SuspendState::NOT_SUSPENDED:
				return false;
			case SuspendState::SUSPENDED:
				return true;
		}
	}
}

void ExecDomain::Reschedule::run () noexcept
{
	ExecDomain& ed = ExecDomain::current ();
	ed.suspend_prepared ();
	ed.resume ();
}

void ExecDomain::Suspend::run () noexcept
{
	ExecDomain::current ().suspend_prepared ();
}

bool ExecDomain::on_signal (const siginfo_t& signal)
{
	// TODO: Check for the signal handlers and return true if signal is handled.
	if (signal.si_excode >= 0)
		Binder::raise_exception (signal);

	return false;
}

}
}
