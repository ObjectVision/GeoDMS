// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "act/MainThread.h"
#include "dbg/SeverityType.h"
#include "mem/FixedAlloc.h"
#include "utl/MemGuard.h"
#include "utl/Environment.h"

#if defined(WIN32)

#include <windows.h>
#include <psapi.h>

#endif

using percentage_type = UInt32;

#if defined(WIN32)
struct memory_info {
	memory_info() {
		memStat.dwLength = sizeof(MEMORYSTATUSEX);
		if (!GlobalMemoryStatusEx(&memStat))
			throwLastSystemError("GlobalMemoryStatusEx");
		dms_assert(memStat.dwMemoryLoad == (memStat.ullTotalPhys - memStat.ullAvailPhys) / (memStat.ullTotalPhys / 100)); // do we get it?
	}

	percentage_type ExpectedMemoryLoad(std::size_t requestedSize) const
	{
		if (requestedSize > memStat.ullTotalPhys)
			return percentage_type(-1);
		return (requestedSize + memStat.ullTotalPhys - memStat.ullAvailPhys) / (memStat.ullTotalPhys / 100);
	}
	percentage_type CurrMemoryLoad() const
	{
		return ExpectedMemoryLoad(0);
	}

	percentage_type MaxPhysMemoryLoad() const { return RTC_GetRegDWord(RegDWordEnum::MemoryFlushThreshold); }

	bool SufficientFreeSpace(std::size_t requestedSize) const
	{
		if (!MaxPhysMemoryLoad() || MaxPhysMemoryLoad() == 100)
			return true;
		percentage_type expectedPhysicalMemoryLoad = ExpectedMemoryLoad(requestedSize);
		return expectedPhysicalMemoryLoad <= MaxPhysMemoryLoad();
	}

	bool IsLowOnFreeRAM() const
	{
		return !SufficientFreeSpace(0);
//		return memStat.ullAvailPhys / (memStat.ullTotalPhys / 256) < (256 / 8);
	}

	MEMORYSTATUSEX memStat;
};

bool SufficientFreeSpace(std::size_t requestedSize) {
	memory_info info;
	return info.SufficientFreeSpace(requestedSize);
}

bool IsLowOnFreeRAM() {
	memory_info info;
	return info.IsLowOnFreeRAM();
}

std::atomic<SizeT> s_CumulativeMemoryAllocCount = 0;

#endif //defined(WIN32)

void ConsiderMakingFreeSpace(SizeT sz)
{
#if defined(WIN32)
	s_CumulativeMemoryAllocCount += sz;
	if (s_CumulativeMemoryAllocCount >= 100000000) // only check for clean-up after 100MB of cumulative allocation
	{
		s_CumulativeMemoryAllocCount = 0;

		if (!SufficientFreeSpace(sz))
		{
			// TODO, SEE FROM http://msdn.microsoft.com/query/dev10.query?appId=Dev10IDEF1&l=EN-US&k=k(EMPTYWORKINGSET);k(DevLang-%22C%2B%2B%22);k(TargetOS-WINDOWS)&rd=true
			// Programs that must run on earlier versions of Windows as well as Windows 7 and later versions should always call this function as K32EmptyWorkingSet. 
			// To ensure correct resolution of symbols, add Psapi.lib to the TARGETLIBS macro and compile the program with -DPSAPI_VERSION=1. To use run-time dynamic linking, load Psapi.dll.

			K32EmptyWorkingSet(GetCurrentProcess());
			SendMainThreadOper([] 
				{
					reportD(SeverityTypeID::ST_MajorTrace, "Calling EmptyWorkingSet to release used pages to the standby status.");
//					DBG_DebugReport();
				}
			);
		}
	}
#else //defined(WIN32)

	// GNU TODO

#endif //defined(WIN32)

}

const UInt32 minWaitTime   =  100; // milliseconds
const UInt32 minReportTime = 2000; // milliseconds
const UInt32 maxWaitTime   = 6000; // milliseconds

void WaitForAvailableMemory(std::size_t requestedSize)
{
	ConsiderMakingFreeSpace(requestedSize);
/*
	memory_info info;

	if (info.SufficientFreeSpace(requestedSize))
		return;

	UInt32 waitTime = minWaitTime, cumulWaitTime = 0;

	percentage_type expectedPhysicalMemoryLoad = info.ExpectedMemoryLoad(requestedSize);
	percentage_type currPhysicalMemoryLoad = info.CurrMemoryLoad();
	percentage_type firstLoad = currPhysicalMemoryLoad;

	do {
		if (waitTime >= minReportTime)
			reportF(ST_MajorTrace, "Waiting %u.%03u[s] to allow system to flush modified data before requesting a new page view of %u[kiB]. "
					"PhysicalMemoryLoad would go from %u%% to %u%%; target %u%%", 
				waitTime / 1000, waitTime % 1000, requestedSize / 1024,
				currPhysicalMemoryLoad, expectedPhysicalMemoryLoad, info.MaxPhysMemoryLoad()
			);

		Wait(waitTime); cumulWaitTime += waitTime;
		waitTime *= 2;
		memory_info newInfo;
		expectedPhysicalMemoryLoad = newInfo.ExpectedMemoryLoad(requestedSize);
		currPhysicalMemoryLoad = newInfo.CurrMemoryLoad();
		if (expectedPhysicalMemoryLoad <= newInfo.MaxPhysMemoryLoad())
			goto exit;
	}	while (waitTime < maxWaitTime);

#if defined(DMS32)

	// TODO, SEE FROM http://msdn.microsoft.com/query/dev10.query?appId=Dev10IDEF1&l=EN-US&k=k(EMPTYWORKINGSET);k(DevLang-%22C%2B%2B%22);k(TargetOS-WINDOWS)&rd=true
	// Programs that must run on earlier versions of Windows as well as Windows 7 and later versions should always call this function as K32EmptyWorkingSet. 
	// To ensure correct resolution of symbols, add Psapi.lib to the TARGETLIBS macro and compile the program with -DPSAPI_VERSION=1. To use run-time dynamic linking, load Psapi.dll.

	reportD(ST_MajorTrace, "Calling EmptyWorkingSet to release used pages to the standby status.");
	EmptyWorkingSet(GetCurrentProcess());

#endif

	UInt32 waitCount = 0;
	while (waitCount++ < 5 || expectedPhysicalMemoryLoad >= 99)
	{
		if (expectedPhysicalMemoryLoad <= info.MaxPhysMemoryLoad())
			goto exit;

		reportF(ST_MajorTrace, "Waiting %u[s] to allow system to flush modified data.", maxWaitTime / 1000);

		Wait(maxWaitTime); cumulWaitTime += maxWaitTime;
		memory_info newInfo;
		expectedPhysicalMemoryLoad = newInfo.ExpectedMemoryLoad(requestedSize);
		currPhysicalMemoryLoad = newInfo.CurrMemoryLoad();
	}
	reportD(ST_MajorTrace, "Resuming anyway, hoping the System page file might save the system to provide sufficient resources.");
exit:
	reportF(ST_MajorTrace, "Waited %u.%03u[s] to allow modified data to be flushed before requesting a new page view of %u[kiB]. "
			"PhysicalMemoryLoad went from %u%% to %u%% and is expected to go to %u%%.", 
		UInt32(cumulWaitTime / 1000), UInt32(cumulWaitTime % 1000), UInt32(requestedSize / 1024),
		firstLoad, currPhysicalMemoryLoad, expectedPhysicalMemoryLoad
	);
	*/
}
