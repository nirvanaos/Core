add_executable (TestCore
	TestAtomicPtr.cpp
	TestFileLock.cpp
	TestHeap.cpp
	TestHeapDirectory.cpp
	TestMemory.cpp
	TestPreallocatedStack.cpp
	TestPriorityQueue.cpp
	TestSkipList.cpp
	TestStack.cpp
	TestWeakRef.cpp
)

target_include_directories (TestCore PRIVATE "${NIRVANA_CORE_SDK_DIR}/Include")
target_link_directories (TestCore PRIVATE "${NIRVANA_CORE_SDK_DIR}/${NIRVANA_HOST_TYPE}/lib/${NIRVANA_TARGET_PLATFORM}/$<CONFIG>")
target_link_libraries (TestCore PRIVATE
	Core
	Port
	MockImport
	mockhost
	crtl-main
	googletest-nirvana
	pe
)
target_compile_definitions (TestCore PRIVATE GTEST_HAS_POSIX_RE=0 GTEST_HAS_SEH=0)

include (GoogleTest)
gtest_discover_tests (TestCore
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	PROPERTIES DISCOVERY_TIMEOUT 60
)
