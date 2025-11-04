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
#ifndef NIRVANA_CORE_FILEDESCRIPTORSCONTEXT_H_
#define NIRVANA_CORE_FILEDESCRIPTORSCONTEXT_H_
#pragma once

#include <Nirvana/Nirvana.h>
#include <Nirvana/File.h>
#include <Nirvana/posix_defs.h>
#include <Nirvana/Shell.h>
#include "UserObject.h"
#include "UserAllocator.h"

namespace Nirvana {
namespace Core {

/// Context for the POSIX file descriptor operations.
class FileDescriptorsContext
{
	static const unsigned POSIX_CHANGEABLE_FLAGS = O_SYNC | O_APPEND | O_NONBLOCK;
	static const TimeBase::TimeT SETLOCK_WAIT_TIMEOUT = std::numeric_limits<TimeBase::TimeT>::max ();

public:
	FileDescriptorsContext ()
	{}

	unsigned fd_add (Access::_ref_type&& access, FileSize pos = 0)
	{
		// Allocate file descriptor cell
		size_t i = alloc_fd ();
		file_descriptors_ [i].attach (make_fd (std::move (access), pos));

		return (unsigned)(i + STD_CNT);
	}

	void close (unsigned ifd)
	{
		get_open_fd (ifd).close ();
		while (!file_descriptors_.empty () && file_descriptors_.back ().closed ()) {
			file_descriptors_.pop_back ();
		}
	}

	size_t read (unsigned ifd, void* p, size_t size)
	{
		return get_open_fd (ifd).ref ()->read (p, size);
	}

	void write (unsigned ifd, const void* p, size_t size)
	{
		get_open_fd (ifd).ref ()->write (p, size);
	}

	bool seek (unsigned ifd, const FileOff& off, unsigned method, FileSize& pos)
	{
		return get_open_fd (ifd).ref ()->seek (method, off, pos);
	}

	unsigned dup (unsigned ifd, unsigned start)
	{
		const DescriptorInfo& src = get_open_fd (ifd);
		size_t i = alloc_fd (start);
		file_descriptors_ [i].dup (src);
		return (unsigned)(i + STD_CNT);
	}

	void dup2 (unsigned ifd_src, unsigned ifd_dst)
	{
		const DescriptorInfo& src = get_open_fd (ifd_src);
		DescriptorInfo& dst = get_fd (ifd_dst);
		if (dst.closed ())
			dst.close ();
		dst.dup (src);
	}

	unsigned fd_flags (unsigned ifd)
	{
		return get_open_fd (ifd).fd_flags ();
	}

	void fd_flags (unsigned ifd, unsigned flags)
	{
		if (flags & ~FD_CLOEXEC)
			throw_INV_FLAG (make_minor_errno (EINVAL));

		get_open_fd (ifd).fd_flags (flags);
	}

	unsigned flags (unsigned ifd)
	{
		return get_open_fd (ifd).ref ()->flags ();
	}

	void flags (unsigned ifd, unsigned flags)
	{
		return get_open_fd (ifd).ref ()->flags (flags);
	}

	void flush (unsigned ifd)
	{
		get_open_fd (ifd).ref ()->flush ();
	}

	bool isatty (unsigned ifd)
	{
		return get_open_fd (ifd).ref ()->isatty ();
	}

	void get_lock (unsigned ifd, struct flock& lk)
	{
		get_open_fd (ifd).ref ()->get_lock (lk);
	}

	void lock (unsigned ifd, const struct flock& lk, bool wait)
	{
		TimeBase::TimeT timeout = wait ? SETLOCK_WAIT_TIMEOUT : 0;
		if (!get_open_fd (ifd).ref ()->lock (lk, timeout))
			throw_TRANSIENT (make_minor_errno (EAGAIN));
	}

	void stat (unsigned ifd, FileStat& fs)
	{
		get_open_fd (ifd).ref ()->stat (fs);
	}

	void set_spawn_files (const IDL::Sequence <SpawnFD>& files);
	void get_spawn_files (IDL::Sequence <SpawnFD>& files) const;

private:
	class NIRVANA_NOVTABLE Descriptor : public UserObject
	{
		static const unsigned PUSH_BACK_MAX = 3;

	public:
		void close () const;
		void stat (FileStat& fs) const;
		virtual size_t read (void* p, size_t size) = 0;
		virtual void write (const void* p, size_t size) = 0;
		virtual bool seek (unsigned method, FileOff off, FileSize& pos) = 0;
		virtual unsigned flags () const = 0;
		virtual void flags (unsigned fl) = 0;
		virtual void flush () = 0;
		virtual bool isatty () const = 0;
		virtual void get_spawn_fd (SpawnFD& fd) const;
		virtual bool lock (const struct flock& lk, TimeBase::TimeT timeout) const = 0;
		virtual void get_lock (struct flock& lk) const = 0;

		void add_descriptor_ref () noexcept
		{
			++descriptor_ref_cnt_;
		}

		unsigned remove_descriptor_ref () noexcept
		{
			assert (descriptor_ref_cnt_);
			return --descriptor_ref_cnt_;
		}

	protected:
		Descriptor (Access::_ref_type&& access) :
			access_base_ (std::move (access)),
			descriptor_ref_cnt_ (1)
		{}

		virtual ~Descriptor ()
		{}

		static LockType get_lock_type (int l_type);

	protected:
		Access::_ref_type access_base_;

	private:
		unsigned descriptor_ref_cnt_;
	};

	typedef ImplDynamicSync <Descriptor> DescriptorBase;
	typedef Ref <DescriptorBase> DescriptorRef;

	class DescriptorInfo
	{
	public:
		DescriptorInfo () noexcept :
			fd_flags_ (0)
		{}

		void close ()
		{
			if (0 == ref_->remove_descriptor_ref ())
				ref_->close ();
			ref_ = nullptr;
			fd_flags_ = 0;
		}

		void attach (DescriptorRef&& fd) noexcept
		{
			assert (!ref_);
			ref_ = std::move (fd);
		}

		void assign (const DescriptorRef& fd) noexcept
		{
			assert (!ref_);
			(ref_ = fd)->add_descriptor_ref ();
		}

		void dup (const DescriptorInfo& src) noexcept
		{
			assert (!ref_);
			(ref_ = src.ref_)->add_descriptor_ref ();
		}

		bool closed () const noexcept
		{
			return !ref_;
		}

		// For operations with context switch, hold descriptor reference
		DescriptorRef ref () const noexcept
		{
			return ref_;
		}

		// For simple operations, just a pointer
		Descriptor* ptr () const noexcept
		{
			return ref_;
		}

		unsigned fd_flags () const noexcept
		{
			return fd_flags_;
		}

		void fd_flags (unsigned fl) noexcept
		{
			fd_flags_ = fl;
		}

	private:
		DescriptorRef ref_;
		unsigned fd_flags_;
	};

	class DescriptorBuf;
	class DescriptorChar;
	class DescriptorDirect;

private:
	static DescriptorRef make_fd (Access::_ref_type&& access, FileSize pos);
	
	DescriptorInfo& get_fd (unsigned fd);
	
	const DescriptorInfo& get_fd (unsigned fd) const
	{
		return const_cast <FileDescriptorsContext*> (this)->get_fd (fd);
	}
	
	DescriptorInfo& get_open_fd (unsigned fd);
	size_t alloc_fd (unsigned start = 0);

private:
	enum StandardFileDescriptor
	{
		STD_IN,
		STD_OUT,
		STD_ERR,

		STD_CNT
	};

	DescriptorInfo std_file_descriptors_ [StandardFileDescriptor::STD_CNT];
	typedef std::vector <DescriptorInfo, UserAllocator <DescriptorInfo> > Descriptors;
	Descriptors file_descriptors_;
};

}
}

#endif
