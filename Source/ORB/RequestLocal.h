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
#ifndef NIRVANA_ORB_CORE_REQUESTLOCAL_H_
#define NIRVANA_ORB_CORE_REQUESTLOCAL_H_
#pragma once

#include "RequestLocalBase.h"
#include "ProxyManager.h"
#include "CallbackRef.h"

namespace CORBA {
namespace Core {

class NIRVANA_NOVTABLE RequestLocal : public RequestLocalBase
{
protected:
	RequestLocal (ProxyManager& proxy, Internal::OperationIndex op_idx,
		Nirvana::Core::Heap* callee_memory, unsigned response_flags) noexcept;

	ProxyManager& proxy () const noexcept
	{
		return *proxy_;
	}

	Internal::OperationIndex op_idx () const noexcept
	{
		return op_idx_;
	}

	virtual void invoke () override;
	void invoke_sync () noexcept;

private:
	servant_reference <ProxyManager> proxy_;
	Internal::OperationIndex op_idx_;
};

class NIRVANA_NOVTABLE RequestLocalOneway : public RequestLocal
{
	typedef RequestLocal Base;

protected:
	RequestLocalOneway (ProxyManager& proxy, Internal::OperationIndex op_idx,
		Nirvana::Core::Heap* callee_memory, unsigned response_flags) noexcept :
		Base (proxy, op_idx, callee_memory, response_flags)
	{}

	virtual void invoke ();

private:
	class Runnable : public Nirvana::Core::Runnable
	{
	public:
		Runnable (RequestLocalOneway& rq) :
			request_ (&rq)
		{}

	private:
		virtual void run () noexcept override;

	private:
		servant_reference <RequestLocalOneway> request_;
	};

	void run () noexcept
	{
		assert (&Nirvana::Core::SyncContext::current () == &proxy ().get_sync_context (op_idx ()));
		Base::invoke_sync ();
	}

};

class NIRVANA_NOVTABLE RequestLocalAsync : public RequestLocalOneway
{
	typedef RequestLocalOneway Base;

protected:
	RequestLocalAsync (ProxyManager & proxy, Internal::OperationIndex op_idx,
		Nirvana::Core::Heap* callee_memory, unsigned response_flags, CallbackRef&& callback) noexcept :
		Base (proxy, op_idx, callee_memory, response_flags),
		callback_ (std::move (callback))
	{
		assert (callback_);
	}

	virtual void cancel () noexcept override;
	virtual void finalize () noexcept override;

private:
	CallbackRef callback_;
};

}
}

#endif
