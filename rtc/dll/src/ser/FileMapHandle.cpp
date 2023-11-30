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
#include "ser/SafeFileWriter.h"
#include "utl/Environment.h"
#include "utl/MemGuard.h"

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

UInt32 GetAllocationGrannularity()
{
	static UInt32 allocGrannularity = GetAllocationGrannularityImpl();
	return allocGrannularity;
}

UInt8 GetLog2AllocationGrannularityImpl()
{
	UInt32 x = GetAllocationGrannularity();
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

void FileHandle::OpenRw(WeakStr fileName, SafeFileWriterArray* sfwa, dms::filesize_t requiredNrBytes, dms_rw_mode rwMode, bool isTmp, bool doRetry, bool deleteOnClose)
{
	assert(!IsOpen());
	assert(rwMode != dms_rw_mode::unspecified);
	assert(rwMode >= dms_rw_mode::read_write);
	m_IsTmp     = isTmp;
	bool readData  = (rwMode < dms_rw_mode::write_only_mustzero);

	FileCreationMode fcm = readData ? FCM_OpenRwGrowable : FCM_CreateAlways;
	m_hFile = WinHandle(
		CreateFileHandleForRwView(
			GetWorkingFileName(sfwa, fileName, fcm)
		,	fcm
		,	isTmp
		,	doRetry
		,	deleteOnClose
		)
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

void FileHandle::OpenForRead(WeakStr fileName, SafeFileWriterArray* sfwa, bool throwOnError, bool doRetry, bool mayBeEmpty)
{
	assert(!IsOpen());

	HANDLE fileHandle;
	UInt32 retryCounter = 0;
	do {
		fileHandle = CreateFile(
				ConvertDmsFileName(
					GetWorkingFileName(sfwa, fileName, FCM_OpenReadOnly)
				).c_str()                               // file name
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

FileChunckSpec MappedFileHandle::alloc(dms::filesize_t vs)
{
	auto fs = m_AllocatedSize;
	m_AllocatedSize = (NrMemPages(m_AllocatedSize) << GetLog2AllocationGrannularity());
	m_AllocatedSize += vs;

	if (m_AllocatedSize > GetFileSize())
	{
		auto awaitExclusiveAcces = std::scoped_lock(m_ResizeMutex);
		SetFileSize(m_AllocatedSize);
		assert(m_FileSize == m_AllocatedSize);
		Map(true);
	}
	return {fs, vs};
}

//  -----------------------------------------------------------------------

MappedFileHandle::MappedFileHandle()
{}

MappedFileHandle::~MappedFileHandle()
{}


void MappedFileHandle::Map(bool alsoWrite)
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

void MappedFileHandle::OpenRw(WeakStr fileName, SafeFileWriterArray* sfwa, dms::filesize_t requiredNrBytes, dms_rw_mode rwMode, bool isTmp)
{
	FileHandle::OpenRw(fileName, sfwa, requiredNrBytes, rwMode, isTmp);
	if (m_FileSize == 0)
	{
		m_hFileMapping = WinHandle();
		return;
	}
	Map(true);
}

void MappedFileHandle::OpenForRead(WeakStr fileName, SafeFileWriterArray* sfwa, bool throwOnError, bool doRetry)
{
	FileHandle::OpenForRead(fileName, sfwa, throwOnError, doRetry);

	Map(false);
}

//  -----------------------------------------------------------------------

FileViewHandle::FileViewHandle(std::shared_ptr<MappedFileHandle> mfh, dms::filesize_t viewOffset, dms::filesize_t viewSize)
	: m_MappedFile(mfh)
{
	if (viewOffset == -1)
		m_ViewSpec = mfh->alloc(viewSize);
	else
		m_ViewSpec = { viewOffset, viewSize };
//	Map(false);
}

ConstFileViewHandle::ConstFileViewHandle(std::shared_ptr<ConstMappedFileHandle> cmfh, dms::filesize_t viewOffset, dms::filesize_t viewSize)
	:	m_MappedFile(cmfh)
{
	if (viewOffset == -1)
		m_ViewSpec = cmfh->alloc(viewSize);
	else
		m_ViewSpec = { viewOffset, viewSize };

	MakeMin(m_ViewSpec.first, cmfh->GetFileSize());
	MakeMin(m_ViewSpec.second, cmfh->GetFileSize() - m_ViewSpec.first);

//	Map(true);
}

void FileViewHandle::operator =(FileViewHandle&& rhs) noexcept
{
	m_MappedFile = std::move(rhs.m_MappedFile); assert(!rhs.m_MappedFile);
	std::swap(m_ViewSpec, rhs.m_ViewSpec);
	std::swap(m_ViewData, rhs.m_ViewData);
}

void ConstFileViewHandle::operator =(ConstFileViewHandle&& rhs) noexcept
{
	m_MappedFile = std::move(rhs.m_MappedFile); assert(!rhs.m_MappedFile);
	std::swap(m_ViewSpec, rhs.m_ViewSpec);
	std::swap(m_ViewData, rhs.m_ViewData);
}

void FileViewHandle::Map(bool alsoWrite)
{
	MG_CHECK(m_MappedFile); // precondition

	if (m_ViewData || !GetViewSize())
		return;

	m_MappedFile->m_ResizeMutex.lock_shared();

	while (true) {
		m_ViewData =
			MapViewOfFile(m_MappedFile->m_hFileMapping, // Handle to mapping object. 
				alsoWrite ? FILE_MAP_WRITE : FILE_MAP_READ,
				HiDWORD(GetViewOffset()),
				LoDWORD(GetViewOffset()),
				GetViewSize() // NrMemPages(GetViewSize()) << GetLog2AllocationGrannularity()
			);
		if (m_ViewData)
			break;

		DWORD lastErr = GetLastError();
		if (lastErr != ERROR_NOT_ENOUGH_MEMORY || !DMS_CoalesceHeap(GetViewSize()))
		{
			m_MappedFile->m_ResizeMutex.unlock_shared();
			throwSystemError(lastErr, "FileMapHandle('%s').MapViewOfFile(%I64u)", m_MappedFile->GetFileName().c_str(), (UInt64)GetViewSize());
		}
	};
}

void FileViewHandle::CloseView()
{
	if (!m_ViewData)
		return;
	UnmapViewOfFile(m_ViewData); 
	m_ViewData = nullptr;
	m_MappedFile->m_ResizeMutex.unlock_shared();
}

void ConstFileViewHandle::Map()
{
	MG_CHECK(m_MappedFile); // precondition

	if (m_ViewData)
		return;

	m_MappedFile->m_ResizeMutex.lock_shared();

	while (true) {
		m_ViewData =
			MapViewOfFile(m_MappedFile->m_hFileMapping, // Handle to mapping object. 
				FILE_MAP_READ,
				HiDWORD(GetViewOffset()),
				LoDWORD(GetViewOffset()),
				GetViewSize()
			);
		if (m_ViewData)
			break;

		DWORD lastErr = GetLastError();
		if (lastErr != ERROR_NOT_ENOUGH_MEMORY || !DMS_CoalesceHeap(GetViewSize()))
		{
			m_MappedFile->m_ResizeMutex.unlock_shared();
			throwSystemError(lastErr, "FileMapHandle('%s').MapViewOfFile(%I64u)", m_MappedFile->GetFileName().c_str(), (UInt64)GetViewSize());
		}
	};
}

void ConstFileViewHandle::CloseView()
{
	if (!m_ViewData)
		return;
	UnmapViewOfFile(m_ViewData);
	m_ViewData = nullptr;
	m_MappedFile->m_ResizeMutex.unlock_shared();
}

void FileViewHandle::realloc(dms::filesize_t requiredNrBytes)
{
	assert(IsUsable());
//	MGD_CHECKDATA(IsWritable(m_FCM) );
	m_ViewSpec = m_MappedFile->alloc(requiredNrBytes);
/*
	dms_rw_mode rwMode = (m_FileSize) ? dms_rw_mode::read_write : dms_rw_mode::write_only_all;

	if( requiredNrBytes == m_FileSize)
		return;
	CloseView(requiredNrBytes == 0);
	m_FileSize = requiredNrBytes;
	dbg_assert(m_FileSize <= sd_MaxFileSize);
	Map(rwMode, fileName, sfwa);
	// returns with valid m_hFileMap, m_ViewData and m_FileSize or throws a system error
*/
}

/*

void MappedFileHandle::CloseFMH()
{
	if (IsOpen())
	{
		assert(!IsMapped()); // Lock should be removed before closing
		if (IsMapped())
			CloseView(false); // REMOVE if assert if proven
		CloseFile();
	}
	assert(!IsMapped());
	assert(!m_hFile);
}

void FileHandle::DropFile(WeakStr fileName)
{
//	assert(IsOpen());
	assert(m_hFile != INVALID_HANDLE_VALUE);

	if (IsOpen())
		CloseFile();

//	ReOpenFile only valid for Vista
//	FileHandle hnd(ReOpenFile(m_hFile, GENERIC_DELETE, SHARE_ALL, FILE_FLAG_DELETE_ON_CLOSE));
	KillFileOrDir(fileName); // file was opened with SHARE_DELETE?

	assert(!IsOpen());
	dbg_assert(!IsFileOrDirAccessible(fileName));
}

void FileMapHandle::Drop(WeakStr fileName)
{
//	assert(IsOpen());
	assert(m_hFile != INVALID_HANDLE_VALUE);

	CloseView(true);

	DropFile(fileName);
//	ReOpenFile only valid for Vista
	assert(!IsMapped());
}

void FileViewHandle::Unmap()
{
	assert(IsUsable());
	if (IsOpen())
		CloseView(false);
	assert(!IsMapped());
}
*/
//  -----------------------------------------------------------------------

/*
void FileViewHandle::Map(dms_rw_mode rwMode, WeakStr fileName, SafeFileWriterArray* sfwa)
{
	bool alsoWrite = rwMode > dms_rw_mode::read_only;

	MGD_CHECKDATA(!alsoWrite || m_FCM != FCM_OpenReadOnly);

#if defined(DMS_CLOSING_UNMAP)
	if (!IsOpen())
	{
		if (alsoWrite)
			OpenRw(fileName, sfwa, m_FileSize, rwMode, m_IsTmp);
		else
			OpenForRead(fileName, sfwa, true, true);
	}
#endif
	assert(IsOpen());

	assert(!IsMapped());
	assert(IsUsable() == (m_FileSize == 0)); // m_FileSize == 0 implies IsUsable

	assert( m_FileSize != UNDEFINED_FILE_SIZE);

	if (IsMapped())
		return;

	assert(m_hFileMap == NULL);
	assert(m_ViewData == NULL); 
	if (!m_FileSize)
		return;

	WaitForAvailableMemory(m_FileSize);

	WinHandle fileMap = 
		CreateFileMapping(
			m_hFile,                           // Current file handle. 
			NULL,                              // Default security. 
			alsoWrite ? PAGE_READWRITE : PAGE_READONLY, 
			HiDWORD(m_FileSize),               // high-order DWORD of size
			m_FileSize & DWORD(-1),            // low-order DWORD of size
			NULL                               // Name of mapping object. 
		);
	if (fileMap.m_Hnd == NULL) 
		throwLastSystemError("FileMapHandle('%s').CreateFileMapping(%I64u)", fileName.c_str(), (UInt64)m_FileSize);


	m_hFileMap = fileMap.m_Hnd; fileMap.m_Hnd = 0;
}

void FileViewHandle::CloseView(bool drop)
{
	if (!m_hFile)
	{
		assert(!m_ViewData);
		assert(!m_hFileMap);
		return;
	}

	if (m_ViewData) { UnmapViewOfFile(m_ViewData); m_ViewData = nullptr; }
	if (m_hFileMap) { CloseHandle(m_hFileMap);     m_hFileMap = NULL; }

	if (m_FileSize != UNDEFINED_FILE_SIZE || drop)
	{
		dms::filesize_t fileSize = (drop ? 0 : m_FileSize);
		LONG fileSizeHigh = HiDWORD(fileSize);
		SetFilePointer(
			m_hFile, 
			fileSize & DWORD(-1), 
			&fileSizeHigh, 
			FILE_BEGIN
		);
		SetEndOfFile(m_hFile);
	}
#if defined(DMS_CLOSING_UNMAP)
	CloseFile();
#endif
}
*/
//  -----------------------------------------------------------------------
/*
ConstFileViewHandle::ConstFileViewHandle(ConstMappedFileHandle& mfh, dms::filesize_t viewOffset, dms::filesize_t viewSize)
{
	if (cmfh.IsOpen())
		Map(dms_rw_mode::read_only, fileName);
}
*/

#else //defined(WIN32)

// GNU TODO

#endif //defined(WIN32)

