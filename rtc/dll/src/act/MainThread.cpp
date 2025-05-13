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

#include "act/MainThread.h"
#include "act/TriggerOperator.h"
#include "dbg/Check.h"
#include "dbg/DmsCatch.h"
#include "utl/Environment.h"

#include <future>

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

void SetPriority()  noexcept
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

void SetMainThreadID()  noexcept
{
	assert(sMainThreadID == sThreadID); // must be set from this thread or not set/called at all.
	sMainThreadID = GetThreadID();
	sMetaThreadID = GetThreadID();
	assert(sMainThreadID == 1);
	sMainThreadHnd = GetCurrentThreadId();
	assert(sMainThreadHnd);

	SetPriority();
}

void SetMetaThreadID() noexcept
{
	sMetaThreadID = GetThreadID();
	SetPriority();
}

bool IsMainThread() noexcept
{
	assert(sMainThreadID); // must be set prior.
	return GetThreadID() == sMainThreadID;
}

bool IsMetaThread() noexcept
{
	assert(sMetaThreadID); // must be set prior.
	return GetThreadID() == sMetaThreadID;
}

bool NoOtherThreadsStarted()
{
	return IsMainThread() && (sThreadID == 1);
}

std::atomic<bool> s_MainThreadOperProcessRequestPending = false;

static thread_local UInt32 s_MainThreadOperProcessingRequestLockCounter = 0;
static thread_local bool   s_MainThreadOperProcessingRequested = false;

RequestMainThreadOperProcessingBlocker::RequestMainThreadOperProcessingBlocker()
{
	if (!s_MainThreadOperProcessingRequestLockCounter++)
	{
		assert(!s_MainThreadOperProcessingRequested);
	}
}
RequestMainThreadOperProcessingBlocker::RequestMainThreadOperProcessingBlocker(const RequestMainThreadOperProcessingBlocker&)
{
	assert(s_MainThreadOperProcessingRequestLockCounter);
	s_MainThreadOperProcessingRequestLockCounter++;
}

RequestMainThreadOperProcessingBlocker::~RequestMainThreadOperProcessingBlocker()
{
	assert(s_MainThreadOperProcessingRequestLockCounter);
	if (--s_MainThreadOperProcessingRequestLockCounter)
		return;

	if (s_MainThreadOperProcessingRequested)
	{
		RequestMainThreadOperProcessing();
		s_MainThreadOperProcessingRequested = false;
	}
}

RTC_CALL void RequestMainThreadOperProcessing()
{
	if (s_MainThreadOperProcessingRequestLockCounter)
	{
		s_MainThreadOperProcessingRequested = true;
		return; // come back later when the last RequestMainThreadOperProcessingBlocker is being  destructed.
	}

	if (s_MainThreadOperProcessRequestPending.exchange(true)) // a request was alredy posted?
	{
		static std::time_t lastPostTime = 0;
		auto currTime = std::time(nullptr);
		if (lastPostTime + 5 > currTime)
			return;
		lastPostTime = currTime; // only post again every 5 second
	}
	if (sMainThreadHnd)  // not yet initialized.
		PostThreadMessage(sMainThreadHnd, UM_PROCESS_MAINTHREAD_OPERS, 0, 0); // only effective when MainThread has MessageQueue, ie in GeoDmsGui, and not in GeoDmsRun or python process

	WakeUpJoiners();
}

RTC_CALL void ConfirmMainThreadOperProcessing()
{
	assert(IsMainThread());
	assert(!SuspendTrigger::DidSuspend());

	s_MainThreadOperProcessRequestPending = false;

	assert(!SuspendTrigger::DidSuspend());

//	MSG msg;
//	auto peekResult = PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE); // create a message queue for the main thread.
}

RTC_CALL bool IsMainThreadOperProcessingRequestPending()
{
	return s_MainThreadOperProcessRequestPending;
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
	assert(IsMetaThread());
	assert(!SuspendTrigger::DidSuspend());

	decltype(m_Operations) operQueue;
	{
		auto lock = std::scoped_lock(s_MainQueueSection);
		operQueue = std::move(m_Operations);
		assert(m_Operations.empty());
	}

	SuspendTrigger::SilentBlocker blockSuspensions("operation_queue::Process");
	for (auto& oper : operQueue)
	{
		try {
			assert(!SuspendTrigger::DidSuspend());
			oper();
		}
		catch (...)
		{
			catchAndReportException();
		}
	}
}

bool operation_queue::Empty() const
{ 
	assert(!s_MainQueueSection.try_lock());
	return m_Operations.empty(); 
}

bool operation_queue::SynchonizedEmpty() const
{
	auto lock = std::scoped_lock(s_MainQueueSection);
	return Empty();
}

bool suspendible_task_queue::Post(fence_number fn, suspendible_task_type&& task)
{
	auto lock = std::scoped_lock(s_MainQueueSection);
	bool result = m_OperationMap.empty();
	m_OperationMap[fn].emplace_back(std::move(task));
	return result;
}

void suspendible_task_queue::Process()
{
	assert(IsMetaThread());
	while (!SuspendTrigger::MustSuspend())
	{
		suspendible_task_type currTask; // Current task being processed
		fence_number fn;
		{
			auto lockForPoppingTask = std::scoped_lock(s_MainQueueSection);
			if (Empty())
				return;

			auto taskMapFrontIter = m_OperationMap.begin();
			fn = taskMapFrontIter->first;
			assert(!taskMapFrontIter->second.empty());
			currTask = std::move(taskMapFrontIter->second.front());
			taskMapFrontIter->second.pop_front();
			if (taskMapFrontIter->second.empty())
				m_OperationMap.erase(taskMapFrontIter);
		}

		assert(!SuspendTrigger::DidSuspend());
		try {
			bool suspended = !currTask(false);
			assert(suspended == SuspendTrigger::DidSuspend());
			if (suspended)
			{
				// If the task was not completed, move it back to the frony of the queue for its fence number
				auto lockForReinsertingTask = std::scoped_lock(s_MainQueueSection);

				m_OperationMap[fn].emplace_front(std::move(currTask));
				return;
			}
		}
		catch (...)
		{
			catchAndReportException();
			currTask(true); // let this task bell 
		}
		assert(!SuspendTrigger::DidSuspend());
	}
}

void suspendible_task_queue::CancelTasks()
{
	assert(IsMetaThread());
	assert(!SuspendTrigger::DidSuspend());

	decltype(m_OperationMap) localTaskArrayMap;
	{
		auto lock = std::scoped_lock(s_MainQueueSection);
		localTaskArrayMap = std::move(m_OperationMap);
		assert(m_OperationMap.empty());
	}

	for (auto& taskArray : localTaskArrayMap)
		for (auto& task : taskArray.second)
		{
			try {
				assert(!SuspendTrigger::DidSuspend());
				task(true);
			}
			catch (...)
			{}
		}
}

bool suspendible_task_queue::Empty() const 
{ 
	assert(!s_MainQueueSection.try_lock());
	return m_OperationMap.empty();
}

operation_queue s_OperQueue;
suspendible_task_queue s_TaskQueue;


void PostMainThreadOper(operation_type&& func)
{
	if (s_OperQueue.Post(std::move(func)))
		RequestMainThreadOperProcessing();
}

void SendMainThreadOper(operation_type&& func)
{
	s_OperQueue.Send(std::move(func));
}

void PostMainThreadTask(fence_number fn, suspendible_task_type&& func)
{
	if (s_TaskQueue.Post(fn, std::move(func)))
		RequestMainThreadOperProcessing();
}

static UInt32 s_ProcessMainThreadOperLevel = 0;

bool IsProcessingMainThreadOpers()
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
	assert(!SuspendTrigger::DidSuspend());

	if (!s_ProcessMainThreadOperLevel)
	{

		StaticStIncrementalLock<s_ProcessMainThreadOperLevel> avoidReenty;

		assert(!SuspendTrigger::DidSuspend());
		s_OperQueue.Process();
	}
	assert(!SuspendTrigger::DidSuspend());
	s_TaskQueue.Process();
}

void CancelMainThreadTasks()
{
	assert(IsMetaThread());

	s_TaskQueue.CancelTasks();
}

#include "ASync.h"

leveled_std_section cs_ThreadMessing(item_level_type(0), ord_level_type::ThreadMessing, "LockedThreadMessing");
std::condition_variable cv_TaskCompleted;

RTC_CALL void wakeUpJoiners()
{
	assert(!cs_ThreadMessing.try_lock());
	cv_TaskCompleted.notify_all();
}

bool HasMainThreadTasks()
{
	auto lock = std::scoped_lock(s_MainQueueSection);

	if (!s_TaskQueue.Empty())
		return true;
	if (s_ProcessMainThreadOperLevel)
		return false;
	if (s_OperQueue.Empty())
		return true;
	return false;
}

RTC_CALL void WakeUpJoiners()
{
	leveled_critical_section::scoped_lock lockToAvoidHasMainThreadTasksToBeMissed(cs_ThreadMessing);

	wakeUpJoiners();
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

// make it constant to avoid rounding off errors to depend on architecture or settings.
RTC_CALL UInt32 MaxAllowedConcurrentTreads()
{
	return 32;
}

