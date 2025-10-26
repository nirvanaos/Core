/*
* Nirvana package manager.
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
#include "Connection.h"
#include <Nirvana/BindErrorUtl.h>

#define DATABASE_PATH "/var/lib/packages.db"

const char Connection::connect_rwc [] = "file:" DATABASE_PATH "?mode=rwc";
const char Connection::connect_rw [] = "file:" DATABASE_PATH "?mode=rw";
const char Connection::connect_ro [] = "file:" DATABASE_PATH "?mode=ro";

NIRVANA_NORETURN void Connection::on_sql_exception (NDBC::SQLException& ex)
{
	Nirvana::BindError::Error err;
	NIRVANA_TRACE ("SQL error: %s\n", ex.error ().sqlState ().c_str ());
	Nirvana::BindError::set_message (err.info (), std::move (ex.error ().sqlState ()));
	for (NDBC::SQLWarning& sqle : ex.next ()) {
		NIRVANA_TRACE ("SQL error: %s\n", sqle.sqlState ().c_str ());
		Nirvana::BindError::set_message (Nirvana::BindError::push (err), std::move (sqle.sqlState ()));
	}
	throw err;
}

void Connection::get_module_bindings (Nirvana::ModuleId id, Nirvana::PM::ModuleBindings& metadata)
{
	auto stm = get_statement (
		"SELECT name,version,prerelease,flags FROM module WHERE id=?;"
		"SELECT name,major,minor,interface,type FROM export WHERE module=? ORDER BY name,major,minor;"
		"SELECT name,version,interface FROM import WHERE module=? ORDER BY name,version,interface;"
	);
	stm->setInt (1, id);
	stm->setInt (2, id);
	stm->setInt (3, id);

	auto rs = stm->executeQuery ();

	if (rs->next ()) {
		Nirvana::SemVer name (rs->getString (1), rs->getBigInt (2), rs->getString (3));
		metadata.mod ().name (name.to_string ());
		metadata.mod ().flags (rs->getSmallInt (4));
	} else
		throw Nirvana::PM::PackageDB::ModuleNotFound ();

	NIRVANA_VERIFY (stm->getMoreResults ());
	rs = stm->getResultSet ();

	while (rs->next ()) {
		metadata.exports ().emplace_back (
			Nirvana::PM::ObjBinding (rs->getString (1), rs->getSmallInt (2), rs->getSmallInt (3),
				rs->getString (4)),
			(Nirvana::PM::ObjType)rs->getInt (5));
	}

	NIRVANA_VERIFY (stm->getMoreResults ());
	rs = stm->getResultSet ();

	while (rs->next ()) {
		uint32_t ver = rs->getInt (2);
		metadata.imports ().emplace_back (rs->getString (1), major (ver), minor (ver),
			rs->getString (3));
	}
}
