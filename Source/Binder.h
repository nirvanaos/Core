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
#ifndef NIRVANA_CORE_BINDER_H_
#define NIRVANA_CORE_BINDER_H_
#pragma once

#include "SyncDomain.h"
#include "Section.h"
#include "Synchronized.h"
#include "Module.h"
#include "BinderMemory.h"
#include <CORBA/RepId.h>
#include <Nirvana/Main.h>
#include <Nirvana/ModuleInit.h>
#include <IDL/CoreDomains.h>
#include "WaitableRef.h"
#include "Chrono.h"
#include "MapUnorderedStable.h"
#include "MapUnorderedUnstable.h"
#include "ORB/RemoteReferences.h"
#include "TimerAsyncCall.h"
#include "SystemInfo.h"
#include "BinaryMap.h"

namespace Nirvana {
namespace Core {

class ClassLibrary;
class Singleton;
class Executable;

/// Implements object binding and module loading.
class Binder
{
	/// To avoid priority inversion, if module loader deadline is too far,
	/// it will be temporary adjusted. TODO: config.h
	static const TimeBase::TimeT MODULE_LOADING_DEADLINE_MAX = 1 * TimeBase::SECOND;

	struct ObjectVal;

public:
	typedef CORBA::Internal::Interface::_ref_type InterfaceRef;
	template <class T>
	using Allocator = BinderMemory::Allocator <T>;

	Binder () :
		housekeeping_domains_on_ (false)
	{}

	~Binder ()
	{}

	static void initialize ();
	static void terminate () noexcept;

	struct BindResult
	{
		InterfaceRef itf;
		SyncContext* sync_context;

		BindResult () :
			sync_context (nullptr)
		{}

		BindResult (const ObjectVal* pf) noexcept;

		BindResult (BindResult&&) = default;

		BindResult& operator = (BindResult&&) = default;

		BindResult& operator = (const ObjectVal* pf) noexcept;
	};

	/// Implements System::bind_interface()
	static BindResult bind_interface (CORBA::Internal::String_in name, CORBA::Internal::String_in iid);

	template <class Itf>
	static CORBA::Internal::I_ref <Itf> bind_interface (CORBA::Internal::String_in name)
	{
		return bind_interface (name, CORBA::Internal::RepIdOf <Itf>::id).itf.template downcast <Itf> ();
	}

	static BindResult load_and_bind (int32_t mod_id, Nirvana::AccessDirect::_ptr_type binary,
		CORBA::Internal::String_in name, CORBA::Internal::String_in iid);

	template <class Itf>
	static CORBA::Internal::I_ref <Itf> load_and_bind (int32_t mod_id,
		Nirvana::AccessDirect::_ptr_type binary, CORBA::Internal::String_in name)
	{
		return load_and_bind (mod_id, binary, name, CORBA::Internal::RepIdOf <Itf>::id).itf
			.template downcast <Itf> ();
	}

	/// Bind executable.
	/// 
	/// \param mod Module interface.
	/// \param metadata Module OLF metadata section.
	/// \returns The Main interface pointer.
	inline static Main::_ptr_type bind (Executable& mod);

	/// Unbind executable.
	/// 
	/// \param mod The Nirvana::Module interface.
	/// \param metadata Module metadata.
	inline static void unbind (Executable& mod) noexcept;

	/// Unmarshal remote reference.
	/// 
	/// \tparam DomainKey Domain key type: ESIOP::ProtDomainId or const IIOP::ListenPoint&.
	/// \param domain Object endpoint.
	/// \param iid Primary interface id.
	/// \param addr IOR address.
	/// \param object_key Domain-relative object key.
	/// \param ORB_type The ORB type.
	/// \param components Tagged components.
	/// \returns CORBA::Object reference.
	template <class EndPoint>
	static CORBA::Object::_ref_type unmarshal_remote_reference (const EndPoint& domain,
		const IDL::String& iid, const IOP::TaggedProfileSeq& addr, const IOP::ObjectKey& object_key,
		uint32_t ORB_type, const IOP::TaggedComponentSeq& components, CORBA::Core::ReferenceRemoteRef& unconfirmed)
	{
		CORBA::Object::_ref_type ret;
		SYNC_BEGIN (sync_domain (), nullptr)
			ret = singleton_->remote_references_.unmarshal (domain, iid, addr, object_key, ORB_type,
				components, unconfirmed);
		SYNC_END ();
		return ret;
	}

	/// Get CORBA::Core::Domain reference.
	/// 
	/// \param domain Domain address.
	/// \returns Reference to CORBA::Core::Domain
	static Ref <CORBA::Core::DomainProt> get_domain (ESIOP::ProtDomainId domain)
	{
		Ref <CORBA::Core::DomainProt> ret;
		SYNC_BEGIN (sync_domain (), nullptr)
			ret = singleton_->remote_references_.get_domain (domain);
		SYNC_END ();
		return ret;
	}

	static uint_fast16_t get_module_bindings (AccessDirect::_ptr_type binary, PM::ModuleBindings& bindings);

	static Binder& singleton () noexcept
	{
		return singleton_;
	}

	static SyncDomain& sync_domain () noexcept
	{
		return sync_domain_;
	}

	static MemContext& memory () noexcept
	{
		return sync_domain_->mem_context ();
	}

	CORBA::Core::RemoteReferences& remote_references () noexcept
	{
		return remote_references_;
	}

	void erase_reference (const CORBA::OctetSeq& ref, const char* object_name) noexcept
	{
		if (object_name)
			object_map_.erase (object_name);
		remote_references_.erase (ref);
	}

	static bool initialized () noexcept
	{
		return initialized_;
	}

	static void heartbeat (const CORBA::Core::DomainAddress& domain) noexcept
	{
		try {
			Nirvana::Core::Synchronized _sync_frame (sync_domain (), nullptr);
			singleton_->remote_references_.heartbeat (domain);
			_sync_frame.return_to_caller_context ();
		} catch (...) {}
	}

	static void complex_ping (const CORBA::Core::DomainAddress& domain, CORBA::Internal::IORequest::_ptr_type rq)
	{
		SYNC_BEGIN (sync_domain (), nullptr)
			singleton_->remote_references_.complex_ping (domain, rq);
		_sync_frame.return_to_caller_context ();
		SYNC_END ();
	}

	static void clear_remote_references () noexcept
	{
		try {
			Nirvana::Core::Synchronized _sync_frame (sync_domain (), nullptr);
			singleton_->remote_references_.shutdown ();
		} catch (...) {}
	}

	void start_domains_housekeeping ()
	{
		// Called from sync context
		assert (&SyncContext::current () == &sync_domain ());
		if (!housekeeping_domains_on_) {
			const TimeBase::TimeT interval = CORBA::Core::DomainProt::HEARTBEAT_TIMEOUT;
			housekeeping_timer_domains_.set (0, interval, interval);
			housekeeping_domains_on_ = true;
		}
	}

	static void raise_exception (const siginfo_t& signal)
	{
		Binary* binary = nullptr;
		Thread* th = Thread::current_ptr ();
		if (th && th->executing ())
			binary = singleton_->binary_map_.find (signal.si_addr);
		if (binary)
			binary->raise_exception ((CORBA::SystemException::Code)signal.si_excode, signal.si_code);
		else
			CORBA::SystemException::_raise_by_code ((CORBA::SystemException::Code)signal.si_excode, signal.si_code);
	}

private:
	typedef CORBA::Internal::RepId RepId;
	typedef CORBA::Internal::RepId::Version Version;
	typedef CORBA::Internal::Interface::_ptr_type InterfacePtr;

	// ObjectKey class for object name and version.
	// Notice that ObjectKey does not store name and works like string view.
	class ObjectKey
	{
	public:
		ObjectKey (const char* id, size_t length) noexcept :
			name_ (id),
			full_length_ (length)
		{
			const char* sver = RepId::version (id, id + length);
			name_length_ = sver - id;
			version_ = Version (sver);
		}

		template <size_t cc>
		ObjectKey (char const (&ar) [cc]) noexcept :
			ObjectKey (ar, cc - 1)
		{}

		ObjectKey (const char* id) noexcept :
			ObjectKey (id, strlen (id))
		{}

		template <class A>
		ObjectKey (const std::basic_string <char, std::char_traits <char>, A>& id) noexcept :
			ObjectKey (id.c_str (), id.length ())
		{}

		ObjectKey (const CORBA::Internal::StringView <char>& id) noexcept :
			ObjectKey (id.c_str (), id.length ())
		{}

		bool name_eq (const ObjectKey& rhs) const noexcept
		{
			return name_length_ == rhs.name_length_ && std::equal (name_, name_ + name_length_, rhs.name_);
		}

		bool compatible (const ObjectKey& rhs) const noexcept
		{
			return name_eq (rhs) && version_.compatible (rhs.version_);
		}

		const char* name () const noexcept
		{
			return name_;
		}

		size_t name_length () const noexcept
		{
			return name_length_;
		}

		size_t full_length () const noexcept
		{
			return full_length_;
		}

		const Version& version () const noexcept
		{
			return version_;
		}

	private:
		const char* name_;
		size_t full_length_;
		size_t name_length_;
		Version version_;
	};

	struct ObjectHash
	{
		size_t operator () (const ObjectKey& key) const noexcept
		{
			size_t h = Hash::hash_bytes (key.name (), key.name_length ());
			return Hash::append_bytes (h, &key.version ().major, sizeof (key.version ().major));
		}
	};

	struct ObjectEq
	{
		size_t operator () (const ObjectKey& l, const ObjectKey& r) const noexcept
		{
			return l.name_length () == r.name_length () &&
				std::equal (l.name (), l.name () + l.name_length (), r.name ()) &&
				l.version ().major == r.version ().major;
		}
	};

	struct ObjectVal
	{
		InterfacePtr itf;
		SyncContext* sync_context;

		ObjectVal (InterfacePtr i, SyncContext* sc) :
			itf (i),
			sync_context (sc)
		{}
	};

	// Fast hash map without the iterator stability.
	typedef MapUnorderedUnstable <ObjectKey, ObjectVal, ObjectHash, ObjectEq, Allocator>
		ObjectMapBase;

	// Name->interface map.
	class ObjectMap : public ObjectMapBase
	{
	public:
		void insert (const char* name, InterfacePtr itf, SyncContext* sc);
		void erase (const char* name) noexcept;
		void merge (const ObjectMap& src);
		const ObjectVal* find (const ObjectKey& key) const;
	};

	// Map of the loaded modules.
	// Map must provide the pointer stability.
	// phmap::node_hash_map is declared as with pointer stability,
	// but it seems that not.
	typedef WaitableRef <Module*> ModulePtr;
	typedef MapUnorderedStable
		<int32_t, ModulePtr, std::hash <int32_t>,
		std::equal_to <int32_t>, Allocator> ModuleMap;

	// Module dependencies

	struct BindingHash
	{
		size_t operator () (const PM::ObjBinding& binding) const noexcept
		{
			size_t h = Hash::hash_bytes (binding.name ().data (), binding.name ().size ());
			uint32_t v = ((uint32_t)binding.major () << 16) | binding.minor ();
			h = Hash::append_bytes (h, &v, sizeof (v));
			return Hash::append_bytes (h, binding.itf_id ().data (), binding.itf_id ().size ());
		}
	};

	struct BindingEq
	{
		size_t operator () (const PM::ObjBinding& l, const PM::ObjBinding& r) const noexcept
		{
			return l.name () == r.name () && l.major () == r.major () && l.minor () == r.minor ()
				&& l.itf_id () == r.itf_id ();
		}
	};

	typedef SetUnorderedUnstable <PM::ObjBinding, BindingHash, BindingEq, Allocator> Dependencies;

	// Module binding context
	struct ModuleContext
	{
		Module* mod;

		// Synchronization context.
		//   If module is ClassLibrary, sync_context is free context.
		//   If module is Singleton, sync_context is the singleton synchronization domain.
		SyncContext& sync_context;

		// Module exports.
		ObjectMap exports;

		Dependencies dependencies;

		bool collect_dependencies;

		ModuleContext (SyncContext& sc) :
			mod (nullptr),
			sync_context (sc),
			collect_dependencies (false)
		{}

		ModuleContext (Module* m, bool depends = false) :
			mod (m),
			sync_context (m->sync_context ()),
			collect_dependencies (depends)
		{}
	};

	/// Bind module.
	/// 
	/// \param mod The Nirvana::Module interface.
	/// \param metadata Module metadata.
	/// \param mod_context Module binding context.
	///   If module is Executable, mod_context must be `nullptr`.
	/// 
	/// \returns Pointer to the ModuleStartup metadata structure, if found. Otherwise `nullptr`.
	const ModuleStartup* module_bind (Nirvana::Module::_ptr_type mod, const Section& metadata,
		ModuleContext* mod_context);

	void bind_and_init (Module& mod, ModuleContext& context);
	void terminate_and_unbind (Module& mod) noexcept;
	
	/// Unbind module.
	/// Must be called from the module synchronization context.
	static void module_unbind (Nirvana::Module::_ptr_type mod, const Section& metadata) noexcept;
	
	inline void remove_exports (const Section& metadata) noexcept;
	static void release_imports (Nirvana::Module::_ptr_type mod, const Section& metadata);

	BindResult bind_interface_sync (const ObjectKey& name, CORBA::Internal::String_in iid);
	
	BindResult find (const ObjectKey& name);

	static InterfaceRef query_interface (InterfacePtr itf, CORBA::Internal::String_in iid, const ObjectKey& name);

	static void query_interface (BindResult& ref, CORBA::Internal::String_in iid, const ObjectKey& name)
	{
		ref.itf = query_interface (ref.itf, iid, name);
	}

	Ref <Module> load (int32_t mod_id, AccessDirect::_ptr_type binary);
	void unload (Module* pmod) noexcept;

	BindResult load_and_bind_sync (int32_t mod_id, AccessDirect::_ptr_type binary,
		const ObjectKey& name, CORBA::Internal::String_in iid);

	uint_fast16_t get_module_bindings_sync (AccessDirect::_ptr_type binary, PM::ModuleBindings& bindings);

	void housekeeping_modules ();
	void delete_module (Module* mod) noexcept;
	void unload_modules ();

	static NIRVANA_NORETURN void throw_object_not_in_module (const ObjectKey& name, int32_t mod_id);

	class HousekeepingTimerModules : public TimerAsyncCall
	{
	protected:
		HousekeepingTimerModules () :
			TimerAsyncCall (sync_domain (), INFINITE_DEADLINE)
		{}

	private:
		virtual void run (const TimeBase::TimeT& signal_time);
	};

	void housekeeping_domains () noexcept;

	class HousekeepingTimerDomains : public TimerAsyncCall
	{
	protected:
		HousekeepingTimerDomains () :
			TimerAsyncCall (sync_domain (), INFINITE_DEADLINE)
		{}

	private:
		virtual void run (const TimeBase::TimeT& signal_time);
	};

	class Request;

private:
	ObjectMap object_map_;
	ModuleMap module_map_;
	BinaryMap binary_map_;
	CORBA::Core::RemoteReferences remote_references_;
	ImplStatic <HousekeepingTimerModules> housekeeping_timer_modules_;
	ImplStatic <HousekeepingTimerDomains> housekeeping_timer_domains_;
	bool housekeeping_domains_on_;

	static StaticallyAllocated <ImplStatic <SyncDomainCore> > sync_domain_;
	static StaticallyAllocated <Binder> singleton_;
	static bool initialized_;
};

inline Binder::BindResult::BindResult (const ObjectVal* pf) noexcept :
	sync_context (nullptr)
{
	if (pf) {
		itf = pf->itf;
		sync_context = pf->sync_context;
	}
}

inline Binder::BindResult& Binder::BindResult::operator = (const Binder::ObjectVal* pf) noexcept
{
	if (pf) {
		itf = pf->itf;
		sync_context = pf->sync_context;
	}

	return *this;
}

}
}

#endif
