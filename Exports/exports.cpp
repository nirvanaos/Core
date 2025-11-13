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
#include "../Source/Memory.h"
#include "../Source/CoreMemory.h"
#include "../Source/FileAccessBuf.h"
#include "../Source/Shell.h"
#include "../Source/ORB/ExceptionHolder.h"
#include <CORBA/ccm/CCM_Cookie_s.h>
#include "../Source/ORB/ObjectFactory.h"
#include "../Source/ORB/ORB.h"
#include "../Source/Debugger.h"
#include "../Source/the_security.h"
#include "../Source/System.h"
#include "../Source/POSIX.h"
#include "../Source/CoreModule.h"

NIRVANA_EXPORT_STATIC (_exp_Nirvana_the_memory, "Nirvana/the_memory", Nirvana::Core::Memory)
NIRVANA_EXPORT_STATIC (_exp_Nirvana_the_debugger, "Nirvana/the_debugger", Nirvana::Core::Debugger)
NIRVANA_EXPORT_STATIC (_exp_Nirvana_the_posix, "Nirvana/the_posix", Nirvana::Static_the_posix)

namespace Nirvana {

NIRVANA_STATIC_IMPORT NIRVANA_CONSTINIT ImportInterfaceT <Memory> the_memory = { OLF_IMPORT_INTERFACE,
	"Nirvana/the_memory", CORBA::Internal::RepIdOf <Memory>::id, Core::CoreMemory::_bridge ()};

NIRVANA_STATIC_IMPORT NIRVANA_CONSTINIT ImportInterfaceT <Debugger> the_debugger = { OLF_IMPORT_INTERFACE,
	"Nirvana/the_debugger", CORBA::Internal::RepIdOf <Debugger>::id, Core::Debugger::_bridge ()};

NIRVANA_STATIC_IMPORT ImportInterfaceT <Module> the_module = { OLF_IMPORT_INTERFACE,
	"Nirvana/the_module", CORBA::Internal::RepIdOf <Module>::id, Core::g_core_module.reference ()._bridge ()};

NIRVANA_STATIC_IMPORT NIRVANA_CONSTINIT ImportInterfaceT <POSIX> the_posix = { OLF_IMPORT_INTERFACE,
	"Nirvana/the_posix", CORBA::Internal::RepIdOf <POSIX>::id, Static_the_posix::_bridge ()};

}

NIRVANA_EXPORT_FACTORY (_exp_Nirvana_AccessBuf, Nirvana::AccessBuf)
NIRVANA_EXPORT_FACTORY (_exp_Messaging_ExceptionHolder, Messaging::ExceptionHolder)
NIRVANA_EXPORT_FACTORY (_exp_Components_Cookie, Components::Cookie)

NIRVANA_EXPORT_STATIC (_exp_Nirvana_the_shell, "Nirvana/the_shell", Nirvana::Static_the_shell)

NIRVANA_EXPORT_PSEUDO (_exp_CORBA_Internal_object_factory, CORBA::Internal::Static_object_factory)
NIRVANA_EXPORT_PSEUDO (_exp_CORBA_the_orb, CORBA::Static_the_orb)
NIRVANA_EXPORT_PSEUDO (_exp_Nirvana_the_system, Nirvana::Static_the_system)
NIRVANA_EXPORT_PSEUDO (_exp_Nirvana_the_security, Nirvana::Static_the_security)
