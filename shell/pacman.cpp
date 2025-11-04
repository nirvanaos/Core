/*
* Nirvana shell.
*
* This is a part of the Nirvana project.
*
* Author: Igor Popov
*
* Copyright (c) 2025 Igor Popov.
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
#include <Nirvana/Nirvana.h>
#include <Nirvana/Shell_s.h>
#include <Nirvana/Domains.h>
#include <Nirvana/Packages.h>
#include <Nirvana/System.h>
#include <Nirvana/POSIX.h>
#include <Nirvana/platform.h>

namespace Nirvana {

class Static_pacman :
	public CORBA::servant_traits <Nirvana::Cmdlet>::ServantStatic <Static_pacman>
{
public:
	static int run (const StringSeq& argv)
	{
		static const char usage [] = "Usage: pacman <command> [parameters]";
		if (argv.size () <= 1) {
			print (1, usage);
			return -1;
		}

		int ret = -1;

		try {
			auto packages = SysDomain::_narrow (CORBA::the_orb->resolve_initial_references ("SysDomain")
				)->provide_packages ();
			try {
				if (argv [1] == "reg-bin")
					ret = reg_bin (packages, argv);
				else {
					print (2, "Unknown command: ");
					print (2, argv [1]);
					println (2);
				}
			} catch (const BindError::Error& ex) {
				print (packages, ex);
			}
		} catch (const CORBA::SystemException& ex) {
			print (ex);
			println (2);
		}

		return ret;
	}

private:
	static int reg_bin (PM::Packages::_ptr_type packages, const StringSeq& argv)
	{
		PM::PacMan::_ref_type pacman = manage (packages);
		if (!pacman)
			return -1;

		IDL::String bin_path;
		{
			CosNaming::Name name;
			the_system->append_path (name, argv [2], true);
			bin_path = the_system->to_string (name);
		}

		PM::ModuleInfo info;
		PlatformId platform = pacman->register_binary (bin_path, info);
		pacman->commit ();

		print (1, "Binary '");
		print (1, bin_path);
		print (1, "' was successfully registered for platform ");
		const char* platform_name = get_platform_name (platform);
		if (platform_name)
			print (1, platform_name);
		else
			print (1, platform);
		print (1, " of module ");
		print (1, info);
		print (1, ".");
		println (1);

		return 0;
	}

	static PM::PacMan::_ref_type manage (PM::Packages::_ptr_type packages);

	static void print (int fd, const char* s);
	static void print (int fd, const std::string& s);
	static void print (int fd, int d);
	static void println (int fd);
	static void print (PM::PackageDB::_ptr_type packages, const BindError::Error& err);
	static void print (PM::PackageDB::_ptr_type packages, const BindError::Info& err);
	static void print (const CORBA::SystemException& se);
	static void print (int fd, const PM::ModuleInfo& info);
};

PM::PacMan::_ref_type Static_pacman::manage (PM::Packages::_ptr_type packages)
{
	PM::PacMan::_ref_type pm = packages->manage ();
	if (!pm)
		print (2, "Installation session is already started, try later.\n");
	return pm;
}

void Static_pacman::print (int fd, const char* s)
{
	the_posix->write (fd, s, strlen (s));
}

void Static_pacman::print (int fd, const std::string& s)
{
	the_posix->write (fd, s.data (), s.size ());
}

void Static_pacman::println (int fd)
{
	const char n = '\n';
	the_posix->write (fd, &n, 1);
}

void Static_pacman::print (int fd, int d)
{
	print (fd, std::to_string (d));
}

void Static_pacman::print (PM::PackageDB::_ptr_type packages, const BindError::Error& err)
{
	print (packages, err.info ());
	for (auto it = err.stack ().cbegin (), end = err.stack ().cend (); it != end; ++it) {
		print (packages, *it);
	}
}

void Static_pacman::print (PM::PackageDB::_ptr_type packages, const BindError::Info& err)
{
	switch (err._d ()) {
	case BindError::Type::ERR_MESSAGE:
		print (2, err.s ());
		break;

	case BindError::Type::ERR_OBJ_NAME:
		print (2, "Error binding object: ");
		print (2, err.s ());
		break;

	case BindError::Type::ERR_ITF_NOT_FOUND:
		print (2, "Interface ");
		print (2, err.s ());
		print (2, " is not available for object");
		break;

	case BindError::Type::ERR_MOD_LOAD:
		print (2, "Module load: ");
		print (2, packages->get_module_info (err.mod_info ().module_id ()));
		break;

	case BindError::Type::ERR_SYSTEM: {
		CORBA::UNKNOWN se;
		err.system_exception () >>= static_cast <CORBA::SystemException&> (se);
		print (se);
	} break;

	case BindError::Type::ERR_UNSUP_PLATFORM:
		print (2, "Unsupported platform: ");
		print (2, err.platform_id ());
		break;

	default:
		print (2, "Unknown error");
		break;
	}

	println (2);
}

void Static_pacman::print (const CORBA::SystemException& se)
{
	print (2, se.what ());
	int err = get_minor_errno (se.minor ());
	if (err) {
		print (2, " errno: ");
		print (2, err);
	}
}

void Static_pacman::print (int fd, const PM::ModuleInfo& info)
{
	print (fd, info.name ());
	if (info.flags () & PM::MODULE_FLAG_SINGLETON)
		print (fd, " (singleton)");
}

}

NIRVANA_EXPORT_STATIC (_exp_pacman, "Nirvana/pacman", Nirvana::Static_pacman)
