nirvana_module (SFloat)
target_sources (SFloat PRIVATE
	module.cpp
	SFloat16.cpp
	state.cpp
)	
target_compile_definitions (SFloat PRIVATE NIRVANA_NO_BAD_ALLOC)
