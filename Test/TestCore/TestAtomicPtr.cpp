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
#include "../Source/AtomicPtr.h"
#include "../Source/LockablePtr.h"
#include "../Source/AtomicCounter.h"
#include "../Source/Chrono.h"
#include "../Source/SystemInfo.h"
#include <gtest/gtest.h>
#include <vector>
#include <Mock/Thread.h>

using namespace Nirvana::Core;

namespace TestAtomicPtr {

using thread = Nirvana::Mock::Thread;

template <unsigned ALIGN>
struct Align
{
	static const unsigned alignment = ALIGN;
};

template <typename Align>
class TestAtomicPtr :
	public ::testing::Test
{
protected:
	static const unsigned NUM_ITERATIONS = 10000;
	static const unsigned alignment = Align::alignment;

	struct Value
	{
		RefCounter ref_cnt;

		void add_ref ()
		{
			ref_cnt.increment ();
		}

		void remove_ref ()
		{
			if (!ref_cnt.decrement ())
				delete this;
		}

		void* operator new (size_t cb)
		{
			return aligned_alloc (alignment, cb);
		}

		void operator delete (void* p)
		{
			free (p);
		}
	};

	TestAtomicPtr ()
	{}

	virtual ~TestAtomicPtr ()
	{}

	// If the constructor and destructor are not enough for setting up
	// and cleaning up each test, you can define the following methods:

	virtual void SetUp ()
	{
		// Code here will be called immediately after the constructor (right
		// before each test).
		Nirvana::Core::SystemInfo::initialize ();
		Nirvana::Core::Chrono::initialize ();
	}

	virtual void TearDown ()
	{
		// Code here will be called immediately after each test (right
		// before the destructor).
		Value* p = shared_ptr_.load ();
		if (p)
			p->remove_ref ();
		Nirvana::Core::Chrono::terminate ();
		Nirvana::Core::SystemInfo::terminate ();
	}

	typedef TaggedPtrT <Value, 0, alignment> Ptr;
	typedef typename Ptr::Lockable SharedPtr;

	static SharedPtr shared_ptr_;

public:
	static void run ()
	{
		for (unsigned cnt = NUM_ITERATIONS; cnt; --cnt) {
			Value* pnew = new Value;
			for (;;) {
				auto old = shared_ptr_.lock ();
				Value* p = old;
				if (p)
					p->add_ref ();
				shared_ptr_.unlock ();
				if (shared_ptr_.cas (old, pnew)) {
					if (p) {
						p->remove_ref ();
						p->remove_ref ();
					}
					break;
				} else if (p)
					p->remove_ref ();
			}
		}
	}
};

template <typename Align>
typename TestAtomicPtr <Align>::SharedPtr TestAtomicPtr <Align>::shared_ptr_;

typedef ::testing::Types <Align <4>, Align <32> > Alignments;

TYPED_TEST_SUITE (TestAtomicPtr, Alignments);

TYPED_TEST (TestAtomicPtr, Run)
{
	const size_t thread_cnt = std::max (thread::hardware_concurrency (), (unsigned)2);
	std::vector <thread> threads;
	threads.reserve (thread_cnt);
	for (unsigned int i = 0; i < thread_cnt; ++i)
		threads.emplace_back (&TestAtomicPtr <TypeParam>::run);
	for (auto t = threads.begin (); t != threads.end (); ++t) {
		t->join ();
	}
}

}
