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
#include "../Source/WeakRef.h"
#include "../Source/Chrono.h"
#include <gtest/gtest.h>

using namespace Nirvana::Core;

namespace TestWeakRef {

template <class Obj>
class TestWeakRef :
	public ::testing::Test
{
protected:
	TestWeakRef ()
	{}

	virtual ~TestWeakRef ()
	{}

	// If the constructor and destructor are not enough for setting up
	// and cleaning up each test, you can define the following methods:

	virtual void SetUp ()
	{
		// Code here will be called immediately after the constructor (right
		// before each test).
		ASSERT_TRUE (Nirvana::Core::Heap::initialize ());
	}

	virtual void TearDown ()
	{
		// Code here will be called immediately after each test (right
		// before the destructor).
		Nirvana::Core::Heap::terminate ();
	}
};

class SmallObject
{
public:
	SmallObject (const std::string& v) :
		v_ (v)
	{}

	const std::string& v () const
	{
		return v_;
	}

private:
	std::string v_;
};

class LargeObject
{
public:
	LargeObject (const std::string& v) :
		v1_ (v),
		v2_ (v)
	{}

	const std::string& v1 () const
	{
		return v1_;
	}

	const std::string& v2 () const
	{
		return v2_;
	}

private:
	std::string v1_, v2_;
};

typedef ::testing::Types <SmallObject, LargeObject> Types;

TYPED_TEST_SUITE (TestWeakRef, Types);

TYPED_TEST (TestWeakRef, ReleaseStrong)
{
	SharedRef <TypeParam> strong = SharedRef <TypeParam>::template create <ImplShared <TypeParam> > ("test");
	WeakRef <TypeParam> weak (strong);
	EXPECT_TRUE (weak);
	{
		SharedRef <TypeParam> s = weak.lock ();
		EXPECT_TRUE (s);
	}
	strong = nullptr;
	{
		SharedRef <TypeParam> s = weak.lock ();
		EXPECT_FALSE (s);
	}
	weak = nullptr;
}

TYPED_TEST (TestWeakRef, ReleaseWeak)
{
	SharedRef <TypeParam> strong = SharedRef <TypeParam>::template create <ImplShared <TypeParam> > ("test");
	WeakRef <TypeParam> weak (strong);
	EXPECT_TRUE (weak);
	SharedRef <TypeParam> s = weak.lock ();
	EXPECT_TRUE (s);
	strong = nullptr;
	weak = nullptr;
	s = nullptr;
}

}
