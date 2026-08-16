// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  Process/session environment: the global main-window handle, waiting and
 *  yielding, stack-space measurement, child-process launching, message-pump
 *  queries, platform identification (PlatformInfo) and UTF-8/wide-char
 *  conversion. The former registry, file-system, OS-error and time-format
 *  clusters moved to utl/Registry.h, utl/FileSystem.h, utl/PlatformError.h
 *  and utl/TimeFmt.h (header-hygiene-2026-08.md §5A); include those
 *  directly where used.
 */

#if !defined(__UTL_ENVIRONMENT_H)
#define __UTL_ENVIRONMENT_H

#include <atomic>
#include <memory>
#include <utility>

#include "cpc/Types.h"
#include "ptr/SharedStr.h"

//  -----------------------------------------------------------------------

extern "C" RTC_CALL void* SetGlobalMainWindowHandle(void* hWindow); // Delphi code also calls this
extern "C" RTC_CALL void* GetGlobalMainWindowHandle(); // Delphi code could also call this

//  -----------------------------------------------------------------------

using start_process_result_t = std::pair<HANDLE, HANDLE>;

RTC_CALL void   Wait(UInt32 nrMillisecs);
RTC_CALL void   DmsYield(UInt32 nrMillisecs = 50);
RTC_CALL SizeT  RemainingStackSpace();
RTC_CALL start_process_result_t StartChildProcess(CharPtr moduleName, Char* cmdLine = nullptr);
RTC_CALL DWORD  ExecuteChildProcess(CharPtr moduleName, Char* cmdLine);

extern "C" {

RTC_CALL bool   DMS_CONV HasWaitingMessages();
RTC_CALL void   DMS_CONV DMS_Appl_SetFont();

}	// extern "C"

namespace PlatformInfo
{
	RTC_CALL SharedStr GetVersionStr();
	RTC_CALL SharedStr GetUserNameA();
	RTC_CALL SharedStr GetComputerNameA();
	RTC_CALL bool      GetEnv(CharPtr varName, SharedStr& result);
	RTC_CALL bool      GetEnvString(CharPtr section, CharPtr key, SharedStr& result);
	RTC_CALL SharedStr GetProgramFiles32();
};

RTC_CALL extern std::atomic<UInt32> g_DispatchLockCount;

RTC_CALL std::unique_ptr<wchar_t[]> Utf8_2_wchar(CharPtr utf8str, int strLen = -1);
RTC_CALL std::unique_ptr<wchar_t[]> Utf8_2_wchar(WeakStr utf8str);
RTC_CALL auto wchar_2_Utf8Str(const wchar_t* wCharStr, int strLen = -1) -> SharedStr;

#endif // __UTL_ENVIRONMENT_H
