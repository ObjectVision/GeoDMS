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
};

struct FileMapHandle
{
	RTC_CALL FileMapHandle();
	RTC_CALL ~FileMapHandle();
/*
	void OpenRw(WeakStr fileName, SafeFileWriterArray* sfwa, dms::filesize_t requiredNrBytes, dms_rw_mode rwMode, bool isTmp)
	{
		dms_assert(!IsMapped());
		dms_assert(m_ViewData == 0); // only non-zero when file is open
		FileHandle::OpenRw(fileName, sfwa, requiredNrBytes, rwMode, isTmp);
	}

	void OpenForRead(WeakStr fileName, SafeFileWriterArray* sfwa, bool throwOnError, bool doRetry )
	{
		dms_assert(!IsMapped()); // only non-zero when file is open
		dms_assert(m_ViewData == nullptr); // only non-zero when file is open
		FileHandle::OpenForRead(fileName, sfwa, throwOnError, doRetry);
	}
*/

	RTC_CALL void realloc(FileHandle& storage, dms::filesize_t requiredNrBytes, WeakStr fileName, SafeFileWriterArray* sfwa);

	bool IsMapped() const { return m_hFileMap; }
//	bool IsUsable() const { return IsMapped() || (GetFileSize() == 0); }

	RTC_CALL void CloseFMH();
	RTC_CALL void Drop (WeakStr fileName);
	RTC_CALL void Unmap();
	RTC_CALL void Map(dms_rw_mode rwMode, WeakStr fileName, SafeFileWriterArray* sfwa);

	char*   DataBegin()       { assert(IsMapped()); return reinterpret_cast<char*  >(m_ViewData); }
	char*   DataEnd  ()       { assert(IsMapped()); return reinterpret_cast<char*  >(m_ViewData) + GetViewSize(); }
	CharPtr DataBegin() const { assert(IsMapped()); return reinterpret_cast<CharPtr>(m_ViewData); }
	CharPtr DataEnd  () const { assert(IsMapped()); return reinterpret_cast<CharPtr>(m_ViewData) + GetViewSize(); }

	dms::filesize_t GetViewSize() { return m_ViewSize;  }

private:
	void CloseView(bool drop);

	HANDLE           m_hFileMap;
	dms::filesize_t  m_ViewOffset = 0, m_ViewSize = 0;
	void*            m_ViewData = nullptr;
};

struct MappedConstFileMapHandle : FileMapHandle
{
	MappedConstFileMapHandle(WeakStr fileName, SafeFileWriterArray* sfwa, bool throwOnError, bool doRetry)
	{
		OpenForRead(fileName, sfwa, throwOnError, doRetry);
		dms_assert(IsOpen() || !throwOnError);
		if (IsOpen())
			Map(dms_rw_mode::read_only, fileName, sfwa);
	}
	~MappedConstFileMapHandle()
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
