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
#include "../Source/SkipList.h"
#include "../Source/Heap.h"
#include "../Source/Chrono.h"
#include "../Source/SystemInfo.h"
#include <gtest/gtest.h>
#include <random>
#include <Mock/Thread.h>

namespace TestSkipList {

using thread = Nirvana::Mock::Thread;

template <typename Val, unsigned MAX_LEVEL>
class SkipList : public Nirvana::Core::SkipList <Val, MAX_LEVEL>
{
	using Base = Nirvana::Core::SkipList <Val, MAX_LEVEL>;

public:
	SkipList () :
		Base (Nirvana::Core::Heap::core_heap ())
	{}
};

template <class SL>
class TestSkipList : public ::testing::Test
{
protected:
	TestSkipList ()
	{}

	virtual ~TestSkipList ()
	{}

	// If the constructor and destructor are not enough for setting up
	// and cleaning up each test, you can define the following methods:

	virtual void SetUp ()
	{
		// Code here will be called immediately after the constructor (right
		// before each test).
		Nirvana::Core::SystemInfo::initialize ();
		ASSERT_TRUE (Nirvana::Core::Heap::initialize ());
		Nirvana::Core::Chrono::initialize ();
	}

	virtual void TearDown ()
	{
		// Code here will be called immediately after each test (right
		// before the destructor).
		Nirvana::Core::Heap::terminate ();
		Nirvana::Core::Chrono::terminate ();
		Nirvana::Core::SystemInfo::terminate ();
	}
};

typedef ::testing::Types <
	SkipList <int, 4>,  // 0
	SkipList <int, 8>,  // 1
	SkipList <int, 10>, // 2
	SkipList <int, 16>  // 3
> MaxLevel;

TYPED_TEST_SUITE (TestSkipList, MaxLevel);

TYPED_TEST (TestSkipList, DeleteMin)
{
	TypeParam sl;

	sl.release_node (sl.insert (0).first);
	int i;
	ASSERT_TRUE (sl.delete_min (i));
	EXPECT_EQ (i, 0);
#ifndef NDEBUG
	ASSERT_EQ (sl.dbg_node_cnt (), 0);
#endif

	std::mt19937 rndgen;
	static const size_t MAX_COUNT = 1000;
	std::vector <int> rand_numbers;
	rand_numbers.reserve (MAX_COUNT);
	while (rand_numbers.size () < MAX_COUNT) {
		int r = std::uniform_int_distribution <int> (
			std::numeric_limits <int>::min (), std::numeric_limits <int>::max ())(rndgen);
		auto ins = sl.insert (r);
		sl.release_node (ins.first);
		if (ins.second)
			rand_numbers.push_back (r);
	}

	std::sort (rand_numbers.begin (), rand_numbers.end ());

	for (auto n : rand_numbers) {
		ASSERT_TRUE (sl.delete_min (i));
		ASSERT_EQ (i, n);
	}
#ifndef NDEBUG
	EXPECT_EQ (sl.dbg_node_cnt (), 0);
#endif
}

std::atomic <size_t> list_size = 0;

template <class SL>
void delete_min (SL& sl, unsigned seed)
{
	std::mt19937 rndgen (seed);
	static const size_t NUM_ITERATIONS = 200000;
	static const size_t MAX_LIST_SIZE = 10000;
	for (size_t i = NUM_ITERATIONS; i > 0; --i) {
		if (!std::bernoulli_distribution (std::min (1., ((double)list_size / (double)MAX_LIST_SIZE))) (rndgen)) {
			int r = std::uniform_int_distribution <int> (
				std::numeric_limits <int>::min (), std::numeric_limits <int>::max ())(rndgen);
			auto ins = sl.insert (r);
			sl.release_node (ins.first);
			if (ins.second)
				++list_size;
		} else {
			int i;
			if (sl.delete_min (i))
				--list_size;
		}
	}
}

TYPED_TEST (TestSkipList, DeleteMinMT)
{
	TypeParam sl;

	const unsigned int thread_cnt = thread::hardware_concurrency ();
	std::vector <thread> threads;

	threads.reserve (thread_cnt);
	for (unsigned int i = 0; i < thread_cnt; ++i) {
		threads.emplace_back (delete_min <TypeParam>, std::ref (sl), std::mt19937::default_seed + i);
	}
	for (unsigned int i = 0; i < thread_cnt; ++i) {
		threads [i].join ();
	}
	threads.clear ();
	int i;
	while (sl.delete_min (i))
		;
#ifndef NDEBUG
	ASSERT_EQ (sl.dbg_node_cnt (), 0);
#endif
}

TYPED_TEST (TestSkipList, Move)
{
	TypeParam sl;
	sl.release_node (sl.insert (0).first);
	sl.release_node (sl.insert (1).first);
	sl.release_node (sl.insert (2).first);

	TypeParam sl1;
	sl1 = std::move (sl);
#ifndef NDEBUG
	ASSERT_EQ (sl.dbg_node_cnt (), 0);
	ASSERT_EQ (sl1.dbg_node_cnt (), 3);
#endif
	ASSERT_FALSE (sl.get_min_node ());

	int i;
	ASSERT_TRUE (sl1.delete_min (i));
	EXPECT_EQ (i, 0);
	ASSERT_TRUE (sl1.delete_min (i));
	EXPECT_EQ (i, 1);
	ASSERT_TRUE (sl1.delete_min (i));
	EXPECT_EQ (i, 2);
}

TYPED_TEST (TestSkipList, Equal)
{
	TypeParam sl;

	auto ins = sl.insert (1);
	ASSERT_TRUE (ins.second);
	auto ins2 = sl.insert (1);
	ASSERT_FALSE (ins2.second);
	EXPECT_EQ (ins.first, ins2.first);
	sl.release_node (ins.first);
	sl.release_node (ins2.first);

	int i;
	ASSERT_TRUE (sl.delete_min (i));
	EXPECT_EQ (i, 1);
	EXPECT_FALSE (sl.delete_min (i));
#ifndef NDEBUG
	EXPECT_EQ (sl.dbg_node_cnt (), 0);
#endif
}

TYPED_TEST (TestSkipList, MinMax)
{
	TypeParam sl;

	auto ins = sl.insert (std::numeric_limits <int>::max ());
	ASSERT_TRUE (ins.second);
	sl.release_node (ins.first);
	int i;
	ASSERT_TRUE (sl.delete_min (i));
	EXPECT_EQ (i, std::numeric_limits <int>::max ());

	ins = sl.insert (std::numeric_limits <int>::min ());
	ASSERT_TRUE (ins.second);
	sl.release_node (ins.first);
	ASSERT_TRUE (sl.delete_min (i));
	EXPECT_EQ (i, std::numeric_limits <int>::min ());
}

template <class SL>
void find_and_delete (SL& sl, unsigned seed)
{
	std::mt19937 rndgen (seed);

	static const size_t MAX_COUNT = 1000;
	std::vector <int> rand_numbers;
	rand_numbers.reserve (MAX_COUNT);
	while (rand_numbers.size () < MAX_COUNT) {
		int r = std::uniform_int_distribution <int> (
			std::numeric_limits <int>::min (), std::numeric_limits <int>::max ())(rndgen);
		auto ins = sl.insert (r);
		sl.release_node (ins.first);
		if (ins.second)
			rand_numbers.push_back (r);
	}

	std::shuffle (rand_numbers.begin (), rand_numbers.end (), rndgen);

	for (auto n : rand_numbers) {
		typename SL::NodeVal* p = sl.find_and_delete (n);
		EXPECT_TRUE (p);
		sl.release_node (p);
	}
}

TYPED_TEST (TestSkipList, FindAndDelete)
{
	TypeParam sl;
	find_and_delete (sl, std::mt19937::default_seed);
	ASSERT_FALSE (sl.delete_min ());
#ifndef NDEBUG
	EXPECT_EQ (sl.dbg_node_cnt (), 0);
#endif
}
 
TYPED_TEST (TestSkipList, FindAndDeleteMT)
{
	TypeParam sl;

	const unsigned int thread_cnt = thread::hardware_concurrency ();
	std::vector <thread> threads;

	threads.reserve (thread_cnt);
	for (int it = 0; it < 10; ++it) {
		for (unsigned int i = 0; i < thread_cnt; ++i) {
			threads.emplace_back (find_and_delete <TypeParam>, std::ref (sl), std::mt19937::default_seed + it + i + 1);
		}
		for (unsigned int i = 0; i < thread_cnt; ++i) {
			threads [i].join ();
		}
		threads.clear ();
		ASSERT_FALSE (sl.delete_min ());
#ifndef NDEBUG
		ASSERT_EQ (sl.dbg_node_cnt (), 0);
#endif
	}
}

template <class SL>
void find_and_remove (SL& sl, unsigned seed)
{
	std::mt19937 rndgen (seed);

	static const size_t MAX_COUNT = 1000;
	std::vector <int> rand_numbers;
	rand_numbers.reserve (MAX_COUNT);
	while (rand_numbers.size () < MAX_COUNT) {
		int r = std::uniform_int_distribution <int> (
			std::numeric_limits <int>::min (), std::numeric_limits <int>::max ())(rndgen);
		auto ins = sl.insert (r);
		sl.release_node (ins.first);
		if (ins.second)
			rand_numbers.push_back (r);
	}

	std::shuffle (rand_numbers.begin (), rand_numbers.end (), rndgen);

	for (auto n : rand_numbers) {
		typename SL::NodeVal* p = sl.find (n);
		ASSERT_TRUE (p);
		ASSERT_TRUE (sl.remove (p));
		sl.release_node (p);
	}
}

TYPED_TEST (TestSkipList, FindAndRemove)
{
	TypeParam sl;
	find_and_remove (sl, std::mt19937::default_seed);
	ASSERT_FALSE (sl.delete_min ());
#ifndef NDEBUG
	EXPECT_EQ (sl.dbg_node_cnt (), 0);
#endif
}

TYPED_TEST (TestSkipList, FindAndRemoveMT)
{
	TypeParam sl;

	const unsigned int thread_cnt = thread::hardware_concurrency ();
	std::vector <thread> threads;

	threads.reserve (thread_cnt);
	for (int it = 0; it < 10; ++it) {
		for (unsigned int i = 0; i < thread_cnt; ++i) {
			threads.emplace_back (find_and_remove <TypeParam>, std::ref (sl), std::mt19937::default_seed + it + i + 1);
		}
		for (unsigned int i = 0; i < thread_cnt; ++i) {
			threads [i].join ();
		}
		threads.clear ();
		ASSERT_FALSE (sl.delete_min ());
#ifndef NDEBUG
		ASSERT_EQ (sl.dbg_node_cnt (), 0);
#endif
	}
}

}
