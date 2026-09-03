// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_ACT_MAINTHREAD_H)
#define __RTC_ACT_MAINTHREAD_H

#include "RtcBase.h"

#include "Parallel.h"
#include <condition_variable>
#include <map>
#include <deque>
#include <functional>

enum {
	WM_QT_ACTIVATENOTIFIERS = 0x402,
	UM_PROCESS_QUEUE = 0x8010,
	UM_PROCESS_MAINTHREAD_OPERS = 0x8003,
	UM_SCALECHANGE = 0x8011,
	UM_COPYDATA = 0x8004,
};

//----------------------------------------------------------------------
// section : Operation Queues
//----------------------------------------------------------------------

using operation_type = std::function<void()>;
struct operation_queue
{
	RTC_CALL bool Post(operation_type&& func); // returns true if the queue was empty before posting
	void Send(operation_type&& func);

	RTC_CALL void Process();
	bool Empty() const;
	RTC_CALL bool SynchonizedEmpty() const;

private:
	std::vector<operation_type> m_Operations;
};

using suspendible_task_type = std::function<bool(bool)>;
using suspendible_task_array_type = std::deque<suspendible_task_type>;
using suspendible_task_map_type = std::map<phase_number, suspendible_task_array_type>;

struct suspendible_task_queue
{
	bool Post(phase_number fn, suspendible_task_type&& task); // returns true if the queue was empty before posting
//	RTC_CALL void Send(operation_type&& func);

	void Process();
	void CancelTasks();

	bool Empty() const;

private:
	suspendible_task_map_type m_OperationMap;
};

/********** helper funcs  **********/

RTC_CALL void   SetMainThreadID() noexcept;
void   SetMetaThreadID() noexcept;
RTC_CALL bool   IsMainThread() noexcept;
RTC_CALL bool   IsMetaThread() noexcept;
bool   NoOtherThreadsStarted();
bool   IsElevatedThread();
UInt32 GetCallCount();
RTC_CALL UInt32 GetThreadID(); // exported: qtgui DmsMainWindow needs it in Debug links (/OPT:REF strips the reference in Release)
// The ceiling of a posted oper is deliberately UNCONSTRAINED -- opers do real work (GUI garbage
// collection, theme updates, deferred reporting) and any DMS_CALLEE_ENTERS level here would be
// false. That is only sound because operation_queue::Process runs them with no GLOBAL leveled
// section held: the queue lock is released before the first oper runs, and every pump site
// (ProcessMainThreadOpers and its callers) holds at most per-item locks -- which Process asserts
// (#1227, #1233). Under a per-item lock an oper may still take every global section; what it may
// not take, a per-item lock at an equal or higher item level, the checker refuses by itself. An
// oper that must not run while this thread holds the token registry is already deferred by
// RequestMainThreadOperProcessingBlocker (see IndexedString_shared_lock in sym/Token.h).
RTC_CALL void PostMainThreadOper(operation_type&& func);
void SendMainThreadOper(operation_type&& func);
RTC_CALL void PostMainThreadTask(phase_number fn, suspendible_task_type&& task);

// Called by DMS_Rtc_Terminate() when the run is over: from here on nothing drains these queues, so
// posting to them is dropped rather than left for a static destructor to trip over.
RTC_CALL void CloseMainThreadQueues();
RTC_CALL void ProcessMainThreadOpers();
void ProcessSuspendibleTasks();
RTC_CALL void ProcessMainThreadOpersAndTasks();
bool HasMainThreadTasks();
RTC_CALL void CancelMainThreadTasks();
RTC_CALL bool IsProcessingMainThreadOpers();
RTC_CALL void RequestMainThreadOperProcessing();
RTC_CALL void ConfirmMainThreadOperProcessing();
#if !defined(WIN32)
RTC_CALL void SetRequestMainThreadOperProcessingCallback(std::function<void()> callback);
#endif
bool IsMainThreadOperProcessingRequestPending();

//----------------------------------------------------------------------
// section : responsive waiting for task-state notifications (#1156)
//----------------------------------------------------------------------
// WaitForTaskNotification replaces cv.wait_for(lock, 500ms) at spots where the
// main thread can park while joining work. Off the main thread it is exactly
// that. On the main thread (Windows) it blocks in MsgWaitForMultipleObjectsEx
// instead: user32 then has the thread in the waiting-for-input state, so the
// hang detector never declares its windows "Not Responding", and the arrival of
// any message (input, posted, or cross-thread sent) also ends the wait --
// without delivering anything, so no window procedure runs mid-computation.
// Callers must re-check their wait predicate in a loop (all current callers do);
// a message wake-up simply reaches their MustSuspend()/predicate check earlier.
// Completion notifiers pair every cv.notify_all() with WakeUpMainThreadWaiter()
// so the main thread still wakes promptly; a missed pairing only costs the
// 500ms timeout, not correctness.
void WaitForTaskNotification(std::condition_variable& cv, std::unique_lock<std::mutex>& lock);
void WakeUpMainThreadWaiter() noexcept;

struct MainThreadBlocker
{
	RTC_CALL MainThreadBlocker();
	RTC_CALL ~MainThreadBlocker();
};

struct RequestMainThreadOperProcessingBlocker
{
	RTC_CALL RequestMainThreadOperProcessingBlocker();
	RTC_CALL RequestMainThreadOperProcessingBlocker(const RequestMainThreadOperProcessingBlocker&);
	RTC_CALL ~RequestMainThreadOperProcessingBlocker();
	// Declared explicitly to silence -Wdeprecated-copy (a user copy-ctor+dtor deprecates the
	// implicit copy-assignment). The class is stateless, so a defaulted no-op matches prior behavior.
	RequestMainThreadOperProcessingBlocker& operator=(const RequestMainThreadOperProcessingBlocker&) = default;
};

#endif // __RTC_ACT_MAINTHREAD_H