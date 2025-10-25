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
#include "Packages.h"

#define DATABASE_VERSION 1
#define RESERVE_SYS_MODULE_ID 100

#define STRINGIZE0(n) #n
#define STRINGIZE(n) STRINGIZE0 (n)

static_assert (Nirvana::PM::MAX_SYS_MODULE_ID == RESERVE_SYS_MODULE_ID, "RESERVE_SYS_MODULE_ID");

const char* const Packages::db_script_ [] = {

// Modules
"CREATE TABLE module("
"id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT NOT NULL,version INT NOT NULL,"
"prerelease TEXT NOT NULL DEFAULT '',flags INT NOT NULL,"
"UNIQUE (name,version,prerelease)"
")STRICT",

// Module binaries
"CREATE TABLE binary("
"module INT NOT NULL REFERENCES module(id)ON DELETE CASCADE,"
"platform INT NOT NULL,path TEXT NOT NULL,"
"UNIQUE(module,platform),UNIQUE(path)"
")STRICT",

// Module exports
"CREATE TABLE export("
"name TEXT NOT NULL,major INT NOT NULL,minor INT NOT NULL,"
"module INT NOT NULL REFERENCES module(id)ON DELETE CASCADE,"
"type INT NOT NULL,interface TEXT NOT NULL,"
"UNIQUE(name,major,module)"
")STRICT",

// Module imports
// Version stored as major << 16 | minor
"CREATE TABLE import("
"name TEXT NOT NULL,version INT NOT NULL,interface TEXT NOT NULL,"
"module INT NOT NULL REFERENCES module(id)ON DELETE CASCADE,"
"UNIQUE(name,version,interface,module)"
")STRICT",

// Reserve first 100 module identifiers as the system
"INSERT INTO module(id, name, version, flags)VALUES(" STRINGIZE (RESERVE_SYS_MODULE_ID) ",'',0,0); "
"DELETE FROM module WHERE id=" STRINGIZE (RESERVE_SYS_MODULE_ID) ";",

// Set database version
"PRAGMA user_version=" STRINGIZE (DATABASE_VERSION) ";"
};

Packages::Packages (CORBA::Object::_ptr_type component) :
	Servant (component)
{
	create_database ();
	pool_ = Connection::create_pool (Connection::connect_ro, POOL_MAX_CACHE, POOL_MAX_CREATE);
	manager_ = Nirvana::PM::man_factory->create ();
}

inline void Packages::create_database ()
{
	Connection connection (Connection::connect_rwc);
	auto st (connection.get_statement ());
	NDBC::ResultSet::_ref_type rs = st->executeQuery ("PRAGMA user_version;");
	rs->next ();
	int32_t cur_version = rs->getInt (1);
	if (DATABASE_VERSION != cur_version) {
		connection->setAutoCommit (false);
		for (const char* sql : db_script_) {
#ifndef NDEBUG
			try {
#endif
				st->executeUpdate (sql);
#ifndef NDEBUG
			} catch (const NDBC::SQLException& ex) {
				NIRVANA_WARNING ("SQL Error: %s\n%s\n", ex.error ().sqlState ().c_str (), sql);
				throw;
			}
#endif
		}
		connection.commit ();
	}
}
