nirvana_module (SQLite)

target_sources (SQLite PRIVATE
	Connection.cpp
	Cursor.cpp
	Driver.cpp
	filesystem.cpp
	Global.cpp
	mem_methods.cpp
	module.cpp
	mutex_methods.cpp
	PreparedStatement.cpp
	StatementBase.cpp

	sqlite/sqlite3.c
)

target_compile_definitions (SQLite PRIVATE SQLITE_CUSTOM_INCLUDE=../sqlite_config.h NIRVANA_NO_BAD_ALLOC)
