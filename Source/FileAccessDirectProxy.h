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
#ifndef NIRVANA_CORE_FILEACCESSDIRECTPROXY_H_
#define NIRVANA_CORE_FILEACCESSDIRECTPROXY_H_
#pragma once

#include "FileAccessDirect.h"
#include <Nirvana/File_s.h>
#include <fnctl.h>

namespace Nirvana {
namespace Core {

class File;

class FileAccessDirectProxy :
	public CORBA::servant_traits <AccessDirect>::Servant <FileAccessDirectProxy>
{
public:
	FileAccessDirectProxy (File& file, uint_fast16_t flags);
	~FileAccessDirectProxy ();

	bool _non_existent () const noexcept
	{
		return !file_;
	}

	Nirvana::File::_ref_type file () const;

	void close ();

	FileSize size () const
	{
		check_exist ();

		return driver_.size ();
	}

	void size (FileSize new_size) const
	{
		check_exist ();

		if (flags_ & O_ACCMODE)
			driver_.size (new_size);
		else
			throw_NO_PERMISSION (make_minor_errno (EACCES));
	}

	void read (const FileLock& rel, const FileSize& pos, uint32_t size, LockType lock, bool nonblock, Bytes& data) const
	{
		check_exist ();

		if (!(flags_ & O_WRONLY))
			driver_.read (pos, size, data);
		else
			throw_NO_PERMISSION (make_minor_errno (EACCES));
	}

	void write (FileSize pos, const Bytes& data, const FileLock& rel, bool sync)
	{
		check_exist ();

		if (flags_ & O_ACCMODE) {
			if (flags_ & O_APPEND)
				pos = std::numeric_limits <FileSize>::max ();
			dirty_ = true;
			driver_.write (pos, data);
		} else
			throw_NO_PERMISSION (make_minor_errno (EACCES));
	}

	void flush ()
	{
		check_exist ();

		if (dirty_) {
			dirty_ = false;
			driver_.flush ();
		}
	}

	void lock (const FileLock& rem, const FileLock& add, bool nonblock)
	{
		// TODO: Implement
	}

	void get_lock (FileLock& fl) const
	{
		throw_NO_IMPLEMENT ();
	}

	uint_fast16_t flags () const noexcept
	{
		return flags_;
	}

	void flags (uint_fast16_t f);

	Access::_ref_type dup (uint_fast16_t mask, uint_fast16_t flags) const;

private:
	void check_exist () const;
	void check_flags (uint_fast16_t flags) const;

private:
	FileAccessDirect& driver_;
	Ref <File> file_;
	uint_fast16_t flags_;
	bool dirty_;
};

}
}

#endif
