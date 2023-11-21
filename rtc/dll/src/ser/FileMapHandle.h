// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_SER_FILEMAPHANDLE_H)
#define __RTC_SER_FILEMAPHANDLE_H

#include "cpc/Types.h"
#include "ser/FileCreationMode.h"
struct SafeFileWriterArray;

// the following define should clean-up resources of Closed FileMap Handles by Closing the file
#define DMS_CLOSING_UNMAP

#define UNDEFINED_FILE_SIZE UNDEFINED_VALUE(dms::filesize_t)

//----------------------------------------------------------------------

RTC_CALL UInt32 GetAllocationGrannularity();
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

	WinHandle(WinHandle&& rhs)
	{
		operator =(std::move(rhs));
	}
	void operator = (WinHandle&& rhs)
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

using FileChunckSpec = std::pair<dms::filesize_t, dms::filesize_t>;

struct FileHandle
{
	FileHandle() = default;

	RTC_CALL ~FileHandle();

	RTC_CALL void OpenRw (WeakStr fileName, SafeFileWriterArray* sfwa, dms::filesize_t requiredNrBytes, dms_rw_mode rwMode, bool isTmp, bool retry = true, bool deleteOnClose = false);
	RTC_CALL void OpenForRead(WeakStr fileName, SafeFileWriterArray* sfwa, bool throwOnError, bool doRetry, bool mayBeEmpty = false );
	RTC_CALL void CloseFile();
	RTC_CALL void DropFile (WeakStr fileName);

	bool IsOpen  () const { return m_hFile; }

	dms::filesize_t GetFileSize() const { return m_FileSize; }
	void  SetFileSize(dms::filesize_t sz) { m_FileSize = sz; }

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

	RTC_CALL void OpenRw(WeakStr fileName, SafeFileWriterArray* sfwa, dms::filesize_t requiredNrBytes, dms_rw_mode rwMode, bool isTmp);
	RTC_CALL void OpenForRead(WeakStr fileName, SafeFileWriterArray* sfwa, bool throwOnError, bool doRetry);

	RTC_CALL FileChunckSpec alloc(dms::filesize_t vs);

private:
	void Map(bool alsoWrite);

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
	ConstMappedFileHandle(WeakStr fileName, SafeFileWriterArray* sfwa = nullptr, bool throwOnError = true, bool doRetry = false)
	{
		OpenForRead(fileName, sfwa, throwOnError, doRetry);
		assert(IsOpen() || !throwOnError);
	}
};

struct FileViewHandle
{
	using mapped_file_type = MappedFileHandle;

	FileViewHandle() = default;
	FileViewHandle(FileViewHandle&& rhs) { operator =(std::move(rhs)); }
	RTC_CALL FileViewHandle(std::shared_ptr<mapped_file_type> mfh, dms::filesize_t viewOffset = 0, dms::filesize_t viewSize = -1);


	~FileViewHandle() { CloseView(); }
	RTC_CALL void operator = (FileViewHandle&& rhs);

	RTC_CALL void realloc(dms::filesize_t requiredNrBytes);

//	bool IsMapped() const { return m_hFileMap; }
	bool IsUsable() const { return m_ViewData != nullptr || GetViewSize() == 0; }

//	RTC_CALL void CloseFMH();
//	RTC_CALL void Drop (WeakStr fileName);
	RTC_CALL void Map(bool alsoWrite);
	void Unmap() { CloseView(); }
//	RTC_CALL void Map(dms_rw_mode rwMode, WeakStr fileName, SafeFileWriterArray* sfwa);

	char*   DataBegin()       { assert(IsUsable()); return reinterpret_cast<char*  >(m_ViewData); }
	char*   DataEnd  ()       { return DataBegin() + GetViewSize(); }
	CharPtr DataBegin() const { assert(IsUsable()); return reinterpret_cast<CharPtr>(m_ViewData); }
	CharPtr DataEnd() const { return DataBegin() + GetViewSize(); }

	dms::filesize_t GetViewOffset() const { return m_ViewSpec.first; }
	dms::filesize_t GetViewSize() const { return m_ViewSpec.second; }

//	auto GetFileName() const -> SharedStr { return m_MappedFile->GetFileName(); }
	auto GetMappedFile() const { return m_MappedFile; }

protected:
	std::shared_ptr< mapped_file_type> m_MappedFile;

	RTC_CALL void CloseView();

	FileChunckSpec m_ViewSpec = { 0, 0 };
	void*          m_ViewData = nullptr;
};

struct ConstFileViewHandle
{
	using mapped_file_type = ConstMappedFileHandle;

	ConstFileViewHandle() = default;
	ConstFileViewHandle(ConstFileViewHandle&& rhs) { operator =(std::move(rhs)); }
	RTC_CALL ConstFileViewHandle(std::shared_ptr<ConstMappedFileHandle> cmfh, dms::filesize_t viewOffset = 0, dms::filesize_t viewSize = -1);

	~ConstFileViewHandle() { CloseView(); }
	RTC_CALL void operator = (ConstFileViewHandle&& rhs);

	//	bool IsMapped() const { return m_hFileMap; }
	bool IsUsable() const { return m_ViewData != nullptr || GetViewSize() == 0; }

	//	RTC_CALL void CloseFMH();
	//	RTC_CALL void Drop (WeakStr fileName);
	RTC_CALL void Map();
	void Unmap() { CloseView(); }
	//	RTC_CALL void Map(dms_rw_mode rwMode, WeakStr fileName, SafeFileWriterArray* sfwa);

	CharPtr DataBegin() const { assert(IsUsable()); return reinterpret_cast<CharPtr>(m_ViewData); }
	CharPtr DataEnd() const { return DataBegin() + GetViewSize(); }

	dms::filesize_t GetViewOffset() const { return m_ViewSpec.first; }
	dms::filesize_t GetViewSize() const { return m_ViewSpec.second; }

	//	auto GetFileName() const -> SharedStr { return m_MappedFile->GetFileName(); }
	auto GetMappedFile() const { return m_MappedFile; }

protected:
	std::shared_ptr< mapped_file_type> m_MappedFile;

	RTC_CALL void CloseView();

	FileChunckSpec m_ViewSpec = { 0, 0 };
	void* m_ViewData = nullptr;
};

//  -----------------------------------------------------------------------

#endif // __RTC_SER_FILEMAPHANDLE_H
