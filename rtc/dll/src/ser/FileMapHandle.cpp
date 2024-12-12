// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "ser/FileMapHandle.h"
#include "set/FileView.h"

#include "geo/MinMax.h"
#include "geo/Undefined.h"
#include "ser/FileCreationMode.h"
#include "utl/Environment.h"
#include "utl/MemGuard.h"
#include "utl/scoped_exit.h"

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

UInt8 GetLog2AllocationGrannularityImpl()
{
	auto x = GetAllocationGrannularityImpl();
	MG_CHECK2(std::popcount(x) == 1, "System Allocation Grannularity is unexpectedly not a power of 2");

	auto y = sizeof(UInt32) * 8 - std::countl_zero(x) -1;
	MG_CHECK2(x == (1 << y), "System Allocation Grannularity is unexpectedly not a power of 2");
	return y;
}

UInt8 GetLog2AllocationGrannularity()
{
	static auto result = GetLog2AllocationGrannularityImpl();
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

	do {
		fileHandle= CreateFile(
				ConvertDmsFileName(fileName).c_str()  // file name
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
	}	while ((fileHandle == INVALID_HANDLE_VALUE) && ManageSystemError(retryCounter, "CreateFileHandleForRwView(%s)", fileName.c_str(), true, doRetry));
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

void FileHandle::OpenForRead(WeakStr fileName, bool throwOnError, bool doRetry, bool mayBeEmpty)
{
	assert(!IsOpen());

	HANDLE fileHandle;
	UInt32 retryCounter = 0;
	do {
		fileHandle = CreateFile(
				ConvertDmsFileName(fileName).c_str()
			,	GENERIC_READ                            // access mode
			,	FILE_SHARE_READ                         // share mode
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
	}	while ((fileHandle == INVALID_HANDLE_VALUE) && ManageSystemError(retryCounter, "FileMapHandle(%s).OpenForRead", fileName.c_str(), throwOnError, doRetry));

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
		throwSystemError(GetLastError(), "GetFileSize(%S)", handleName);
	m_FileSize = fileSize.QuadPart;
	dbg_assert(m_FileSize <= sd_MaxFileSize);
}

FileChunckSpec MappedFileHandle::allocAtEnd(dms::filesize_t viewSize, dms::filesize_t viewCapacity)
{
	auto awaitExclusiveAcces = std::scoped_lock(m_ResizeMutex);

	m_AllocatedSize = (NrMemPages(m_AllocatedSize) << GetLog2AllocationGrannularity());
	auto newOffset = m_AllocatedSize;
	m_AllocatedSize += viewCapacity;

	if (m_AllocatedSize > GetFileSize())
	{
//		assert(!m_ResizeMutex.try_lock_shared()); // caller must have locked this exclusively.

		SetFileSize(m_AllocatedSize);
		assert(m_FileSize == m_AllocatedSize);
		MapFile(true);
	}
	assert((newOffset & (GetAllocationGrannularity() - 1)) == 0);
	return { newOffset, viewSize, viewCapacity};
}

//  -----------------------------------------------------------------------

MappedFileHandle::MappedFileHandle()
{}

MappedFileHandle::~MappedFileHandle()
{}


void MappedFileHandle::MapFile(bool alsoWrite)
{
	m_hFileMapping = WinHandle();

	WinHandle fileMapping =
		CreateFileMapping(
			m_hFile,                           // Current file handle. 
			nullptr,                           // Default security. 
			alsoWrite ? PAGE_READWRITE : PAGE_READONLY,
			HiDWORD(m_FileSize), LoDWORD(m_FileSize),
			nullptr                            // Name of mapping object. 
		);
	if (fileMapping == nullptr)
		throwLastSystemError("FileMapHandle('%s').CreateFileMapping(%I64u)", m_FileName.c_str(), (UInt64)m_FileSize);

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

void MappedFileHandle::OpenForRead(WeakStr fileName, bool throwOnError, bool doRetry)
{
	FileHandle::OpenForRead(fileName, throwOnError, doRetry);

	MapFile(false);
}

//  -----------------------------------------------------------------------

FileViewHandle::FileViewHandle(std::shared_ptr<MappedFileHandle> mfh, dms::filesize_t viewOffset, dms::filesize_t viewSize, dms::filesize_t viewCapacity)
	: m_MappedFile(mfh)
{
	if (viewOffset == -1)
	{
		assert(mfh);
//		auto lockThis = std::lock_guard(mfh->m_ResizeMutex);
		m_ViewSpec = mfh->allocAtEnd(viewSize, viewCapacity);
	}
	else
		m_ViewSpec = { viewOffset, viewSize, viewCapacity };
	assert((m_ViewSpec.offset & (GetAllocationGrannularity()-1)) == 0);
//	MapFile(false);
}

ConstFileViewHandle::ConstFileViewHandle(std::shared_ptr<ConstMappedFileHandle> cmfh, dms::filesize_t viewOffset, dms::filesize_t viewSize, dms::filesize_t viewCapacity)
	:	m_MappedFile(cmfh)
{
	if (viewOffset == -1)
		m_ViewSpec = cmfh->allocAtEnd(viewSize, viewCapacity);
	else
		m_ViewSpec = { viewOffset, viewSize, viewCapacity };
	assert((m_ViewSpec.offset & (GetAllocationGrannularity() - 1)) == 0);

	MakeMin(m_ViewSpec.offset  , cmfh->GetFileSize() & ~(GetAllocationGrannularity() - 1));
	MakeMin(m_ViewSpec.capacity, cmfh->GetFileSize() - m_ViewSpec.offset);

	if (m_ViewSpec.size == -1)
		m_ViewSpec.size = m_ViewSpec.capacity;

	MG_CHECK(m_ViewSpec.size <= m_ViewSpec.capacity);

	assert((m_ViewSpec.offset & (GetAllocationGrannularity() - 1)) == 0);

//	MapFile(true);
}

void FileViewHandle::operator =(FileViewHandle&& rhs) noexcept
{
	m_MappedFile = std::move(rhs.m_MappedFile); assert(!rhs.m_MappedFile);
	std::swap(m_ViewSpec, rhs.m_ViewSpec);
	std::swap(m_ViewData, rhs.m_ViewData);

	assert((m_ViewSpec.offset & (GetAllocationGrannularity() - 1)) == 0);
}

void ConstFileViewHandle::operator =(ConstFileViewHandle&& rhs) noexcept
{
	m_MappedFile = std::move(rhs.m_MappedFile); assert(!rhs.m_MappedFile);
	std::swap(m_ViewSpec, rhs.m_ViewSpec);
	std::swap(m_ViewData, rhs.m_ViewData);

	assert((m_ViewSpec.offset & (GetAllocationGrannularity() - 1)) == 0);
}

void FileViewHandle::MapView(bool alsoWrite)
{
	MG_CHECK(m_MappedFile); // precondition

	auto lock = std::scoped_lock(m_MappedFile->m_ResizeMutex);

	if (m_ViewData)
	{
		UnmapViewOfFile(m_ViewData);
		m_ViewData = nullptr;
	}

	if (!GetViewCapacity())
		return;

/*
	std::unique_lock< std::shared_mutex> uniqueLock;
	std::shared_lock< std::shared_mutex> sharedLock;

	if (alsoWrite)
		uniqueLock = std::unique_lock(m_MappedFile->m_ResizeMutex);
	else
		sharedLock = std::shared_lock(m_MappedFile->m_ResizeMutex);
*/
	m_AlsoWrite = alsoWrite;

	assert((m_ViewSpec.offset & (GetAllocationGrannularity() - 1)) == 0);

	while (true) {
		m_ViewData =
			MapViewOfFile(m_MappedFile->m_hFileMapping, // Handle to mapping object. 
				alsoWrite ? FILE_MAP_WRITE : FILE_MAP_READ,
				HiDWORD(GetViewOffset()),
				LoDWORD(GetViewOffset()),
				GetViewCapacity() // NrMemPages(GetViewSize()) << GetLog2AllocationGrannularity()
			);
		if (m_ViewData)
		{
/*
			if (alsoWrite)
				uniqueLock.release();
			else
				sharedLock.release();
*/
			break;
		}

		DWORD lastErr = GetLastError();
		if (lastErr != ERROR_NOT_ENOUGH_MEMORY || !DMS_CoalesceHeap(GetViewCapacity()))
			throwSystemError(lastErr, "FileMapHandle('%s').MapViewOfFile(%I64u)", m_MappedFile->GetFileName().c_str(), (UInt64)GetViewCapacity());
	};
}

void FileViewHandle::CloseView()
{
	if (!m_ViewData)
		return;

	UnmapViewOfFile(m_ViewData);
	m_ViewData = nullptr;
}

void ConstFileViewHandle::MapView()
{
	MG_CHECK(m_MappedFile); // precondition

	if (m_ViewData)
		return;

	auto lock = std::scoped_lock(m_MappedFile->m_ResizeMutex);

	while (true) {
		m_ViewData =
			MapViewOfFile(m_MappedFile->m_hFileMapping, // Handle to mapping object. 
				FILE_MAP_READ,
				HiDWORD(GetViewOffset()),
				LoDWORD(GetViewOffset()),
				GetViewCapacity()
			);
		if (m_ViewData)
		{
			break;
		}

		DWORD lastErr = GetLastError();
		if (lastErr != ERROR_NOT_ENOUGH_MEMORY || !DMS_CoalesceHeap(GetViewCapacity()))
			throwSystemError(lastErr, "FileMapHandle('%s').MapViewOfFile(%I64u)", m_MappedFile->GetFileName().c_str(), (UInt64)GetViewCapacity());
	};
}

void ConstFileViewHandle::CloseView()
{
	if (!m_ViewData)
		return;
	UnmapViewOfFile(m_ViewData);
	m_ViewData = nullptr;
}

void FileViewHandle::realloc(dms::filesize_t capacity)
{
	assert(IsUsable());
	m_ViewSpec = m_MappedFile->allocAtEnd(m_ViewSpec.size, capacity);
	MapView(true);
}


#else //defined(WIN32)

// GNU TODO

#endif //defined(WIN32)

