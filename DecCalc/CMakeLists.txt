nirvana_module (DecCalc)
target_sources (DecCalc PRIVATE
	DecCalcImpl.cpp
	module.cpp
)
target_compile_definitions (DecCalc PRIVATE NIRVANA_NO_BAD_ALLOC)
