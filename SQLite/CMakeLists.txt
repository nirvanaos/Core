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
