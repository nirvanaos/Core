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
#ifndef NIRVANA_ORB_CORE_STREAMOUT_H_
#define NIRVANA_ORB_CORE_STREAMOUT_H_
#pragma once

#include <CORBA/CORBA.h>
#include "../CoreInterface.h"
#include "IDL/ORB/GIOP.h"

namespace CORBA {
namespace Core {

class NIRVANA_NOVTABLE StreamOut
{
	DECLARE_CORE_INTERFACE

public:
	/// Marshal data.
	/// 
	/// \param align Data alignment
	/// \param element_size Element size.
	/// \param CDR_element_size CDR element size.
	/// \param element_count Count of elements.
	/// \param data Data pointer.
	/// \param allocated_size If this parameter is not zero, the stream may adopt the memory block.
	///   When stream adopts the memory block, it sets \p allocated_size to 0.
	virtual void write (size_t align, size_t element_size, size_t CDR_element_size,
		size_t element_count, void* data, size_t& allocated_size) = 0;

	/// \returns The data size include any alignment gaps.
	virtual size_t size () const = 0;

	/// Get stream buffer header.
	/// 
	/// \param hdr_size The header size in bytes.
	///   This parameter used only for check.
	///   At least \p hdr_size contiguous bytes must be available for writing.
	/// \returns Pointer to the stream begin.
	virtual void* header (size_t hdr_size) = 0;

	/// Rewind stream to the header size.
	/// Stream must drop all data after \p hdr_size bytes
	/// and set write position after hdr_size.
	/// 
	/// \param hdr_size Header size in bytes.
	virtual void rewind (size_t hdr_size) = 0;

	/// Start chunk.
	/// If chunk is already started, does nothing.
	virtual void chunk_begin () = 0;

	/// Closes current chunk, started with chunk_begin ().
	///
	/// Returns `true` if chunk was closed. `false` if no chunk was started.
	virtual bool chunk_end () = 0;

	/// \returns Current chunk size or -1 if no chunk was started.
	virtual Long chunk_size () const = 0;

	/// Marshal data helper.
	/// 
	/// \param align Data alignment
	/// \param size Data size.
	/// \param data Data pointer.
	void write_c (size_t align, size_t size, const void* data)
	{
		size_t zero = 0;
		write (align, size, size, 1, const_cast <void*> (data), zero);
	}

	template <class T>
	void write_one (const T& v)
	{
		write_c (IDL::Type <T>::CDR_align, IDL::Type <T>::CDR_size, &v);
	}

	/// Write GIOP message header.
	void write_message_header (unsigned GIOP_minor, GIOP::MsgType msg_type);

	/// Write string.
	/// 
	/// \tparam C The character type.
	/// \param [in,out] s The string.
	/// \param move If `true`, use move semantic.
	template <typename C>
	void write_string (Internal::StringT <C>& s, bool move);

	/// Write string.
	/// 
	/// \param s The string.
	void write_string_c (const Internal::StringView <Char>& s)
	{
		write_string (const_cast <IDL::String&> (
			static_cast <const IDL::String&> (s)), false);
	}

	/// Write size of sequence or string.
	/// \param size The size.
	void write_size (size_t size);

	/// Write CDR sequence.
	/// 
	/// \param align Data alignment.
	/// \param element_size Element size.
	/// \param CDR_element_size CDR element size.
	/// \param element_count Count of elements.
	/// \param data Pointer to the data with common-data-representation (CDR).
	/// \param allocated_size If this parameter is not zero, the request may adopt the memory block.
	///   If stream adopts the memory block, it sets \p allocated_size to 0.
	void write_seq (size_t align, size_t element_size, size_t CDR_element_size, size_t element_count, void* data,
		size_t& allocated_size);

	/// Write CDR sequence.
	///
	/// \tparam T Type of the sequence element.
	/// \param s The sequence.
	/// \param move If `true`, use move semantic.
	template <typename T>
	void write_seq (IDL::Sequence <T>& s, bool move);

	/// Write CDR sequence.
	///
	/// \tparam T Type of the sequence element.
	/// \param s The sequence.
	template <typename T>
	void write_seq (const IDL::Sequence <T>& s)
	{
		write_seq (const_cast <IDL::Sequence <T>&> (s), false);
	}

	void write_tagged (const IOP::TaggedProfileSeq& seq);

	void write_tagged (const IOP::TaggedComponentSeq& seq)
	{
		write_tagged (reinterpret_cast <const IOP::TaggedProfileSeq&> (seq));
	}

	void write_tagged (const IOP::ServiceContextList& seq)
	{
		write_tagged (reinterpret_cast <const IOP::TaggedProfileSeq&> (seq));
	}

	void write_id_name (TypeCode::_ptr_type tc);

	void write32 (ULong val)
	{
		write_c (alignof (ULong), sizeof (ULong), &val);
	}
};

template <typename C>
void StreamOut::write_string (Internal::StringT <C>& s, bool move)
{
	typedef typename IDL::Type <Internal::StringT <C> >::ABI ABI;
	ABI& abi = (ABI&)s;
	uint32_t size;
	C* ptr;
	if (abi.is_large ()) {
		size = (uint32_t)abi.large_size ();
		ptr = abi.large_pointer ();
	} else {
		size = (uint32_t)abi.small_size ();
		ptr = abi.small_pointer ();
	}
	write_size (size + 1); // String length includes the terminating zero.
	if (move && size <= ABI::SMALL_CAPACITY)
		move = false;
	size_t allocated = move ? abi.allocated () : 0;
	write (alignof (C), sizeof (C), sizeof (C), size + 1, ptr, allocated);
	if (move && !allocated)
		abi.reset ();
}

template <typename T>
void StreamOut::write_seq (IDL::Sequence <T>& s, bool move)
{
	typedef typename IDL::Type <IDL::Sequence <T> >::ABI ABI;
	ABI& abi = (ABI&)s;
	if (move) {
		write_seq (IDL::Type <T>::CDR_align, sizeof (T), IDL::Type <T>::CDR_size,
			abi.size, abi.ptr, abi.allocated);
		if (!abi.allocated)
			abi.reset ();
	} else {
		size_t zero = 0;
		write_seq (IDL::Type <T>::CDR_align, sizeof (T), IDL::Type <T>::CDR_size,
			abi.size, abi.ptr, zero);
	}
}

}
}

#endif
