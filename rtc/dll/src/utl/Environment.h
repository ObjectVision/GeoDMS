// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_UTL_ENVIRONMENT_H)
#define __RTC_UTL_ENVIRONMENT_H

#include "cpc/Types.h"
#include "geo/iterrange.h"
#include "ptr/OwningPtrArray.h"

//  -----------------------------------------------------------------------
namespace platform {
	RTC_CALL SharedStr GetSystemErrorText(DWORD lastErr);
	RTC_CALL DWORD GetLastError();
	RTC_CALL bool isCharPtrAndExceeds_MAX_PATH(CharPtr xFileName);
}

template<typename T>
bool isCharPtrAndExceeds_MAX_PATH(const T& xFileName) { return false;  }

template<typename ...Args>
[[noreturn]] void throwSystemError(DWORD lastErr, CharPtr format, Args&&... args)
{
	throwErrorF("WindowsSystem", "%s:\nErrorCode %d: %s%s"
	,	mgFormat2string<Args...>(format, std::forward<Args>(args)...).c_str()
	,	lastErr
	,	::platform::GetSystemErrorText(lastErr).c_str()
	,	(... || isCharPtrAndExceeds_MAX_PATH(args)) ? "\nNote that filenames cannot be longer than 260 characters" : ""
	);
}

template<typename ...Args>
[[noreturn]] void throwLastSystemError(CharPtr format, Args&&... args) {
	throwSystemError<Args...>(::platform::GetLastError(), format, std::forward<Args>(args)...);
}

bool ManageSystemError(UInt32& retryCounter, CharPtr format, CharPtr fileName, bool throwOnError, bool doRetry);

extern "C" RTC_CALL void* SetGlobalMainWindowHandle(void* hWindow); // Delphi code also calls this
extern "C" RTC_CALL void* GetGlobalMainWindowHandle(); // Delphi code could also call this

//  -----------------------------------------------------------------------

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
RTC_CALL SharedStr GetConvertedGeoDmsRegKey(CharPtr key);
RTC_CALL SharedStr ConvertDmsFileName(WeakStr path);
RTC_CALL SharedStr ConvertDmsFileNameAlways(SharedStr&& path); // for updated WinAPI funcs
RTC_CALL void ReplaceSpecificDelimiters(MutableCharPtrRange range, const char delimiter);

enum RegStatusFlags
{
	// flags only used in GeoDmsClient.exe
	RSF_AdminMode = 1,
	RSF_SuspendForGUI = 2,
	RSF_ShowStateColors = 4,
	RSF_TraceLogFile = 8,

	RSF_TreeViewVisible = 16,
	RSF_DetailsVisible = 32,
	RSF_EventLogVisible = 64,
	RSF_ToolBarVisible = 128,
	RSF_CurrentItemBarHidden = 0x800,
	RSF_AllPanelsVisible = RSF_TreeViewVisible + RSF_DetailsVisible + RSF_EventLogVisible + RSF_ToolBarVisible,

	RSF_DynamicROI = 0x1000,

	//  Flags really in use by the GeoDMS C++ Engine
	RSF_MultiThreading0 = RSF_SuspendForGUI,
	RSF_MultiThreading1 = 0x100,
	//	RSF_MultiThreading2 = 0x200,
	RSF_MultiThreading2 = 0x400,
	RSF_MultiThreading3 = 0x2000,
	RSF_AllMultiThreading = RSF_SuspendForGUI | RSF_MultiThreading1 | RSF_MultiThreading2 | RSF_MultiThreading3,

	RSF_ShowThousandSeparator = 0x4000,

	RSF_EventLog_ShowDateTime = 0x8000,
	RSF_EventLog_ShowThreadID = 0x10000,
	RSF_EventLog_ShowCategory = 0x20000,
	RSF_EventLog_ShowAnyExtra = RSF_EventLog_ShowDateTime | RSF_EventLog_ShowThreadID | RSF_EventLog_ShowCategory,

	RSF_EventLog_ClearOnLoad = 0x40000,
	RSF_EventLog_ClearOnReLoad = 0x80000,
	
	RSF_TreeView_FollowOSLayout = 0x100000,
	
	RSF_WasRead = 0x80000000,
	RSF_Default = RSF_AdminMode | RSF_ShowStateColors | RSF_AllPanelsVisible | RSF_AllMultiThreading
		| RSF_EventLog_ClearOnLoad | RSF_EventLog_ShowDateTime | RSF_EventLog_ShowCategory,
};

RTC_CALL UInt32 GetRegStatusFlags();
RTC_CALL void SetCachedStatusFlag(UInt32 newSF, bool newVal = true);
RTC_CALL bool HasDynamicROI();
RTC_CALL bool ShowThousandSeparator();

//  -----------------------------------------------------------------------

enum class RegDWordEnum
{
	MemoryFlushThreshold = 0,
	SwapFileMinSize = 1,
	DrawingSizeInPixels = 2
};

extern "C" RTC_CALL DWORD DMS_CONV RTC_GetRegDWord(RegDWordEnum i);
extern "C" RTC_CALL void  DMS_CONV RTC_SetCachedDWord(RegDWordEnum i, DWORD dw);
extern "C" RTC_CALL bool  DMS_CONV RTC_ParseRegStatusFlag(CharPtr param);

RTC_CALL void ParseRegStatusFlags(int& argc, char**& argv);

RTC_CALL SharedStr GetGeoDmsRegKey(CharPtr key);
RTC_CALL std::vector<std::string> GetGeoDmsRegKeyMultiString(CharPtr key);

RTC_CALL DWORD GetGeoDmsRegKeyDWord(CharPtr key, DWORD defaultValue, CharPtr section = "");
RTC_CALL bool SetGeoDmsRegKeyDWord(CharPtr key, DWORD dw, CharPtr section = "");
RTC_CALL bool SetGeoDmsRegKeyString(CharPtr key, std::string str);
RTC_CALL bool SetGeoDmsRegKeyMultiString(CharPtr key, std::vector<std::string> strings);

//  -----------------------------------------------------------------------

struct FindFileBlock
{
	RTC_CALL FindFileBlock(WeakStr fileSearchSpec);
	RTC_CALL FindFileBlock(FindFileBlock&& src) noexcept;
	RTC_CALL ~FindFileBlock() noexcept;

	RTC_CALL bool    IsValid() const;
	RTC_CALL CharPtr GetCurrFileName() const;
	RTC_CALL DWORD   GetFileAttr() const;
	RTC_CALL bool    IsDirectory() const;
	RTC_CALL FileDateTime GetFileOrDirDateTime() const;

	RTC_CALL bool    Next();

private:
	std::unique_ptr<Byte[]> m_Data;
	HANDLE                  m_Handle;
};

using start_process_result_t = std::pair<HANDLE, HANDLE>;

RTC_CALL SharedStr AsDateTimeString(FileDateTime t);
RTC_CALL SharedStr GetCurrentTimeStr();
RTC_CALL SharedStr GetSessionStartTimeStr();

//  -----------------------------------------------------------------------

RTC_CALL FileDateTime AsFileDateTime(UInt32 hiDW, UInt32 loDW);

//  -----------------------------------------------------------------------

RTC_CALL void   MakeDir(WeakStr dirName);
RTC_CALL void   CopyFileOrDir(CharPtr srcFileOrDirName, CharPtr destFileOrDirName, bool mayBeMissing);
RTC_CALL bool   MoveFileOrDir(CharPtr srcFileOrDirName, CharPtr destFileOrDirName, bool mayBeMissing);
RTC_CALL bool   KillFileOrDir(WeakStr fileOrDirName, bool canBeDir = true);
RTC_CALL void   Wait(UInt32 nrMillisecs);
RTC_CALL void   DmsYield(UInt32 nrMillisecs = 50);
RTC_CALL SizeT  RemainingStackSpace();
RTC_CALL bool   IsFileOrDirAccessible(WeakStr fileOrDirName);
RTC_CALL bool   IsFileOrDirWritable(WeakStr fileOrDirName);
RTC_CALL void   GetWritePermission(WeakStr fileName);
RTC_CALL FileDateTime GetFileOrDirDateTime(WeakStr fileOrDirName);
RTC_CALL auto   GetFileOrDirDateTimeAsReadableString(WeakStr fileOrDirName) -> SharedStr;
RTC_CALL void   MakeDirsForFile(WeakStr fileName);
RTC_CALL start_process_result_t StartChildProcess(CharPtr moduleName, Char* cmdLine = nullptr);
RTC_CALL DWORD  ExecuteChildProcess(CharPtr moduleName, Char* cmdLine);
RTC_CALL bool   HasDosDelimiters(CharPtr source);
RTC_CALL bool   HasDosDelimiters(CharPtrRange source);
RTC_CALL bool   IsRelative(CharPtr source);

extern "C" {

RTC_CALL bool   DMS_CONV HasWaitingMessages();

RTC_CALL void   DMS_CONV SetCurrentDir(CharPtr dir);
RTC_CALL void   DMS_CONV DMS_Appl_SetExeDir(CharPtr exeDir);
RTC_CALL void   DMS_CONV DMS_Appl_SetFont();
RTC_CALL void   DMS_CONV DMS_Appl_SetRegStatusFlags(UInt32 sf);
RTC_CALL UInt32 DMS_CONV DMS_Appl_GetRegStatusFlags();
}	// extern "C"

RTC_CALL Int32     GetConfigKeyValue (WeakStr configFileName, CharPtr sectionName, CharPtr keyName, Int32   defaultValue);
RTC_CALL SharedStr GetConfigKeyString(WeakStr configFileName, CharPtr sectionName, CharPtr keyName, CharPtr defaultValue);
RTC_CALL void      SetConfigKeyString(WeakStr configFileName, CharPtr sectionName, CharPtr keyName, CharPtr keyValue);
RTC_CALL Int64     GetSecsSince1970();

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

#endif // __RTC_UTL_ENVIRONMENT_H
