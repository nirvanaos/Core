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
#ifndef NIRVANA_CORE_POSIX_H_
#define NIRVANA_CORE_POSIX_H_

#include <Nirvana/Nirvana.h>
#include <Nirvana/POSIX_s.h>
#include <Nirvana/nls.h>
#include <Nirvana/signal_defs.h>
#include "Signals.h"
#include "FileDescriptors.h"
#include "CurrentDir.h"
#include "ExecDomain.h"
#include "TimerSleep.h"
#include "append_path.h"
#include "unrecoverable_error.h"
#include "Chrono.h"
#include "SystemInfo.h"
#include "InitOnce.h"

namespace Nirvana {

class Static_the_posix :
	public IDL::traits <POSIX>::ServantStatic <Static_the_posix>
{
public:
	static TimeBase::UtcT _s_system_clock (CORBA::Internal::Bridge <POSIX>* _b, CORBA::Internal::Interface* _env)
	{
		return Core::Chrono::system_clock ();
	}

	static TimeBase::UtcT _s_UTC (CORBA::Internal::Bridge <POSIX>* _b, CORBA::Internal::Interface* _env)
	{
		return Core::Chrono::UTC ();
	}

	static void set_UTC (const TimeBase::TimeT& t)
	{
		Core::Chrono::set_UTC (t);
	}

	static IDL::Type <TimeBase::TimeT>::ConstRef _s__get_system_clock_resolution (CORBA::Internal::Bridge <POSIX>* _b, CORBA::Internal::Interface* _env)
	{
		return IDL::Type <TimeBase::TimeT>::VT_ret (Core::Chrono::system_clock_resolution ());
	}

	static SteadyTime _s_steady_clock (CORBA::Internal::Bridge <POSIX>* _b, CORBA::Internal::Interface* _env)
	{
		return Core::Chrono::steady_clock ();
	}

	static IDL::Type <SteadyTime>::ConstRef _s__get_steady_clock_resolution (CORBA::Internal::Bridge <POSIX>* _b, CORBA::Internal::Interface* _env)
	{
		return IDL::Type <SteadyTime>::VT_ret (Core::Chrono::steady_clock_resolution ());
	}

	static DeadlineTime _s_deadline_clock (CORBA::Internal::Bridge <POSIX>* _b, CORBA::Internal::Interface* _env)
	{
		return Core::Chrono::deadline_clock ();
	}

	static IDL::Type <uint64_t>::ConstRef _s__get_deadline_clock_frequency (CORBA::Internal::Bridge <POSIX>* _b, CORBA::Internal::Interface* _env)
	{
		return IDL::Type <uint64_t>::VT_ret (Core::Chrono::deadline_clock_frequency ());
	}

	static int* error_number ()
	{
		return Core::RuntimeGlobal::current ().error_number ();
	}

	static void raise (int signal)
	{
		Core::Thread* th = Core::Thread::current_ptr ();
		if (th) {
			Core::ExecDomain* ed = th->exec_domain ();
			if (ed) {
				if (Core::Signals::is_supported (signal))
					ed->raise (signal);
				else
					throw_BAD_PARAM ();
			}
			return;
		}
		unrecoverable_error (signal);
	}

	static void sigaction (int signal, const struct sigaction* act, struct sigaction* oldact)
	{
		throw_NO_IMPLEMENT ();
	}

	static void srand (unsigned seed)
	{
		Core::RuntimeGlobal::current ().srand (seed);
	}

	static unsigned rand ()
	{
		return Core::RuntimeGlobal::current ().rand ();
	}

	static IDL::String get_current_dir ()
	{
		return CosNaming::Core::NameService::to_string_unchecked (Core::CurrentDir::current_dir ());
	}

	static void chdir (const IDL::String& path)
	{
		Core::CurrentDir::chdir (path);
	}

	static FilDesc open (const IDL::String& path, unsigned oflag, unsigned mode)
	{
		if (!(oflag & O_CREAT))
			mode = 0;
		// Get full path name
		CosNaming::Name name;
		Core::append_path (name, path, true);
		// Remove root name
		name.erase (name.begin ());
		// Get file system root
		Nirvana::Dir::_ref_type root = Nirvana::Dir::_narrow (name_service ()->resolve (CosNaming::Name ()));
		// Open file
		Nirvana::Access::_ref_type access = root->open (name, oflag, mode);
		return Core::FileDescriptors::fd_add (access, oflag);
	}

	static FilDesc mkostemps (CharPtr tpl, unsigned suffix_len, unsigned flags)
	{
		CosNaming::Name dir_name;
		IDL::String file;
		size_t tpl_len;
		{
			IDL::String tpl_path (tpl);
			tpl_len = tpl_path.size ();
			Core::append_path (dir_name, tpl_path, true);
			CosNaming::Name file_name;
			file_name.push_back (std::move (dir_name.back ()));
			dir_name.pop_back ();
			file = CosNaming::Core::NameService::to_string_unchecked (file_name);
		}

		Nirvana::AccessBuf::_ref_type access = Nirvana::AccessBuf::_downcast (
			Nirvana::Dir::_narrow (name_service ()->resolve (dir_name))->
			mkostemps (file, (uint16_t)suffix_len, (uint16_t)flags, 0)->_to_value ());

		unsigned fd = Core::FileDescriptors::fd_add (access, flags);
		size_t src_end = file.size () - suffix_len;
		size_t src_begin = src_end - 6;
		const char* src = file.c_str ();
		Nirvana::real_copy (src + src_begin, src + src_end, tpl + tpl_len - suffix_len - 6);
		return fd;
	}

	static void close (FilDesc fd)
	{
		Core::FileDescriptors::close (fd);
	}

	static size_t read (FilDesc fd, void* p, size_t size)
	{
		return Core::FileDescriptors::read (fd, p, size);
	}

	static void write (FilDesc fd, const void* p, size_t size)
	{
		Core::FileDescriptors::write (fd, p, size);
	}

	static FileSize seek (FilDesc fd, const FileOff& offset, uint_fast16_t whence)
	{
		return Core::FileDescriptors::seek (fd, offset, whence);
	}

	static FilDesc fcntl (FilDesc fd, unsigned cmd, uintptr_t arg)
	{
		return Core::FileDescriptors::fcntl (fd, cmd, arg);
	}

	static void fsync (FilDesc fd)
	{
		Core::FileDescriptors::flush (fd);
	}

	static void dup2 (FilDesc src, FilDesc dst)
	{
		Core::FileDescriptors::dup2 (src, dst);
	}

	static bool isatty (FilDesc fd)
	{
		return Core::FileDescriptors::isatty (fd);
	}

	static void sleep (TimeBase::TimeT period100ns)
	{
		if (!period100ns)
			Core::ExecDomain::reschedule ();
		else
			Core::TimerSleep ().sleep (period100ns);
	}

	static void yield () noexcept
	{
		Core::ExecDomain::reschedule ();
	}

	static void unlink (const IDL::String& path)
	{
		CosNaming::Name name;
		Core::append_path (name, path, true);
		name_service ()->unbind (name);
	}

	static void rmdir (const IDL::String& path)
	{
		CosNaming::Name name;
		Core::append_path (name, path, true);
		auto ns = name_service ();
		Nirvana::Dir::_ref_type dir = Nirvana::Dir::_narrow (ns->resolve (name));
		if (dir) {
			dir->destroy ();
			dir = nullptr;
			ns->unbind (name);
		} else
			throw_BAD_PARAM (make_minor_errno (ENOTDIR));
	}

	static void mkdir (const IDL::String& path, unsigned mode)
	{
		CosNaming::Name name;
		Core::append_path (name, path, true);
		// Remove root name
		name.erase (name.begin ());
		// Get file system root
		Nirvana::Dir::_ref_type root = Nirvana::Dir::_narrow (name_service ()->resolve (CosNaming::Name ()));
		// Create directory
		if (!root->mkdir (name, mode))
			throw_BAD_PARAM (make_minor_errno (EEXIST));
	}

	static void stat (const IDL::String& path, FileStat& st)
	{
		CosNaming::Name name;
		Core::append_path (name, path, true);
		Nirvana::DirItem::_narrow (name_service ()->resolve (name))->stat (st);
	}

	static void rename (const IDL::String& old_path, const IDL::String& new_path)
	{
		CosNaming::Name old_name, new_name;
		Core::append_path (old_name, old_path, true);
		Core::append_path (new_name, new_path, true);

		CosNaming::NamingContext::_ref_type nc = name_service ();
		CORBA::Object::_ref_type obj = nc->resolve (old_name);
		CosNaming::NamingContext::_ref_type dir = CosNaming::NamingContext::_narrow (obj);
		if (dir)
			nc->rebind_context (new_name, dir);
		else
			nc->rebind (new_name, obj);
	}

	static CS_Key CS_alloc (Deleter deleter)
	{
		return (CS_Key)Core::TLS::allocate (deleter);
	}

	static void CS_free (unsigned idx)
	{
		Core::TLS::release (idx);
	}

	static void CS_set (unsigned idx, void* ptr)
	{
		Core::TLS::set (idx, ptr);
	}

	static void* CS_get (unsigned idx)
	{
		return Core::TLS::get (idx);
	}

	static uint32_t _s__get_hardware_concurrency (CORBA::Internal::Bridge <Nirvana::POSIX>* _b, CORBA::Internal::Interface* _env)
	{
		return Core::SystemInfo::hardware_concurrency ();
	}

	static Locale::_ptr_type cur_locale ()
	{
		Core::MemContext* mc = Core::MemContext::current_ptr ();
		if (mc)
			return mc->locale ();
		else
			return nullptr;
	}

	static void cur_locale (Locale::_ptr_type newloc)
	{
		Core::MemContext* mc = Core::MemContext::current_ptr ();
		if (mc)
			mc->locale (newloc);
	}

	static Nirvana::Locale::_ref_type create_locale (int mask, const IDL::String& name, Nirvana::Locale::_ptr_type base)
	{
		return Core::locale_create (mask, name, base);
	}

	static void once (Pointer& control, InitFunc init_func)
	{
		Core::InitOnce::once (control, init_func);
	}

private:
	static CosNaming::NamingContext::_ref_type name_service ()
	{
		return CosNaming::NamingContext::_narrow (CORBA::Core::Services::bind (
			CORBA::Core::Services::NameService));
	}
};

}

#endif
