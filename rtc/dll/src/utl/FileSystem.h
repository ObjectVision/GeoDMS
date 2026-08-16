// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  File-system services: the GeoDMS directory accessors (CurrentDir,
 *  ExeDir, LocalDataDir, SourceDataDir), dms<->dos path conversion,
 *  file/directory operations (make, copy, move, kill, accessibility and
 *  date-time queries), directory iteration (FindFileBlock), and .ini-style
 *  config-file access. Split out of utl/Environment.h
 *  (header-hygiene-2026-08.md §5A).
 */

#if !defined(__UTL_FILESYSTEM_H)
#define __UTL_FILESYSTEM_H

#include <memory>

#include "cpc/Types.h"
#include "vt/iterrange.h"
#include "ptr/SharedStr.h"

// GetCurrentDir()
//
// CurrDir contains current main configuration file; does NOT end with '/'
// main configuration directory (is a sub dir from Current Dir) contains config.ini
// data file location can be defined in config.ix (with an absolute or relative path)

RTC_CALL SharedStr GetCurrentDir();

// ExeDir contains DmsClient.exe (+dlls?) and dms.ini; does NOT end with '/'

RTC_CALL SharedStr GetExeDir();
RTC_CALL SharedStr GetLocalDataDir();
RTC_CALL SharedStr GetSourceDataDir();
RTC_CALL SharedStr ConvertDosFileName(WeakStr fileName);
RTC_CALL SharedStr ConvertDmsFileName(WeakStr path);
RTC_CALL SharedStr ConvertDmsFileNameAlways(SharedStr&& path); // for updated WinAPI funcs
RTC_CALL void ReplaceSpecificDelimiters(MutableCharPtrRange range, const char delimiter);

//  -----------------------------------------------------------------------

struct FindFileBlock
{
	RTC_CALL FindFileBlock(WeakStr fileSearchSpec);
	RTC_CALL FindFileBlock(FindFileBlock&& src) noexcept;
	RTC_CALL ~FindFileBlock() noexcept;

	RTC_CALL bool    IsValid() const;
	RTC_CALL CharPtr GetCurrFileName() const;          // UTF-8 (transcoded from WIN32_FIND_DATAW::cFileName)
	RTC_CALL DWORD   GetFileAttr() const;
	RTC_CALL bool    IsDirectory() const;
	RTC_CALL FileDateTime GetFileOrDirDateTime() const;

	RTC_CALL bool    Next();

private:
	// m_Data carries a WIN32_FIND_DATAW (wide-char filenames). m_CurrFileNameUtf8
	// is refreshed from cFileName whenever the iterator advances, so that
	// GetCurrFileName() can hand back a stable UTF-8 string pointer to callers.
	std::unique_ptr<Byte[]> m_Data;
	HANDLE                  m_Handle;
	mutable SharedStr       m_CurrFileNameUtf8;
};

//  -----------------------------------------------------------------------

RTC_CALL void   MakeDir(WeakStr dirName);
RTC_CALL void   CopyFileOrDir(CharPtr srcFileOrDirName, CharPtr destFileOrDirName, bool mayBeMissing);
RTC_CALL bool   MoveFileOrDir(CharPtr srcFileOrDirName, CharPtr destFileOrDirName, bool mayBeMissing);
RTC_CALL bool   KillFileOrDir(WeakStr fileOrDirName, bool canBeDir = true);
RTC_CALL bool   IsFileOrDirAccessible(WeakStr fileOrDirName);
RTC_CALL bool   IsFileOrDirWritable(WeakStr fileOrDirName);
RTC_CALL void   GetWritePermission(WeakStr fileName);
RTC_CALL FileDateTime GetFileOrDirDateTime(WeakStr fileOrDirName);
RTC_CALL auto   GetFileOrDirDateTimeAsReadableString(WeakStr fileOrDirName) -> SharedStr;
RTC_CALL void   MakeDirsForFile(WeakStr fileName);
RTC_CALL bool   HasDosDelimiters(CharPtr source);
RTC_CALL bool   HasDosDelimiters(CharPtrRange source);
RTC_CALL bool   IsRelative(CharPtr source);

extern "C" RTC_CALL void DMS_CONV SetCurrentDir(CharPtr dir);

RTC_CALL Int32     GetConfigKeyValue (WeakStr configFileName, CharPtr sectionName, CharPtr keyName, Int32   defaultValue);
RTC_CALL SharedStr GetConfigKeyString(WeakStr configFileName, CharPtr sectionName, CharPtr keyName, CharPtr defaultValue);
RTC_CALL void      SetConfigKeyString(WeakStr configFileName, CharPtr sectionName, CharPtr keyName, CharPtr keyValue);

#endif // __UTL_FILESYSTEM_H
