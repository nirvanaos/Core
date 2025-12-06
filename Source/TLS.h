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
#ifndef NIRVANA_CORE_TLS_H_
#define NIRVANA_CORE_TLS_H_
#pragma once

#include "MemContext.h"
#include "BitmapOps.h"

namespace Nirvana {

typedef void (*Deleter) (void*);

namespace Core {

/// Thread-local storage.
class TLS
{
	typedef BitmapOps::BitmapWord BitmapWord;
	static const unsigned BW_BITS = sizeof (BitmapWord) * 8;

	/// Limit of the user TLS indexes.
	static const unsigned USER_TLS_INDEXES_MIN = 128;

	static const size_t BITMAP_SIZE = (USER_TLS_INDEXES_MIN + BW_BITS - 1) / BW_BITS;
	static const unsigned USER_TLS_INDEXES_END = BITMAP_SIZE * sizeof (BitmapWord) * 8;

public:
	TLS () noexcept :
		free_count_ (USER_TLS_INDEXES_END)
	{
		std::fill_n (bitmap_, BITMAP_SIZE, ~0);
	}

	/// Allocate TLS index.
	///
	/// \returns New TLS index.
	/// \param deleter Optional deleter function.
	/// \throws CORBA::IMP_LIMIT if the limit of the user TLS indexes is reached.
	unsigned CS_alloc (Deleter deleter)
	{
		if (BitmapOps::acquire (&free_count_)) {
			for (BitmapWord* p = bitmap_;;) {
				int bit;
				do {
					bit = BitmapOps::clear_rightmost_one (p);
					if (bit >= 0) {
						unsigned idx = (unsigned)((p - bitmap_) * sizeof (BitmapWord) * 8) + bit;
						deleters_ [idx] = deleter;
						return idx + 1;
					}
				} while (std::end (bitmap_) != ++p);
			}
		} else
			throw_IMP_LIMIT ();
	}

	/// Release TLS index.
	///
	/// \param idx TLS index.
	/// \throws CORBA::BAD_PARAM if \p is not allocated.
	void CS_free (unsigned idx)
	{
		if (--idx >= USER_TLS_INDEXES_END)
			throw_BAD_PARAM ();
		size_t i = idx / BW_BITS;
		BitmapWord mask = (BitmapWord)1 << (idx % BW_BITS);
		if (BitmapOps::bit_set (bitmap_ + i, mask))
			BitmapOps::release (&free_count_);
		else
			throw_BAD_PARAM ();
	}

	/// Set TLS value.
	/// 
	/// \param idx TLS index.
	/// \param p Value.
	/// \throws CORBA::BAD_PARAM if \p idx is wrong.
	void CS_set (unsigned idx, void* p)
	{
		{
			if (--idx >= USER_TLS_INDEXES_END)
				throw_BAD_PARAM ();
			size_t i = idx / BW_BITS;
			BitmapWord mask = (BitmapWord)1 << (idx % BW_BITS);
			if (bitmap_ [i] & mask)
				throw_BAD_PARAM (); // This index is free

			MemContext::current ().tls ().set_value (idx, p, deleters_ [idx]);
		}
	}

	/// Get TLS value.
	/// 
	/// \param idx TLS index.
	/// \returns TLS value.
	static void* CS_get (unsigned idx) noexcept
	{
		MemContext* mc = MemContext::current_ptr ();
		if (mc) {
			TLS_Context* ctx = mc->tls_ptr ();
			if (ctx)
				return ctx->get_value (--idx);
		}
		return nullptr;
	}

private:
	BitmapWord bitmap_ [BITMAP_SIZE];
	Deleter deleters_ [USER_TLS_INDEXES_END];
	uint16_t free_count_;
};

}
}

#endif
