nirvana_module (InterfaceRepository OPTIONS -proxy IDL_ROOT "${NIRVANA_SDK_DIR}/include/CORBA" IDL_FILES InterfaceRepository.idl)
target_sources (InterfaceRepository PRIVATE module.cpp)
target_compile_definitions (InterfaceRepository PRIVATE NIRVANA_NO_BAD_ALLOC)

