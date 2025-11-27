nirvana_module (dbc OPTIONS -proxy IDL_ROOT "${NIRVANA_SDK_DIR}/include/Nirvana" IDL_FILES NDBC.idl)
target_sources (dbc PRIVATE
	Manager.cpp
	module.cpp
	PollableConnection.cpp
	Pool.cpp
	PoolableStatement.cpp
	ResultSetImpl.cpp
)
