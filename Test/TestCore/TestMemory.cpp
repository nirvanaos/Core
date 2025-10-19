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
#include <Port/Memory.h>
#include "../Source/Chrono.h"
#include "../Source/SystemInfo.h"
#include <gtest/gtest.h>

using namespace Nirvana;
using namespace Nirvana::Core;
using namespace CORBA;

namespace TestMemory {

class TestMemory :
	public ::testing::Test
{
protected:
	TestMemory ()
	{}

	virtual ~TestMemory ()
	{}

	// If the constructor and destructor are not enough for setting up
	// and cleaning up each test, you can define the following methods:

	virtual void SetUp ()
	{
		// Code here will be called immediately after the constructor (right
		// before each test).
		SystemInfo::initialize ();
		ASSERT_TRUE (Port::Memory::initialize ());
		Chrono::initialize ();
	}

	virtual void TearDown ()
	{
		// Code here will be called immediately after each test (right
		// before the destructor).
		Chrono::terminate ();
		Port::Memory::terminate ();
		SystemInfo::terminate ();
	}
};

TEST_F (TestMemory, Allocate)
{
	size_t BLOCK_SIZE = 0x10000000;	// 256M

	static const size_t ITER_CNT = 2;
	static const uint16_t iter_flags [ITER_CNT] = {
		Memory::RESERVED,
		0
	};
	for (int iteration = 0; iteration < ITER_CNT; ++iteration) {

		uint16_t flags = iter_flags [iteration];

		// Allocate and release memory.
		uint8_t* block = (uint8_t*)Port::Memory::allocate (0, BLOCK_SIZE, flags);
		ASSERT_TRUE (block);
		Port::Memory::release (block, BLOCK_SIZE);

		flags |= Memory::EXACTLY;

		// Allocate memory at the same range.
		ASSERT_EQ (block, (uint8_t*)Port::Memory::allocate (block, BLOCK_SIZE, flags));
		
		// Release the first half.
		Port::Memory::release (block, BLOCK_SIZE / 2);
		
		// Release the second half.
		Port::Memory::release (block + BLOCK_SIZE / 2, BLOCK_SIZE / 2);

		// Allocate the range again.
		ASSERT_EQ (block, (uint8_t*)Port::Memory::allocate (block, BLOCK_SIZE, flags));
		
		// Release the second half.
		Port::Memory::release (block + BLOCK_SIZE / 2, BLOCK_SIZE / 2);
		
		// Release the first half.
		Port::Memory::release (block, BLOCK_SIZE / 2);

		// Allocate the range again.
		ASSERT_EQ (block, (uint8_t*)Port::Memory::allocate (block, BLOCK_SIZE, flags));

		// Release half at center
		Port::Memory::release (block + BLOCK_SIZE / 4, BLOCK_SIZE / 2);
		
		// Release the first quarter.
		Port::Memory::release (block, BLOCK_SIZE / 4);
		
		// Release the last quarter.
		Port::Memory::release (block + BLOCK_SIZE / 4 * 3, BLOCK_SIZE / 4);

		// Allocate the first half.
		size_t cb = BLOCK_SIZE / 2;
		ASSERT_EQ (block, (uint8_t*)Port::Memory::allocate (block, cb, flags));
		
		// Allocate the second half.
		cb = BLOCK_SIZE / 2;
		ASSERT_EQ (Port::Memory::allocate (block + BLOCK_SIZE / 2, cb, flags), block + BLOCK_SIZE / 2);
		
		// Release all range
		Port::Memory::release (block, BLOCK_SIZE);

		// Allocate and release range to check that it is free.
		ASSERT_EQ (block, (uint8_t*)Port::Memory::allocate (block, BLOCK_SIZE, flags));
		Port::Memory::release (block, BLOCK_SIZE);
	}
}

TEST_F (TestMemory, Commit)
{
	size_t BLOCK_SIZE = 0x20000000;	// 512M
	size_t cb = BLOCK_SIZE;
	uint8_t* block = (uint8_t*)Port::Memory::allocate (0, cb, Memory::READ_WRITE | Memory::RESERVED);
	ASSERT_TRUE (block);
	
	EXPECT_FALSE (Port::Memory::is_writable (block, 1));

	Port::Memory::commit (block, BLOCK_SIZE);
	for (int* p = (int*)block, *end = (int*)(block + BLOCK_SIZE); p < end; ++p)
		*p = rand ();
	
	EXPECT_TRUE (Port::Memory::is_private (block, BLOCK_SIZE));

	Port::Memory::decommit (block, BLOCK_SIZE);
	Port::Memory::decommit (block, BLOCK_SIZE);

	Port::Memory::commit (block, BLOCK_SIZE);
	Port::Memory::commit (block, BLOCK_SIZE);

	Port::Memory::release (block, BLOCK_SIZE);
}

// Test the sharing of the large memory block.
TEST_F (TestMemory, Share)
{
	size_t BLOCK_SIZE = 0x20000000;	// 512M
	size_t cb = BLOCK_SIZE;
	uint8_t* block = (uint8_t*)Port::Memory::allocate (0, cb, 0);
	ASSERT_TRUE (block);
	uint8_t* end = block + BLOCK_SIZE;

	uint8_t b = 0;
	for (uint8_t* p = block; p < end; ++p)
		*p = b++;
	EXPECT_EQ (block [1], 1);

	EXPECT_TRUE (Port::Memory::is_private (block, BLOCK_SIZE));

	uint8_t* sblock = (uint8_t*)Port::Memory::copy (0, block, BLOCK_SIZE, 0);
	ASSERT_TRUE (sblock);
	uint8_t* send = sblock + BLOCK_SIZE;
	EXPECT_EQ (sblock [1], 1);

	EXPECT_FALSE (Port::Memory::is_private (block, BLOCK_SIZE));
	EXPECT_FALSE (Port::Memory::is_private (sblock, BLOCK_SIZE));
	EXPECT_TRUE (Port::Memory::is_copy (sblock, block, BLOCK_SIZE));
	EXPECT_TRUE (Port::Memory::is_copy (block, sblock, BLOCK_SIZE));

	EXPECT_EQ (sblock, (uint8_t*)Port::Memory::copy (sblock, block, BLOCK_SIZE, 0));
	EXPECT_EQ (sblock [1], 1);

	EXPECT_FALSE (Port::Memory::is_private (block, BLOCK_SIZE));
	EXPECT_FALSE (Port::Memory::is_private (sblock, BLOCK_SIZE));
	EXPECT_TRUE (Port::Memory::is_copy (sblock, block, BLOCK_SIZE));
	EXPECT_TRUE (Port::Memory::is_copy (block, sblock, BLOCK_SIZE));

	b = 1;
	for (uint8_t* p = block; p < end; ++p)
		*p = b++;
	EXPECT_EQ (block [1], 2);
	EXPECT_EQ (sblock [1], 1);
	
	EXPECT_TRUE (Port::Memory::is_private (block, BLOCK_SIZE));
	EXPECT_FALSE (Port::Memory::is_copy (sblock, block, BLOCK_SIZE));

	b = 2;
	for (uint8_t* p = sblock; p < send; ++p)
		*p = b++;
	EXPECT_EQ (block [1], 2);
	EXPECT_EQ (sblock [1], 3);
	
	EXPECT_TRUE (Port::Memory::is_private (sblock, BLOCK_SIZE));

	EXPECT_EQ (block, Port::Memory::copy (block, sblock, BLOCK_SIZE, 0));
	EXPECT_EQ (block [1], 3);

	EXPECT_FALSE (Port::Memory::is_private (block, BLOCK_SIZE));
	EXPECT_FALSE (Port::Memory::is_private (sblock, BLOCK_SIZE));
	EXPECT_TRUE (Port::Memory::is_copy (sblock, block, BLOCK_SIZE));
	EXPECT_TRUE (Port::Memory::is_copy (block, sblock, BLOCK_SIZE));

	Port::Memory::release (block, BLOCK_SIZE);
	Port::Memory::release (sblock, BLOCK_SIZE);
}

TEST_F (TestMemory, Move)
{
	size_t BLOCK_SIZE = 0x20000000;	// 512M
	size_t SHIFT = Port::Memory::SHARING_ASSOCIATIVITY;
	// Allocate block.
	size_t cb = BLOCK_SIZE + SHIFT;
	int* block = (int*)Port::Memory::allocate (0, cb, Memory::ZERO_INIT | Memory::RESERVED);
	ASSERT_TRUE (block);
	Port::Memory::commit (block, BLOCK_SIZE);

	int i = 0;
	for (int* p = block, *end = block + BLOCK_SIZE / sizeof (int); p != end; ++p)
		*p = ++i;

	// Shift block right on SHIFT
	cb = BLOCK_SIZE;
	int* shifted = (int*)Port::Memory::copy (block + SHIFT / sizeof (int), block, cb, Memory::EXACTLY | Memory::SRC_RELEASE);
	EXPECT_EQ (shifted, block + SHIFT / sizeof (int));
	i = 0;
	for (int* p = shifted, *end = shifted + BLOCK_SIZE / sizeof (int); p != end; ++p)
		EXPECT_EQ (*p, ++i);
	EXPECT_TRUE (Port::Memory::is_private (shifted, BLOCK_SIZE));

	// Allocate region to ensure that it is free.
	cb = SHIFT;
	EXPECT_TRUE (Port::Memory::allocate (block, cb, Memory::RESERVED | Memory::EXACTLY));
	Port::Memory::release (block, SHIFT);

	// Shift it back.
	cb = BLOCK_SIZE;
	EXPECT_EQ (block, (int*)Port::Memory::copy (block, shifted, cb, Memory::DST_ALLOCATE | Memory::EXACTLY | Memory::SRC_RELEASE));
	i = 0;
	for (int* p = block, *end = block + BLOCK_SIZE / sizeof (int); p != end; ++p)
		EXPECT_EQ (*p, ++i);
	EXPECT_TRUE (Port::Memory::is_private (block, BLOCK_SIZE));

	// Allocate region to ensure that it is free.
	cb = SHIFT;
	EXPECT_TRUE (Port::Memory::allocate (block + BLOCK_SIZE / sizeof (int), cb, Memory::RESERVED | Memory::EXACTLY));
	Port::Memory::release (block + BLOCK_SIZE / sizeof (int), SHIFT);

	Port::Memory::release (block, BLOCK_SIZE);
}

TEST_F (TestMemory, SmallBlock)
{
	size_t cb = sizeof (int);
	int* block = (int*)Port::Memory::allocate (nullptr, cb, Memory::ZERO_INIT);
	ASSERT_TRUE (block);
	EXPECT_TRUE (Port::Memory::is_private (block, sizeof (int)));
	*block = 1;
	{
		cb = sizeof (int);
		int* copy = (int*)Port::Memory::copy (0, block, cb, 0);
		ASSERT_TRUE (copy);
		EXPECT_EQ (*copy, *block);
		EXPECT_TRUE (Port::Memory::is_copy (copy, block, sizeof (int)));
		EXPECT_FALSE (Port::Memory::is_private (block, sizeof (int)));
		*copy = 2;
		EXPECT_EQ (*block, 1);
		Port::Memory::release (copy, sizeof (int));
	}
	{
		cb = sizeof (int);
		int* copy = (int*)Port::Memory::copy (0, block, cb, Memory::READ_ONLY);
		ASSERT_TRUE (copy);
		EXPECT_EQ (*copy, *block);
		EXPECT_TRUE (Port::Memory::is_copy (copy, block, sizeof (int)));
		Port::Memory::release (copy, sizeof (int));
	}
	Port::Memory::decommit (block, Port::Memory::FIXED_COMMIT_UNIT);
	Port::Memory::commit (block, sizeof (int));
	*block = 1;
	{
		EXPECT_TRUE (Port::Memory::is_private (block, sizeof (int)));
		cb = Port::Memory::FIXED_COMMIT_UNIT;
		int* copy = (int*)Port::Memory::copy (0, block, cb, Memory::SRC_DECOMMIT);
		EXPECT_EQ (*copy, 1);
		Port::Memory::commit (block, sizeof (int));
		*block = 2;
		EXPECT_EQ (*copy, 1);
		EXPECT_TRUE (Port::Memory::is_private (block, sizeof (int)));
		//EXPECT_TRUE (Port::Memory::is_private (copy, sizeof (int)));
		EXPECT_FALSE (Port::Memory::is_copy (copy, block, sizeof (int)));
		Port::Memory::release (copy, sizeof (int));
	}
	{
		cb = sizeof (int);
		int* copy = (int*)Port::Memory::copy (0, block, cb, Memory::SRC_RELEASE);
		ASSERT_EQ (copy, block);
	}
	Port::Memory::release (block, sizeof (int));
}

TEST_F (TestMemory, NotShared)
{
	static const char test_const [] = "test";
	size_t cb = sizeof (test_const);
	char* copy = (char*)Port::Memory::copy (0, (void*)test_const, cb, Memory::DST_ALLOCATE);
	static char test [sizeof (test_const)];
	cb = sizeof (test_const);
	Port::Memory::copy (test, copy, cb, 0);
	EXPECT_STREQ (test, test_const);
	Port::Memory::release (copy, sizeof (test_const));
}

}
