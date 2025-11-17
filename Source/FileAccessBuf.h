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
#ifndef NIRVANA_CORE_FILEACCESSBUF_H_
#define NIRVANA_CORE_FILEACCESSBUF_H_
#pragma once

#include <Nirvana/Nirvana.h>
#include <Nirvana/File_s.h>
#include <Nirvana/bitutils.h>
#include <Nirvana/posix_defs.h>
#include "virtual_copy.h"
#include "WaitableRef.h"
#include <algorithm>

namespace Nirvana {
namespace Core {

class FileAccessBufData
{
protected:
	FileAccessBufData (AccessDirect::_ptr_type acc = nullptr) noexcept;

	FileAccessBufData (const FileAccessBufData&) noexcept :
		FileAccessBufData (nullptr)
	{}

	FileAccessBufData (FileAccessBufData&&) noexcept :
		FileAccessBufData (nullptr)
	{}

	FileAccessBufData& operator = (const FileAccessBufData&) = delete;

	bool dirty () const noexcept
	{
		return dirty_begin_ < dirty_end_;
	}

	void reset_dirty () noexcept
	{
		dirty_begin_ = std::numeric_limits <size_t>::max ();
		dirty_end_ = 0;
	}

protected:
	size_t dirty_begin_, dirty_end_;
	WaitableRef <AccessDirect::_ptr_type> own_direct_;
};

/// Implementation of FileAccessBuf value type.
class FileAccessBuf :
	public IDL::traits <AccessBuf>::Servant <FileAccessBuf>,
	public FileAccessBufData
{
	typedef IDL::traits <AccessBuf>::Servant <FileAccessBuf> Servant;

	// Direct access is used by other object and must be duplicated before use.
	static const CORBA::UShort DIRECT_USED = 0x8000;

	// Direct access duplication deadline timeout for sync domains.
	static const TimeBase::TimeT DIRECT_DUP_TIMEOUT = 100 * TimeBase::MILLISECOND;

public:
	FileAccessBuf (const FileSize& position, const FileSize& buf_pos, AccessDirect::_ref_type&& access,
		Bytes&& buffer, uint32_t block_size, uint_fast16_t flags, std::array <char, 2> eol) :
		Servant (position, buf_pos, std::move (access), std::move (buffer), block_size, flags, eol),
		FileAccessBufData (Servant::access ())
	{
		assert (!(pflags () & DIRECT_USED));
	}

	// For unmarshal
	FileAccessBuf ()
	{}

	FileAccessBuf (const FileAccessBuf& src) :
		Servant (src),
		FileAccessBufData (src)
	{
		dup_direct ();
	}

	~FileAccessBuf ()
	{
		if (dirty ()) {
			try {
				flush (); //  Last chance
			} catch (...) {}
		}
	}

	void _marshal (CORBA::Internal::IORequest_ptr rq)
	{
		Servant::_marshal (rq);
		pflags () |= DIRECT_USED;
	}

	void _unmarshal (CORBA::Internal::IORequest_ptr rq)
	{
		Servant::_unmarshal (rq);
		own_direct_.reset ();
	}

	Access::_ref_type dup (uint_fast16_t mask, uint_fast16_t f) const
	{
		mask &= ~DIRECT_USED;
		f &= mask;
		f |= pflags () & ~mask;
		check_flags (f);
		AccessDirect::_ref_type acc = AccessDirect::_narrow (Servant::access ()->dup (O_ACCMODE, f)->_to_object ());
		return CORBA::make_reference <FileAccessBuf> (position (), buf_pos (), std::move (acc),
			Bytes (buffer ()), blksz (), f, eol ());
	}

	Nirvana::File::_ref_type file () const
	{
		check ();
		return Servant::access ()->file ();
	}

	FileSize size ()
	{
		check ();
		return access ()->size ();
	}

	void close ()
	{
		check ();
		flush_internal ();
		if (own_direct_.initialized ())
			own_direct_.get ()->close ();
		Servant::access (nullptr);
		own_direct_.reset ();
		buffer ().clear ();
		buffer ().shrink_to_fit ();
	}

	size_t read (void* p, size_t cb);
	void write (const void* p, size_t cb);

	FileSize position () const noexcept
	{
		check ();
		return Servant::position ();
	}

	void position (const FileSize& pos)
	{
		check ();
		Servant::position (pos);
	}

	void flush ()
	{
		check ();
		flush_internal (true);
	}

	uint_fast16_t flags () const noexcept
	{
		check ();
		return Servant::pflags () & ~DIRECT_USED;
	}

	void set_flags (uint_fast16_t mask, uint_fast16_t f)
	{
		check ();

		f = (f & mask) | (pflags () & ~mask);
		uint_fast16_t changes = check_flags (f);
		if (f & O_DIRECT)
			throw_INV_FLAG (make_minor_errno (EINVAL));

		if (!(pflags () & O_SYNC) && (f & O_SYNC) && (pflags () & O_ACCMODE)) {
			flush_internal ();
			access ()->flush ();
		}

		if (changes & O_ACCMODE)
			access ()->set_flags (O_ACCMODE, f);

		pflags (f);
	}

	LockType lock (const FileLock& fl, LockType tmin, const TimeBase::TimeT& timeout)
	{
		return access ()->lock (fl, tmin, timeout);
	}

	void get_lock (FileLock& fl)
	{
		access ()->get_lock (fl);
	}

private:
	struct BufPos
	{
		FileSize begin;
		FileSize end;
	};

	static AccessDirect::_ref_type dup_direct (AccessDirect::_ptr_type acc);
	void dup_direct ();
	static bool is_sync_domain () noexcept;

	// Always returns the private copy of AccessDirect.
	// Also set DIRECT_USED flag to indicate that current direct access is busy.
	AccessDirect::_ptr_type access ();

	void check () const;
	void check (const void* p) const;
	void check_read () const;
	void check_write () const;
	unsigned check_flags (unsigned f) const;

	bool flush_internal (bool sync = false);

	Bytes::const_iterator get_buffer_read (FileSize pos, size_t cb);
	Bytes::iterator get_buffer_write (FileSize pos, size_t cb);

	void write_internal (const void* p, size_t cb);

	size_t min_buf_size () const noexcept
	{
		// TODO: Adjust
		return blksz ();
	}

	void shrink_buffer ();

	static size_t limit32 (size_t size) noexcept
	{
		if (sizeof (size_t) > sizeof (uint32_t) && size > (size_t)std::numeric_limits <uint32_t>::max ())
			size = 0x80000000;
		return size;
	}

#ifndef NDEBUG

	FileSize buf_pos () const noexcept
	{
		assert (Servant::buf_pos () % Servant::blksz () == 0);
		return Servant::buf_pos ();
	}

	void buf_pos (FileSize pos) noexcept
	{
		assert (pos % Servant::blksz () == 0);
		Servant::buf_pos (pos);
	}

#endif
};

inline
size_t FileAccessBuf::read (void* p, size_t cb)
{
	check (p);
	check_read ();
	if (!cb)
		return 0;

	FileSize pos = position ();
	Bytes::const_iterator src = get_buffer_read (pos, cb);

	uint8_t* dst = (uint8_t*)p;

	if ((pflags () & O_TEXT) && eol () [0]) {

		// Translate line end

		while (buffer ().end () != src) {
			size_t cbr = buffer ().end () - src;
			if (cbr > cb)
				cbr = cb;
			do {
				
				size_t cbline = std::find (src, src + cbr, eol () [0]) - src;
				if (cbline) {
					virtual_copy (&*src, cbline, dst);
					dst += cbline;
					cbr -= cbline;
					cb -= cbline;
					src += cbline;
					pos += cbline;
				}

				if (cbr) {
					// Broken on EOL

					*(dst++) = '\n';
					++src;
					--cbr;
					--cb;
					++pos;

					if (eol () [1]) {
						
						// 2 character EOL
						if (buffer ().end () == src) {
							src = get_buffer_read (pos, cb);
							if (buffer ().end () == src)
								goto exitloop;
						}

						if (eol () [1] == *src) {
							++src;
							++Servant::position ();
						}
						
						cbr = buffer ().end () - src;
						if (cbr > cb)
							cbr = cb;
					}
				}
			} while (cbr);

			if (!cb)
				break;
			src = get_buffer_read (pos, cb);
		}

	} else {
		// No line end translation
		while (buffer ().end () != src) {
			size_t cbr = buffer ().end () - src;
			if (cbr > cb)
				cbr = cb;
			virtual_copy (&*src, cbr, dst);
			dst += cbr;
			cb -= cbr;
			pos += cbr;

			if (!cb)
				break;
			src = get_buffer_read (pos, cb);
		}
	}

exitloop:
	Servant::position (pos);
	shrink_buffer ();
	return dst - (uint8_t*)p;
}

inline
void FileAccessBuf::write (const void* p, size_t cb)
{
	check (p);
	check_write ();

	if (!cb)
		return;

	if ((pflags () & O_APPEND)
		|| (pflags () & (O_ACCMODE | O_SYNC)) == (O_WRONLY | O_SYNC)
	) {
		// Direct write

		Bytes buf ((const uint8_t*)p, (const uint8_t*)p + cb);
		
		if ((pflags () & O_TEXT) && eol () [0]) {
			for (auto it = buf.begin (); (it = std::find (it, buf.end (), '\n')) != buf.end ();) {
				*(it++) = eol () [0];
				if (eol () [1])
					it = buf.insert (it, eol () [1]) + 1;
			}
		}

		FileSize pos = (pflags () & O_APPEND) ? std::numeric_limits <FileSize>::max () : position ();
		access ()->write (pos, buf, pflags () & O_SYNC);
		if (!(pflags () & O_APPEND))
			Servant::position () += buf.size ();
	} else {
		// Buffered write

		if ((pflags () & O_TEXT) && eol () [0]) {

			const uint8_t* src = (const uint8_t*)p, * end = src + cb;
			const uint8_t* line_end = std::find (src, end, '\n');
			if (line_end != end) {

				if (eol () [1]) {
					Bytes buf;
					buf.reserve (cb + 1);
					for (;;) {
						buf.insert (buf.end (), src, line_end);
						buf.insert (buf.end (), eol ().begin (), eol ().end ());
						src = line_end + 1;
						line_end = std::find (src, end, '\n');
						if (line_end == end) {
							buf.insert (buf.end (), src, line_end);
							break;
						}
					}

					write_internal (buf.data (), buf.size ());

				} else {

					FileSize pos = position ();
					Bytes::iterator dst = get_buffer_write (pos, cb);

					for (;;) {
						if (src < line_end) {
							
							if (dst == buffer ().end ())
								dst = get_buffer_write (pos, cb);

							size_t cbc = std::min (line_end - src, buffer ().end () - dst);
							virtual_copy (src, cbc, &*dst);
							src += cbc;
							cb -= cbc;
							dst += cbc;
							pos += cbc;
						}

						if (src == line_end && line_end != end) {
							*(dst++) = eol () [0];
							++src;
							--cb;
							++pos;
							line_end = std::find (src, end, '\n');
						}

						if (!cb)
							break;
						if (dst == buffer ().end ())
							dst = get_buffer_write (pos, cb);
					}

					Servant::position (pos);
				}
			} else
				write_internal (p, cb);
		} else
			write_internal (p, cb);

		if (pflags () & O_SYNC)
			flush_internal ();

		shrink_buffer ();
	}
}

}
}

#endif
