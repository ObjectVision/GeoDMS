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

	~WinHandle(); // CloseHandle(m_Hnd)

	operator HANDLE() const { return m_Hnd; }

private:
	HANDLE m_Hnd = nullptr;
};

//  -----------------------------------------------------------------------


struct FileHandle
{
	RTC_CALL FileHandle();
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

	WinHandle        m_hFile;
	dms::filesize_t  m_FileSize;
	SharedStr        m_FileName;

	bool m_IsTmp    : 1;

MG_DEBUG_DATA_CODE(
	FileCreationMode m_FCM : 3;
)

};
struct MappedFileHandle : FileHandle
{
	dms::filesize_t m_AllocatedSize;

	void OpenRw(WeakStr fileName, SafeFileWriterArray* sfwa, dms::filesize_t requiredNrBytes, dms_rw_mode rwMode, bool isTmp);
	void OpenForRead(WeakStr fileName, SafeFileWriterArray* sfwa, bool throwOnError, bool doRetry);

	WinHandle m_hFileMapping;
};


struct ConstMappedFileHandle : MappedFileHandle
{
	dms::filesize_t m_AllocatedSize;

	ConstMappedFileHandle(WeakStr fileName, SafeFileWriterArray* sfwa = nullptr, bool throwOnError = true, bool doRetry = false)
	{
		OpenForRead(fileName, sfwa, throwOnError, doRetry);
		assert(IsOpen() || !throwOnError);
	}
};

struct FileViewHandle
{
	FileViewHandle() = default;
	FileViewHandle(std::shared_ptr<MappedFileHandle> mfh, dms::filesize_t viewSize);
	FileViewHandle(std::shared_ptr<ConstMappedFileHandle> cmfh, dms::filesize_t viewOffset = 0, dms::filesize_t viewSize = -1);
	FileViewHandle(FileViewHandle&& rhs) { operator =(std::move(rhs)); }


	~FileViewHandle() { CloseView(); }
	void operator = (FileViewHandle&& rhs);

//	RTC_CALL void realloc(FileHandle& storage, dms::filesize_t requiredNrBytes, WeakStr fileName, SafeFileWriterArray* sfwa);

//	bool IsMapped() const { return m_hFileMap; }
	bool IsUsable() const { return m_ViewData != nullptr || m_ViewSize == 0; }

//	RTC_CALL void CloseFMH();
//	RTC_CALL void Drop (WeakStr fileName);
	RTC_CALL void Map(bool alsoWrite);
	void Unmap() { CloseView(); }
//	RTC_CALL void Map(dms_rw_mode rwMode, WeakStr fileName, SafeFileWriterArray* sfwa);

	char*   DataBegin()       { assert(IsUsable()); return reinterpret_cast<char*  >(m_ViewData); }
	char*   DataEnd  ()       { assert(IsUsable()); return reinterpret_cast<char*  >(m_ViewData) + GetViewSize(); }
	CharPtr DataBegin() const { assert(IsUsable()); return reinterpret_cast<CharPtr>(m_ViewData); }
	CharPtr DataEnd  () const { assert(IsUsable()); return reinterpret_cast<CharPtr>(m_ViewData) + GetViewSize(); }

	dms::filesize_t GetViewSize() const { return m_ViewSize;  }

//	auto GetFileName() const -> SharedStr { return m_MappedFile->GetFileName(); }
	auto GetMappedFile() const { return m_MappedFile; }

protected:
	std::shared_ptr< MappedFileHandle> m_MappedFile;

	void CloseView();

	dms::filesize_t  m_ViewOffset = 0, m_ViewSize = 0;
	void*            m_ViewData = nullptr;
};

/*
struct ConstFileViewHandle
{
	ConstFileViewHandle(ConstMappedFileHandle& cmfh, dms::filesize_t viewOffset, dms::filesize_t viewSize);
	~ConstFileViewHandle()
	{
		CloseView();
	}

protected:
	void CloseView();

	dms::filesize_t  m_ViewOffset = 0, m_ViewSize = 0;
	void* m_ViewData = nullptr;
};
*/

//  -----------------------------------------------------------------------

#endif // __RTC_SER_FILEMAPHANDLE_H
