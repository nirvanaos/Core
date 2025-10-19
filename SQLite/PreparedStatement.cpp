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
#include "PreparedStatement.h"

namespace SQLite {

PreparedStatement::ParamIndex PreparedStatement::get_param_index (unsigned i)
{
	for (unsigned si = 0; si < statements ().size (); ++si) {
		int par_cnt = sqlite3_bind_parameter_count (statements () [si]);
		if (i <= (unsigned)par_cnt) {
			reset ();
			return { si, i };
		}
		i -= par_cnt;
	}
	throw_exception ("Invalid parameter index");
}

PreparedStatement::ParamIndex PreparedStatement::get_param_index (const IDL::String& name)
{
	for (unsigned si = 0; si < statements ().size (); ++si) {
		int i = sqlite3_bind_parameter_index (statements () [si], name.c_str ());
		if (i >= 1) {
			reset ();
			return { si, (unsigned)i };
		}
	}
	throw_exception ("Parameter not found: " + name);
}

double PreparedStatement::fixed2double (const CORBA::Any& v)
{
	CORBA::Fixed f;
	CORBA::Any::to_fixed tf (f, 62, 31);
	if (v >>= tf)
		return (double)f;
	else
		throw CORBA::BAD_PARAM ();
}

PreparedStatement::ParamStorage& PreparedStatement::param_storage (const ParamIndex& pi)
{
	return param_storage_ [pi];
}

void PreparedStatement::ParamStorage::destruct () noexcept
{
	switch (d_) {
	case STRING:
		destruct (u_.string);
		break;
	case BLOB:
		destruct (u_.blob);
		break;
	}
}

void PreparedStatement::ParamStorage::adopt (ParamStorage& src) noexcept
{
	switch (d_ = src.d_) {
	case STRING:
		construct (u_.string, std::move (src.u_.string));
		break;
	case BLOB:
		construct (u_.blob, std::move (src.u_.blob));
		break;
	}
	src.d_ = EMPTY;
}

}
