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
#pragma hdrstop

#include "act/MainThread.h"
#include "dbg/SeverityType.h"
#include "mem/FixedAlloc.h"
#include "utl/MemGuard.h"
#include "utl/Environment.h"
#include <windows.h>
#include <psapi.h>

using percentage_type = UInt32;

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
		if (!MaxPhysMemoryLoad())
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
void ConsiderMakingFreeSpace(SizeT sz)
{
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
			AddMainThreadOper([] 
				{
					reportD(SeverityTypeID::ST_MajorTrace, "Calling EmptyWorkingSet to release used pages to the standby status.");
					DBG_DebugReport();
				}
			, true);
		}
	}
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
