// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

// *****************************************************************************
// Section:     Exec
//
// *****************************************************************************


#include "dllimp/RunDllProc.h"

#include "dbg/DmsCatch.h"

#include "geo/StringBounds.h"
#include "set/VectorFunc.h"

#include <map>
#include <windows.h>

struct DllHandle 
{
	DllHandle() : m_hDLL(NULL) {}
	~DllHandle()
	{
		if (!m_hDLL)
			return;

		DMS_CALL_BEGIN
			LPFNDLLFUNC0 finalizeProc = LPFNDLLFUNC0(GetProc("CloseAll"));
			if (finalizeProc)
				(*finalizeProc)();
			FreeLibrary(m_hDLL);
		DMS_CALL_END

		m_hDLL = NULL;
	}
	
	FARPROC GetProc(CharPtr dllProcName)
	{
		DllProcCacheType::iterator i = m_DllProcCache.find(SharedStr(dllProcName));
		if (i != m_DllProcCache.end())
			return i->second;

		FARPROC& proc = m_DllProcCache[SharedStr(dllProcName)];
		proc = GetProcAddress(m_hDLL, dllProcName);
		return proc;
	}

	bool IsLoaded() const { return m_hDLL != NULL; }

	void SetInstance(HINSTANCE hDLL) 
	{
		m_hDLL = hDLL;
	}

	DllHandle(const DllHandle& src) // FORBIDDEN to copy when dll is loaded since destructor does unload!
	{
		MG_CHECK(src.m_hDLL == NULL);
//		src.m_hDLL = NULL;
		m_hDLL = src.m_hDLL;
	}

private:
	typedef std::map<SharedStr, FARPROC> DllProcCacheType;

	DllProcCacheType m_DllProcCache;
	HINSTANCE        m_hDLL;
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

	DllHandleCacheType::iterator i = s_DllHandleCache->find(SharedStr(dllname));
	if (i != s_DllHandleCache->end())
		return &(i->second);

	DllHandle& hnd = (*s_DllHandleCache)[SharedStr(dllname)];
	hnd.SetInstance(LoadLibrary(dllname));
	return &hnd;
}

DllHandle* GetDllChecked(CharPtr dllName)
{
	DllHandle* result = RTC_GetDll(dllName);
	dms_assert(result);
	if (!result->IsLoaded())
      throwErrorF("DllLoad", "Cannot load dll [%s]", dllName);

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
      throwErrorF("DllLoad", "Cannot find proc [%s] in dll [%s]", procName, dllName);
	return result;
}

bool RTC_IsLoaded(const DllHandle* hnd)
{
	return hnd->IsLoaded();
}

RTC_CALL UInt32 RunDllProc0(CharPtr dllName, CharPtr procName)
{
	CDebugContextHandle context(dllName, procName, false);

	LPFNDLLFUNC0 runFunc = (LPFNDLLFUNC0) GetProcChecked(dllName, procName);
	return (*runFunc)();
}

RTC_CALL UInt32 RunDllProc1(CharPtr dllName, CharPtr procName, CharPtr arg1)
{
	CDebugContextHandle context    (dllName, procName, false);
	CDebugContextHandle arg1Context("arg1", arg1, false);

	LPFNDLLFUNC1 runFunc = (LPFNDLLFUNC1) GetProcChecked(dllName, procName);
	return (*runFunc)(arg1);
}

RTC_CALL UInt32 RunDllProc2(CharPtr dllName, CharPtr procName, CharPtr arg1, CharPtr arg2)
{
	CDebugContextHandle context(dllName, procName, false);
	CDebugContextHandle arg1Context("arg1", arg1, false);
	CDebugContextHandle arg2Context("arg2", arg2, false);

	LPFNDLLFUNC2 runFunc = (LPFNDLLFUNC2) GetProcChecked(dllName, procName);
	return (*runFunc)(arg1, arg2);
}

RTC_CALL UInt32 RunDllProc3(CharPtr dllName, CharPtr procName, CharPtr arg1, CharPtr arg2, CharPtr arg3)
{
	CDebugContextHandle context(dllName, procName, false);
	CDebugContextHandle arg1Context("arg1", arg1, false);
	CDebugContextHandle arg2Context("arg2", arg2, false);
	CDebugContextHandle arg3Context("arg3", arg3, false);

	LPFNDLLFUNC3 runFunc = (LPFNDLLFUNC3) GetProcChecked(dllName, procName);
	return (*runFunc)(arg1, arg2, arg3);
}


#include "RtcInterface.h"

bool g_IsTerminating = false;

RTC_CALL void DMS_CONV DMS_Terminate()
{
	g_IsTerminating = true;
}
