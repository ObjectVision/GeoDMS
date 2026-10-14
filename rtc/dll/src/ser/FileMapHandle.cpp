// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "ser/FileMapHandle.h"
#include "set/FileView.h"

#include "vt/MinMax.h"
#include "vt/Undefined.h"
#include "ser/FileCreationMode.h"
#include "set/MemPageTable.h"
#include "set/rangefuncs.h"
#include "utl/Environment.h"
#include "utl/FileSystem.h"
#include "utl/PlatformError.h"
#include "utl/MemGuard.h"
#include "utl/scoped_exit.h"
#include "utl/splitPath.h"
#include "utl/StrFormat.h"

#include <filesystem>

#if defined(WIN32)
#include <windows.h>

MG_DEBUGCODE(
	const UInt64 sd_MaxFileSize = 0x4000000000; // 40 x 4Gb = 160Gb
)

inline DWORD HiDWORD(UInt64 qWord) { return qWord >> 32; }
inline DWORD LoDWORD(UInt64 qWord) { return qWord; } // truncate
inline DWORD HiDWORD(UInt32 dWord) { return 0; }
inline DWORD LoDWORD(UInt32 dWord) { return dWord; }

static UInt32 GetAllocationGrannularityImpl()
{
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return info.dwAllocationGranularity;
}

SizeT GetAllocationGrannularity()
{
	static UInt32 allocGrannularity = GetAllocationGrannularityImpl();
	return allocGrannularity;
}

SizeT GetAllocationMinorMask()
{
	static UInt64 allocMask = GetAllocationGrannularity() - 1;
	return allocMask;
}

SizeT GetAllocationMajorMask()
{
	static UInt64 allocMask = ~GetAllocationMinorMask();
	return allocMask;
}

UInt8 GetLog2AllocationGrannularityImpl()
{
	auto x = GetAllocationGrannularity();
	MG_CHECK2(std::popcount(x) == 1, "System Allocation Grannularity is unexpectedly not a power of 2");

	auto y = sizeof(UInt32) * 8 - std::countl_zero(x) -1;
	MG_CHECK2(x == (SizeT(1) << y), "System Allocation Grannularity is unexpectedly not a power of 2");
	return y;
}

UInt8 GetLog2AllocationGrannularity()
{
	static auto result = GetLog2AllocationGrannularityImpl();
	return result;
}

static UInt32 GetMemPageSizeImpl()
{
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return info.dwPageSize;
}

UInt32 GetMemPageSize()
{
	static UInt32 pageSize = GetMemPageSizeImpl();
	MG_CHECK(pageSize == 0x1000); // .mmd file format depends on this assumption
	return pageSize;
}

UInt8 GetLog2MemPageSizeImpl()
{
	auto x = GetMemPageSize();
	MG_CHECK2(std::popcount(x) == 1, "System page size is unexpectedly not a power of 2");

	auto y = sizeof(UInt32) * 8 - std::countl_zero(x) - 1;
	MG_CHECK2(x == (UInt32(1) << y), "System page size is unexpectedly not a power of 2");
	return y;
}

UInt8 GetLog2MemPageSize()
{
	static auto result = GetLog2MemPageSizeImpl();
	return result;
}

//  -----------------------------------------------------------------------

DWORD GetCreationDisposition(FileCreationMode fcm)
{
	switch (fcm) {
		case FCM_CreateNew:      return CREATE_NEW;
		case FCM_CreateAlways:   return CREATE_ALWAYS;
		case FCM_OpenRwGrowable: return OPEN_ALWAYS;
		case FCM_OpenRwFixed:
		case FCM_OpenReadOnly:   return OPEN_EXISTING;
	}
	return 0;
}

//  -----------------------------------------------------------------------

HANDLE CreateFileHandleForRwView(WeakStr fileName, FileCreationMode fcm, bool isTmp, bool doRetry, bool deleteOnClose)
{
	assert(IsWritable(fcm));
	GetWritePermission(fileName);
	HANDLE fileHandle;
	UInt32 retryCounter = 0;

	auto dosFileName = ConvertDmsFileName(fileName);
	auto dosFileNameW = Utf8_2_wchar(dosFileName);

	do {
		fileHandle= CreateFileW(
				dosFileNameW.get()  // file name
			,	GENERIC_READ | GENERIC_WRITE          // access mode
			,	FILE_SHARE_DELETE // FILE_SHARE_READ                  // share mode
			,	NULL                                  // SD
			,	GetCreationDisposition(fcm)           // how to create
			,	FILE_ATTRIBUTE_NOT_CONTENT_INDEXED    // file attributes
			|	FILE_FLAG_RANDOM_ACCESS
			|	(isTmp         ? FILE_ATTRIBUTE_TEMPORARY  : 0)
			|	(deleteOnClose ? FILE_FLAG_DELETE_ON_CLOSE : 0)
			,	NULL                                  // handle to template file
			);
		assert(fileHandle);
	}	while ((fileHandle == INVALID_HANDLE_VALUE) && ManageSystemError(retryCounter, "CreateFileHandleForRwView({})", fileName.c_str(), true, doRetry));
	return fileHandle;
}


//  -----------------------------------------------------------------------

WinHandle::~WinHandle()
{
	if (m_Hnd == nullptr)
		return;
	CloseHandle(m_Hnd);
}

//  -----------------------------------------------------------------------

FileHandle::~FileHandle()
{
//	assert(!IsOpen());
	// REMOVE IF ASSERTION IS PROVEN
	if (IsOpen())
		CloseFile(); 
}

void FileHandle::OpenRw(WeakStr fileName, dms::filesize_t requiredNrBytes, dms_rw_mode rwMode, bool isTmp, bool doRetry, bool deleteOnClose)
{
	assert(!IsOpen());
	assert(rwMode != dms_rw_mode::unspecified);
	assert(rwMode >= dms_rw_mode::read_write);
	m_IsTmp     = isTmp;
	bool readData  = (rwMode < dms_rw_mode::write_only_mustzero);

	FileCreationMode fcm = readData ? FCM_OpenRwGrowable : FCM_CreateAlways;
	m_hFile = WinHandle(
		CreateFileHandleForRwView(fileName, fcm, isTmp, doRetry, deleteOnClose)
	); // returns a valid handle or throws a system error
	m_FileName = fileName;

	assert(IsOpen());
	assert(m_hFile != INVALID_HANDLE_VALUE);
	MG_DEBUG_DATA_CODE(m_FCM = fcm; )

	if (IsDefined(requiredNrBytes))
		SetFileSize(requiredNrBytes);
	else
		if (readData)
			ReadFileSize(fileName.c_str());
		else
			m_FileSize = 0;
	assert(m_FileSize != UNDEFINED_FILE_SIZE);
	dbg_assert(m_FileSize <= sd_MaxFileSize);
	// returns with valid m_hFileMap or throws a system error
}

void FileHandle::SetFileSize(dms::filesize_t requiredNrBytes)
{ 
	m_FileSize = requiredNrBytes;
	LARGE_INTEGER fs; fs.QuadPart = m_FileSize;
	SetFilePointerEx(m_hFile, fs, nullptr, FILE_BEGIN);
	fs.QuadPart = 0;
	SetFilePointerEx(m_hFile, fs, nullptr, FILE_BEGIN);
}

void FileHandle::OpenForRead(WeakStr fileName, bool throwOnError, bool doRetry, bool mayBeEmpty, bool shareDelete)
{
	assert(!IsOpen());

	HANDLE fileHandle;
	UInt32 retryCounter = 0;

	auto dosFileName = ConvertDmsFileName(fileName);
	auto dosFileNameW = Utf8_2_wchar(dosFileName);

	do {
		fileHandle = CreateFileW(
				dosFileNameW.get()
			,	GENERIC_READ                            // access mode
			,	FILE_SHARE_READ | (shareDelete ? FILE_SHARE_DELETE : 0) // share mode
			,	NULL                                    // SD
			,	mayBeEmpty ?  OPEN_ALWAYS : OPEN_EXISTING // how to create
			,	FILE_ATTRIBUTE_NORMAL
	//			FILE_ATTRIBUTE_NOT_CONTENT_INDEXED      // file attributes
	//		|	FILE_FLAG_RANDOM_ACCESS
	//		|	(deleteOnClose ? FILE_FLAG_DELETE_ON_CLOSE : 0)
	//		|	(isTmp         ? FILE_ATTRIBUTE_TEMPORARY  : 0)
	//		|	FILE_FLAG_RANDOM_ACCESS
			,	NULL                                    // handle to template file
		);
	}	while ((fileHandle == INVALID_HANDLE_VALUE) && ManageSystemError(retryCounter, "FileMapHandle({}).OpenForRead", fileName.c_str(), throwOnError, doRetry));

	if (fileHandle == INVALID_HANDLE_VALUE)
	{
		assert(!throwOnError);
		fileHandle = NULL;
	}

	m_hFile = fileHandle;
	m_FileName = fileName;
	assert(m_hFile != INVALID_HANDLE_VALUE);

	assert(IsOpen() || !throwOnError);
	MG_DEBUG_DATA_CODE( m_FCM = FCM_OpenReadOnly; )

	if (IsOpen())
		ReadFileSize(fileName.c_str());
	else
		m_FileSize = UNDEFINED_FILE_SIZE;
	
	dbg_assert(m_FileSize <= sd_MaxFileSize || m_FileSize == UNDEFINED_FILE_SIZE);
}

void FileHandle::CloseFile()
{
	assert(m_hFile);
	assert(m_hFile != INVALID_HANDLE_VALUE);
	m_hFile = WinHandle();
	// m_FileSize = UNDEFINED_FILE_SIZE;
}

void FileHandle::ReadFileSize(CharPtr handleName)
{
	LARGE_INTEGER fileSize;
	if (!GetFileSizeEx(m_hFile, &fileSize))
		throwSystemError(GetLastError(), "GetFileSize({})", handleName);
	m_FileSize = fileSize.QuadPart;
	dbg_assert(m_FileSize <= sd_MaxFileSize);
}

//  -----------------------------------------------------------------------

MappedFileHandle::MappedFileHandle()
{}

MappedFileHandle::~MappedFileHandle()
{}


void MappedFileHandle::MapFile(bool alsoWrite)
{
	m_hFileMapping = WinHandle();
	if (!m_hFile)
		throwErrorF("CreateFileMapping", "{}('{}') failed"
			, alsoWrite ? "CreateFile" : "OpenFile", m_FileName);

	if (m_FileSize == UNDEFINED_FILE_SIZE)
		throwErrorF("CreateFileMapping", "{}('{}') failed because MappingSize is undefined"
			, alsoWrite ? "CreateFile" : "OpenFile", m_FileName);

	WinHandle fileMapping =
		CreateFileMapping(
			m_hFile,                           // Current file handle. 
			nullptr,                           // Default security. 
			alsoWrite ? PAGE_READWRITE : PAGE_READONLY,
			HiDWORD(m_FileSize), LoDWORD(m_FileSize),
			nullptr                            // Name of mapping object. 
		);
	if (fileMapping == nullptr)
		throwLastSystemError("FileMapHandle('{}').CreateFileMapping({})", m_FileName.c_str(), (UInt64)m_FileSize);

	m_hFileMapping = std::move(fileMapping);
}

void MappedFileHandle::OpenRw(WeakStr fileName, dms::filesize_t requiredNrBytes, dms_rw_mode rwMode, bool isTmp)
{
	FileHandle::OpenRw(fileName, requiredNrBytes, rwMode, isTmp);
	if (m_FileSize == 0)
	{
		m_hFileMapping = WinHandle();
		return;
	}
	MapFile(true);
}

void MappedFileHandle::TruncateAndClose(dms::filesize_t newSize)
{
	m_MemPageAllocTable.reset();
	m_hFileMapping = WinHandle(); // SetEndOfFile refuses a file that a mapping object refers to

	LARGE_INTEGER fs; fs.QuadPart = newSize;
	bool ok = SetFilePointerEx(m_hFile, fs, nullptr, FILE_BEGIN) && SetEndOfFile(m_hFile);
	DWORD lastErr = ok ? 0 : GetLastError();
	if (ok)
		m_FileSize = m_AllocatedSize = newSize;
	CloseForReopen(); // also when the cut failed: the file is complete as it was, and the next view reopens it
	if (!ok)
		throwSystemError(lastErr, "FileMapHandle('{}').TruncateAndClose({})", m_FileName.c_str(), (UInt64)newSize);
}

void ReplaceFileBy(WeakStr fileName, WeakStr replacementFileName)
{
	auto dstW = Utf8_2_wchar(ConvertDmsFileName(fileName).c_str());
	auto srcW = Utf8_2_wchar(ConvertDmsFileName(replacementFileName).c_str());
	UInt32 retryCounter = 0;
	while (!MoveFileExW(srcW.get(), dstW.get(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		ManageSystemError(retryCounter, "ReplaceFileBy({})", fileName.c_str(), true, true); // retries a sharing violation, throws otherwise
}

// #1298: one attempt; returns 0 or the error, which the caller passes on to ManageSystemError
static DWORD TryMoveFile(WeakStr fileName, WeakStr newFileName)
{
	auto srcW = Utf8_2_wchar(ConvertDmsFileName(fileName).c_str());
	auto dstW = Utf8_2_wchar(ConvertDmsFileName(newFileName).c_str());
	if (MoveFileExW(srcW.get(), dstW.get(), MOVEFILE_WRITE_THROUGH))
		return 0;
	return GetLastError();
}

static void SetLastSystemError(DWORD err) { SetLastError(err); }

static DWORD GetProcessIdForStoreFiles() { return GetCurrentProcessId(); }

void MappedFileHandle::OpenForRead(WeakStr fileName, bool throwOnError, bool doRetry, bool shareDelete)
{
	FileHandle::OpenForRead(fileName, throwOnError, doRetry, false, shareDelete);

	// #1246: a zero-length file has nothing to map, and CreateFileMapping refuses a zero-length
	// mapping with ERROR_FILE_INVALID (1006), which made every MMD attribute of an empty domain
	// unreadable. OpenRw above already leaves m_hFileMapping empty for that case; every view of
	// such a file has capacity 0 (ConstFileViewHandle clamps to the file size) and MapView maps
	// nothing, so no reader ever touches the absent mapping.
	if (IsOpen() && m_FileSize == 0)
	{
		m_hFileMapping = WinHandle();
		return;
	}
	MapFile(false);
}

//  -----------------------------------------------------------------------

ViewData::ViewData(MappedFileHandle* mappedFile, DWORD desiredAccess, dms::filesize_t viewOffset, dms::filesize_t viewCapacity)
{
	auto pageBase = viewOffset & GetAllocationMajorMask();
	assert((pageBase & GetAllocationMinorMask()) == 0);
	auto pageOffset = viewOffset & GetAllocationMinorMask();

	while (true) {
		m_Ptr =
			MapViewOfFile(mappedFile->m_hFileMapping, // Handle to mapping object. 
				desiredAccess,
				HiDWORD(pageBase),
				LoDWORD(pageBase),
				viewCapacity + pageOffset
			);
		if (m_Ptr)
		{
			m_Ptr = reinterpret_cast<BYTE*>(m_Ptr) + pageOffset;
			break;
		}


		DWORD lastErr = GetLastError();
		if (lastErr != ERROR_NOT_ENOUGH_MEMORY || !DMS_CoalesceHeap(viewCapacity))
			throwSystemError(lastErr, "FileMapHandle('{}').MapViewOfFile({})", mappedFile->GetFileName().c_str(), (UInt64)viewCapacity);
	};
}

ViewData::~ViewData()
{
	if (has_ptr())
	{
		auto pageBase = reinterpret_cast<UInt64>(get_ptr()) & GetAllocationMajorMask();

		UnmapViewOfFile(reinterpret_cast<void*>(pageBase));
	}
}

//  -----------------------------------------------------------------------
// #1298: the lock file of a store
//  -----------------------------------------------------------------------

StoreLock::StoreLock(WeakStr storeFolder, bool forWrite)
	: m_StoreFolder(storeFolder)
	, m_LockFileName(DelimitedConcat(storeFolder.c_str(), "0Lock"))
	, m_ForWrite(forWrite)
{
	if (forWrite)
		GetWritePermission(m_LockFileName); // makes the folder of a new store
	auto nameW = Utf8_2_wchar(ConvertDmsFileName(m_LockFileName).c_str());
	HANDLE h = CreateFileW(nameW.get()
		, forWrite ? GENERIC_READ | GENERIC_WRITE : GENERIC_READ
		, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE
		, nullptr
		, forWrite ? OPEN_ALWAYS : OPEN_EXISTING
		, FILE_ATTRIBUTE_NORMAL
		, nullptr
	);
	if (h == INVALID_HANDLE_VALUE)
	{
		if (forWrite)
			throwLastSystemError("StoreLock({})", m_LockFileName.c_str());
		return; // a store written before 20.24 has no lock file, and a reader then reads without lock
	}
	m_hFile = WinHandle(h);
}

bool StoreLock::OsLock(UInt32 byteNr, bool exclusive, bool failImmediately)
{
	if (!m_hFile)
		return true;
	OVERLAPPED ov = {};
	ov.Offset = byteNr;
	DWORD flags = (exclusive ? LOCKFILE_EXCLUSIVE_LOCK : 0) | (failImmediately ? LOCKFILE_FAIL_IMMEDIATELY : 0);
	if (LockFileEx(m_hFile, flags, 0, 1, 0, &ov))
		return true;
	DWORD err = GetLastError();
	if (failImmediately && err == ERROR_LOCK_VIOLATION)
		return false;
	throwSystemError(err, "LockFileEx({})", m_LockFileName.c_str());
}

void StoreLock::OsUnlock(UInt32 byteNr)
{
	if (!m_hFile)
		return;
	OVERLAPPED ov = {};
	ov.Offset = byteNr;
	UnlockFileEx(m_hFile, 0, 1, 0, &ov);
}



#else //defined(WIN32)

// =====================================================================
// Linux implementation using mmap / open / ftruncate
// =====================================================================

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio> // rename

MG_DEBUGCODE(
	const UInt64 sd_MaxFileSize = 0x4000000000; // 160Gb
)

SizeT GetAllocationGrannularity()
{
	static SizeT grannularity = sysconf(_SC_PAGESIZE);
	return grannularity;
}

SizeT GetAllocationMinorMask()
{
	static SizeT mask = GetAllocationGrannularity() - 1;
	return mask;
}

SizeT GetAllocationMajorMask()
{
	static SizeT mask = ~GetAllocationMinorMask();
	return mask;
}

UInt8 GetLog2AllocationGrannularityImpl()
{
	auto x = GetAllocationGrannularity();
	MG_CHECK2(std::popcount(static_cast<unsigned>(x)) == 1, "System Allocation Grannularity is unexpectedly not a power of 2");
	auto y = sizeof(SizeT) * 8 - std::countl_zero(x) - 1;
	return y;
}

UInt8 GetLog2AllocationGrannularity()
{
	static auto result = GetLog2AllocationGrannularityImpl();
	return result;
}

UInt32 GetMemPageSize()
{
	static UInt32 pageSize = sysconf(_SC_PAGESIZE);
	return pageSize;
}

UInt8 GetLog2MemPageSizeImpl()
{
	auto x = GetMemPageSize();
	MG_CHECK2(std::popcount(x) == 1, "System page size is unexpectedly not a power of 2");
	auto y = sizeof(UInt32) * 8 - std::countl_zero(x) - 1;
	return y;
}

UInt8 GetLog2MemPageSize()
{
	static auto result = GetLog2MemPageSizeImpl();
	return result;
}

// -----------------------------------------------------------------------
// WinHandle: wraps a file descriptor on Linux
// -----------------------------------------------------------------------

WinHandle::~WinHandle()
{
	auto fd = reinterpret_cast<intptr_t>(m_Hnd);
	if (fd > 0)
		::close(static_cast<int>(fd));
}

static HANDLE FdToHandle(int fd) { return reinterpret_cast<HANDLE>(static_cast<intptr_t>(fd)); }
static int HandleToFd(HANDLE h) { return static_cast<int>(reinterpret_cast<intptr_t>(h)); }

// -----------------------------------------------------------------------
// FileHandle
// -----------------------------------------------------------------------

FileHandle::~FileHandle()
{
	if (IsOpen())
		CloseFile();
}

void FileHandle::OpenRw(WeakStr fileName, dms::filesize_t requiredNrBytes, dms_rw_mode rwMode, bool isTmp, bool doRetry, bool deleteOnClose)
{
	assert(!IsOpen());
	assert(rwMode != dms_rw_mode::unspecified);
	assert(rwMode >= dms_rw_mode::read_write);
	m_IsTmp = isTmp;
	bool readData = (rwMode < dms_rw_mode::write_only_mustzero);

	GetWritePermission(fileName);

	int flags = O_RDWR | O_CREAT;
	if (!readData)
		flags |= O_TRUNC;

	auto path = ConvertDmsFileName(fileName);
	int fd = -1;
	UInt32 retryCounter = 0;
	do {
		fd = ::open(path.c_str(), flags, 0644);
	} while (fd < 0 && ManageSystemError(retryCounter, "FileHandle::OpenRw({})", fileName.c_str(), true, doRetry));

	m_hFile = WinHandle(FdToHandle(fd));
	m_FileName = fileName;
	assert(IsOpen());
	MG_DEBUG_DATA_CODE(m_FCM = readData ? FCM_OpenRwGrowable : FCM_CreateAlways;)

	if (IsDefined(requiredNrBytes))
		SetFileSize(requiredNrBytes);
	else if (readData)
		ReadFileSize(fileName.c_str());
	else
		m_FileSize = 0;

	assert(m_FileSize != UNDEFINED_FILE_SIZE);
}

void FileHandle::SetFileSize(dms::filesize_t requiredNrBytes)
{
	m_FileSize = requiredNrBytes;
	int fd = HandleToFd(m_hFile);
	if (ftruncate(fd, requiredNrBytes) != 0)
		throwLastSystemError("ftruncate({}, {})", m_FileName.c_str(), (unsigned long long)requiredNrBytes);
}

void FileHandle::OpenForRead(WeakStr fileName, bool throwOnError, bool doRetry, bool mayBeEmpty, bool shareDelete)
{
	assert(!IsOpen());
	// shareDelete: an open file can always be renamed or unlinked here

	auto path = ConvertDmsFileName(fileName);
	int fd = -1;
	UInt32 retryCounter = 0;
	do {
		fd = ::open(path.c_str(), mayBeEmpty ? (O_RDONLY | O_CREAT) : O_RDONLY, 0644);
	} while (fd < 0 && ManageSystemError(retryCounter, "FileHandle::OpenForRead({})", fileName.c_str(), throwOnError, doRetry));

	if (fd < 0)
	{
		assert(!throwOnError);
		m_hFile = WinHandle(nullptr);
		m_FileName = fileName;
		m_FileSize = UNDEFINED_FILE_SIZE;
		return;
	}

	m_hFile = WinHandle(FdToHandle(fd));
	m_FileName = fileName;
	MG_DEBUG_DATA_CODE(m_FCM = FCM_OpenReadOnly;)

	ReadFileSize(fileName.c_str());
}

void FileHandle::CloseFile()
{
	assert(m_hFile);
	m_hFile = WinHandle();
}

void FileHandle::ReadFileSize(CharPtr handleName)
{
	int fd = HandleToFd(m_hFile);
	struct stat st;
	if (fstat(fd, &st) != 0)
		throwLastSystemError("fstat({})", handleName);
	m_FileSize = st.st_size;
	dbg_assert(m_FileSize <= sd_MaxFileSize);
}

void FileHandle::DropFile(WeakStr fileName)
{
	CloseFile();
	auto path = ConvertDmsFileName(fileName);
	unlink(path.c_str());
}

// -----------------------------------------------------------------------
// MappedFileHandle
// -----------------------------------------------------------------------

MappedFileHandle::MappedFileHandle() = default;

MappedFileHandle::~MappedFileHandle() = default;

void MappedFileHandle::OpenRw(WeakStr fileName, dms::filesize_t requiredNrBytes, dms_rw_mode rwMode, bool isTmp)
{
	FileHandle::OpenRw(fileName, requiredNrBytes, rwMode, isTmp, true, false);
	// On Linux, store a duplicate fd in m_hFileMapping so ViewData can access it
	int fd = HandleToFd(m_hFile);
	m_hFileMapping = WinHandle(FdToHandle(dup(fd)));
}

void MappedFileHandle::OpenForRead(WeakStr fileName, bool throwOnError, bool doRetry, bool shareDelete)
{
	FileHandle::OpenForRead(fileName, throwOnError, doRetry, false, shareDelete);
	if (IsOpen())
	{
		int fd = HandleToFd(m_hFile);
		m_hFileMapping = WinHandle(FdToHandle(dup(fd)));
	}
}

void MappedFileHandle::MapFile(bool alsoWrite)
{
	// On Linux, mapping is done per-view via ViewData; this is a no-op
}

void MappedFileHandle::TruncateAndClose(dms::filesize_t newSize)
{
	m_MemPageAllocTable.reset();
	m_hFileMapping = WinHandle();
	try {
		SetFileSize(newSize); // ftruncate
	}
	catch (...)
	{
		CloseForReopen(); // the file is complete as it was, and the next view reopens it
		throw;
	}
	m_AllocatedSize = newSize;
	CloseForReopen();
}

void ReplaceFileBy(WeakStr fileName, WeakStr replacementFileName)
{
	auto dst = ConvertDmsFileName(fileName);
	auto src = ConvertDmsFileName(replacementFileName);
	UInt32 retryCounter = 0;
	while (::rename(src.c_str(), dst.c_str()) != 0)
		ManageSystemError(retryCounter, "ReplaceFileBy({})", fileName.c_str(), true, true); // retries EACCES and EBUSY, throws otherwise
}

// #1298: one attempt; returns 0 or the error, which the caller passes on to ManageSystemError
static DWORD TryMoveFile(WeakStr fileName, WeakStr newFileName)
{
	auto src = ConvertDmsFileName(fileName);
	auto dst = ConvertDmsFileName(newFileName);
	if (::rename(src.c_str(), dst.c_str()) == 0)
		return 0;
	return errno;
}

static void SetLastSystemError(DWORD err) { errno = int(err); }

static DWORD GetProcessIdForStoreFiles() { return DWORD(getpid()); }

// -----------------------------------------------------------------------
// #1298: the lock file of a store
// -----------------------------------------------------------------------

StoreLock::StoreLock(WeakStr storeFolder, bool forWrite)
	: m_StoreFolder(storeFolder)
	, m_LockFileName(DelimitedConcat(storeFolder.c_str(), "0Lock"))
	, m_ForWrite(forWrite)
{
	if (forWrite)
		GetWritePermission(m_LockFileName); // makes the folder of a new store
	auto path = ConvertDmsFileName(m_LockFileName);
	int fd = ::open(path.c_str(), forWrite ? (O_RDWR | O_CREAT) : O_RDONLY, 0644);
	if (fd < 0)
	{
		if (forWrite)
			throwLastSystemError("StoreLock({})", m_LockFileName.c_str());
		return; // a store written before 20.24 has no lock file, and a reader then reads without lock
	}
	m_hFile = WinHandle(FdToHandle(fd));
}

// An OFD lock belongs to the open file description, as a lock of LockFileEx belongs to its handle; a classic
// POSIX lock belongs to the process and would go with the close of any other descriptor of the same file.
bool StoreLock::OsLock(UInt32 byteNr, bool exclusive, bool failImmediately)
{
	if (!m_hFile)
		return true;
	struct flock fl = {};
	fl.l_type   = exclusive ? F_WRLCK : F_RDLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start  = byteNr;
	fl.l_len    = 1;
#if defined(F_OFD_SETLKW)
	int cmd = failImmediately ? F_OFD_SETLK : F_OFD_SETLKW;
#else
	int cmd = failImmediately ? F_SETLK : F_SETLKW;
#endif
	while (fcntl(HandleToFd(m_hFile), cmd, &fl) != 0)
	{
		int err = errno;
		if (err == EINTR)
			continue;
		if (failImmediately && (err == EAGAIN || err == EACCES))
			return false;
		throwSystemError(err, "fcntl(lock {})", m_LockFileName.c_str());
	}
	return true;
}

void StoreLock::OsUnlock(UInt32 byteNr)
{
	if (!m_hFile)
		return;
	struct flock fl = {};
	fl.l_type   = F_UNLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start  = byteNr;
	fl.l_len    = 1;
#if defined(F_OFD_SETLK)
	fcntl(HandleToFd(m_hFile), F_OFD_SETLK, &fl);
#else
	fcntl(HandleToFd(m_hFile), F_SETLK, &fl);
#endif
}

// -----------------------------------------------------------------------
// ViewData: memory-mapped view using mmap
// -----------------------------------------------------------------------

// Windows compatibility constants for ViewData::desiredAccess
constexpr DWORD FILE_MAP_READ  = 4;
constexpr DWORD FILE_MAP_WRITE = 2;

// #1297: mmap takes an offset that is a multiple of the page size, and the chunk of a sequence pool starts at any
// byte. As with MapViewOfFile, the view is mapped from the page that holds its first byte, and points past the
// bytes before it; the destructor unmaps from that page.
ViewData::ViewData(MappedFileHandle* mappedFile, DWORD desiredAccess, dms::filesize_t viewOffset, dms::filesize_t viewCapacity)
{
	bool wantWrite = (desiredAccess & FILE_MAP_WRITE) != 0;

	// Access fd via the public m_hFileMapping member (stores fd on Linux)
	int fd = HandleToFd(mappedFile->m_hFileMapping);
	int ownFd = -1; // opened here; a mapping holds its own reference to the file, so this one is closed after mmap
	if (fd <= 0)
	{
		// Fallback: re-open the file to get a fd
		auto path = ConvertDmsFileName(mappedFile->GetFileName());
		fd = ::open(path.c_str(), wantWrite ? O_RDWR : O_RDONLY);
		if (fd < 0)
			throwLastSystemError("ViewData open({})", mappedFile->GetFileName().c_str());
		ownFd = fd;
	}
	int prot = PROT_READ;
	if (wantWrite)
		prot |= PROT_WRITE;

	auto pageBase   = viewOffset & GetAllocationMajorMask();
	auto pageOffset = viewOffset & GetAllocationMinorMask();

	void* addr = mmap(nullptr, viewCapacity + pageOffset, prot, MAP_SHARED, fd, pageBase);
	DWORD lastErr = (addr == MAP_FAILED) ? errno : 0;
	if (ownFd >= 0)
		::close(ownFd);
	if (addr == MAP_FAILED)
		throwSystemError(lastErr, "mmap({}, offset={}, size={})",
			mappedFile->GetFileName().c_str(),
			(unsigned long long)viewOffset, (unsigned long long)viewCapacity);

	m_Ptr = reinterpret_cast<char*>(addr) + pageOffset;
	m_LinuxMmapSize = viewCapacity + pageOffset;
}

ViewData::~ViewData()
{
	if (m_Ptr && m_LinuxMmapSize)
	{
		auto pageBase = reinterpret_cast<SizeT>(get_ptr()) & GetAllocationMajorMask();
		munmap(reinterpret_cast<void*>(pageBase), m_LinuxMmapSize);
	}
}

#endif //defined(WIN32)

// =====================================================================
// Chunk allocation, the chunk table of a .seq file, and the views: the same on both platforms (#1297);
// only FileHandle, MapFile and ViewData differ.
// =====================================================================

#if defined(MG_DEBUG)
void CheckMemPage(const mempage_table* memPageAllocTable, FreeChunk newChunk)
{
	assert(memPageAllocTable->m_FreeListByLexi.size() == memPageAllocTable->m_FreeListBySize.size());

	for (const auto& freeChunk : memPageAllocTable->m_FreeListByLexi)
	{
		assert(freeChunk.first < freeChunk.second);
		assert((newChunk.first >= freeChunk.second) || (newChunk.second <= freeChunk.first));
		assert(memPageAllocTable->m_FreeListBySize.find(freeChunk) != memPageAllocTable->m_FreeListBySize.end());
	}
	for (const auto& freeChunk : memPageAllocTable->m_FreeListBySize)
	{
		assert(freeChunk.first < freeChunk.second);
		assert((newChunk.first >= freeChunk.second) || (newChunk.second <= freeChunk.first));
	}
	auto currFreeByLexi = memPageAllocTable->m_FreeListByLexi.begin();
	auto lastFreeByLexi = memPageAllocTable->m_FreeListByLexi.end();
	if (!(currFreeByLexi == lastFreeByLexi))
	{
		while (true)
		{
			auto nextFreeByLexi = currFreeByLexi;
			++nextFreeByLexi;
			assert(currFreeByLexi->second < nextFreeByLexi->first);
			if (nextFreeByLexi == lastFreeByLexi)
				break;
			currFreeByLexi = nextFreeByLexi;
		}
	}
}
#endif //defined(MG_DEBUG)

FileChunkSpec MappedFileHandle::AllocFile(FileChunkSpec& viewSpec, dms::filesize_t newCapacity)
{
	auto lock = std::scoped_lock(m_ResizeMutex);

	assert(newCapacity > viewSpec.capacity); // precondition
	assert(!m_MemPageAllocTable.get());
	m_AllocatedSize = 0;
	auto result = allocAtEnd(viewSpec.size, newCapacity);
	return result;
}

FileChunkSpec MappedFileHandle::allocChunk(FileChunkSpec& viewSpec, dms::filesize_t newCapacity, tile_id t)
{
	assert(!m_ResizeMutex.try_lock_shared());

	assert(newCapacity > viewSpec.capacity); // precondition

	auto memPageAllocTable = m_MemPageAllocTable.get();
	if (!memPageAllocTable)
	{
		assert(viewSpec.offset == 0);
		assert(viewSpec.capacity <= m_AllocatedSize);
		if (newCapacity > m_AllocatedSize)
			allocAtEnd(0, newCapacity - m_AllocatedSize);
		MakeMax(viewSpec.capacity, newCapacity);
		return viewSpec;
	}
	assert(memPageAllocTable);

	auto newChunk = memPageAllocTable->ReallocChunk(FreeChunk(viewSpec.offset, viewSpec.offset + viewSpec.capacity), newCapacity, m_AllocatedSize);
	assert(newChunk.first >= memPageAllocTable->m_ViewSpec.capacity);
	assert(newChunk.first < newChunk.second);
	assert(newChunk.first + newCapacity == newChunk.second);

	if (newChunk.second > m_AllocatedSize)
		allocAtEnd(0, newChunk.second - m_AllocatedSize);

#if defined(MG_DEBUG)
	CheckMemPage(memPageAllocTable, newChunk);
#endif //defined(MG_DEBUG)

	auto result = FileChunkSpec(newChunk.first, viewSpec.size, newChunk.size());
	assert(t < memPageAllocTable->filed_size());
	(*memPageAllocTable)[t] = result;
	assert((*memPageAllocTable)[t].size <= (*memPageAllocTable)[t].capacity);

	return result;
}

FileChunkSpec MappedFileHandle::allocAtEnd(dms::filesize_t viewSize, dms::filesize_t newCapacity)
{
	assert(!m_ResizeMutex.try_lock_shared()); // caller must have locked this exclusively.

	auto newOffset = m_AllocatedSize;
	m_AllocatedSize += newCapacity;

	if (m_AllocatedSize > GetFileSize())
	{
		SetFileSize(m_AllocatedSize);
		assert(m_FileSize == m_AllocatedSize);
		MapFile(true);
	}
	return { newOffset, viewSize, newCapacity };
}

void CreateMemPageAllocTable(std::shared_ptr<MappedFileHandle> self, bool readOnly, tile_id tn)
{
	auto memPageFileView = std::make_unique<mempage_table>(self, readOnly ? tn : 0, 0);
	if (readOnly)
		memPageFileView->MapView(false);
	else
	{
		memPageFileView->ReserveAndMapElems(tn);
		memPageFileView->SetNrElemsWithoutUpdatingMemPageAllocTable(tn);
		fast_zero(memPageFileView->begin(), memPageFileView->end());
	}
	memPageFileView->m_MappedFile.reset();
	memPageFileView->InitFreeSetsFromChunkSpecs(tn, self->GetFileSize());
	self->m_MemPageAllocTable = std::move(memPageFileView);
}

FileViewHandle::FileViewHandle(std::shared_ptr<MappedFileHandle> mfh, dms::filesize_t viewOffset, dms::filesize_t viewSize, dms::filesize_t viewCapacity)
	: m_MappedFile(mfh)
{
	if (viewOffset == UNDEFINED_FILE_SIZE)
	{
		assert(mfh);
		auto lock = std::scoped_lock(mfh->m_ResizeMutex);

		m_ViewSpec = mfh->allocAtEnd(viewSize, viewCapacity);
	}
	else
		m_ViewSpec = { viewOffset, viewSize, viewCapacity };
	assert(m_ViewSpec.size <= m_ViewSpec.capacity);
}

ConstFileViewHandle::ConstFileViewHandle(std::shared_ptr<ConstMappedFileHandle> cmfh, dms::filesize_t viewOffset, dms::filesize_t viewSize, dms::filesize_t viewCapacity)
	:	m_MappedFile(cmfh)
{
	auto lock = std::scoped_lock(cmfh->m_ResizeMutex);

	if (viewOffset == UNDEFINED_FILE_SIZE)
		m_ViewSpec = cmfh->allocAtEnd(viewSize, viewCapacity);
	else
		m_ViewSpec = { viewOffset, viewSize, viewCapacity };

	assert(m_ViewSpec.size <= m_ViewSpec.capacity);

	MakeMin(m_ViewSpec.capacity, cmfh->GetFileSize() - m_ViewSpec.offset);

	if (m_ViewSpec.size == UNDEFINED_FILE_SIZE)
		m_ViewSpec.size = m_ViewSpec.capacity;

	MG_CHECK(m_ViewSpec.size <= m_ViewSpec.capacity);
}

void FileViewHandle::operator =(FileViewHandle&& rhs) noexcept
{
	m_MappedFile = std::move(rhs.m_MappedFile); assert(!rhs.m_MappedFile);
	std::swap(m_ViewSpec, rhs.m_ViewSpec);
	m_ViewData = std::move(rhs.m_ViewData);

	assert(m_ViewSpec.size <= m_ViewSpec.capacity);
}

void ConstFileViewHandle::operator =(ConstFileViewHandle&& rhs) noexcept
{
	m_MappedFile = std::move(rhs.m_MappedFile); assert(!rhs.m_MappedFile);
	std::swap(m_ViewSpec, rhs.m_ViewSpec);
	m_ViewData = std::move(rhs.m_ViewData);

	assert(m_ViewSpec.size <= m_ViewSpec.capacity);
}

void FileViewHandle::MapView(bool alsoWrite)
{
	MG_CHECK(m_MappedFile); // precondition

	m_ViewData = ViewData();

	if (!m_ViewSpec.capacity)
		return;

	m_MappedFile->ReopenIfClosed(alsoWrite); // #1280: the seal closed the file; the first view after it opens it again
	m_AlsoWrite = alsoWrite;

	m_ViewData = ViewData(m_MappedFile.get(), alsoWrite ? FILE_MAP_WRITE : FILE_MAP_READ, m_ViewSpec.offset, m_ViewSpec.capacity);
}

void ConstFileViewHandle::MapView()
{
	MG_CHECK(m_MappedFile); // precondition

	assert(!m_ViewData);

	m_ViewData = ViewData();
	if (!m_ViewSpec.capacity)
		return;

	m_ViewData = ViewData(m_MappedFile.get(), FILE_MAP_READ, m_ViewSpec.offset, m_ViewSpec.capacity);
}

void FileViewHandle::allocAndMapChunk(dms::filesize_t capacity, tile_id t)
{
	assert(m_MappedFile);
	assert(capacity > m_ViewSpec.capacity); // precondition
	assert(!m_MappedFile->m_ResizeMutex.try_lock_shared());

	m_ViewSpec = m_MappedFile->allocChunk(m_ViewSpec, capacity, t);

	MapView(true);
}

void FileViewHandle::AllocAndMapFile(dms::filesize_t capacity)
{
	assert(m_MappedFile);
	assert(capacity > m_ViewSpec.capacity); // precondition

	m_ViewSpec = m_MappedFile->AllocFile(m_ViewSpec, capacity);

	MapView(true);
}


void RemoveFromFreeList(std::set<FreeChunk, LexiLess>& freeList, FreeChunk chunk)
{
	if (chunk.first == chunk.second)
		return;
	MG_CHECK(chunk.first < chunk.second);

	auto it = freeList.upper_bound(chunk);
	if (it == freeList.end() || it->first > chunk.first)
	{
		MG_CHECK(it != freeList.begin()); // we can always find chunk in freeList
		--it;
	}
	MG_CHECK(it != freeList.end());

	FreeChunk orgFreeChunk = *it;

	MG_CHECK(orgFreeChunk.first <= chunk.first);
	MG_CHECK(orgFreeChunk.second >= chunk.second);
	it = freeList.erase(it);
	if (orgFreeChunk.second > chunk.second)
		it = freeList.insert(it, FreeChunk(chunk.second, orgFreeChunk.second));
	if (orgFreeChunk.first < chunk.first)
		it = freeList.insert(it, FreeChunk(orgFreeChunk.first, chunk.first));
}

void mempage_table::InitFreeSetsFromChunkSpecs(tile_id tn, dms::filesize_t fileSize)
{
	m_FreeListByLexi.clear();
	auto startingOffset = MinimalSeqFileSize(tn);
	assert(startingOffset <= fileSize);
	if (startingOffset < fileSize)
		m_FreeListByLexi.insert(FreeChunk(startingOffset, fileSize));

	for (auto it = begin(); it != end(); ++it)
	{
		auto& chunk = *it;
		RemoveFromFreeList(m_FreeListByLexi, FreeChunk(chunk.offset, chunk.offset + chunk.capacity));
	}

	// sort by size in additional set
	m_FreeListBySize.clear();
	for (auto& chunk : m_FreeListByLexi)
		m_FreeListBySize.insert(chunk);

	assert(m_FreeListByLexi.size() == m_FreeListBySize.size());
}

void mempage_table::FreeAllocatedChunk(FreeChunk currChunk)
{
	if (!currChunk.size())
		return;

	//search in lexi before and after it
	auto it = m_FreeListByLexi.lower_bound(currChunk);
	if (it != m_FreeListByLexi.begin())
	{
		// merge with previous chunk
		auto prevIt = it;
		auto prevChunk = *--prevIt;

		if (prevChunk.second == currChunk.first)
		{
			assert(m_FreeListByLexi.size() == m_FreeListBySize.size());
			m_FreeListBySize.erase(prevChunk);
			it = m_FreeListByLexi.erase(prevIt);
			assert(m_FreeListByLexi.size() == m_FreeListBySize.size());

			currChunk.first = prevChunk.first;
		}
	}
	if (it != m_FreeListByLexi.end() && it->first == currChunk.second)
	{
		// merge with next chunk
		auto nextChunk = *it;

		assert(m_FreeListByLexi.size() == m_FreeListBySize.size());
		it = m_FreeListByLexi.erase(it);
		m_FreeListBySize.erase(nextChunk);
		assert(m_FreeListByLexi.size() == m_FreeListBySize.size());

		currChunk.second = nextChunk.second;
	}

#if defined(MG_DEBUG)
	CheckMemPage(this, currChunk);
#endif //defined(MG_DEBUG)

	assert(currChunk.first < currChunk.second);
	m_FreeListByLexi.insert(it, currChunk);
	m_FreeListBySize.insert(currChunk);

	assert(m_FreeListByLexi.size() == m_FreeListBySize.size());
}

FreeChunk mempage_table::ReallocChunk(FreeChunk currChunk, dms::filesize_t newSize, dms::filesize_t fileSize)
{
#if defined(MG_DEBUG)
	CheckMemPage(this, currChunk);
#endif //defined(MG_DEBUG)

	assert(currChunk.size() < newSize);
	auto it = m_FreeListByLexi.lower_bound(currChunk);
	if (it != m_FreeListByLexi.end())
	{
		assert(it->first >= currChunk.second); // free chunks don't overlap with used chunks, as currChunk should be
		if (it->first == currChunk.second) // free chunk starts where currChunk ends
		{
			auto freeChunk = *it;
			auto newEnd = currChunk.first + newSize;
			if (newEnd <= freeChunk.second)
			{
				// grow into the subsequent free chunk
				it = m_FreeListByLexi.erase(it);
				m_FreeListBySize.erase(freeChunk);
				assert(m_FreeListByLexi.size() == m_FreeListBySize.size());

				if (newEnd < freeChunk.second)
				{
					freeChunk.first = newEnd;
					assert(freeChunk.first < freeChunk.second);
					it = m_FreeListByLexi.insert(it, freeChunk);
					m_FreeListBySize.insert(freeChunk);
				}
				// old chunk has not been freed
				currChunk.second = newEnd;
#if defined(MG_DEBUG)
				CheckMemPage(this, currChunk);
#endif //defined(MG_DEBUG)

				return currChunk;
			}
		}
	}
	auto searchChunk = FreeChunk(0, newSize);
	auto it2 = m_FreeListBySize.lower_bound(searchChunk);
	if (it2 != m_FreeListBySize.end())
	{
		// reallocate into the first matching free chunk
		assert(newSize <= it2->size()); // free chunks don't overlap with used chunks, as currChunk should be
		auto freeChunk = *it2;
		m_FreeListBySize.erase(it2);
		it = m_FreeListByLexi.find(freeChunk);
		assert(it != m_FreeListByLexi.end());
		it = m_FreeListByLexi.erase(it);
		auto newChunk = FreeChunk(freeChunk.first, freeChunk.first + newSize);

		if (newChunk.second < freeChunk.second)
		{
			freeChunk.first = newChunk.second;
			assert(freeChunk.first < freeChunk.second);
			it = m_FreeListByLexi.insert(it, freeChunk);
			m_FreeListBySize.insert(freeChunk);
		}

		FreeAllocatedChunk(currChunk);
		return newChunk;
	}

	// test if curChunk can be extended beyond the EOF
	if (currChunk.second == fileSize)
	{
		auto newChunk = FreeChunk(currChunk.first, currChunk.first + newSize);
#if defined(MG_DEBUG)
		CheckMemPage(this, newChunk);
#endif //defined(MG_DEBUG)

		return newChunk;
	}

	// or after removing the last free chunk
	if (m_FreeListByLexi.size())
	{
		auto lastChunkIt = --m_FreeListByLexi.end();
		auto lastChunk = *lastChunkIt;
		if (currChunk.second == lastChunk.first && lastChunk.second == fileSize)
		{
			assert(m_FreeListByLexi.size() == m_FreeListBySize.size());
			m_FreeListBySize.erase(lastChunk);
			m_FreeListByLexi.erase(lastChunkIt);
			assert(m_FreeListByLexi.size() == m_FreeListBySize.size());

			auto newChunk = FreeChunk(currChunk.first, currChunk.first + newSize);
			if (newChunk.second < lastChunk.first)
			{
				lastChunk.first = newChunk.second;
				m_FreeListByLexi.insert(m_FreeListByLexi.end(), lastChunk);
				m_FreeListBySize.insert(lastChunk);
				assert(m_FreeListByLexi.size() == m_FreeListBySize.size());
			}
#if defined(MG_DEBUG)
			CheckMemPage(this, newChunk);
#endif //defined(MG_DEBUG)

			return newChunk;
		}
	}

	FreeAllocatedChunk(currChunk);

	auto newChunk = FreeChunk(fileSize, fileSize + newSize);
#if defined(MG_DEBUG)
	CheckMemPage(this, newChunk);
#endif //defined(MG_DEBUG)

	return newChunk;
}

// =====================================================================
// #1280: the seal of a stored sequence pool (FileTileArray<V>::SealSequences)
// =====================================================================

void MappedFileHandle::CloseForReopen()
{
	m_MemPageAllocTable.reset(); // unmaps the chunk table; nothing writes chunks after the seal
	m_hFileMapping = WinHandle();
	if (IsOpen())
		CloseFile();
	m_ClosedForReopen.store(true, std::memory_order_release);
}

void MappedFileHandle::ReplaceByAndClose(WeakStr replacementFileName)
{
	CloseForReopen(); // a file that is open cannot be replaced; when the replacement fails, the next view reopens this one, which is complete
	ReplaceFileBy(m_FileName, replacementFileName);
}

void MappedFileHandle::ReopenIfClosed(bool alsoWrite)
{
	if (!m_ClosedForReopen.load(std::memory_order_acquire))
		return;

	auto lock = std::scoped_lock(m_ReopenMutex); // the tiles of one attribute share this file and map their views in parallel
	if (!m_ClosedForReopen.load(std::memory_order_relaxed))
		return;
	MG_CHECK2(!alsoWrite, "a sealed .seq file is opened again for reading only");
	OpenForRead(m_FileName, true, true, true); // shareDelete: a later write of the same array moves this file aside (#1298)
	m_ClosedForReopen.store(false, std::memory_order_release);
}

void WriteCompactSeqFile(WeakStr fileName, tile_id tn, const dms::filesize_t* tileBytes, const std::function<void(tile_id, char*)>& writeTile)
{
	dms::filesize_t headerBytes = (tn > 1) ? MinimalSeqFileSize(tn) : 0; // a domain of one tile has no chunk table
	dms::filesize_t totalBytes = headerBytes;
	for (tile_id t = 0; t != tn; ++t)
		totalBytes += tileBytes[t];

	MappedFileHandle file;
	file.OpenRw(fileName, totalBytes, dms_rw_mode::write_only_all, false);
	if (!totalBytes)
		return;

	if (headerBytes)
	{
		ViewData header(&file, FILE_MAP_WRITE, 0, headerBytes);
		auto chunkSpecs = reinterpret_cast<FileChunkSpec*>(header.get_ptr());
		dms::filesize_t offset = headerBytes;
		for (tile_id t = 0; t != tn; ++t)
		{
			chunkSpecs[t] = FileChunkSpec{ offset, tileBytes[t], tileBytes[t] };
			offset += tileBytes[t];
		}
	}
	dms::filesize_t offset = headerBytes;
	for (tile_id t = 0; t != tn; ++t)
	{
		if (tileBytes[t])
		{
			ViewData view(&file, FILE_MAP_WRITE, offset, tileBytes[t]);
			writeTile(t, reinterpret_cast<char*>(view.get_ptr()));
		}
		offset += tileBytes[t];
	}
}

// =====================================================================
// #1298: the files of an MMD store (see FileMapHandle.h)
// =====================================================================

void MappedFileHandle::RenameTo(WeakStr newFileName)
{
	ReplaceFileBy(newFileName, m_FileName); // a handle of this file shares delete, so the open file can be moved
	m_FileName = newFileName;
}

UInt64 NextStoreFileNr()
{
	static std::atomic<UInt64> s_LastNr = 0;
	return ++s_LastNr;
}

SharedStr MakeStoreTmpName(WeakStr fileName, char kind, UInt64 nr)
{
	return mySSPrintF("{}.~{}{}_{}", fileName.c_str(), kind, GetProcessIdForStoreFiles(), nr);
}

StoreLock::~StoreLock() = default; // the OS releases the locks of the handle when it closes

void StoreLock::BeginWriteSession()
{
	auto lock = std::scoped_lock(m_Mutex);
	if (m_HasWriteSession)
		return;
	MG_CHECK(m_ForWrite);
	if (!OsLock(1, true, true))
		throwErrorF("MMD", "storage {} is being written by another process, which holds {}. "
			"A store is written by one process at a time; try again when that process has finished or closed its configuration."
			, m_StoreFolder.c_str(), m_LockFileName.c_str());
	m_HasWriteSession = true;
}

void StoreLock::LockShared()
{
	std::unique_lock lock(m_Mutex);
	m_Released.wait(lock, [this] { return !m_Exclusive; });
	if (!m_NrShared)
		OsLock(0, false, false); // waits while another process replaces a pair
	++m_NrShared;
}

void StoreLock::UnlockShared()
{
	std::unique_lock lock(m_Mutex);
	assert(m_NrShared);
	if (--m_NrShared)
		return;
	OsUnlock(0);
	m_Released.notify_all();
}

void StoreLock::LockExclusive()
{
	std::unique_lock lock(m_Mutex);
	m_Released.wait(lock, [this] { return !m_Exclusive && !m_NrShared; });
	OsLock(0, true, false); // waits while another process opens a pair
	m_Exclusive = true;
}

void StoreLock::UnlockExclusive()
{
	std::unique_lock lock(m_Mutex);
	assert(m_Exclusive);
	OsUnlock(0);
	m_Exclusive = false;
	m_Released.notify_all();
}

UInt64 StoreLock::StartWrite(WeakStr finalName)
{
	auto nr = NextStoreFileNr();
	auto lock = std::scoped_lock(m_Mutex);
	m_LatestWrite[SharedStr(finalName)] = nr;
	return nr;
}

bool StoreLock::IsLatestWrite(WeakStr finalName, UInt64 writeNr)
{
	auto lock = std::scoped_lock(m_Mutex);
	auto i = m_LatestWrite.find(SharedStr(finalName));
	return i != m_LatestWrite.end() && i->second == writeNr;
}

namespace {

	// Moves fileName aside, when it exists, and returns the name it now has, or the error. A handle of this
	// process shares delete; a reader in another process does not, and keeps it in place until it closes.
	DWORD TryRetireStoreFile(WeakStr fileName, std::vector<SharedStr>& retired)
	{
		if (!IsFileOrDirAccessible(fileName))
			return 0;
		auto zombieName = MakeStoreTmpName(fileName, 'z', NextStoreFileNr());
		if (auto err = TryMoveFile(fileName, zombieName))
			return err;
		retired.emplace_back(std::move(zombieName));
		return 0;
	}

	// The data of a retired file stays where an open handle or view needs it; the name goes now, or, on a file
	// system without POSIX delete semantics, when the last handle closes.
	void DeleteRetiredStoreFiles(const std::vector<SharedStr>& retired)
	{
		for (const auto& zombieName : retired)
			KillFileOrDir(zombieName, false);
	}

} // anonymous namespace

SharedStr StoreFilePublisher::Begin(std::shared_ptr<StoreLock> storeLock, WeakStr finalName, bool hasSeqFile)
{
	assert(storeLock && storeLock->IsForWrite());
	assert(!IsActive());
	m_FinalName  = finalName;
	m_HasSeqFile = hasSeqFile;
	m_WriteNr    = storeLock->StartWrite(finalName);
	m_PartialName = MakeStoreTmpName(finalName, 'w', m_WriteNr);

	// The previous version goes before this one is written, so that a failed write leaves no array rather than
	// the old one under a dictionary that no longer describes it. The index file first: a reader that comes in
	// between finds no array, never half a pair. A reader in another process that has the old files open is
	// waited for outside the lock, as a sharing violation always was.
	std::vector<SharedStr> retired;
	UInt32 retryCounter = 0;
	while (true)
	{
		DWORD err = 0;
		{
			StoreLockExclusive pairLock(storeLock.get());
			err = TryRetireStoreFile(finalName, retired);
			if (!err && hasSeqFile)
				err = TryRetireStoreFile(SharedStr(finalName) + ".seq", retired);
		}
		if (!err)
			break;
		SetLastSystemError(err);
		ManageSystemError(retryCounter, "the previous version of {} cannot be moved aside", finalName.c_str(), true, true); // waits, or throws
	}
	DeleteRetiredStoreFiles(retired);

	m_StoreLock = std::move(storeLock);
	return m_PartialName;
}

void StoreFilePublisher::Publish(MappedFileHandle& dataFile, MappedFileHandle* seqFile)
{
	assert(IsActive());
	assert(bool(seqFile) == m_HasSeqFile);
	if (m_Published)
		return;

	std::vector<SharedStr> retired;
	{
		StoreLockExclusive pairLock(m_StoreLock.get());
		if (!m_StoreLock->IsLatestWrite(m_FinalName, m_WriteNr))
		{
			reportF(SeverityTypeID::ST_MajorTrace, "{} is not published: a later write of it has begun", m_FinalName.c_str());
			return; // the destructor removes what this write left
		}
		// The .seq file first, the index last: until the index has its name, a reader finds no array.
		if (seqFile)
		{
			auto finalSeqName = m_FinalName + ".seq";
			if (auto err = TryRetireStoreFile(finalSeqName, retired))
				throwSystemError(err, "the file {} that is in the way cannot be moved aside", finalSeqName.c_str());
			seqFile->RenameTo(finalSeqName);
		}
		if (auto err = TryRetireStoreFile(m_FinalName, retired))
			throwSystemError(err, "the file {} that is in the way cannot be moved aside", m_FinalName.c_str());
		dataFile.RenameTo(m_FinalName);
		m_Published = true;
	}
	DeleteRetiredStoreFiles(retired);
}

StoreFilePublisher::~StoreFilePublisher()
{
	if (!IsActive() || m_Published)
		return;
	// what a write left that did not reach its publish goes, so that nothing of it can be taken for the array
	try {
		KillFileOrDir(m_PartialName, false);
		if (m_HasSeqFile)
		{
			KillFileOrDir(m_PartialName + ".seq", false);
			KillFileOrDir(m_PartialName + ".seq.tmp", false); // what a seal left that failed halfway
		}
	}
	catch (...) {}
}

void RemoveStoreLeftovers(WeakStr storeFolder)
{
	using path_t = std::filesystem::path;
	using string_t = path_t::string_type;

	auto toNative = [](const std::string& ascii) { return string_t(ascii.begin(), ascii.end()); };
	string_t marker = toNative(".~");
	auto pid = std::to_string(GetProcessIdForStoreFiles());
	string_t ownWrite = toNative(".~w" + pid + "_"), ownZombie = toNative(".~z" + pid + "_");

#if defined(WIN32)
	path_t folder(Utf8_2_wchar(ConvertDmsFileName(storeFolder).c_str()).get());
#else
	path_t folder(ConvertDmsFileName(storeFolder).c_str());
#endif

	std::error_code ec;
	std::vector<path_t> leftovers;
	for (auto it = std::filesystem::recursive_directory_iterator(folder, ec); !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec))
	{
		std::error_code entryError;
		if (!it->is_regular_file(entryError))
			continue;
		auto fileName = it->path().filename();
		const string_t& name = fileName.native();
		if (name.find(marker) == string_t::npos)
			continue;
		if (name.find(ownWrite) != string_t::npos || name.find(ownZombie) != string_t::npos)
			continue; // this process may still write or map it
		leftovers.emplace_back(it->path());
	}
	for (const auto& leftover : leftovers)
	{
		std::error_code removeError; // a file that cannot go now is tried again by the next session
		std::filesystem::remove(leftover, removeError);
	}
	if (!leftovers.empty())
		reportF(SeverityTypeID::ST_MajorTrace, "{} temporary file(s) of an interrupted write removed from {}", leftovers.size(), storeFolder.c_str());
}

