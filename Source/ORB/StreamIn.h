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
#ifndef NIRVANA_ORB_CORE_STREAMIN_H_
#define NIRVANA_ORB_CORE_STREAMIN_H_
#pragma once

#include <CORBA/CORBA.h>
#include "IDL/ORB/GIOP.h"
#include "../CoreInterface.h"
#include "../Heap.h"

namespace CORBA {
namespace Core {

/// \brief Request input stream.
class NIRVANA_NOVTABLE StreamIn
{
	DECLARE_CORE_INTERFACE

public:
	///@{
	/// MARSHAL minor codes
	/// See CORBA 3.0: Chapter 15, General Inter-ORB Protocol
	/// 9.1.4 GIOP Message Header

	/// A MARSHAL exception with minor code 9 indicates that fewer bytes were present in a message
	/// than indicated by	the count. This condition can arise if the sender sends a message
	/// in fragments, and the receiver detects that the final fragment was received but contained
	/// insufficient data for all parameters to be unmarshaled.
	static const uint32_t MARSHAL_MINOR_FEWER = MAKE_OMG_MINOR (9);

	/// A MARSHAL exception with minor code 8 indicates that more bytes were present in a message than
	/// indicated by the count. Depending on the ORB implementation, this condition may be reported
	/// for the current message or the next message that is processed (when the receiver detects that
	/// the previous message is not immediately followed by the GIOP magic number).
	static const uint32_t MARSHAL_MINOR_MORE = MAKE_OMG_MINOR (8);

	///@}

	/// Read data into user-allocated buffer.
	/// 
	/// \param align Data alignment
	/// \param element_size Element size.
	/// \param CDR_element_size CDR element size.
	/// \param element_count Count of elements.
	/// \param buf  Pointer to the data buffer.
	///              If parameter `nullptr`, the stream must skip \p size bytes.
	virtual void read (size_t align, size_t element_size, size_t CDR_element_size,
		size_t element_count, void* buf) = 0;

	/// Allocate buffer and read.
	/// 
	/// \param align Data alignment
	/// \param element_size Element size.
	/// \param CDR_element_size CDR element size.
	/// \param element_count Count of elements.
	/// \param [out] size Allocated size on return.
	/// \returns Allocated data buffer.
	virtual void* read (size_t align, size_t element_size, size_t CDR_element_size,
		size_t element_count, size_t& size) = 0;

	/// Set the remaining data size.
	/// 
	/// Initially, the data size is set to numeric_limits<size_t>::max().
	/// Then it may be reduced to the real data size.
	/// 
	/// \param size The remaining data size in bytes.
	///   This count includes any alignment gaps and must match the size of the actual request
	///   parameters (plus any final padding bytes that may follow the parameters to have a fragment
	///   message terminate on an 8 - byte boundary).
	virtual void set_size (size_t size) = 0;

	/// End of the data reading.
	/// 
	/// Stream can release resources, close socket etc.
	/// 
	/// \returns Count of the remaining bytes in stream.
	virtual size_t end () = 0;

	/// \returns Current position.
	virtual size_t position () const = 0;

	/// \returns The current chunk remaining size.
	virtual size_t chunk_tail () const;

	/// Skip all chunks until the tag.
	/// 
	/// \returns Tag.
	virtual CORBA::Long skip_chunks ();

	/// Set stream endian.
	/// 
	/// \param little `true` for little endian, `false` for big endian.
	void little_endian (bool little) noexcept
	{
		Nirvana::endian stream_endian = little ? Nirvana::endian::little : Nirvana::endian::big;
		other_endian_ = Nirvana::endian::native != stream_endian;
	}

	/// \returns `true` if the stream endian is differ from the native platform endian.
	bool other_endian () const noexcept
	{
		return other_endian_;
	}

	/// Read GIOP message header and set stream endian.
	/// 
	/// \param [out] msg_hdr GIOP message header.
	/// \throw MARSHAL if header can't be read or has invalid signature.
	void read_message_header (GIOP::MessageHeader_1_3& msg_hdr);

	/// Read string.
	/// 
	/// \tparam C The character type.
	/// \param  s The string.
	template <typename C, class A>
	void read_string (std::basic_string <C, std::char_traits <C>, A>& s);

	template <typename C>
	void read_string (Internal::StringT <C>& s);

	/// Read size of sequence or string.
	/// \returns The size.
	size_t read_size ();

	/// Read CDR sequence.
	/// 
	/// \param align Data alignment
	/// \param element_size Element size.
	/// \param CDR_element_size CDR element size.
	/// \param [out] element_count Count of elements.
	/// \param [in,out] data Pointer to the allocated memory block with common-data-representation (CDR).
	///             The caller is owner of this memory block.
	/// \param [in,out] allocated_size Size of the allocated memory block.
	///              
	/// \returns `true` if the byte order must be swapped after unmarshaling.
	bool read_seq (size_t align, size_t element_size, size_t CDR_element_size,
		size_t& element_count, void*& data, size_t& allocated_size);

	/// Read CDR sequence.
	/// 
	/// \typeparam T The sequence element type.
	/// \param s The sequence.
	template <typename T>
	void read_seq (IDL::Sequence <T>& s);

	void read_tagged (IOP::TaggedProfileSeq& seq);

	void read_tagged (IOP::TaggedComponentSeq& seq)
	{
		read_tagged (reinterpret_cast <IOP::TaggedProfileSeq&> (seq));
	}

	void read_tagged (IOP::ServiceContextList& seq)
	{
		read_tagged (reinterpret_cast <IOP::TaggedProfileSeq&> (seq));
	}

	ULong read32 ()
	{
		ULong val;
		read_one (val);
		return val;
	}

	template <class T>
	void read_one (T& v)
	{
		read (IDL::Type <T>::CDR_align, sizeof (T), IDL::Type <T>::CDR_size, 1, &v);
		if (other_endian ())
			IDL::Type <T>::byteswap (v);
	}

	void chunk_mode (bool on) noexcept
	{
		chunk_mode_ = on;
	}

	bool chunk_mode () const noexcept
	{
		return chunk_mode_;
	}

protected:
	StreamIn () :
		other_endian_ (false),
		chunk_mode_ (false)
	{}

	StreamIn (const StreamIn&) = default;
	StreamIn (StreamIn&&) = default;

protected:
	bool other_endian_;
	bool chunk_mode_;
};

template <typename C, class A>
void StreamIn::read_string (std::basic_string <C, std::char_traits <C>, A>& s)
{
	size_t size = read_size ();
	if (!size)
		throw MARSHAL (); // String length includes the terminating zero.
	s.resize (size - 1);
	read (alignof (C), sizeof (C), sizeof (C), size, &*s.begin ());
	if (s.data () [size - 1]) // Not zero-terminated
		throw MARSHAL ();
}

template <typename C>
void StreamIn::read_string (Internal::StringT <C>& s)
{
	typedef typename IDL::Type <Internal::StringT <C> >::ABI ABI;

	size_t size = read_size ();
	if (!size)
		throw MARSHAL (); // String length includes the terminating zero.

	ABI& abi = (ABI&)s;
	if (size <= ABI::SMALL_CAPACITY + 1) {
		if (abi.is_large ()) {
			read (alignof (C), sizeof (C), sizeof (C), size, abi.large_pointer ());
			abi.large_size (size - 1);
		} else {
			read (alignof (C), sizeof (C), sizeof (C), size, abi.small_pointer ());
			abi.small_size (size - 1);
		}
	} else {
		size_t allocated = abi.allocated ();
		if (allocated >= size * sizeof (C)) {
			read (alignof (C), sizeof (C), sizeof (C), size, abi.large_pointer ());
			abi.large_size (size - 1);
		} else {
			size_t allocated_size;
			void* data = read (alignof (C), sizeof (C), sizeof (C), size, allocated_size);
			if (allocated)
				Nirvana::Core::Heap::user_heap ().release (abi.large_pointer (), allocated);
			abi.large_size (size - 1);
			abi.large_pointer ((C*)data);
			abi.allocated (allocated_size);
		}
	}

	if (abi._ptr () [size - 1]) // Not zero-terminated
		throw MARSHAL ();

	if (sizeof (C) > 1 && other_endian ())
		for (C& c : s) {
			IDL::Type <C>::byteswap (c);
		}
}

template <typename T>
void StreamIn::read_seq (IDL::Sequence <T>& s)
{
	typedef typename IDL::Type <IDL::Sequence <T> >::ABI ABI;
	ABI& abi = (ABI&)s;
	bool other_endian = read_seq (IDL::Type <T>::CDR_align, sizeof (T), IDL::Type <T>::CDR_size,
		abi.size, (void*&)abi.ptr, abi.allocated);
	
	if (sizeof (typename IDL::Type <T>::ABI) > 1 && other_endian)
		for (T* p = s.data (), *e = p + s.size (); p != e; ++p) {
			IDL::Type <T>::byteswap (*p);
		}
}

}
}

#endif
