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
#include "Heap.h"
#include "MemContext.h"
#include "HeapAllocator.h"
#include "HeapDynamic.h"

namespace Nirvana {
namespace Core {

StaticallyAllocated <ImplStatic <Heap> > Heap::core_heap_;
StaticallyAllocated <ImplStatic <Heap> > Heap::shared_heap_;

Heap& Heap::user_heap ()
{
	return MemContext::current ().heap ();
}

class Heap::LBErase
{
public:
	/// \param block_list The block list.
	/// \param first_node First large block node found in the block list.
	/// \param p Begin of the released memory.
	/// \param size Size of the released memory.
	LBErase (Heap& heap, BlockList::NodeVal* first_node);
	~LBErase ();

	void prepare (void* p, size_t size);
	void commit () noexcept;
	void rollback () noexcept;

private:
	void rollback_helper () noexcept;

	struct LBNode
	{
		BlockList::NodeVal* node;
		size_t size;

		bool collapse () noexcept
		{
			return node->value ().collapse_large_block (size);
		}

		void restore () noexcept
		{
			node->value ().restore_large_block (size);
		}

		void restore (size_t new_size) noexcept
		{
			node->value ().restore_large_block (new_size);
		}
	};

	Heap& heap_;
	LBNode first_block_, last_block_;
	typedef std::vector <LBNode, HeapAllocator <LBNode> > MiddleBlocks;
	MiddleBlocks middle_blocks_;
	BlockList::NodeVal* new_node_;
	size_t shrink_size_;
};

inline
bool Heap::MemoryBlock::collapse_large_block (size_t size) noexcept
{
	assert (is_large_block ());
	size |= 1;
	return std::atomic_compare_exchange_strong ((volatile std::atomic <size_t>*) & large_block_size_, &size, 1);
}

inline
void Heap::MemoryBlock::restore_large_block (size_t size) noexcept
{
	assert (large_block_size_ == 1);
	large_block_size_ = size | 1;
}

Heap::LBErase::LBErase (Heap& heap, BlockList::NodeVal* first_node) :
	heap_ (heap),
	middle_blocks_ (HeapAllocator <LBNode> (heap)),
	new_node_ (nullptr)
{
	assert (first_node);
	first_block_.node = first_node;
	last_block_.node = nullptr;
}

Heap::LBErase::~LBErase ()
{
	if (last_block_.node)
		heap_.block_list_.release_node (last_block_.node);
	for (auto& mb : middle_blocks_) {
		heap_.block_list_.release_node (mb.node);
	}
	heap_.block_list_.release_node (first_block_.node);
	if (new_node_)
		heap_.block_list_.release_node (new_node_);
}

void Heap::LBErase::prepare (void* p, size_t size)
{
	MemoryBlock* block = &first_block_.node->value ();
	assert (block->is_large_block ());
	if (!(first_block_.size = block->large_block_size ()))
		throw_BAD_PARAM (); // Block is collapsed in another thread.

	// Collect contiguos blocks.
	const uintptr_t au = Port::Memory::ALLOCATION_UNIT;
	uint8_t* end = round_up ((uint8_t*)p + size, au);
	uint8_t* block_begin = block->begin ();
	assert (block_begin <= p);
	shrink_size_ = round_down ((uint8_t*)p, au) - block_begin; // First block shrink size
	uint8_t* block_end = block_begin + first_block_.size;
	assert (block_end > p);
	while (end > block_end) {
		if (last_block_.node)
			middle_blocks_.push_back (last_block_);
		if (!(last_block_.node = heap_.block_list_.find (block_end)))
			throw_BAD_PARAM ();
		block = &last_block_.node->value ();
		if (!block->is_large_block ())
			throw_BAD_PARAM ();
		if (!(last_block_.size = block->large_block_size ()))
			throw_BAD_PARAM ();
		block_end = block->begin () + last_block_.size;
	}

	// Create new block at end if need.
	if (end < block_end)
		new_node_ = heap_.block_list_.create_node (end, block_end - end);

	// Collapse collected blocks.
	if (last_block_.node && !last_block_.collapse ())
		throw_BAD_PARAM ();

	for (MiddleBlocks::reverse_iterator it = middle_blocks_.rbegin (); it != middle_blocks_.rend (); ++it) {
		if (!it->collapse ()) {
			if (last_block_.node)
				last_block_.restore ();
			while (it > middle_blocks_.rbegin ()) {
				(--it)->restore ();
			}
			throw_BAD_PARAM ();
		}
	}

	try {
		if (!first_block_.collapse ())
			throw_BAD_PARAM ();
	} catch (...) {
		rollback_helper ();
		throw;
	}
}

void Heap::LBErase::commit () noexcept
{
	if (shrink_size_)
		first_block_.restore (shrink_size_);

	if (new_node_)
		heap_.block_list_.insert (new_node_);

	// Cleanup
	if (last_block_.node)
		heap_.block_list_.remove (last_block_.node);

	for (auto& mb : middle_blocks_) {
		heap_.block_list_.remove (mb.node);
	}

	if (!shrink_size_)
		heap_.block_list_.remove (first_block_.node);
}

void Heap::LBErase::rollback () noexcept
{
	rollback_helper ();
	first_block_.restore ();
	if (last_block_.node)
		last_block_.restore ();
}

void Heap::LBErase::rollback_helper () noexcept
{
	for (MiddleBlocks::iterator it = middle_blocks_.begin (); it != middle_blocks_.end (); ++it) {
		it->restore ();
	}
}

Heap::Heap (size_t allocation_unit, bool system) noexcept :
	part_list_ (nullptr),
	block_list_ (system ? *this : shared_heap ()),
	allocation_unit_ (allocation_unit),
	dont_decommit_ (system)
{
	if (allocation_unit <= HEAP_UNIT_MIN)
		allocation_unit_ = HEAP_UNIT_MIN;
	else if (allocation_unit >= HEAP_UNIT_MAX)
		allocation_unit_ = HEAP_UNIT_MAX;
	else
		allocation_unit_ = clp2 (allocation_unit);
}

void Heap::release (void* p, size_t size)
{
	if (!p || !size)
		return;

	// Search memory block
	BlockList::NodeVal* node = block_list_.lower_bound (p);
	if (!node)
		throw_FREE_MEM ();
	const MemoryBlock& block = node->value ();
	if (!block.is_large_block ()) {
		// Release from heap partition
		uint8_t* part_end = block.begin () + partition_size ();
		block_list_.release_node (node);
		if ((uint8_t*)p + size <= part_end)
			release (block.directory (), p, size);
		else
			throw_FREE_MEM ();
	} else {
		// Release large block
		LBErase lberase (*this, node);
		lberase.prepare (p, size);
		try {
			Port::Memory::release (p, size);
		} catch (...) {
			lberase.rollback ();
			throw;
		}
		lberase.commit ();
	}
}

void* Heap::move_from (Heap& other, void* p, size_t& size)
{
	if (!p || !size)
		return nullptr;

	// Search memory block
	BlockList::NodeVal* node = other.block_list_.lower_bound (p);
	if (!node)
		throw_BAD_PARAM ();
	const MemoryBlock& block = node->value ();
	if (!block.is_large_block ()) {
		other.block_list_.release_node (node);
		uint8_t* part_end = block.begin () + other.partition_size ();
		size_t old_size = size;
		if ((uint8_t*)p + old_size > part_end)
			throw_BAD_PARAM (); // p does not belong to the source heap.
		// Copy
		void* pc = copy (nullptr, p, size, 0);
		// Release from other heap partition
		other.release (block.directory (), p, old_size);
		p = pc;
	} else {
		// Adopt large block
		const uintptr_t au = Port::Memory::ALLOCATION_UNIT;
		uint8_t* end = round_up ((uint8_t*)p + size, au);
		size = end - (uint8_t*)p;
		LBErase lberase (other, node);
		lberase.prepare (p, size);
		try {
			add_large_block (p, size);
		} catch (...) {
			lberase.rollback ();
			throw;
		}
		lberase.commit ();
	}
	return p;
}

void Heap::release (Directory& part, void* p, size_t size) const
{
	uint8_t* heap = (uint8_t*)(&part + 1);
	size_t offset = (uint8_t*)p - heap;
	size_t begin = offset / allocation_unit_;
	size_t end = (offset + size + allocation_unit_ - 1) / allocation_unit_;
	if (!part.check_allocated (begin, end))
		throw_FREE_MEM ();
	if (Directory::IMPLEMENTATION != HeapDirectoryImpl::PLAIN_MEMORY && !dont_decommit_) {
		HeapInfo hi = { heap, allocation_unit_, Port::Memory::OPTIMAL_COMMIT_UNIT };
		part.release (begin, end, &hi);
	} else
		part.release (begin, end, nullptr);
}

Heap::Directory* Heap::get_partition (const void* p) noexcept
{
	assert (p);
	Directory* part = nullptr;
	BlockList::NodeVal* node = block_list_.lower_bound (p);
	if (node) {
		const MemoryBlock& block = node->value ();
		if (block.is_large_block ())
			block_list_.release_node (node);
		else {
			block_list_.release_node (node);
			if (p < block.begin () + partition_size ())
				part = &block.directory ();
		}
	}
	return part;
}

void* Heap::allocate (void* p, size_t& size, unsigned flags)
{
	if (!size)
		throw_BAD_PARAM ();

	if ((flags & ~(Memory::RESERVED | Memory::READ_ONLY | Memory::EXACTLY | Memory::ZERO_INIT))
		|| ((flags & Memory::READ_ONLY) && !(flags & Memory::RESERVED))
	)
		throw_INV_FLAG ();

	if (p) {
		// Explicit address allocation
		Directory* dir = get_partition (p);
		if (dir) {
			if ((p = allocate (*dir, p, size, flags)) || (flags & Memory::EXACTLY))
				return p;
		} else {
			if ((p = Port::Memory::allocate (p, size, flags | Memory::EXACTLY)))
				add_large_block (p, size);
			else if (flags & Memory::EXACTLY)
				return nullptr;
		}
	}

	if (!p)
		p = allocate (size, flags);

	return p;
}

void* Heap::allocate (size_t& size, unsigned flags)
{
	void* p;
	const size_t max_block_size = Directory::MAX_BLOCK_SIZE * allocation_unit_;
	if (size > max_block_size
		||
		(max_block_size >= Port::Memory::ALLOCATION_UNIT && !(size % Port::Memory::ALLOCATION_UNIT))
		||
		((flags & Memory::RESERVED) && size >= Port::Memory::OPTIMAL_COMMIT_UNIT)
		) {
		if (!(p = Port::Memory::allocate (nullptr, size, flags))) {
			assert (flags & Memory::EXACTLY);
			return nullptr;
		}
		add_large_block (p, size);
	} else {
		try {
			AtomicBlockPtr* link = &part_list_;
			MemoryBlock* part = link->load (std::memory_order_acquire);
			for (;;) {
				if (part) {
					void* p = allocate (part->directory (), size, flags);
					if (p)
						return p;
					link = &part->next_partition ();
					part = link->load (std::memory_order_acquire);
				} else
					part = add_new_partition (*link);
			}
		} catch (const CORBA::NO_MEMORY&) {
			if (flags & Memory::EXACTLY)
				return nullptr;
			throw;
		}
	}
	return p;
}

void Heap::add_large_block (void* p, size_t size)
{
	try {
		const uintptr_t au = Port::Memory::ALLOCATION_UNIT;
		uint8_t* begin = round_down ((uint8_t*)p, au);
		uint8_t* end = round_up ((uint8_t*)p + size, au);
		block_list_.insert (begin, end - begin);
	} catch (...) {
		Port::Memory::release (p, size);
		throw;
	}
}

void* Heap::allocate (Directory& part, size_t& size, unsigned flags) const noexcept
{
	size_t units = (size + allocation_unit_ - 1) / allocation_unit_;
	uint8_t* heap = (uint8_t*)(&part + 1);
	ptrdiff_t unit;
	if (Directory::IMPLEMENTATION != HeapDirectoryImpl::PLAIN_MEMORY && !(flags & Memory::RESERVED)) {
		HeapInfo hi = { heap, allocation_unit_, Port::Memory::OPTIMAL_COMMIT_UNIT };
		unit = part.allocate (units, &hi);
	} else
		unit = part.allocate (units, nullptr);
	if (unit >= 0) {
		assert (unit < Directory::UNIT_COUNT);
		void* p = heap + unit * allocation_unit_;
		if (flags & Memory::ZERO_INIT)
			zero ((size_t*)p, (size_t*)p + (size + sizeof (size_t) - 1) / sizeof (size_t));
		size = units * allocation_unit_;
		return p;
	}
	return nullptr;
}

void* Heap::allocate (Directory& part, void* p, size_t& size, unsigned flags) const noexcept
{
	assert (size);
	uint8_t* heap = (uint8_t*)(&part + 1);
	size_t offset = (uint8_t*)p - heap;
	size_t begin = offset / allocation_unit_;
	size_t end;

	if (flags & Memory::EXACTLY)
		end = (offset + size + allocation_unit_ - 1) / allocation_unit_;
	else
		end = begin + (size + allocation_unit_ - 1) / allocation_unit_;

	bool success;
	if (Directory::IMPLEMENTATION != HeapDirectoryImpl::PLAIN_MEMORY && !(flags & Memory::RESERVED)) {
		HeapInfo hi = { heap, allocation_unit_, Port::Memory::OPTIMAL_COMMIT_UNIT };
		success = part.allocate (begin, end, &hi);
	} else
		success = part.allocate (begin, end, nullptr);
	if (success) {
		uint8_t* pbegin = heap + begin * allocation_unit_;
		p = pbegin;

		if (flags & Memory::ZERO_INIT)
			zero ((size_t*)pbegin, (size_t*)(heap + end * allocation_unit_));

		size = (end - begin) * allocation_unit_;
		return p;
	}
	return nullptr;
}

inline Heap::MemoryBlock* Heap::add_new_partition (AtomicBlockPtr& tail)
{
	// Avoid recursion.
	// The block list node is allocated from the new partition.
	Directory& dir = create_partition ();
	BlockList::NodeVal* node = block_list_.insert_partition (*this, dir);
	MemoryBlock* part = &node->value ();
	MemoryBlock* next = nullptr;
	MemoryBlock* ret = part;
	if (!tail.compare_exchange_strong (next, part)) {
		// New partition was already added in another thread.
		if (ret == part)
			ret = next; // Return the first new partition added.
		block_list_.remove (node);
		block_list_.release_partition (*this, node);
		release_partition (dir);
	} else
		block_list_.release_node (node);
	return ret;
}

inline
Heap::Directory& Heap::create_partition () const
{
	size_t cb = sizeof (Directory) + partition_size (allocation_unit_);
	void* p = Port::Memory::allocate (nullptr, cb, Memory::RESERVED);
	try {
		return *new (p) Directory ();
	} catch (...) {
		Port::Memory::release (p, cb);
		throw;
	}
}

void Heap::release_partition (Directory& dir) const noexcept
{
	Port::Memory::release (&dir, sizeof (Directory) + partition_size (allocation_unit_));
}

void* Heap::copy (void* dst, void* src, size_t& size, unsigned flags)
{
	if (!size)
		return dst;

	if (!src)
		throw_BAD_PARAM ();

	if (flags != Memory::SIMPLE_COPY
		&& (flags & ~(Memory::READ_ONLY | Memory::SRC_RELEASE | Memory::DST_ALLOCATE | Memory::EXACTLY))
		)
		throw_INV_FLAG ();

	if (dst == src) {
		// Special case: change protection
		if (check_owner (dst, size))
			return Port::Memory::copy (dst, src, size, flags);
		else if (flags & Memory::READ_ONLY)
			throw_NO_PERMISSION ();
		return dst;
	}

	size_t cb_copy = size;
	unsigned release_flags = flags & Memory::SRC_RELEASE;
	if (release_flags == Memory::SRC_RELEASE) {
		BlockList::NodeVal* src_node = block_list_.lower_bound (src);
		if (!src_node)
			throw_BAD_PARAM ();
		const MemoryBlock& src_block = src_node->value ();
		if (src_block.is_large_block ()) {
			LBErase lberase (*this, src_node);
			lberase.prepare (src, size);
			if (!dst || (flags & Memory::DST_ALLOCATE)) {
				// New block allocation
				BlockList::NodeVal* new_node = nullptr;
				try {
					new_node = block_list_.create_node ();
					dst = Port::Memory::copy (dst, src, size, flags);
					if (dst)
						new_node->value () = MemoryBlock (dst, size);
				} catch (...) {
					if (new_node)
						block_list_.release_node (new_node);
					lberase.rollback ();
					throw;
				}
				if (dst) {
					lberase.commit ();
					block_list_.insert (new_node);
				} else {
					lberase.rollback ();
					block_list_.release_node (new_node);
				}
			} else {
				// No new block allocation
				try {
#ifndef NDEBUG
					void* dbg_dst = dst;
#endif
					dst = Port::Memory::copy (dst, src, size, flags);
					assert (dst == dbg_dst);
				} catch (...) {
					lberase.rollback ();
					throw;
				}
				lberase.commit ();
			}
		} else {
			// Release from the heap partition
			uint8_t* heap = src_block.begin ();
			if ((uint8_t*)src + size > heap + partition_size ())
				throw_FREE_MEM ();
			size_t offset = (uint8_t*)src - heap;
			size_t begin = offset / allocation_unit_;
			size_t end = (offset + size + allocation_unit_ - 1) / allocation_unit_;
			Directory* part = (Directory*)heap - 1;
			if (!part->check_allocated (begin, end))
				throw_FREE_MEM ();

			if (!dst) {
				// Memory::SRC_RELEASE is specified, so we can use source block as destination
				dst = Port::Memory::copy (src, src, size, (flags & ~Memory::SRC_RELEASE) | Memory::SRC_DECOMMIT);
			} else {
				uint8_t* rel_begin = round_down ((uint8_t*)src, allocation_unit_);
				uint8_t* rel_end = round_up ((uint8_t*)src + size, allocation_unit_);
				uint8_t* alloc_begin = nullptr;
				uint8_t* alloc_end = nullptr;
				bool dst_own = true;
				if (flags & Memory::DST_ALLOCATE) {
					alloc_begin = (uint8_t*)dst;
					alloc_end = alloc_begin + size;
					if (alloc_begin < (uint8_t*)src) {
						if (alloc_end > rel_begin) {
							uint8_t* tmp = rel_begin;
							rel_begin = round_up (alloc_end, allocation_unit_);
							alloc_end = tmp;
						}
					} else if (alloc_begin < rel_end) {
						uint8_t* tmp = rel_end;
						rel_end = round_down (alloc_begin, allocation_unit_);
						alloc_begin = tmp;
					}

					size_t cb = alloc_end - alloc_begin;
					if (!allocate (alloc_begin, cb, (flags & ~Memory::READ_ONLY) | Memory::RESERVED | Memory::EXACTLY)) {
						if (flags & Memory::EXACTLY)
							return nullptr;
						dst = allocate (size, (flags & ~Memory::READ_ONLY) | Memory::RESERVED);
						alloc_begin = (uint8_t*)dst;
						alloc_end = alloc_begin + size;
						rel_begin = (uint8_t*)src;
						rel_end = rel_begin + size;
					}
				} else {
					dst_own = check_owner (dst, size);
					if (!dst_own && (flags & Memory::READ_ONLY))
						throw_NO_PERMISSION ();
				}

				try {
					dst = Port::Memory::copy (dst, src, cb_copy, (flags & ~Memory::SRC_RELEASE) | Memory::SRC_DECOMMIT);
				} catch (...) {
					release (alloc_begin, alloc_end - alloc_begin);
					throw;
				}

				// Release memory
				offset = rel_begin - heap;
				begin = offset / allocation_unit_;
				end = (rel_end - heap + allocation_unit_ - 1) / allocation_unit_;
				part->release (begin, end, nullptr); // Source memory was already decommitted so we don't pass HeapInfo.
			}
		}

		return dst;

	} else if (release_flags) {
		assert (release_flags == Memory::SRC_DECOMMIT);
		if (!check_owner (src, size))
			throw_NO_PERMISSION ();
	}

	uint8_t* alloc_begin = nullptr;
	uint8_t* alloc_end = nullptr;
	if (!dst) {
		if (flags & Memory::SIMPLE_COPY)
			throw_INV_FLAG ();

		if (!(dst = allocate (size, (flags & ~(Memory::READ_ONLY | Memory::DST_ALLOCATE)) | Memory::RESERVED))) {
			assert (flags & Memory::EXACTLY);
			return nullptr;
		}
		alloc_begin = (uint8_t*)dst;
		alloc_end = alloc_begin + size;
	} else if (flags & Memory::DST_ALLOCATE) {
		uintptr_t au = query (dst, Memory::QueryParam::ALLOCATION_UNIT);
		alloc_begin = round_down ((uint8_t*)dst, au);
		alloc_end = round_up ((uint8_t*)dst + size, au);
		if (alloc_begin < (uint8_t*)src) {
			if (alloc_end > (uint8_t*)src)
				alloc_end = round_down ((uint8_t*)src, au);
		} else {
			uint8_t* src_end = (uint8_t*)src + size;
			if (alloc_begin < src_end)
				alloc_begin = round_up (src_end, au);
		}

		size_t cb = alloc_end - alloc_begin;
		if (!allocate (alloc_begin, cb, (flags & ~Memory::READ_ONLY) | Memory::RESERVED | Memory::EXACTLY)) {
			if (flags & Memory::EXACTLY)
				return nullptr;
			dst = allocate (size, flags | Memory::RESERVED);
			alloc_begin = (uint8_t*)dst;
			alloc_end = alloc_begin + size;
		}
	} else if (flags != Memory::SIMPLE_COPY && !check_owner (dst, size)) {
		if (flags & Memory::READ_ONLY)
			throw_BAD_PARAM ();
		else
			flags = Memory::SIMPLE_COPY;
	}

	if (flags == Memory::SIMPLE_COPY && (
		!Port::Memory::SHARING_UNIT || cb_copy < Port::Memory::SHARING_UNIT
		||
		(uintptr_t)dst % Port::Memory::SHARING_ASSOCIATIVITY
		!= (uintptr_t)src % Port::Memory::SHARING_ASSOCIATIVITY
		)
	)
		real_move (src, (const void*)((uint8_t*)src + cb_copy), dst);
	else {
		try {
			dst = Port::Memory::copy (dst, src, cb_copy, flags & ~Memory::DST_ALLOCATE);
		} catch (...) {
			Port::Memory::release (alloc_begin, alloc_end - alloc_begin);
			throw;
		}
	}

	return dst;
}

uintptr_t Heap::query (const void* p, Memory::QueryParam param)
{
	if (Memory::QueryParam::ALLOCATION_UNIT == param) {
		if (!p || get_partition (p))
			return allocation_unit_;
	} else if (p && (param == Memory::QueryParam::ALLOCATION_SPACE_BEGIN
		|| param == Memory::QueryParam::ALLOCATION_SPACE_END)) {

		const Directory* part = get_partition (p);
		if (part) {
			if (param == Memory::QueryParam::ALLOCATION_SPACE_BEGIN)
				return (uintptr_t)(part + 1);
			else
				return (uintptr_t)(part + 1 + partition_size ());
		}
	} else if (Memory::QueryParam::MEMORY_STATE == param) {
		if (!check_owner (p, allocation_unit_))
			return (uintptr_t)Memory::MemoryState::MEM_NOT_ALLOCATED;
	}
	return Port::Memory::query (p, param);
}

bool Heap::check_owner (const void* p, size_t size)
{
	bool ok = false;
	BlockList::NodeVal* node = block_list_.lower_bound (p);
	if (node) {
		const MemoryBlock* block = &node->value ();
		if (block->is_large_block ()) {
			uint8_t* block_end = block->begin () + block->large_block_size ();
			if ((uint8_t*)p < block_end) {
				uint8_t* end = (uint8_t*)p + size;
				for (;;) {
					if (end <= block_end) {
						ok = true;
						break;
					}
					block_list_.release_node (node);
					node = block_list_.find (block_end);
					if (!node)
						break;
					block = &node->value ();
					if (!block->is_large_block ())
						break;
					size_t block_size = block->large_block_size ();
					if (!block_size)
						break;
					block_end = block->begin () + block_size;
				}
			}
			if (node)
				block_list_.release_node (node);
		} else {
			uint8_t* heap = block->begin ();
			if ((uint8_t*)p + size <= heap + partition_size ()) {
				size_t offset = (uint8_t*)p - heap;
				size_t begin = offset / allocation_unit_;
				size_t end = (offset + size + allocation_unit_ - 1) / allocation_unit_;
				Directory* part = (Directory*)heap - 1;
				ok = part->check_allocated (begin, end);
			}
			block_list_.release_node (node);
		}
	}
	return ok;
}

void Heap::change_protection (bool read_only)
{
	assert (&block_list_.heap () != this);

	unsigned short protection = read_only ? (Memory::READ_ONLY | Memory::EXACTLY) : (Memory::READ_WRITE | Memory::EXACTLY);
	
	for (BlockList::NodeVal* p = block_list_.get_min_node (); p; p = block_list_.next (p)) {
		const MemoryBlock& mb = p->value ();
		uint8_t* begin = mb.begin ();
		uint8_t* end = begin + (mb.is_large_block () ? mb.large_block_size () : partition_size ());
		size_t commit_unit = Port::Memory::query (begin, Memory::QueryParam::COMMIT_UNIT);

		do {
			if (Port::Memory::query (begin, Memory::QueryParam::MEMORY_STATE) < (uintptr_t)Memory::MemoryState::MEM_READ_ONLY)
				begin += commit_unit;
			else {
				uint8_t* commit_end = begin + commit_unit;
				while (commit_end < end) {
					if (Port::Memory::query (commit_end, Memory::QueryParam::MEMORY_STATE) < (uintptr_t)Memory::MemoryState::MEM_READ_ONLY)
						break;
					commit_end += commit_unit;
				}
				size_t cb = commit_end - begin;
				Port::Memory::copy (begin, begin, cb, protection);
				begin = commit_end;
			}
		} while (begin < end);
	}
}

bool Heap::cleanup (bool final) noexcept
{
	bool empty = true;

	// Clear large blocks
	for (BlockList::NodeVal* node = block_list_.get_min_node (); node;) {
		const MemoryBlock& block = node->value ();
		if (block.is_large_block ()) {
			empty = false;
			Port::Memory::release (block.begin (), block.large_block_size ());
			BlockList::copy_node (node);
			BlockList::NodeVal* next = block_list_.next (node);
			block_list_.remove (node);
			block_list_.release_node (node);
			node = next;
		} else
			node = block_list_.next (node);
	}

	// Clear partitions. Keep one if not final.
	Directory* keep_dir = nullptr;

	while (BlockList::NodeVal* node = block_list_.delete_min ()) {
		const MemoryBlock block = node->value ();
		block_list_.release_partition (*this, node);
		assert (!block.is_large_block ());
		Directory& dir = block.directory ();
		if (!keep_dir && !final)
			keep_dir = &dir;
		else {
			if (!dir.empty ())
				empty = false;
			release_partition (dir);
		}
	}

	part_list_ = nullptr;
	assert (block_list_.empty ());

	if (keep_dir) {
		assert (!final);

		if (!keep_dir->empty ()) {
			empty = false;
			keep_dir->reset ();
			if (!dont_decommit_)
				Port::Memory::decommit ((keep_dir + 1), partition_size ());
		}

		BlockList::NodeVal* node = block_list_.insert_partition (*this, *keep_dir);
		part_list_ = &node->value ();
		block_list_.release_node (node);
	}

	return empty;
}

Heap::BlockList::NodeVal* Heap::BlockList::insert_partition (Heap& heap, Directory& part) noexcept
{
	std::pair <NodeVal*, bool> ins;
	if (&Base::heap () == &heap) {
		unsigned level = Base::random_level ();
		size_t cb = Base::node_size (level);
		ins = Base::insert (new (heap.allocate (part, cb, 0))
			NodeVal (level, std::ref (part)));
		assert (ins.second);
#ifndef NDEBUG
		node_cnt_.increment ();
#endif
	} else
		ins = Base::insert (std::ref (part));
	return ins.first;
}

void Heap::BlockList::release_partition (Heap& heap, NodeVal* node) noexcept
{
	assert (node->deleted);
	if (&Base::heap () == &heap) {
		Directory& part = node->value ().directory ();
		heap.release (part, node, Base::node_size (node->level));
#ifndef NDEBUG
		node_cnt_.decrement ();
#endif
	} else
		Base::release_node (node);
}

void Heap::initialize () noexcept
{
	if (!Port::Memory::initialize ())
		unrecoverable_error (0);
	core_heap_.construct (HEAP_UNIT_CORE, true);
	if (sizeof (void*) > 2)
		shared_heap_.construct (HEAP_UNIT_DEFAULT, true);
	HeapDynamic::initialize ();
}

void Heap::terminate () noexcept
{
	HeapDynamic::terminate ();
	if (sizeof (void*) > 2)
		shared_heap_.destruct ();
	core_heap_.destruct ();
	Port::Memory::terminate ();
}

}
}
