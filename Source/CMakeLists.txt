target_sources (Core PRIVATE
	append_path.cpp
	AtExit.cpp
	BackOff.cpp
	Binary.cpp
	Binder.cpp
	BinderMemory.cpp
	BitmapOps.cpp
	ClassLibrary.cpp
	Console.cpp
	CoreModule.cpp
	CurrentDirContext.cpp
	deactivate_servant.cpp
	Event.cpp
	EventSync.cpp
	EventSyncTimeout.cpp
	ExecContext.cpp
	ExecDomain.cpp
	Executable.cpp
	FileAccessBuf.cpp
	FileAccessChar.cpp
	FileAccessCharProxy.cpp
	FileAccessDirect.cpp
	FileAccessDirectProxy.cpp
	FileDescriptors.cpp
	FileDescriptorsContext.cpp
	FileLockQueue.cpp
	FileLockRanges.cpp
	Heap.cpp
	HeapCustom.cpp
	HeapDirectory.cpp
	initterm.cpp
	InternalRequest.cpp
	LocaleImpl.cpp
	LockablePtr.cpp
	MemContext.cpp
	Module.cpp
	ObjectPool.cpp
	open_binary.cpp
	ProcessImpl.cpp
	ProtDomain.cpp
	RandomGen.cpp
	Runnable.cpp
	Scheduler.cpp
	Security.cpp
	Signals.cpp
	Singleton.cpp
	SkipList.cpp
	Startup.cpp
	StartupProt.cpp
	StartupSys.cpp
	SyncContext.cpp
	SyncDomain.cpp
	Synchronized.cpp
	SysDomain.cpp
	SysManager.cpp
	Thread.cpp
	ThreadBackground.cpp
	Timeout.cpp
	TimerAsyncCall.cpp
	TimerEvent.cpp
	TLS.cpp
	virtual_copy.cpp
	WaitableRef.cpp
	WaitList.cpp
)

add_subdirectory (ORB)
add_subdirectory (NameService)

find_package (boost_preprocessor CONFIG REQUIRED)
target_link_libraries (Core PRIVATE Boost::preprocessor)
