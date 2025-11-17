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
#include "FileAccessBuf.h"
#include "SyncContext.h"

namespace Nirvana {
namespace Core {

FileAccessBufData::FileAccessBufData (AccessDirect::_ptr_type acc) noexcept :
	dirty_begin_ (std::numeric_limits <size_t>::max ()),
	dirty_end_ (0),
	own_direct_ (acc)
{}

AccessDirect::_ref_type FileAccessBuf::dup_direct (AccessDirect::_ptr_type acc)
{
	return AccessDirect::_narrow (acc->dup (0, 0)->_to_object ());
}

void FileAccessBuf::dup_direct ()
{
	Servant::access (dup_direct (Servant::access ()));
	own_direct_ = Servant::access ();
	pflags () &= ~DIRECT_USED;
}

bool FileAccessBuf::is_sync_domain () noexcept
{
	switch (SyncContext::current ().sync_context_type ()) {
	case SyncContext::Type::SYNC_DOMAIN:
	case SyncContext::Type::SYNC_DOMAIN_SINGLETON:
	case SyncContext::Type::SINGLETON_TERM:
		return true;
	}
	return false;
}

AccessDirect::_ptr_type FileAccessBuf::access ()
{
	if (Servant::access () && !own_direct_.initialized ()) {
		if (pflags () & DIRECT_USED) {
			// We need to duplicate direct access to obtain private proxy.
			if (is_sync_domain ()) {
				if (own_direct_.initialize (DIRECT_DUP_TIMEOUT)) {
					auto wait_list = own_direct_.wait_list ();
					try {
						dup_direct ();
						wait_list->finish_construction (Servant::access ());
					} catch (...) {
						wait_list->on_exception ();
						throw;
					}
				} else
					own_direct_.get ();
			} else
				dup_direct ();
		} else
			own_direct_ = Servant::access ();
	}

	pflags () |= DIRECT_USED;
	return Servant::access ();
}

void FileAccessBuf::check () const
{
	if (!Servant::access ())
		throw_BAD_PARAM (make_minor_errno (EBADF));
}

void FileAccessBuf::check (const void* p) const
{
	check ();
	if (!p)
		throw_BAD_PARAM (make_minor_errno (EFAULT));
}

void FileAccessBuf::check_read () const
{
	if ((flags () & O_ACCMODE) == O_WRONLY)
		throw_NO_PERMISSION (make_minor_errno (EBADF));
}

void FileAccessBuf::check_write () const
{
	if ((flags () & O_ACCMODE) == O_RDONLY)
		throw_NO_PERMISSION (make_minor_errno (EBADF));
}

unsigned FileAccessBuf::check_flags (unsigned f) const
{
	if (f & O_DIRECT)
		throw_INV_FLAG (make_minor_errno (EINVAL));

	unsigned changes = flags () ^ f;
	if (changes & ~(O_APPEND | O_SYNC | O_TEXT | O_NONBLOCK | O_ACCMODE))
		throw_INV_FLAG (make_minor_errno (EINVAL));

	return changes;
}

Bytes::const_iterator FileAccessBuf::get_buffer_read (FileSize pos, size_t cb)
{
	while (pos < buf_pos () || pos >= (buf_pos () + buffer ().size ())) {
		// Out of the old buffer. We must drop old buffer and read a new.
		
		if (flush_internal ())
			continue; // Buffer position may be changed, do new check

		FileSize new_buf_pos = round_down (pos, (FileSize)blksz ());
		size_t new_buf_size = cb + (size_t)(pos - new_buf_pos);
		size_t mbs = min_buf_size ();
		if (new_buf_size <= mbs)
			new_buf_size = mbs;
		else
			new_buf_size = round_up (new_buf_size, (size_t)blksz ());
		Bytes new_buf;
		access ()->read (new_buf_pos, (uint32_t)limit32 (new_buf_size), new_buf);
		if (new_buf.empty ())
			return buffer ().end ();

		buf_pos (new_buf_pos);
		buffer (std::move (new_buf));

		break;
	}
	return buffer ().begin () + (size_t)(pos - buf_pos ());
}

Bytes::iterator FileAccessBuf::get_buffer_write (FileSize pos, size_t cb)
{
	for (;;) {
		bool drop_buffer = pos < buf_pos ();

		if (!drop_buffer) {
			FileSize buf_off = pos - buf_pos ();
			if (buf_off > buffer ().size ())
				drop_buffer = true;
			else if (buf_off >= min_buf_size ())
				drop_buffer = true;
		}

		if (drop_buffer) {

			if (flush_internal ())
				continue; // Buffer position may be changed, do new check

			FileSize new_buf_pos = round_down (pos, (FileSize)blksz ());
			size_t read_size = pos - new_buf_pos;
			if (read_size + cb < min_buf_size ())
				read_size = min_buf_size ();
			if (read_size) {
				Bytes new_buf;
				access ()->read (new_buf_pos, (uint32_t)read_size, new_buf);
				buffer (std::move (new_buf));
			} else
				buffer ().clear ();

			buf_pos (new_buf_pos);
		}

		break;
	}

	size_t buf_offset = pos - buf_pos ();
	size_t buf_end = limit32 (cb + buf_offset);
	if (buffer ().size () < buf_end) {
		size_t res_buf_size = buf_end;
		size_t mbs = min_buf_size ();
		if (res_buf_size <= mbs)
			res_buf_size = mbs;
		else
			res_buf_size = round_up (res_buf_size, (size_t)blksz ());
		buffer ().reserve (res_buf_size);
		buffer ().resize (buf_end);
	}

	if (dirty_begin_ > buf_offset)
		dirty_begin_ = buf_offset;
	if (dirty_end_ < buf_end)
		dirty_end_ = buf_end;

	return buffer ().begin () + buf_offset;
}

void FileAccessBuf::write_internal (const void* p, size_t cb)
{
	FileSize pos = position ();
	
	const uint8_t* src = (const uint8_t*)p;
	for (;;) {
		Bytes::iterator dst = get_buffer_write (pos, cb);
		size_t cbc = std::min (cb, (size_t)(buffer ().end () - dst));
		virtual_copy (src, cbc, &*dst);
		src += cbc;
		dst += cbc;
		pos += cbc;
		if (!(cb -= cbc))
			break;
	}

	Servant::position (pos);
}

bool FileAccessBuf::flush_internal (bool sync)
{
	assert (buffer ().size () <= std::numeric_limits <uint32_t>::max ());
	if (dirty ()) {
		if (!sync)
			sync = flags () & O_SYNC;

		assert (dirty_begin_ < buffer ().size () && dirty_end_ <= buffer ().size ());
		if (dirty_begin_ == 0 && dirty_end_ == buffer ().size ()) {
			reset_dirty ();
			access ()->write (buf_pos (), buffer (), sync);
		} else {
			Bytes tmp (buffer ().data () + dirty_begin_, buffer ().data () + dirty_end_);
			FileSize pos = buf_pos () + dirty_begin_;
			reset_dirty ();
			access ()->write (pos, tmp, sync);
		}
		return true;
	}
	return false;
}

void FileAccessBuf::shrink_buffer ()
{
	size_t mbs = min_buf_size ();
	while (buffer ().size () > mbs) {
		size_t drop_size = round_down (buffer ().size () - mbs, (size_t)blksz ());
		if (drop_size) {
			if (dirty_begin_ < drop_size && flush_internal ())
				continue; // Buffer position may be changed, do new check

			buffer ().erase (buffer ().begin (), buffer ().begin () + drop_size);
			buffer ().shrink_to_fit ();
			buf_pos (buf_pos () + drop_size);
			if (dirty ()) {
				dirty_begin_ -= drop_size;
				dirty_end_ -= drop_size;
			}
		}

		break;
	}
}

}
}

NIRVANA_GET_FACTORY (Nirvana::AccessBuf, Nirvana::Core::FileAccessBuf)
