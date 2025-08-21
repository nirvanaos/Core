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
#ifndef PACMAN_CONNECTION_H_
#define PACMAN_CONNECTION_H_
#pragma once

#include <Nirvana/Nirvana.h>
#include <Nirvana/NDBC.h>
#include "SemVer.h"
#include "version.h"

class Connection
{
public:
	static const char connect_rwc [];
	static const char connect_rw [];
	static const char connect_ro [];

	static NDBC::ConnectionPool::_ref_type create_pool (const char* url, unsigned max_keep, unsigned max_create)
	{
		return NDBC::the_manager->createConnectionPool (SQLite::driver, url, nullptr, nullptr, max_keep, max_create, 0);
	}

	Connection (const char* url) :
		connection_ (SQLite::driver->connect (url, nullptr, nullptr))
	{
		assert (connection_);
	}

	Connection (NDBC::ConnectionPool::_ptr_type pool) :
		connection_ (pool->getConnection ())
	{
		assert (connection_);
	}

	Connection (const Connection&) = delete;
	Connection (Connection&&) = delete;

	~Connection ()
	{
		if (connection_)
			connection_->close ();
	}

	Connection& operator = (const Connection&) = delete;
	Connection& operator = (Connection&&) = delete;

	template <class Itf>
	class StatementT
	{
	public:
		StatementT () noexcept
		{}

		StatementT (typename Itf::_ref_type&& ref) noexcept :
			ref_ (std::move (ref))
		{}

		StatementT (StatementT&& src) noexcept :
			ref_ (std::move (src.ref_))
		{}

		StatementT (const StatementT& src) = delete;

		~StatementT ()
		{
			if (ref_)
				ref_->close ();
		}

		typename Itf::_ptr_type operator -> () const noexcept
		{
			return ref_;
		}

		StatementT& operator = (StatementT&& src) noexcept
		{
			if (ref_)
				ref_->close ();
			ref_ = std::move (src.ref_);
			return *this;
		}

		StatementT& operator = (const StatementT& src) = delete;

	private:
		typename Itf::_ref_type ref_;
	};

	using PreparedStatement = StatementT <NDBC::PreparedStatement>;
	using Statement = StatementT <NDBC::Statement>;

	void get_binding (const IDL::String& name, Nirvana::PlatformId platform,
		Nirvana::PM::Binding& binding)
	{
		const char* name_begin = name.data ();
		const char* sver = CORBA::Internal::RepId::version (name_begin, name_begin + name.size ());
		CORBA::Internal::RepId::Version version (sver);
		NDBC::ResultSet::_ref_type rs;
		try {
			auto stm = get_statement ("SELECT id,path,platform,flags"
				" FROM module JOIN export ON export.module=module.id JOIN binary ON binary.module=module.id"
				" WHERE export.name=? AND platform=? AND major=? AND minor>=?");
			stm->setString (1, IDL::String (name_begin, sver - name_begin));
			stm->setInt (2, platform);
			stm->setInt (3, version.major);
			stm->setInt (4, version.minor);
			rs = stm->executeQuery ();
		} catch (const NDBC::SQLException& ex) {
			Nirvana::BindError::Error err;
			err.info ().s (ex.error ().sqlState ());
			err.info ()._d (Nirvana::BindError::Type::ERR_MESSAGE);
			throw err;
		}

		if (rs->next ()) {
			binding.module_id (rs->getInt (1));
			binding.binary_path (rs->getString (2));
			binding.platform (rs->getInt (3));
			binding.module_flags (rs->getSmallInt (4));
		} else {
			Nirvana::BindError::Error err;
			err.info ().s (name);
			err.info ()._d (Nirvana::BindError::Type::ERR_OBJ_NAME);
			throw err;
		}
	}

	IDL::String get_module_name (Nirvana::ModuleId id)
	{
		auto stm = get_statement ("SELECT name,version,prerelease FROM module WHERE id=?");
		stm->setInt (1, id);
		NDBC::ResultSet::_ref_type rs = stm->executeQuery ();
		IDL::String name;
		if (rs->next ()) {
			SemVer svname (rs->getString (1), rs->getBigInt (2), rs->getString (3));
			name = svname.to_string ();
		}
		return name;
	}

	Nirvana::PM::Packages::Modules get_module_dependencies (Nirvana::ModuleId id)
	{
		Nirvana::throw_NO_IMPLEMENT ();
	}

	Nirvana::PM::Packages::Modules get_dependent_modules (Nirvana::ModuleId id)
	{
		Nirvana::throw_NO_IMPLEMENT ();
	}

	void get_module_bindings (Nirvana::ModuleId id, Nirvana::PM::ModuleBindings& metadata)
	{
		auto stm = get_statement (
			"SELECT name,major,minor,interface,type FROM export WHERE module=? ORDER BY name,major,minor;"
			"SELECT name,version,interface FROM import WHERE module=? ORDER BY name,version,interface;"
		);
		stm->setInt (1, id);

		auto rs = stm->executeQuery ();

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

	PreparedStatement get_statement (const char* sql)
	{
		return connection_->prepareStatement (sql, NDBC::ResultSet::Type::TYPE_FORWARD_ONLY, 0);
	}

	Statement get_statement ()
	{
		return connection_->createStatement (NDBC::ResultSet::Type::TYPE_FORWARD_ONLY);
	}

	NDBC::Connection::_ptr_type operator -> () const noexcept
	{
		return connection_;
	}

	static NIRVANA_NORETURN void on_sql_exception (NDBC::SQLException& ex);

	void commit ()
	{
		try {
			connection_->commit ();
			connection_->close ();
			connection_ = nullptr;
		} catch (NDBC::SQLException& ex) {
			on_sql_exception (ex);
		}
	}

	void rollback ()
	{
		try {
			connection_->rollback (nullptr);
			connection_->close ();
			connection_ = nullptr;
		} catch (NDBC::SQLException& ex) {
			on_sql_exception (ex);
		}
	}

	NDBC::Connection::_ptr_type connection () const noexcept
	{
		return connection_;
	}

private:
	NDBC::Connection::_ref_type connection_;
};

#endif
