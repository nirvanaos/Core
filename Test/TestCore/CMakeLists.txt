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
)
target_compile_definitions (TestCore PRIVATE GTEST_HAS_POSIX_RE=0 GTEST_HAS_SEH=0)

# With CMAKE_SYSTEM_NAME Generic
# Google Test can not find .exe file without the explicitly specified suffix
if (${NIRVANA_HOST_TYPE} STREQUAL "Windows")
set_target_properties (TestCore PROPERTIES SUFFIX ".exe")
endif ()

include (GoogleTest)
gtest_discover_tests (TestCore
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	PROPERTIES DISCOVERY_TIMEOUT 120
)
