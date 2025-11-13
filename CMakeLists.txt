add_library (Core STATIC)

target_include_directories (Core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Source)
target_compile_definitions (Core PUBLIC NIRVANA_HOST_PLATFORM=${NIRVANA_HOST_PLATFORM})

add_subdirectory (Source)

set (core_idl)
add_subdirectory (IDL)
nirvana_module_idl (Core IDL_FILES ${core_idl} OPTIONS -no_proxy)
nirvana_module_idl (Core IDL_FILES ${CMAKE_CURRENT_SOURCE_DIR}/PacMan/PacFactory.idl OPTIONS -client)

# #[===[
target_precompile_headers (Core PUBLIC
	<Nirvana/Nirvana.h>
	"Source/MapOrderedStable.h"
	"Source/MapOrderedUnstable.h"
	"Source/MapUnorderedStable.h"
	"Source/MapUnorderedUnstable.h"
)
# ]===]
