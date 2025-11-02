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
#include "MemContext.h"
#include "ExecDomain.h"
#include "BinderMemory.h"
#include "Chrono.h"
#include "HeapDynamic.h"
#include "ORB/Services.h"
#include "ORB/RefCnt.h"
#include <Port/config.h>

namespace Nirvana {
namespace Core {

static_assert (sizeof (MemContext) == 8 * sizeof (void*), "sizeof (MemContext)");

bool MemContext::is_current (const MemContext* context) noexcept
{
	Thread* th = Thread::current_ptr ();
	if (th && th->executing ()) {
		ExecDomain* ed = th->exec_domain ();
		if (ed)
			return ed->mem_context_ptr () == context;
	}
	return !context;
}

Ref <Heap> MemContext::create_heap ()
{
	return sizeof (void*) > 2 ? HeapDynamic::create () : Ref <Heap> (&Heap::shared_heap ());
}

Ref <MemContext> MemContext::create (Ref <Heap>&& heap, bool class_library_init, bool core_context)
{
	Heap& allocate_from = class_library_init ? BinderMemory::heap () : *heap;
	size_t cb = sizeof (MemContext);
	return CreateRef (new (allocate_from.allocate (nullptr, cb, 0))
		MemContext (std::move (heap), class_library_init, core_context));
}

MemContext& MemContext::current ()
{
	return ExecDomain::current ().mem_context ();
}

MemContext* MemContext::current_ptr () noexcept
{
	Thread* th = Thread::current_ptr ();
	if (th && th->executing ()) {
		ExecDomain* ed = th->exec_domain ();
		if (ed)
			return ed->mem_context_ptr ();
	}
	return nullptr;
}

inline
MemContext::MemContext (Ref <Heap>&& heap, bool class_library_init, bool core_context) noexcept :
	heap_ (std::move (heap)),
	ref_cnt_ (1),
	class_library_init_ (class_library_init),
	core_context_ (core_context),
	error_number_ (0)
{
	MemContext* parent = current_ptr ();
	if (parent)
		locale_ = parent->locale ();
}

inline
MemContext::~MemContext ()
{
	// Set core_context_ to prohibit runtime proxy creation during the destruction.
	core_context_ = true;
}

class MemContext::Replacer
{
public:
	Replacer (MemContext& mc, ExecDomain& ed) :
		exec_domain_ (ed)
	{
		// Increment reference counter to prevent recursive deletion
		mc.ref_cnt_.increment ();
		if (ed.mem_context_stack_empty ()) {
			// If context stack is empty, mem_context_push definitely won't throw an exception
			// (stack has preallocated space).
			pop_ = true;
			ed.mem_context_push (&mc);
		} else {
			pop_ = false;
			ref_ = &mc;
			ed.mem_context_swap (ref_);
		}
	}

	~Replacer ()
	{
		if (pop_)
			exec_domain_.mem_context_pop ();
		else
			exec_domain_.mem_context_swap (ref_);
	}

private:
	ExecDomain& exec_domain_;
	Ref <MemContext> ref_;
	bool pop_;
};

class MemContext::Deleter : public Runnable
{
public:
	Deleter (MemContext& mc) :
		mc_ (mc)
	{}

private:
	virtual void run () noexcept override
	{
		mc_.destroy (ExecDomain::current ());
	}

private:
	MemContext& mc_;
};

void MemContext::_remove_ref () noexcept
{
	if (!ref_cnt_.decrement_seq ()) {

		Thread* th = Thread::current_ptr ();
		if (th && th->executing ()) {
			ExecDomain* ed = th->exec_domain ();
			if (ed) {
				destroy (*ed);
				return;
			}
		}

		if (ENABLE_MEM_CONTEXT_ASYNC_DESTROY) {
			// For some host implementations, MemContext may be released out of the execution domain.
			// In this case we create async call for this.

			Nirvana::DeadlineTime deadline =
				GC_DEADLINE == INFINITE_DEADLINE ?
				INFINITE_DEADLINE : Chrono::make_deadline (GC_DEADLINE);

			try {
				ExecDomain::async_call <Deleter> (deadline, g_core_free_sync_context, nullptr,
					std::ref (*this));
			} catch (...) {
				assert (false);
				// TODO: Log
			}
		} else
			unrecoverable_error (-1);
	}
}

void MemContext::destroy (ExecDomain& cur_ed) noexcept
{
	// Hold heap reference
	Ref <Heap> heap;
	if (class_library_init_)
		heap = &BinderMemory::heap ();
	else
		heap = heap_;

	if (cur_ed.mem_context_ptr () != this) {
		// Replace memory context and call destructor
		Replacer replace (*this, cur_ed);
		this->~MemContext ();
	} else
		this->~MemContext ();

	// Release memory
	heap->release (this, sizeof (MemContext));
}

FileDescriptors MemContext::get_inherited_files (unsigned create_std_mask)
{
	FileDescriptors files;
	const FileDescriptorsContext* fdc = file_descriptors_ptr ();
	if (fdc) {
		unsigned std_mask;
		files = fdc->get_inherited_files (&std_mask);
		create_std_mask &= ~std_mask;
	}

	if (create_std_mask) {

		Nirvana::File::_ref_type console = Nirvana::File::_narrow (
			CORBA::Core::Services::bind (CORBA::Core::Services::Console));
		assert (console);
		FileDescr fd;
		fd.access (console->open (
			((create_std_mask & 1) ? O_RDWR : O_WRONLY) | O_TEXT, 0));

		for (unsigned i = 0, mask = 1; i < 3; mask <<= 1, ++i) {
			if (mask & create_std_mask) {
				fd.descriptors ().push_back (i);
			}
		}
		files.push_back (std::move (fd));
	}

	return files;
}

}
}
