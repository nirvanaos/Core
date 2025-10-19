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
#include "../Source/PriorityQueueReorder.h"
#include "../Source/SkipListWithPool.h"
#include "../Source/Heap.h"
#include "../Source/Chrono.h"
#include "../Source/SystemInfo.h"
#include <gtest/gtest.h>
#include <queue>
#include <array>
#include <random>
#include <vector>
#include <atomic>
#include <Mock/Thread.h>
#include <Mock/Mutex.h>

using Nirvana::DeadlineTime;
using Nirvana::Core::PriorityQueueReorder;
using Nirvana::Core::SkipListWithPool;
using Nirvana::RandomGen;

namespace TestPriorityQueue {

using thread = Nirvana::Mock::Thread;
using mutex = Nirvana::Mock::Mutex;
using Nirvana::Mock::LockGuard;

template <class PQ>
class TestPriorityQueue :
	public ::testing::Test
{
protected:
	TestPriorityQueue ()
	{}

	virtual ~TestPriorityQueue ()
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
		Nirvana::Core::Chrono::terminate ();
		Nirvana::Core::Heap::terminate ();
		Nirvana::Core::SystemInfo::terminate ();
	}
};

typedef uint32_t Index;

struct Value
{
	Index idx;

	bool operator < (const Value& rhs) const
	{
		return idx < rhs.idx;
	}
};

struct StdNode : Value
{
	DeadlineTime deadline;

	bool operator < (const StdNode& rhs) const
	{ // STL priority queue sorted in descending order.
		if (deadline > rhs.deadline)
			return true;
		else if (deadline < rhs.deadline)
			return false;
		return idx > rhs.idx;
	}
};

typedef std::priority_queue <StdNode> StdPriorityQueue;

typedef SkipListWithPool <PriorityQueueReorder <Value, 10> > QueueWithPoolBase;

class QueueWithPool : public QueueWithPoolBase
{
public:
	QueueWithPool () :
		QueueWithPoolBase (8)
	{}
};

typedef ::testing::Types <
	PriorityQueueReorder <Value, 10> // 0
	, QueueWithPool              // 1
> QTypes;

TYPED_TEST_SUITE (TestPriorityQueue, QTypes);

TYPED_TEST (TestPriorityQueue, SingleThread)
{
	TypeParam queue;
	StdPriorityQueue queue_std;
	RandomGen rndgen;
	std::uniform_int_distribution <DeadlineTime> distr;

	Value v;
	ASSERT_FALSE (queue.delete_min (v));
	EXPECT_TRUE (queue.empty ());

	static const Index MAX_COUNT = 1000;
	for (Index i = 0; i < MAX_COUNT; ++i) {
		DeadlineTime deadline = distr (rndgen);
		StdNode node;
		node.deadline = deadline;
		node.idx = i;
		ASSERT_TRUE (queue.insert (deadline, node));
		EXPECT_FALSE (queue.empty ());
		queue_std.push (node);
	}

	for (Index i = 0; i < MAX_COUNT; ++i) {
		Value val;
		DeadlineTime deadline;
		ASSERT_TRUE (queue.delete_min (val, deadline));
		ASSERT_EQ (val.idx, queue_std.top ().idx);
		ASSERT_EQ (deadline, queue_std.top ().deadline);
		queue_std.pop ();
	}

	EXPECT_TRUE (queue.empty ());
}

TYPED_TEST (TestPriorityQueue, Reorder)
{
	TypeParam queue;
	StdPriorityQueue queue_std;
	RandomGen rndgen;
	std::uniform_int_distribution <DeadlineTime> distr;

	static const Index MAX_COUNT = 1000;
	for (Index i = 0; i < MAX_COUNT; ++i) {
		DeadlineTime deadline = distr (rndgen);
		StdNode node;
		node.deadline = deadline;
		node.idx = i;
		ASSERT_TRUE (queue.insert (deadline, node));
		EXPECT_FALSE (queue.empty ());
		queue_std.push (node);
	}

	for (int j = 0; j < 2; ++j) {
		StdPriorityQueue tmp;
		tmp.swap (queue_std);
		for (int i = 0; i < MAX_COUNT; ++i) {
			DeadlineTime deadline = distr (rndgen);
			StdNode val = tmp.top ();
			if (deadline != val.deadline) {
				DeadlineTime old = val.deadline;
				val.deadline = deadline;
				ASSERT_TRUE (queue.reorder (deadline, val, old));
			}
			queue_std.push (val);
			tmp.pop ();
		}
	}

	for (Index i = 0; i < MAX_COUNT; ++i) {
		Value val;
		DeadlineTime deadline;
		ASSERT_TRUE (queue.delete_min (val, deadline));
		ASSERT_EQ (val.idx, queue_std.top ().idx);
		ASSERT_EQ (deadline, queue_std.top ().deadline);
		queue_std.pop ();
	}

	EXPECT_TRUE (queue.empty ());
}

// Ensure that all values are different.
std::atomic <Index> g_timestamp (0);

template <class PQ>
class ThreadTest
{
	static const size_t NUM_PRIORITIES = 20;
#ifdef NDEBUG
	static const int NUM_ITERATIONS = 200000;
#else
	static const int NUM_ITERATIONS = 50000;
#endif
	static const int MAX_QUEUE_SIZE = 10000;

public:
	ThreadTest () :
		queue_ (),
		queue_size_ (0)
	{}

	void thread_proc ();

	void finalize ();

private:
	class Map
	{
	public:
		bool insert (const Value& val, DeadlineTime deadline)
		{
			LockGuard lock (mutex_);
			return map_.emplace (val.idx, deadline).second;
		}

		bool erase (const Value& val)
		{
			mutex_.lock ();
			bool ret = map_.erase (val.idx);
			mutex_.unlock ();
			return ret;
		}

		bool empty () const
		{
			return map_.empty ();
		}

	private:
		mutable mutex mutex_;
		std::unordered_map <Index, DeadlineTime> map_;
	};

private:
	Map map_;
	PQ queue_;
	std::atomic <int> queue_size_;
};

template <class PQ>
void ThreadTest <PQ>::thread_proc ()
{
	RandomGen rndgen;
	std::uniform_int_distribution <DeadlineTime> distr (0, NUM_PRIORITIES - 1);

	std::vector <StdNode> this_thread_nodes;

	static const bool REORDER = false;

	for (int i = NUM_ITERATIONS; i > 0; --i) {
		Value val;
		DeadlineTime deadline;
		if (!std::bernoulli_distribution (std::min (1., ((double)queue_size_ / (double)MAX_QUEUE_SIZE))) (rndgen)) {
			deadline = distr (rndgen);
			++queue_size_;
			val.idx = g_timestamp++;
			ASSERT_TRUE (map_.insert (val, deadline));
			ASSERT_TRUE (queue_.insert (deadline, val));
			this_thread_nodes.push_back ({ val.idx, deadline });
		} else if (REORDER && !this_thread_nodes.empty () && std::bernoulli_distribution (0.5) (rndgen)) {
			size_t idx = std::uniform_int_distribution <size_t> (0, this_thread_nodes.size () - 1) (rndgen);
			StdNode& node = this_thread_nodes [idx];
			DeadlineTime new_deadline;
			for (;;) {
				deadline = distr (rndgen);
				if (deadline != node.deadline)
					break;
			}
			val.idx = node.idx;
			if (queue_.reorder (deadline, val, node.deadline))
				node.deadline = deadline;
			else
				this_thread_nodes.erase (this_thread_nodes.begin () + idx);
		} else if (queue_.delete_min (val, deadline)) {
			--queue_size_;
			ASSERT_TRUE (map_.erase (val));

			auto it = std::lower_bound (this_thread_nodes.begin (), this_thread_nodes.end (), val.idx,
				[](const StdNode& l, Index r) -> bool { return l.idx < r; });
			if (it != this_thread_nodes.end ())
				this_thread_nodes.erase (it);
		}
	}
}

template <class PQ>
void ThreadTest <PQ>::finalize ()
{
	Value val;
	DeadlineTime deadline;
	while (queue_.delete_min (val, deadline)) {
		ASSERT_TRUE (map_.erase (val));
		--queue_size_;
	}

	EXPECT_EQ (queue_size_, 0);
	EXPECT_TRUE (map_.empty ());
}

TYPED_TEST (TestPriorityQueue, MultiThread)
{
	ThreadTest <TypeParam> test;

	const unsigned int thread_cnt = thread::hardware_concurrency ();
	std::vector <thread> threads;

	for (unsigned int i = 0; i < thread_cnt; ++i)
		threads.emplace_back (&ThreadTest <TypeParam>::thread_proc, std::ref (test));

	for (auto p = threads.begin (); p != threads.end (); ++p)
		p->join ();

	test.finalize ();
}

}
