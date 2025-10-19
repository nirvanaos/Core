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
#include "Cursor.h"
#include <limits>

namespace SQLite {

NDBC::Row Cursor::get_row (sqlite3_stmt* stmt)
{
	NDBC::Row row;

	int cnt = sqlite3_data_count (stmt);
	assert (cnt > 0);
	for (int i = 0; i < cnt; ++i) {
		NDBC::Variant v;
		int ct = sqlite3_column_type (stmt, i);
		switch (ct) {
		case SQLITE_INTEGER: {
			sqlite3_int64 i64 = sqlite3_column_int64 (stmt, i);
			if (i64 >= 0 && i64 <= std::numeric_limits <CORBA::Octet>::max ())
				v.byte_val ((CORBA::Octet)i64);
			else if (std::numeric_limits <CORBA::Short>::min () <= i64 && i64 <= std::numeric_limits <CORBA::Short>::max ())
				v.si_val ((CORBA::Short)i64);
			else if (std::numeric_limits <CORBA::Long>::min () <= i64 && i64 <= std::numeric_limits <CORBA::Long>::max ())
				v.l_val ((CORBA::Long)i64);
			else
				v.ll_val (i64);
		} break;
		case SQLITE_FLOAT:
			v.dbl_val (sqlite3_column_double (stmt, i));
			break;
		case SQLITE_TEXT: {
			size_t size = sqlite3_column_bytes (stmt, i);
			v.s_val (IDL::String ((const char*)sqlite3_column_text (stmt, i), size));
		} break;
		case SQLITE_BLOB: {
			size_t size = sqlite3_column_bytes (stmt, i);
			const CORBA::Octet* b = (const CORBA::Octet*)sqlite3_column_blob (stmt, i);
			v.blob (NDBC::Blob (b, b + size));
		} break;
		}

		row.push_back (std::move (v));
	}

	return row;
}

Connection& Cursor::check_parent ()
{
	if (!parent_)
		raise_closed ();
	try {
		if (parent_version_ != parent_->version ())
			raise_closed ();
		return parent_->connection ();
	} catch (...) {
		close_no_check ();
		throw;
	}
}

NIRVANA_NORETURN void Cursor::raise_closed ()
{
	throw_exception ("Cursor is closed");
}

}
