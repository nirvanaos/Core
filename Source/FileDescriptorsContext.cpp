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
#include "FileDescriptorsContext.h"
#include "MapUnorderedUnstable.h"
#include "virtual_copy.h"
#include <Nirvana/posix_defs.h>
#include "ORB/Services.h"
#include <queue>

namespace Nirvana {
namespace Core {

void FileDescriptorsContext::Descriptor::get_spawn_fd (SpawnFD& fd) const
{
	fd.access (access_base_->dup (0, 0));
}

void FileDescriptorsContext::Descriptor::close () const
{
	access_base_->close ();
}

void FileDescriptorsContext::Descriptor::stat (FileStat& fs) const
{
	access_base_->file ()->stat (fs);
}

LockType FileDescriptorsContext::Descriptor::get_lock_type (int l_type)
{
	switch (l_type) {
	case F_UNLCK:
		return LockType::LOCK_NONE;
	case F_RDLCK:
		return LockType::LOCK_SHARED;
	case F_WRLCK:
		return LockType::LOCK_EXCLUSIVE;
	default:
		throw_BAD_PARAM (make_minor_errno (EINVAL));
	}
}

class FileDescriptorsContext::DescriptorBuf : public DescriptorBase
{
public:
	DescriptorBuf (Access::_ref_type&& base, AccessBuf::_ref_type&& access) :
		DescriptorBase (std::move (base)),
		access_ (std::move (access))
	{}

	virtual size_t read (void* p, size_t size) override;
	virtual void write (const void* p, size_t size) override;
	virtual bool seek (unsigned method, FileOff off, FileSize& pos) override;
	virtual unsigned flags () const override;
	virtual void flags (unsigned fl) override;
	virtual void flush () override;
	virtual bool isatty () const override;
	virtual bool lock (const struct flock& lk, TimeBase::TimeT timeout) const override;
	virtual void get_lock (struct flock& lk) const override;

private:
	FileSize calc_pos (unsigned method, FileOff off) const;
	void conv_lock (const struct flock& from, FileLock& to) const;

private:
	const AccessBuf::_ref_type access_;
};

class FileDescriptorsContext::DescriptorChar : public DescriptorBase
{
public:
	DescriptorChar (Access::_ref_type&& base, AccessChar::_ref_type&& access) :
		DescriptorBase (std::move (base)),
		access_ (std::move (access)),
		flags_ (access_->flags ())
	{
		if ((flags_ & O_ACCMODE) != O_WRONLY)
			access_->connect_pull_consumer (nullptr);
	}

	virtual size_t read (void* p, size_t size) override;
	virtual void write (const void* p, size_t size) override;
	virtual bool seek (unsigned method, FileOff off, FileSize& pos) override;
	virtual unsigned flags () const override;
	virtual void flags (unsigned fl) override;
	virtual void flush () override;
	virtual bool isatty () const override;
	virtual bool lock (const struct flock& lk, TimeBase::TimeT timeout) const override;
	virtual void get_lock (struct flock& lk) const override;

private:
	typedef std::queue <char> Buffer;
	const AccessChar::_ref_type access_;
	Buffer buffer_;
	unsigned flags_;
};

class FileDescriptorsContext::DescriptorDirect : public DescriptorBase
{
public:
	DescriptorDirect (Access::_ref_type&& base, AccessDirect::_ref_type&& access, FileSize pos) :
		DescriptorBase (std::move (base)),
		access_ (std::move (access)),
		pos_ (pos)
	{}

	virtual size_t read (void* p, size_t size) override;
	virtual void write (const void* p, size_t size) override;
	virtual bool seek (unsigned method, FileOff off, FileSize& pos) override;
	virtual unsigned flags () const override;
	virtual void flags (unsigned fl) override;
	virtual void flush () override;
	virtual bool isatty () const override;
	virtual bool lock (const struct flock& lk, TimeBase::TimeT timeout) const override;
	virtual void get_lock (struct flock& lk) const override;
	virtual void get_spawn_fd (SpawnFD& fd) const override;

private:
	FileSize calc_pos (unsigned method, FileOff off) const;
	void conv_lock (const struct flock& from, FileLock& to) const;

private:
	const AccessDirect::_ref_type access_;
	FileSize pos_;
};

void FileDescriptorsContext::DescriptorDirect::get_spawn_fd (SpawnFD& fd) const
{
	Descriptor::get_spawn_fd (fd);
	fd.pos (pos_);
}

size_t FileDescriptorsContext::alloc_fd (unsigned start)
{
	size_t i;
	if (start >= (unsigned)STD_CNT)
		i = start - (unsigned)STD_CNT;
	else
		i = 0;
	if (i < file_descriptors_.size ()) {
		auto it = file_descriptors_.begin ();
		for (; it != file_descriptors_.end (); ++it) {
			if (it->closed ())
				break;
		}
		i = it - file_descriptors_.begin ();
	}
	if (i > (unsigned)std::numeric_limits <int>::max () - (unsigned)STD_CNT)
		throw_IMP_LIMIT (make_minor_errno (EMFILE));
	if (i >= file_descriptors_.size ())
		file_descriptors_.resize (i + 1);
	return i;
}

FileDescriptorsContext::DescriptorInfo& FileDescriptorsContext::get_fd (unsigned ifd)
{
	if (ifd < StandardFileDescriptor::STD_CNT) {
		DescriptorInfo& desc = std_file_descriptors_ [ifd];
		if (desc.closed ()) {
			Nirvana::File::_ref_type console = Nirvana::File::_narrow (
				CORBA::Core::Services::bind (CORBA::Core::Services::Console));
			assert (console);
			desc.attach (make_fd (console->open (((ifd == 0) ? O_RDONLY : O_WRONLY) | O_TEXT, 0), 0));
		}

		return desc;
	}

	ifd -= StandardFileDescriptor::STD_CNT;
	if (ifd >= file_descriptors_.size ())
		throw_BAD_PARAM (make_minor_errno (EBADF));
	return file_descriptors_ [ifd];
}

FileDescriptorsContext::DescriptorInfo& FileDescriptorsContext::get_open_fd (unsigned ifd)
{
	DescriptorInfo& fd = get_fd (ifd);
	if (fd.closed ())
		throw_BAD_PARAM (make_minor_errno (EBADF));
	return fd;
}

FileDescriptorsContext::DescriptorRef FileDescriptorsContext::make_fd (
	Access::_ref_type&& access, FileSize pos)
{
	CORBA::AbstractBase::_ptr_type base = access;
	CORBA::ValueBase::_ref_type val = base->_to_value ();
	if (val) {
		AccessBuf::_ref_type ab = AccessBuf::_downcast (val);
		if (!ab)
			throw_UNKNOWN (make_minor_errno (EIO));
		return CORBA::make_reference <DescriptorBuf> (std::move (access), std::move (ab));
	} else {
		CORBA::Object::_ref_type obj = base->_to_object ();
		AccessChar::_ref_type ac = AccessChar::_narrow (obj);
		if (ac)
			return CORBA::make_reference <DescriptorChar> (std::move (access), std::move (ac));
		AccessDirect::_ref_type ad = AccessDirect::_narrow (obj);
		if (ad)
			return CORBA::make_reference <DescriptorDirect> (std::move (access), std::move (ad), pos);
	}
	throw_UNKNOWN (make_minor_errno (EIO));
}

void FileDescriptorsContext::set_spawn_files (const IDL::Sequence <SpawnFD>& files)
{
	size_t max = 0;
	for (const auto& f : files) {
		for (auto d : f.descriptors ()) {
			if (max < d)
				max = d;
		}
	}
	if (max >= StandardFileDescriptor::STD_CNT) {
		if (max > std::numeric_limits <int>::max ())
			throw_BAD_PARAM ();
		file_descriptors_.resize (max + 1 - StandardFileDescriptor::STD_CNT);
	}
	for (const auto& f : files) {
		DescriptorRef fd = make_fd (f.access (), f.pos ());
		fd->remove_descriptor_ref ();
		for (auto d : f.descriptors ()) {
			DescriptorInfo& fdr = get_fd ((unsigned)d);
			if (!fdr.closed ())
				throw_BAD_PARAM ();
			fdr.assign (fd);
		}
	}
}

void FileDescriptorsContext::get_spawn_files (IDL::Sequence <SpawnFD>& files) const
{
	using FileMap = MapUnorderedUnstable <Descriptor*, IDL::Sequence <int>,
		std::hash <Descriptor*>, std::equal_to <Descriptor*>, UserAllocator>;

	FileMap file_map;
	for (unsigned ifd = 0, end = StandardFileDescriptor::STD_CNT + file_descriptors_.size (); ifd < end; ++ifd) {
		const DescriptorInfo& d = get_fd (ifd);
		if (!d.closed () && !(d.fd_flags () & FD_CLOEXEC)) {
			file_map [d.ptr ()].push_back (ifd);
		}
	}

	for (auto& f : file_map) {
		SpawnFD fd;
		f.first->get_spawn_fd (fd);
		fd.descriptors (std::move (f.second));
		files.push_back (std::move (fd));
	}
}

size_t FileDescriptorsContext::DescriptorBuf::read (void* p, size_t size)
{
	return access_->read (p, size);
}

size_t FileDescriptorsContext::DescriptorChar::read (void* p, size_t size)
{
	if ((flags_ & O_ACCMODE) == O_WRONLY)
		throw_NO_PERMISSION (make_minor_errno (EBADF));

	if (!size)
		return 0;

	size_t cb = 0;
	for (;;) {
		while (size && !buffer_.empty ()) {
			*(char*)p = buffer_.front ();
			buffer_.pop ();
			++cb;
			--size;
		}

		if (cb)
			break;

		CORBA::Any evt;
		if (flags_ & O_NONBLOCK) {
			bool has_event;
			evt = access_->try_pull (has_event);
			if (!has_event)
				throw_TRANSIENT (make_minor_errno (EAGAIN));
		} else
			evt = access_->pull ();

		const IDL::String* ps;
		if (evt >>= ps) {
			if (ps->empty ())
				break;
			for (char c : *ps) {
				buffer_.push (c);
			}
		} else {
			AccessChar::Error error;
			if (evt >>= error)
				throw_COMM_FAILURE (make_minor_errno (error.error ()));
		}
	}

	return cb;
}

size_t FileDescriptorsContext::DescriptorDirect::read (void* p, size_t size)
{
	if (!size)
		return 0;

	FileSize pos = pos_;
	FileSize next = pos + size;
	if (next < pos)
		throw_BAD_PARAM (make_minor_errno (EOVERFLOW));

	Bytes data;
	pos_ = next;
	try {
		access_->read (pos, size, data);
	} catch (...) {
		pos_ = pos;
		throw;
	}
	if (!data.empty ()) {
		assert (data.size () <= size);
		virtual_copy (data.data (), data.size (), p, Memory::SRC_DECOMMIT);
	}
	return data.size ();
}

void FileDescriptorsContext::DescriptorBuf::write (const void* p, size_t size)
{
	access_->write (p, size);
}

void FileDescriptorsContext::DescriptorChar::write (const void* p, size_t size)
{
	const char* pc = (const char*)p;
	if (!pc [size])
		access_->write (CORBA::Internal::StringView <char> (pc, size));
	else
		access_->write (IDL::String (pc, size));
}

void FileDescriptorsContext::DescriptorDirect::write (const void* p, size_t size)
{
	if (!size)
		return;

	FileSize pos = pos_;
	FileSize next = pos + size;
	if (next < pos)
		throw_BAD_PARAM (make_minor_errno (EOVERFLOW));

	Bytes data ((const uint8_t*)p, (const uint8_t*)p + size);
	pos_ = next;
	try {
		access_->write (pos, data, false);
	} catch (...) {
		pos_ = pos;
		throw;
	}
}

bool FileDescriptorsContext::DescriptorBuf::seek (unsigned method, FileOff off, FileSize& pos)
{
	pos = calc_pos (method, off);
	access_->position (pos);
	return true;
}

FileSize FileDescriptorsContext::DescriptorBuf::calc_pos (unsigned method, FileOff off) const
{
	FileSize pos;
	switch (method) {
		case SEEK_SET:
			pos = 0;
			break;

		case SEEK_CUR:
			pos = access_->position ();
			break;

		case SEEK_END:
			pos = access_->size ();
			break;

		default:
			throw_BAD_PARAM (make_minor_errno (EINVAL));
	}

	if (off < 0) {
		if (pos < (FileSize)(-off))
			throw_BAD_PARAM (make_minor_errno (EOVERFLOW));
	} else {
		if (std::numeric_limits <FileSize>::max () - pos < (FileSize)off)
			throw_BAD_PARAM (make_minor_errno (EOVERFLOW));
	}

	return pos + off;
}

FileSize FileDescriptorsContext::DescriptorDirect::calc_pos (unsigned method, FileOff off) const
{
	FileSize pos;
	switch (method) {
		case SEEK_SET:
			pos = 0;
			break;

		case SEEK_CUR:
			pos = pos_;
			break;

		case SEEK_END:
			pos = access_->size ();
			break;

		default:
			throw_BAD_PARAM (make_minor_errno (EINVAL));
	}

	if (off < 0) {
		if (pos < (FileSize)(-off))
			throw_BAD_PARAM (make_minor_errno (EOVERFLOW));
	} else {
		if (std::numeric_limits <FileSize>::max () - pos < (FileSize)off)
			throw_BAD_PARAM (make_minor_errno (EOVERFLOW));
	}

	return pos + off;
}

bool FileDescriptorsContext::DescriptorDirect::seek (unsigned method, FileOff off, FileSize& pos)
{
	pos = pos_ = calc_pos (method, off);
	return true;
}

bool FileDescriptorsContext::DescriptorChar::seek (unsigned method, FileOff off, FileSize& pos)
{
	return false;
}

unsigned FileDescriptorsContext::DescriptorBuf::flags () const
{
	return access_base_->flags ();
}

void FileDescriptorsContext::DescriptorBuf::flags (unsigned fl)
{
	access_->set_flags (POSIX_CHANGEABLE_FLAGS, fl);
}

unsigned FileDescriptorsContext::DescriptorChar::flags () const
{
	return flags_;
}

void FileDescriptorsContext::DescriptorChar::flags (unsigned fl)
{
	unsigned chg = (fl ^ flags_) & POSIX_CHANGEABLE_FLAGS;
	if (chg != O_NONBLOCK)
		access_->set_flags (POSIX_CHANGEABLE_FLAGS, fl);
	flags_ = (flags_ & ~POSIX_CHANGEABLE_FLAGS) | (fl & POSIX_CHANGEABLE_FLAGS);
}

unsigned FileDescriptorsContext::DescriptorDirect::flags () const
{
	return access_base_->flags ();
}

void FileDescriptorsContext::DescriptorDirect::flags (unsigned fl)
{
	access_base_->set_flags (POSIX_CHANGEABLE_FLAGS, fl);
}

void FileDescriptorsContext::DescriptorBuf::flush ()
{
	access_->flush ();
}

void FileDescriptorsContext::DescriptorChar::flush ()
{
}

void FileDescriptorsContext::DescriptorDirect::flush ()
{
	access_->flush ();
}

bool FileDescriptorsContext::DescriptorBuf::isatty () const
{
	return false;
}

bool FileDescriptorsContext::DescriptorChar::isatty () const
{
	return access_->isatty ();
}

bool FileDescriptorsContext::DescriptorDirect::isatty () const
{
	return false;
}

void FileDescriptorsContext::DescriptorBuf::conv_lock (const struct flock& from, FileLock& to) const
{
	to.start (calc_pos (from.l_whence, from.l_start));
	to.len (from.l_len);
	to.type (get_lock_type (from.l_type));
}

bool FileDescriptorsContext::DescriptorBuf::lock (const struct flock& lk, TimeBase::TimeT timeout) const
{
	FileLock file_lock;
	conv_lock (lk, file_lock);
	return access_->lock (file_lock, file_lock.type (), timeout) == file_lock.type ();
}

bool FileDescriptorsContext::DescriptorChar::lock (const struct flock& lk, TimeBase::TimeT timeout) const
{
	throw_BAD_PARAM (make_minor_errno (EINVAL));
}

void FileDescriptorsContext::DescriptorDirect::conv_lock (const struct flock& from, FileLock& to) const
{
	to.start (calc_pos (from.l_whence, from.l_start));
	to.len (from.l_len);
	to.type (get_lock_type (from.l_type));
}

bool FileDescriptorsContext::DescriptorDirect::lock (const struct flock& lk, TimeBase::TimeT timeout) const
{
	FileLock file_lock;
	conv_lock (lk, file_lock);
	return access_->lock (file_lock, file_lock.type (), timeout) == file_lock.type ();
}

void FileDescriptorsContext::DescriptorBuf::get_lock (struct flock& lk) const
{
	FileLock file_lock;
	conv_lock (lk, file_lock);
	access_->get_lock (file_lock);
}

void FileDescriptorsContext::DescriptorDirect::get_lock (struct flock& lk) const
{
	FileLock file_lock;
	conv_lock (lk, file_lock);
	access_->get_lock (file_lock);
}

void FileDescriptorsContext::DescriptorChar::get_lock (struct flock& lk) const
{
	throw_BAD_PARAM (make_minor_errno (EINVAL));
}

}
}
