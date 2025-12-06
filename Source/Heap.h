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
#ifndef NIRVANA_CORE_HEAP_H_
#define NIRVANA_CORE_HEAP_H_
#pragma once

#include "HeapDirectory.h"
#include "SkipList.h"
#include "CoreInterface.h"
#include "StaticallyAllocated.h"
#include <atomic>
#include <Port/config.h>

#define DEFINE_ALLOCATOR(Name) template <class U> operator const Name <U>& () const \
noexcept { return *reinterpret_cast <const Name <U>*> (this); }\
template <class U> struct rebind { typedef Name <U> other; }

namespace Nirvana {
namespace Core {

/// \brief Heap implementation.
///
/// The Heap is lock-free object and can be shared a number of threads.
class Heap
{
	Heap (const Heap&) = delete;
	Heap& operator = (const Heap&) = delete;

	DECLARE_CORE_INTERFACE

public:
	/// See Nirvana::Memory::allocate
	void* allocate (void* p, size_t& size, unsigned flags);

	/// See Nirvana::Memory::release
	void release (void* p, size_t size);

	/// See Nirvana::Memory::commit
	void commit (void* p, size_t size)
	{
		if (!check_owner (p, size))
			throw_BAD_PARAM ();
		Port::Memory::commit (p, size);
	}

	/// See Nirvana::Memory::decommit
	void decommit (void* p, size_t size)
	{
		if (!check_owner (p, size))
			throw_BAD_PARAM ();
		Port::Memory::decommit (p, size);
	}

	/// See Nirvana::Memory::copy
	void* copy (void* dst, void* src, size_t& size, unsigned flags);

	/// See Nirvana::Memory::is_private
	static bool is_private (const void* p, size_t size)
	{
		return Port::Memory::is_private (p, size);
	}

	/// See Nirvana::Memory::query
	uintptr_t query (const void* p, Memory::QueryParam param);

	/// Changes the entire heap protection.
	/// 
	/// \param read_only If `true`, makes this heap read-only.
	///   Otherwise, makes this heap read-write.
	void change_protection (bool read_only);

	/// Checks that memory block is owned by this heap.
	/// 
	/// \param p The memory block pointer.
	/// \param size The memory block size.
	/// \returns `true` if entire memory block is owned by this heap.
	bool check_owner (const void* p, size_t size);

	/// Moves memory block from other heap to this heap.
	/// 
	/// \param other Other heap.
	/// \param p Memory block in \p other.
	/// \param size Memory block size.
	/// \returns Pointer of the memory block in this heap.
	void* move_from (Heap& other, void* p, size_t& size);

	/// Add large block
	///
	/// \param p Memory block in \p other.
	/// \param size Memory block size.
	/// 
	/// On exception, memory block will be released, then exception rethrown.
	void add_large_block (void* p, size_t size);

	/// \brief Releases all memory.
	/// 
	/// \param final If `false`, keep the first partition.
	/// \returns `true` if no memory leaks.
	bool cleanup (bool final) noexcept;

	/* Unused. Kept just for case.
	Heap& operator = (Heap&& other) noexcept
	{
		cleanup (true);
		allocation_unit_ = other.allocation_unit_;
		part_list_ = other.part_list_;
		other.part_list_ = nullptr;
		block_list_ = std::move (other.block_list_);
		return *this;
	}
	*/

	/// Returns core heap.
	static Heap& core_heap () noexcept;

	/// Returns shared heap.
	static Heap& shared_heap () noexcept;

	/// Returns current user memory context heap.
	/// If no current memory context present, a new memory context will be created.
	static Heap& user_heap ();

	/// Global class initialization.
	static void initialize () noexcept;
	
	/// Global class temination.
	static void terminate () noexcept;

protected:
	Heap (size_t allocation_unit = HEAP_UNIT_DEFAULT, bool system = false) noexcept;
	
	~Heap ()
	{
		cleanup (true);
	}

	typedef HeapDirectory <HEAP_DIRECTORY_SIZE, HEAP_DIRECTORY_LEVELS,
		(Port::Memory::FLAGS & Memory::SPACE_RESERVATION) ?
		HeapDirectoryImpl::COMMITTED_BITMAP : HeapDirectoryImpl::PLAIN_MEMORY> Directory;

	class MemoryBlock;
	using AtomicBlockPtr = std::atomic <MemoryBlock*>;
	
	// MemoryBlock represents the heap partition or large memory block allocated from the main memory service.
	class MemoryBlock
	{
	public:
		MemoryBlock () noexcept :
			begin_ (nullptr),
			next_partition_ (nullptr)
		{}

		MemoryBlock (const void* p) noexcept :
			begin_ ((uint8_t*)p),
			next_partition_ (nullptr)
		{}

		MemoryBlock (void* p, size_t size) noexcept :
			begin_ ((uint8_t*)p),
			large_block_size_ (size | 1)
		{
			assert (size);
			assert (!(size & 1));
		}

		MemoryBlock (Directory& part) noexcept :
			begin_ ((uint8_t*)(&part + 1)),
			next_partition_ (nullptr)
		{}

		uint8_t* begin () const noexcept
		{
			return begin_;
		}

		// Use lowest bit as large block tag.
		size_t is_large_block () const noexcept
		{
			return large_block_size_ & 1;
		}

		size_t large_block_size () const noexcept
		{
			assert (is_large_block ());
			return large_block_size_ & ~1;
		}

		Directory& directory () const noexcept
		{
			assert (!is_large_block ());
			return *(Directory*)(begin_ - sizeof (Directory));
		}

		AtomicBlockPtr& next_partition () noexcept
		{
			assert (!is_large_block ());
			return (AtomicBlockPtr&)next_partition_;
		}

		// Blocks are sorted in descending order.
		// So we can use lower_bound to search block begin.
		bool operator < (const MemoryBlock& rhs) const noexcept
		{
			if (begin_ > rhs.begin_)
				return true;
			else if (begin_ < rhs.begin_)
				return false;
			// Hide collapsed blocks
			else if (large_block_size_ == 1)
				return false;
			else if (rhs.large_block_size_ == 1)
				return true;
			else
				return false;
		}

		bool collapse_large_block (size_t size) noexcept;
		void restore_large_block (size_t size) noexcept;

	private:
		// Pointer to the memory block.
		// For the heap partition it is pointer to memory space immediately after heap directory.
		uint8_t* begin_;

		union alignas (AtomicBlockPtr)
		{
			MemoryBlock* next_partition_;
			volatile size_t large_block_size_;
		};
	};

	class BlockList : public SkipList <MemoryBlock, SKIP_LIST_DEFAULT_LEVELS>
	{
		typedef SkipList <MemoryBlock, SKIP_LIST_DEFAULT_LEVELS> Base;

	public:
		BlockList (Heap& heap) :
			Base (heap)
		{}

		void insert (NodeVal*& node)
		{
			auto ins = Base::insert (node);
			assert (ins.second);
			release_node (ins.first);
			node = nullptr;
		}

		void insert (void* p, size_t size)
		{
			auto ins = Base::insert (p, size);
			assert (ins.second);
			release_node (ins.first);
		}

		NodeVal* insert_partition (Heap& heap, Directory& part) noexcept;
		void release_partition (Heap& heap, NodeVal* node) noexcept;
	};

	static size_t partition_size (size_t allocation_unit)
	{
		return Directory::UNIT_COUNT * allocation_unit;
	}

	size_t partition_size () const
	{
		return partition_size (allocation_unit_);
	}

	Directory& create_partition () const;
	void release_partition (Directory& dir) const noexcept;

	Directory* get_partition (const void* p) noexcept;

	void* allocate (Directory& part, size_t& size, unsigned flags) const noexcept;
	void* allocate (Directory& part, void* p, size_t& size, unsigned flags) const noexcept;

	void* allocate (size_t& size, unsigned flags);

	void release (Directory& part, void* p, size_t size) const;

	MemoryBlock* add_new_partition (AtomicBlockPtr& tail);

	/// \summary Atomically erase large block information from the block list.
	class LBErase;

private:
	AtomicBlockPtr part_list_;
	BlockList block_list_;
	size_t allocation_unit_;
	bool dont_decommit_;

private:
	static StaticallyAllocated <ImplStatic <Heap> > core_heap_;
	static StaticallyAllocated <ImplStatic <Heap> > shared_heap_;
};

/// Object allocated from the core heap.
///
/// Rarely used. Use SharedObject instead.
/// The core heap is relatively small and therefore fast.
/// Use CoreObject only when an object should be created extrememly quickly.
class CoreObject
{
public:
	void* operator new (size_t cb)
	{
		return Heap::core_heap ().allocate (nullptr, cb, 0);
	}

	void operator delete (void* p, size_t cb)
	{
		Heap::core_heap ().release (p, cb);
	}

	void* operator new (size_t cb, void* place)
	{
		return place;
	}

	void operator delete (void*, void*)
	{}
};

/// \brief Object allocated from the shared memory context.
class SharedObject
{
public:
	void* operator new (size_t cb)
	{
		return Heap::shared_heap ().allocate (nullptr, cb, 0);
	}

	void operator delete (void* p, size_t cb)
	{
		Heap::shared_heap ().release (p, cb);
	}

	void* operator new (size_t cb, void* place)
	{
		return place;
	}

	void operator delete (void*, void*)
	{}
};

inline Heap& Heap::core_heap () noexcept
{
	return core_heap_;
}

inline Heap& Heap::shared_heap () noexcept
{
	if (sizeof (void*) > 2)
		return shared_heap_;
	else
		return core_heap_;
}

}
}

#endif
