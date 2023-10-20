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


#include <windows.h>

#include "LockLevels.h"
#include "Parallel.h"

#include "dbg/Check.h"
#include "act/MainThread.h"
#include "act/TriggerOperator.h"


namespace { // local defs

	THREAD_LOCAL dms_thread_id sThreadID  = 0;
	dms_thread_id sMainThreadID = 0;
	dms_thread_id sMetaThreadID = 0;
	std::atomic<dms_thread_id> sLastThreadID = 0;
	HANDLE sPriorityThread = 0;

} // end anonymous namespace

dms_thread_id GetThreadID()
{
	if (!sThreadID)
		sThreadID = ++sLastThreadID;
	return sThreadID;
}

void SetPriority()
{
	HANDLE currThread = GetCurrentThread();
	SetThreadPriority(currThread, THREAD_PRIORITY_ABOVE_NORMAL);
	if (sPriorityThread)
		SetThreadPriority(sPriorityThread, THREAD_PRIORITY_NORMAL);
	sPriorityThread = currThread;
}

void SetMainThreadID()
{
	assert(sMainThreadID == sThreadID); // must be set from this thread or not set/called at all.
	sMainThreadID = GetThreadID();
	sMetaThreadID = GetThreadID();
	assert(sMainThreadID == 1);
	SetPriority();
}

void SetMetaThreadID()
{
	sMetaThreadID = GetThreadID();
	SetPriority();
}

bool IsMainThread()
{
	dms_assert(sMainThreadID); // must be set prior.
	return GetThreadID() == sMainThreadID;
}

bool IsMetaThread()
{
	dms_assert(sMetaThreadID); // must be set prior.
	return GetThreadID() == sMetaThreadID;
}

bool NoOtherThreadsStarted()
{
	return IsMainThread() && (sThreadID == 1);
}

bool IsElevatedThread()
{
	return GetThreadPriority(GetCurrentThread()) >= THREAD_PRIORITY_ABOVE_NORMAL;
}

std::vector < std::function<void()>>  s_OperQueue;

leveled_std_section s_QueueSection(item_level_type(0), ord_level_type::OperationQueue, "OperationQueue");


void AddMainThreadOper(std::function<void()>&& func, bool postAlways)
{
	if (IsMainThread() && !postAlways)
	{
		ProcessMainThreadOpers();
		func();
		return;
	}
	leveled_std_section::scoped_lock lock(s_QueueSection);
	bool wasEmpty = s_OperQueue.empty();
	s_OperQueue.emplace_back(std::move(func));
	SuspendTrigger::DoSuspend();
}

static UInt32 s_ProcessMainThreadOperLevel = 0;
RTC_CALL bool IsProcessingMainThreadOpers()
{
	assert(IsMainThread());
	return s_ProcessMainThreadOperLevel != 0;
}

void ProcessMainThreadOpers()
{
	assert(IsMainThread());

	if (s_ProcessMainThreadOperLevel)
		return;
	StaticStIncrementalLock<s_ProcessMainThreadOperLevel> avoidReenty;

	decltype(s_OperQueue) operQueue;
	{
		auto lock = leveled_std_section::scoped_lock(s_QueueSection);
		operQueue = std::move(s_OperQueue);
		s_OperQueue.clear();
	}
	for (auto& oper : operQueue)
		oper();
}

#include "ASync.h"

std::atomic<UInt32>& throttle_counter()
{
	static std::atomic<UInt32> the_counter;
	return the_counter;
}


#include <concrtrm.h>

UInt32 GetNrVCPUs()
{
	return concurrency::GetProcessorCount();
}
