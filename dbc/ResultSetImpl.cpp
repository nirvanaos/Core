/*
* Database connection module.
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
#include "ResultSetImpl.h"
#include <Nirvana/string_conv.h>

namespace NDBC {

NIRVANA_NORETURN void ResultSetImpl::throw_exception (IDL::String msg)
{
	throw NDBC::SQLException (NDBC::SQLWarning (0, std::move (msg)), NDBC::SQLWarnings ());
}

NIRVANA_NORETURN void ResultSetImpl::data_conversion_error ()
{
	throw_exception ("Data conversion error");
}

ResultSetImpl::~ResultSetImpl ()
{}

bool ResultSetImpl::fetch (RowOff i)
{
	check ();

	if (i)
		check_scrollable ();

	if (empty ())
		return false;

	if (0 == position () && !row ().empty ()) {
		// Special case: initial state after construction
		assert (!after_last ());
		if (i == 0 || i == 1) {
			position (1);
			return true;
		}
	}

	Row tmp;
	RowIdx ri = cursor ()->fetch (i, tmp);
	position (ri);
	bool fetched = !tmp.empty ();
	bool after_last = i >= 0 && !fetched;
	Base::after_last (after_last);
	if (after_last)
		last_row (ri - 1);
	else if (-1 == i)
		last_row (ri);
	row ().swap (tmp);
	return fetched;
}

void ResultSetImpl::check_scrollable () const
{
	check ();
	if (flags () & FLAG_FORWARD_ONLY)
		throw_exception ("ResultSet if forward-only");
}

void ResultSetImpl::check () const
{
	if (!cursor ())
		throw_exception ("ResultSet is closed");
}

void ResultSetImpl::check_row_valid () const
{
	check ();
	if (row ().empty ())
		throw_exception ("No current row");
}

const Variant& ResultSetImpl::field (Ordinal ord) const
{
	check_row_valid ();
	if (ord == 0 || ord > column_count ())
		throw_exception ("Invalid column index");
	return row () [ord - 1];
}

const NDBC::ColumnNames& ResultSetImpl::getColumnNames ()
{
	check ();
	if (Base::column_names ().empty ())
		Base::column_names (cursor ()->getColumnNames ());
	return Base::column_names ();
}

}

NIRVANA_VALUETYPE_IMPL (_exp_NDBC_ResultSet, NDBC::ResultSet, NDBC::ResultSetImpl)
