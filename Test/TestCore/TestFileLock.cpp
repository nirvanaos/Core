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
#include "../Source/FileLockRanges.h"
#include <gtest/gtest.h>

using Nirvana::Core::FileLockRanges;
using Nirvana::LockType;
using Nirvana::FileLock;

namespace TestFileLock {

class TestFileLock :
	public ::testing::Test
{
protected:
	TestFileLock ()
	{}

	virtual ~TestFileLock ()
	{}

	// If the constructor and destructor are not enough for setting up
	// and cleaning up each test, you can define the following methods:

	virtual void SetUp ()
	{
		// Code here will be called immediately after the constructor (right
		// before each test).
	}

	virtual void TearDown ()
	{
		// Code here will be called immediately after each test (right
		// before the destructor).
	}

	static const void* owner (uintptr_t i)
	{
		return (const void*)i;
	}

	static const Nirvana::FileSize FILE_SIZE_MAX = std::numeric_limits <Nirvana::FileSize>::max ();
};

TEST_F (TestFileLock, Empty)
{
	FileLockRanges ranges;

	EXPECT_TRUE (ranges.check_read (0, FILE_SIZE_MAX, owner (1)));
	EXPECT_TRUE (ranges.check_write (0, FILE_SIZE_MAX, owner (1)));
}

TEST_F (TestFileLock, Shared)
{
	FileLockRanges ranges;

	ASSERT_EQ (ranges.set (0, FILE_SIZE_MAX, LockType::LOCK_SHARED, LockType::LOCK_SHARED, owner (1)),
		LockType::LOCK_SHARED);
	ASSERT_EQ (ranges.set (0, FILE_SIZE_MAX, LockType::LOCK_SHARED, LockType::LOCK_SHARED, owner (2)),
		LockType::LOCK_SHARED);

	EXPECT_TRUE (ranges.check_read (0, FILE_SIZE_MAX, owner (1)));
	EXPECT_TRUE (ranges.check_read (0, FILE_SIZE_MAX, owner (2)));
	EXPECT_TRUE (ranges.check_read (0, FILE_SIZE_MAX, owner (3)));
	EXPECT_FALSE (ranges.check_write (0, FILE_SIZE_MAX, owner (1)));
	EXPECT_FALSE (ranges.check_write (0, FILE_SIZE_MAX, owner (2)));
	EXPECT_FALSE (ranges.check_write (0, FILE_SIZE_MAX, owner (3)));

	EXPECT_TRUE (ranges.clear (0, FILE_SIZE_MAX, owner (1)));
	EXPECT_TRUE (ranges.get_all (owner (1)).empty ());
	EXPECT_TRUE (ranges.clear (0, FILE_SIZE_MAX, owner (2)));
	EXPECT_TRUE (ranges.get_all (owner (2)).empty ());
	EXPECT_FALSE (ranges.clear (0, FILE_SIZE_MAX, owner (2)));
	EXPECT_TRUE (ranges.check_write (0, FILE_SIZE_MAX, owner (1)));
	EXPECT_TRUE (ranges.check_write (0, FILE_SIZE_MAX, owner (2)));
	EXPECT_TRUE (ranges.check_write (0, FILE_SIZE_MAX, owner (3)));
}

TEST_F (TestFileLock, Upgrade)
{
	FileLockRanges ranges;

	ASSERT_EQ (ranges.set (0, FILE_SIZE_MAX, LockType::LOCK_SHARED, LockType::LOCK_SHARED,
		owner (1)), LockType::LOCK_SHARED);
	ASSERT_EQ (ranges.set (0, FILE_SIZE_MAX, LockType::LOCK_SHARED, LockType::LOCK_SHARED,
		owner (2)), LockType::LOCK_SHARED);
	ASSERT_EQ (ranges.set (0, FILE_SIZE_MAX, LockType::LOCK_RESERVED, LockType::LOCK_RESERVED,
		owner (1)), LockType::LOCK_RESERVED);
	ASSERT_EQ (ranges.set (0, FILE_SIZE_MAX, LockType::LOCK_RESERVED, LockType::LOCK_RESERVED,
		owner (2)), LockType::LOCK_NONE);

	EXPECT_EQ (ranges.get_all (owner (1)).size (), 1);
	EXPECT_EQ (ranges.get_all (owner (2)).size (), 1);

	EXPECT_TRUE (ranges.check_read (0, FILE_SIZE_MAX, owner (1)));
	EXPECT_TRUE (ranges.check_read (0, FILE_SIZE_MAX, owner (2)));
	EXPECT_TRUE (ranges.check_read (0, FILE_SIZE_MAX, owner (3)));
	EXPECT_FALSE (ranges.check_write (0, FILE_SIZE_MAX, owner (1)));
	EXPECT_FALSE (ranges.check_write (0, FILE_SIZE_MAX, owner (2)));
	EXPECT_FALSE (ranges.check_write (0, FILE_SIZE_MAX, owner (3)));

	ASSERT_EQ (ranges.set (0, FILE_SIZE_MAX, LockType::LOCK_SHARED, LockType::LOCK_SHARED,
		owner (1)), LockType::LOCK_SHARED);
	ASSERT_EQ (ranges.set (0, FILE_SIZE_MAX, LockType::LOCK_RESERVED, LockType::LOCK_RESERVED,
		owner (2)), LockType::LOCK_RESERVED);

	EXPECT_EQ (ranges.get_all (owner (1)).size (), 1);
	EXPECT_EQ (ranges.get_all (owner (2)).size (), 1);

	ASSERT_EQ (ranges.set (0, FILE_SIZE_MAX, LockType::LOCK_EXCLUSIVE, LockType::LOCK_PENDING,
		owner (2)), LockType::LOCK_PENDING);
	EXPECT_EQ (ranges.get_all (owner (2)).size (), 1);

	EXPECT_TRUE (ranges.clear (0, FILE_SIZE_MAX, owner (1)));
	EXPECT_TRUE (ranges.get_all (owner (1)).empty ());

	ASSERT_EQ (ranges.set (0, FILE_SIZE_MAX, LockType::LOCK_SHARED, LockType::LOCK_SHARED,
		owner (1)), LockType::LOCK_NONE);
	EXPECT_TRUE (ranges.get_all (owner (1)).empty ());

	ASSERT_EQ (ranges.set (0, FILE_SIZE_MAX, LockType::LOCK_EXCLUSIVE, LockType::LOCK_PENDING,
		owner (2)), LockType::LOCK_EXCLUSIVE);
	EXPECT_EQ (ranges.get_all (owner (2)).size (), 1);

	EXPECT_FALSE (ranges.check_read (0, FILE_SIZE_MAX, owner (1)));
	EXPECT_TRUE (ranges.check_read (0, FILE_SIZE_MAX, owner (2)));
	EXPECT_FALSE (ranges.check_write (0, FILE_SIZE_MAX, owner (1)));
	EXPECT_TRUE (ranges.check_write (0, FILE_SIZE_MAX, owner (2)));

	EXPECT_TRUE (ranges.clear (0, FILE_SIZE_MAX, owner (2)));
	ASSERT_EQ (ranges.set (0, FILE_SIZE_MAX, LockType::LOCK_EXCLUSIVE, LockType::LOCK_PENDING,
		owner (1)), LockType::LOCK_EXCLUSIVE);

	EXPECT_FALSE (ranges.check_read (0, FILE_SIZE_MAX, owner (2)));
	EXPECT_TRUE (ranges.check_read (0, FILE_SIZE_MAX, owner (1)));
	EXPECT_FALSE (ranges.check_write (0, FILE_SIZE_MAX, owner (2)));
	EXPECT_TRUE (ranges.check_write (0, FILE_SIZE_MAX, owner (1)));
}

TEST_F (TestFileLock, Merge)
{
	{
		FileLockRanges ranges;

		EXPECT_EQ (ranges.set (2, 7, LockType::LOCK_SHARED, LockType::LOCK_SHARED, owner (1)),
			LockType::LOCK_SHARED);

		std::vector <FileLock> locks = ranges.get_all (owner (1));
		EXPECT_EQ (locks.size (), 1);
		EXPECT_EQ (locks.front ().start (), 2);
		EXPECT_EQ (locks.front ().len (), 5);
		EXPECT_EQ (locks.front ().type (), LockType::LOCK_SHARED);

		EXPECT_EQ (ranges.set (8, 12, LockType::LOCK_SHARED, LockType::LOCK_SHARED, owner (1)),
			LockType::LOCK_SHARED);
		locks = ranges.get_all (owner (1));
		EXPECT_EQ (locks.size (), 2);
		EXPECT_EQ (locks.front ().start (), 2);
		EXPECT_EQ (locks.front ().len (), 5);
		EXPECT_EQ (locks.back ().start (), 8);
		EXPECT_EQ (locks.back ().len (), 4);

		EXPECT_EQ (ranges.set (7, 8, LockType::LOCK_SHARED, LockType::LOCK_SHARED, owner (1)),
			LockType::LOCK_SHARED);
		locks = ranges.get_all (owner (1));
		EXPECT_EQ (locks.size (), 1);
		EXPECT_EQ (locks.front ().start (), 2);
		EXPECT_EQ (locks.front ().len (), 10);

		EXPECT_EQ (ranges.set (1, 23, LockType::LOCK_SHARED, LockType::LOCK_SHARED, owner (1)),
			LockType::LOCK_SHARED);
		locks = ranges.get_all (owner (1));
		EXPECT_EQ (locks.size (), 1);
		EXPECT_EQ (locks.front ().start (), 1);
		EXPECT_EQ (locks.front ().len (), 22);
	}

	{
		FileLockRanges ranges;

		EXPECT_EQ (ranges.set (2, 7, LockType::LOCK_SHARED, LockType::LOCK_SHARED, owner (1)),
			LockType::LOCK_SHARED);
		EXPECT_EQ (ranges.set (7, 10, LockType::LOCK_SHARED, LockType::LOCK_SHARED, owner (1)),
			LockType::LOCK_SHARED);
		std::vector <FileLock> locks = ranges.get_all (owner (1));
		EXPECT_EQ (locks.size (), 1);
		EXPECT_EQ (locks.front ().start (), 2);
		EXPECT_EQ (locks.front ().len (), 8);
	}

	{
		FileLockRanges ranges;

		EXPECT_EQ (ranges.set (7, 10, LockType::LOCK_SHARED, LockType::LOCK_SHARED, owner (1)),
			LockType::LOCK_SHARED);
		EXPECT_EQ (ranges.set (2, 7, LockType::LOCK_SHARED, LockType::LOCK_SHARED, owner (1)),
			LockType::LOCK_SHARED);
		std::vector <FileLock> locks = ranges.get_all (owner (1));
		EXPECT_EQ (locks.size (), 1);
		EXPECT_EQ (locks.front ().start (), 2);
		EXPECT_EQ (locks.front ().len (), 8);
	}
}

}
