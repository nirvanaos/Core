nirvana_module (PacMan IDL_FILES PacFactory.idl)

target_sources (PacMan PRIVATE
	Connection.cpp
	man_factory.cpp
	module.cpp
	pac_factory.cpp
	Packages.cpp
	PacMan.cpp
)

target_compile_definitions (PacMan PRIVATE NIRVANA_NO_BAD_ALLOC)
