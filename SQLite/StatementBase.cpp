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
#include "StatementBase.h"
#include "Cursor.h"

namespace SQLite {

Connection& StatementBase::connection ()
{
	if (!connection_)
		throw CORBA::OBJECT_NOT_EXIST ();
	if (connection_->isClosed ()) {
		close ();
		throw CORBA::OBJECT_NOT_EXIST ();
	}
	return *connection_;
}

void StatementBase::prepare (const IDL::String& sql, unsigned flags)
{
	finalize ();
	warnings_.clear ();

	const char* tail = sql.c_str ();
	const char* end = tail + sql.size ();
	Statements statements;
	try {
		for (;;) {
			sqlite3_stmt* stmt = nullptr;
			connection ().check_result (sqlite3_prepare_v3 (connection (), tail, (int)(end - tail), flags, &stmt, &tail));
			assert (stmt);
			statements.emplace_back (stmt);
			if (!*tail)
				break;
		}
	} catch (...) {
		while (!statements.empty ()) {
			sqlite3_finalize (statements.back ());
			statements.pop_back ();
		}
		throw;
	}
	statements_ = std::move (statements);
	cur_statement_ = statements_.size ();
}

void StatementBase::finalize (bool silent) noexcept
{
	assert (connection_);

	result_set_ = nullptr;
	changed_rows_ = -1;
	if (!statements_.empty ()) {
		Connection& conn = *connection_;
		if (!silent && conn.isClosed ())
			silent = true;
		Statements statements (std::move (statements_));
		cur_statement_ = 0;
		change_version ();
		for (auto stmt : statements) {
			int err = sqlite3_finalize (stmt);
			if (!silent)
				conn.check_warning (err);
		}
	}
}

void StatementBase::reset () noexcept
{
	if (cur_statement_ > 0) {
		size_t i = cur_statement_;
		do {
			check_warning (sqlite3_reset (statements_ [--i]));
		} while (i > 0);
		cur_statement_ = 0;
		change_version ();
	}
}

void StatementBase::check_warning (int err) noexcept
{
	if (err) {
		try {
			warnings_.emplace_back (connection_->create_warning (err));
		} catch (...) {
		}
	}
}

bool StatementBase::execute_next (bool resultset)
{
	assert (cur_statement_ < statements_.size ());

	result_set_ = nullptr;
	changed_rows_ = -1;

	sqlite3_stmt* stmt = statements_ [cur_statement_++];
	int step_result = step (stmt);
	changed_rows_ = sqlite3_changes (connection ());

	if (resultset) {
		int column_cnt = sqlite3_column_count (stmt);
		if (SQLITE_ROW == step_result || column_cnt > 0) {
			NDBC::Row row;
			if (SQLITE_ROW == step_result)
				row = Cursor::get_row (stmt);

			NDBC::Cursor::_ref_type cursor = CORBA::make_reference <Cursor> (
				std::ref (*this), stmt)->_this ();
			
			PortableServer::ServantBase::_ref_type servant (servant_);
			NDBC::StatementBase::_ref_type parent = NDBC::StatementBase::_narrow (servant->_default_POA ()->servant_to_reference (servant));

			result_set_ = NDBC::ResultSet::_factory->create (parent, column_cnt, NDBC::ResultSet::FLAG_FORWARD_ONLY,
				std::move (cursor), std::move (row));

			return true;
		}
	}
	return false;
}

int StatementBase::step (sqlite3_stmt* stmt)
{
	auto ver = version ();

	int step_result = sqlite3_step (stmt);

	if (ver != version ())
		throw CORBA::INVALID_TRANSACTION (); // TODO: Maybe other exception?

	switch (step_result) {
	case SQLITE_ROW:
	case SQLITE_DONE:
		break;

	default:
		if (connection_)
			connection_->check_result (step_result);
	}

	return step_result;
}

uint32_t StatementBase::executeUpdate ()
{
	uint32_t cnt = 0;
	execute_first (false);
	for (;;) {
		result_set_ = nullptr;
		cnt += changed_rows_;
		if (cur_statement_ >= statements_.size ())
			break;
		execute_next (false);
	}
	return cnt;
}

}
