// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// Platform environment services: registry status flags, config and data
// paths, file/directory operations, session times and platform info —
// implemented in an MSVC section and a POSIX section.

#include "utl/Environment.h"
#include "utl/FileSystem.h"
#include "utl/PlatformError.h"
#include "utl/Registry.h"
#include "utl/TimeFmt.h"
#include "utl/MgFormat.h"

#include "dbg/DmsCatch.h"
#include "vt/Conversions.h"
#include "geom/Point.h"
#include "vt/MinMax.h"
#include "vt/StringBounds.h"
#include "mem/FixedAlloc.h" // SetFreeStackDrainageEnabled: the /SF - /CF switch
#include "ptr/IterCast.h"
#include "set/IndexedStrings.h"
#include "utl/StrFormat.h"
#include "utl/splitPath.h"
#include "LockLevels.h"
#include <thread>

#if defined(_MSC_VER)

#include <vector>

#include <windows.h>
#include <io.h>
#include <Lmcons.h>

#if defined(MG_DEBUG)
	const bool MG_DEBUG_ENVIRONMENT = false;
#endif

#undef Concurrency
#undef Context
#undef Yield

static SharedStr          g_LocalDataDir;

struct LocalAllocatedPtr
{
	LPVOID m_Ptr;
	LocalAllocatedPtr() : m_Ptr(0) {}
	~LocalAllocatedPtr() { if (m_Ptr) LocalFree(m_Ptr); }
};

SharedStr platform::GetSystemErrorText(DWORD lastErr)
{
	// Use FormatMessageW so the system-language message comes back in UTF-16,
	// then transcode to UTF-8 for the rest of the DMS string pipeline.
	// FormatMessageA returns the message in the active code page, which then
	// gets handed up as if it were UTF-8 — non-ASCII characters in localised
	// system messages would become mojibake.
	LPWSTR wMsgBuf = nullptr;
	auto wLen = ::FormatMessageW(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		lastErr,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		reinterpret_cast<LPWSTR>(&wMsgBuf),
		0,
		NULL
	);
	if (!wMsgBuf || wLen == 0)
		return SharedStr();
	auto utf8 = wchar_2_Utf8Str(wMsgBuf, wLen);
	::LocalFree(wMsgBuf);
	return utf8;
};

bool platform::isCharPtrAndExceeds_MAX_PATH(CharPtr xFileName)
{
	return strnlen(xFileName, MAX_PATH) >= MAX_PATH;
}

RTC_CALL DWORD platform::GetLastError()
{
	return ::GetLastError();
}

SizeT RemainingStackSpace()
{
	ULONG_PTR low, high;
	GetCurrentThreadStackLimits(&low, &high);
	auto remaining = reinterpret_cast<ULONG_PTR>(&low) - low;
	return remaining;
}


void DmsYield(UInt32 nrMillisecs)
{
	SYSTEMTIME currTime, nextTime;
	GetSystemTime(&currTime);
	std::this_thread::yield(); // Yield to other contexts (=tasks?) in the current thread or if none available, another OS thread
	GetSystemTime(&nextTime);
	UInt32 currMillisecs = currTime.wMilliseconds + currTime.wSecond * 1000 + currTime.wMinute * 60000; dms_assert(currMillisecs < 60 * 60 * 1000);
	UInt32 nextMillisecs = currTime.wMilliseconds + currTime.wSecond * 1000 + currTime.wMinute * 60000; dms_assert(nextMillisecs < 60 * 60 * 1000);
	if (nextMillisecs < currMillisecs)
		nextMillisecs += 60 * 60 * 1000;
	assert(nextMillisecs >= currMillisecs);
	nextMillisecs -= currMillisecs;
	if (nextMillisecs < nrMillisecs)
		Wait(nrMillisecs - nextMillisecs);
}

bool ManageSystemError(UInt32& retryCounter, CharPtr format, CharPtr fileName, bool throwOnError, bool doRetry)
{
	DWORD lastErr = GetLastError();
	switch (lastErr)
	{
//		case 2:  // cannot find the file specified
		case 5:  // access denied
		case 32: // Het proces heeft geen toegang tot het bestand omdat het door een ander proces wordt gebruikt
			if (!doRetry)
				break;
			if (++retryCounter > 10)
				break;
			UInt32 nrWaitSecs = (1 << retryCounter);
			reportF(SeverityTypeID::ST_MajorTrace,
				"WindowsSystem Error {}:\nErrorCode {}: {}\nWaiting {} seconds before retry #{}", 
				mySSPrintF(format, fileName).c_str(),
				lastErr, 
				platform::GetSystemErrorText(lastErr).c_str(),
				nrWaitSecs,
				retryCounter
			);
			Wait(1000 * nrWaitSecs);
			return true;
	}
	if (throwOnError)
		throwSystemError(lastErr, format, fileName);
	return false;
}

//  -------------------- Main Window Handle

static void* s_GlobalMainWindow = nullptr;

RTC_CALL void* GetGlobalMainWindowHandle() {
	return s_GlobalMainWindow; 
}

RTC_CALL void* SetGlobalMainWindowHandle(void* hWindow)
{
	auto oldHandle = GetGlobalMainWindowHandle();
	s_GlobalMainWindow = hWindow;
	return oldHandle;
}


//  -----------------------------------------------------------------------

#include "utl/IncrementalLock.h"

void Wait(UInt32 nrMillisecs)
{
	Sleep(nrMillisecs);
}

std::atomic<UInt32> g_DispatchLockCount = 0;

bool HasWaitingMessages()
{
	// GetQueueStatus is a pure status probe: unlike PeekMessage it never delivers
	// pending cross-thread sent messages, so it cannot re-enter window procedures
	// mid-computation (a PeekMessage-based implementation once did, dispatching
	// WM_SIZE and the like at arbitrary suspend-poll points; see #1156).
	// QS_ALLINPUT rather than QS_ALLEVENTS: QS_ALLINPUT adds QS_SENDMESSAGE, so a
	// pending incoming SendMessage from another thread or process (shell WM_GETICON
	// probes, SendMessageTimeout(SMTO_ABORTIFHUNG) hang probes, blocking senders)
	// also makes MustSuspend() yield to the event loop, where the next retrieval
	// call delivers it within ~1s instead of after the whole computation.
	return IsMultiThreaded0() && GetQueueStatus(QS_ALLINPUT);
}

extern "C" RTC_CALL bool DMS_CONV DMS_HasWaitingMessages()
{
	return HasWaitingMessages();
}

//  -----------------------------------------------------------------------

bool IsDosFileOrDirAccessible(CharPtr dosFileOrDirName)
{
	auto dosFileOrDirNameW = Utf8_2_wchar(dosFileOrDirName);
	return _waccess(dosFileOrDirNameW.get(), 0) != -1;
}

SharedStr GetCurrentDir()
{
	DWORD buffSize = GetCurrentDirectoryW(0, nullptr);
	std::vector<wchar_t> wchar_buffer(buffSize);
	if (!GetCurrentDirectoryW(buffSize, begin_ptr(wchar_buffer)))
		throwLastSystemError("GetCurrrentDir");
	assert(wchar_buffer.back() == wchar_t(0));

	if (wchar_buffer.empty())
		return SharedStr();
	auto buffer = wchar_2_Utf8Str(begin_ptr(wchar_buffer));
	return ConvertDosFileName(buffer);
}

void SetCurrentDir(CharPtr dir)
{
	auto dirW = Utf8_2_wchar(dir);
	SetCurrentDirectoryW(dirW.get());
}

void AddFontResourceExA_checked(_In_ LPCSTR name, _In_ DWORD fl, _Reserved_ PVOID res)
{
	while (true)
	{
		auto nameW = Utf8_2_wchar(name);
		auto result = AddFontResourceExW(nameW.get(), fl, res);
		if (result)
			break;
		// MessageBoxW: the message contains the font filename (UTF-8) which
		// would mojibake under ANSI MessageBoxA for non-ASCII paths.
		auto msg     = mySSPrintF("Failed to load FontResource {}", name);
		auto msgW    = Utf8_2_wchar(msg.c_str());
		auto userResponse = MessageBoxW(nullptr, msgW.get(), L"Warning", MB_ABORTRETRYIGNORE | MB_ICONWARNING);
		switch (userResponse)
		{
		case IDABORT: terminate();
		case IDIGNORE: break;
		}
	}
}

RTC_CALL void DMS_CONV DMS_Appl_SetFont()
{
	auto exeDir = GetExeDir();
	MG_CHECK(!exeDir.empty());
	AddFontResourceExA_checked(DelimitedConcat(exeDir.c_str(), "misc/fonts/dms.ttf").c_str(), FR_PRIVATE, nullptr);
	AddFontResourceExA_checked(DelimitedConcat(exeDir.c_str(), "misc/fonts/dmstext.ttf").c_str(), FR_PRIVATE, nullptr);
}

// The GeoDms binaries (and their bundled resources: fonts, proj4data, gdal
// data, RewriteExpr.lsp, ...) all live next to this Rtc module, so we derive
// the exe-root dir from this module's own path rather than having each host
// (GeoDmsGuiQt, GeoDmsRun, the python binding) convey it.
extern "C" IMAGE_DOS_HEADER __ImageBase; // linker-provided base of this (Rtc) module

static SharedStr GetExeDirImpl()
{
	std::vector<wchar_t> buf(MAX_PATH);
	for (;;)
	{
		DWORD n = GetModuleFileNameW(reinterpret_cast<HINSTANCE>(&__ImageBase), buf.data(), DWORD(buf.size()));
		if (n == 0)
			return SharedStr();
		if (n < buf.size())            // fit; a truncated result returns buf.size()
			return splitFullPath(ConvertDosFileName(wchar_2_Utf8Str(buf.data(), n)).c_str());
		buf.resize(buf.size() * 2);    // ERROR_INSUFFICIENT_BUFFER: grow and retry
	}
}

RTC_CALL SharedStr GetExeDir()     // dir holding the GeoDms binaries + dms.ini; does NOT end with '/'
{
	static SharedStr s_exeDir = GetExeDirImpl();
	assert(!s_exeDir.empty());
	return s_exeDir;
}

#include "utl/Registry.h"

// Session-local overrides (not persisted to registry)
static std::map<SharedStr, SharedStr> s_SessionLocalOverrides;
static std::mutex s_SessionLocalMutex;

RTC_CALL void SetSessionLocalOverride(CharPtr key, CharPtr value)
{
	std::lock_guard lock(s_SessionLocalMutex);
	s_SessionLocalOverrides[SharedStr(key)] = SharedStr(value);
}

RTC_CALL void ClearSessionLocalOverride(CharPtr key)
{
	std::lock_guard lock(s_SessionLocalMutex);
	s_SessionLocalOverrides.erase(SharedStr(key));
}

RTC_CALL bool HasSessionLocalOverride(CharPtr key)
{
	std::lock_guard lock(s_SessionLocalMutex);
	return s_SessionLocalOverrides.contains(SharedStr(key));
}

RTC_CALL SharedStr GetSessionLocalOverride(CharPtr key)
{
	std::lock_guard lock(s_SessionLocalMutex);
	auto it = s_SessionLocalOverrides.find(SharedStr(key));
	if (it != s_SessionLocalOverrides.end())
		return it->second;
	return SharedStr();
}

RTC_CALL SharedStr GetGeoDmsRegKey(CharPtr key)
{
	// First check session-local overrides
	{
		std::lock_guard lock(s_SessionLocalMutex);
		auto it = s_SessionLocalOverrides.find(SharedStr(key));
		if (it != s_SessionLocalOverrides.end())
			return it->second;
	}

	try {
		RegistryHandleLocalMachineRO regLM;
		if (regLM.ValueExists(key))
		{
			SharedStr result = regLM.ReadString(key);
			if (result == "#DELETED#")
				goto exit;
			return result;
		}
		RegistryHandleCurrentUserRO regCU;
		if (regCU.ValueExists(key))
			return regCU.ReadString(key);
	}
	catch(...) {}
exit:
	return SharedStr();
}

RTC_CALL auto GetGeoDmsRegKeyMultiString(CharPtr key) -> std::vector<SharedStr>
{
	try {
		RegistryHandleLocalMachineRO regLM;
		if (regLM.ValueExists(key))
			return regLM.ReadMultiString(key);

		RegistryHandleCurrentUserRO regCU;
		if (regCU.ValueExists(key))
			return regCU.ReadMultiString(key);
	}
	catch (...) {}
	return {};
}

RTC_CALL bool SetGeoDmsRegKeyDWord(CharPtr key, DWORD dw, CharPtr section)
{
	try {
		RegistryHandleLocalMachineRW regLM(section);
		auto result = regLM.WriteDWORD(key, dw);
	}
	catch (...) {}

	return true;
}

RTC_CALL DWORD GetGeoDmsRegKeyDWord(CharPtr key, DWORD defaultValue, CharPtr section)
{
	try {
		RegistryHandleLocalMachineRO regLM(section);
		if (regLM.ValueExists(key))
			return regLM.ReadDWORD(key);
	}
	catch (...) {}

	return defaultValue;
}

RTC_CALL bool SetGeoDmsRegKeyString(CharPtr key, CharPtr str)
{
	try {
		RegistryHandleLocalMachineRW regLM;
		regLM.WriteString(key, str);
	}
	catch (...) {}
	return true;
}

RTC_CALL bool SetGeoDmsRegKeyMultiString(CharPtr key, const std::vector<SharedStr>& strings)
{
	try {
		RegistryHandleLocalMachineRW regLM;
		auto result = regLM.WriteMultiString(key, strings);
	}
	catch (...) {}

	return true;
}

SharedStr GetConvertedGeoDmsRegKey(CharPtr key)
{
	SharedStr result;
	if (!PlatformInfo::GetEnvString("directories", key, result))
		result = ConvertDosFileName(GetGeoDmsRegKey(key));
	return result;
}

SharedStr GetLocalDataDirImpl()
{
	SharedStr localDataDir = GetConvertedGeoDmsRegKey("LocalDataDir");
	if (localDataDir.empty())
		localDataDir = "C:/LocalData";
	return localDataDir;
}

RTC_CALL SharedStr GetLocalDataDir()
{
	static SharedStr localDataDir = GetLocalDataDirImpl();
	return localDataDir;
}

SharedStr GetSourceDataDirImpl()
{
	SharedStr sourceDataDir = GetConvertedGeoDmsRegKey("SourceDataDir");
	if (sourceDataDir.empty())
		sourceDataDir = "C:\\SourceData";

	return sourceDataDir;
}

SharedStr GetSourceDataDir()
{
	static SharedStr sourceDataDir = GetSourceDataDirImpl();
	return sourceDataDir;
}

UInt32 g_RegStatusFlags = 0; // status flags as found in the register
UInt32 g_OvrStatusFlags = 0; // status flags as set from the command line
UInt32 g_OvrStatusMask  = 0; // mask for status flags as set from the command line

// Order-safe accessor for the registry-access section. This section is reached
// from DYNAMIC INITIALIZATION: a namespace-scope TokenID initializer (e.g.
// token::UInt32 in tic/LispTreeType.cpp) creates a token, IndexedStrings then
// calls EventLog_HideDepreciatedCaseMixupWarnings, which reads the registry
// status flags. A namespace-scope section would then be used before this TU's
// own initializers have run -- on GCC that order is link-order dependent, and
// the Linux Debug build aborted at startup on EnterLevel's level != 0 assert
// (in Release the assert is compiled out, so the same disorder passed
// unnoticed). A function-local static is constructed on first use, so the
// order no longer matters. See also the ThousandSeparator note in
// dbg/MsgDispatch.cpp, which worked around this same fragility at a call site.
static leveled_critical_section& RegAccessSection()
{
	static leveled_critical_section s_RegAccess(item_level_type(0), ord_level_type::RegisterAccess, "RegisterAccess");
	return s_RegAccess;
}

RTC_CALL void DMS_Appl_SetRegStatusFlags(UInt32 newSF)
{
	leveled_critical_section::scoped_lock lock(RegAccessSection());
	g_RegStatusFlags = (newSF | RSF_WasRead);
}

UInt32 ReadOnceRegisteredStatusFlags()
{
	if (g_RegStatusFlags & RSF_WasRead)
		return g_RegStatusFlags;

	leveled_critical_section::scoped_lock lock(RegAccessSection());

	if (g_RegStatusFlags & RSF_WasRead)
		return g_RegStatusFlags;

	g_RegStatusFlags |= RSF_WasRead;
	try {
		RegistryHandleLocalMachineRO reg;
		if (reg.ValueExists("StatusFlags"))
		{
			g_RegStatusFlags |= reg.ReadDWORD("StatusFlags");
			return g_RegStatusFlags;
		}
	}
	catch (...) {}
	try {
		RegistryHandleCurrentUserRO reg;
		if (reg.ValueExists("StatusFlags"))
		{
			g_RegStatusFlags |= reg.ReadDWORD("StatusFlags");
			return g_RegStatusFlags;
		}
	}
	catch (...) {}

	g_RegStatusFlags |= RSF_Default;
	return g_RegStatusFlags;
}

RTC_CALL UInt32 GetRegStatusFlags()
{
	auto registeredFlags = ReadOnceRegisteredStatusFlags();
	return (registeredFlags & ~(g_OvrStatusMask | RSF_WasRead)) | (g_OvrStatusFlags & g_OvrStatusMask);
}

RTC_CALL UInt32 DMS_Appl_GetRegStatusFlags()
{
	return GetRegStatusFlags();
}

RTC_CALL void SetCachedStatusFlag(UInt32 newSF, bool newVal)
{
	leveled_critical_section::scoped_lock lock(RegAccessSection());
	g_OvrStatusMask |= newSF;
	if (newVal)
		g_OvrStatusFlags |= newSF; // set
	else
		g_OvrStatusFlags &= ~newSF; // clear


}

void SetRegStatusFlags(UInt32 newSF)
{
	SetGeoDmsRegKeyDWord("StatusFlags", newSF);
	DMS_Appl_SetRegStatusFlags(newSF);
}

RTC_CALL void SetStatusFlag(UInt32 newSF, bool newVal)
{
	leveled_critical_section::scoped_lock lock(RegAccessSection());
	g_OvrStatusMask |= newSF;
	if (newVal)
		g_OvrStatusFlags |= newSF; // set
	else
		g_OvrStatusFlags &= ~newSF; // clear

	auto sf = ReadOnceRegisteredStatusFlags();
	if (newVal)
		sf |= newSF; // set
	else
		sf &= ~newSF; // clear

	sf &= ~RSF_WasRead;
	SetGeoDmsRegKeyDWord("StatusFlags", sf);
	g_RegStatusFlags = (sf | RSF_WasRead);
}

RTC_CALL bool IsInDebugMode()
{
	return GetRegStatusFlags() & RSF_DebugMode;
}

RTC_CALL bool IsMultiThreaded0()
{
	return GetRegStatusFlags() & RSF_SuspendForGUI;
}

RTC_CALL bool IsMultiThreaded1()
{
	return GetRegStatusFlags() & RSF_MultiThreading1;
}

RTC_CALL bool IsMultiThreaded2()
{
	return GetRegStatusFlags() & RSF_MultiThreading2;
}

RTC_CALL bool IsMultiThreaded3()
{
	return GetRegStatusFlags() & RSF_MultiThreading3;
}

bool IsMultiThreaded1or2()
{
	return GetRegStatusFlags() & (RSF_MultiThreading1| RSF_MultiThreading2);
}

bool HasDynamicROI()
{
	return GetRegStatusFlags() & RSF_DynamicROI;
}

RTC_CALL bool ShowThousandSeparator()
{
	return GetRegStatusFlags() & RSF_ShowThousandSeparator;
}

bool EventLog_HideDepreciatedCaseMixupWarnings()
{
	return GetRegStatusFlags() & RSF_EventLog_HideDepreciated;
}

extern "C" RTC_CALL bool DMS_CONV RTC_ParseRegStatusFlag(const char* param)
{
	dms_assert(param);

	if (param[0] != '/')
		return false;

	char cmd = param[1];
	if (cmd != 'S' && cmd != 'C')
		return false;

	bool newValue = (cmd == 'S');

	switch (param[2])
	{
		case 'A': SetCachedStatusFlag(RSF_AdminMode, newValue); break;
		case 'C': SetCachedStatusFlag(RSF_ShowStateColors, newValue); break;
		case 'V': SetCachedStatusFlag(RSF_TreeViewVisible, newValue); break;
		case 'D': SetCachedStatusFlag(RSF_DetailsVisible, newValue); break;
		case 'E': SetCachedStatusFlag(RSF_EventLogVisible, newValue); break;
		case 'T': SetCachedStatusFlag(RSF_ToolBarVisible, newValue); break;
		case 'I': SetCachedStatusFlag(RSF_CurrentItemBarHidden, newValue); break;
		case 'M': SetCachedStatusFlag(RSF_DebugMode, newValue); break;
		case 'R': SetCachedStatusFlag(RSF_DynamicROI, newValue); break;
		case 'S':
		case '0': SetCachedStatusFlag(RSF_SuspendForGUI, newValue); break;
		case '1': SetCachedStatusFlag(RSF_MultiThreading1, newValue); break;
		case '2': SetCachedStatusFlag(RSF_MultiThreading2, newValue); break;
		case '3': SetCachedStatusFlag(RSF_MultiThreading3, newValue); break;
		case 'H': SetCachedStatusFlag(RSF_ShowThousandSeparator, newValue); break;
		case 'P': SetPerformanceLogging(newValue); break; // not a status flag: that DWORD is out of bits
		case 'Q': SetResourceScheduling(newValue ? resource_scheduling::enforce : resource_scheduling::off); break;
		case 'q': SetResourceScheduling(newValue ? resource_scheduling::shadow : resource_scheduling::off); break;
		// /SB<MB> caps the scheduler's admission budget for this run; /CB restores the derived one.
		// A value-taking switch, like /L: everything after the 'B' is the number.
		case 'B': RTC_SetCachedDWord(RegDWordEnum::SchedulerBudgetMB, newValue ? DWORD(atoi(param + 3)) : 0); break;
		// /CF switches OFF the free-store drainage that otherwise starts once RAM use passes
		// MemoryFlushThreshold; /SF restores the default. 'F' as in Free-store: 'D' is DetailsVisible.
		case 'F': RTC_SetCachedDWord(RegDWordEnum::MemoryDrainage, newValue ? 1 : 0);
		          SetFreeStackDrainageEnabled(newValue); break;
		case 'W': SetCachedStatusFlag(RSF_EventLog_HideDepreciated, !newValue); break; // the command line option is /SW to Show (not hide) deprecated events, but the flag is HideDepreciated, so invert the value
		default:
			reportF(SeverityTypeID::ST_Warning, "Unrecognised command line {} option {}",  (newValue ? "Set" : "Clear"), param);
			return true;
	}
//	reportF(SeverityTypeID::ST_MinorTrace, "Recognised command line option {} {}", (newValue ? "Set" : "Clear"), param[2]);
	return true;
}

RTC_CALL void ParseRegStatusFlags(int& argc, char**& argv)
{
	while (argc)
	{
		if (!RTC_ParseRegStatusFlag(argv[0]))
			return;
		++argv;
		--argc;
	}
}

struct RegDWordAttr
{
	CharPtr key;
	DWORD   value;
	bool    wasRead;
};

RegDWordAttr s_RegDWordAttrs[] =
{
	{ "MemoryFlushThreshold", 80, false},
	{ "SwapFileMinSize", 0, false },
    { "DrawingSizeInPixels", 0, false },
	{ "MemoryMaxRAM_GB", 64, false }, // simulates a smaller machine; also throttles operation activation via IsLowOnFreeRAM
	{ "PerformanceLogging", 0, false },
	{ "ResourceAwareScheduling", 0, false }, // OFF by default (0=off, 1=shadow, 2=enforce). Switched on
	                                        // with /Sq or /SQ, or the q/Q boxes under Settings >
	                                        // Local machine options > Parallel Processing.
	                                        // Off because enforce does not yet pay for itself: measured
	                                        // on t641_2 it parked 124 184 operations and still left the
	                                        // live peak at 171.9 GiB -- identical to the run without it
	                                        // (doc SS8.1.33). Budget = MemoryFlushThreshold % of allowed
	                                        // RAM -- the same threshold that triggers MemoryDrainage --
	                                        // unless SchedulerBudgetMB (/SB<MB>) overrides it.
	{ "SchedulerBudgetMB", 0, false },
	{ "MemoryDrainage", 1, false } // on by default; the trigger is MemoryFlushThreshold (doc SS8.1.32)
};

extern "C" RTC_CALL DWORD RTC_GetRegDWord(RegDWordEnum i)
{
	auto ui = UInt32(i);
	MG_CHECK(ui < sizeof(s_RegDWordAttrs) / sizeof(RegDWordAttr));

	leveled_critical_section::scoped_lock lock(RegAccessSection());

	RegDWordAttr& regAttr = s_RegDWordAttrs[ui];
	if (!regAttr.wasRead)
	{
		regAttr.wasRead = true;
		try {
			RegistryHandleLocalMachineRO reg;
			if (reg.ValueExists(regAttr.key))
			{
				regAttr.value = reg.ReadDWORD(regAttr.key);
				goto exit;
			}
		}
		catch(...) {}
		try {
			RegistryHandleCurrentUserRO reg;
			if (reg.ValueExists(regAttr.key))
			{
				regAttr.value = reg.ReadDWORD(regAttr.key);
				goto exit;
			}
		}
		catch (...) {}
	}
exit:
	return regAttr.value;
}

extern "C" RTC_CALL void RTC_SetCachedDWord(RegDWordEnum i, DWORD dw)
{
	auto ui = UInt32(i);
	assert(ui < sizeof(s_RegDWordAttrs) / sizeof(RegDWordAttr));

	leveled_critical_section::scoped_lock lock(RegAccessSection());
	RegDWordAttr& regAttr = s_RegDWordAttrs[ui];
	regAttr.wasRead = true;
	regAttr.value   = dw;
}

void MakeDir(WeakStr dirName)
{
	auto dmsDirName = ConvertDmsFileName(dirName);
	if (!CreateDirectoryW(Utf8_2_wchar(dmsDirName).get(), 0))
	{
		if (GetLastError() == ERROR_ALREADY_EXISTS)
			return;
		throwLastSystemError("MakeDir('{}')", dirName.c_str());
	}
}

bool IsDosDir(WeakStr dosFileName, CharPtr dmsFileName)
{
	// dosFileName is UTF-8; the unsuffixed GetFileAttributes resolves to
	// GetFileAttributesA (UNICODE/_UNICODE not defined for this project),
	// which interprets the bytes as the active code page. Use the wide-
	// char variant so non-ASCII paths (e.g. Greek β) resolve correctly.
	// See #1101 for the original symptom.
	DWORD attr = GetFileAttributesW(Utf8_2_wchar(dosFileName.c_str()).get());
	if (attr == INVALID_FILE_ATTRIBUTES)
		throwLastSystemError("IsDir({})", dmsFileName);
	return (attr & FILE_ATTRIBUTE_DIRECTORY);
}

void ReplaceSpecificDelimiters(MutableCharPtrRange range, const char delimiter)
{
	while (range.first != range.second)
	{
		if (*range.first == delimiter)
			*range.first = '\\';
		++range.first;
	}
}

void ReplaceDosDelimiters(MutableCharPtrRange range)
{
	while (range.first != range.second)
	{
		if (*range.first == '\\')
			*range.first = DELIMITER_CHAR;
		++range.first;
	}
}

void ReplaceDmsDelimiters(MutableCharPtrRange range)
{
	while (range.first != range.second)
	{
		if (*range.first == DELIMITER_CHAR)
			*range.first = '\\';
		++range.first;
	}
}

SharedStr ConvertDosFileName(WeakStr fileName) // replaces '\' by '/' and prefixes //SYSTEM/path by 'file:'
{
	SharedStr result(fileName);
	if (HasDosDelimiters(fileName.c_str()))
		ReplaceDosDelimiters(result.GetAsMutableRange());
	if (!result.empty() && result[0] == '/' && result[1] == '/')
		return "file:" + result;
	return result;
}

SharedStr ConvertDmsFileNameAlways(SharedStr&& path)
{
	ReplaceDmsDelimiters(path.GetAsMutableRange());
	return path;
}

SharedStr ConvertDmsFileName(WeakStr path) // replaces '/' by '\' iff prefixed by ' file:' to prevent misinterpretation of  file://SYSTEM/path
{
	if (path.empty())
		return path;

	if (strncmp(path.begin(), "file:", 5))
		return path;

	return ConvertDmsFileNameAlways(SharedStr(CharPtrRange(path.begin()+5, path.send())));
}

bool HasDosDelimiters(CharPtr source)
{
	dms_assert(source);
	if (source[0] == '/' && source[1] == '/')
		return true; // should have been prefixed by "file:"
	while (*source)
	{
		if (*source == '\\')
			return true;
		++source;
	}
	return false;
}

bool HasDosDelimiters(CharPtrRange source)
{
	if (source.size() >= 2 && source[0] == '/' && source[1] == '/')
		return true; // should have been prefixed by "file:"
	while (source.first != source.second)
	{
		if (*source.first == '\\')
			return true;
		++source.first;
	}
	return false;
}

bool IsRelative(CharPtr source)
{
	dms_assert(source);
	while (*source)
	{
		dms_assert(*source != '\\'); // Precondition of source being a dms path
		if (*source == '/' || *source == ':' || *source == '%')
			return false;
		++source;
	}
	return true;
}

void MakeDirsForFileImpl(WeakStr fullFileName)
{
	SharedStr pathStr = splitFullPath(fullFileName.c_str());
	if (IsFileOrDirAccessible(pathStr))
		return;
	if (pathStr.empty())
		return;
	dms_assert(pathStr.ssize());
	char ch = pathStr.send()[-1];
	if (ch == ':' || ch == '/') // don't create (new) volumes
		return;
	MakeDirsForFileImpl(pathStr);
	MakeDir(pathStr);
}

void MakeDirsForFile(WeakStr fullFileName)
{
	dms_assert(fullFileName.c_str());
	dms_assert(!HasDosDelimiters(fullFileName.c_str()));
	dms_assert(IsAbsolutePath(fullFileName.c_str()));

//	SharedStr pathStr = ConvertDosFileName(fullFileName);

//	MakeDirsForFileImpl(pathStr.c_str());
	MakeDirsForFileImpl(fullFileName);
}

extern "C" RTC_CALL void DMS_CONV DMS_MakeDirsForFile(CharPtr fileName)
{
	MakeDirsForFile(ConvertDosFileName(SharedStr(fileName MG_DEBUG_ALLOCATOR_SRC("DMS_MakeDirsForFile"))));
}

//  -----------------------------------------------------------------------

FindFileBlock::FindFileBlock(WeakStr fileSearchSpec)
	:	m_Data  (new Byte[sizeof(WIN32_FIND_DATAW)])
	,	m_Handle(
			// Use the wide-char variant: WIN32_FIND_DATA (= WIN32_FIND_DATAA
			// since UNICODE/_UNICODE aren't defined for this project) would
			// fail on non-ASCII filenames the same way #1101 did.
			FindFirstFileW(
				Utf8_2_wchar(ConvertDmsFileName(fileSearchSpec).c_str()).get()
			,	reinterpret_cast<WIN32_FIND_DATAW*>(m_Data.get())
			)
		)
{
}

FindFileBlock::FindFileBlock(FindFileBlock&& src) noexcept
	:	m_Data  (std::move(src.m_Data  ))
	,	m_Handle(std::move(src.m_Handle))
{
	src.m_Handle = INVALID_HANDLE_VALUE;
}

FindFileBlock::~FindFileBlock() 
{
	if (IsValid()) 
		FindClose(m_Handle); 
}

bool FindFileBlock::IsValid() const
{
	return m_Handle != INVALID_HANDLE_VALUE;
}

DWORD FindFileBlock::GetFileAttr() const
{
	dms_assert(IsValid());
	return reinterpret_cast<const WIN32_FIND_DATAW*>(m_Data.get())->dwFileAttributes;
}

bool FindFileBlock::IsDirectory() const
{
	return GetFileAttr() & FILE_ATTRIBUTE_DIRECTORY;
}

CharPtr FindFileBlock::GetCurrFileName() const
{
	// Lazy-cache the UTF-8 transcoding of cFileName so the returned pointer
	// stays valid for the caller's lifetime of this iterator entry.
	if (m_CurrFileNameUtf8.empty())
		m_CurrFileNameUtf8 = wchar_2_Utf8Str(reinterpret_cast<const WIN32_FIND_DATAW*>(m_Data.get())->cFileName);
	return m_CurrFileNameUtf8.c_str();
}

bool FindFileBlock::Next()
{
	m_CurrFileNameUtf8 = SharedStr(); // invalidate cached UTF-8 view
	return FindNextFileW(m_Handle, reinterpret_cast<WIN32_FIND_DATAW*>(m_Data.get()));
}

FileDateTime AsFileDateTime(UInt32 hiDW, UInt32 loDW)
{
	union {
		UInt64                asUInt64;
		struct { 
			#if defined(CC_BYTEORDER_INTEL)
				UInt32 low, high;
			#else
				UInt32 high, low;
			#endif
		}                     asUInt32Pair;
	}	result;
	result.asUInt32Pair.high = hiDW;
	result.asUInt32Pair.low  = loDW;
	return result.asUInt64;
}

FileDateTime FindFileBlock::GetFileOrDirDateTime() const
{
	if (IsValid())
	{
		const WIN32_FIND_DATAW* findData= reinterpret_cast<const WIN32_FIND_DATAW*>(m_Data.get());
		return AsFileDateTime(
			findData->ftLastWriteTime.dwHighDateTime,
			findData->ftLastWriteTime.dwLowDateTime
		);
	}
	return AsFileDateTime(0, 0);
}

auto AsDateTimeString(FileDateTime t64) -> SharedStr
{
	FILETIME lft1, lft2;

	lft1.dwHighDateTime = (t64 >> 32);
	lft1.dwLowDateTime = t64 & 0xFFFFFFFF;

	FileTimeToLocalFileTime(&lft1, &lft2);
	SYSTEMTIME stCreate;
	FileTimeToSystemTime(&lft2, &stCreate);

	return mySSPrintF("{:04}/{:02}/{:02}  {:02}:{:02}:{:02}",
		stCreate.wYear, stCreate.wMonth, stCreate.wDay,
		stCreate.wHour, stCreate.wMinute, stCreate.wSecond
	);
}

//  -----------------------------------------------------------------------

void CopyAllInDir(CharPtr srcDirName, CharPtr destDirName)
{
	FindFileBlock searchFileOrDirs(DelimitedConcat(srcDirName, "*"));

	if (!searchFileOrDirs.IsValid()) 
		goto error;
	do
	{
		CharPtr currFileName = searchFileOrDirs.GetCurrFileName();
		if (currFileName[0] != '.')
			CopyFileOrDir(
				DelimitedConcat(srcDirName , currFileName).c_str()
			,	DelimitedConcat(destDirName, currFileName).c_str()
			,   false);
	}	while (searchFileOrDirs.Next());

	if (GetLastError() == ERROR_NO_MORE_FILES) 
		return;
error:
	throwLastSystemError("CopyAllInDir({}, {})", srcDirName, destDirName);
}

void SetWritable(CharPtr dosFileName)
{
	// PRECONDITION: fileName is in dos format (no file:// prefix).
	// dosFileName is UTF-8; use the wide-char Win32 variants so non-ASCII
	// paths (e.g. Greek β, see #1101) actually resolve.
	auto wideName = Utf8_2_wchar(dosFileName);
	DWORD dwAttrs = GetFileAttributesW(wideName.get());
	if (dwAttrs & FILE_ATTRIBUTE_READONLY)
	{
		SetFileAttributesW(wideName.get(), dwAttrs & ~FILE_ATTRIBUTE_READONLY);
	}
}

bool CopyOrMoveFileOrDirImpl(CharPtr srcFileOrDirName, CharPtr destFileOrDirName, bool mustCopy, bool mayBeMissing)
{
	SharedStr currDir = GetCurrentDir();
	dms_assert(!HasDosDelimiters(currDir.c_str()  ));
	dms_assert(!HasDosDelimiters(srcFileOrDirName ));
	dms_assert(!HasDosDelimiters(destFileOrDirName));
	SharedStr fullSrc = DelimitedConcat(currDir.c_str(), srcFileOrDirName );
	SharedStr fullDst = DelimitedConcat(currDir.c_str(), destFileOrDirName);
	dms_assert(!HasDosDelimiters(fullSrc.c_str()));
	dms_assert(!HasDosDelimiters(fullDst.c_str()));

	if (!stricmp(fullSrc.c_str(), fullDst.c_str()))
		return false;

	UInt32 fullSrcSize = fullSrc.ssize();
	if (!strnicmp(fullSrc.c_str(), fullDst.c_str(), fullSrcSize))
	{
		dms_assert(fullDst.ssize() > fullSrcSize);
		if (fullDst[fullSrcSize] == '/')
			throwErrorF("FileSystem", "CopyOrMoveFileOrDirImpl({}, {}):\n"
				"Cannot {} to a subdir from source because this would result in infinite recursion.", 
				srcFileOrDirName, destFileOrDirName,
				mustCopy ? "copy" : "move"
			);
	}

	MakeDirsForFile(fullDst);

	SharedStr
		fullSrcDOS = ConvertDmsFileName(fullSrc),
		fullDstDOS = ConvertDmsFileName(fullDst);

	auto wideSrc = Utf8_2_wchar(fullSrcDOS.c_str());
	auto wideDst = Utf8_2_wchar(fullDstDOS.c_str());
	if (!mustCopy)
	{
		if (!MoveFileW(wideSrc.get(), wideDst.get()))
		{
			if (GetLastError() != 2 || !mayBeMissing)
				return false;
			throwLastSystemError("MoveFileOrDir({}, {})", srcFileOrDirName, destFileOrDirName);
		}
	}
	else
	{
		DWORD attr = GetFileAttributesW(wideSrc.get());
		if (attr == INVALID_FILE_ATTRIBUTES)
			throwLastSystemError("CopyFileOrDir({}, {})", srcFileOrDirName, destFileOrDirName);

		if (attr & FILE_ATTRIBUTE_DIRECTORY)
		{
			MakeDir(fullDst);
			CopyAllInDir(fullSrc.c_str(), fullDst.c_str());
		}
		else
		{
			if (!CopyFileW(wideSrc.get(), wideDst.get(), FALSE))
				throwLastSystemError("CopyFile({}, {})", srcFileOrDirName, destFileOrDirName);
			SetWritable(fullDstDOS.c_str());
		}
	}
	return true;
}

void CopyFileOrDir(CharPtr srcFileOrDirName, CharPtr destFileOrDirName, bool mayBeMissing)
{
	CopyOrMoveFileOrDirImpl(srcFileOrDirName, destFileOrDirName, true, mayBeMissing);
}

bool MoveFileOrDir(CharPtr srcFileOrDirName, CharPtr destFileOrDirName, bool mayBeMissing)
{
	return CopyOrMoveFileOrDirImpl(srcFileOrDirName, destFileOrDirName, false, mayBeMissing);
}

bool KillAllInDir(CharPtr dirName)
{
	FindFileBlock searchFileOrDirs(DelimitedConcat(dirName, "*.*"));
	bool result = true;

	if (!searchFileOrDirs.IsValid()) 
		goto error;

	do
	{
		CharPtr currFileName = searchFileOrDirs.GetCurrFileName();
		if (currFileName[0] != '.' && !KillFileOrDir(DelimitedConcat(dirName, currFileName)))
			result = false;
	}	while (searchFileOrDirs.Next());

	if (GetLastError() == ERROR_NO_MORE_FILES) 
		return result;

error:
	throwLastSystemError("KillAllInDir({})", dirName);
}

bool BreakingReport(CharPtr funcStr, CharPtr fileName)
{
	DWORD lastError = GetLastError();
	dms_assert(lastError);
	bool mustBreak = (lastError != 32 && lastError != 5);
	if (mustBreak)
	{
		reportF(SeverityTypeID::ST_MajorTrace, "Failure in {}({}) because {}"
		,	funcStr
		,	fileName
		,	platform::GetSystemErrorText(lastError).c_str()
		);
		return true;
	}
	reportF(SeverityTypeID::ST_MajorTrace, "Retry {}({}) after waiting 1 sec because {}"
	,	funcStr
	,	fileName
	,	platform::GetSystemErrorText(lastError).c_str()
	);
	Sleep(1000);
	return false;
}

bool KillFileOrDir(WeakStr fileOrDirName, bool canBeDir)
{
	DBG_START("KillFileOrDir", fileOrDirName.c_str(), MG_DEBUG_ENVIRONMENT);

	SharedStr dosFileOrDirName = ConvertDmsFileName(fileOrDirName);
	if (!IsDosFileOrDirAccessible(dosFileOrDirName.c_str()))
		return true;
	if (canBeDir && IsDosDir(dosFileOrDirName, fileOrDirName.c_str()))
	{
		if (!KillAllInDir(fileOrDirName.c_str()))
			return false;

		auto wideName = Utf8_2_wchar(dosFileOrDirName.c_str());
		UInt32 retryConter = 0;
		do {
			if (RemoveDirectoryW(wideName.get()))
				return true;
			if (BreakingReport("RemoveDirectory", fileOrDirName.c_str()))
				break;
		}
		while (retryConter++ < 5);
	}
	else
	{
		// safeguard by *.bin filter for now!
		CharPtr ext = getFileNameExtension(fileOrDirName.c_str());
		if (canBeDir
		&&	stricmp(ext, "dmsdata")
		&&	stricmp(ext, "tmp")
		&&	stricmp(ext, "old")
		)
			throwErrorF("FileSystem", 
				"Suspected call to KillFileOrDir('{}').\n"
				"Only .dmsdata, .tmp or .old extensions are allowed now.", fileOrDirName.c_str()
			);

		auto wideName = Utf8_2_wchar(dosFileOrDirName.c_str());
		UInt32 retryConter = 0;
		do {
			if (DeleteFileW(wideName.get()))
				return true;
			if (BreakingReport("DeleteFile", fileOrDirName.c_str()))
				break;
		}
		while (retryConter++ < 5);
	}

	return false;
}

bool IsFileOrDirAccessible(WeakStr fileOrDirName)
{
	return IsDosFileOrDirAccessible(ConvertDmsFileName(fileOrDirName).c_str());
}

bool IsFileOrDirWritable(WeakStr fileOrDirName)
{
	auto dosNameW = Utf8_2_wchar(ConvertDmsFileName(fileOrDirName).c_str());
	return _waccess(dosNameW.get(), 2) != -1;
}

void GetWritePermission(WeakStr fileName)
{
	if (IsFileOrDirAccessible(fileName))
	{
		if (!IsFileOrDirWritable(fileName))
			throwErrorF("FileSystem", "Write permission for '{}' denied", fileName);
	}
	else
		MakeDirsForFile(fileName);
}

FileDateTime GetFileOrDirDateTime(WeakStr fileOrDirName)
{
	FindFileBlock fileInfo(fileOrDirName);
	return fileInfo.GetFileOrDirDateTime();
}

//////////////////////////////////////////////////////////////////////
// utf8 -> wchar_t
//////////////////////////////////////////////////////////////////////

std::unique_ptr<wchar_t[]> Utf8_2_wchar(const char* utf8str, int sSize)
{
	if (!utf8str)
		return {};

	if (!sSize || !*utf8str)
	{
		auto result = std::make_unique<wchar_t[]>(1);
		result.get()[0] = wchar_t(0);
		return result;
	}

	// If sSize < 0, treat as null-terminated input (include terminator in output).
	const int inLen = (sSize < 0) ? -1 : sSize;

	// Query required UTF-16 length (including null terminator)
	int required = MultiByteToWideChar(
		CP_UTF8,
		0,
		utf8str,
		inLen,  // null-terminated UTF-8 or given size
		nullptr,
		0
	);

	if (required == 0) 
		throwLastSystemError("MultiByteToWideChar(CP_UTF8) size query failed");

	std::size_t allocSize =
		(inLen == -1) ? static_cast<std::size_t>(required)
		: static_cast<std::size_t>(required + 1);

	auto utf16Buff = std::make_unique<wchar_t[]>(allocSize);

	int written = MultiByteToWideChar(
		CP_UTF8,
		0,
		utf8str,
		sSize,
		utf16Buff.get(),
		required
	);

	if (written == 0)
		throwLastSystemError("MultiByteToWideChar(CP_UTF8) conversion failed");

	// If inLen != -1, the output is NOT null-terminated by WideCharToMultiByte.
	// Add a terminator if there's room (there should be, given our sizing).
	if (inLen != -1) {
		assert(SizeT(written )< allocSize);
		utf16Buff[written] = '\0';
	}
	return utf16Buff;
}

std::unique_ptr<wchar_t[]> Utf8_2_wchar(WeakStr utf8str)
{
	return Utf8_2_wchar(utf8str.c_str(), ThrowingConvert<int>(utf8str.ssize()));
}


auto wchar_2_Utf8Str(const wchar_t* wCharStr, int strLen) -> SharedStr
{
	assert(wCharStr);

	if (!strLen || !*wCharStr)
		return SharedStr();

	// If strLen < 0, treat as null-terminated input (include terminator in output).
	const int inLen = (strLen < 0) ? -1 : strLen;

	int required = ::WideCharToMultiByte(
		CP_UTF8,
		0,
		wCharStr,
		inLen,
		nullptr,
		0,
		nullptr,
		nullptr
	);

	if (required == 0) {
		throwLastSystemError("WideCharToMultiByte(CP_UTF8) size query failed");
	}

	std::size_t allocSize =
		(inLen == -1) ? static_cast<std::size_t>(required)
		: static_cast<std::size_t>(required + 1);

	auto utf8Buff = SharedCharArray::CreateUninitialized(allocSize MG_DEBUG_ALLOCATOR_SRC("wchar_2_Utf8Str"));
	SharedStr result(utf8Buff);

	int written = ::WideCharToMultiByte(
		CP_UTF8,
		0,
		wCharStr,
		inLen,
		utf8Buff->begin(),
		required,
		nullptr,
		nullptr
	);

	if (written == 0)
		throwLastSystemError("WideCharToMultiByte(CP_UTF8) conversion failed");


	// If inLen != -1, the output is NOT null-terminated by WideCharToMultiByte.
	// Add a terminator if there's room (there should be, given our sizing).
#if defined(MG_DEBUG)
	auto writtenStrlength = written; if (inLen == -1) writtenStrlength--;
	for (int i = 0; i != writtenStrlength; ++i)
	{
		assert(utf8Buff->begin()[i]);
	}
#endif

	if (inLen != -1) {

		assert(SizeT(written) < allocSize);
		utf8Buff->begin()[written] = '\0';
	}
#if defined(MG_DEBUG)
	else
	{
		assert(utf8Buff->begin()[writtenStrlength] == char(0));
	}
#endif

	return result;
}

//  -----------------------------------------------------------------------

// Child process used by exec expressions for executables; Create; Execute and Wait for termination
start_process_result_t StartChildProcess(CharPtr moduleName, Char* cmdLine)
{
	STARTUPINFOW siStartInfo;
	PROCESS_INFORMATION piProcInfo;

	// Set up members of STARTUPINFO structure. 
	ZeroMemory(&siStartInfo, sizeof(STARTUPINFOW));
	ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
	siStartInfo.cb = sizeof(STARTUPINFO);
	//   siStartInfo.dwFlags = STARTF_FORCEONFEEDBACK;

//	MessageBox(nullptr, cmdLine, moduleName, MB_OK);

	auto moduleNameW = Utf8_2_wchar(moduleName);
	auto cmdLineW = Utf8_2_wchar(cmdLine);

	// Create the child process.
	BOOL res = CreateProcessW
	(
		moduleNameW.get(),
		cmdLineW.get(),		// command line can be rewritten
		NULL,           // process security attributes 
		NULL,           // primary thread security attributes 
		TRUE,           // handles are inherited 
		0,              // creation flags		
		NULL,           // use parent's environment 
		NULL,           // use parent's current directory 
		&siStartInfo,   // STARTUPINFO pointer 
		&piProcInfo
	);  // receives PROCESS_INFORMATION 

	if (!res)
		throwLastSystemError("ExecuteChildProcess({}, {}) failed", moduleName?moduleName:"NULL", cmdLine);

	return { piProcInfo.hProcess, piProcInfo.hThread };
}

DWORD ExecuteChildProcess(CharPtr moduleName, Char * cmdLine)
{
	auto childProcess = StartChildProcess(moduleName, cmdLine);

	// Wait until child process exits.
	UINT32 waitCounter = 0;
	while (auto resWFSO = WaitForSingleObject(childProcess.first, INFINITE))
	{
		++waitCounter;
	}
	DWORD exitCode;
	BOOL res = GetExitCodeProcess(childProcess.first, &exitCode);
	if (!res)
		throwLastSystemError("ExecuteChildProcess({}, {}) failed to return an exitcode", moduleName, cmdLine);

	CloseHandle(childProcess.second);
	CloseHandle(childProcess.first);

	return exitCode;
};
 
Int32 GetConfigKeyValue(WeakStr configFileName, CharPtr sectionName, CharPtr keyName, Int32 defaultValue)
{
	if (!IsFileOrDirAccessible(configFileName))
		return defaultValue;

	auto sectionNameW = Utf8_2_wchar(sectionName);
	auto keyNameW = Utf8_2_wchar(keyName);
	auto dmsFileName = ConvertDmsFileName(configFileName);
	auto dmsFileNameW = Utf8_2_wchar(dmsFileName.c_str());

	return GetPrivateProfileIntW(sectionNameW.get(), keyNameW.get(), defaultValue, dmsFileNameW.get());
}

SharedStr GetConfigKeyString(WeakStr configFileName, CharPtr sectionName, CharPtr keyName, CharPtr defaultValue)
{
	if (!IsFileOrDirAccessible(configFileName))
		return SharedStr(defaultValue MG_DEBUG_ALLOCATOR_SRC("GetConfigKeyString"));

	// The section/key/value/filename are UTF-8; use the explicit -W INI API with
	// UTF-8<->wide conversion (matching GetConfigKeyValue above).
	auto sectionNameW  = Utf8_2_wchar(sectionName);
	auto keyNameW      = Utf8_2_wchar(keyName);
	auto defaultValueW = Utf8_2_wchar(defaultValue);
	auto dmsFileNameW  = Utf8_2_wchar(ConvertDmsFileName(configFileName).c_str());

	const UInt32 DEFAULT_BUFFER_SIZE = 300;

	std::unique_ptr<wchar_t[]> heapBuffer;

	wchar_t  stackBuffer[DEFAULT_BUFFER_SIZE];
	wchar_t* buf  = stackBuffer;
	DWORD    size = DEFAULT_BUFFER_SIZE;

	while (true)
	{
		DWORD resSize = GetPrivateProfileStringW(sectionNameW.get(), keyNameW.get(), defaultValueW.get(), buf, size, dmsFileNameW.get());
		if (resSize+2 < size )
			return wchar_2_Utf8Str(buf, resSize);
		size *= 2;
		heapBuffer.reset(new wchar_t[size]);
		buf = heapBuffer.get();
	}
}

void SetConfigKeyString(WeakStr configFileName, CharPtr sectionName, CharPtr keyName, CharPtr keyValue)
{
	MakeDirsForFile(configFileName);

	auto sectionNameW = Utf8_2_wchar(sectionName);
	auto keyNameW     = Utf8_2_wchar(keyName);
	auto keyValueW    = Utf8_2_wchar(keyValue);
	auto dmsFileNameW = Utf8_2_wchar(ConvertDmsFileName(configFileName).c_str());
	WritePrivateProfileStringW(sectionNameW.get(), keyNameW.get(), keyValueW.get(), dmsFileNameW.get());
}

#include <time.h>

Int64 GetSecsSince1970()
{
	return time(0);
}

#include "versionhelpers.h"

namespace PlatformInfo
{
	SharedStr GetVersionStr()
	{
		auto result = SharedStr("Windows ");

		if (!IsWindows7OrGreater())
			result = SharedStr("version before Windows 7, Unsupported");
		else if (!IsWindows10OrGreater())
			result += "version 7 or greater, but not 10";
		else 
			result += "version 10 or greater";
		if (IsWindowsServer())
			result += ", server edition";
		return result;
	}
	SharedStr GetUserNameA()
	{
		DWORD sz = UNLEN+1;
		wchar_t buffer[UNLEN+1];
		if (!::GetUserNameW(buffer, &sz))
			throwLastSystemError("GetUserName");
		return SharedStr(wchar_2_Utf8Str(buffer, sz - 1)); // GetUserNameW includes null in sz
	}
	SharedStr GetComputerNameA()
	{
		DWORD sz = MAX_COMPUTERNAME_LENGTH+1;
		wchar_t buffer[MAX_COMPUTERNAME_LENGTH + 1]; 
		if (!::GetComputerNameW(buffer, &sz))
			throwLastSystemError("GetComputerName");
		return SharedStr(wchar_2_Utf8Str(buffer, sz));
	}
	bool GetEnv(CharPtr varName, SharedStr& result)
	{
		auto varNameW = Utf8_2_wchar(varName);
		const wchar_t* resPtr = _wgetenv(varNameW.get());
		if (!resPtr)
			return false;
		result = wchar_2_Utf8Str(resPtr);
		return true;
	}
	bool GetEnvString(CharPtr section, CharPtr key, SharedStr& result)
	{
		SharedStr varName = mySSPrintF("GEODMS_{}_{}", section, key);
		return GetEnv(varName.c_str(), result);
	}

	SharedStr GetProgramFiles32()
	{
		SharedStr result;
		if (!GetEnv("ProgramFiles(x86)", result))
			GetEnv("ProgramFiles", result);
		return result;
	}
};

#include "VersionComponent.h"

struct WindowsComponent : AbstrVersionComponent {
	void Visit(ClientHandle clientHandle, VersionComponentCallbackFunc callBack, UInt32 componentLevel) const override {
		WCHAR localeName_utf16[LOCALE_NAME_MAX_LENGTH];
		auto sz = GetUserDefaultLocaleName(localeName_utf16, LOCALE_NAME_MAX_LENGTH);
		char localeName_utf8[LOCALE_NAME_MAX_LENGTH * 3];
		WideCharToMultiByte(utf8CP, 0, localeName_utf16, sz, localeName_utf8, LOCALE_NAME_MAX_LENGTH * 3, nullptr, nullptr);

		callBack(clientHandle, componentLevel, mgFormat2string("GetUserDefaultLocaleName(Win32) '{0}'", localeName_utf8).c_str());
		callBack(clientHandle, componentLevel, mgFormat2string("std::locale(\"\"): '{0}'", std::locale("").name().c_str()).c_str());
	}
};

#else //defined(_MSC_VER)

// =====================================================================
// Linux implementations of Environment.h declarations
// =====================================================================

// pthread_getattr_np is a glibc extension (note the _np = "non-portable"
// suffix) — its declaration is only visible when _GNU_SOURCE is set. Other
// libcs (musl) provide it too but might gate it the same way. The define
// below is local to the Linux block.
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cerrno>
#include <climits>
#include <pthread.h>     // pthread_getattr_np / pthread_attr_getstack — for RemainingStackSpace
#include <sys/resource.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <dirent.h>
#include <fcntl.h>
#include <pwd.h>
#include <spawn.h>
#include <strings.h>     // strncasecmp
#include <sys/wait.h>
#include <dlfcn.h>       // dladdr — for GetExeDir self-determination

#include <vector>
#include <map>
#include <mutex>
#include <filesystem>
#include <atomic>

#include "utl/IncrementalLock.h"

// =====================================================================
// platform namespace
// =====================================================================

SharedStr platform::GetSystemErrorText(DWORD lastErr)
{
	return SharedStr(strerror(lastErr));
}

bool platform::isCharPtrAndExceeds_MAX_PATH(CharPtr xFileName)
{
	return strnlen(xFileName, PATH_MAX) >= PATH_MAX;
}

RTC_CALL DWORD platform::GetLastError()
{
	return errno;
}

// =====================================================================
// Stack / Wait / Yield
// =====================================================================

SizeT RemainingStackSpace()
{
	// Real per-thread measurement: ask pthread for the current thread's
	// stack [base, base+size), then return (current_sp - base).
	//
	// The previous implementation returned a fake constant (rl.rlim_cur/2),
	// which on a default 8 MB Linux stack is ~4 MB — always above the
	// 327680 (=320 KB) threshold in TreeItem::UpdateMetaInfo, so the
	// meta-thread baton transfer that #1102 depends on never fired on
	// Linux. Without baton transfer, deeply recursive cfg trees would
	// overflow the main thread's stack instead of handing the metadata
	// work to a worker.
	//
	// pthread_getattr_np reads /proc/self/maps via getline — millisecond
	// cost on every call, which surfaces as massive slowdowns when
	// UpdateMetaInfo runs in tight per-record loops (gpkg writer was
	// 80 min vs 2 min on Windows). Cache the stack base in TLS — it is
	// stable for the thread's lifetime.
	thread_local uintptr_t s_stack_low = 0;
	if (!s_stack_low)
	{
		pthread_attr_t attr;
		if (pthread_getattr_np(pthread_self(), &attr) != 0)
		{
			// Fallback: rlimit-based estimate, halved (preserves previous
			// behaviour if pthread introspection is unavailable for some
			// reason — e.g. very stripped-down libc).
			struct rlimit rl;
			getrlimit(RLIMIT_STACK, &rl);
			return rl.rlim_cur / 2;
		}
		void*  stack_addr = nullptr;
		size_t stack_size = 0;
		pthread_attr_getstack(&attr, &stack_addr, &stack_size);
		pthread_attr_destroy(&attr);
		s_stack_low = reinterpret_cast<uintptr_t>(stack_addr);
	}

	// stack grows downward on x86_64/aarch64; remaining = current_sp - base.
	char stackVar;
	auto current_sp = reinterpret_cast<uintptr_t>(&stackVar);
	if (current_sp <= s_stack_low)
		return 0;
	return current_sp - s_stack_low;
}

void Wait(UInt32 nrMillisecs)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(nrMillisecs));
}

void DmsYield(UInt32 nrMillisecs)
{
	std::this_thread::yield();
	if (nrMillisecs > 0)
		Wait(nrMillisecs);
}

bool ManageSystemError(UInt32& retryCounter, CharPtr format, CharPtr fileName, bool throwOnError, bool doRetry)
{
	int lastErr = errno;
	if (doRetry && (lastErr == EACCES || lastErr == EBUSY))
	{
		if (++retryCounter <= 10)
		{
			UInt32 nrWaitSecs = (1 << retryCounter);
			reportF(SeverityTypeID::ST_MajorTrace,
				"System Error {}:\nErrorCode {}: {}\nWaiting {} seconds before retry #{}",
				mySSPrintF(format, fileName).c_str(),
				lastErr,
				strerror(lastErr),
				nrWaitSecs,
				retryCounter
			);
			Wait(1000 * nrWaitSecs);
			return true;
		}
	}
	if (throwOnError)
		throwSystemError(lastErr, format, fileName);
	return false;
}

// =====================================================================
// Main Window Handle (stub on Linux)
// =====================================================================

static void* s_GlobalMainWindow = nullptr;

RTC_CALL void* GetGlobalMainWindowHandle() { return s_GlobalMainWindow; }

RTC_CALL void* SetGlobalMainWindowHandle(void* hWindow)
{
	auto oldHandle = s_GlobalMainWindow;
	s_GlobalMainWindow = hWindow;
	return oldHandle;
}

// =====================================================================
// Message Queue (stubs on Linux - no GUI message loop)
// =====================================================================

std::atomic<UInt32> g_DispatchLockCount = 0;

bool HasWaitingMessages() { return false; }

extern "C" RTC_CALL bool DMS_CONV DMS_HasWaitingMessages() { return false; }

// =====================================================================
// File Path Utilities
// =====================================================================

bool IsDosFileOrDirAccessible(CharPtr dosFileOrDirName)
{
	return access(dosFileOrDirName, F_OK) == 0;
}

SharedStr GetCurrentDir()
{
	char buf[PATH_MAX];
	if (!getcwd(buf, sizeof(buf)))
		throwErrorD("Environment", "getcwd failed");
	return ConvertDosFileName(SharedStr(buf));
}

void SetCurrentDir(CharPtr dir)
{
	if (chdir(dir) != 0)
		throwErrorD("Environment", "chdir failed");
}

RTC_CALL void DMS_CONV DMS_Appl_SetFont()
{
	// No-op on Linux (no Windows font resource loading)
}

static SharedStr GetExeDirImpl()
{
	// dladdr on an address in this shared object yields its own path; the
	// GeoDms binaries + bundled resources live in that directory.
	Dl_info info;
	if (dladdr(reinterpret_cast<const void*>(&GetExeDir), &info) && info.dli_fname)
		return splitFullPath(ConvertDosFileName(SharedStr(info.dli_fname)).c_str());
	return SharedStr();
}

RTC_CALL SharedStr GetExeDir()
{
	static SharedStr s_exeDir = GetExeDirImpl();
	assert(!s_exeDir.empty());
	return s_exeDir;
}

SharedStr ConvertDosFileName(WeakStr fileName)
{
	// On Linux, '/' is already the native delimiter; just return as-is
	SharedStr result(fileName);
	// Still handle backslashes from cross-platform config files
	auto range = result.GetAsMutableRange();
	while (range.first != range.second)
	{
		if (*range.first == '\\')
			*range.first = '/';
		++range.first;
	}
	return result;
}

SharedStr ConvertDmsFileNameAlways(SharedStr&& path)
{
	// On Linux, no conversion needed (/ is native)
	return std::move(path);
}

SharedStr ConvertDmsFileName(WeakStr path)
{
	if (path.empty())
		return path;
	// Strip file: prefix if present
	SharedStr stripped = !strncmp(path.begin(), "file:", 5)
		? SharedStr(CharPtrRange(path.begin() + 5, path.send()))
		: SharedStr(path);

	// POSIX path resolution rejects `..` after a non-directory component
	// (e.g. `cfg/Regression_test/../main/Units.dms` where `Regression_test`
	// is the .dms-file stem, not a real directory). Windows pathname
	// resolution is more forgiving — match it by lexically normalising
	// `<dir>/<name>/../<rest>` to `<dir>/<rest>` before any access()/open()
	// call. Skip when there is nothing to collapse so the common path stays
	// a no-op.
	if (std::strstr(stripped.c_str(), "/../") == nullptr
	 && std::strstr(stripped.c_str(), "/./") == nullptr)
		return stripped;
	auto norm = std::filesystem::path(stripped.c_str()).lexically_normal().string();
	return SharedStr(norm.c_str() MG_DEBUG_ALLOCATOR_SRC("ConvertDmsFileName-norm"));
}

void ReplaceSpecificDelimiters(MutableCharPtrRange range, const char delimiter)
{
	while (range.first != range.second)
	{
		if (*range.first == delimiter)
			*range.first = '/';
		++range.first;
	}
}

bool HasDosDelimiters(CharPtr source)
{
	assert(source);
	while (*source)
	{
		if (*source == '\\')
			return true;
		++source;
	}
	return false;
}

bool HasDosDelimiters(CharPtrRange source)
{
	while (source.first != source.second)
	{
		if (*source.first == '\\')
			return true;
		++source.first;
	}
	return false;
}

bool IsRelative(CharPtr source)
{
	assert(source);
	while (*source)
	{
		if (*source == '/' || *source == ':' || *source == '%')
			return false;
		++source;
	}
	return true;
}

// =====================================================================
// Directory / File Operations
// =====================================================================

void MakeDir(WeakStr dirName)
{
	auto path = ConvertDmsFileName(dirName);
	if (mkdir(path.c_str(), 0755) != 0)
	{
		if (errno == EEXIST)
			return;
		throwLastSystemError("MakeDir('{}')", dirName.c_str());
	}
}

void MakeDirsForFileImpl(WeakStr fullFileName)
{
	SharedStr pathStr = splitFullPath(fullFileName.c_str());
	if (IsFileOrDirAccessible(pathStr))
		return;
	if (pathStr.empty())
		return;
	char ch = pathStr.send()[-1];
	if (ch == ':' || ch == '/')
		return;
	MakeDirsForFileImpl(pathStr);
	MakeDir(pathStr);
}

void MakeDirsForFile(WeakStr fullFileName)
{
	assert(fullFileName.c_str());
	MakeDirsForFileImpl(fullFileName);
}

extern "C" RTC_CALL void DMS_CONV DMS_MakeDirsForFile(CharPtr fileName)
{
	MakeDirsForFile(ConvertDosFileName(SharedStr(fileName)));
}

bool IsFileOrDirAccessible(WeakStr fileOrDirName)
{
	return access(ConvertDmsFileName(fileOrDirName).c_str(), F_OK) == 0;
}

bool IsFileOrDirWritable(WeakStr fileOrDirName)
{
	return access(ConvertDmsFileName(fileOrDirName).c_str(), W_OK) == 0;
}

void GetWritePermission(WeakStr fileName)
{
	if (IsFileOrDirAccessible(fileName))
	{
		if (!IsFileOrDirWritable(fileName))
			throwErrorF("FileSystem", "Write permission for '{}' denied", fileName);
	}
	else
		MakeDirsForFile(fileName);
}

// =====================================================================
// FindFileBlock (uses opendir/readdir on Linux)
// =====================================================================

#include <fnmatch.h>

struct FindFileBlockData
{
	DIR* dir = nullptr;
	struct dirent* entry = nullptr;
	SharedStr dirPath;
	SharedStr pattern;
	SharedStr currentFullPath;
};

FindFileBlock::FindFileBlock(WeakStr fileSearchSpec)
	: m_Data(new Byte[sizeof(FindFileBlockData)])
	, m_Handle(nullptr)
{
	auto* data = new (m_Data.get()) FindFileBlockData();
	SharedStr spec(fileSearchSpec);
	// Split into directory and pattern
	auto lastSlash = spec.send();
	for (auto p = spec.begin(); p != spec.send(); ++p)
		if (*p == '/' || *p == '\\')
			lastSlash = p;

	if (lastSlash != spec.send())
	{
		data->dirPath = SharedStr(CharPtrRange(spec.begin(), lastSlash));
		data->pattern = SharedStr(CharPtrRange(lastSlash + 1, spec.send()));
	}
	else
	{
		data->dirPath = SharedStr(".");
		data->pattern = spec;
	}

	data->dir = opendir(data->dirPath.c_str());
	if (!data->dir)
	{
		m_Handle = reinterpret_cast<HANDLE>(static_cast<intptr_t>(-1));
		return;
	}
	m_Handle = data->dir;
	// Advance to first matching entry
	if (!Next())
		m_Handle = reinterpret_cast<HANDLE>(static_cast<intptr_t>(-1));
}

FindFileBlock::FindFileBlock(FindFileBlock&& src) noexcept
	: m_Data(std::move(src.m_Data))
	, m_Handle(src.m_Handle)
{
	src.m_Handle = reinterpret_cast<HANDLE>(static_cast<intptr_t>(-1));
}

FindFileBlock::~FindFileBlock() noexcept
{
	if (IsValid())
	{
		auto* data = reinterpret_cast<FindFileBlockData*>(m_Data.get());
		if (data->dir)
			closedir(data->dir);
		data->~FindFileBlockData();
	}
}

bool FindFileBlock::IsValid() const
{
	return m_Handle != reinterpret_cast<HANDLE>(static_cast<intptr_t>(-1)) && m_Handle != nullptr;
}

DWORD FindFileBlock::GetFileAttr() const
{
	assert(IsValid());
	auto* data = reinterpret_cast<const FindFileBlockData*>(m_Data.get());
	struct stat st;
	if (stat(data->currentFullPath.c_str(), &st) != 0)
		return 0;
	// Emulate FILE_ATTRIBUTE_DIRECTORY
	return S_ISDIR(st.st_mode) ? 0x10 : 0; // FILE_ATTRIBUTE_DIRECTORY = 0x10
}

bool FindFileBlock::IsDirectory() const
{
	return GetFileAttr() & 0x10;
}

CharPtr FindFileBlock::GetCurrFileName() const
{
	auto* data = reinterpret_cast<const FindFileBlockData*>(m_Data.get());
	return data->entry ? data->entry->d_name : "";
}

bool FindFileBlock::Next()
{
	auto* data = reinterpret_cast<FindFileBlockData*>(m_Data.get());
	if (!data->dir)
		return false;
	while ((data->entry = readdir(data->dir)) != nullptr)
	{
		if (fnmatch(data->pattern.c_str(), data->entry->d_name, 0) == 0)
		{
			data->currentFullPath = DelimitedConcat(data->dirPath.c_str(), data->entry->d_name);
			return true;
		}
	}
	return false;
}

// =====================================================================
// FileDateTime
// =====================================================================

FileDateTime AsFileDateTime(UInt32 hiDW, UInt32 loDW)
{
	return (static_cast<UInt64>(hiDW) << 32) | loDW;
}

FileDateTime GetFileOrDirDateTime(WeakStr fileOrDirName)
{
	struct stat st;
	SharedStr path = ConvertDmsFileName(fileOrDirName);
	if (stat(path.c_str(), &st) != 0)
		return 0;
	// Convert timespec to 100-nanosecond intervals (Windows FILETIME-compatible)
	return static_cast<FileDateTime>(st.st_mtime) * 10000000ULL + 116444736000000000ULL;
}

SharedStr AsDateTimeString(FileDateTime t64)
{
	// Convert from Windows FILETIME to Unix time
	if (t64 < 116444736000000000ULL)
		return SharedStr("unknown");
	time_t unixTime = (t64 - 116444736000000000ULL) / 10000000ULL;
	struct tm tm;
	localtime_r(&unixTime, &tm);
	return mySSPrintF("{:04}/{:02}/{:02}  {:02}:{:02}:{:02}",
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec
	);
}

// =====================================================================
// Copy / Move / Kill File Operations
// =====================================================================

void CopyFileOrDir(CharPtr srcFileOrDirName, CharPtr destFileOrDirName, bool mayBeMissing)
{
	try {
		std::filesystem::copy(srcFileOrDirName, destFileOrDirName,
			std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing);
	} catch (const std::filesystem::filesystem_error& e) {
		if (!mayBeMissing)
			throwErrorF("FileSystem", "CopyFileOrDir({}, {}) failed: {}", srcFileOrDirName, destFileOrDirName, e.what());
	}
}

bool MoveFileOrDir(CharPtr srcFileOrDirName, CharPtr destFileOrDirName, bool mayBeMissing)
{
	std::error_code ec;
	std::filesystem::rename(srcFileOrDirName, destFileOrDirName, ec);
	if (ec)
	{
		if (!mayBeMissing)
			throwErrorF("FileSystem", "MoveFileOrDir({}, {}) failed: {}", srcFileOrDirName, destFileOrDirName, ec.message().c_str());
		return false;
	}
	return true;
}

bool KillFileOrDir(WeakStr fileOrDirName, bool canBeDir)
{
	std::error_code ec;
	auto path = std::filesystem::path(ConvertDmsFileName(fileOrDirName).c_str());
	if (!std::filesystem::exists(path, ec))
		return true;
	if (canBeDir && std::filesystem::is_directory(path, ec))
		std::filesystem::remove_all(path, ec);
	else
		std::filesystem::remove(path, ec);
	return !ec;
}

// =====================================================================
// Child Process
// =====================================================================

start_process_result_t StartChildProcess(CharPtr moduleName, Char* cmdLine)
{
	pid_t pid;
	int status;

	// On Linux, use /bin/sh -c for command interpretation when:
	// - no module specified (nullptr/empty), or
	// - module is a Windows command shell reference (e.g. "cmd.exe", "env:ComSpec")
	bool useShell = !moduleName || !*moduleName
		|| strstr(moduleName, "cmd") != nullptr
		|| strstr(moduleName, "ComSpec") != nullptr;

	// Also use shell mode when module is explicitly a Unix shell
	if (!useShell && moduleName)
	{
		CharPtr base = strrchr(moduleName, '/');
		base = base ? base + 1 : moduleName;
		if (strcmp(base, "sh") == 0 || strcmp(base, "bash") == 0 || strcmp(base, "dash") == 0)
			useShell = true;
	}

	if (useShell)
	{
		CharPtr shell = (moduleName && *moduleName && strstr(moduleName, "cmd") == nullptr && strstr(moduleName, "ComSpec") == nullptr)
			? moduleName : "/bin/sh";

		// Strip Windows cmd.exe "/c " or Unix shell "-c " prefixes if present,
		// since we pass -c as a separate argv element to the shell.
		const char* shellCmd = cmdLine ? cmdLine : "";
		if (strncmp(shellCmd, "/c ", 3) == 0 || strncmp(shellCmd, "/C ", 3) == 0)
			shellCmd = shellCmd + 3;
		else if (strncmp(shellCmd, "-c ", 3) == 0)
			shellCmd = shellCmd + 3;

		char* argv[] = { const_cast<char*>(shell), const_cast<char*>("-c"), const_cast<char*>(shellCmd), nullptr };
		status = posix_spawn(&pid, shell, nullptr, nullptr, argv, environ);
		if (status != 0)
			throwErrorF("Environment", "posix_spawn({} -c '{}') failed: {}", shell, shellCmd, strerror(status));
	}
	else
	{
		char* argv[] = { const_cast<char*>(moduleName), cmdLine, nullptr };
		status = posix_spawn(&pid, moduleName, nullptr, nullptr, argv, environ);
		if (status != 0)
			throwErrorF("Environment", "posix_spawn({}) failed: {}", moduleName, strerror(status));
	}

	// Return pid in the HANDLE pair (process, thread=0 on Linux)
	return { reinterpret_cast<HANDLE>(static_cast<intptr_t>(pid)), nullptr };
}

DWORD ExecuteChildProcess(CharPtr moduleName, Char* cmdLine)
{
	auto result = StartChildProcess(moduleName, cmdLine);
	pid_t pid = static_cast<pid_t>(reinterpret_cast<intptr_t>(result.first));
	int status;
	waitpid(pid, &status, 0);
	return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

// =====================================================================
// Registry / Config — INI-file backend for Linux
// =====================================================================
// All GeoDMS settings are stored in ~/.config/geodms/geodms.ini.
// Format:
//   [section]
//   key=value
//
// GetGeoDmsRegKey* / SetGeoDmsRegKey* use the fixed section [GeoDMS].
// GetConfigKey* / SetConfigKey* use per-file sections.
// =====================================================================

static std::map<SharedStr, SharedStr> s_SessionLocalOverrides;
static std::mutex s_SessionLocalMutex;

// --------------- INI file helpers ------------------------------------

static SharedStr GetGeoDmsConfigDir()
{
    const char* xdg = getenv("XDG_CONFIG_HOME");
    SharedStr base = xdg && *xdg ? SharedStr(xdg) : mySSPrintF("{}/.config", getenv("HOME") ? getenv("HOME") : "/tmp");
    return base + "/geodms";
}

static SharedStr GetGeoDmsIniPath()
{
    return GetGeoDmsConfigDir() + "/geodms.ini";
}

// Simple in-memory INI cache: section -> (key -> value)
using IniData = std::map<std::string, std::map<std::string, std::string>>;

// Function-local, NOT namespace-scope. std::map is dynamically initialised, and this cache is
// read during static initialisation of other translation units -- every static GetTokenID_st()
// goes through EventLog_HideDepreciatedCaseMixupWarnings() -> GetRegStatusFlags() -> IniGet(),
// and the allocator reaches it too. Static init order ACROSS translation units is unspecified,
// so a namespace-scope std::map here was routinely used before its constructor had run: the
// red-black tree walk dereferenced garbage and SIGSEGV'd. That killed every GeoDmsRun and
// GeoDmsGuiQt invocation on Linux at startup (20.11.0: exit 139 on all unit tests); Windows was
// unaffected only because RTC_GetRegDWord reads the real registry there instead of this file.
//
// A function-local static is constructed on first use, whenever that is, which is exactly the
// guarantee the namespace-scope version lacked. std::mutex is left as-is deliberately: its
// default constructor is constexpr, so it is constant-initialised and safe before any dynamic
// initialisation runs.
static std::mutex s_IniMutex;

static IniData& IniDataRef()
{
    static IniData s_IniData;
    return s_IniData;
}

static bool& IniLoadedRef()
{
    static bool s_IniLoaded = false;
    return s_IniLoaded;
}

static void EnsureIniDirExists()
{
    auto dir = GetGeoDmsConfigDir();
    // mkdir -p equivalent: create each component
    std::string path;
    for (const char* p = dir.c_str(); *p; ++p)
    {
        path += *p;
        if (*p == '/')
            mkdir(path.c_str(), 0755);
    }
    mkdir(dir.c_str(), 0755);
}

static void LoadIni_Locked()
{
    if (IniLoadedRef()) return;
    IniLoadedRef() = true;

    auto path = GetGeoDmsIniPath();
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return;

    char line[4096];
    std::string section;
    while (fgets(line, sizeof(line), f))
    {
        // strip trailing newline
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = 0;

        const char* p = line;
        // skip leading whitespace
        while (*p == ' ' || *p == '\t') ++p;
        if (!*p || *p == ';' || *p == '#') continue; // comment / blank

        if (*p == '[')
        {
            const char* end = strchr(p + 1, ']');
            if (end) section = std::string(p + 1, end);
        }
        else
        {
            const char* eq = strchr(p, '=');
            if (eq && !section.empty())
            {
                std::string key(p, eq);
                std::string val(eq + 1);
                IniDataRef()[section][key] = val;
            }
        }
    }
    fclose(f);
}

static void SaveIni_Locked()
{
    EnsureIniDirExists();
    auto path = GetGeoDmsIniPath();
    FILE* f = fopen(path.c_str(), "w");
    if (!f) return;
    for (auto& [sec, kv] : IniDataRef())
    {
        fprintf(f, "[%s]\n", sec.c_str());
        for (auto& [k, v] : kv)
            fprintf(f, "%s=%s\n", k.c_str(), v.c_str());
        fprintf(f, "\n");
    }
    fclose(f);
}

static std::string IniGet(const char* section, const char* key, const char* defaultValue = "")
{
    std::lock_guard lock(s_IniMutex);
    LoadIni_Locked();
    auto si = IniDataRef().find(section);
    if (si == IniDataRef().end()) return defaultValue;
    auto ki = si->second.find(key);
    if (ki == si->second.end()) return defaultValue;
    return ki->second;
}

static void IniSet(const char* section, const char* key, const char* value)
{
    std::lock_guard lock(s_IniMutex);
    LoadIni_Locked();
    IniDataRef()[section][key] = value;
    SaveIni_Locked();
}

RTC_CALL void SetSessionLocalOverride(CharPtr key, CharPtr value)
{
	std::lock_guard lock(s_SessionLocalMutex);
	s_SessionLocalOverrides[SharedStr(key)] = SharedStr(value);
}

RTC_CALL void ClearSessionLocalOverride(CharPtr key)
{
	std::lock_guard lock(s_SessionLocalMutex);
	s_SessionLocalOverrides.erase(SharedStr(key));
}

RTC_CALL bool HasSessionLocalOverride(CharPtr key)
{
	std::lock_guard lock(s_SessionLocalMutex);
	return s_SessionLocalOverrides.contains(SharedStr(key));
}

RTC_CALL SharedStr GetSessionLocalOverride(CharPtr key)
{
	std::lock_guard lock(s_SessionLocalMutex);
	auto it = s_SessionLocalOverrides.find(SharedStr(key));
	if (it != s_SessionLocalOverrides.end())
		return it->second;
	return SharedStr();
}

RTC_CALL SharedStr GetGeoDmsRegKey(CharPtr key)
{
	// Check session-local overrides first
	{
		std::lock_guard lock(s_SessionLocalMutex);
		auto it = s_SessionLocalOverrides.find(SharedStr(key));
		if (it != s_SessionLocalOverrides.end())
			return it->second;
	}
	auto val = IniGet("GeoDMS", key);
	return SharedStr(val.c_str());
}

RTC_CALL auto GetGeoDmsRegKeyMultiString(CharPtr key) -> std::vector<SharedStr>
{
	SharedStr val = GetGeoDmsRegKey(key);
	if (val.empty())
		return {};
	// Values are stored as NUL-separated; on Linux we write ';'-separated
	std::vector<SharedStr> result;
	const char* p = val.c_str();
	const char* start = p;
	while (*p)
	{
		if (*p == ';')
		{
			result.emplace_back(CharPtrRange(start, p));
			start = p + 1;
		}
		++p;
	}
	if (start != p)
		result.emplace_back(CharPtrRange(start, p));
	return result;
}

RTC_CALL bool SetGeoDmsRegKeyDWord(CharPtr key, DWORD dw, CharPtr section)
{
	char buf[32];
	snprintf(buf, sizeof(buf), "%u", (unsigned)dw);
	IniSet("GeoDMS", key, buf);
	return true;
}

RTC_CALL DWORD GetGeoDmsRegKeyDWord(CharPtr key, DWORD defaultValue, CharPtr section)
{
	auto val = IniGet("GeoDMS", key);
	if (val.empty()) return defaultValue;
	return (DWORD)strtoul(val.c_str(), nullptr, 10);
}

RTC_CALL bool SetGeoDmsRegKeyString(CharPtr key, CharPtr str)
{
	IniSet("GeoDMS", key, str ? str : "");
	return true;
}

RTC_CALL bool SetGeoDmsRegKeyMultiString(CharPtr key, const std::vector<SharedStr>& strings)
{
	// Store as ';'-separated (NUL-separated on Win32, but ';' is safe on Linux)
	std::string val;
	for (size_t i = 0; i < strings.size(); ++i)
	{
		if (i > 0) val += ";";
		val += strings[i].c_str();
	}
	IniSet("GeoDMS", key, val.c_str());
	return true;
}

SharedStr GetConvertedGeoDmsRegKey(CharPtr key)
{
	SharedStr result;
	if (!PlatformInfo::GetEnvString("directories", key, result))
		result = GetGeoDmsRegKey(key);
	return result;
}

SharedStr GetLocalDataDirImpl()
{
	SharedStr localDataDir = GetConvertedGeoDmsRegKey("LocalDataDir");
	if (localDataDir.empty())
		localDataDir = "/tmp/geodms/LocalData";
	return localDataDir;
}

RTC_CALL SharedStr GetLocalDataDir()
{
	static SharedStr localDataDir = GetLocalDataDirImpl();
	return localDataDir;
}

SharedStr GetSourceDataDirImpl()
{
	SharedStr sourceDataDir = GetConvertedGeoDmsRegKey("SourceDataDir");
	if (sourceDataDir.empty())
		sourceDataDir = "/tmp/geodms/SourceData";
	return sourceDataDir;
}

SharedStr GetSourceDataDir()
{
	static SharedStr sourceDataDir = GetSourceDataDirImpl();
	return sourceDataDir;
}

// =====================================================================
// Status Flags (same logic, env-var backed on Linux)
// =====================================================================

UInt32 g_RegStatusFlags = 0;
UInt32 g_OvrStatusFlags = 0;
UInt32 g_OvrStatusMask  = 0;

// Order-safe accessor for the registry-access section. This section is reached
// from DYNAMIC INITIALIZATION: a namespace-scope TokenID initializer (e.g.
// token::UInt32 in tic/LispTreeType.cpp) creates a token, IndexedStrings then
// calls EventLog_HideDepreciatedCaseMixupWarnings, which reads the registry
// status flags. A namespace-scope section would then be used before this TU's
// own initializers have run -- on GCC that order is link-order dependent, and
// the Linux Debug build aborted at startup on EnterLevel's level != 0 assert
// (in Release the assert is compiled out, so the same disorder passed
// unnoticed). A function-local static is constructed on first use, so the
// order no longer matters. See also the ThousandSeparator note in
// dbg/MsgDispatch.cpp, which worked around this same fragility at a call site.
static leveled_critical_section& RegAccessSection()
{
	static leveled_critical_section s_RegAccess(item_level_type(0), ord_level_type::RegisterAccess, "RegisterAccess");
	return s_RegAccess;
}

RTC_CALL void DMS_Appl_SetRegStatusFlags(UInt32 newSF)
{
	leveled_critical_section::scoped_lock lock(RegAccessSection());
	g_RegStatusFlags = (newSF | RSF_WasRead);
}

UInt32 ReadOnceRegisteredStatusFlags()
{
	if (g_RegStatusFlags & RSF_WasRead)
		return g_RegStatusFlags;

	leveled_critical_section::scoped_lock lock(RegAccessSection());
	if (g_RegStatusFlags & RSF_WasRead)
		return g_RegStatusFlags;

	g_RegStatusFlags |= RSF_WasRead;
	DWORD val = GetGeoDmsRegKeyDWord("StatusFlags", 0);
	if (val)
		g_RegStatusFlags |= val;
	else
		g_RegStatusFlags |= RSF_Default;
	return g_RegStatusFlags;
}

RTC_CALL UInt32 GetRegStatusFlags()
{
	auto registeredFlags = ReadOnceRegisteredStatusFlags();
	return (registeredFlags & ~(g_OvrStatusMask | RSF_WasRead)) | (g_OvrStatusFlags & g_OvrStatusMask);
}

RTC_CALL UInt32 DMS_Appl_GetRegStatusFlags()
{
	return GetRegStatusFlags();
}

RTC_CALL void SetCachedStatusFlag(UInt32 newSF, bool newVal)
{
	leveled_critical_section::scoped_lock lock(RegAccessSection());
	g_OvrStatusMask |= newSF;
	if (newVal) g_OvrStatusFlags |= newSF;
	else        g_OvrStatusFlags &= ~newSF;
}

void SetRegStatusFlags(UInt32 newSF)
{
	SetGeoDmsRegKeyDWord("StatusFlags", newSF);
	DMS_Appl_SetRegStatusFlags(newSF);
}

RTC_CALL void SetStatusFlag(UInt32 newSF, bool newVal)
{
	leveled_critical_section::scoped_lock lock(RegAccessSection());
	g_OvrStatusMask |= newSF;
	if (newVal) g_OvrStatusFlags |= newSF;
	else        g_OvrStatusFlags &= ~newSF;

	auto sf = ReadOnceRegisteredStatusFlags();
	if (newVal) sf |= newSF;
	else        sf &= ~newSF;
	sf &= ~RSF_WasRead;
	SetGeoDmsRegKeyDWord("StatusFlags", sf);
	g_RegStatusFlags = (sf | RSF_WasRead);
}

RTC_CALL bool IsInDebugMode()       { return GetRegStatusFlags() & RSF_DebugMode; }
RTC_CALL bool IsMultiThreaded0()    { return GetRegStatusFlags() & RSF_SuspendForGUI; }
RTC_CALL bool IsMultiThreaded1()    { return GetRegStatusFlags() & RSF_MultiThreading1; }
RTC_CALL bool IsMultiThreaded2()    { return GetRegStatusFlags() & RSF_MultiThreading2; }
RTC_CALL bool IsMultiThreaded3()    { return GetRegStatusFlags() & RSF_MultiThreading3; }
bool IsMultiThreaded1or2() { return GetRegStatusFlags() & (RSF_MultiThreading1 | RSF_MultiThreading2); }
bool HasDynamicROI()       { return GetRegStatusFlags() & RSF_DynamicROI; }
RTC_CALL bool ShowThousandSeparator() { return GetRegStatusFlags() & RSF_ShowThousandSeparator; }
bool EventLog_HideDepreciatedCaseMixupWarnings() { return GetRegStatusFlags() & RSF_EventLog_HideDepreciated; }

extern "C" RTC_CALL bool DMS_CONV RTC_ParseRegStatusFlag(const char* param)
{
	assert(param);
	if (param[0] != '/') return false;
	char cmd = param[1];
	if (cmd != 'S' && cmd != 'C') return false;
	bool newValue = (cmd == 'S');
	switch (param[2])
	{
		case 'A': SetCachedStatusFlag(RSF_AdminMode, newValue); break;
		case 'C': SetCachedStatusFlag(RSF_ShowStateColors, newValue); break;
		case 'V': SetCachedStatusFlag(RSF_TreeViewVisible, newValue); break;
		case 'D': SetCachedStatusFlag(RSF_DetailsVisible, newValue); break;
		case 'E': SetCachedStatusFlag(RSF_EventLogVisible, newValue); break;
		case 'T': SetCachedStatusFlag(RSF_ToolBarVisible, newValue); break;
		case 'I': SetCachedStatusFlag(RSF_CurrentItemBarHidden, newValue); break;
		case 'M': SetCachedStatusFlag(RSF_DebugMode, newValue); break;
		case 'R': SetCachedStatusFlag(RSF_DynamicROI, newValue); break;
		case 'S': case '0': SetCachedStatusFlag(RSF_SuspendForGUI, newValue); break;
		case '1': SetCachedStatusFlag(RSF_MultiThreading1, newValue); break;
		case '2': SetCachedStatusFlag(RSF_MultiThreading2, newValue); break;
		case '3': SetCachedStatusFlag(RSF_MultiThreading3, newValue); break;
		case 'H': SetCachedStatusFlag(RSF_ShowThousandSeparator, newValue); break;
		case 'P': SetPerformanceLogging(newValue); break; // not a status flag: that DWORD is out of bits
		case 'Q': SetResourceScheduling(newValue ? resource_scheduling::enforce : resource_scheduling::off); break;
		case 'q': SetResourceScheduling(newValue ? resource_scheduling::shadow : resource_scheduling::off); break;
		// /SB<MB> caps the scheduler's admission budget for this run; /CB restores the derived one.
		// A value-taking switch, like /L: everything after the 'B' is the number.
		case 'B': RTC_SetCachedDWord(RegDWordEnum::SchedulerBudgetMB, newValue ? DWORD(atoi(param + 3)) : 0); break;
		// /CF switches OFF the free-store drainage that otherwise starts once RAM use passes
		// MemoryFlushThreshold; /SF restores the default. 'F' as in Free-store: 'D' is DetailsVisible.
		case 'F': RTC_SetCachedDWord(RegDWordEnum::MemoryDrainage, newValue ? 1 : 0);
		          SetFreeStackDrainageEnabled(newValue); break;
		case 'W': SetCachedStatusFlag(RSF_EventLog_HideDepreciated, !newValue); break;
		default:
			reportF(SeverityTypeID::ST_Warning, "Unrecognised command line {} option {}", (newValue ? "Set" : "Clear"), param);
			return true;
	}
	return true;
}

RTC_CALL void ParseRegStatusFlags(int& argc, char**& argv)
{
	while (argc)
	{
		if (!RTC_ParseRegStatusFlag(argv[0]))
			return;
		++argv;
		--argc;
	}
}

// =====================================================================
// RegDWord
// =====================================================================

struct RegDWordAttr { CharPtr key; DWORD value; bool wasRead; };

static RegDWordAttr s_RegDWordAttrs[] =
{
	{ "MemoryFlushThreshold", 80, false },
	{ "SwapFileMinSize", 0, false },
	{ "DrawingSizeInPixels", 0, false },
	{ "MemoryMaxRAM_GB", 64, false }, // simulates a smaller machine; also throttles operation activation via IsLowOnFreeRAM
	{ "PerformanceLogging", 0, false },
	{ "ResourceAwareScheduling", 0, false }, // OFF by default (0=off, 1=shadow, 2=enforce). Switched on
	                                        // with /Sq or /SQ, or the q/Q boxes under Settings >
	                                        // Local machine options > Parallel Processing.
	                                        // Off because enforce does not yet pay for itself: measured
	                                        // on t641_2 it parked 124 184 operations and still left the
	                                        // live peak at 171.9 GiB -- identical to the run without it
	                                        // (doc SS8.1.33). Budget = MemoryFlushThreshold % of allowed
	                                        // RAM -- the same threshold that triggers MemoryDrainage --
	                                        // unless SchedulerBudgetMB (/SB<MB>) overrides it.
	{ "SchedulerBudgetMB", 0, false },
	{ "MemoryDrainage", 1, false } // on by default; the trigger is MemoryFlushThreshold (doc SS8.1.32)
};

extern "C" RTC_CALL DWORD RTC_GetRegDWord(RegDWordEnum i)
{
	auto ui = UInt32(i);
	MG_CHECK(ui < sizeof(s_RegDWordAttrs) / sizeof(RegDWordAttr));
	leveled_critical_section::scoped_lock lock(RegAccessSection());
	RegDWordAttr& regAttr = s_RegDWordAttrs[ui];
	if (!regAttr.wasRead)
	{
		regAttr.wasRead = true;
		DWORD val = GetGeoDmsRegKeyDWord(regAttr.key, regAttr.value);
		regAttr.value = val;
	}
	return regAttr.value;
}

extern "C" RTC_CALL void RTC_SetCachedDWord(RegDWordEnum i, DWORD dw)
{
	auto ui = UInt32(i);
	assert(ui < sizeof(s_RegDWordAttrs) / sizeof(RegDWordAttr));
	leveled_critical_section::scoped_lock lock(RegAccessSection());
	s_RegDWordAttrs[ui].wasRead = true;
	s_RegDWordAttrs[ui].value = dw;
}

// =====================================================================
// Config files (use simple file I/O on Linux)
// =====================================================================

// Config key storage: use "<configFile>/<sectionName>" as the INI section,
// so each config file gets its own namespace.
static std::string ConfigSection(WeakStr configFileName, CharPtr sectionName)
{
	std::string sec = "cfg:";
	sec += getFileNameBase(configFileName.c_str()).c_str();
	sec += "/";
	sec += sectionName;
	return sec;
}

Int32 GetConfigKeyValue(WeakStr configFileName, CharPtr sectionName, CharPtr keyName, Int32 defaultValue)
{
	auto sec = ConfigSection(configFileName, sectionName);
	auto val = IniGet(sec.c_str(), keyName);
	if (val.empty()) return defaultValue;
	return (Int32)strtol(val.c_str(), nullptr, 10);
}

SharedStr GetConfigKeyString(WeakStr configFileName, CharPtr sectionName, CharPtr keyName, CharPtr defaultValue)
{
	auto sec = ConfigSection(configFileName, sectionName);
	auto val = IniGet(sec.c_str(), keyName, defaultValue ? defaultValue : "");
	return SharedStr(val.c_str());
}

void SetConfigKeyString(WeakStr configFileName, CharPtr sectionName, CharPtr keyName, CharPtr keyValue)
{
	auto sec = ConfigSection(configFileName, sectionName);
	IniSet(sec.c_str(), keyName, keyValue ? keyValue : "");
}

#include <time.h>

Int64 GetSecsSince1970()
{
	return time(nullptr);
}

// =====================================================================
// PlatformInfo
// =====================================================================

namespace PlatformInfo
{
	SharedStr GetVersionStr()
	{
		struct utsname buf;
		if (uname(&buf) == 0)
			return mySSPrintF("Linux {} {}", buf.release, buf.machine);
		return SharedStr("Linux (unknown version)");
	}

	SharedStr GetUserNameA()
	{
		const char* user = getenv("USER");
		if (user) return SharedStr(user);
		struct passwd* pw = getpwuid(getuid());
		if (pw) return SharedStr(pw->pw_name);
		return SharedStr("unknown");
	}

	SharedStr GetComputerNameA()
	{
		char hostname[256];
		if (gethostname(hostname, sizeof(hostname)) == 0)
			return SharedStr(hostname);
		return SharedStr("unknown");
	}

	bool GetEnv(CharPtr varName, SharedStr& result)
	{
		const char* val = getenv(varName);
		if (val)
		{
			result = SharedStr(val);
			return true;
		}
		// Linux env vars are case-sensitive; Windows env vars are not.
		// Test harnesses (e.g. tst/batch/full.py) sometimes set var names in
		// a different case than what callers look up — match Windows
		// behaviour by walking environ for a case-insensitive hit.
		size_t nameLen = std::strlen(varName);
		for (char** envp = environ; envp && *envp; ++envp)
		{
			const char* eq = std::strchr(*envp, '=');
			if (!eq) continue;
			if (size_t(eq - *envp) != nameLen) continue;
			if (strncasecmp(*envp, varName, nameLen) == 0)
			{
				result = SharedStr(eq + 1);
				return true;
			}
		}
		return false;
	}

	bool GetEnvString(CharPtr section, CharPtr key, SharedStr& result)
	{
		SharedStr varName = mySSPrintF("GEODMS_{}_{}", section, key);
		return GetEnv(varName.c_str(), result);
	}

	SharedStr GetProgramFiles32()
	{
		return SharedStr("/usr/local");
	}
}

// =====================================================================
// Utf8 / wchar conversions (trivial on Linux where wchar_t is UTF-32)
// =====================================================================

std::unique_ptr<wchar_t[]> Utf8_2_wchar(const char* utf8str, int sSize)
{
	if (!utf8str || (!sSize && !*utf8str))
	{
		auto result = std::make_unique<wchar_t[]>(1);
		result[0] = 0;
		return result;
	}

	size_t len = (sSize < 0) ? strlen(utf8str) : static_cast<size_t>(sSize);
	auto result = std::make_unique<wchar_t[]>(len + 1);

	mbstate_t state{};
	const char* src = utf8str;
	size_t converted = mbsrtowcs(result.get(), &src, len + 1, &state);
	if (converted == static_cast<size_t>(-1))
	{
		// Fallback: byte-by-byte copy
		for (size_t i = 0; i < len; ++i)
			result[i] = static_cast<wchar_t>(static_cast<unsigned char>(utf8str[i]));
		result[len] = 0;
	}
	return result;
}

std::unique_ptr<wchar_t[]> Utf8_2_wchar(WeakStr utf8str)
{
	return Utf8_2_wchar(utf8str.c_str(), static_cast<int>(utf8str.ssize()));
}

auto wchar_2_Utf8Str(const wchar_t* wCharStr, int strLen) -> SharedStr
{
	if (!wCharStr || !*wCharStr)
		return SharedStr();

	size_t len = (strLen < 0) ? wcslen(wCharStr) : static_cast<size_t>(strLen);

	// Each wchar_t can produce up to 4 UTF-8 bytes
	size_t bufSize = len * 4 + 1;
	auto buf = std::make_unique<char[]>(bufSize);

	mbstate_t state{};
	const wchar_t* src = wCharStr;
	size_t converted = wcsrtombs(buf.get(), &src, bufSize, &state);
	if (converted == static_cast<size_t>(-1))
		return SharedStr();

	return SharedStr(CharPtrRange(buf.get(), buf.get() + converted));
}

#endif //defined(_MSC_VER)

//  -----------------------------------------------------------------------
// Performance logging (cross-platform)

#include <atomic>

// Tri-state cache of the PerformanceLogging setting: 0 = not yet read, 1 = off, 2 = on.
// RTC_GetRegDWord takes s_RegAccess, which is too much for a per-operation check, and the
// setting is not meant to change mid-session.
static std::atomic<UInt8> s_PerformanceLoggingState = 0;

RTC_CALL bool IsPerformanceLogging()
{
	auto state = s_PerformanceLoggingState.load(std::memory_order_relaxed);
	if (!state)
	{
		state = RTC_GetRegDWord(RegDWordEnum::PerformanceLogging) ? 2 : 1;
		s_PerformanceLoggingState.store(state, std::memory_order_relaxed);
	}
	return state == 2;
}

void SetPerformanceLogging(bool enable)
{
	RTC_SetCachedDWord(RegDWordEnum::PerformanceLogging, enable ? 1 : 0);
	s_PerformanceLoggingState.store(enable ? 2 : 1, std::memory_order_relaxed);
}

// Same tri-state caching trick: 0 = not yet read, else 1 + the mode.
static std::atomic<UInt8> s_ResourceSchedulingState = 0;

resource_scheduling GetResourceScheduling()
{
	auto state = s_ResourceSchedulingState.load(std::memory_order_relaxed);
	if (!state)
	{
		auto dw = RTC_GetRegDWord(RegDWordEnum::ResourceAwareScheduling);
		state = UInt8(1 + Min<DWORD>(dw, DWORD(resource_scheduling::enforce)));
		s_ResourceSchedulingState.store(state, std::memory_order_relaxed);
	}
	return resource_scheduling(state - 1);
}

RTC_CALL void SetResourceScheduling(resource_scheduling mode)
{
	RTC_SetCachedDWord(RegDWordEnum::ResourceAwareScheduling, DWORD(mode));
	s_ResourceSchedulingState.store(UInt8(1 + UInt8(mode)), std::memory_order_relaxed);
}

//  -----------------------------------------------------------------------
// Executable version component (cross-platform), shown in the Help/About
// dialog as two lines:
//   Executable:        full path of the running host process (e.g. the GUI
//                      exe, or python.exe when driven via the python binding),
//                      queried from the running process so a renamed binary
//                      shows its actual file name.
//   GeoDms Exe Folder: the GeoDms binary/resource root (GetExeDir), which is
//                      this Rtc module's own directory and may differ from the
//                      host process (e.g. the python case).

#include "VersionComponent.h"

// Full path of the currently running host-process executable (UTF-8, DMS-style
// '/' delimiters), or an empty string when it cannot be determined.
static SharedStr GetExeFullPath()
{
#if defined(_MSC_VER)
	std::vector<wchar_t> buf(MAX_PATH);
	for (;;)
	{
		DWORD n = GetModuleFileNameW(nullptr, buf.data(), DWORD(buf.size()));
		if (n == 0)
			return SharedStr();
		if (n < buf.size())            // fit; a truncated result returns buf.size()
			return ConvertDosFileName(wchar_2_Utf8Str(buf.data(), n));
		buf.resize(buf.size() * 2);    // ERROR_INSUFFICIENT_BUFFER: grow and retry
	}
#else
	char buf[PATH_MAX];
	ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
	if (n <= 0)
		return SharedStr();
	return ConvertDosFileName(SharedStr(CharPtrRange(buf, buf + n)));
#endif
}

struct ExeComponent : AbstrVersionComponent {
	void Visit(ClientHandle clientHandle, VersionComponentCallbackFunc callBack, UInt32 componentLevel) const override {
		SharedStr exePath = GetExeFullPath();
		if (!exePath.empty())
			callBack(clientHandle, componentLevel, mySSPrintF("Executable: {}", exePath.c_str()).c_str());
		callBack(clientHandle, componentLevel, mySSPrintF("GeoDms Exe Folder: {}", GetExeDir().c_str()).c_str());
	}
};

static ExeComponent s_ExeComponent;
