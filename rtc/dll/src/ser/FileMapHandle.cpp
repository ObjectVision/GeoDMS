// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "ser/FileMapHandle.h"

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
	dms_assert(IsWritable(fcm));
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
		dms_assert(fileHandle);
	}	while ((fileHandle == INVALID_HANDLE_VALUE) && ManageSystemError(retryCounter, "CreateFileHandleForRwView(%s)", fileName.c_str(), true, doRetry));
	return fileHandle;
}


//  -----------------------------------------------------------------------

FileHandle::FileHandle()
	:	m_hFile   (NULL)
	,	m_FileSize(UNDEFINED_FILE_SIZE)
	,	m_IsTmp    (false)
{
	MG_DEBUG_DATA_CODE( m_FCM = FCM_Undefined; )
}

FileHandle::~FileHandle()
{
	dms_assert(!IsOpen());
	// REMOVE IF ASSERTION IS PROVEN
	if (IsOpen())
		CloseFile(); 
}

void FileHandle::OpenRw(WeakStr fileName, SafeFileWriterArray* sfwa, dms::filesize_t requiredNrBytes, dms_rw_mode rwMode, bool isTmp, bool doRetry, bool deleteOnClose)
{
	dms_assert(!IsOpen());
	dms_assert(rwMode != dms_rw_mode::unspecified);
	dms_assert(rwMode >= dms_rw_mode::read_write);
	m_IsTmp     = isTmp;
	bool readData  = (rwMode < dms_rw_mode::write_only_mustzero);

	FileCreationMode fcm = readData ? FCM_OpenRwGrowable : FCM_CreateAlways;
	m_hFile = 
		CreateFileHandleForRwView(
			GetWorkingFileName(sfwa, fileName, fcm)
		,	fcm
		,	isTmp
		,	doRetry
		,	deleteOnClose
		); // returns a valid handle or throws a system error
	dms_assert(IsOpen());
	dms_assert(m_hFile != INVALID_HANDLE_VALUE);
	MG_DEBUG_DATA_CODE( m_FCM = fcm; )

	if (IsDefined(requiredNrBytes))
	{
		m_FileSize = requiredNrBytes;
	}
	else
		if (readData)
			ReadFileSize(fileName.c_str());
		else
			m_FileSize = 0;
	dms_assert(m_FileSize != UNDEFINED_FILE_SIZE);
	dbg_assert(m_FileSize <= sd_MaxFileSize);
	// returns with valid m_hFileMap or throws a system error
}

void FileHandle::OpenForRead(WeakStr fileName, SafeFileWriterArray* sfwa, bool throwOnError, bool doRetry, bool mayBeEmpty)
{
	dms_assert(!IsOpen());

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
		dms_assert(!throwOnError);
		fileHandle = NULL;
	}

	m_hFile = fileHandle;
	dms_assert(m_hFile != INVALID_HANDLE_VALUE);

	dms_assert(IsOpen() || !throwOnError);
	MG_DEBUG_DATA_CODE( m_FCM = FCM_OpenReadOnly; )

	if (IsOpen())
		ReadFileSize(fileName.c_str());
	else
		m_FileSize = UNDEFINED_FILE_SIZE;
	
	dbg_assert(m_FileSize <= sd_MaxFileSize || m_FileSize == UNDEFINED_FILE_SIZE);
}

void FileHandle::CloseFile()
{
	dms_assert(m_hFile);
	dms_assert(m_hFile != INVALID_HANDLE_VALUE);
	CloseHandle(m_hFile);
	m_hFile = NULL;
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

//  -----------------------------------------------------------------------

FileMapHandle::FileMapHandle()
	:	m_hFileMap(0)
	,	m_ViewData(0)
{
}

FileMapHandle::~FileMapHandle()
{
	CloseFMH();
}

//  -----------------------------------------------------------------------

void FileMapHandle::realloc(dms::filesize_t requiredNrBytes, WeakStr fileName, SafeFileWriterArray* sfwa)
{
	dms_assert(IsOpen());
	dms_assert(IsUsable());
	MGD_CHECKDATA(IsWritable(m_FCM) );

	dms_rw_mode rwMode = (m_FileSize) ? dms_rw_mode::read_write : dms_rw_mode::write_only_all;

	if( requiredNrBytes == m_FileSize)
		return;
	CloseView(requiredNrBytes == 0);
	m_FileSize = requiredNrBytes;
	dbg_assert(m_FileSize <= sd_MaxFileSize);
	Map(rwMode, fileName, sfwa);
	// returns with valid m_hFileMap, m_ViewData and m_FileSize or throws a system error
}

void FileMapHandle::CloseFMH()
{
	if (IsOpen())
	{
		dms_assert(!IsMapped()); // Lock should be removed before closing
		if (IsMapped())
			CloseView(false); // REMOVE if assert if proven
		CloseFile();
	}
	dms_assert(!IsMapped());
	dms_assert(!m_hFile);
}

struct WinHandle
{
	WinHandle(HANDLE hnd)
		:	m_Hnd(hnd)
	{}
	~WinHandle()
	{
		CloseHandle(m_Hnd);
	}
	HANDLE m_Hnd;
};

void FileHandle::DropFile(WeakStr fileName)
{
//	dms_assert(IsOpen());
	dms_assert(m_hFile != INVALID_HANDLE_VALUE);

	if (IsOpen())
		CloseFile();

//	ReOpenFile only valid for Vista
//	FileHandle hnd(ReOpenFile(m_hFile, GENERIC_DELETE, SHARE_ALL, FILE_FLAG_DELETE_ON_CLOSE));
	KillFileOrDir(fileName); // file was opened with SHARE_DELETE?

	dms_assert(!IsOpen());
	dbg_assert(!IsFileOrDirAccessible(fileName));
}

void FileMapHandle::Drop(WeakStr fileName)
{
//	dms_assert(IsOpen());
	dms_assert(m_hFile != INVALID_HANDLE_VALUE);

	CloseView(true);

	DropFile(fileName);
//	ReOpenFile only valid for Vista
	dms_assert(!IsMapped());
}

void FileMapHandle::Unmap()
{
	dms_assert(IsUsable());
	if (IsOpen())
		CloseView(false);
	dms_assert(!IsMapped());
}

inline DWORD HiDWORD(UInt64 qWord) { return qWord >> 32; }
inline DWORD HiDWORD(UInt32 dWord) { return 0; }

//  -----------------------------------------------------------------------

void FileMapHandle::Map(dms_rw_mode rwMode, WeakStr fileName, SafeFileWriterArray* sfwa)
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
	dms_assert(IsOpen());

	dms_assert(!IsMapped());
	dms_assert(IsUsable() == (m_FileSize == 0)); // m_FileSize == 0 implies IsUsable

	dms_assert( m_FileSize != UNDEFINED_FILE_SIZE);

	if (IsMapped())
		return;

	dms_assert(m_hFileMap == NULL);
	dms_assert(m_ViewData == NULL); 
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

	while (true) {
		m_ViewData = 
			MapViewOfFile(fileMap.m_Hnd,       // Handle to mapping object. 
				alsoWrite ? FILE_MAP_ALL_ACCESS : FILE_MAP_READ,
				0,                             // high-order DWORD of offset
				0,                             // low-order DWORD of offset
				m_FileSize                     // Map entire file. 
			);
		if (m_ViewData)
			break;

		DWORD lastErr = GetLastError();
		if (lastErr != ERROR_NOT_ENOUGH_MEMORY || !DMS_CoalesceHeap(m_FileSize))
			throwSystemError(lastErr, "FileMapHandle('%s').MapViewOfFile(%I64u)", fileName.c_str(), (UInt64)m_FileSize);
	};

	m_hFileMap = fileMap.m_Hnd; fileMap.m_Hnd = 0;
}

void FileMapHandle::CloseView(bool drop)
{
	if (!m_hFile)
	{
		dms_assert(!m_ViewData);
		dms_assert(!m_hFileMap);
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

#else //defined(WIN32)

// GNU TODO

#endif //defined(WIN32)

