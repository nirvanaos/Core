/*
* Database connection module.
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
#include "PoolableStatement.h"
#include "ConnectionPoolImpl.h"

namespace NDBC {

PoolableStatementBase::PoolableStatementBase (PoolableConnection& conn,
	StatementBase::_ptr_type base) noexcept :
	conn_ (&conn),
	base_ (base)
{
	conn_->add_active_statement ();
}

PoolableStatementBase::~PoolableStatementBase ()
{
	if (conn_)
		conn_->remove_active_statement ();
}

void PoolableStatementBase::close () noexcept
{
	assert (conn_);
	conn_->remove_active_statement ();
	conn_ = nullptr;
	base_ = nullptr;
}

void PoolableStatementBase::cleanup (StatementBase::_ptr_type s)
{
	while (s->getMoreResults ())
		;
	s->getUpdateCount ();
	s->clearWarnings ();
}

}
