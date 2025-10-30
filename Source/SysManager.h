/// \file
/*
* Nirvana Core.
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
#ifndef NIRVANA_CORE_SYSMANAGER_H_
#define NIRVANA_CORE_SYSMANAGER_H_
#pragma once

#include <CORBA/Server.h>
#include <Nirvana/Domains_s.h>
#include <Port/SysDomain.h>
#include "ORB/ESIOP.h"
#include "ORB/ServantProxyObject.h"

namespace Nirvana {
namespace Core {

/// System manager
class SysManager :
	public CORBA::servant_traits <Nirvana::SysManager>::Servant <SysManager>,
	private Port::SysDomain
{
	static const TimeBase::TimeT PROCESS_STARTUP_TIMEOUT = 10 * TimeBase::SECOND;

	typedef CORBA::servant_traits <Nirvana::SysManager>::Servant <SysManager> Servant;

public:
	SysManager (CORBA::Object::_ptr_type comp) :
		Servant (comp),
		shutdown_started_ (false)
	{
		proxy_.construct (CORBA::Core::object2proxy (CORBA::Core::servant2object (this)));
	}

	~SysManager ()
	{}

	// Nirvana::SysManager

	Nirvana::ProtDomain::_ref_type create_prot_domain (uint16_t platform)
	{
		return Nirvana::ProtDomain::_narrow (prot_domain_ref (
			Port::SysDomain::create_prot_domain (platform, IDL::String (), 0)));
	}

	static Nirvana::ProtDomain::_ref_type create_prot_domain_as_user (uint16_t platform,
		const IDL::String& user, const IDL::String& password)
	{
		throw_NO_IMPLEMENT ();
	}

	void shutdown (unsigned flags) noexcept;

	// Internal core API

	class AsyncCallContext
	{
	public:
		SysManager& sys_manager () const noexcept
		{
			return *implementation_;
		}

	private:
		friend class SysManager;
		Ref <CORBA::Core::ServantProxyObject> proxy_;
		SysManager* implementation_;
	};

	class AsyncCall :
		public AsyncCallContext,
		public Runnable
	{
	protected:
		AsyncCall (AsyncCallContext&& context) noexcept :
			AsyncCallContext (std::move (context))
		{}
	};

	template <class RunnableClass, class ... Args>
	static bool async_call (const DeadlineTime& deadline, Args&& ... args)
	{
		AsyncCallContext ctx;
		if (get_call_context (ctx)) {
			SyncContext& sync_context = ctx.proxy_->sync_context ();
			ExecDomain::async_call <RunnableClass> (deadline,
				sync_context, nullptr, std::move (ctx), std::forward <Args> (args)...);
			return true;
		}
		return false;
	}

	/// Receive shutdown Runnable
	class ReceiveShutdown : public AsyncCall
	{
	public:
		ReceiveShutdown (AsyncCallContext&& ctx, unsigned flags) noexcept :
			Nirvana::Core::SysManager::AsyncCall (std::move (ctx)),
			flags_ (flags)
		{}

	protected:
		void run () noexcept override;

	private:
		unsigned flags_;
	};

	/// Called from the port level
	/// 
	/// \param domain_id Protection domain id.
	/// \param platform Platform id.
	/// \param user User id.
	void domain_created (ESIOP::ProtDomainId domain_id, uint_fast16_t platform,
		SecurityId&& user) noexcept
	{
		Port::SysDomain::on_domain_start (domain_id);
		if (!shutdown_started_) {
			try {
				domains_.emplace (domain_id, platform, std::move (user));
				return;
			} catch (...) {}
		}
		ESIOP::send_shutdown (domain_id, 0);
	}

	/// Called from the port level
	/// 
	/// \param domain_id Protection domain id.
	void domain_destroyed (ESIOP::ProtDomainId domain_id) noexcept
	{
		domains_.erase (domain_id);
		if (shutdown_started_ && domains_.empty ())
			final_shutdown ();
	}

private:
	void final_shutdown ()
	{
		proxy_->operator = (nullptr);
		Scheduler::internal_shutdown (shutdown_flags_);
	}

	static bool get_call_context (AsyncCallContext& ctx);

	static CORBA::Object::_ref_type prot_domain_ref (ESIOP::ProtDomainId domain_id);

	struct DomainInfo
	{
		SecurityId user;
		uint_fast16_t platform;

		DomainInfo (SecurityId&& _user, uint_fast16_t _platform) :
			user (std::move (_user)),
			platform (_platform)
		{}
	};

	typedef MapUnorderedUnstable <ESIOP::ProtDomainId, DomainInfo, std::hash <ESIOP::ProtDomainId>,
		std::equal_to <ESIOP::ProtDomainId>, UserAllocator> DomainMap;

	class DomainsImpl
	{
	public:
		void emplace (ESIOP::ProtDomainId domain_id, uint_fast16_t platform, SecurityId&& user)
		{
			domains_.emplace (std::piecewise_construct, std::forward_as_tuple (domain_id),
				std::forward_as_tuple (std::move (user), platform));
		}

		void erase (ESIOP::ProtDomainId domain_id) noexcept
		{
			domains_.erase (domain_id);
		}

		bool empty () const noexcept
		{
			return domains_.empty ();
		}

		void shutdown (unsigned flags)
		{
			for (const auto& d : domains_) {
				ESIOP::send_shutdown (d.first, flags);
			}
		}

	private:
		DomainMap domains_;
	};

	class DomainsDummy
	{
	public:
		void emplace (ESIOP::ProtDomainId domain_id, uint_fast16_t platform, SecurityId&& user)
		{}

		void erase (ESIOP::ProtDomainId domain_id)
		{}

		static bool empty () noexcept
		{
			return true;
		}

		void shutdown ()
		{}
	};

	typedef std::conditional_t <SINGLE_DOMAIN, DomainsDummy, DomainsImpl> Domains;

	Domains domains_;
	bool shutdown_started_;
	unsigned shutdown_flags_;

	// Singleton proxy
	static StaticallyAllocated <LockablePtrT <CORBA::Core::ServantProxyObject> > proxy_;
};

}

}

#endif
