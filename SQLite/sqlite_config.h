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
#ifndef SQLITE_CONFIG_H_
#define SQLITE_CONFIG_H_
#pragma once

#define SQLITE_OS_OTHER 1

#define SQLITE_DEFAULT_MMAP_SIZE 0x7fffffffffffffffL
#define SQLITE_MAX_MMAP_SIZE 0x7fffffffffffffffL

#define SQLITE_DEFAULT_FOREIGN_KEYS 1
#define SQLITE_DEFAULT_MEMSTATUS 0
//#define SQLITE_DEFAULT_WAL_SYNCHRONOUS 1
#define SQLITE_ENABLE_COLUMN_METADATA 1
#define SQLITE_LIKE_DOESNT_MATCH_BLOBS 1
#define SQLITE_OS_OTHER 1
#define SQLITE_OMIT_AUTOINIT 1
#define SQLITE_OMIT_DEPRECATED 1
#define SQLITE_OMIT_PROGRESS_CALLBACK 1
#define SQLITE_OMIT_SHARED_CACHE 1
#define SQLITE_OMIT_WSD 1
#define SQLITE_STRICT_SUBTYPE 1
//#define SQLITE_USE_ALLOCA 1
#define SQLITE_USE_URI 1
#define SQLITE_WITHOUT_MSIZE 1

// WAL requires shared memory. Impossible in Nirvana.
#define SQLITE_OMIT_WAL 1

#if defined (_MSC_VER) && !defined (__clang__)
#define HAVE_LOCALTIME_S 1
#else
#define HAVE_LOCALTIME_R 1
#endif

#define HAVE_GMTIME_R 1

#define SQLITE_THREADSAFE 0
#define SQLITE_MUTEX_OMIT

#ifndef NDEBUG
//#define SQLITE_DEBUG 1
#endif

#define SQLITE_OMIT_SEH

// Declare for CLang
#if defined (_MSC_VER) && defined (__clang__)
unsigned short _byteswap_ushort (unsigned short);
unsigned long _byteswap_ulong (unsigned long);
unsigned __int64 __cdecl _byteswap_uint64 (unsigned __int64);
#endif

#endif
