// Copyright (C) 1998-2024 Object Vision b.v. 
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
#include "set/MemPageTable.h"
#include "set/RangeFuncs.h"
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
	assert(memPageAllocTable);

	auto newChunk = memPageAllocTable->ReallocChunk(FreeChunk(viewSpec.offset, viewSpec.offset + viewSpec.capacity), newCapacity, m_AllocatedSize);
	assert(newChunk.first >= memPageAllocTable->m_ViewSpec.capacity);
	assert(newChunk.first < newChunk.second);
	assert(newChunk.first + newCapacity  == newChunk.second);

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

//  -----------------------------------------------------------------------

MappedFileHandle::MappedFileHandle()
{}

MappedFileHandle::~MappedFileHandle()
{}


void MappedFileHandle::MapFile(bool alsoWrite)
{
	m_hFileMapping = WinHandle();
	if (!m_hFile)
		throwErrorF("CreateFileMapping", "%s('%s') failed"
			, alsoWrite ? "CreateFile" : "OpenFile", m_FileName);

	if (m_FileSize == UNDEFINED_FILE_SIZE)
		throwErrorF("CreateFileMapping", "%s('%s') failed because MappingSize is undefined"
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
			throwSystemError(lastErr, "FileMapHandle('%s').MapViewOfFile(%I64u)", mappedFile->GetFileName().c_str(), (UInt64)viewCapacity);
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

FileViewHandle::FileViewHandle(std::shared_ptr<MappedFileHandle> mfh, dms::filesize_t viewOffset, dms::filesize_t viewSize, dms::filesize_t viewCapacity)
	: m_MappedFile(mfh)
{
	if (viewOffset == -1)
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

	if (viewOffset == -1)
		m_ViewSpec = cmfh->allocAtEnd(viewSize, viewCapacity);
	else
		m_ViewSpec = { viewOffset, viewSize, viewCapacity };

	assert(m_ViewSpec.size <= m_ViewSpec.capacity);

	MakeMin(m_ViewSpec.capacity, cmfh->GetFileSize() - m_ViewSpec.offset);

	if (m_ViewSpec.size == -1)
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

}

void mempage_table::FreeAllocatedChunk(FreeChunk currChunk)
{
	if (!currChunk.size())
		return;

	//search in lexi before and after it
	auto it = m_FreeListByLexi.lower_bound(currChunk);
	if (it != m_FreeListByLexi.begin())
	{
		auto prevIt = it;
		auto prevChunk = *--prevIt;
		if (prevChunk.second == currChunk.first)
		{
			m_FreeListBySize.erase(prevChunk);
			it = m_FreeListByLexi.erase(prevIt);
			currChunk.first = prevChunk.first;
		}
	}
	if (it != m_FreeListByLexi.end() && it->first == currChunk.second)
	{
		auto nextChunk = *it;
		it = m_FreeListByLexi.erase(it);
		m_FreeListBySize.erase(nextChunk);
		currChunk.second = nextChunk.second;
	}
	m_FreeListByLexi.insert(it, currChunk);
	m_FreeListBySize.insert(currChunk);
}

FreeChunk mempage_table::ReallocChunk(FreeChunk currChunk, dms::filesize_t newSize, dms::filesize_t fileSize)
{
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
				if (newEnd < freeChunk.second)
				{
					freeChunk.first = newEnd;
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

		if (newSize < it2->size())
		{
			freeChunk.first = currChunk.first + newSize;
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
			m_FreeListBySize.erase(lastChunk);
			m_FreeListByLexi.erase(lastChunkIt);
			auto newChunk = FreeChunk(currChunk.first, currChunk.first + newSize);
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


#else //defined(WIN32)

// GNU TODO

#endif //defined(WIN32)

