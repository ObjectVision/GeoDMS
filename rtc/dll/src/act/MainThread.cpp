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
#include "dbg/DmsCatch.h"
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

DWORD sMainThreadHnd = 0;

void SetMainThreadID()
{
	assert(sMainThreadID == sThreadID); // must be set from this thread or not set/called at all.
	sMainThreadID = GetThreadID();
	sMetaThreadID = GetThreadID();
	assert(sMainThreadID == 1);
	sMainThreadHnd = GetCurrentThreadId();
	assert(sMainThreadHnd);

	SetPriority();
}

void SetMetaThreadID()
{
	sMetaThreadID = GetThreadID();
	SetPriority();
}

bool IsMainThread()
{
	assert(sMainThreadID); // must be set prior.
	return GetThreadID() == sMainThreadID;
}

bool IsMetaThread()
{
	assert(sMetaThreadID); // must be set prior.
	return GetThreadID() == sMetaThreadID;
}

bool NoOtherThreadsStarted()
{
	return IsMainThread() && (sThreadID == 1);
}

std::atomic<bool> s_MainThreadOperProcessRequestPending = false;

RTC_CALL void RequestMainThreadOperProcessing()
{
	if (!sMainThreadHnd)
		return; // not yet initialized.

	if (!s_MainThreadOperProcessRequestPending.exchange(true))
		PostThreadMessage(sMainThreadHnd, UM_PROCESS_MAINTHREAD_OPERS, 0, 0); // only effective when MainThread has MessageQueue, ie in GeoDmsGui, and not in GeoDmsRun or python process
}

RTC_CALL void ConfirmMainThreadOperProcessing()
{
	assert(IsMainThread());
	s_MainThreadOperProcessRequestPending = false;

//	MSG msg;
//	auto peekResult = PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE); // create a message queue for the main thread.
}

//----------------------------------------------------------------------
// section : Operation Queues
//----------------------------------------------------------------------

std::mutex s_MainQueueSection;

bool operation_queue::Post(operation_type&& func)
{
	auto lock = std::scoped_lock(s_MainQueueSection);
	bool result = m_Operations.empty();
	m_Operations.emplace_back(std::move(func));
	return result;
}

void operation_queue::Send(operation_type&& func)
{
	if (IsMetaThread())
		func();
	else
		PostMainThreadOper(std::move(func));
}

void operation_queue::Process()
{
	assert(IsMainThread());
	decltype(m_Operations) operQueue;
	{
		auto lock = std::scoped_lock(s_MainQueueSection);
		operQueue = std::move(m_Operations);
		assert(m_Operations.empty());
	}
	for (auto& oper : operQueue)
	{
		try {
			oper();
		}
		catch (...)
		{
			catchAndReportException();
		}
	}
}

operation_queue s_OperQueue;

void PostMainThreadOper(std::function<void()>&& func)
{
	if (s_OperQueue.Post(std::move(func)))
		RequestMainThreadOperProcessing();
}

void SendMainThreadOper(std::function<void()>&& func)
{
	s_OperQueue.Send(std::move(func));
}

static UInt32 s_ProcessMainThreadOperLevel = 0;
RTC_CALL bool IsProcessingMainThreadOpers()
{
	assert(IsMetaThread());
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
	assert(IsMetaThread());

	if (s_ProcessMainThreadOperLevel)
		return;

	StaticStIncrementalLock<s_ProcessMainThreadOperLevel> avoidReenty;

	s_OperQueue.Process();
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
