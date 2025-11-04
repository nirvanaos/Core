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
#ifndef NIRVANA_CORE_FILEDESCRIPTORS_H_
#define NIRVANA_CORE_FILEDESCRIPTORS_H_
#pragma once

#include "FileDescriptorsContext.h"

namespace Nirvana {
namespace Core {

class FileDescriptors
{
public:
	static unsigned fd_add (Access::_ref_type&& access)
	{
		return context ().fd_add (std::move (access));
	}

	static void close (unsigned ifd)
	{
		context_for_fd ().close (ifd);
	}

	static size_t read (unsigned ifd, void* p, size_t size)
	{
		return context_for_fd ().read (ifd, p, size);
	}

	static void write (unsigned ifd, const void* p, size_t size)
	{
		context_for_fd ().write (ifd, p, size);
	}

	static bool seek (unsigned ifd, const FileOff& off, unsigned method, FileSize& pos)
	{
		return context_for_fd ().seek (ifd, off, method, pos);
	}

	static unsigned fcntl (unsigned ifd, int cmd, uintptr_t arg)
	{
		unsigned ret = 0;
		FileDescriptorsContext& data = context_for_fd ();
		switch (cmd) {
		case F_DUPFD:
			ret = data.dup (ifd, arg);
			break;

		case F_GETFD:
			ret = data.fd_flags (ifd);
			break;

		case F_SETFD:
			data.fd_flags (ifd, arg);
			break;

		case F_GETFL:
			ret = data.flags (ifd);
			break;

		case F_SETFL:
			data.flags (ifd, arg);
			break;

		case F_GETLK:
			if (!arg)
				throw_BAD_PARAM (make_minor_errno (EINVAL));
			data.get_lock (ifd, *(struct flock*)arg);
			break;

		case F_SETLK:
		case F_SETLKW:
			if (!arg)
				throw_BAD_PARAM (make_minor_errno (EINVAL));
			data.lock (ifd, *(struct flock*)arg, F_SETLKW == cmd);
			break;

		default:
			throw_BAD_PARAM (make_minor_errno (EINVAL));
		}
		return ret;
	}

	static void flush (unsigned ifd)
	{
		context_for_fd ().flush (ifd);
	}

	static void dup2 (unsigned src, unsigned dst)
	{
		context_for_fd ().dup2 (src, dst);
	}

	static bool isatty (unsigned ifd)
	{
		return context_for_fd ().isatty (ifd);
	}

	static void stat (unsigned ifd, FileStat& fs)
	{
		return context_for_fd ().stat (ifd, fs);
	}

private:
	static FileDescriptorsContext& context_for_fd ();
	static FileDescriptorsContext& context ();
};

}
}

#endif
