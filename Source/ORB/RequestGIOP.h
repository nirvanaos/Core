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
#ifndef NIRVANA_ORB_CORE_REQUESTGIOP_H_
#define NIRVANA_ORB_CORE_REQUESTGIOP_H_
#pragma once

#include <CORBA/Server.h>
#include "StreamInEncap.h"
#include "StreamOutEncap.h"
#include <CORBA/IORequest_s.h>
#include "RqHelper.h"
#include "IDL/ORB/IIOP.h"
#include "CodeSetConverter.h"
#include "LocalAddress.h"
#include "IndirectMap.h"
#include "ReferenceRemote.h"
#include "TC_FactoryImpl.h"

namespace CORBA {
namespace Core {

/// Implements IORequest for GIOP messages
class NIRVANA_NOVTABLE RequestGIOP :
	public servant_traits <Internal::IORequest>::Servant <RequestGIOP>,
	protected RqHelper,
	public Internal::LifeCycleRefCnt <RequestGIOP>,
	public Nirvana::Core::UserObject
{
	static const size_t CHUNK_SIZE_LIMIT = 0x10000;

protected:
	void _add_ref () noexcept
	{
		ref_cnt_.increment ();
	}

	void _remove_ref () noexcept;

	friend class Internal::LifeCycleRefCnt <RequestGIOP>;

	template <class T> friend class CORBA::servant_reference;

public:
	/// \returns Response flags.
	unsigned response_flags () const noexcept
	{
		return response_flags_;
	}

	/// \returns The request deadline.
	const Nirvana::DeadlineTime deadline () const noexcept
	{
		return deadline_;
	}

	/// When request object is created, it saves the current memory context reference.
	///
	Nirvana::Core::MemContext& memory () const noexcept
	{
		return *mem_context_;
	}

	///@{
	/// Marshal/unmarshal data that meet the common data representation.

	/// Marshal CDR data.
	/// 
	/// \param align Data alignment
	/// \param size  Data size.
	/// \param data  Pointer to the data with common-data-representation (CDR).
	void marshal (size_t align, size_t size, const void* data)
	{
		check_align (align);
		if (marshal_chunk ())
			stream_out_->write_c (align, size, data);
	}

	/// Unmarshal CDR data.
	/// 
	/// \param align Data alignment
	/// \param size  Data size.
	/// \param data Pointer to the data buffer.
	/// \returns `true` if the byte order must be swapped after unmarshaling.
	bool unmarshal (size_t align, size_t size, void* data)
	{
		check_align (align);
		stream_in_->read (align, size, size, 1, data);
		return stream_in_->other_endian ();
	}

	/// Marshal CDR sequence or array.
	/// 
	/// \param align CDR data alignment.
	///   For the constructed types is an alignment of the first primitive type.
	/// 
	/// \param element_size Element size in memory.
	/// 
	/// \param CDR_element_size CDR element size.
	///   CDR_element_size <= element_size.
	/// 
	/// \param element_count Count of elements.
	/// 
	/// \param data Pointer to the data with common-data-representation (CDR).
	/// 
	/// \param allocated_size If this parameter is not zero, the request may adopt the memory block.
	///   When request adopts the memory block, it sets \p allocated_size to 0.
	/// 
	void marshal_seq (size_t align, size_t element_size, size_t CDR_element_size, size_t element_count, void* data,
		size_t& allocated_size)
	{
		check_align (align);
		if (!marshal_chunk ())
			return;

		stream_out_->write_seq (align, element_size, CDR_element_size, element_count, data, allocated_size);
	}

	/// Unmarshal CDR sequence.
	/// 
	/// \param align Data alignment.
	/// 
	/// \param element_size Element size in memory.
	/// 
	/// \param CDR_element_size CDR element size.
	///   CDR_element_size <= element_size.
	/// 
	/// \param [out] element_count Count of elements.
	/// 
	/// \param [inout] data Pointer to the allocated memory block with common-data-representation (CDR).
	///   The caller becomes an owner of this memory block.
	/// 
	/// \param [inout] allocated_size Size of the allocated memory block.
	///              
	/// \returns `true` if the byte order must be swapped after unmarshaling.
	/// 
	bool unmarshal_seq (size_t align, size_t element_size, size_t CDR_element_size,
		size_t& element_count, void*& data, size_t& allocated_size)
	{
		check_align (align);
		return stream_in_->read_seq (align, element_size, CDR_element_size,
			element_count, data, allocated_size);
	}

	///@}

	///@{
	/// Marshal/unmarshal sequences.

	/// Write marshaling sequence size.
	/// 
	/// \param element_count The sequence size.
	bool marshal_seq_begin (size_t element_count)
	{
		if (!marshal_chunk ())
			return false;
		stream_out_->write_size (element_count);
		return true;
	}

	/// Get unmarshaling sequence size.
	/// 
	/// \returns The sequence size.
	size_t unmarshal_seq_begin ()
	{
		return stream_in_->read_size ();
	}

	///@}

	///@{
	/// Marshal/unmarshal character data.

	void marshal_string (IDL::String& s, bool move)
	{
		if (marshal_chunk ())
			code_set_conv_->marshal_string (s, move, *stream_out_);
	}

	void unmarshal_string (IDL::String& s)
	{
		code_set_conv_->unmarshal_string (*stream_in_, s);
	}

	void marshal_wchar (size_t count, const WChar* data)
	{
		if (marshal_chunk ())
			code_set_conv_w_->marshal_char (count, data, *stream_out_);
	}

	void unmarshal_wchar (size_t count, WChar* data)
	{
		code_set_conv_w_->unmarshal_char (*stream_in_, count, data);
	}

	void marshal_wstring (IDL::WString& s, bool move)
	{
		if (marshal_chunk ())
			code_set_conv_w_->marshal_string (s, move, *stream_out_);
	}

	void unmarshal_wstring (IDL::WString& s)
	{
		code_set_conv_w_->unmarshal_string (*stream_in_, s);
	}

	void marshal_wchar_seq (WCharSeq& s, bool move)
	{
		if (marshal_chunk ())
			code_set_conv_w_->marshal_char_seq (s, move, *stream_out_);
	}

	void unmarshal_wchar_seq (WCharSeq& s)
	{
		code_set_conv_w_->unmarshal_char_seq (*stream_in_, s);
	}

	///@}

	///@{
	/// Object operations.

	/// Marshal interface.
	/// 
	/// \param itf The interface derived from Object.
	void marshal_interface (Internal::Interface::_ptr_type itf)
	{
		if (marshal_chunk ()) {
			if (itf)
				marshal_object (interface2object (itf));
			else
				stream_out_->write_string_c (nullptr); // Empty interface id
		}
	}

	/// Unmarshal interface.
	/// 
	/// \param rep_id The interface repository id.
	/// \param [out] itf The interface.
	/// 
	void unmarshal_interface (const IDL::String& interface_id, Internal::Interface::_ref_type& itf);

	/// Marshal TypeCode.
	/// 
	/// \param tc TypeCode.
	void marshal_type_code (TypeCode::_ptr_type tc)
	{
		if (!marshal_chunk ())
			return;

		IndirectMapMarshal map;
		marshal_type_code (*stream_out_, tc, map, 0);
	}

	/// Unmarshal TypeCode.
	/// 
	/// \param [out] TypeCode.
	void unmarshal_type_code (TypeCode::_ref_type& tc)
	{
		size_t start_pos = Nirvana::round_up (stream_in_->position (), (size_t)4);
		ULong kind = stream_in_->read32 ();
		if (INDIRECTION_TAG == kind) {
			Long off = stream_in_->read32 ();
			if (off >= -4)
				throw MARSHAL ();
			Interface* itf = top_level_tc_unmarshal_.find (start_pos + 4 + off);
			if (!itf)
				throw BAD_TYPECODE ();
			tc = TypeCode::_ptr_type (static_cast <TypeCode*> (itf));
		}

		if (!TC_FactoryImpl::get_simple_tc ((TCKind)kind, tc)) {
			tc = TC_FactoryImpl::unmarshal_type_code ((TCKind)kind, *stream_in_,
				top_level_tc_unmarshal_.get_allocator ().heap ());
			top_level_tc_unmarshal_.emplace (start_pos, &TypeCode::_ptr_type (tc));
		}
	}

	/// 	/// Marshal value type.
	/// 
	/// \param val  ValueBase.
	void marshal_value (Internal::Interface::_ptr_type val)
	{
		if (!marshal_op ())
			return;

		if (chunk_level_)
			stream_out_->chunk_end ();

		if (!val) {
			stream_out_->write32 (0);
			return;
		}

		marshal_value (value_type2base (val), val);
	}

	/// Unmarshal value type.
	/// 
	/// \param rep_id The value type repository id.
	/// \param [out] val The value interface.
	/// 
	void unmarshal_value (const IDL::String& interface_id, Internal::Interface::_ref_type& val);

	/// Marshal abstract interface.
	/// 
	/// \param itf The interface derived from AbstractBase.
	void marshal_abstract (Internal::Interface::_ptr_type itf)
	{
		if (!marshal_chunk ())
			return;

		if (itf) {
			// Downcast to AbstractBase
			AbstractBase::_ptr_type base = abstract_interface2base (itf);
			Object::_ref_type obj = base->_to_object ();
			if (obj) {
				Octet is_object = 1;
				stream_out_->write_c (1, 1, &is_object);
				marshal_object (obj);
			} else {
				ValueBase::_ref_type value = base->_to_value ();
				if (!value)
					Nirvana::throw_MARSHAL ();
				Octet is_object = 0;
				stream_out_->write_c (1, 1, &is_object);
				marshal_value (value, itf);
			}
		} else {
			// If there is no indication whether a nil abstract interface represents a
			// nil object reference or a null valuetype, it shall be encoded as a null valuetype.
			Octet is_object = 0;
			stream_out_->write_c (1, 1, &is_object);
			stream_out_->write32 (0);
		}
	}

	/// Unmarshal abstract interface.
	/// 
	/// \param rep_id The interface repository id.
	/// \param [out] itf The nterface.
	/// 
	void unmarshal_abstract (const IDL::String& interface_id, Internal::Interface::_ref_type& itf)
	{
		Octet is_object;
		stream_in_->read_one (is_object);
		if (is_object)
			unmarshal_interface (interface_id, itf);
		else
			unmarshal_value (interface_id, itf);
	}

	///@}

	///@{
	/// Callee operations.

	/// End of input data marshaling.
	/// 
	/// Marshaling resources may be released.
	virtual void unmarshal_end (bool no_check_size = false);

	/// Return exception to caller.
	/// Operation has move semantics so \p e may be cleared.
	virtual void set_exception (Any& e) = 0;

	/// Marks request as successful.
	virtual void success () = 0;

	///@}

	///@{
	/// Caller operations.

	/// Invoke request.
	/// 
	virtual void invoke () = 0;

	/// Unmarshal exception.
	/// \param [out] e If request completed with exception, receives the exception.
	///                Otherwise unchanged.
	/// \returns `true` if e contains an exception.
	virtual bool get_exception (Any& e) = 0;

	///@}

	///@{
	/// Asynchronous caller operations.

	/// Cancel the request.
	virtual void cancel () = 0;

	///@}

	virtual ~RequestGIOP ();

	///@{
	/// API for code set converters.

	StreamIn* stream_in () const
	{
		return stream_in_;
	}

	StreamOut* stream_out () const
	{
		return stream_out_;
	}

	void code_set_converter (servant_reference <CodeSetConverter>&& csc) noexcept
	{
		code_set_conv_ = std::move (csc);
	}

	void code_set_converter (servant_reference <CodeSetConverterW>&& csc) noexcept
	{
		code_set_conv_w_ = std::move (csc);
	}

	///@}

	Domain* domain () const noexcept
	{
		return domain_;
	}

protected:
	/// Constructor
	/// 
	/// \param GIOP_minor GIOP minor version number.
	/// \param response_flags Response flags
	/// \param domain Domain pointer for outgoing request, `nullptr` for incoming request.
	///        For the incoming requests, domain_ will be set in RequestIn::create_output ().
	RequestGIOP (unsigned GIOP_minor, unsigned response_flags, Domain* domain);

	RequestGIOP (const RequestGIOP&) = delete;
	RequestGIOP (RequestGIOP&&) = delete;

	virtual bool marshal_op () = 0;

	/// Set the size of message in the output message header.
	/// Must be called for remote messages.
	/// In the ESIOP we do not use the message size to allow > 4GB data transferring.
	void set_out_size ();

private:
	typedef std::basic_string <char, std::char_traits <char>, Nirvana::Core::HeapAllocator <char> >
		RepositoryId;

	typedef Nirvana::Core::MapUnorderedUnstable <RepositoryId, size_t, std::hash <RepositoryId>,
		std::equal_to <RepositoryId>, Nirvana::Core::HeapAllocator> IndirectRepIdMarshalCont;

	typedef Nirvana::Core::MapUnorderedUnstable <size_t, RepositoryId, std::hash <size_t>,
		std::equal_to <size_t>, Nirvana::Core::HeapAllocator> IndirectRepIdUnmarshalCont;

	class IndirectRepIdMarshal : public IndirectRepIdMarshalCont
	{
	public:
		IndirectRepIdMarshal () :
			IndirectRepIdMarshalCont (Nirvana::Core::Heap::user_heap ())
		{}
	};

	class IndirectRepIdUnmarshal : public IndirectRepIdUnmarshalCont
	{
	public:
		IndirectRepIdUnmarshal () :
			IndirectRepIdUnmarshalCont (Nirvana::Core::Heap::user_heap ())
		{}
	};

	typedef IndirectMap <IndirectRepIdMarshal, IndirectRepIdUnmarshal> IndirectMapRepId;

	void marshal_val_rep_id (Internal::String_in id);
	const RepositoryId& unmarshal_val_rep_id ();
	bool marshal_chunk ();
	void marshal_value (ValueBase::_ptr_type base, Interface::_ptr_type val);

protected:
	unsigned GIOP_minor_;
	unsigned response_flags_;
	Nirvana::DeadlineTime deadline_;
	Nirvana::Core::RefCounter ref_cnt_;
	Long chunk_level_;
	servant_reference <Domain> domain_;
	servant_reference <StreamIn> stream_in_;
	servant_reference <StreamOut> stream_out_;
	servant_reference <Nirvana::Core::MemContext> mem_context_;
	servant_reference <CodeSetConverter> code_set_conv_;
	servant_reference <CodeSetConverterW> code_set_conv_w_;
	ReferenceSet <Nirvana::Core::HeapAllocator> marshaled_DGC_references_;

private:

	static void marshal_type_code (StreamOut& stream, TypeCode::_ptr_type tc, IndirectMapMarshal& map, size_t parent_offset);
	void marshal_object (Object::_ptr_type obj);

	IndirectMapUnmarshal top_level_tc_unmarshal_;
	IndirectMapItf value_map_;
	IndirectMapRepId rep_id_map_;
	std::vector <ReferenceRemoteRef, Nirvana::Core::HeapAllocator <ReferenceRemoteRef> > references_to_confirm_;
};

}
}

#endif
