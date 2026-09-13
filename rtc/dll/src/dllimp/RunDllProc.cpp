// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "RtcInterface.h"

// *****************************************************************************
// Section:     Exec
//
// *****************************************************************************



#include "dllimp/RunDllProc.h"

#include "dbg/DmsCatch.h"

#include "vt/StringBounds.h"
#include "set/VectorFunc.h"
#include "utl/Environment.h"  // Utf8_2_wchar

#include <map>

// The platform primitives: open, symbol lookup, close. Everything else about a loaded
// library (ownership, the symbol cache, the CloseAll call on unload) is platform-neutral.
#if defined(WIN32)

#include <windows.h>

namespace {
	using dll_handle_t = HMODULE;

	dll_handle_t dll_open(CharPtr dllname)
	{
		// dllname is UTF-8; the unsuffixed LoadLibrary resolves to LoadLibraryA
		// (no UNICODE/_UNICODE in this project), interpreting the bytes as ACP.
		// Use the wide-char variant so non-ASCII DLL paths load correctly.
		return LoadLibraryW(Utf8_2_wchar(dllname).get());
	}
	void* dll_sym(dll_handle_t hDLL, CharPtr procName) { return reinterpret_cast<void*>(GetProcAddress(hDLL, procName)); }
	void  dll_close(dll_handle_t hDLL) { FreeLibrary(hDLL); }
}

#else //defined(WIN32)

#include <dlfcn.h>

namespace {
	using dll_handle_t = void*;

	dll_handle_t dll_open(CharPtr dllname) { return dlopen(dllname, RTLD_LAZY); }
	void* dll_sym(dll_handle_t hDLL, CharPtr procName) { return dlsym(hDLL, procName); }
	void  dll_close(dll_handle_t hDLL) { dlclose(hDLL); }
}

#endif //defined(WIN32)

struct DllHandle
{
	DllHandle() = default;

	// the destructor unloads, so a handle has exactly one owner: the entry in s_DllHandleCache
	DllHandle(const DllHandle&) = delete;
	DllHandle& operator =(const DllHandle&) = delete;

	~DllHandle()
	{
		if (!m_hDLL)
			return;

		DMS_CALL_BEGIN
			LPFNDLLFUNC0 finalizeProc = LPFNDLLFUNC0(GetProc("CloseAll"));
			if (finalizeProc)
				(*finalizeProc)();
			dll_close(m_hDLL);
		DMS_CALL_END

		m_hDLL = nullptr;
	}

	void Load(CharPtr dllname)
	{
		dms_assert(!m_hDLL);
		m_hDLL = dll_open(dllname);
	}

	void* GetProc(CharPtr dllProcName)
	{
		DllProcCacheType::iterator i = m_DllProcCache.find(SharedStr(dllProcName MG_DEBUG_ALLOCATOR_SRC("GetProc")));
		if (i != m_DllProcCache.end())
			return i->second;

		void*& proc = m_DllProcCache[SharedStr(dllProcName MG_DEBUG_ALLOCATOR_SRC("GetProc"))];
		proc = dll_sym(m_hDLL, dllProcName);
		return proc;
	}

	bool IsLoaded() const { return m_hDLL != nullptr; }

private:
	typedef std::map<SharedStr, void*> DllProcCacheType;

	DllProcCacheType m_DllProcCache;
	dll_handle_t     m_hDLL = nullptr;
};


namespace {

	typedef std::map<SharedStr, DllHandle> DllHandleCacheType;
	DllHandleCacheType* s_DllHandleCache      = 0;
	UInt32              s_nrDllComponwentLocks = 0;

	struct DllComponentLock : RtcReportLock
	{
		DllComponentLock()
		{
			if (!s_nrDllComponwentLocks++)
			{
				dms_assert(!s_DllHandleCache);
				s_DllHandleCache = new DllHandleCacheType;
			}
		}
		~DllComponentLock()
		{
			dms_assert(s_DllHandleCache);
			if (!--s_nrDllComponwentLocks)
			{
				delete s_DllHandleCache;
				s_DllHandleCache = 0;
			}
		}
	};
}	// end anonymous namespace


DllHandle* RTC_GetDll(CharPtr dllname)
{
	static DllComponentLock lock;

	dms_assert(s_nrDllComponwentLocks);

	DllHandleCacheType::iterator i = s_DllHandleCache->find(SharedStr(dllname MG_DEBUG_ALLOCATOR_SRC("RTC_GetDll")));
	if (i != s_DllHandleCache->end())
		return &(i->second);

	DllHandle& hnd = (*s_DllHandleCache)[SharedStr(dllname MG_DEBUG_ALLOCATOR_SRC("RTC_GetDll"))];
	hnd.Load(dllname);
	return &hnd;
}

DllHandle* GetDllChecked(CharPtr dllName)
{
	DllHandle* result = RTC_GetDll(dllName);
	dms_assert(result);
	if (!result->IsLoaded())
      throwErrorF("DllLoad", "Cannot load dll [{}]", dllName);

	return result;
}

void* RTC_GetProc(DllHandle* hnd, CharPtr procName)
{
	dms_assert(hnd);
	return hnd->GetProc(procName);
}

void* GetProcChecked(CharPtr dllName, CharPtr procName)
{
	void* result = RTC_GetProc(GetDllChecked(dllName), procName);
	if (!result)
      throwErrorF("DllLoad", "Cannot find proc [{}] in dll [{}]", procName, dllName);
	return result;
}

bool RTC_IsLoaded(const DllHandle* hnd)
{
	return hnd->IsLoaded();
}

UInt32 RunDllProc0(CharPtr dllName, CharPtr procName)
{
	CDebugContextHandle context(dllName, procName, false);

	LPFNDLLFUNC0 runFunc = (LPFNDLLFUNC0) GetProcChecked(dllName, procName);
	return (*runFunc)();
}

UInt32 RunDllProc1(CharPtr dllName, CharPtr procName, CharPtr arg1)
{
	CDebugContextHandle context    (dllName, procName, false);
	CDebugContextHandle arg1Context("arg1", arg1, false);

	LPFNDLLFUNC1 runFunc = (LPFNDLLFUNC1) GetProcChecked(dllName, procName);
	return (*runFunc)(arg1);
}

UInt32 RunDllProc2(CharPtr dllName, CharPtr procName, CharPtr arg1, CharPtr arg2)
{
	CDebugContextHandle context(dllName, procName, false);
	CDebugContextHandle arg1Context("arg1", arg1, false);
	CDebugContextHandle arg2Context("arg2", arg2, false);

	LPFNDLLFUNC2 runFunc = (LPFNDLLFUNC2) GetProcChecked(dllName, procName);
	return (*runFunc)(arg1, arg2);
}

UInt32 RunDllProc3(CharPtr dllName, CharPtr procName, CharPtr arg1, CharPtr arg2, CharPtr arg3)
{
	CDebugContextHandle context(dllName, procName, false);
	CDebugContextHandle arg1Context("arg1", arg1, false);
	CDebugContextHandle arg2Context("arg2", arg2, false);
	CDebugContextHandle arg3Context("arg3", arg3, false);

	LPFNDLLFUNC3 runFunc = (LPFNDLLFUNC3) GetProcChecked(dllName, procName);
	return (*runFunc)(arg1, arg2, arg3);
}

bool g_IsTerminating = false;

RTC_CALL void DMS_CONV DMS_Terminate()
{
	g_IsTerminating = true;
}
