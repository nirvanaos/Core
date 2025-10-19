/*
* Nirvana SQLite driver.
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
#include "filesystem.h"
#include "Global.h"
#include <Nirvana/NDBC.h>
#include <Nirvana/File.h>
#include <Nirvana/System.h>
#include <Nirvana/POSIX.h>
#include <Nirvana/c_heap_dbg.h>
#include <Nirvana/posix_defs.h>
#include <Nirvana/RandomGen.h>

extern "C" int sqlite3_os_init ()
{
	return SQLITE_OK;
}

extern "C" int sqlite3_os_end ()
{
	return SQLITE_OK;
}

namespace SQLite {

static const Nirvana::FileSize FILE_HEADER_SIZE = 100;

extern const sqlite3_io_methods io_methods;

class File : public sqlite3_file
{
public:
	File (Nirvana::AccessDirect::_ref_type fa, int flags) noexcept :
		access_ (std::move (fa)),
		lock_level_ (Nirvana::LockType::LOCK_NONE),
		flags_ (flags)
	{
		pMethods = &io_methods;
	}

	int close () noexcept
	{
		try {
			Nirvana::File::_ref_type file_to_delete;
			if (flags_ & SQLITE_OPEN_DELETEONCLOSE)
				file_to_delete = access_->file ();
			access_->close ();
			access_ = nullptr;
			if (file_to_delete)
				file_to_delete->remove ();
		} catch (...) {
		}
		return SQLITE_OK;
	}

	int read (void* p, int cb, sqlite3_int64 off) const noexcept
	{
		int ret = SQLITE_OK;
		try {
			NDBC::Blob data;
			access_->read (off, cb, data);
			memcpy (p, data.data (), data.size ());
			if ((int)data.size () < cb) {
				// If xRead () returns SQLITE_IOERR_SHORT_READ it must also fill in the unread portions of the
				// buffer with zeros.A VFS that fails to zero - fill short reads might seem to work.
				// However, failure to zero - fill short reads will eventually lead to database corruption.
				// https://www.sqlite.org/c3ref/io_methods.html
				memset ((char*)p + data.size (), 0, (size_t)cb - data.size ());
				ret = SQLITE_IOERR_SHORT_READ;
			}
		} catch (...) {
			ret = SQLITE_IOERR_READ;
		}
		return ret;
	}

	int write (const void* p, int cb, sqlite3_int64 off) const noexcept
	{
		int ret = SQLITE_OK;
		try {
			access_->write (off, NDBC::Blob ((const CORBA::Octet*)p, (const CORBA::Octet*)p + cb), false);
		} catch (const CORBA::SystemException& ex) {
			if (ENOSPC == Nirvana::get_minor_errno (ex.minor ()))
				ret = SQLITE_FULL;
			else
				ret = SQLITE_IOERR_WRITE;
		} catch (...) {
			ret = SQLITE_IOERR_WRITE;
		}
		return ret;
	}

	int truncate (sqlite3_int64 size) const noexcept
	{
		int ret = SQLITE_OK;
		try {
			access_->size (size);
		} catch (...) {
			ret = SQLITE_IOERR_TRUNCATE;
		}
		return ret;
	}

	int sync () const noexcept
	{
		int ret = SQLITE_OK;
		try {
			access_->flush ();
		} catch (...) {
			ret = SQLITE_IOERR_FSYNC;
		}
		return ret;
	}

	int size (sqlite3_int64& cb) const noexcept
	{
		int ret = SQLITE_OK;
		try {
			cb = access_->size ();
		} catch (...) {
			ret = SQLITE_IOERR_FSTAT;
		}
		return ret;
	}

	int sector_size () const noexcept
	{
		int ret = 512;
		try {
			ret = (int)access_->block_size ();
		} catch (...) {
		}
		return ret;
	}

	int fetch (sqlite3_int64 off, int cb, void** pp) noexcept
	{
		int ret = SQLITE_OK;
		*pp = nullptr;
		assert (cb);
		auto it = cache_.find (off);
		if (it == cache_.end ()) {
			try {
				CacheEntry entry;
				access_->read (off, cb, entry.block);
				if ((int)entry.block.size () == cb)
					*pp = cache_.emplace (off, std::move (entry)).first->second.block.data ();
			} catch (const CORBA::NO_MEMORY&) {
				ret = SQLITE_IOERR_NOMEM;
			} catch (...) {
				ret = SQLITE_IOERR_READ;
			}
		} else {
			if (cb > it->second.block.size ()) {
				assert (false);
				ret = SQLITE_IOERR_READ;
			}
			it->second.ref_cnt++;
			*pp = it->second.block.data ();
		}
		return ret;
	}

	int unfetch (sqlite3_int64 off, void* p) noexcept
	{
		auto it = cache_.find (off);
//		assert (it != cache_.end ());
		if (it != cache_.end () && !--(it->second.ref_cnt))
			cache_.erase (it);
		return SQLITE_OK;
	}

	int lock (Nirvana::LockType level) noexcept
	{
		if (level > lock_level_) {
			if (level > Nirvana::LockType::LOCK_SHARED && !(flags_ & SQLITE_OPEN_READWRITE))
				return SQLITE_IOERR_LOCK;

			try {
				Nirvana::LockType lock_min = level;
				if (Nirvana::LockType::LOCK_EXCLUSIVE == level
					&& lock_level_ < Nirvana::LockType::LOCK_PENDING)
					lock_min = Nirvana::LockType::LOCK_PENDING;

				Nirvana::LockType ret = access_->lock (Nirvana::FileLock (FILE_HEADER_SIZE, 0, level),
					lock_min, 0);

				if (ret >= lock_min)
					lock_level_ = ret;

				if (lock_level_ < level)
					return SQLITE_BUSY;

			} catch (...) {
				if (level > Nirvana::LockType::LOCK_SHARED)
					return SQLITE_IOERR_LOCK;
				else
					return SQLITE_IOERR_RDLOCK;
			}
		}
		return SQLITE_OK;
	}

	int unlock (Nirvana::LockType level) noexcept
	{
		if (lock_level_ > level) {
			try {
				lock_level_ = access_->lock (Nirvana::FileLock (FILE_HEADER_SIZE, 0, level), level, 0);
			} catch (...) {
				return SQLITE_IOERR_UNLOCK;
			}
		}
		return SQLITE_OK;
	}

	int check_reserved_lock (int& ret) const noexcept
	{
		if (lock_level_ > Nirvana::LockType::LOCK_SHARED)
			return true;
		try {
			Nirvana::FileLock fl (FILE_HEADER_SIZE, 0, Nirvana::LockType::LOCK_RESERVED);
			access_->get_lock (fl);
			ret = fl.type () != Nirvana::LockType::LOCK_NONE;
		} catch (...) {
			return SQLITE_IOERR_CHECKRESERVEDLOCK;
		}
		return SQLITE_OK;
	}

private:
	Nirvana::AccessDirect::_ref_type access_;
	Nirvana::LockType lock_level_;
	int flags_;

	struct CacheEntry
	{
		NDBC::Blob block;
		unsigned ref_cnt;

		CacheEntry () :
			ref_cnt (1)
		{}
	};

	typedef Nirvana::Core::MapUnorderedUnstable <sqlite3_int64, CacheEntry> Cache;
	Cache cache_;
};

extern "C" int xClose (sqlite3_file * f) noexcept
{
	return static_cast <File&> (*f).close ();
}

extern "C" int xRead (sqlite3_file* f, void* p, int iAmt, sqlite3_int64 off)
{
	return static_cast <File&> (*f).read (p, iAmt, off);
}

extern "C" int xWrite (sqlite3_file* f, const void* p, int iAmt, sqlite3_int64 off)
{
	return static_cast <File&> (*f).write (p, iAmt, off);
}

extern "C" int xTruncate (sqlite3_file* f, sqlite3_int64 size)
{
	return static_cast <File&> (*f).truncate (size);
}

extern "C" int xSync (sqlite3_file* f, int)
{
	return static_cast <File&> (*f).sync ();
}

extern "C" int xFileSize (sqlite3_file* f, sqlite3_int64* pSize)
{
	return static_cast <File&> (*f).size (*pSize);
}

extern "C" int xLock (sqlite3_file* f, int level)
{
	assert ((int)Nirvana::LockType::LOCK_SHARED <= level && level <= (int)Nirvana::LockType::LOCK_EXCLUSIVE);
	return static_cast <File&> (*f).lock ((Nirvana::LockType)level);
}

extern "C" int xUnlock (sqlite3_file* f, int level)
{
	assert ((int)Nirvana::LockType::LOCK_NONE <= level && level <= (int)Nirvana::LockType::LOCK_SHARED);
	return static_cast <File&> (*f).unlock ((Nirvana::LockType)level);
}

extern "C" int xCheckReservedLock (sqlite3_file* f, int* pResOut)
{
	return static_cast <File&> (*f).check_reserved_lock (*pResOut);
}

extern "C" int xSectorSize (sqlite3_file* f)
{
	return static_cast <File&> (*f).sector_size ();
}

int xDeviceCharacteristics (sqlite3_file*)
{
	return
		SQLITE_IOCAP_UNDELETABLE_WHEN_OPEN |
//?		SQLITE_IOCAP_POWERSAFE_OVERWRITE |
		SQLITE_IOCAP_ATOMIC;
}

extern "C" int xFetch (sqlite3_file* f, sqlite3_int64 iOfst, int iAmt, void** pp)
{
	return static_cast <File&> (*f).fetch (iOfst, iAmt, pp);
}

extern "C" int xUnfetch (sqlite3_file* f, sqlite3_int64 iOfst, void* p)
{
	return static_cast <File&> (*f).unfetch (iOfst, p);
}

extern "C" int xFileControl (sqlite3_file * f, int op, void* pArg)
{
	return SQLITE_NOTFOUND;
}

const sqlite3_io_methods io_methods = {
	3,
	xClose,
	xRead,
	xWrite,
	xTruncate,
	xSync,
	xFileSize,
	xLock,
	xUnlock,
	xCheckReservedLock,
	xFileControl,
	xSectorSize,
	xDeviceCharacteristics,
	nullptr, // xShmMap
	nullptr, // xShmLock
	nullptr, // xShmBarrier
	nullptr, // xShmUnmap
	xFetch,
	xUnfetch
};

int xOpen (sqlite3_vfs*, sqlite3_filename zName, sqlite3_file* file,
	int flags, int* pOutFlags) noexcept
{
	Nirvana::AccessDirect::_ref_type fa;
	try {
		fa = global.open_file (zName, flags);
	} catch (CORBA::NO_MEMORY ()) {
		return SQLITE_NOMEM;
#ifndef NDEBUG
	} catch (const std::exception& ex) {
		SQLITE_TRACE ("%s", ex.what ());
#else
	} catch (...) {
#endif
		return SQLITE_CANTOPEN;
	}
	new (file) File (fa, flags);
	return SQLITE_OK;
}

extern "C" int xDelete (sqlite3_vfs*, sqlite3_filename zName, int syncDir)
{
	try {
		// Get full path name
		CosNaming::Name name;
		Nirvana::the_system->append_path (name, zName, true);
		// Remove root name
		name.erase (name.begin ());
		// Delete
		global.file_system ()->unbind (name);
	} catch (...) {
		return SQLITE_ERROR;
	}
	return SQLITE_OK;
}

extern "C" int xAccess (sqlite3_vfs*, const char* zName, int flags, int* pResOut)
{
	*pResOut = 0;
	try {
		// Get full path name
		CosNaming::Name name;
		Nirvana::the_system->append_path (name, zName, true);
		// Remove root name
		name.erase (name.begin ());
		unsigned mask = 0;
		switch (flags) {
		case SQLITE_ACCESS_EXISTS:
			mask = F_OK;
			break;
		case SQLITE_ACCESS_READWRITE:
			mask = R_OK | W_OK;
			break;
		case SQLITE_ACCESS_READ:
			mask = R_OK;
			break;
		}
		*pResOut = (global.file_system ()->access (name) & mask) == mask;
	} catch (...) {
		return SQLITE_ERROR;
	}
	return SQLITE_OK;
}

extern "C" int xFullPathname (sqlite3_vfs*, const char* zName, int nOut, char* zOut)
{
	try {
		// Get full path name
		CosNaming::Name name;
		Nirvana::the_system->append_path (name, zName, true);
		auto s = Nirvana::the_system->to_string (name);
		if ((int)s.size () >= nOut)
			return SQLITE_CANTOPEN;
		const char* n = s.c_str ();
		std::copy (n, n + s.size () + 1, zOut);
	} catch (...) {
		return SQLITE_CANTOPEN;
	}
	return SQLITE_OK;
}

extern "C" int xSleep (sqlite3_vfs*, int microseconds)
{
	Nirvana::the_posix->sleep ((TimeBase::TimeT)microseconds * TimeBase::MICROSECOND);
	return SQLITE_OK;
}

extern "C" int xCurrentTimeInt64 (sqlite3_vfs*, sqlite3_int64* time)
{
	TimeBase::UtcT t = Nirvana::the_posix->system_clock ();
	*time = (t.time () + t.tdf () * 600000000LL) / TimeBase::MILLISECOND + TimeBase::JULIAN_MS;
	return SQLITE_OK;
}

extern "C" int xRandomness (sqlite3_vfs*, int nByte, char* zOut)
{
	Nirvana::RandomGen random ((Nirvana::RandomGen::result_type)Nirvana::the_posix->UTC ().time ());
	for (size_t cnt = nByte; cnt > 0;) {
		Nirvana::RandomGen::result_type rnd = random ();
		size_t bcnt = sizeof (Nirvana::RandomGen::result_type);
		if (bcnt > cnt)
			bcnt = cnt;
		const char* src = (const char*)&rnd;
		zOut = std::copy (src, src + bcnt, zOut);
		cnt -= bcnt;
	}
	return nByte;
}

const size_t HEAP_BLOCK_OVERHEAD = sizeof (Nirvana::HeapBlockHdrType) + Nirvana::HeapBlockHdrType::TRAILER_SIZE;

struct sqlite3_vfs vfs = {
	3,                   /* Structure version number (currently 3) */
	(int)sizeof (File),  /* Size of subclassed sqlite3_file */
  512 - HEAP_BLOCK_OVERHEAD, /* Maximum file pathname length */
	nullptr,             /* Next registered VFS */
	VFS_NAME,            /* Name of this virtual file system */
	nullptr,             /* Pointer to application-specific data */
	xOpen,
	xDelete,
	xAccess,
	xFullPathname,
	nullptr, // xDlOpen
	nullptr, // xDlError
	nullptr, // xDlSym
	nullptr, // xDlClose
	xRandomness,
	xSleep,
	nullptr, // xCurrentTime
	nullptr, // xGetLastError
	xCurrentTimeInt64
};

}
