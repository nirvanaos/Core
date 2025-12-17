# Core library

add_library (Core STATIC)

target_include_directories (Core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Source)
target_compile_definitions (Core PUBLIC NIRVANA_HOST_PLATFORM=${NIRVANA_HOST_PLATFORM})

add_subdirectory (Source)

set (core_idl_h_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
set (core_idl)
add_subdirectory (IDL)
nirvana_compile_idl (Core IDL_FILES ${core_idl} OPTIONS -no_proxy OUT_H ${core_idl_h_dir})
nirvana_compile_idl (Core IDL_FILES ${CMAKE_CURRENT_SOURCE_DIR}/PacMan/PacFactory.idl OPTIONS -client)

# #[===[
target_precompile_headers (Core PUBLIC
	<Nirvana/Nirvana.h>
	"Source/MapOrderedStable.h"
	"Source/MapOrderedUnstable.h"
	"Source/MapUnorderedStable.h"
	"Source/MapUnorderedUnstable.h"
)
# ]===]

# Core test
add_custom_command (TARGET Core POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${NIRVANA_CORE_SDK_DIR}/${NIRVANA_HOST_TYPE}/bin/${NIRVANA_TARGET_PLATFORM}/$<CONFIG>"
		${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

add_subdirectory (Test/TestCore)

# Nirvana executable

add_executable (Nirvana)
add_subdirectory (Exports)

set (idl_nirvana "${NIRVANA_SDK_DIR}/include/Nirvana")
set (idl_orb "${NIRVANA_SDK_DIR}/include/CORBA")

set (proxy_idl
	Nirvana/BindError.idl
	Nirvana/Domains.idl
	Nirvana/File.idl
	Nirvana/FileStat.idl
	Nirvana/NDBC_ex.idl
	Nirvana/Packages.idl
	Nirvana/Shell.idl

	CORBA/AMI.idl
	CORBA/Components.idl
	CORBA/Context.idl
	CORBA/CORBA_Stream.idl
	CORBA/CORBA_types.idl
	CORBA/CosEventChannelAdmin.idl
	CORBA/CosEventComm.idl
	CORBA/CosLwLogAdministrator.idl
	CORBA/CosLwLogConsumer.idl
	CORBA/CosLwLogProducer.idl
	CORBA/CosLwLogService.idl
	CORBA/CosLwLogStatus.idl
	CORBA/CosNaming.idl
	CORBA/CosTime.idl
	CORBA/CosTimerEvent.idl
	CORBA/CosTypedEventChannelAdmin.idl
	CORBA/CosTypedEventComm.idl
	CORBA/Current.idl
	CORBA/DomainManager.idl
	CORBA/Dynamic.idl
#	CORBA/DynamicServant.idl
	CORBA/FT.idl
# CORBA/IORequest.idl
	CORBA/Messaging.idl
	CORBA/NirvanaDGC.idl
#	CORBA/NVList.idl
#	CORBA/ObjectFactory.idl
#	CORBA/ORB.idl
	CORBA/Policy.idl
	CORBA/Pollable.idl
	CORBA/PortableServer.idl
#	CORBA/RefCnt.idl
	CORBA/Request.idl
#	CORBA/SFloat.idl
	CORBA/TCKind.idl
	CORBA/TimeBase.idl

	CORBA/ccm/CCM_Base.idl
	CORBA/ccm/CCM_Cookie.idl
	CORBA/ccm/CCM_Events.idl
	CORBA/ccm/CCM_Exceptions.idl
	CORBA/ccm/CCM_Home.idl
	CORBA/ccm/CCM_Navigation.idl
	CORBA/ccm/CCM_Object.idl
	CORBA/ccm/CCM_Receptacle.idl
)

nirvana_compile_idl (Nirvana IDL_FILES ${proxy_idl} OPTIONS -proxy -DCCM_LW -DCCM_NOEVENT IDL_ROOT "${NIRVANA_SDK_DIR}/include")
nirvana_compile_idl (Nirvana IDL_FILES ${core_idl} OPTIONS -proxy OUT_H ${core_idl_h_dir})
target_link_libraries (Nirvana PRIVATE Core Port)
target_include_directories (Nirvana PRIVATE Source/ORB)

# Core modules

add_subdirectory (dbc)
add_subdirectory (DecCalc)
add_subdirectory (InterfaceRepository)
add_subdirectory (PacMan)
add_subdirectory (SFloat)
add_subdirectory (shell)
add_subdirectory (SQLite)
