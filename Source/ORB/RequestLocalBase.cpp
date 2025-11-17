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
#include "RequestLocalBase.h"
#include "../ExecDomain.h"
#include "../virtual_copy.h"

using namespace Nirvana;
using namespace Nirvana::Core;

namespace CORBA {

using namespace Internal;

namespace Core {

const size_t RequestLocalBase::BLOCK_SIZE;

RequestLocalBase::RequestLocalBase (Heap* callee_memory, unsigned response_flags) noexcept :
	caller_memory_ (&MemContext::current ()),
	callee_memory_ (callee_memory),
	cur_ptr_ (nullptr),
	state_ (State::CALLER),
	response_flags_ ((uint8_t)response_flags),
	cancelled_ (false),
	first_block_ (nullptr),
	cur_block_ (nullptr),
	interfaces_ (nullptr),
	segments_ (nullptr)
{
	assert ((uintptr_t)this % BLOCK_SIZE == 0);
}

Heap& RequestLocalBase::target_memory ()
{
	switch (state_) {
	case State::CALLER:
	case State::CALL:
		// Caller-side allocation
		if (!callee_memory_)
			callee_memory_ = MemContext::create_heap ();
		return *callee_memory_;

	default:
		// Callee-side allocation
		return caller_memory_->heap ();
	}
}

Heap& RequestLocalBase::source_memory ()
{
	switch (state_) {
	case State::CALLER:
	case State::CALL:
		// Caller-side
		return caller_memory_->heap ();

	default:
		// Callee-side allocation
		// callee_memory_ must be set in marshal_op ()
		assert (callee_memory_);
		return *callee_memory_;
	}
}

void RequestLocalBase::clear_on_callee_side () noexcept
{
	cleanup ();
	reset ();
}

bool RequestLocalBase::marshal_op () noexcept
{
	if (State::CALL == state_) {
		ExecDomain& ed = ExecDomain::current ();

		// callee_memory_ here may be nil or contain temporary memory context.
		// We must set it to the callee memory context.
		MemContext* mc = ed.mem_context_ptr ();
		if (mc)
			callee_memory_ = &mc->heap ();
		else
			callee_memory_ = nullptr;

		// Leave sync domain, if any.
		// Output data marshaling performed out of sync domain.
		ed.switch_to_free_sync_context ();

		state_ = State::CALLEE;
	}
	return state_ == State::CALLER || (response_flags_ & RESPONSE_DATA)
		|| (state_ == State::EXCEPTION && response_flags_);
}

void RequestLocalBase::rewind () noexcept
{
	invert_list ((Element*&)interfaces_);
	invert_list ((Element*&)segments_);
	reset ();
}

void RequestLocalBase::invert_list (Element*& head)
{
	Element* p = head;
	Element* tail = nullptr;
	while (p) {
		Element* next = p->next;
		p->next = tail;
		tail = p;
		p = next;
	}
	head = tail;
}

void RequestLocalBase::cleanup () noexcept
{
	{
		const ItfRecord* p = interfaces_;
		interfaces_ = nullptr;
		while (p) {
			interface_release ((Interface*)p->ptr);
			p = (const ItfRecord*)p->next;
		}
	}
	{
		const Segment* segment = segments_;
		if (segment) {
			segments_ = nullptr;
			Heap& heap = target_memory ();
			do {
				heap.release (segment->ptr, segment->allocated_size);
				segment = (const Segment*)segment->next;
			} while (segment);
		}
	}
	{
		BlockHdr* block = first_block_;
		if (block) {
			first_block_ = nullptr;
			Heap& heap = caller_memory_->heap ();
			while (block) {
				BlockHdr* next = block->next;
				heap.release (block, block->size);
				block = next;
			}
		}
	}
}

void RequestLocalBase::write (size_t align, size_t size, const void* data)
{
	if (!size)
		return;
	if (!data)
		throw BAD_PARAM ();

	const Octet* src = (const Octet*)data;
	Octet* dst = round_up (cur_ptr_, align);
	ptrdiff_t cb = cur_block_end () - dst;
	if (cb >= (ptrdiff_t)align) {
		// Write at current block
		if ((size_t)cb > size)
			cb = size;
		virtual_copy (src, cb, dst);
		dst += cb;
		src += cb;
		size -= cb;
	}

	// If there is data left, write it all to the new block.
	if (size) {

		// Adjust alignment if the remaining size less than it
		if (align > size)
			align = flp2 (size);

		dst = allocate_block (align, size);
		virtual_copy (src, size, dst);
		dst += size;
	}
	cur_ptr_ = dst;
}

void RequestLocalBase::write_size (size_t size)
{
	Octet* dst = round_up (cur_ptr_, alignof (size_t));
	if (cur_block_end () - dst < (ptrdiff_t)sizeof (size_t))
		dst = allocate_block (alignof (size_t), sizeof (size_t));
	*(size_t*)dst = size;
	cur_ptr_ = dst + sizeof (size_t);
}

void RequestLocalBase::write8 (unsigned val)
{
	Octet* dst = cur_ptr_;
	if (cur_block_end () - dst < 1)
		dst = allocate_block (1, 1);
	*dst = (Octet)val;
	cur_ptr_ = dst + 1;
}

Octet* RequestLocalBase::allocate_block (size_t align, size_t size)
{
	size_t data_offset = round_up (sizeof (BlockHdr), align);
	size_t block_size = std::max (BLOCK_SIZE, data_offset + size);
	BlockHdr* block = (BlockHdr*)caller_memory_->heap ().allocate (nullptr, block_size, 0);
	block->size = block_size;
	block->next = nullptr;
	if (!first_block_)
		first_block_ = block;
	if (cur_block_)
		cur_block_->next = block;
	cur_block_ = block;
	return (cur_ptr_ = (Octet*)block + data_offset);
}

void RequestLocalBase::read (size_t align, size_t size, void* data)
{
	if (!size)
		return;

	Octet* dst = (Octet*)data;

	const Octet* src = round_up (cur_ptr_, align);
	for (;;) {
		ptrdiff_t cb = cur_block_end () - src;
		if (cb >= (ptrdiff_t)align) {
			// Read from current block
			if ((size_t)cb > size)
				cb = size;
			virtual_copy (src, cb, dst);
			dst += cb;
			src += cb;
			size -= cb;
			if (!size)
				break;
			// Adjust alignment if the remaining size less than it
			if (align > size)
				align = flp2 (size);
		}

		src = next_block (align);
	}

	cur_ptr_ = (Octet*)src;
}

size_t RequestLocalBase::read_size ()
{
	const Octet* src = round_up (cur_ptr_, alignof (size_t));
	if (cur_block_end () - src < (ptrdiff_t)sizeof (size_t))
		src = next_block (alignof (size_t));
	size_t ret = *(const size_t*)src;
	cur_ptr_ = (Octet*)src + sizeof (size_t);
	return ret;
}

unsigned RequestLocalBase::read8 ()
{
	const Octet* src = cur_ptr_;
	if (cur_block_end () - src < 1)
		src = next_block (1);
	unsigned ret = *src;
	cur_ptr_ = (Octet*)src + 1;
	return ret;
}

const Octet* RequestLocalBase::next_block (size_t align)
{
	if (!cur_block_)
		cur_block_ = first_block_;
	else
		cur_block_ = cur_block_->next;
	if (!cur_block_)
		throw MARSHAL (MARSHAL_MINOR_FEWER);
	return (cur_ptr_ = round_up ((Octet*)(cur_block_ + 1), align));
}

RequestLocalBase::Element* RequestLocalBase::get_element_buffer (size_t size)
{
	Octet* dst = round_up (cur_ptr_, alignof (Element));
	if ((cur_block_end () - dst) < (ptrdiff_t)size)
		dst = allocate_block (alignof (Element), size);
	cur_ptr_ = dst + size;
	return (Element*)dst;
}

void RequestLocalBase::marshal_segment (size_t align, size_t element_size,
	size_t element_count, void* data, size_t& allocated_size)
{
	assert (element_count);
	assert (element_size % align == 0);
	size_t size = element_count * element_size;
	if (allocated_size && allocated_size < size)
		throw BAD_PARAM ();
	Segment* segment = (Segment*)get_element_buffer (sizeof (Segment));

	Heap& target_heap = target_memory ();

	// If allocated_size != 0, we can adopt memory block (move semantic).
	if (allocated_size) {
		Heap& source_heap = source_memory ();
		if (&target_heap == &source_heap) {
			// Heaps are the same, just adopt memory block
			segment->allocated_size = allocated_size;
			segment->ptr = data;
			allocated_size = 0;
		} else {
			// Move memory block from one heap to another
			segment->ptr = target_heap.move_from (source_heap, data, size);
			size_t cb_release = allocated_size - size;
			allocated_size = 0;
			if (cb_release)
				source_heap.release ((Octet*)data + size, cb_release);
			segment->allocated_size = size;
		}
	} else {
		// Copy block
		segment->ptr = target_heap.copy (nullptr, data, size, 0);
		segment->allocated_size = size;
	}
	segment->next = segments_;
	segments_ = segment;
}

void RequestLocalBase::unmarshal_segment (size_t min_size, void*& data, size_t& allocated_size)
{
	if (!segments_)
		throw MARSHAL (MARSHAL_MINOR_FEWER);

	const Segment* segment = (const Segment*)round_up (cur_ptr_, alignof (Element));
	if ((cur_block_end () - (const Octet*)segment) < sizeof (Segment))
		segment = (const Segment*)next_block (alignof (Element));
	if (segments_ != segment || segment->allocated_size < min_size)
		throw MARSHAL ();
	segments_ = (Segment*)(segment->next);
	cur_ptr_ = (Octet*)(segment + 1);

	void* p = segment->ptr;
	size_t size = segment->allocated_size;

	Heap& cur_heap = Heap::user_heap ();
	Heap& target_heap = target_memory ();
	if (&cur_heap != &target_heap) {
		try {
			p = cur_heap.move_from (target_heap, p, size);
		} catch (...) {
			target_heap.release (p, size);
			throw;
		}
	}
	allocated_size = size;
	data = p;
}

void RequestLocalBase::marshal_interface (Interface::_ptr_type itf)
{
	if (marshal_op ()) {
		if (itf)
			marshal_interface_internal (itf);
		else {
			Interface* nil = nullptr;
			write (alignof (Interface*), sizeof (Interface*), &nil);
		}
	}
}

void RequestLocalBase::marshal_interface_internal (Interface::_ptr_type itf)
{
	assert (itf);
	const Octet* block_end = cur_block_end ();
	Octet* dst = round_up (cur_ptr_, alignof (Interface*));
	ptrdiff_t tail = block_end - dst;
	if (sizeof (Interface*) <= tail && tail < sizeof (ItfRecord)) {
		// Mark interface non-null
		*(Interface**)dst = &itf;
	}
	ItfRecord* itf_rec = (ItfRecord*)get_element_buffer (sizeof (ItfRecord));
	itf_rec->ptr = interface_duplicate (&itf);
	itf_rec->next = interfaces_;
	interfaces_ = itf_rec;
}

void RequestLocalBase::unmarshal_interface (const IDL::String& interface_id, Interface::_ref_type& itf)
{
	static_assert (alignof (Interface*) == alignof (ItfRecord), "alignof (Interface*) == alignof (ItfRecord)");
	Interface** pitf = (Interface**)round_up (cur_ptr_, alignof (Interface*));
	const Octet* block_end = cur_block_end ();
	if (block_end - (const Octet*)pitf < sizeof (Interface*)) {
		pitf = (Interface**)next_block (alignof (Interface*));
		block_end = cur_block_end ();
	}

	if (!*pitf) {
		cur_ptr_ = (Octet*)(pitf + 1);
		itf = nullptr;
		return;
	}

	if (!interfaces_)
		throw MARSHAL (MARSHAL_MINOR_FEWER);
	ItfRecord* itf_rec;
	if (block_end - (const Octet*)pitf < sizeof (ItfRecord))
		itf_rec = (ItfRecord*)next_block (alignof (ItfRecord));
	else
		itf_rec = (ItfRecord*)pitf;
	if (interfaces_ != itf_rec)
		throw MARSHAL ();

	Interface::_check ((Interface*)itf_rec->ptr, interface_id);
	interfaces_ = (ItfRecord*)(itf_rec->next);
	cur_ptr_ = (Octet*)(itf_rec + 1);
	itf = std::move (reinterpret_cast <Interface::_ref_type&> (itf_rec->ptr));
}

void RequestLocalBase::invoke ()
{
	rewind ();
	state_ = State::CALL;
}

void RequestLocalBase::marshal_value_internal (ValueBase::_ptr_type base)
{
	assert (base);

	auto ins = value_map_.marshal_map ().emplace (&base, (uintptr_t)cur_ptr_);
	if (!ins.second) {
		// This value was already marshalled, write indirection tag.
		write8 (2);
		write (alignof (uintptr_t), sizeof (uintptr_t), &ins.first->second);
	} else {
		auto factory = base->_factory ();
		if (!factory)
			throw MARSHAL (MAKE_OMG_MINOR (1)); // Unable to locate value factory.
		write8 (1);
		marshal_interface_internal (factory);
		base->_marshal (_get_ptr ());
	}
}

void RequestLocalBase::set_exception (Any& e)
{
	if (e.type ()->kind () != TCKind::tk_except)
		throw BAD_PARAM (MAKE_OMG_MINOR (21));

	clear_on_callee_side ();
	marshal_op ();
	state_ = State::EXCEPTION;
	if (response_flags_) {
		try {
			IDL::Type <Any>::marshal_out (e, _get_ptr ());
		} catch (...) {}
		rewind ();
	}
	finalize ();
}

void RequestLocalBase::set_exception (Exception&& e) noexcept
{
	try {
		Any any = exception2any (std::move (e));
		set_exception (any);
	} catch (...) {
		set_unknown_exception ();
	}
}

void RequestLocalBase::set_unknown_exception () noexcept
{
	try {
		set_exception (UNKNOWN ());
	} catch (...) {}
}

}
}
