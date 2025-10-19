/*
* Nirvana Core test.
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
#include "../Source/HeapDirectory.h"
#include <Port/Memory.h>
#include <gtest/gtest.h>
#include <random>
#include <atomic>
#include <vector>
#include <set>
#include <Mock/Thread.h>

using namespace Nirvana;
using namespace Nirvana::Core;

namespace TestHeapDirectory {

using thread = Nirvana::Mock::Thread;

template <size_t SIZE, size_t LEVELS, HeapDirectoryImpl IMPL>
class HeapDirectoryFactory
{
public:
	typedef HeapDirectory <SIZE, LEVELS, IMPL> DirectoryType;

	static DirectoryType* create ()
	{
		size_t cb = sizeof (DirectoryType);
		return new (Port::Memory::allocate (nullptr, cb, 
				IMPL > HeapDirectoryImpl::COMMITTED_BITMAP ? Memory::RESERVED : Memory::ZERO_INIT)
			) DirectoryType ();
	}

	static void destroy (DirectoryType *p)
	{
		Port::Memory::release (p, sizeof (DirectoryType));
	}
};

template <class Factory>
class TestHeapDirectory :
	public ::testing::Test
{
protected:
	typedef typename Factory::DirectoryType DirectoryType;

	TestHeapDirectory () :
		directory_ (0)
	{
	}

	virtual ~TestHeapDirectory ()
	{
	}

	// If the constructor and destructor are not enough for setting up
	// and cleaning up each test, you can define the following methods:

	virtual void SetUp ()
	{
		ASSERT_TRUE (Port::Memory::initialize ());
		// Code here will be called immediately after the constructor (right
		// before each test).
		directory_ = Factory::create ();
		ASSERT_TRUE (directory_);
	}

	virtual void TearDown ()
	{
		// Code here will be called immediately after each test (right
		// before the destructor).
		Factory::destroy (directory_);
		Port::Memory::terminate ();
	}

protected:
	DirectoryType* directory_;
};

typedef ::testing::Types <
	HeapDirectoryFactory <0x10000, 11, HeapDirectoryImpl::COMMITTED_BITMAP>
, HeapDirectoryFactory <0x8000, 10, HeapDirectoryImpl::COMMITTED_BITMAP>
, HeapDirectoryFactory <0x8000, 11, HeapDirectoryImpl::COMMITTED_BITMAP>
, HeapDirectoryFactory <0x4000, 9, HeapDirectoryImpl::COMMITTED_BITMAP>
, HeapDirectoryFactory <0x4000, 11, HeapDirectoryImpl::COMMITTED_BITMAP>
> MyTypes;

TYPED_TEST_SUITE (TestHeapDirectory, MyTypes);

TYPED_TEST (TestHeapDirectory, Allocate)
{
	EXPECT_TRUE (this->directory_->empty ());

	for (int pass = 0; pass < 2; ++pass) {
		for (size_t block_size = TypeParam::DirectoryType::MAX_BLOCK_SIZE; block_size > 0; block_size >>= 1) {
			size_t blocks_cnt = TypeParam::DirectoryType::UNIT_COUNT / block_size;
			for (size_t i = 0; i < blocks_cnt; ++i) {
				EXPECT_FALSE (this->directory_->check_allocated (i * block_size, TypeParam::DirectoryType::UNIT_COUNT));
				size_t block = this->directory_->allocate (block_size);
				ASSERT_EQ (block, i * block_size);
				EXPECT_TRUE (this->directory_->check_allocated (block, block + block_size));
			}
			ASSERT_TRUE (this->directory_->check_allocated (0, TypeParam::DirectoryType::UNIT_COUNT));
			this->directory_->release (0, TypeParam::DirectoryType::UNIT_COUNT);
		}

		EXPECT_TRUE (this->directory_->empty ());
	}
}

TYPED_TEST (TestHeapDirectory, Release)
{
	EXPECT_TRUE (this->directory_->empty ());

	for (int pass = 0; pass < 2; ++pass) {
		for (size_t block_size = TypeParam::DirectoryType::MAX_BLOCK_SIZE; block_size >= 4; block_size >>= 1) {
			size_t blocks_cnt = TypeParam::DirectoryType::UNIT_COUNT / block_size;
			for (size_t i = 0; i < blocks_cnt; ++i) {

				// Allocate and release block.
				EXPECT_FALSE (this->directory_->check_allocated (i * block_size, TypeParam::DirectoryType::UNIT_COUNT));
				size_t block = this->directory_->allocate (block_size);
				ASSERT_EQ (block, i * block_size);
				EXPECT_TRUE (this->directory_->check_allocated (block, block + block_size));
				this->directory_->release (block, block + block_size);
				EXPECT_FALSE (this->directory_->check_allocated (block, block + block_size));

				// Allocate the same block.
				ASSERT_TRUE (this->directory_->allocate (block, block + block_size));
				EXPECT_TRUE (this->directory_->check_allocated (block, block + block_size));

				// Release the first half.
				this->directory_->release (block, block + block_size / 2);
				EXPECT_FALSE (this->directory_->check_allocated (block, block + block_size / 2));
				EXPECT_TRUE (this->directory_->check_allocated (block + block_size / 2, block + block_size));

				// Release the second half.
				this->directory_->release (block + block_size / 2, block + block_size);
				EXPECT_FALSE (this->directory_->check_allocated (block, block + block_size));

				// Allocate the range again.
				ASSERT_TRUE (this->directory_->allocate (block, block + block_size));
				EXPECT_TRUE (this->directory_->check_allocated (block, block + block_size));

				// Release the second half.
				this->directory_->release (block + block_size / 2, block + block_size);
				EXPECT_TRUE (this->directory_->check_allocated (block, block + block_size / 2));
				EXPECT_FALSE (this->directory_->check_allocated (block + block_size / 2, block + block_size));

				// Release the first half.
				this->directory_->release (block, block + block_size / 2);
				EXPECT_FALSE (this->directory_->check_allocated (block, block + block_size));

				// Allocate the range again.
				ASSERT_TRUE (this->directory_->allocate (block, block + block_size));
				EXPECT_TRUE (this->directory_->check_allocated (block, block + block_size));

				// Release half at center
				this->directory_->release (block + block_size / 4, block + block_size / 4 * 3);
				EXPECT_TRUE (this->directory_->check_allocated (block, block + block_size / 4));
				EXPECT_FALSE (this->directory_->check_allocated (block + block_size / 4, block + block_size / 4 * 3));
				EXPECT_TRUE (this->directory_->check_allocated (block + block_size / 4 * 3, block + block_size));

				// Release the first quarter.
				this->directory_->release (block, block + block_size / 4);
				EXPECT_FALSE (this->directory_->check_allocated (block, block + block_size / 4 * 3));
				EXPECT_TRUE (this->directory_->check_allocated (block + block_size / 4 * 3, block + block_size));

				// Release the last quarter.
				this->directory_->release (block + block_size / 4 * 3, block + block_size);
				EXPECT_FALSE (this->directory_->check_allocated (block, block + block_size));

				// Allocate the first half.
				this->directory_->allocate (block, block + block_size / 2);
				EXPECT_TRUE (this->directory_->check_allocated (block, block + block_size / 2));
				EXPECT_FALSE (this->directory_->check_allocated (block + block_size / 2, block + block_size));

				// Allocate the second half.
				this->directory_->allocate (block + block_size / 2, block + block_size);
				EXPECT_TRUE (this->directory_->check_allocated (block, block + block_size));
			}
			EXPECT_TRUE (this->directory_->check_allocated (0, TypeParam::DirectoryType::UNIT_COUNT));
			this->directory_->release (0, TypeParam::DirectoryType::UNIT_COUNT);
		}

		EXPECT_TRUE (this->directory_->empty ());
	}
}

TYPED_TEST (TestHeapDirectory, Release2)
{
	EXPECT_TRUE (this->directory_->empty ());
	for (int pass = 0; pass < 2; ++pass) {

		for (size_t block = 0, end = TypeParam::DirectoryType::UNIT_COUNT; block < end; block += TypeParam::DirectoryType::MAX_BLOCK_SIZE) {
			ASSERT_TRUE (this->directory_->allocate (block, block + TypeParam::DirectoryType::MAX_BLOCK_SIZE));
		}

		for (size_t block = 0, end = TypeParam::DirectoryType::UNIT_COUNT; block < end; ++block) {
			ASSERT_TRUE (this->directory_->check_allocated (block, block + 1));
			this->directory_->release (block, block + 1);
		}

		EXPECT_TRUE (this->directory_->empty ());
	}
}

TYPED_TEST (TestHeapDirectory, Reset)
{
	EXPECT_TRUE (this->directory_->empty ());
	this->directory_->allocate (1);
	EXPECT_TRUE (!this->directory_->empty ());
	this->directory_->reset ();
	EXPECT_TRUE (this->directory_->empty ());
}

struct Block
{
	unsigned begin;
	unsigned end;

	bool operator < (const Block& rhs) const
	{
		return begin < rhs.begin;
	}
};

class RandomAllocator
{
public:
	RandomAllocator (unsigned seed = std::mt19937::default_seed) :
		rndgen_ (seed)
	{
		allocated_.reserve (1024);
	}

	template <class DirType>
	void run (DirType* dir, int iterations);

	const std::vector <Block>& allocated () const
	{
		return allocated_;
	}

private:
	std::mt19937 rndgen_;
	std::vector <Block> allocated_;
	static std::atomic <unsigned> total_allocated_;
};

std::atomic <unsigned> RandomAllocator::total_allocated_ (0);

template <class DirType>
void RandomAllocator::run (DirType* dir, int iterations)
{
	for (int i = 0; i < iterations; ++i) {
		unsigned total = total_allocated_;
		bool rel = !allocated_.empty () 
			&& (total >= DirType::UNIT_COUNT || std::bernoulli_distribution ((double)total_allocated_ / (double)DirType::UNIT_COUNT)(rndgen_));
		if (!rel) {
			unsigned free_cnt = DirType::UNIT_COUNT - total_allocated_;
			if (!free_cnt)
				rel = true;
			else {
				unsigned max_size = DirType::MAX_BLOCK_SIZE;
				if (max_size > free_cnt)
					max_size = 0x80000000 >> nlz ((unsigned)free_cnt);

				unsigned size = std::uniform_int_distribution <unsigned> (1, max_size)(rndgen_);
				unsigned block = dir->allocate (size);
				if (block >= 0) {
					total_allocated_ += size;
					allocated_.push_back ({(unsigned)block, (unsigned)block + size});
					EXPECT_TRUE (dir->check_allocated (allocated_.back ().begin, allocated_.back ().end));
				} else
					rel = true;
			}
		}

		if (rel) {
			ASSERT_FALSE (allocated_.empty ());
			size_t idx = std::uniform_int_distribution <size_t> (0, allocated_.size () - 1)(rndgen_);
			Block& block = allocated_ [idx];
			EXPECT_TRUE (dir->check_allocated (block.begin, block.end));
			dir->release (block.begin, block.end);
			total_allocated_ -= block.end - block.begin;
			allocated_.erase (allocated_.begin () + idx);
		}
	}
}

class AllocatedBlocks :
	public std::set <Block>
{
public:
	void add (const std::vector <Block>& blocks);
	
	template <class DirType>
	void check (DirType* dir) const;
};

void AllocatedBlocks::add (const std::vector <Block>& blocks)
{
	for (auto p = blocks.cbegin (); p != blocks.cend (); ++p) {
		auto ins = insert (*p);
		ASSERT_TRUE (ins.second);
		if (ins.first != begin ()) {
			auto prev = ins.first;
			--prev;
			ASSERT_LE (prev->end, ins.first->begin);
		}
		auto next = ins.first;
		++next;
		if (next != end ()) {
			ASSERT_LE (ins.first->end, next->begin);
		}
	}
}

template <class DirType>
void AllocatedBlocks::check (DirType* dir) const
{
	size_t free_begin = 0;
	for (auto pa = begin ();;) {
		size_t free_end;
		if (pa != end ())
			free_end = pa->begin;
		else
			free_end = DirType::UNIT_COUNT;
		if (free_begin != free_end) {
			bool ok = dir->allocate (free_begin, free_end);
			ASSERT_TRUE (ok);
			dir->release (free_begin, free_end);
		}
		if (pa != this->end ()) {
			ASSERT_TRUE (dir->check_allocated (pa->begin, pa->end));
			free_begin = pa->end;
			++pa;
		} else
			break;
	}
}

TYPED_TEST (TestHeapDirectory, Random)
{
	EXPECT_TRUE (this->directory_->empty ());

	RandomAllocator ra;
	static const int ITERATIONS = 1000;
	static const int ALLOC_ITERATIONS = 1000;
	for (int i = 0; i < ITERATIONS; ++i) {
		ra.run (this->directory_, ALLOC_ITERATIONS);

		AllocatedBlocks checker;
		ASSERT_NO_FATAL_FAILURE (checker.add (ra.allocated ()));
		ASSERT_NO_FATAL_FAILURE (checker.check (this->directory_));
	}
	for (auto p = ra.allocated ().cbegin (); p != ra.allocated ().cend (); ++p)
		this->directory_->release (p->begin, p->end);

	EXPECT_TRUE (this->directory_->empty ());
}

class ThreadAllocator :
	public RandomAllocator,
	public thread
{
public:
	ThreadAllocator (unsigned seed) :
		RandomAllocator (seed)
	{}

	template <class DirType>
	void run (DirType* dir, int iterations)
	{
		thread t (&RandomAllocator::template run <DirType>, this, dir, iterations);
		swap (t);
	}
};

TYPED_TEST (TestHeapDirectory, MultiThread)
{
	EXPECT_TRUE (this->directory_->empty ());

	const size_t thread_cnt = thread::hardware_concurrency ();
	static const int THREAD_ITERATIONS = 1000;
	static const int ITERATIONS = 1000;
	std::vector <ThreadAllocator> threads;
	threads.reserve (thread_cnt);
	for (unsigned i = 0; i < thread_cnt; ++i)
		threads.emplace_back (i + 1);

	for (int i = 0; i < ITERATIONS; ++i) {
		for (auto p = threads.begin (); p != threads.end (); ++p)
			p->run (this->directory_, THREAD_ITERATIONS);

		for (auto p = threads.begin (); p != threads.end (); ++p)
			p->join ();

		AllocatedBlocks checker;
		for (auto pt = threads.begin (); pt != threads.end (); ++pt)
			ASSERT_NO_FATAL_FAILURE (checker.add (pt->allocated ()));

		ASSERT_NO_FATAL_FAILURE (checker.check (this->directory_));
	}

	for (auto pt = threads.begin (); pt != threads.end (); ++pt) {
		for (auto p = pt->allocated ().cbegin (); p != pt->allocated ().cend (); ++p)
			this->directory_->release (p->begin, p->end);
	}

	EXPECT_TRUE (this->directory_->empty ());
}

}
