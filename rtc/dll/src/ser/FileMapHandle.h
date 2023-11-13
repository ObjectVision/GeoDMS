// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__RTC_SER_FILEMAPHANDLE_H)
#define __RTC_SER_FILEMAPHANDLE_H

#include "cpc/Types.h"
#include "ser/FileCreationMode.h"
struct SafeFileWriterArray;

// the following define should clean-up resources of Closed FileMap Handles by Closing the file
#define DMS_CLOSING_UNMAP

#define UNDEFINED_FILE_SIZE UNDEFINED_VALUE(dms::filesize_t)

//  -----------------------------------------------------------------------


struct FileHandle
{
	RTC_CALL FileHandle();
	RTC_CALL ~FileHandle();

	RTC_CALL void OpenRw (WeakStr fileName, SafeFileWriterArray* sfwa, dms::filesize_t requiredNrBytes, dms_rw_mode rwMode, bool isTmp, bool retry = true, bool deleteOnClose = false);
	RTC_CALL void OpenForRead(WeakStr fileName, SafeFileWriterArray* sfwa, bool throwOnError, bool doRetry, bool mayBeEmpty = false );
	RTC_CALL void CloseFile();
	RTC_CALL void DropFile (WeakStr fileName);

	bool IsOpen  () const { return m_hFile;    }

	dms::filesize_t GetFileSize() const { return m_FileSize; }
	void  SetFileSize(dms::filesize_t sz) { m_FileSize = sz; }

protected:
	void ReadFileSize(CharPtr handleName);

	HANDLE      m_hFile;
	dms::filesize_t  m_FileSize;

	bool m_IsTmp    : 1;

MG_DEBUG_DATA_CODE(
	FileCreationMode m_FCM : 3;
)

};
struct MappedFileHandle : FileHandle
{
	dms::filesize_t m_AllocatedSize;

	void OpenRw(WeakStr fileName, SafeFileWriterArray* sfwa, dms::filesize_t requiredNrBytes, dms_rw_mode rwMode, bool isTmp)
	{
		FileHandle::OpenRw(fileName, sfwa, requiredNrBytes, rwMode, isTmp);
		m_hFileMap = MapViewOfFile()
	}

	void OpenForRead(WeakStr fileName, SafeFileWriterArray* sfwa, bool throwOnError, bool doRetry)
	{
		assert(!IsMapped()); // only non-zero when file is open
		assert(m_ViewData == nullptr); // only non-zero when file is open
		FileHandle::OpenForRead(fileName, sfwa, throwOnError, doRetry);
	}

	HANDLE m_hFileMap;
};

struct ConstMappedFileHandle : FileHandle
{
	dms::filesize_t m_AllocatedSize;

	ConstMappedFileHandle(WeakStr fileName, SafeFileWriterArray* sfwa, bool throwOnError, bool doRetry)
	{
		OpenForRead(fileName, sfwa, throwOnError, doRetry);
		assert(IsOpen() || !throwOnError);
	}
};

struct FileMapHandle
{
	~FileMapHandle() { CloseFMH(); }

	RTC_CALL void realloc(FileHandle& storage, dms::filesize_t requiredNrBytes, WeakStr fileName, SafeFileWriterArray* sfwa);

//	bool IsMapped() const { return m_hFileMap; }
	bool IsUsable() const { return m_hFileMap; }

	RTC_CALL void CloseFMH();
	RTC_CALL void Drop (WeakStr fileName);
	RTC_CALL void Unmap();
	RTC_CALL void Map(dms_rw_mode rwMode, WeakStr fileName, SafeFileWriterArray* sfwa);

	char*   DataBegin()       { assert(IsUsable()); return reinterpret_cast<char*  >(m_ViewData); }
	char*   DataEnd  ()       { assert(IsUsable()); return reinterpret_cast<char*  >(m_ViewData) + GetViewSize(); }
	CharPtr DataBegin() const { assert(IsUsable()); return reinterpret_cast<CharPtr>(m_ViewData); }
	CharPtr DataEnd  () const { assert(IsUsable()); return reinterpret_cast<CharPtr>(m_ViewData) + GetViewSize(); }

	dms::filesize_t GetViewSize() const { return m_ViewSize;  }

private:
	void CloseView(bool drop);

	dms::filesize_t  m_ViewOffset = 0, m_ViewSize = 0;
	void*            m_ViewData = nullptr;
};

struct MappedConstFileMapHandle : FileMapHandle
{
	ConstMappedFileHandle(ConstMappedFileHandle& cmfh)
	{
		if (cmfh.IsOpen())
			Map(dms_rw_mode::read_only, fileName);
	}
	~ConstMappedFileHandle()
	{
		if (IsMapped())
			Unmap();
	}

	void CloseMCFMH()
	{
		Unmap();
		CloseFMH();
	}
};

//  -----------------------------------------------------------------------

#endif // __RTC_SER_FILEMAPHANDLE_H
