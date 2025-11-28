nirvana_module (shell)

target_sources (shell PRIVATE
	module.cpp
	pacman.cpp
)

target_compile_definitions (shell PRIVATE NIRVANA_NO_BAD_ALLOC)
