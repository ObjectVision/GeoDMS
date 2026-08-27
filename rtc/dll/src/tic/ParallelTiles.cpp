// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "TicPCH.h"
#include "act/UpdateMark.h" // UpdateMarker

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// tile_task_group: the work-stealing pool of commissioned tile-task slots
// (declared in ParallelTiles.h) with its global registry s_TileTaskGroups,
// ticketing, exception propagation and vCPU-bounded thread commissioning.
// Split out of OperationContext.cpp (2026-08); the operation-level scheduler
// stays there.

#include "OperationContext.h"
#include "utl/Registry.h" // resource_scheduling, RTC_GetRegDWord
#include "utl/MgFormat.h"

#include <deque>

#include "Parallel.h"
#include "DbgInterface.h" // DBG_ReportBoundaryException
#include "dbg/Diagnostics.h"
#include "dbg/Timer.h"
#include "dbg/DmsCatch.h"
#include "dbg/SeverityType.h"
#include "ser/AsString.h"
#include "utl/MemGuard.h"
#include "utl/StrFormat.h"
#include "utl/scoped_exit.h"
#include "utl/IncrementalLock.h"
#include "xct/DmsException.h"

#include "LockLevels.h"
#include "LispTreeType.h"

#include "DataLocks.h"
#include "SessionData.h"
#include "Operator.h"
#include "MoreDataControllers.h"
#include "PerfMeasurement.h"
#include "stg/AbstrStorageManager.h" // #933: NonmappableStorageManager + m_CriticalSection

// *****************************************************************************
// Section:     tile_task_group
// *****************************************************************************
//
// A light-weight ticket dispenser for tile-level tasks.
// - Multiple tile_task_group instances can coexist. Each instance has 'm_Last'
//   indicating the total number of tiles (tickets).
// - Global array s_TileTaskGroups functions as a registry for stealing.
// - Threads grab a commissioned index and process it; completion is tracked.
// - Destructor waits for all slots to be completed or propagates exception.
//
// *****************************************************************************

#include "ParallelTiles.h"
#include <numeric>

// Global registry of tile task groups for work stealing.
static std::vector<tile_task_group*> s_TileTaskGroups;
// Guards s_TileTaskGroups.
std::mutex s_TileTaskGroupsMutex;
// Global cancellation flag for tile tasks (currently unused here).
static bool s_IsCancelled = false;

// Number of worker threads currently running DoThisOrThatAndDecommission loop.
static UInt32 s_NrRunningTileTaskThreads = 0;

bool s_OcTaskGroupIsCanceling = false; // also read/written by OperationContext.cpp


void CheckThis(tile_task_group* self)
{
#if defined(MG_DEBUG)
	assert(!s_TileTaskGroupsMutex.try_lock()); // don't let the notification fall outside a waiter lock

	auto nrCompleted = std::accumulate(self->md_CompletedWork.begin(), self->md_CompletedWork.end(), UInt32(0), std::plus<>());
	assert(nrCompleted == self->m_NrCompleted);
#endif
}

// Internal: pop a valid commissioned slot from any registered tile_task_group.
// Requires s_TileTaskGroupsMutex to be held.
auto takeOneTileTask() -> std::pair<tile_task_group*, tile_task_group::IndexType>
{
	assert(!SuspendTrigger::DidSuspend());
	assert(!s_TileTaskGroupsMutex.try_lock());
	while (!s_TileTaskGroups.empty())
	{
		auto* taskGroup = s_TileTaskGroups.back();
		if (taskGroup)
		{
			auto i = taskGroup->getNextCommissioned();
			if (IsDefined(i))
				return { taskGroup, i };
			if (SuspendTrigger::DidSuspend())
				break;
		}
		s_TileTaskGroups.pop_back();
	}
	return { nullptr, UNDEFINED_VALUE(tile_task_group::IndexType) };
}

// Try to take a tile task. Thread-safe wrapper.
auto TakeOneTileTask() -> std::pair<tile_task_group*, tile_task_group::IndexType>
{
	assert(!SuspendTrigger::DidSuspend());

	auto lock = std::unique_lock<std::mutex>(s_TileTaskGroupsMutex);
	return takeOneTileTask();
}

// Take a tile task or decide the caller should decommission its worker thread.
auto TakeOneTaskOrDecommissionThread() -> std::pair<tile_task_group*, tile_task_group::IndexType>
{
	auto lock = std::unique_lock<std::mutex>(s_TileTaskGroupsMutex);
	auto tileTask = takeOneTileTask();
	if (!SuspendTrigger::DidSuspend())
		if (!tileTask.first)
		{
			assert(s_NrRunningTileTaskThreads > 0);
			--s_NrRunningTileTaskThreads; // no task available, caller must decommission this thread
		}
	return tileTask;
}

// Attempt to steal and execute a single tile task. Returns true if did work.
void StealTasks()
{
	auto tileTask = TakeOneTileTask();
	if (tileTask.first)
	{
		assert(!SuspendTrigger::DidSuspend());
		assert(IsDefined(tileTask.second)); // we assume starting with a valid ticket to a slot, or else this tile_task_group may already be destroyed.
		tileTask.first->DoWork(tileTask.second);
	}
}

// Worker loop that executes tile tasks until no tasks remain, then exits.
void DoThisOrThatAndDecommission()
{
	SuspendTrigger::SilentBlocker blockSuspensionInWorkerTask("DoThisOrThatAndDecommission");
	assert(!SuspendTrigger::DidSuspend());

	try {
		while(true)
		{
			auto tileTask = TakeOneTaskOrDecommissionThread();
			assert(!SuspendTrigger::DidSuspend());
			if (!tileTask.first)
				return; // TakeOneTaskOrDecommissionThread already did the decommission accounting
			assert(IsDefined(tileTask.second)); // we assume starting with a valid ticket to a slot, or else this tile_task_group may already be destroyed.
			tileTask.first->DoWork(tileTask.second);
			assert(!SuspendTrigger::DidSuspend());
		}
	}
	catch (...)
	{
		// DoWork catches everything itself (it has to: it owns the slot bookkeeping), so what lands here
		// came from TakeOneTaskOrDecommissionThread or the SilentBlocker. That path never reached the
		// branch that decrements the running-thread count, so do it here: left too high, it permanently
		// throttles tile parallelism, because CommissionThreads only hands out
		// maxNrThreads - s_NrRunningTileTaskThreads slots. And an escape from here would terminate the
		// process, this being a task_group task with nothing above it to catch (#1191).
		DBG_ReportBoundaryException("DoThisOrThatAndDecommission");

		auto lock = std::unique_lock<std::mutex>(s_TileTaskGroupsMutex);
		assert(s_NrRunningTileTaskThreads > 0);
		if (s_NrRunningTileTaskThreads > 0)
			--s_NrRunningTileTaskThreads;
	}
}

// Construct a tile task group with 'last' slots and a work function.
// Commissions worker threads based on vCPU availability.
tile_task_group::tile_task_group(IndexType last, task_func func)
	: m_Last(last)
	, m_Func(func)
	, m_CallingContext(CancelableFrame::CurrActive())
{
#if defined(MG_DEBUG)
	md_CompletedWork.resize(m_Last);
#endif

	UInt32 nrThreadsToCommission = 0;
	if (m_Last > 0)
	{
		auto lock = std::unique_lock<std::mutex>(s_TileTaskGroupsMutex);
		s_TileTaskGroups.emplace_back(this);

		// Fire workers, bounded by available vCPUs and remaining slots.
		if (IsMultiThreaded1())
		{
			auto maxNrThreads = GetNrVCPUs();
			if (s_NrRunningTileTaskThreads < maxNrThreads)
			{
				nrThreadsToCommission = maxNrThreads - s_NrRunningTileTaskThreads;
				if (last < nrThreadsToCommission)
					nrThreadsToCommission = last;
			}
		}
		s_NrRunningTileTaskThreads += nrThreadsToCommission;
	}

	// Launch workers outside of lock.
	while (nrThreadsToCommission-- > 0)
		GetPortableTaskGroup().run([] { DoThisOrThatAndDecommission(); });




}

// Destructor waits for all commissioned slots to complete and propagates exceptions.
tile_task_group::~tile_task_group()
{
	auto lock = std::unique_lock<std::mutex>(s_TileTaskGroupsMutex);
	if (m_Commissioned < m_Last) // recheck if decommission is still required
	{
		auto remainingTasks = (m_Last - m_Commissioned); // current slot still has to be registered as completed, after which this task_group may be destructed.
		auto firstBulkSlot = m_Commissioned;
		m_Commissioned += remainingTasks;
		CheckThis(this);
		assert(m_Commissioned == m_Last);
		decommission(); // stop handing out slots for this task group, so that no new tasks will be started, but the current ones can still finish.
#if defined(MG_DEBUG)
		// Keep CheckThis's sum-vs-counter invariant valid across the bulk
		// completion path. registerCompletions(N) below bumps m_NrCompleted
		// by N for the uncommissioned slots without touching md_CompletedWork;
		// mark those slots here so the per-slot bits stay in sync with the
		// counter. The asserted-zero precondition catches a stray double-
		// completion if a slot somehow got both individually and bulk-completed.
		for (IndexType i = firstBulkSlot; i < m_Last; ++i)
		{
			assert(md_CompletedWork[i] == 0);
			md_CompletedWork[i] = 1;
		}
#endif
		registerCompletions(remainingTasks); // other tasks in flight might still come in before m_TileTasksDone can be notified.
	}
	assert(m_Commissioned == m_Last); // we now expect to have completed all commissioned task-slots.
	while (true)
	{
		if (m_NrCompleted >= m_Last)
			break;

		WaitForTaskNotification(m_TileTasksDone, lock);
	}
	assert(m_NrCompleted == m_Last); // we now expect to have completed all commissioned task-slots.
}

// Remove this group from the global registry, preventing new slots to be handed out.
void tile_task_group::decommission()
{
	assert(!s_TileTaskGroupsMutex.try_lock()); // don't let the notification fall outside a waiter lock
	assert(m_Commissioned == m_Last);

	auto ttgFirst = s_TileTaskGroups.begin();
	auto ttgPtr = s_TileTaskGroups.end();

	while (ttgPtr != ttgFirst)
		if (*--ttgPtr == this)
		{
			*ttgPtr = nullptr; // decommission this task group
			break;
		}
}

// Commission next slot if available and return the ticket; otherwise undefined.
auto tile_task_group::getNextCommissioned() -> IndexType
{
	assert(!SuspendTrigger::DidSuspend());
	assert(!s_TileTaskGroupsMutex.try_lock()); // don't let the notification fall outside a waiter lock

	if (m_Commissioned >= m_Last)
		return UNDEFINED_VALUE(IndexType);

	if (SuspendTrigger::MustSuspend())
		return UNDEFINED_VALUE(IndexType);

	auto result = m_Commissioned++; // ownership of this slot of the task_group is now obtained and will continue until this thread causes m_NrCompleted to increment accordingly
	CheckThis(this);
	assert(result < m_Last);
	if (m_Commissioned >= m_Last)
		decommission(); // stop handing out slots for this task group, so that no new tasks will be started, but the current ones, including the slot that the caller must complete, can still finish.
	return result;
}

// Thread-safe wrapper to get a commissioned slot.
auto tile_task_group::GetNextCommissioned() -> IndexType
{
	auto lock = std::unique_lock<std::mutex>(s_TileTaskGroupsMutex);
	return getNextCommissioned();
}

// Register multiple completions (e.g., on exception settle path).
void tile_task_group::registerCompletions(IndexType nr)
{
	assert(!s_TileTaskGroupsMutex.try_lock()); // don't let the notification fall outside a waiter lock

	assert(nr);
	m_NrCompleted += nr;
	CheckThis(this);
	assert(m_NrCompleted <= m_Last);
	if (m_NrCompleted == m_Last)
	{
		m_TileTasksDone.notify_all();
		WakeUpMainThreadWaiter();
	}
}

bool tile_task_group::registerCompletion(IndexType i)
{
	assert(!s_TileTaskGroupsMutex.try_lock()); // don't let the notification fall outside a waiter lock

	assert(m_NrCompleted < m_Last);

#if defined(MG_DEBUG)
	assert(md_CompletedWork[i] == 0);
	md_CompletedWork[i] = 1;
#endif

	auto nrCompleted = ++m_NrCompleted; // don't increment outside lock as it may cause another thread to Join and destruct
	CheckThis(this);
	if (nrCompleted != m_Last)
		return false;
	m_TileTasksDone.notify_all();
	WakeUpMainThreadWaiter();
	return true;
}

/*
// Register a single completion.
void tile_task_group::RegisterCompletion(IndexType i)
{
	auto lock = std::unique_lock<std::mutex>(s_TileTaskGroupsMutex); // don't let the notification fall outside a waiter lock

	registerCompletion(i); // perform debug checks and bookkeeping for this completion
}
*/

// Register completion and immediately try to commission a next slot (if any).
auto tile_task_group::RegisterCompletionAndGetNextCommissioned(IndexType i)->IndexType
{
	assert(!SuspendTrigger::DidSuspend());
	auto lock = std::unique_lock<std::mutex>(s_TileTaskGroupsMutex); // don't let the notification fall outside a waiter lock

	auto isCompleted = registerCompletion(i); // perform debug checks and bookkeeping for this completion

	if (isCompleted)
		return UNDEFINED_VALUE(IndexType);

	return getNextCommissioned();
}

// Execute slot 'i' and optionally continue with more slots (doMore=true).
// Handles cancellation and exception paths, ensuring proper decommissioning.
void tile_task_group::DoWork(IndexType i)
{
	assert(!std::uncaught_exceptions());
	assert(IsDefined(i)); // we assume starting with a valid ticket to a slot, or else this tile_task_group may already be destroyed.
	try {
		CancelableFrame frame(m_CallingContext);
		while (IsDefined(i))
		{
			CancelIfOutOfInterest(); // don't continue if m_CallingContext or Process is cancelling
			ASyncContinueCheck(); // additional check, set from CloseConfig
			if (s_OcTaskGroupIsCanceling)
				throw task_canceled{};

			UpdateMarker::PrepareDataInvalidatorLock preventInvalidations;
			m_Func(i);

			i = RegisterCompletionAndGetNextCommissioned(i); // release this slot and obtain the next one, or none if this task group is done.
		}
//		assert((m_Commissioned == m_Last) || SuspendTrigger::DidSuspend()); // post condition, but this task_group may already be destructed by owner
	}
	catch (...)
	{
		auto lock = std::unique_lock<std::mutex>(s_TileTaskGroupsMutex);
		if (m_ExceptionPtr)
		{
#if defined(MG_DEBUG)
			// This slot's work threw after another thread already set
			// m_ExceptionPtr; we still need to count its completion. Mark
			// md_CompletedWork[i] here so the registerCompletions(1) below
			// keeps the sum-vs-counter invariant valid in CheckThis.
			assert(md_CompletedWork[i] == 0);
			md_CompletedWork[i] = 1;
#endif
			registerCompletions(1); // release this slot of the task_group, so from here, this may be destroyed; decommissioning already was done by an earlier settler of m_Exception.
		}
		else
		{
			m_ExceptionPtr = std::current_exception();
			auto firstBulkSlot = m_Commissioned;
			auto remainingTasks = m_Last - m_Commissioned; // current slot still has to be registered as completed, after which this task_group may be destructed.
			m_Commissioned += remainingTasks;
			CheckThis(this);
			assert(m_Commissioned == m_Last);
			decommission(); // stop handing out slots for this task group, so that no new tasks will be started, but the current ones can still finish.
#if defined(MG_DEBUG)
			// Same bookkeeping fix as the destructor's bulk-completion
			// branch: registerCompletions(1 + remainingTasks) below bumps
			// m_NrCompleted by (1 + remainingTasks) - the failed slot i
			// plus the uncommissioned tail - without touching
			// md_CompletedWork. Mark all of those slots so the invariant
			// in CheckThis holds.
			assert(md_CompletedWork[i] == 0);
			md_CompletedWork[i] = 1;
			for (IndexType k = firstBulkSlot; k < m_Last; ++k)
			{
				assert(md_CompletedWork[k] == 0);
				md_CompletedWork[k] = 1;
			}
#endif
			registerCompletions(1 + remainingTasks); // other tasks in flight might still come in before m_TileTasksDone can be notified.
		}
//		assert(m_Commissioned == m_Last); // post condition, but this task_group may already be destructed by owner
	}
}

// Wait until all commissioned slots are completed. No exceptions thrown.
void tile_task_group::AwaitRunningSlots() 
{
	MG_CHECK(m_Commissioned == m_Last); // post condition of DoWork

	while (true)
	{
		// TODO: this can cause other tiles to process that use the same m_Mutex in FutureTileFunctor::tile_record::GetTile
		// if (StealOneTileTask(false))
		//	 continue;

		auto lock = std::unique_lock<std::mutex>(s_TileTaskGroupsMutex);

		assert(m_NrCompleted <= m_Last);

		if (m_NrCompleted >= m_Last)
			break;

		if (m_CallingContext)
			CancelIfOutOfInterest();
		ASyncContinueCheck(); // can throw !

		WaitForTaskNotification(m_TileTasksDone, lock);
	}
}

// Join ensures completion of all tasks and rethrows any stored exception.
void tile_task_group::Join()
{
	assert(!SuspendTrigger::DidSuspend());
	SuspendTrigger::SilentBlocker dontSuspendThis("tile_task_group::Join()");

	IndexType nextSlot = GetNextCommissioned();
	if (IsDefined(nextSlot))
		DoWork(nextSlot);
	assert(!SuspendTrigger::DidSuspend());
	assert(m_Commissioned == m_Last); // post condition of DoWork

	AwaitRunningSlots();
	// no more other worker threads can access this task group, so we can safely access m_ExceptionPtr now.

	if (m_ExceptionPtr) 
		std::rethrow_exception(m_ExceptionPtr);
}
