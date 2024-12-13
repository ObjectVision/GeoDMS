// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_SER_FILEMAPHANDLE_H)
#define __RTC_SER_FILEMAPHANDLE_H

#include "cpc/Types.h"
#include "geo/IndexRange.h"
#include "ser/FileCreationMode.h"

// the following define should clean-up resources of Closed FileMap Handles by Closing the file
#define DMS_CLOSING_UNMAP

#define UNDEFINED_FILE_SIZE UNDEFINED_VALUE(dms::filesize_t)

//----------------------------------------------------------------------

RTC_CALL SizeT GetAllocationGrannularity();
RTC_CALL UInt8 GetLog2AllocationGrannularity();

inline SizeT NrMemPages(SizeT nrBytes)
{
	auto LOG_MEM_PAGE_SIZE = GetLog2AllocationGrannularity();
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

	RTC_CALL ~WinHandle(); // CloseHandle(m_Hnd)

	operator HANDLE() const { return m_Hnd; }

private:
	HANDLE m_Hnd = nullptr;
};

//  -----------------------------------------------------------------------

struct FileChunckSpec {
	dms::filesize_t offset, size, capacity;
};

struct FileHandle
{
	FileHandle() = default;

	RTC_CALL ~FileHandle();

	RTC_CALL void OpenRw (WeakStr fileName, dms::filesize_t requiredNrBytes, dms_rw_mode rwMode, bool isTmp, bool retry = true, bool deleteOnClose = false);
	RTC_CALL void OpenForRead(WeakStr fileName, bool throwOnError, bool doRetry, bool mayBeEmpty = false );
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

struct mempage_file_view;

struct MappedFileHandle : FileHandle
{
	RTC_CALL MappedFileHandle();
	RTC_CALL ~MappedFileHandle();

	RTC_CALL void OpenRw(WeakStr fileName, dms::filesize_t requiredNrBytes, dms_rw_mode rwMode, bool isTmp);
	RTC_CALL void OpenForRead(WeakStr fileName, bool throwOnError, bool doRetry);

	RTC_CALL FileChunckSpec allocAtEnd(dms::filesize_t viewSize, dms::filesize_t viewCapacity);

private:
	void MapFile(bool alsoWrite);

public:
	WinHandle m_hFileMapping;
	std::shared_mutex m_ResizeMutex;

	std::unique_ptr< mempage_file_view > m_MemPageAllocTable;
	dms::filesize_t m_AllocatedSize = 0;

	MappedFileHandle(const MappedFileHandle&&) = delete;
	MappedFileHandle(MappedFileHandle&&) = delete;
	MappedFileHandle& operator =(const MappedFileHandle&) = delete;
	MappedFileHandle& operator =(MappedFileHandle&&) = delete;
};


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
	RTC_CALL ViewData(MappedFileHandle* mappedFile, DWORD desiredAccess, dms::filesize_t viewOffset, dms::filesize_t viewCapacity);
	RTC_CALL ~ViewData();

	ViewData() {}
	ViewData(ViewData&& rhs) noexcept
	{
		operator =(std::move(rhs));
	}
	void operator=(ViewData&& rhs) noexcept
	{
		ptr_base<void, movable >::swap(rhs);
	}
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

	RTC_CALL FileViewHandle(std::shared_ptr<mapped_file_type> mfh, dms::filesize_t viewOffset, dms::filesize_t viewSize, dms::filesize_t viewCapacity);
	RTC_CALL void operator = (FileViewHandle&& rhs) noexcept;

	RTC_CALL void realloc(dms::filesize_t capacity);

	bool IsUsable() const { return m_ViewData != nullptr || GetViewCapacity() == 0; }

	RTC_CALL void MapView(bool alsoWrite);
	void UnmapView() { m_ViewData = ViewData(); }

	char*   DataBegin()       { assert(IsUsable()); return reinterpret_cast<char*  >(m_ViewData.get_ptr()); }
	char*   DataEnd  ()       { return DataBegin() + GetViewSize(); }
	CharPtr DataBegin() const { assert(IsUsable()); return reinterpret_cast<CharPtr>(m_ViewData.get_ptr()); }
	CharPtr DataEnd() const { return DataBegin() + GetViewSize(); }

	auto            GetViewSpec    () const { return m_ViewSpec; }
	dms::filesize_t GetViewOffset  () const { return m_ViewSpec.offset; }
	dms::filesize_t GetViewSize    () const { return m_ViewSpec.size; }
	dms::filesize_t GetViewCapacity() const { return m_ViewSpec.capacity; }

//	auto GetFileName() const -> SharedStr { return m_MappedFile->GetFileName(); }
	auto GetMappedFile() const { return m_MappedFile; }

	std::shared_ptr<mapped_file_type> m_MappedFile;

	FileChunckSpec m_ViewSpec = { 0, 0, 0 };
	ViewData       m_ViewData;
	bool           m_AlsoWrite = false;
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

	bool IsUsable() const { return m_ViewData != nullptr || GetViewCapacity() == 0; }

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

	FileChunckSpec m_ViewSpec = { 0, 0, 0 };
	ViewData m_ViewData;
};

//  -----------------------------------------------------------------------

#endif // __RTC_SER_FILEMAPHANDLE_H
