//<HEADER> 
/*
Data & Model Server (DMS) is a server written in C++ for DSS applications. 
Version: see srv/dms/rtc/dll/src/RtcVersion.h for version info.

Copyright (C) 1998-2004  YUSE GSO Object Vision BV. 

Documentation on using the Data & Model Server software can be found at:
http://www.ObjectVision.nl/DMS/

See additional guidelines and notes in srv/dms/Readme-srv.txt 

This library is free software; you can use, redistribute, and/or
modify it under the terms of the GNU General Public License version 2 
(the License) as published by the Free Software Foundation,
provided that this entire header notice and readme-srv.txt is preserved.

See LICENSE.TXT for terms of distribution or look at our web site:
http://www.objectvision.nl/DMS/License.txt
or alternatively at: http://www.gnu.org/copyleft/gpl.html

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details. However, specific warranties might be
granted by an additional written contract for support, assistance and/or development
*/
//</HEADER>

#include "RtcPCH.h"

#if defined(_MSC_VER)
#pragma hdrstop
#endif

#include <concrtrm.h>
#include <agents.h>

#include "utl/Environment.h"

#include "dbg/DmsCatch.h"
#include "geo/MinMax.h"
#include "geo/Point.h"
#include "geo/StringBounds.h"
#include "ptr/IterCast.h"
#include "set/IndexedStrings.h"
#include "utl/mySPrintF.h"
#include "utl/splitPath.h"
#include "utl/Environment.h"
#include "LockLevels.h"

#include <concrt.h>
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
	return SharedStr((LPCTSTR)lpMsgBuf.m_Ptr);
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
	Concurrency::Context::Yield(); // Yield to other contexts (=tasks?) in the current thread or if none available, another OS thread
	GetSystemTime(&nextTime);
	UInt32 currMillisecs = currTime.wMilliseconds + currTime.wSecond * 1000 + currTime.wMinute * 60000; dms_assert(currMillisecs < 60 * 60 * 1000);
	UInt32 nextMillisecs = currTime.wMilliseconds + currTime.wSecond * 1000 + currTime.wMinute * 60000; dms_assert(nextMillisecs < 60 * 60 * 1000);
	if (nextMillisecs < currMillisecs)
		nextMillisecs += 60 * 60 * 1000;
	dms_assert(nextMillisecs >= currMillisecs);
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
				"WindowsSystemError %s:\nErrorCode %d: %s\nWaiting %d seconds before retry #%d", 
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
//UInt32 g_IE_MsgCount = 0;
//UInt32 g_VS_MsgCount = 0;

bool HasWaitingMessages()
{
	return IsMultiThreaded0() && GetQueueStatus(QS_ALLEVENTS & ~QS_TIMER);
}

extern "C" RTC_CALL bool DMS_CONV DMS_HasWaitingMessages()
{
	return HasWaitingMessages();
}

//  -----------------------------------------------------------------------

bool IsDosFileOrDirAccessible(CharPtr doFileOrDirName)
{
	return _access(doFileOrDirName, 0) != -1;
}

SharedStr GetCurrentDir()
{
	DWORD buffSize = GetCurrentDirectory(0, 0);
	std::vector<char> buffer(buffSize);
	if (!GetCurrentDirectory(buffSize, begin_ptr(buffer)))
		throwLastSystemError("GetCurrrentDir");
	dms_assert(buffer.back() == char(0));

	if (buffer.empty())
		return SharedStr();
	return ConvertDosFileName(SharedStr(begin_ptr(buffer), end_ptr(buffer)-1));
}

void SetCurrentDir(CharPtr dir)
{
	SetCurrentDirectory(dir);
}

static exe_type s_ExeType = exe_type::unknown_run_or_dephi;

void DMS_Appl_SetExeType(exe_type t)
{
	s_ExeType = t;
}

exe_type DMS_Appl_GetExeType()
{
	return s_ExeType;
}

void AddFontResourceExA_checked(_In_ LPCSTR name, _In_ DWORD fl, _Reserved_ PVOID res)
{
	while (true)
	{
		auto result = AddFontResourceExA(name, fl, res);
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
	dms_assert(g_ExeDir.empty()); // should only called once, exeDirs don't just change during a session
	g_ExeDir = ConvertDosFileName(SharedStr(exeDir));
	AddFontResourceExA_checked(DelimitedConcat(g_ExeDir.c_str(), "misc/fonts/dms.ttf").c_str(), FR_PRIVATE, 0);
	SetMainThreadID();
}

RTC_CALL SharedStr GetExeDir()     // contains DmsClient.exe (+dlls?) and dms.ini; does NOT end with '/' 
{
	dms_assert(! g_ExeDir.empty());
	return g_ExeDir;
}

#include "utl/Registry.h"
static bool s_localDataDirWasUsed = false;

extern "C" 
RTC_CALL bool DMS_Appl_LocalDataDirWasUsed()
{
	return s_localDataDirWasUsed;
}


RTC_CALL SharedStr GetGeoDmsRegKey(CharPtr key)
{
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

RTC_CALL std::vector<std::string> GetGeoDmsRegKeyMultiString(CharPtr key)
{
	std::vector<std::string> result;
	try {
		RegistryHandleLocalMachineRO regLM;
		if (regLM.ValueExists(key))
		{
			result = regLM.ReadMultiString(key);
			if (result.empty())
				goto exit;
			return result;
		}
		RegistryHandleCurrentUserRO regCU;
		if (regCU.ValueExists(key))
			return regCU.ReadMultiString(key);
	}
	catch (...) {}
exit:
	return result;
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

RTC_CALL bool SetGeoDmsRegKeyString(CharPtr key, std::string str)
{
	try {
		RegistryHandleLocalMachineRW regLM;
		regLM.WriteString(key, str);
	}
	catch (...) {}
	return true;
}

RTC_CALL bool SetGeoDmsRegKeyMultiString(CharPtr key, std::vector<std::string> strings)
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

RTC_CALL SharedStr GetLocalDataDir()
{
	static SharedStr localDataDir;
	if (!DMS_Appl_LocalDataDirWasUsed())
	{
		s_localDataDirWasUsed = true;
		localDataDir = GetConvertedGeoDmsRegKey("LocalDataDir");
		if (localDataDir.empty())
			localDataDir = "C:/LocalData";
	}
	return localDataDir;
}

static bool s_sourceDataDirWasUsed = false;

extern "C" 
RTC_CALL bool DMS_Appl_SourceDataDirWasUsed()
{
	return s_sourceDataDirWasUsed;
}

RTC_CALL SharedStr GetSourceDataDir()
{
	static SharedStr sourceDataDir;
	if (!DMS_Appl_SourceDataDirWasUsed())
	{
		s_sourceDataDirWasUsed = true;
		sourceDataDir = GetConvertedGeoDmsRegKey("SourceDataDir");
		if (sourceDataDir.empty())
			sourceDataDir = "C:\\SourceData";
	}
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

RTC_CALL UInt32 GetRegStatusFlags()
{
	leveled_critical_section::scoped_lock lock(s_RegAccess);

	if (!(g_RegStatusFlags & RSF_WasRead))
	{
		g_RegStatusFlags |= RSF_WasRead;	
		try {
			RegistryHandleLocalMachineRO reg;
			if (reg.ValueExists("StatusFlags"))
			{
				g_RegStatusFlags |= reg.ReadDWORD("StatusFlags");
				goto exit;
			}
		}
		catch (...) {}
		try {
			RegistryHandleCurrentUserRO reg;
			if (reg.ValueExists("StatusFlags"))
			{
				g_RegStatusFlags |= reg.ReadDWORD("StatusFlags");
				goto exit;
			}
		}
		catch(...) {}

		g_RegStatusFlags |= RSF_Default;
	}
exit:
	return (g_RegStatusFlags & ~(g_OvrStatusMask | RSF_WasRead)) | (g_OvrStatusFlags & g_OvrStatusMask);
}

RTC_CALL UInt32 GetRegFlags(std::string key, bool& exists)
{
	UInt32 flags = 0;
	try {
		RegistryHandleLocalMachineRO reg;
		if (reg.ValueExists(key.c_str()))
		{
			flags = reg.ReadDWORD(key.c_str());
			exists = true;
			return flags;
		}
		else
			exists = false;

	}
	catch (...) {}
	try {
		RegistryHandleCurrentUserRO reg;
		if (reg.ValueExists(key.c_str()))
		{
			flags = reg.ReadDWORD(key.c_str());
			exists = true;
			return flags;
		}
		else
			exists = false;
	}
	catch (...) {}

	return flags;
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

RTC_CALL bool HasDynamicROI()
{
	return GetRegStatusFlags() & RSF_DynamicROI;
}

RTC_CALL bool ShowThousandSeparator()
{
	return GetRegStatusFlags() & RSF_ShowThousandSeparator;
}

RTC_CALL UInt32 MaxConcurrentTreads()
{
	if (!IsMultiThreaded1())
		return 1;
	return concurrency::GetProcessorCount();
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
	if (!CreateDirectory(ConvertDmsFileName(dirName).c_str(), 0))
	{
		if (GetLastError() == ERROR_ALREADY_EXISTS)
			return;
		throwLastSystemError("MakeDir('%s')", dirName.c_str());
	}
//	reportF(ST_MinorTrace, "MakeDir('%s')", dirName.c_str());
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

	return ConvertDmsFileNameAlways(SharedStr(path.begin()+5, path.send()));
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
	MakeDirsForFile(ConvertDosFileName(SharedStr(fileName)));
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

//  -----------------------------------------------------------------------

// Child process used by exec expressions for executables; Create; Execute and Wait for termination
start_process_result_t StartChildProcess(CharPtr moduleName, Char* cmdLine)
{
	STARTUPINFO siStartInfo;
	PROCESS_INFORMATION piProcInfo;

	// Set up members of STARTUPINFO structure. 
	ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
	ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
	siStartInfo.cb = sizeof(STARTUPINFO);
	//   siStartInfo.dwFlags = STARTF_FORCEONFEEDBACK;

//	MessageBox(nullptr, cmdLine, moduleName, MB_OK);

	// Create the child process.
	BOOL res = CreateProcessA
	(
		moduleName,
		cmdLine,		// command line can be rewritten
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

	return GetPrivateProfileInt(sectionName, keyName, defaultValue, ConvertDmsFileName(configFileName).c_str());
}

SharedStr GetConfigKeyString(WeakStr configFileName, CharPtr sectionName, CharPtr keyName, CharPtr defaultValue)
{
	if (!IsFileOrDirAccessible(configFileName))
		return SharedStr(defaultValue);

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
			return SharedStr(buf, buf+resSize);
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
		char buffer[UNLEN+1];
		if (!::GetUserNameA(buffer, &sz))
			throwLastSystemError("GetUserName");
		return SharedStr(buffer, buffer+sz-1);
	}
	RTC_CALL SharedStr GetComputerNameA()
	{
		DWORD sz = MAX_COMPUTERNAME_LENGTH+1;
		char buffer[MAX_COMPUTERNAME_LENGTH + 1]; 
		if (!::GetComputerNameA(buffer, &sz))
			throwLastSystemError("GetComputerName");
		return SharedStr(buffer, buffer+sz);
	}
	RTC_CALL bool GetEnv(CharPtr varName, SharedStr& result)
	{
		CharPtr resPtr = getenv(varName);
		if (!resPtr)
			return false;
		result = SharedStr(resPtr);
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
