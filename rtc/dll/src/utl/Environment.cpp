// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "utl/Environment.h"

#include "dbg/DmsCatch.h"
#include "geo/Conversions.h"
#include "geo/Point.h"
#include "geo/MinMax.h"
#include "geo/StringBounds.h"
#include "ptr/IterCast.h"
#include "set/IndexedStrings.h"
#include "utl/mySPrintF.h"
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

static SharedStr          g_ExeDir;
static SharedStr          g_LocalDataDir;

struct LocalAllocatedPtr
{
	LPVOID m_Ptr;
	LocalAllocatedPtr() : m_Ptr(0) {}
	~LocalAllocatedPtr() { if (m_Ptr) LocalFree(m_Ptr); }
};

SharedStr platform::GetSystemErrorText(DWORD lastErr)
{
	LocalAllocatedPtr lpMsgBuf;
	FormatMessage( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM | 
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		lastErr,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPTSTR) &(lpMsgBuf.m_Ptr),
		0,
		NULL 
	);
	// Process any inserts in lpMsgBuf.
	return SharedStr((LPCTSTR)lpMsgBuf.m_Ptr MG_DEBUG_ALLOCATOR_SRC("platform::GetSystemErrorText"));
};

RTC_CALL bool platform::isCharPtrAndExceeds_MAX_PATH(CharPtr xFileName)
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
				"WindowsSystem Error %s:\nErrorCode %d: %s\nWaiting %d seconds before retry #%d", 
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
	return IsMultiThreaded0() && GetQueueStatus(QS_ALLEVENTS);
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
		auto userResponse = MessageBoxA(nullptr, mySSPrintF("Failed to load FontResource %s", name).c_str(), "Warning", MB_ABORTRETRYIGNORE | MB_ICONWARNING);
		switch (userResponse)
		{
		case IDABORT: terminate();
		case IDIGNORE: break;
		}
	}
}

void DMS_Appl_SetExeDir(CharPtr exeDir)
{
	assert(g_ExeDir.empty()); // should only called once, exeDirs don't just change during a session
	g_ExeDir = ConvertDosFileName(SharedStr(exeDir MG_DEBUG_ALLOCATOR_SRC("DMS_Appl_SetExeDir")));
	
	SetMainThreadID();
}

RTC_CALL void DMS_CONV DMS_Appl_SetFont()
{
	MG_CHECK(!g_ExeDir.empty());
	AddFontResourceExA_checked(DelimitedConcat(g_ExeDir.c_str(), "misc/fonts/dms.ttf").c_str(), FR_PRIVATE, nullptr);
	AddFontResourceExA_checked(DelimitedConcat(g_ExeDir.c_str(), "misc/fonts/dmstext.ttf").c_str(), FR_PRIVATE, nullptr);
}

RTC_CALL SharedStr GetExeDir()     // contains DmsClient.exe (+dlls?) and dms.ini; does NOT end with '/' 
{
	assert(! g_ExeDir.empty());
	return g_ExeDir;
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

RTC_CALL SharedStr GetSourceDataDir()
{
	static SharedStr sourceDataDir = GetSourceDataDirImpl();
	return sourceDataDir;
}

UInt32 g_RegStatusFlags = 0; // status flags as found in the register
UInt32 g_OvrStatusFlags = 0; // status flags as set from the command line
UInt32 g_OvrStatusMask  = 0; // mask for status flags as set from the command line

leveled_critical_section s_RegAccess(item_level_type(0), ord_level_type::RegisterAccess, "RegisterAccess");

RTC_CALL void DMS_Appl_SetRegStatusFlags(UInt32 newSF)
{
	leveled_critical_section::scoped_lock lock(s_RegAccess);
	g_RegStatusFlags = (newSF | RSF_WasRead);
}

UInt32 ReadOnceRegisteredStatusFlags()
{
	if (g_RegStatusFlags & RSF_WasRead)
		return g_RegStatusFlags;

	leveled_critical_section::scoped_lock lock(s_RegAccess);

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
	leveled_critical_section::scoped_lock lock(s_RegAccess);
	g_OvrStatusMask |= newSF;
	if (newVal)
		g_OvrStatusFlags |= newSF; // set
	else
		g_OvrStatusFlags &= ~newSF; // clear


}

RTC_CALL void SetRegStatusFlags(UInt32 newSF)
{
	SetGeoDmsRegKeyDWord("StatusFlags", newSF);
	DMS_Appl_SetRegStatusFlags(newSF);
}

RTC_CALL void SetStatusFlag(UInt32 newSF, bool newVal)
{
	leveled_critical_section::scoped_lock lock(s_RegAccess);
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

RTC_CALL bool IsMultiThreaded1or2()
{
	return GetRegStatusFlags() & (RSF_MultiThreading1| RSF_MultiThreading2);
}

RTC_CALL bool HasDynamicROI()
{
	return GetRegStatusFlags() & RSF_DynamicROI;
}

RTC_CALL bool ShowThousandSeparator()
{
	return GetRegStatusFlags() & RSF_ShowThousandSeparator;
}

RTC_CALL bool EventLog_HideDepreciatedCaseMixupWarnings()
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
		case 'R': SetCachedStatusFlag(RSF_DynamicROI, newValue); break;
		case 'S':
		case '0': SetCachedStatusFlag(RSF_SuspendForGUI, newValue); break;
		case '1': SetCachedStatusFlag(RSF_MultiThreading1, newValue); break;
		case '2': SetCachedStatusFlag(RSF_MultiThreading2, newValue); break;
		case '3': SetCachedStatusFlag(RSF_MultiThreading3, newValue); break;
		case 'H': SetCachedStatusFlag(RSF_ShowThousandSeparator, newValue); break;
		case 'W': SetCachedStatusFlag(RSF_EventLog_HideDepreciated, !newValue); break; // the command line option is /SW to Show (not hide) deprecated events, but the flag is HideDepreciated, so invert the value
		default:
			reportF(SeverityTypeID::ST_Warning, "Unrecognised command line %s option %s",  (newValue ? "Set" : "Clear"), param);
			return true;
	}
//	reportF(SeverityTypeID::ST_MinorTrace, "Recognised command line option %s %s", (newValue ? "Set" : "Clear"), param[2]);
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
	{ "MemoryMaxRAM_GB", 64, false }
};

extern "C" RTC_CALL DWORD RTC_GetRegDWord(RegDWordEnum i)
{
	auto ui = UInt32(i);
	MG_CHECK(ui < sizeof(s_RegDWordAttrs) / sizeof(RegDWordAttr));

	leveled_critical_section::scoped_lock lock(s_RegAccess);

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

	leveled_critical_section::scoped_lock lock(s_RegAccess);
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
		throwLastSystemError("MakeDir('%s')", dirName.c_str());
	}
}

bool IsDosDir(WeakStr dosFileName, CharPtr dmsFileName)
{
	DWORD attr = GetFileAttributes(dosFileName.c_str());
	if (attr == INVALID_FILE_ATTRIBUTES)
		throwLastSystemError("IsDir(%s)", dmsFileName);
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
	:	m_Data  (new Byte[sizeof(WIN32_FIND_DATA)])
	,	m_Handle(
			FindFirstFile(
				ConvertDmsFileName(fileSearchSpec).c_str()
			,	reinterpret_cast<WIN32_FIND_DATA*>(m_Data.get())
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
	return reinterpret_cast<const WIN32_FIND_DATA*>(m_Data.get())->dwFileAttributes;
}

bool FindFileBlock::IsDirectory() const
{
	return GetFileAttr() & FILE_ATTRIBUTE_DIRECTORY;
}

CharPtr FindFileBlock::GetCurrFileName() const
{
	return reinterpret_cast<const WIN32_FIND_DATA*>(m_Data.get())->cFileName;
}

bool FindFileBlock::Next()
{
	return FindNextFile(m_Handle, reinterpret_cast<WIN32_FIND_DATA*>(m_Data.get()));
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
		const WIN32_FIND_DATA* findData= reinterpret_cast<const WIN32_FIND_DATA*>(m_Data.get());
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

	return mySSPrintF("%04d/%02d/%02d  %02d:%02d:%02d",
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
	throwLastSystemError("CopyAllInDir(%s, %s)", srcDirName, destDirName);
}

void SetWritable(CharPtr dosFileName)
{
	// PRECONDITION: fileName is in dos format (no file:// prefix).
	DWORD dwAttrs = GetFileAttributes(dosFileName);
	if (dwAttrs & FILE_ATTRIBUTE_READONLY) 
	{ 
		SetFileAttributes(dosFileName, dwAttrs & ~FILE_ATTRIBUTE_READONLY);
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
			throwErrorF("FileSystem", "CopyOrMoveFileOrDirImpl(%s, %s):\n"
				"Cannot %s to a subdir from source because this would result in infinite recursion.", 
				srcFileOrDirName, destFileOrDirName,
				mustCopy ? "copy" : "move"
			);
	}

	MakeDirsForFile(fullDst);

	SharedStr
		fullSrcDOS = ConvertDmsFileName(fullSrc),
		fullDstDOS = ConvertDmsFileName(fullDst);

	if (!mustCopy)
	{
		if (!MoveFile(fullSrcDOS.c_str(), fullDstDOS.c_str()))
		{
			if (GetLastError() != 2 || !mayBeMissing)
				return false;
			throwLastSystemError("MoveFileOrDir(%s, %s)", srcFileOrDirName, destFileOrDirName);
		}
	}
	else
	{
		DWORD attr = GetFileAttributes(fullSrcDOS.c_str());
		if (attr == INVALID_FILE_ATTRIBUTES)
			throwLastSystemError("CopyFileOrDir(%s, %s)", srcFileOrDirName, destFileOrDirName);

		if (attr & FILE_ATTRIBUTE_DIRECTORY)
		{
			MakeDir(fullDst);
			CopyAllInDir(fullSrc.c_str(), fullDst.c_str());
		}
		else
		{
			if (!CopyFile(fullSrcDOS.c_str(), fullDstDOS.c_str(), FALSE))
				throwLastSystemError("CopyFile(%s, %s)", srcFileOrDirName, destFileOrDirName);
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
	throwLastSystemError("KillAllInDir(%s)", dirName);
}

bool BreakingReport(CharPtr funcStr, CharPtr fileName)
{
	DWORD lastError = GetLastError();
	dms_assert(lastError);
	bool mustBreak = (lastError != 32 && lastError != 5);
	if (mustBreak)
	{
		reportF(SeverityTypeID::ST_MajorTrace, "Failure in %s(%s) because %s"
		,	funcStr
		,	fileName
		,	platform::GetSystemErrorText(lastError).c_str()
		);
		return true;
	}
	reportF(SeverityTypeID::ST_MajorTrace, "Retry %s(%s) after waiting 1 sec because %s"
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

		UInt32 retryConter = 0;
		do {
			if (RemoveDirectory(dosFileOrDirName.c_str()))
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
				"Suspected call to KillFileOrDir('%s').\n"
				"Only .dmsdata, .tmp or .old extensions are allowed now.", fileOrDirName.c_str()
			);

		UInt32 retryConter = 0;
		do {
			if (DeleteFile(dosFileOrDirName.c_str()))
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
	return _access(ConvertDmsFileName(fileOrDirName).c_str(), 2) != -1;
}

void GetWritePermission(WeakStr fileName)
{
	if (IsFileOrDirAccessible(fileName))
	{
		if (!IsFileOrDirWritable(fileName))
			throwErrorF("FileSystem", "Write permission for '%s' denied", fileName);
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
		assert(written < allocSize);
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

		assert(written < allocSize);
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
		throwLastSystemError("ExecuteChildProcess(%s, %s) failed", moduleName?moduleName:"NULL", cmdLine);

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
		throwLastSystemError("ExecuteChildProcess(%s, %s) failed to return an exitcode", moduleName, cmdLine);

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

	const UInt32 DEFAULT_BUFFER_SIZE = 300;

	std::unique_ptr<char[]> heapBuffer;

	char stackBuffer[DEFAULT_BUFFER_SIZE];
	char* buf  = stackBuffer;
	DWORD size = DEFAULT_BUFFER_SIZE;

	while (true)
	{
//		char* buffer = s_CharBuffer.GetBuffer(size);
		DWORD resSize = GetPrivateProfileString(sectionName, keyName, defaultValue, buf, size, ConvertDmsFileName(configFileName).c_str());
		if (resSize+2 < size )
			return SharedStr(CharPtrRange(buf, buf+resSize));
		size *= 2;
		heapBuffer.reset(new char[size]);
		buf = heapBuffer.get();
	}
}

void SetConfigKeyString(WeakStr configFileName, CharPtr sectionName, CharPtr keyName, CharPtr keyValue)
{
	MakeDirsForFile(configFileName);

	WritePrivateProfileString(sectionName, keyName, keyValue, ConvertDmsFileName(configFileName).c_str());
}

#include <time.h>

Int64 GetSecsSince1970()
{
	return time(0);
}

#include "versionhelpers.h"

namespace PlatformInfo
{
	RTC_CALL SharedStr GetVersionStr()
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
	RTC_CALL SharedStr GetUserNameA()
	{
		DWORD sz = UNLEN+1;
		wchar_t buffer[UNLEN+1];
		if (!::GetUserNameW(buffer, &sz))
			throwLastSystemError("GetUserName");
		return SharedStr(wchar_2_Utf8Str(buffer, sz - 1)); // GetUserNameW includes null in sz
	}
	RTC_CALL SharedStr GetComputerNameA()
	{
		DWORD sz = MAX_COMPUTERNAME_LENGTH+1;
		wchar_t buffer[MAX_COMPUTERNAME_LENGTH + 1]; 
		if (!::GetComputerNameW(buffer, &sz))
			throwLastSystemError("GetComputerName");
		return SharedStr(wchar_2_Utf8Str(buffer, sz));
	}
	RTC_CALL bool GetEnv(CharPtr varName, SharedStr& result)
	{
		auto varNameW = Utf8_2_wchar(varName);
		const wchar_t* resPtr = _wgetenv(varNameW.get());
		if (!resPtr)
			return false;
		result = wchar_2_Utf8Str(resPtr);
		return true;
	}
	RTC_CALL bool GetEnvString(CharPtr section, CharPtr key, SharedStr& result)
	{
		SharedStr varName = mySSPrintF("GEODMS_%s_%s", section, key);
		return GetEnv(varName.c_str(), result);
	}

	RTC_CALL SharedStr GetProgramFiles32()
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

		callBack(clientHandle, componentLevel, mgFormat2string("GetUserDefaultLocaleName(Win32) '%1%'", localeName_utf8).c_str());
		callBack(clientHandle, componentLevel, mgFormat2string("std::locale(\"\"): '%1%'", std::locale("").name().c_str()).c_str());
	}
};

#else //defined(_MSC_VER)

// =====================================================================
// Linux implementations of Environment.h declarations
// =====================================================================

#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cerrno>
#include <climits>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <dirent.h>
#include <fcntl.h>
#include <pwd.h>
#include <spawn.h>
#include <sys/wait.h>

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

RTC_CALL bool platform::isCharPtrAndExceeds_MAX_PATH(CharPtr xFileName)
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
	struct rlimit rl;
	getrlimit(RLIMIT_STACK, &rl);
	// Approximate: distance from current stack variable to estimated stack bottom
	char stackVar;
	// This is a rough approximation; for a more precise solution, use pthread_attr_getstack
	SizeT stackSize = rl.rlim_cur;
	return stackSize / 2; // conservative estimate
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
				"System Error %s:\nErrorCode %d: %s\nWaiting %d seconds before retry #%d",
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
	chdir(dir);
}

static SharedStr g_ExeDir;

void DMS_Appl_SetExeDir(CharPtr exeDir)
{
	assert(g_ExeDir.empty());
	g_ExeDir = ConvertDosFileName(SharedStr(exeDir));
	SetMainThreadID();
}

RTC_CALL void DMS_CONV DMS_Appl_SetFont()
{
	// No-op on Linux (no Windows font resource loading)
}

RTC_CALL SharedStr GetExeDir()
{
	assert(!g_ExeDir.empty());
	return g_ExeDir;
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
	if (!strncmp(path.begin(), "file:", 5))
		return SharedStr(CharPtrRange(path.begin() + 5, path.send()));
	return path;
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
		throwLastSystemError("MakeDir('%s')", dirName.c_str());
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
			throwErrorF("FileSystem", "Write permission for '%s' denied", fileName);
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
	return mySSPrintF("%04d/%02d/%02d  %02d:%02d:%02d",
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
			throwErrorF("FileSystem", "CopyFileOrDir(%s, %s) failed: %s", srcFileOrDirName, destFileOrDirName, e.what());
	}
}

bool MoveFileOrDir(CharPtr srcFileOrDirName, CharPtr destFileOrDirName, bool mayBeMissing)
{
	std::error_code ec;
	std::filesystem::rename(srcFileOrDirName, destFileOrDirName, ec);
	if (ec)
	{
		if (!mayBeMissing)
			throwErrorF("FileSystem", "MoveFileOrDir(%s, %s) failed: %s", srcFileOrDirName, destFileOrDirName, ec.message().c_str());
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
			throwErrorF("Environment", "posix_spawn(%s -c '%s') failed: %s", shell, shellCmd, strerror(status));
	}
	else
	{
		char* argv[] = { const_cast<char*>(moduleName), cmdLine, nullptr };
		status = posix_spawn(&pid, moduleName, nullptr, nullptr, argv, environ);
		if (status != 0)
			throwErrorF("Environment", "posix_spawn(%s) failed: %s", moduleName, strerror(status));
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
    SharedStr base = xdg && *xdg ? SharedStr(xdg) : mySSPrintF("%s/.config", getenv("HOME") ? getenv("HOME") : "/tmp");
    return base + "/geodms";
}

static SharedStr GetGeoDmsIniPath()
{
    return GetGeoDmsConfigDir() + "/geodms.ini";
}

// Simple in-memory INI cache: section -> (key -> value)
using IniData = std::map<std::string, std::map<std::string, std::string>>;

static std::mutex s_IniMutex;
static IniData    s_IniData;
static bool       s_IniLoaded = false;

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
    if (s_IniLoaded) return;
    s_IniLoaded = true;

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
                s_IniData[section][key] = val;
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
    for (auto& [sec, kv] : s_IniData)
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
    auto si = s_IniData.find(section);
    if (si == s_IniData.end()) return defaultValue;
    auto ki = si->second.find(key);
    if (ki == si->second.end()) return defaultValue;
    return ki->second;
}

static void IniSet(const char* section, const char* key, const char* value)
{
    std::lock_guard lock(s_IniMutex);
    LoadIni_Locked();
    s_IniData[section][key] = value;
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

RTC_CALL SharedStr GetSourceDataDir()
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

leveled_critical_section s_RegAccess(item_level_type(0), ord_level_type::RegisterAccess, "RegisterAccess");

RTC_CALL void DMS_Appl_SetRegStatusFlags(UInt32 newSF)
{
	leveled_critical_section::scoped_lock lock(s_RegAccess);
	g_RegStatusFlags = (newSF | RSF_WasRead);
}

UInt32 ReadOnceRegisteredStatusFlags()
{
	if (g_RegStatusFlags & RSF_WasRead)
		return g_RegStatusFlags;

	leveled_critical_section::scoped_lock lock(s_RegAccess);
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
	leveled_critical_section::scoped_lock lock(s_RegAccess);
	g_OvrStatusMask |= newSF;
	if (newVal) g_OvrStatusFlags |= newSF;
	else        g_OvrStatusFlags &= ~newSF;
}

RTC_CALL void SetRegStatusFlags(UInt32 newSF)
{
	SetGeoDmsRegKeyDWord("StatusFlags", newSF);
	DMS_Appl_SetRegStatusFlags(newSF);
}

RTC_CALL void SetStatusFlag(UInt32 newSF, bool newVal)
{
	leveled_critical_section::scoped_lock lock(s_RegAccess);
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
RTC_CALL bool IsMultiThreaded1or2() { return GetRegStatusFlags() & (RSF_MultiThreading1 | RSF_MultiThreading2); }
RTC_CALL bool HasDynamicROI()       { return GetRegStatusFlags() & RSF_DynamicROI; }
RTC_CALL bool ShowThousandSeparator() { return GetRegStatusFlags() & RSF_ShowThousandSeparator; }
RTC_CALL bool EventLog_HideDepreciatedCaseMixupWarnings() { return GetRegStatusFlags() & RSF_EventLog_HideDepreciated; }

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
		case 'R': SetCachedStatusFlag(RSF_DynamicROI, newValue); break;
		case 'S': case '0': SetCachedStatusFlag(RSF_SuspendForGUI, newValue); break;
		case '1': SetCachedStatusFlag(RSF_MultiThreading1, newValue); break;
		case '2': SetCachedStatusFlag(RSF_MultiThreading2, newValue); break;
		case '3': SetCachedStatusFlag(RSF_MultiThreading3, newValue); break;
		case 'H': SetCachedStatusFlag(RSF_ShowThousandSeparator, newValue); break;
		case 'W': SetCachedStatusFlag(RSF_EventLog_HideDepreciated, !newValue); break;
		default:
			reportF(SeverityTypeID::ST_Warning, "Unrecognised command line %s option %s", (newValue ? "Set" : "Clear"), param);
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
	{ "MemoryMaxRAM_GB", 64, false }
};

extern "C" RTC_CALL DWORD RTC_GetRegDWord(RegDWordEnum i)
{
	auto ui = UInt32(i);
	MG_CHECK(ui < sizeof(s_RegDWordAttrs) / sizeof(RegDWordAttr));
	leveled_critical_section::scoped_lock lock(s_RegAccess);
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
	leveled_critical_section::scoped_lock lock(s_RegAccess);
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
	RTC_CALL SharedStr GetVersionStr()
	{
		struct utsname buf;
		if (uname(&buf) == 0)
			return mySSPrintF("Linux %s %s", buf.release, buf.machine);
		return SharedStr("Linux (unknown version)");
	}

	RTC_CALL SharedStr GetUserNameA()
	{
		const char* user = getenv("USER");
		if (user) return SharedStr(user);
		struct passwd* pw = getpwuid(getuid());
		if (pw) return SharedStr(pw->pw_name);
		return SharedStr("unknown");
	}

	RTC_CALL SharedStr GetComputerNameA()
	{
		char hostname[256];
		if (gethostname(hostname, sizeof(hostname)) == 0)
			return SharedStr(hostname);
		return SharedStr("unknown");
	}

	RTC_CALL bool GetEnv(CharPtr varName, SharedStr& result)
	{
		const char* val = getenv(varName);
		if (!val) return false;
		result = SharedStr(val);
		return true;
	}

	RTC_CALL bool GetEnvString(CharPtr section, CharPtr key, SharedStr& result)
	{
		SharedStr varName = mySSPrintF("GEODMS_%s_%s", section, key);
		return GetEnv(varName.c_str(), result);
	}

	RTC_CALL SharedStr GetProgramFiles32()
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
