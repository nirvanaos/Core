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
#include "../Source/Stack.h"
#include "../Source/Heap.h"
#include "../Source/Chrono.h"
#include "../Source/SystemInfo.h"
#include <gtest/gtest.h>
#include <unordered_set>
#include <vector>
#include <random>
#include <Mock/Thread.h>

namespace TestStack {

using namespace Nirvana::Core;

using thread = Nirvana::Mock::Thread;

template <class Val>
class TestStack :
	public ::testing::Test
{
protected:
	TestStack ()
	{}

	virtual ~TestStack ()
	{}

	// If the constructor and destructor are not enough for setting up
	// and cleaning up each test, you can define the following methods:

	virtual void SetUp ()
	{
		// Code here will be called immediately after the constructor (right
		// before each test).
		Nirvana::Core::SystemInfo::initialize ();
		ASSERT_TRUE (Heap::initialize ());
		Nirvana::Core::Chrono::initialize ();
	}

	virtual void TearDown ()
	{
		// Code here will be called immediately after each test (right
		// before the destructor).
		Nirvana::Core::Chrono::terminate ();
		Heap::terminate ();
		Nirvana::Core::SystemInfo::terminate ();
	}

};

template <size_t SIZE>
class Value : 
	public StackElem,
	public CoreObject
{
public:
	virtual ~Value ()	// Make class virtual to test unaligned StackElem
	{}

	size_t dummy [SIZE];
};

typedef ::testing::Types <
	Value <4>,
	Value <8>,
	Value <100>
> ValTypes;

TYPED_TEST_SUITE (TestStack, ValTypes);


TYPED_TEST (TestStack, SingleThread)
{
	using Value = TypeParam;
	using MyStack = Stack <Value>;

	MyStack stack;
	ASSERT_FALSE (stack.pop ());
	for (int i = 0; i < 3; ++i) {
		stack.push (*new Value);
	}
	for (int i = 0; i < 3; ++i) {
		Value* p = stack.pop ();
		ASSERT_TRUE (p);
		delete p;
	}
	ASSERT_FALSE (stack.pop ());
}

TYPED_TEST (TestStack, MultiThread)
{
	using Value = TypeParam;
	using MyStack = Stack <Value>;

	static const unsigned thread_cnt = thread::hardware_concurrency ();
	static const unsigned element_cnt = 20;
	static const unsigned iterations = 10000;

	MyStack stack;
	std::unordered_set <Value*> valset;
	for (unsigned cnt = thread_cnt * element_cnt; cnt; --cnt) {
		Value* val = new Value;
		valset.insert (val);
		stack.push (*val);
	}

	std::vector <thread> threads;
	threads.reserve (thread_cnt);
	for (unsigned cnt = thread_cnt; cnt; --cnt) {
		threads.emplace_back (thread (
			[&stack](unsigned seed) {
				std::mt19937 rndgen (seed);
				std::uniform_int_distribution <> distrib (0, element_cnt);
				std::vector <Value*> buf (element_cnt);
				for (unsigned i = 0; i < iterations; ++i) {
					unsigned cnt = distrib (rndgen);
					for (unsigned i = 0; i < cnt; ++i) {
						Value* val = stack.pop ();
						EXPECT_TRUE (val);
						if (!val)
							return;
						buf [i] = val;
					}
					for (unsigned i = 0; i < cnt; ++i) {
						stack.push (*buf [i]);
					}
				}
			}, cnt));
	}

	for (auto& t : threads) {
		t.join ();
	}

	for (unsigned cnt = thread_cnt * element_cnt; cnt; --cnt) {
		Value* val = stack.pop ();
		ASSERT_TRUE (val);
		ASSERT_EQ (valset.erase (val), 1);
		delete val;
	}

	ASSERT_TRUE (valset.empty ());
	ASSERT_FALSE (stack.pop ());
}

TYPED_TEST (TestStack, Stress)
{
	using Value = TypeParam;
	using MyStack = Stack <Value>;

	static const unsigned thread_cnt = thread::hardware_concurrency ();
	static const unsigned iterations = 100000;

	MyStack stack;

	std::vector <thread> threads;
	threads.reserve (thread_cnt);
	for (unsigned cnt = thread_cnt; cnt; --cnt) {
		threads.emplace_back (thread (
			[&stack]() {
				for (unsigned i = 0; i < iterations; ++i) {
					Value* val = stack.pop ();
					if (!val)
						val = new Value;
					stack.push (*val);
				}
			}));
	}

	for (auto& t : threads) {
		t.join ();
	}

	while (Value* val = stack.pop ()) {
		delete val;
	}
}

}