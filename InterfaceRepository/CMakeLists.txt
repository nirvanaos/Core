nirvana_module (InterfaceRepository OPTIONS -proxy IDL_ROOT "${NIRVANA_SDK_DIR}/include/CORBA" IDL_FILES InterfaceRepository.idl)
target_sources (dbc PRIVATE module.cpp)

