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
#include "../Source/PreallocatedStack.h"
#include <gtest/gtest.h>

using namespace Nirvana::Core;

namespace TestPreallocatedStack {

class TestPreallocatedStack :
	public ::testing::Test
{
protected:
	TestPreallocatedStack ()
	{}

	virtual ~TestPreallocatedStack ()
	{}

	// If the constructor and destructor are not enough for setting up
	// and cleaning up each test, you can define the following methods:

	virtual void SetUp ()
	{
		// Code here will be called immediately after the constructor (right
		// before each test).
		ASSERT_TRUE (Heap::initialize ());
	}

	virtual void TearDown ()
	{
		// Code here will be called immediately after each test (right
		// before the destructor).
		Heap::terminate ();
	}
};

TEST_F (TestPreallocatedStack, Test)
{
	PreallocatedStack <int, 16, 16> stack;

	ASSERT_TRUE (stack.empty ());

	for (int i = 0; i < 100; ++i) {
		stack.push (i);
		ASSERT_FALSE (stack.empty ());
		ASSERT_EQ (i, stack.top ());
	}

	for (int i = 99; i >= 0; --i) {
		ASSERT_EQ (i, stack.top ());
		ASSERT_FALSE (stack.empty ());
		stack.pop ();
	}
}

TEST_F (TestPreallocatedStack, Init)
{
	PreallocatedStack <int> stack (1);
	ASSERT_FALSE (stack.empty ());
	EXPECT_EQ (stack.top (), 1);
}

}
