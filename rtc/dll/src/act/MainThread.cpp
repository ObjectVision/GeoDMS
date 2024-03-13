// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#if defined(WIN32)
#include <windows.h>

#else

#endif

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

#if defined(WIN32)

void SetPriority()
{
	HANDLE currThread = GetCurrentThread();
	SetThreadPriority(currThread, THREAD_PRIORITY_ABOVE_NORMAL);
	if (sPriorityThread)
		SetThreadPriority(sPriorityThread, THREAD_PRIORITY_NORMAL);
	sPriorityThread = currThread;
}

bool IsElevatedThread()
{
	return GetThreadPriority(GetCurrentThread()) >= THREAD_PRIORITY_ABOVE_NORMAL;
}

#else //defined(WIN32)

// GNU TODO

void SetPriority() {}

bool IsElevatedThread()
{
	return false;
}

#endif //defined(WIN32)

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
	s_OperQueue.emplace_back(std::move(func));
	SuspendTrigger::DoSuspend();
}

static UInt32 s_ProcessMainThreadOperLevel = 0;
RTC_CALL bool IsProcessingMainThreadOpers()
{
	assert(IsMainThread());
	return s_ProcessMainThreadOperLevel != 0;
}

MainThreadBlocker::MainThreadBlocker()
{
	++s_ProcessMainThreadOperLevel;
}

MainThreadBlocker::~MainThreadBlocker()
{
	--s_ProcessMainThreadOperLevel;
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


UInt32 GetNrVCPUs()
{
	auto nrVCPUs = std::thread::hardware_concurrency();
	if (nrVCPUs < 1)
		return 1;
	return nrVCPUs;
}

RTC_CALL UInt32 MaxConcurrentTreads()
{
	if (!IsMultiThreaded1())
		return 1;
	return GetNrVCPUs();
}
