// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_SER_FILEMAPHANDLE_H)
#define __RTC_SER_FILEMAPHANDLE_H

#include "cpc/Types.h"
#include "vt/IndexRange.h"
#include "ser/FileCreationMode.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>

// the following define should clean-up resources of Closed FileMap Handles by Closing the file
#define DMS_CLOSING_UNMAP

#define UNDEFINED_FILE_SIZE UNDEFINED_VALUE(dms::filesize_t)

//----------------------------------------------------------------------

SizeT GetAllocationGrannularity();
UInt8 GetLog2AllocationGrannularity();

RTC_CALL UInt8 GetLog2MemPageSize();

inline SizeT NrMemPages(SizeT nrBytes)
{
	auto LOG_MEM_PAGE_SIZE = GetLog2MemPageSize();
	nrBytes += ((1 << LOG_MEM_PAGE_SIZE) - 1);
	return nrBytes >> LOG_MEM_PAGE_SIZE;
}

//  -----------------------------------------------------------------------

struct WinHandle
{
	WinHandle(HANDLE hnd = nullptr)
		: m_Hnd(hnd)
	{}

	WinHandle(WinHandle&& rhs) noexcept
	{
		operator =(std::move(rhs));
	}
	void operator = (WinHandle&& rhs) noexcept
	{
		std::swap(m_Hnd, rhs.m_Hnd); 
	}

	WinHandle(const WinHandle& rhs) = delete;
	void operator = (const WinHandle&) = delete;

	RTC_CALL ~WinHandle(); // CloseHandle(m_Hnd) // exported: stg FileSystemStorageManager's FileHandle ctor-unwind needs it in Debug links (/OPT:REF strips the reference in Release)

	operator HANDLE() const { return m_Hnd; }

private:
	HANDLE m_Hnd = nullptr;
};

//  -----------------------------------------------------------------------

struct FileChunkSpec {
	dms::filesize_t offset = 0, size = 0, capacity = 0;
};

struct FileHandle
{
	FileHandle() = default;

	RTC_CALL ~FileHandle();

	RTC_CALL void OpenRw (WeakStr fileName, dms::filesize_t requiredNrBytes, dms_rw_mode rwMode, bool isTmp, bool retry = true, bool deleteOnClose = false);
	// shareDelete lets another handle move or delete the file while this one has it open (#1298)
	RTC_CALL void OpenForRead(WeakStr fileName, bool throwOnError, bool doRetry, bool mayBeEmpty = false, bool shareDelete = false);
	RTC_CALL void CloseFile();
	RTC_CALL void DropFile (WeakStr fileName);

	bool IsOpen  () const { return m_hFile; }

	dms::filesize_t GetFileSize() const { return m_FileSize; }
	void  SetFileSize(dms::filesize_t requiredNrBytes);

	SharedStr GetFileName() const { return m_FileName; }

protected:
	void ReadFileSize(CharPtr handleName);

	WinHandle        m_hFile = nullptr;
	dms::filesize_t  m_FileSize = UNDEFINED_FILE_SIZE;
	SharedStr        m_FileName;

	bool m_IsTmp           : 1 = false;
	FileCreationMode m_FCM : 3 = FCM_Undefined;
};

struct mempage_table;

struct MappedFileHandle : FileHandle
{
	RTC_CALL MappedFileHandle();
	RTC_CALL ~MappedFileHandle();

	void OpenRw(WeakStr fileName, dms::filesize_t requiredNrBytes, dms_rw_mode rwMode, bool isTmp);
	RTC_CALL void OpenForRead(WeakStr fileName, bool throwOnError, bool doRetry, bool shareDelete = false);

	// #1298: gives this file, open and mapped as it is, another name; the views and the handle keep working
	void RenameTo(WeakStr newFileName);

	FileChunkSpec AllocFile (FileChunkSpec& viewSpec, dms::filesize_t viewCapacity);
	FileChunkSpec allocChunk(FileChunkSpec& viewSpec, dms::filesize_t viewCapacity, tile_id t);
	FileChunkSpec allocAtEnd(dms::filesize_t viewSize, dms::filesize_t viewCapacity);

	void MapFile(bool alsoWrite);

	// #1280: the seal of a stored sequence pool ends the write of its .seq file. It closes the file, after
	// cutting it to newSize or after putting a compacted copy in its place; no view may be mapped then.
	// ReopenIfClosed opens it again, for reading; FileTileArray::FinishWrite calls it right after the seal
	// (#1298), because the name may belong to another file later on.
	void TruncateAndClose(dms::filesize_t newSize);
	void ReplaceByAndClose(WeakStr replacementFileName);
	void ReopenIfClosed(bool alsoWrite);

	WinHandle m_hFileMapping;
	std::shared_mutex m_ResizeMutex;

	std::unique_ptr< mempage_table > m_MemPageAllocTable;
	dms::filesize_t m_AllocatedSize = 0;

private:
	void CloseForReopen();

	std::atomic<bool> m_ClosedForReopen = false;
	std::mutex        m_ReopenMutex;
public:

	MappedFileHandle(const MappedFileHandle&&) = delete;
	MappedFileHandle(MappedFileHandle&&) = delete;
	MappedFileHandle& operator =(const MappedFileHandle&) = delete;
	MappedFileHandle& operator =(MappedFileHandle&&) = delete;
};

void CreateMemPageAllocTable(std::shared_ptr<MappedFileHandle> self, bool readOnly, tile_id tn);

// #1280: writes fileName afresh as the compact .seq file of tn tiles: for more than one tile the chunk table,
// then the pools of the tiles back to back in tile order, each with a capacity equal to its size.
// writeTile(t, dst) writes the tileBytes[t] bytes of tile t to dst.
void WriteCompactSeqFile(WeakStr fileName, tile_id tn, const dms::filesize_t* tileBytes, const std::function<void(tile_id, char*)>& writeTile);

struct ConstMappedFileHandle : MappedFileHandle
{
	ConstMappedFileHandle(WeakStr fileName, bool throwOnError = true, bool doRetry = false)
	{
		OpenForRead(fileName, throwOnError, doRetry);
		assert(IsOpen() || !throwOnError);
	}
};


struct ViewData : ptr_base<void, movable>
{
	ViewData(MappedFileHandle* mappedFile, DWORD desiredAccess, dms::filesize_t viewOffset, dms::filesize_t viewCapacity);
	RTC_CALL ~ViewData(); // exported: stg/stx ConstFileViewHandle dtor needs it in Debug links (/OPT:REF strips the reference in Release)

	ViewData() {}
	ViewData(ViewData&& rhs) noexcept
	{
		operator =(std::move(rhs));
	}
	void operator=(ViewData&& rhs) noexcept
	{
		ptr_base<void, movable >::swap(rhs);
#if !defined(_MSC_VER)
		std::swap(m_LinuxMmapSize, rhs.m_LinuxMmapSize);
#endif
	}

#if !defined(_MSC_VER)
	// Linux: track mmap size for munmap on destruction. Windows uses
	// UnmapViewOfFile which only needs the address; Linux munmap requires
	// the size and leaks the mapping if we forget. Fixed an unbounded
	// anon-RSS growth that OOM-killed long-running tile-heavy runs (t641).
	// The size counts from the start of the page that holds the view's first byte (#1297).
	std::size_t m_LinuxMmapSize = 0;
#endif
};

struct FileViewHandle
{
	using mapped_file_type = MappedFileHandle;

	FileViewHandle() = default;
	FileViewHandle(FileViewHandle&& rhs) noexcept
	{ 
		operator =(std::move(rhs));
		assert(rhs.m_ViewData.is_null());
		assert(rhs.m_ViewSpec.size == 0);
		assert(rhs.m_ViewSpec.capacity == 0);
		assert(rhs.m_ViewSpec.offset == 0);
	}

	FileViewHandle(std::shared_ptr<mapped_file_type> mfh, dms::filesize_t viewOffset, dms::filesize_t viewSize, dms::filesize_t viewCapacity);
	void operator = (FileViewHandle&& rhs) noexcept;

	void AllocAndMapFile (dms::filesize_t capacity);
	void allocAndMapChunk(dms::filesize_t capacity, tile_id t);

	bool IsUsable() const { return m_ViewData.has_ptr() || GetViewCapacity() == 0; }

	void MapView(bool alsoWrite);
	void UnmapView() { m_ViewData = ViewData(); }

	char*   DataBegin()       { assert(IsUsable()); return reinterpret_cast<char*  >(m_ViewData.get_ptr()); }
	char*   DataEnd  ()       { return DataBegin() + GetViewSize(); }
	CharPtr DataBegin() const { assert(IsUsable()); return reinterpret_cast<CharPtr>(m_ViewData.get_ptr()); }
	CharPtr DataEnd() const { return DataBegin() + GetViewSize(); }

	auto            GetViewSpec    () const { return m_ViewSpec; }
	dms::filesize_t GetViewOffset  () const { return m_ViewSpec.offset; }
	dms::filesize_t GetViewSize    () const { return m_ViewSpec.size; }
	dms::filesize_t GetViewCapacity() const { return m_ViewSpec.capacity; }

	auto GetMappedFile() const { return m_MappedFile; }

	std::shared_ptr<mapped_file_type> m_MappedFile;

	FileChunkSpec m_ViewSpec = { 0, 0, 0 };
	ViewData      m_ViewData;
	bool          m_AlsoWrite = false;
};

struct ConstFileViewHandle
{
	using mapped_file_type = ConstMappedFileHandle;

	ConstFileViewHandle() = default;
	ConstFileViewHandle(ConstFileViewHandle&& rhs) noexcept 
	{ 
		operator =(std::move(rhs));
		assert(rhs.m_ViewData.is_null());
		assert(rhs.m_ViewSpec.size == 0);
		assert(rhs.m_ViewSpec.capacity == 0);
		assert(rhs.m_ViewSpec.offset == 0);
	}

	RTC_CALL ConstFileViewHandle(std::shared_ptr<ConstMappedFileHandle> cmfh, dms::filesize_t viewOffset, dms::filesize_t viewSize, dms::filesize_t viewCapacity);
	RTC_CALL void operator = (ConstFileViewHandle&& rhs) noexcept;

	bool IsUsable() const { return m_ViewData.has_ptr() || GetViewCapacity() == 0; }

	RTC_CALL void MapView();
	void UnmapView() { m_ViewData = ViewData(); }

	CharPtr DataBegin() const { assert(IsUsable()); return reinterpret_cast<CharPtr>(m_ViewData.get_ptr()); }
	CharPtr DataEnd() const { return DataBegin() + GetViewSize(); }

	dms::filesize_t GetViewOffset  () const { return m_ViewSpec.offset; }
	dms::filesize_t GetViewSize    () const { return m_ViewSpec.size; }
	dms::filesize_t GetViewCapacity() const { return m_ViewSpec.capacity; }

	auto GetMappedFile() const { return m_MappedFile; }

protected:
	std::shared_ptr< mapped_file_type> m_MappedFile;

	FileChunkSpec m_ViewSpec = { 0, 0, 0 };
	ViewData      m_ViewData;
};

// =====================================================================
// #1298: the files of an MMD store
//
// A reader takes the presence of a file in a store for proof that the array in it is complete. So an array is
// written under a temporary name and gets its own name only when its write has finished (StoreFilePublisher),
// and the files of its previous version are moved aside under a temporary name and deleted there when its
// write begins (RetireStoreFile): a handle or view that still has them open, in this process or another,
// keeps their data until it closes, while the name is free at once. A write that fails or is interrupted
// therefore leaves no file under the array's name, and a reader reports that instead of reading zeros.
// Every temporary name contains ".~", which no item name can, so what an interrupted write left behind is
// removed when the next write session of the store begins (RemoveStoreLeftovers).
// =====================================================================

// The lock file 0Lock of a store. Its byte 0 keeps the opening of an array's two files (shared) apart from
// the replacement of that pair (exclusive); its byte 1 is held by the one process that writes the store.
// The OS releases both when the process ends. The counts make one OS lock serve all threads of this process.
struct StoreLock
{
	// forWrite creates the folder of the store and its lock file; a reader opens an existing lock file and
	// reads without lock when there is none, which is the case for a store written before 20.24.
	RTC_CALL StoreLock(WeakStr storeFolder, bool forWrite);
	RTC_CALL ~StoreLock();

	bool IsForWrite() const { return m_ForWrite; }

	// takes byte 1 for as long as this object lives; throws when another process holds it
	RTC_CALL void BeginWriteSession();

	RTC_CALL void LockShared();
	RTC_CALL void UnlockShared();
	RTC_CALL void LockExclusive();
	RTC_CALL void UnlockExclusive();

	// The writes of an array in this process are numbered; only the last one begun may publish, so a write
	// that a later one overtook cannot put an outdated version under the array's name.
	RTC_CALL UInt64 StartWrite(WeakStr finalName);
	RTC_CALL bool IsLatestWrite(WeakStr finalName, UInt64 writeNr);

	StoreLock(const StoreLock&) = delete;
	StoreLock& operator =(const StoreLock&) = delete;

private:
	bool OsLock(UInt32 byteNr, bool exclusive, bool failImmediately);
	void OsUnlock(UInt32 byteNr);

	SharedStr m_StoreFolder, m_LockFileName;
	WinHandle m_hFile;
	bool      m_ForWrite = false, m_HasWriteSession = false, m_Exclusive = false;
	UInt32    m_NrShared = 0;

	std::mutex m_Mutex;
	std::condition_variable m_Released;
	std::map<SharedStr, UInt64> m_LatestWrite;
};

struct StoreLockShared
{
	explicit StoreLockShared(StoreLock* storeLock) : m_StoreLock(storeLock) { if (m_StoreLock) m_StoreLock->LockShared(); }
	~StoreLockShared() { if (m_StoreLock) m_StoreLock->UnlockShared(); }
	StoreLockShared(const StoreLockShared&) = delete;
	StoreLockShared& operator =(const StoreLockShared&) = delete;
private:
	StoreLock* m_StoreLock;
};

struct StoreLockExclusive
{
	explicit StoreLockExclusive(StoreLock* storeLock) : m_StoreLock(storeLock) { if (m_StoreLock) m_StoreLock->LockExclusive(); }
	~StoreLockExclusive() { if (m_StoreLock) m_StoreLock->UnlockExclusive(); }
	StoreLockExclusive(const StoreLockExclusive&) = delete;
	StoreLockExclusive& operator =(const StoreLockExclusive&) = delete;
private:
	StoreLock* m_StoreLock;
};

// fileName + ".~" + kind + <process id> + "_" + nr: 'w' for a file being written, 'z' for a file moved aside
RTC_CALL SharedStr MakeStoreTmpName(WeakStr fileName, char kind, UInt64 nr);
RTC_CALL UInt64    NextStoreFileNr();

// moves the file replacementFileName to fileName, replacing what is there; retries a sharing violation
RTC_CALL void ReplaceFileBy(WeakStr fileName, WeakStr replacementFileName);

// removes, in the store folder and below, the temporary files that a write of another process left
RTC_CALL void RemoveStoreLeftovers(WeakStr storeFolder);

// The write of one stored array: Begin moves the files of the previous version aside and returns the name
// to write under, Publish gives the written files the array's name. An unpublished write removes its files.
struct StoreFilePublisher
{
	StoreFilePublisher() = default;
	RTC_CALL ~StoreFilePublisher();

	RTC_CALL SharedStr Begin(std::shared_ptr<StoreLock> storeLock, WeakStr finalName, bool hasSeqFile);
	RTC_CALL void Publish(MappedFileHandle& dataFile, MappedFileHandle* seqFile);

	bool IsActive() const { return bool(m_StoreLock); }

	StoreFilePublisher(const StoreFilePublisher&) = delete;
	StoreFilePublisher& operator =(const StoreFilePublisher&) = delete;

private:
	std::shared_ptr<StoreLock> m_StoreLock;
	SharedStr m_FinalName, m_PartialName;
	UInt64    m_WriteNr = 0;
	bool      m_HasSeqFile = false, m_Published = false;
};

//  -----------------------------------------------------------------------

#endif // __RTC_SER_FILEMAPHANDLE_H
