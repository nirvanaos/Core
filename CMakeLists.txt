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

add_custom_command (TARGET Core POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${NIRVANA_CORE_SDK_DIR}/${NIRVANA_HOST_TYPE}/bin/${NIRVANA_TARGET_PLATFORM}/$<CONFIG>"
		${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
add_subdirectory (Test/TestCore)

add_executable (Nirvana)
add_subdirectory (Exports)

set (idl_nirvana "${NIRVANA_SDK_DIR}/include/Nirvana")
set (idl_orb "${NIRVANA_SDK_DIR}/include/CORBA")

set (proxy_idl
	${idl_nirvana}/BindError.idl
	${idl_nirvana}/Domains.idl
	${idl_nirvana}/File.idl
	${idl_nirvana}/FileStat.idl
	${idl_nirvana}/NDBC_ex.idl
	${idl_nirvana}/Packages.idl
	${idl_nirvana}/Shell.idl

	${idl_orb}/AMI.idl
	${idl_orb}/Components.idl
	${idl_orb}/Context.idl
	${idl_orb}/CORBA_Stream.idl
	${idl_orb}/CORBA_types.idl
	${idl_orb}/CosEventChannelAdmin.idl
	${idl_orb}/CosEventComm.idl
	${idl_orb}/CosLwLogAdministrator.idl
	${idl_orb}/CosLwLogConsumer.idl
	${idl_orb}/CosLwLogProducer.idl
	${idl_orb}/CosLwLogService.idl
	${idl_orb}/CosLwLogStatus.idl
	${idl_orb}/CosNaming.idl
	${idl_orb}/CosTime.idl
	${idl_orb}/CosTimerEvent.idl
	${idl_orb}/CosTypedEventChannelAdmin.idl
	${idl_orb}/CosTypedEventComm.idl
	${idl_orb}/Current.idl
	${idl_orb}/DomainManager.idl
	${idl_orb}/Dynamic.idl
#	${idl_orb}/DynamicServant.idl
	${idl_orb}/FT.idl
	${idl_orb}/Messaging.idl
	${idl_orb}/NirvanaDGC.idl
#	${idl_orb}/NVList.idl
#	${idl_orb}/ObjectFactory.idl
#	${idl_orb}/ORB.idl
#	${idl_orb}/Policy.idl
	${idl_orb}/Pollable.idl
#	${idl_orb}/RefCnt.idl
	${idl_orb}/Request.idl
#	${idl_orb}/SFloat.idl
	${idl_orb}/TCKind.idl
	${idl_orb}/TimeBase.idl

	${idl_orb}/ccm/CCM_Base.idl
	${idl_orb}/ccm/CCM_Cookie.idl
	${idl_orb}/ccm/CCM_Events.idl
	${idl_orb}/ccm/CCM_Exceptions.idl
	${idl_orb}/ccm/CCM_Home.idl
	${idl_orb}/ccm/CCM_Navigation.idl
	${idl_orb}/ccm/CCM_Object.idl
	${idl_orb}/ccm/CCM_Receptacle.idl
)

nirvana_compile_idl (Nirvana IDL_FILES ${proxy_idl} OPTIONS -proxy -DCCM_LW -DCCM_NOEVENT)
nirvana_compile_idl (Nirvana IDL_FILES ${core_idl} OPTIONS -proxy OUT_H ${core_idl_h_dir})
target_link_libraries (Nirvana PRIVATE Core Port)
target_link_options (Nirvana PRIVATE "LINKER:/entry:nirvana_startup")
