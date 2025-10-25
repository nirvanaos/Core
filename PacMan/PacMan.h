/*
* Nirvana package manager.
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
#ifndef PACMAN_PACMAN_H_
#define PACMAN_PACMAN_H_
#pragma once

#include "Connection.h"
#include <Nirvana/Packages_s.h>
#include <Nirvana/Domains.h>
#include <Nirvana/BindErrorUtl.h>

class Manager;

class PacMan :
	public CORBA::servant_traits <Nirvana::PM::PacMan>::Servant <PacMan>,
	public Connection
{
public:
	PacMan (CORBA::servant_reference <Manager>&& manager) :
		Connection (Connection::create_pool (connect_rw, 0, 1)),
		manager_ (std::move (manager)),
		busy_ (false)
	{
		connection ()->setAutoCommit (false);
	}

	~PacMan ();

	void commit ()
	{
		Lock lock (*this);
		Connection::commit ();
		complete ();
	}

	void rollback ()
	{
		Lock lock (*this);
		Connection::rollback ();
		complete ();
	}

	static long compare_name (const Nirvana::PM::ObjBinding& l, const Nirvana::PM::ObjBinding& r) noexcept;

	struct Less
	{
		bool operator () (const Nirvana::PM::ObjBinding& l, const Nirvana::PM::ObjBinding& r) const noexcept
		{
			long cmp = compare_name (l, r);
			if (cmp < 0)
				return true;
			else if (cmp > 0)
				return false;

			return l.itf_id () < r.itf_id ();
		}

		bool operator () (const Nirvana::PM::Export& l, const Nirvana::PM::Export& r) const noexcept
		{
			return compare_name (l.binding (), r.binding ()) < 0;
		}
	};

	struct Equal
	{
		bool operator () (const Nirvana::PM::ObjBinding& l, const Nirvana::PM::ObjBinding& r)
			const noexcept
		{
			return l.name () == r.name () && l.major () == r.major () && l.minor () == r.minor ()
				&& l.itf_id () == r.itf_id ();
		}

		bool operator () (const Nirvana::PM::Export& l, const Nirvana::PM::Export& r) const noexcept
		{
			return operator () (l.binding (), r.binding ()) && l.type () == r.type ();
		}
	};

	Nirvana::PlatformId register_binary (const IDL::String& path, Nirvana::PM::ModuleInfo& info)
	{
		Lock lock (*this);

		Nirvana::SysDomain::_ref_type sys_domain = Nirvana::SysDomain::_narrow (
			CORBA::the_orb->resolve_initial_references ("SysDomain"));
		Nirvana::PM::ModuleBindings metadata;
		Nirvana::PlatformId platform = sys_domain->get_module_bindings (path, metadata);
		Nirvana::SemVer svname;
		if (!svname.parse (metadata.mod ().name ()))
			Nirvana::BindError::throw_message ("Invalid name: " + metadata.mod ().name ());

		try {

			auto stm = get_statement ("SELECT id,flags FROM module WHERE name=? AND version=? AND prerelease=?");

			stm->setString (1, svname.name ());
			stm->setBigInt (2, svname.version ());
			stm->setString (3, svname.prerelease ());

			NDBC::ResultSet::_ref_type rs = stm->executeQuery ();

			int32_t module_id;
			if (rs->next ()) {

				module_id = rs->getInt (1);
				if (metadata.mod ().flags () != rs->getSmallInt (2))
					Nirvana::BindError::throw_message ("Module type mismatch");

				std::sort (metadata.imports ().begin (), metadata.imports ().end (), Less ());
				std::sort (metadata.exports ().begin (), metadata.exports ().end (), Less ());

				Nirvana::PM::ModuleBindings cur_md;
				get_module_bindings (module_id, cur_md);

				check_match (metadata, cur_md);

			} else {

				stm = get_statement ("INSERT INTO module(name,version,prerelease,flags)VALUES(?,?,?,?)"
					"RETURNING id");
				stm->setString (1, svname.name ());
				stm->setBigInt (2, svname.version ());
				stm->setString (3, svname.prerelease ());
				stm->setInt (4, metadata.mod ().flags ());
				rs = stm->executeQuery ();
				rs->next ();
				module_id = rs->getInt (1);

				stm = get_statement ("INSERT INTO export(module,name,major,minor,type,interface)"
					"VALUES(?,?,?,?,?,?)");
				stm->setInt (1, module_id);
				for (const auto& e : metadata.exports ()) {
					stm->setString (2, e.binding ().name ());
					stm->setInt (3, e.binding ().major ());
					stm->setInt (4, e.binding ().minor ());
					stm->setInt (5, (int)e.type ());
					stm->setString (6, e.binding ().itf_id ());
					stm->executeUpdate ();
				}

				stm = get_statement ("INSERT INTO import(module,name,version,interface)"
					"VALUES(?,?,?,?)");
				stm->setInt (1, module_id);
				for (const auto& b : metadata.imports ()) {
					stm->setString (2, b.name ());
					stm->setInt (3, version (b.major (), b.minor ()));
					stm->setString (4, b.itf_id ());
					stm->executeUpdate ();
				}
			}

			stm = get_statement ("INSERT OR REPLACE INTO binary VALUES(?,?,?)");
			stm->setInt (1, module_id);
			stm->setInt (2, platform);
			stm->setString (3, path);
			stm->executeUpdate ();

		} catch (NDBC::SQLException& ex) {
			on_sql_exception (ex);
		}

		info = std::move (metadata.mod ());
		return platform;
	}

	uint32_t unregister (Nirvana::ModuleId id, bool recursive)
	{
		Lock lock (*this);

		try {

			auto stm = get_statement ("DELETE FROM module WHERE id=?");
			stm->setInt (1, id);

			return stm->executeUpdate ();
		} catch (NDBC::SQLException& ex) {
			on_sql_exception (ex);
		}
	}

private:
	// Does not allow parallel installations
	class Lock
	{
	public:
		Lock (PacMan& obj);

		~Lock ()
		{
			obj_.busy_ = false;
		}

	private:
		PacMan& obj_;
	};

	void complete () noexcept;

	static void check_match (const Nirvana::PM::ModuleBindings& l, const Nirvana::PM::ModuleBindings& r)
	{
		if (!
			std::equal (l.exports ().begin (), l.exports ().end (),
				r.exports ().begin (), r.exports ().end (), Equal ())
			&&
			std::equal (l.imports ().begin (), l.imports ().end (),
				r.imports ().begin (), r.imports ().end (), Equal ())
			)
			Nirvana::BindError::throw_message ("Metadata mismatch");
	}

private:
	CORBA::servant_reference <Manager> manager_;
	bool busy_;
};

#endif
