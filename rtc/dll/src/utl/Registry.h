// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  GeoDMS registry access: the low-level RegistryHandle wrappers over an
 *  HKEY, and the GeoDMS settings stored through them — the RegStatusFlags
 *  bit set with its cached accessors, the RegDWordEnum settings (memory
 *  thresholds, performance logging, resource-aware scheduling), the
 *  GeoDmsRegKey string accessors and their session-local overrides. The
 *  settings block moved here from utl/Environment.h
 *  (header-hygiene-2026-08.md §5A).
 */

#if !defined(__UTL_REGISTRY_H)
#define __UTL_REGISTRY_H

#include <vector>

#include "cpc/Types.h"
#include "ptr/OwningPtr.h"
#include "ptr/SharedStr.h"

WINDECL_HANDLE(HKEY);
typedef unsigned char BYTE;

enum class RegDataType { Unknown, String, ExpandString, Binary, DWORD, DWORDBigEndian, LINK, MultiString};

struct RegistryHandle
{
public:
	RTC_CALL bool        ValueExists(CharPtr name) const;
	RTC_CALL UInt32      GetDataSize(CharPtr name) const;
	RTC_CALL RegDataType GetDataType(CharPtr name) const;
	RTC_CALL UInt32      GetDataW(CharPtr name, BYTE* buffer, DWORD bufSize, RegDataType& regDataType)  const;
//	RTC_CALL UInt32      GetData(CharPtr name, std::vector<BYTE>& buffer, DWORD bufSize, RegDataType& regDataType) const;
	RTC_CALL SharedStr   ReadString(CharPtr name) const;
	RTC_CALL void        WriteString(CharPtr name, CharPtrRange str) const;
	RTC_CALL void        DeleteValue(CharPtr name) const;
	RTC_CALL auto        ReadMultiString(CharPtr name) const->std::vector<SharedStr>;
	RTC_CALL bool        WriteMultiString(CharPtr name, const std::vector<SharedStr>& strings) const;
	RTC_CALL DWORD       ReadDWORD (CharPtr name) const;
	RTC_CALL bool        WriteDWORD(CharPtr name, DWORD dw) const;

protected:
	RegistryHandle(HKEY key);
	RTC_CALL ~RegistryHandle();

private:
	HKEY m_Key;
};


//  -----------------------------------------------------------------------

struct RegistryHandleCurrentUserRO : RegistryHandle
{
	RegistryHandleCurrentUserRO();
};

struct RegistryHandleLocalMachineRO : RegistryHandle
{
	RTC_CALL RegistryHandleLocalMachineRO(CharPtr section = "");
};

struct RegistryHandleLocalMachineRW : RegistryHandle
{
	RTC_CALL RegistryHandleLocalMachineRW(CharPtr section = "");
};

//  -----------------------------------------------------------------------
//  GeoDMS settings stored in the registry (moved from utl/Environment.h)
//  -----------------------------------------------------------------------

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
	RSF_DebugMode       = 0x200,
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

	RSF_TreeView_FollowOSLayout  = 0x100000,
	RSF_EventLog_ShowMinorTrace  = 0x200000,
	RSF_EventLog_HideMajorTrace  = 0x400000,
	RSF_EventLog_HideDepreciated = 0x800000,
	RSF_EventLog_HideWarning     = 0x1000000,
	RSF_EventLog_HideError       = 0x2000000,
	RSF_EventLog_HideStorageRead = 0x4000000,
	RSF_EventLog_HideStorageWrite= 0x8000000,
//	RSF_EventLog_ShowConnection  = 0x10000000, out of bits, forget it
//	RSF_EventLog_ShowRequest     = 0x20000000,out of bits, forget it
	RSF_EventLog_HideCommands    = 0x10000000,
	RSF_EventLog_ShowMemory      = 0x20000000,
	RSF_EventLog_HideOther       = 0x40000000,

	RSF_WasRead = 0x80000000,
	RSF_Default = RSF_AdminMode | RSF_ShowStateColors | RSF_AllPanelsVisible | RSF_AllMultiThreading
		| RSF_EventLog_ClearOnLoad | RSF_EventLog_ShowDateTime | RSF_EventLog_ShowCategory,
};

RTC_CALL UInt32 GetRegStatusFlags();
RTC_CALL void SetCachedStatusFlag(UInt32 newSF, bool newVal = true);
RTC_CALL void SetRegStatusFlags(UInt32 newSF);
RTC_CALL void SetStatusFlag(UInt32 newSF, bool newVal);
RTC_CALL bool HasDynamicROI();
RTC_CALL bool ShowThousandSeparator();
RTC_CALL bool EventLog_HideDepreciatedCaseMixupWarnings();
RTC_CALL bool IsInDebugMode();

//  -----------------------------------------------------------------------

enum class RegDWordEnum
{
	MemoryFlushThreshold = 0,
	SwapFileMinSize = 1,
	DrawingSizeInPixels = 2,
	MemoryRAM_MAX_GB = 3,
	PerformanceLogging = 4,
	ResourceAwareScheduling = 5, // 0 = off, 1 = shadow (log what would be refused), 2 = enforce
	SchedulerBudgetMB = 6,       // 0 = derive from MemoryRAM_MAX_GB x MemoryFlushThreshold
	MemoryDrainage = 7,          // 1 = give freed <2MB stores back once RAM use passes MemoryFlushThreshold (default), 0 = never
};

// Resource-aware admission of operations (doc/development/schedule-with-lookahead.md §5.1).
// 0 = off (default), 1 = shadow: decide and report, never withhold, 2 = enforce.
// Cached like IsPerformanceLogging, so the run gate pays one relaxed load.
enum class resource_scheduling : UInt8 { off = 0, shadow = 1, enforce = 2 };
RTC_CALL resource_scheduling GetResourceScheduling();
RTC_CALL void SetResourceScheduling(resource_scheduling mode);

// Whether to measure and report per-operation cost and footprint under MsgCategory::performance.
// Off by default; caches the PerformanceLogging setting so hot paths pay one relaxed load.
RTC_CALL bool IsPerformanceLogging();

// Session-local override of that setting, as the /SP and /CP command-line options do.
RTC_CALL void SetPerformanceLogging(bool enable);

extern "C" RTC_CALL DWORD DMS_CONV RTC_GetRegDWord(RegDWordEnum i);
extern "C" RTC_CALL void  DMS_CONV RTC_SetCachedDWord(RegDWordEnum i, DWORD dw);
extern "C" RTC_CALL bool  DMS_CONV RTC_ParseRegStatusFlag(CharPtr param);
extern "C" RTC_CALL void   DMS_CONV DMS_Appl_SetRegStatusFlags(UInt32 sf);
extern "C" RTC_CALL UInt32 DMS_CONV DMS_Appl_GetRegStatusFlags();

RTC_CALL void ParseRegStatusFlags(int& argc, char**& argv);

RTC_CALL SharedStr GetGeoDmsRegKey(CharPtr key);
RTC_CALL SharedStr GetConvertedGeoDmsRegKey(CharPtr key);
RTC_CALL auto GetGeoDmsRegKeyMultiString(CharPtr key) -> std::vector<SharedStr>;

// Session-local overrides (not persisted to registry, only affects current session)
RTC_CALL void SetSessionLocalOverride(CharPtr key, CharPtr value);
RTC_CALL void ClearSessionLocalOverride(CharPtr key);
RTC_CALL bool HasSessionLocalOverride(CharPtr key);
RTC_CALL SharedStr GetSessionLocalOverride(CharPtr key);

RTC_CALL DWORD GetGeoDmsRegKeyDWord(CharPtr key, DWORD defaultValue, CharPtr section = "");
RTC_CALL bool SetGeoDmsRegKeyDWord(CharPtr key, DWORD dw, CharPtr section = "");
RTC_CALL bool SetGeoDmsRegKeyString(CharPtr key, CharPtr str);
RTC_CALL bool SetGeoDmsRegKeyMultiString(CharPtr key, const std::vector<SharedStr>& strings);

#endif // __UTL_REGISTRY_H
