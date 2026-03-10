// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////
//
// Overview
// --------
// This file implements the core scheduling and execution machinery for
// OperationContext and tile_task_group processing. It orchestrates:
//  - A cooperative task-stealing scheduler for both OperationContext tasks and
//    fine-grained tile tasks (tile_task_group).
//  - Phase-based activation: operations are grouped by 'phase_number' and only
//    operations in the current active phase are allowed to run.
//  - Supplier/waiter dependency management: operations wait for their suppliers
//    to complete (or fail) before activation.
//  - Low-RAM heuristics: limits parallel activations when RAM is low.
//  - Integration with a global task_group for background execution,
//    while allowing inline execution on join.
//  - Robust, lock-disciplined state transitions with extensive debug checks.
//
// Threading model
// ---------------
// - cs_ThreadMessing protects all shared scheduling state: queues, phase maps,
//   counters, and OperationContext life-cycle changes.
// - s_TileTaskGroupsMutex protects the global list of tile_task_group instances.
// - task_group from PPL executes background functors.
//
// OperationContext life-cycle
// ---------------------------
// States (task_status):
//   none -> waiting_for_suppliers (if suppliers exist) -> scheduled -> activated -> running -> [done|exception|cancelled]
// - Schedule* methods enqueue contexts and manage supplier connections.
// - collectOperationContexts scans scheduled queues (by phase), activates contexts,
//   and transfers them to the "radio actives" queue for potential inline/parallel run.
// - TryRunningTaskInline tries to acquire a unique run license and inlines execution.
// - Join waits for completion; on non-main threads it steals work; on main thread it
//   pumps UI-related work and respects suspend triggers.
// - OnEnd/separateResources finalize state, detach waiters/suppliers, and free resources.
//	
// tile_task_group
// ---------------
// Manages a pool of tile tasks with ticketing (commissioned slots) and cooperatively
// executes them via the same task_group. It supports:
// - Work stealing by threads via TakeOneTileTask.
// - Exception propagation and early decommissioning.
// - Controlled thread commissioning based on available vCPUs.
//
// Important invariants and practices
// ----------------------------------
// - All modifications to scheduling state happen under cs_ThreadMessing.
// - All modifications to tile_task_group registry happen under s_TileTaskGroupsMutex.
// - Notifications (cv_TaskCompleted) are signaled while holding cs_ThreadMessing to
//   avoid missed wakeups.
// - No lock order inversions: keep lock ordering consistent and avoid acquiring
//   cs_ThreadMessing from tile_task_group paths and vice versa where possible.
// - Exceptions from tasks are caught and transformed into state transitions.
//
// Debug support
// -------------
// - MG_TRACE_OPERATIONCONTEXTS provides extensive consistency checks and tracing.
// - sd_RunningOC/s_NrActivatedOrRunningOperations are kept in sync via asserts.
//
//

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include <windows.h>

#include "OperationContext.h"

#include <agents.h>
#include <deque>
#include <ppl.h>

#include "Parallel.h"
#include "dbg/Check.h"
#include "dbg/DmsCatch.h"
#include "dbg/SeverityType.h"
#include "ser/AsString.h"
#include "utl/memGuard.h"
#include "utl/mySPrintF.h"
#include "utl/scoped_exit.h"
#include "utl/IncrementalLock.h"
#include "xct/DmsException.h"

#include "LockLevels.h"
#include "LispTreeType.h"

#include "DataLocks.h"
#include "DataStoreManagerCaller.h"
#include "Operator.h"
#include "MoreDataControllers.h"
RTC_CALL void NotifyCurrentTargetCount();


#if defined(MG_DEBUG)
const bool MG_DEBUG_FUNCCONTEXT = false;
#endif

#if defined(MG_DEBUG)
const bool MG_DEBUGCONNECTIONS = false;
#else
const bool MG_DEBUGCONNECTIONS = false;
#endif //defined(MG_DEBUG)

concurrency::task_group& GetTaskGroup();

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

static concurrency::task_group* s_OcTaskGroup = nullptr;
static bool s_OcTaskGroupIsCanceling = false;


void CheckThis(tile_task_group* self)
{
#if defined(MG_DEBUG)
	assert(!s_TileTaskGroupsMutex.try_lock()); // don't let the notification fall outside a waiter lock

	auto nrCompleted = std::accumulate(self->md_CompletedWork.begin(), self->md_CompletedWork.end(), 0, std::plus<>());
	assert(nrCompleted == self->m_NrCompleted);

	if (self->m_Commissioned == 858 && self->m_Last == 857)
	{
		reportD(SeverityTypeID::ST_Warning, "Daar gaan we");
	}
	if (self->m_NrCompleted == 856 && self->m_Last == 857)
	{
		reportD(SeverityTypeID::ST_Warning, "KoekKoek");
	}
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

	while(true)
	{
		auto tileTask = TakeOneTaskOrDecommissionThread();
		assert(!SuspendTrigger::DidSuspend());
		if (!tileTask.first)
			return;
		assert(IsDefined(tileTask.second)); // we assume starting with a valid ticket to a slot, or else this tile_task_group may already be destroyed.
		tileTask.first->DoWork(tileTask.second);
		assert(!SuspendTrigger::DidSuspend());
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
		GetTaskGroup().run([] { DoThisOrThatAndDecommission(); });
}

// Destructor waits for all commissioned slots to complete and propagates exceptions.
tile_task_group::~tile_task_group()
{
	auto lock = std::unique_lock<std::mutex>(s_TileTaskGroupsMutex);
	if (m_Commissioned < m_Last) // recheck if decommission is still required
	{
		auto remainingTasks = (m_Last - m_Commissioned); // current slot still has to be registered as completed, after which this task_group may be destructed.
		m_Commissioned += remainingTasks;
		CheckThis(this);
		assert(m_Commissioned == m_Last);
		decommission(); // stop handing out slots for this task group, so that no new tasks will be started, but the current ones can still finish.
		registerCompletions(remainingTasks); // other tasks in flight might still come in before m_TileTasksDone can be notified.
	}
	assert(m_Commissioned == m_Last); // we now expect to have completed all commissioned task-slots.
	while (true)
	{
		if (m_NrCompleted >= m_Last)
			break;

		m_TileTasksDone.wait_for(lock, std::chrono::milliseconds(500));
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
		m_TileTasksDone.notify_all();
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
			DSM::CancelIfOutOfInterest(); // don't continue if m_CallingContext or Process is cancelling
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
			registerCompletions(1); // release this slot of the task_group, so from here, this may be destroyed; decommissioning already was done by an earlier settler of m_Exception.
		else
		{
			m_ExceptionPtr = std::current_exception();
			auto remainingTasks = m_Last - m_Commissioned; // current slot still has to be registered as completed, after which this task_group may be destructed.
			m_Commissioned += remainingTasks;
			CheckThis(this);
			assert(m_Commissioned == m_Last);
			decommission(); // stop handing out slots for this task group, so that no new tasks will be started, but the current ones can still finish.
			registerCompletions(1 + remainingTasks); // other tasks in flight might still come in before m_TileTasksDone can be notified.
		}
//		assert(m_Commissioned == m_Last); // post condition, but this task_group may already be destructed by owner
	}
}

// Wait until all commissioned slots are completed. No exceptions thrown.
void tile_task_group::AwaitRunningSlots() noexcept
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
			DSM::CancelIfOutOfInterest();
		ASyncContinueCheck();

		m_TileTasksDone.wait_for(lock, std::chrono::milliseconds(500));
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

// *****************************************************************************
// Section:     OperatorContextHandle
// *****************************************************************************
//
// Lightweight ContextHandle for debugging/describing the running operator and
// its arguments during scheduling/execution.
//
// *****************************************************************************

struct OperatorContextHandle : ContextHandle
{
	OperatorContextHandle(bool mustCalc, const FuncDC* funcDC)
		: m_MustCalc(mustCalc), m_FuncDC(funcDC)
	{}

protected:
void GenerateDescription() override;

private:
	bool              m_MustCalc;
	const FuncDC*     m_FuncDC;
};

/********** Implementation **********/

void OperatorContextHandle::GenerateDescription()
{
	UInt32 c = 0;
	auto operID= m_FuncDC->m_OperatorGroup->GetNameID();

	SharedStr msg = mySSPrintF("while operator %s is %s\n",
		GetTokenStr(operID).c_str(),
		(m_MustCalc
			? "calculating data"
			: "creating a result item"));

	for (auto argi = m_FuncDC->GetArgList(); argi; argi = argi->m_Next.get())
	{
		auto arg = argi->m_DC->GetOld();
		msg += mySSPrintF(
			"arg%u: %s of type %s\n",
			++c,
			arg ? arg->GetName().c_str() : "<nullptr>",
			arg ? arg->GetClsName().c_str() : "<nullptr>"
		);
	}

	SetText(msg);
}


// *****************************************************************************
// Section:     OperatorContextQueue
// *****************************************************************************
//
// Central scheduling structures and synchronization for OperationContext.
// - s_ScheduledContextsMap: per-phase queues of weak_ptrs ready to be activated.
// - s_RadioActives: "radio-active" queue of contexts that have been activated and
//   are candidates for inline execution or stealing.
// - s_NrActivatedOrRunningOperations: per-phase counters (activated+running).
// - s_CurrActivePhaseNumber: currently focus phase; only this phase is activated.
//
// *****************************************************************************

leveled_std_section cs_ThreadMessing(item_level_type(0), ord_level_type::ThreadMessing, "LockedThreadMessing");
std::condition_variable cv_TaskCompleted;

phase_number s_CurrBlockedPhaseNumber = 0;
const TreeItem* s_CurrBlockedPhaseItem = nullptr;
const TreeItem* s_CurrPhaseContainer = nullptr;

using context_array = std::vector<OperationContextWPtr>;
using contexts_within_one_phase = std::deque<OperationContextWPtr>;
using RunningOperationsCounter = UInt32;

// protected by exclusive lock on cs_ThreadMessing
static std::map<phase_number, contexts_within_one_phase> s_ScheduledContextsMap;
static contexts_within_one_phase s_RadioActives; // contexts that were collected for running, some of them may be already running, made for work stealing and prioritizing in case the GUI thread calls Join on a specific context.
static std::map<phase_number, RunningOperationsCounter> s_NrActivatedOrRunningOperations; 
static phase_number s_CurrActivePhaseNumber = 0;
static bool s_IsInLowRamMode = false;
static UInt32 s_CurrFinishedCount = 0;

static std::atomic<UInt32> s_NrWaitingJoins = 0;
UInt32 GetCurrFinishedCount()
{
	leveled_critical_section::scoped_lock lockToAvoidHasMainThreadTasksToBeMissed(cs_ThreadMessing);
	return s_CurrFinishedCount;
}
// *****************************************************************************
// Section:     extra debug stuff
// *****************************************************************************
//
// Debug-only bookkeeping to assert consistency of running counts and
// to report leaked OperationContext instances.
//
// *****************************************************************************

#if defined(MG_DEBUG)

static UInt32 sd_runOperationContextsRecursionCount = 0;
static std::set<const OperationContext*> sd_ManagedContexts;

#endif

#if defined(MG_DEBUG)
#define MG_TRACE_OPERATIONCONTEXTS
#endif

#define MG_TRACE_OPERATIONCONTEXTS

#if defined(MG_TRACE_OPERATIONCONTEXTS)

leveled_critical_section cs_OcAdm(item_level_type(0), ord_level_type::OperationContext, "OperationContextSet");

static UInt32 sd_OcCount;
static std::set<OperationContext*> sd_OC;
static std::map<OperationContext*, phase_number> sd_RunningOC;

SizeT getNumberOfActivatedOrRunningOperations()
{
	SizeT numberOfRunningOCs = 0;
	for (const auto& levelNumberPair : s_NrActivatedOrRunningOperations)
		numberOfRunningOCs += levelNumberPair.second;
	MG_CHECK(numberOfRunningOCs == sd_RunningOC.size());

	return numberOfRunningOCs;
}

SizeT GetNumberOfActivatedOrRunningOperations()
{
	leveled_critical_section::scoped_lock lockToAvoidHasMainThreadTasksToBeMissed(cs_ThreadMessing);

	return getNumberOfActivatedOrRunningOperations();
}

// Verify that administrative counters agree.
void CheckNumberOfRunningOCConsistency()
{
	MG_CHECK(!cs_ThreadMessing.try_lock());
		
	SizeT numberOfRunningOCs = getNumberOfActivatedOrRunningOperations();

	std::map<phase_number, RunningOperationsCounter> recount;
	for (const auto& runningPair : sd_RunningOC)
	{
		MG_CHECK(runningPair.first->m_PhaseNumber == runningPair.second);
		++recount[runningPair.second];
	}
	for (const auto& recountPair : recount)
	{
		MG_CHECK(s_NrActivatedOrRunningOperations[recountPair.first] == recountPair.second);
	}
}

// Debug reporting helper.
void reportOC(CharPtr source, OperationContext* ocPtr)
{
	auto item = ocPtr->GetResult();
	reportF_without_cancellation_check(MsgCategory::other, SeverityTypeID::ST_MajorTrace, "OperationContext %s %d: %s"
		, source
		, int(ocPtr->GetStatus())
		, item ? item->GetFullName().c_str() : ""
	);
}
auto reportRemaingOcOnExit = make_scoped_exit([]()
	{
		for (auto ocPtr : sd_OC)
			reportOC("Leaked", ocPtr);
	});

#endif

// Notify waiting threads that a task completed or state changed.
void wakeUpJoiners()
{
	assert(!cs_ThreadMessing.try_lock());
	cv_TaskCompleted.notify_all();
}

TIC_CALL void WakeUpJoiners()
{
	leveled_critical_section::scoped_lock lockToAvoidHasMainThreadTasksToBeMissed(cs_ThreadMessing);

	wakeUpJoiners();
}

// *****************************************************************************
// Section: GetTaskGroup() en tg_maintainer
// 
// Purpose:
//  - Initialize the default PPL scheduler with vCPU based policy.
//  - Maintain a global task_group used by OperationContext and tile tasks.
// 
// note: caller is responsible for having exactly one tg_maintainer object in main() or at least during calls to GetTaskGroup();
// note: GetTaskGroup() returns a singleton, but calls to GetTaskGroup().run(...) can be made from different threads
// 
// *****************************************************************************

TIC_CALL tg_maintainer::tg_maintainer()
{
	// build policy: pin min/max concurrency to number of vCPUs
	concurrency::SchedulerPolicy policy(2, concurrency::MinConcurrency, GetNrVCPUs(), concurrency::MaxConcurrency, GetNrVCPUs());

	// install that policy as the DEFAULT scheduler’s policy --
	// must do this *before* any parallel work runs
	concurrency::Scheduler::SetDefaultSchedulerPolicy(policy);

	assert(!s_OcTaskGroup);
	s_OcTaskGroup = new concurrency::task_group;
}

TIC_CALL tg_maintainer::~tg_maintainer()
{
	assert(s_OcTaskGroup);
	s_OcTaskGroupIsCanceling = true;
	{
		leveled_std_section::scoped_lock lock(cs_ThreadMessing);

		dbg_assert(sd_runOperationContextsRecursionCount == 0);

		s_ScheduledContextsMap.clear();
		s_RadioActives.clear();

		assert(s_ScheduledContextsMap.empty());
		assert(s_RadioActives.empty());
	}
	s_OcTaskGroup->cancel();
	s_OcTaskGroup->wait();

	assert(sd_RunningOC.empty() || g_IsTerminating);
//	assert(sd_OC.empty() || g_IsTerminating); issue with scheduled PrepareDataReadTasks that hold a readInfoPtr that holds a StorageMetaInfo that refCounts the m_Curr item that holds its PrepareDataReadTask in m_ReadAssets until the read has started.

	delete s_OcTaskGroup;
	s_OcTaskGroup = nullptr;
}

// Global access to the task group, must be initialized by tg_maintainer.
concurrency::task_group& GetTaskGroup()
{
	assert(s_OcTaskGroup);
	return *s_OcTaskGroup;
}

// *****************************************************************************
// Section: PhaseNumbers
// *****************************************************************************
//
// Provide increasing unique phase identifiers for scheduling phases.
//
// *****************************************************************************

auto GetNextPhaseNumber() -> phase_number
{
	static std::atomic<phase_number> s_SchedulingFenceNumber = 1;
	return ++s_SchedulingFenceNumber;
}

// *****************************************************************************
// Section: helper functions
// *****************************************************************************
//
// Utilities for weak_ptr comparison, queue lookup, supplier checks etc.
//
// *****************************************************************************

// Find the position of 'self' in its phase queue. Requires cs_ThreadMessing.
SizeT getScheduledContextsPos(OperationContextSPtr self)
{
	assert(!cs_ThreadMessing.try_lock());
	auto& queue = s_ScheduledContextsMap[self->m_PhaseNumber];
	auto pos = queue.begin(), end = queue.end();
	for (; pos != end; ++pos)
		if (pos->lock() == self)
			return pos - queue.begin();
	return UNDEFINED_VALUE(SizeT);
}

bool hasSuppliers(OperationContextSPtr waiter)
{
	return waiter && waiter->m_Suppliers.size();
}

bool equal(OperationContextWPtr a, OperationContextWPtr b)
{
	if (a.owner_before(b) || b.owner_before(a))
		return false;

#if defined(MG_DEBUG)
	auto sa = a.lock();
	auto sb = b.lock();
	assert(!sa && !sb || sa.get() == sb.get());
#endif
	return true;
}

template <typename Set, typename Elem>
bool contains(const Set& set, const Elem& elem)
{
	return set.find(elem) != set.end();
}

bool isSupplier(OperationContext* waiter, OperationContextSPtr supplier)
{
	return waiter && contains(waiter->m_Suppliers, supplier);
}

#if defined(MG_DEBUG)

// Debug printing helpers for OperationContext pointers.
std::string AsText(OperationContext* ptr)
{
	assert(ptr);

	auto funcDC = ptr->GetFuncDC();
	return mgFormat2string("status=%x %s", (int)ptr->getStatus(), funcDC ? funcDC->md_sKeyExpr.c_str(): "(leeg)");
}

template <typename T>
std::string AsText(std::shared_ptr<T> xPtr)
{
	auto uc = xPtr.use_count();
	return mgFormat2string("share_ptr %x %x %s", xPtr.get(), uc, AsText(xPtr.get()));
}

template <typename T>
std::string AsText(std::weak_ptr<T> xPtr)
{
	auto xShr = xPtr.lock();
	if (xShr)
		return AsText(xShr);
	auto uc = xPtr.use_count();
	return mgFormat2string("expired weakPtr with uc=%s", uc);
}

#endif

// Connect 'waiter' to a 'supplier' OperationContext to receive completion/fail events.
void connect(OperationContext* waiter, OperationContextSPtr supplier)
{
	DBG_START("OperationContextPtr", "connect", MG_DEBUGCONNECTIONS);
	DBG_TRACE(("waiter   = %s", AsText(waiter)));
	DBG_TRACE(("supplier = %s", AsText(supplier)));

	assert(!cs_ThreadMessing.try_lock());
	if (isSupplier(waiter, supplier))
		return;

	waiter->m_Suppliers.insert(supplier);
	supplier->m_Waiters.insert(waiter->weak_from_this());
}

// Enqueue a runnable context into its phase queue and record scheduling state.
void scheduleRunnableTask(OperationContext* self)
{
	assert(!cs_ThreadMessing.try_lock());
	assert(self->m_Status < task_status::scheduled);

	// next StartOperationContexts() must not start scheduling tasks with phase numbers higher than this task.
	auto fn = self->m_PhaseNumber;
	MakeMin<phase_number>(s_CurrActivePhaseNumber, fn);
	assert(s_CurrBlockedPhaseNumber == 0 || s_CurrBlockedPhaseNumber >= s_CurrActivePhaseNumber);

	s_ScheduledContextsMap[fn].emplace_back(self->weak_from_this());
	self->releaseRunCount(task_status::scheduled);
	assert(self->m_Status == task_status::scheduled);
	assert(!self->m_FuncDC || self->GetOperator()->CanRunParallel());
	assert(IsDefined(getScheduledContextsPos(self->shared_from_this()))); // always true for scheduled tasks outzide cs_ThreadMessing
}

// *****************************************************************************
// Section: disconnect suppliers
// *****************************************************************************
//
// Helper transitions for finalization: detach from suppliers and notify waiters.
// Ensures consistent state transitions and triggers follow-up scheduling.
//
// *****************************************************************************

// Disconnect specific supplier and handle state transitions based on supplier status.
garbage_can OperationContext::disconnect_supplier(OperationContext* supplier)
{
	DBG_START("OperationContextPtr", "disconnect_supplier", MG_DEBUGCONNECTIONS);

	assert(!cs_ThreadMessing.try_lock());

	OperationContextWPtr wptr_supplier = supplier->weak_from_this();
	auto status = getStatus();
	assert(status != task_status::running);
	assert(status != task_status::activated);
	assert(status != task_status::scheduled);
	if (status > task_status::running)
	{
		assert(status != task_status::done); 
		assert(m_Suppliers.empty());
	}
	assert(!supplier->m_FuncDC || !supplier->m_FuncDC->m_OperContext);

	garbage_can garbage;
	if (OperationContextSPtr sptr_supplier = wptr_supplier.lock())
	{
		m_Suppliers.erase(sptr_supplier);
		garbage |= std::move(sptr_supplier);
	}

#if defined(MG_DEBUG)
	for (const auto& s : m_Suppliers)
	{
		assert(s.get() != supplier);
	}
#endif

	auto supplierStatus = supplier->getStatus();
	switch (supplierStatus)
	{
	case task_status::done:
		assert(supplier->m_Result);
		assert(IsDataReady(supplier->m_Result->GetCurrUltimateItem()) || m_Status >= task_status::running);
		if (!m_Suppliers.empty())
			break;

		if (m_Status == task_status::waiting_for_suppliers)
			scheduleRunnableTask(this);
		assert(m_Status != task_status::waiting_for_suppliers);
		break;

	case task_status::exception:
	{
		SharedTreeItem supplResult = supplier->GetResult();
		if (supplResult)
			garbage |= make_movable_scoped_exit([self = weak_from_this(), supplResult]
				{
					if (auto selfSPtr = self.lock())
					{
						selfSPtr->GetResult()->Fail(supplResult);
						selfSPtr->OnEnd(task_status::exception);
					}
				}
			);
		break;
	}
	case task_status::cancelled:
		garbage |= make_movable_scoped_exit([self = weak_from_this()]
			{
				if (auto selfSPtr = self.lock())
					selfSPtr->OnEnd(task_status::cancelled);
			}
		);
		break;

	default:
		assert(supplierStatus != task_status::running);
		assert(supplierStatus != task_status::suspended);
		assert(supplierStatus != task_status::scheduled);
		assert(supplierStatus != task_status::waiting_for_suppliers);
		assert(supplierStatus != task_status::none);
	}

	return garbage;
}

// Disconnect all waiters and suppliers; to be called under cs_ThreadMessing.
garbage_can OperationContext::disconnect_waiters()
{
	DBG_START("OperationContextPtr", "disconnect_waiters", MG_DEBUGCONNECTIONS);
	DBG_TRACE(("this = %s", AsText(this)))

	assert(cs_ThreadMessing.isLocked());

#if defined(MG_DEBUG)
	assert( sd_ManagedContexts.find(this) != sd_ManagedContexts.end() || !m_FuncDC);
	sd_ManagedContexts.erase(this);
#endif

	assert(!cs_ThreadMessing.try_lock());

	garbage_can garbage;
	garbage |= std::move(m_Suppliers);

	dbg_assert(m_Suppliers.empty()); // DEBUG

	if (!m_Waiters.empty())
	{
		auto waiters = std::move(m_Waiters);
		for (const auto& waiterWPtr: waiters)
			if (auto waiterSPtr = waiterWPtr.lock())
			{
				garbage |= waiterSPtr->disconnect_supplier(this);
				garbage |= std::move(waiterSPtr);
			}

		m_Waiters.clear();
	}
	return garbage;
}

/// <summary>
///  collectOperations that must be set to GetOperGroup() outside the cs_ThreadMessing lock by the caller
/// </summary>
/// <returns>
/// a pair of context_array that contains the collected operations and garbage that must be destructed before calling GetOperGroup, but after teh cs_ThreadMessing lock.
/// </returns>
//
// Collect and activate OperationContexts in the current phase until RAM pressure
// or queue exhaustion. Returns the list of activated contexts (as weak_ptrs)
// and a garbage_can for post-lock destruction.
//
auto collectOperationContexts() -> std::pair<context_array, garbage_can>
{
	assert(!cs_ThreadMessing.try_lock());

#if defined(MG_DEBUG)
	assert(!sd_runOperationContextsRecursionCount);
	DynamicIncrementalLock s_ActivationGuard(sd_runOperationContextsRecursionCount);
#endif

	context_array results;
	garbage_can cancelGarbage;

	while (!s_ScheduledContextsMap.empty())
	{
		auto nextPhaseNumber = s_ScheduledContextsMap.begin()->first;

		assert(s_CurrBlockedPhaseNumber == 0 || s_CurrBlockedPhaseNumber >= s_CurrActivePhaseNumber);

		// Advance to next phase only when all lower-phase tasks are done.
		if (nextPhaseNumber > s_CurrActivePhaseNumber)
		{
			while (!s_NrActivatedOrRunningOperations.empty())
			{
				auto firstIter = s_NrActivatedOrRunningOperations.begin();
				if (firstIter->second == 0)
				{
					s_NrActivatedOrRunningOperations.erase(firstIter);
					continue;
				}
				if (firstIter->first < nextPhaseNumber)
					goto exit; // try again later, when all operations of the current phase have completed
				break;
			}
			s_CurrActivePhaseNumber = nextPhaseNumber;
			assert(s_CurrBlockedPhaseNumber == 0 || s_CurrBlockedPhaseNumber >= s_CurrActivePhaseNumber); // else we would not be allowd to enter this level as currBlockedPhaseNumber is in the MainStack being executed.
			NotifyCurrentTargetCount();
		}

		auto& scheduledContexts = s_ScheduledContextsMap.begin()->second;

		auto currContext = scheduledContexts.begin();

		// Reset low-RAM throttle for this activation pass.
		s_IsInLowRamMode = false; // reset for next activation round
		bool isLowOnFreeRamTested = false;
		bool isCancelling = false;

		for (; currContext != scheduledContexts.end(); ++currContext)
		{
			// Heuristic: don't activate too many when low on RAM or when too many joins are waiting.
			if (!isLowOnFreeRamTested && s_NrActivatedOrRunningOperations[nextPhaseNumber] >= s_NrWaitingJoins) // HEURISTIC: only allow more than 1 operations when RAM isn't being exausted
			{
				if (IsLowOnFreeRAM())
				{
					s_IsInLowRamMode = true;
					break;
				}
				isLowOnFreeRamTested = true;
			}
			auto operContext = currContext->lock();
			if (!operContext)
				continue;

			cancelGarbage |= operContext->shared_from_this(); // copy shared_ptr into container for destruction consideration outside current critical section
			assert(operContext->m_Status >= task_status::scheduled);
			assert(s_CurrBlockedPhaseNumber == 0 || s_CurrBlockedPhaseNumber >= s_CurrActivePhaseNumber);
			assert(operContext->m_PhaseNumber == s_CurrActivePhaseNumber);
			if (operContext->m_Status < task_status::activated)
			{
				assert(operContext->m_TaskFunc);
				if (isCancelling || DSM::IsCancelling())
				{
					isCancelling = true;
					cancelGarbage |= operContext->separateResources(task_status::cancelled);
					continue;
				}

				assert(operContext->m_Status < task_status::activated);
				assert(operContext->m_TaskFunc);

				// activate and collect task
				if (operContext->collectTaskImpl())
					results.emplace_back(operContext);
				assert(s_NrActivatedOrRunningOperations[nextPhaseNumber] >= 0);
			}
		}
		// Drop processed scheduled contexts.

#if defined(MG_DEBUG)
		if (!isCancelling)
			for (auto contextPtr = scheduledContexts.begin(); contextPtr != currContext; ++contextPtr)
				if (auto activatedContext = contextPtr->lock())
				{
					assert(activatedContext->m_Status != task_status::scheduled);
				}

#endif

		scheduledContexts.erase(scheduledContexts.begin(), currContext);
		if (!scheduledContexts.empty())
			break;

		s_ScheduledContextsMap.erase(s_ScheduledContextsMap.begin());

	}
exit:
	return { std::move(results), std::move(cancelGarbage) };
}

// Collect activated contexts (under lock) and return by value.
auto CollectOperationContextsImpl() -> context_array
{
	context_array activatedOperationContextArray;
	garbage_can receivedGarbage;

	leveled_std_section::scoped_lock lock(cs_ThreadMessing);
	std::tie(activatedOperationContextArray, receivedGarbage) = collectOperationContexts();
	return activatedOperationContextArray;
}

// Schedule execution of contexts via the task_group.
void StartCollectedOperationContexts(context_array collectedActivatedContexts)
{
	for (auto activatedContextWPtr : collectedActivatedContexts)
		if (auto activatedContext = activatedContextWPtr.lock())
		{
			auto selfCaller = [activatedContextWPtr]()
				{
					if (auto self = activatedContextWPtr.lock())
						self->TryRunningTaskInline();
				};

			GetTaskGroup().run(selfCaller);
		}
}

// Entry point to advance scheduling and run newly activated contexts.
TIC_CALL void StartOperationContexts()
{
	auto collectedActivatedContexts = CollectOperationContextsImpl();
	StartCollectedOperationContexts(std::move(collectedActivatedContexts));
}

inline bool IsActiveOrRunning(task_status s) { return s >= task_status::activated && s <= task_status::running; }

// Transition a context to 'activated' and bump counters. Requires cs_ThreadMessing.
void OperationContex_setActivated(OperationContext* self)
{
	assert(self);

	assert(!cs_ThreadMessing.try_lock());
	assert(!IsActiveOrRunning(self->getStatus()));
	assert(self->m_Suppliers.empty());

#if defined(MG_TRACE_OPERATIONCONTEXTS)
	CheckNumberOfRunningOCConsistency();
#endif

	self->m_Status = task_status::activated;
	++s_NrActivatedOrRunningOperations[self->m_PhaseNumber];

	assert(IsActiveOrRunning(self->m_Status));


	assert(s_NrActivatedOrRunningOperations[self->m_PhaseNumber] > 0);

#if defined(MG_TRACE_OPERATIONCONTEXTS)

	auto nrRunningOCs = sd_RunningOC.size();
	MG_CHECK(sd_RunningOC.find(self) == sd_RunningOC.end());
	sd_RunningOC[self] = self->m_PhaseNumber;
	MG_CHECK(sd_RunningOC.size() == nrRunningOCs + 1);

	CheckNumberOfRunningOCConsistency();

#endif
}

// *****************************************************************************
// Section:     OperatorContext
// *****************************************************************************
//
// Implements OperationContext scheduling, execution, dependency management, and
// resource cleanup. See top-of-file overview for the life-cycle.
//
// *****************************************************************************

/// <summary>
///		This constructor is used to create a new OperationContext with a given FuncDC.
/// </summary>
/// <param name="self"></param>
/// 
/// Call ScheduleCalcResult() to start the calculation.
/// todo: integreate ScheduleCalcResult into the constructor.

// Debug/admin counter increment for constructed OperationContext.
void OperationContext_AddOcCount(OperationContext* self)
{

#if defined(MG_TRACE_OPERATIONCONTEXTS)

	leveled_critical_section::scoped_lock lock(cs_OcAdm);
	++sd_OcCount;
	sd_OC.insert(self);

#endif

}

// Construct for a FuncDC-backed operation (operator execution).
OperationContext::OperationContext(const FuncDC* self)
	: m_FuncDC(self)
	, m_PhaseNumber(self->GetPhaseNumber())
{
	assert(m_PhaseNumber);

	m_Result = self->GetOld();

	OperationContext_AddOcCount(this);
}

// Construct for a raw task_func-based operation (custom tasks).
OperationContext::OperationContext(task_func_type func)
	: m_TaskFunc( std::move(func) )
{
	OperationContext_AddOcCount(this);
}

// Destructor enforces finalization and ensures no active/running states linger.
OperationContext::~OperationContext()
{
	DBG_START("OperationContext", "DTor", MG_DEBUG_FUNCCONTEXT);
	DBG_TRACE(("FuncDC: %s", m_FuncDC ? m_FuncDC->md_sKeyExpr.c_str() : "(leeg)"));

	assert(!m_FuncDC || !m_FuncDC->m_OperContext);


	assert(m_Status != task_status::running);
	OnEnd(task_status::cancelled);

	assert(m_Status != task_status::scheduled); // cancel, exception or done caught.
	assert(m_Status == task_status::exception || m_Status == task_status::cancelled || m_Status == task_status::done || m_Status == task_status::none); // cancel, exception or done caught.

	assert(!IsActiveOrRunning(m_Status));

	#if defined(MG_TRACE_OPERATIONCONTEXTS)

	leveled_critical_section::scoped_lock lock(cs_OcAdm);
	--sd_OcCount;
	sd_OC.erase(this);

	#endif

}

// Connect argument suppliers under lock; returns true if any connections were made.
bool OperationContext_ConnectArgs(OperationContext* oc, const FutureSuppliers& allArgInterest)
{
	leveled_std_section::scoped_lock lock(cs_ThreadMessing);

	return oc->connectArgs(allArgInterest);
}

// Schedule a single OperationContext (recursively pre-schedules suppliers if needed).
void OperationContext_scheduleThis(OperationContext* self)
{
	assert(!cs_ThreadMessing.try_lock());

	assert(self);
	assert(!self->m_FuncDC || self->GetOperator()->CanRunParallel());

	if (self->getStatus() != task_status::none)
		return;

	if (self->m_Suppliers.empty())
	{
		scheduleRunnableTask(self);
		assert(self->getStatus() == task_status::scheduled);
	}
	else
	{
		for (const auto& s : self->m_Suppliers)
		{
			auto supplier = s.get();
			assert(supplier);
			if (supplier->m_FuncDC && !supplier->GetOperator()->CanRunParallel())
			{
				assert(supplier->m_Status == task_status::done);
			}
			else
				OperationContext_scheduleThis(supplier);
		}
		self->m_Status = task_status::waiting_for_suppliers;
		assert(!(self->m_Suppliers.empty()));
	}
}

// Thread-safe wrapper to schedule this context.
task_status OperationContext_ScheduleThis(OperationContext* self, bool runDirect, explain_context_ptr_t context)
{
	assert(self);
	if (self->m_FuncDC && !self->GetOperator()->CanRunParallel())
		runDirect = true;

	if (runDirect)
	{
		// Inline path: ensure suppliers are resolved or suspended.
		if (self->GetStatus() < task_status::scheduled)
		{
			auto supplStatus = self->JoinSupplOrSuspendTrigger();
			assert(supplStatus > task_status::running);
			if (supplStatus == task_status::suspended)
				return task_status::suspended;
			assert(self->m_Suppliers.empty() || context);
			if (supplStatus != task_status::done)
			{
				assert(supplStatus == task_status::suspended || supplStatus == self->GetStatus());
				return supplStatus;
			}
		}
		bool canRun = false;
		{
			if (SuspendTrigger::DidSuspend())
				return task_status::suspended;

			leveled_std_section::scoped_lock lock(cs_ThreadMessing);

			if (self->m_Status < task_status::activated)
				OperationContex_setActivated(self);
			canRun = self->getUniqueLicenseToRun(true);
			assert(!SuspendTrigger::DidSuspend());
		}
		if (canRun)
			self->Run_with_cleanup(context);
	}
	else
	{
		// Async path: enqueue for activation.
		assert(!context);

		assert(!self->m_FuncDC || self->GetOperator()->CanRunParallel());
		leveled_std_section::scoped_lock lock(cs_ThreadMessing);

		OperationContext_scheduleThis(self);
	}
	return self->GetStatus();
}

// Core scheduling entry: sets up phase/result/write lock, connects suppliers,
// and either runs inline or enqueues for async execution.
task_status OperationContext::Schedule(TreeItem* item, const FutureSuppliers& allArgInterest, bool runDirect, explain_context_ptr_t context)
{
	assert(IsMetaThread());
	assert(!m_FuncDC || GetOperator()->CanRunParallel() || runDirect);

	assert(UpdateMarker::IsInActiveState());
	assert(m_Status == task_status::none || context);

	assert(IsMetaThread());
	//	dms_assert(!m_TaskFunc);
	assert(m_Status == task_status::none || context);
	assert(m_TaskFunc);
	assert(runDirect || !context); // runDirect must be set when a result collecting context is given.
	if (item)
	{
		auto itemFN = item->GetPhaseNumber();
//		assert(!m_PhaseNumber || m_PhaseNumber == itemFN);
		MakeMax(m_PhaseNumber, itemFN);
		item->m_PhaseNumber = m_PhaseNumber; // set the phase number of the item to the phase number of this operation context

	}

	assert(m_PhaseNumber);


	if (item)
		m_Result = item;
	else
		m_Result = m_FuncDC->GetOld();
	assert(m_Result && m_Result->HasInterest());

	if (!runDirect && !IsMultiThreaded2())
		runDirect = true;

	assert(!runDirect || IsMetaThread());
	assert(m_Status == task_status::none || context);

	m_ActiveTimestamp = UpdateMarker::GetActiveTS(MG_DEBUG_TS_SOURCE_CODE("Obtaining active frame for produce item task"));

	if (item)
		m_WriteLock = ItemWriteLock(item, weak_from_this()); // sets item->m_Producer to this; no future access right yet

	bool connectedArgs = (!allArgInterest.empty()) && OperationContext_ConnectArgs(this, allArgInterest);

	return OperationContext_ScheduleThis(this, runDirect, context);
}

// *****************************************************************************
// Section: CancelableFrame
// *****************************************************************************
//
// Thread-local guard that marks the current OperationContext as "active" for
// cancellation propagation and context-sensitive operations.
//
// *****************************************************************************

THREAD_LOCAL OperationContext* sl_CurrOperationContext = nullptr;

CancelableFrame::CancelableFrame(OperationContext* self)
: m_Prev(sl_CurrOperationContext)
{
	sl_CurrOperationContext = self;
}

CancelableFrame::~CancelableFrame()
{
	sl_CurrOperationContext = m_Prev;
}

OperationContext* CancelableFrame::CurrActive()
{
	return sl_CurrOperationContext;
}

bool CancelableFrame::CurrActiveCanceled()
{
	auto ca = CurrActive();
	return ca && ca->IsCanceled();
}

void CancelableFrame::CurrActiveCancelIfNoInterestOrForced(bool forceCancel)
{
	if (CurrActive())
		CurrActive()->CancelIfNoInterestOrForced(forceCancel);
}

// Simple thread-safe getter for status.
task_status OperationContext::GetStatus() const
{ 
	leveled_std_section::scoped_lock lock(cs_ThreadMessing);
	return m_Status;
}

// Try to collect task payload and transition to 'activated'.
// Adds to s_RadioActives for potential inline/stealing.
bool OperationContext::collectTaskImpl()
{
	assert(!cs_ThreadMessing.try_lock());

	auto res = GetResult();
	assert(res);
	SharedActorInterestPtr resKeeper;
	resKeeper = GetResult()->GetInterestPtrOrNull();

	if (!resKeeper)
		return false;

	assert(s_NrActivatedOrRunningOperations[m_PhaseNumber] >= 0);
	assert(m_Status < task_status::activated);
	assert(m_TaskFunc);

	if (m_FuncDC && !GetOperator()->CanRunParallel())
		return false;

	assert(!IsActiveOrRunning(m_Status));
	assert(s_NrActivatedOrRunningOperations[m_PhaseNumber] >= 0);

	OperationContex_setActivated(this);

	m_ResKeeper = std::move(resKeeper);

	std::weak_ptr<OperationContext> selfWptr = shared_from_this();
	s_RadioActives.emplace_back(selfWptr);
	return true;
}

// Try to run inline: first drain tile work, then acquire license and run.
bool OperationContext::TryRunningTaskInline()
{
	assert(m_Suppliers.empty());
	StealTasks();

	if (!GetUniqueLicenseToRun())
		return false;

	Run_with_cleanup({});
	return true;
}


// Acquire the unique run license under scheduling constraints (phase fences).
// If 'runDirect' is false and the phase is not active, reschedule back.
bool  OperationContext::getUniqueLicenseToRun(bool runDirect)
{
	assert(!cs_ThreadMessing.try_lock());
	auto status = getStatus();

	if (status == task_status::scheduled) // maybe it was placed back from active to scheduled because earlier phases became in focus again.
		return false;

	if (status >= task_status::running)
		return false;

	assert(status == task_status::activated);

	assert(s_CurrBlockedPhaseNumber == 0 || s_CurrBlockedPhaseNumber >= s_CurrActivePhaseNumber);
	if (!runDirect)
		if (m_PhaseNumber > s_CurrActivePhaseNumber)
		{
			scheduleRunnableTask(this); // place it back 
			return false;
		}

	DSM::CancelIfOutOfInterest(m_Result);
	if (GetTaskGroup().is_canceling())
		throw task_canceled{};

	m_Status = task_status::running;
	return true;
}

// Thread-safe wrapper for licensing.
bool OperationContext::GetUniqueLicenseToRun()
{
	leveled_std_section::scoped_lock lock(cs_ThreadMessing);
	return getUniqueLicenseToRun(false);
}

// Transition to 'exception' final state (no-throw wrapper).
void OperationContext::OnException() noexcept
{
	OnEnd(task_status::exception);
}

// Internal helper for OnEnd paths; returns garbage_can for deferred destruction.
garbage_can OperationContext::onEnd(task_status status) noexcept
{
	if (m_Status >= task_status::cancelled)
	{
		assert(m_Suppliers.empty());
		return {};
	}

	assert(status != task_status::exception || m_Result->WasFailed(FailType::Data));

	return separateResources(status);
}

// Finalization entry (thread-safe).
void OperationContext::OnEnd(task_status status) noexcept
{
	assert(status >= task_status::cancelled);

	garbage_can garbage;

	leveled_std_section::scoped_lock lock(cs_ThreadMessing);
	garbage = onEnd(status);
	assert(getStatus() >= task_status::cancelled);
}

// Decrease activated/running count and set 'status'. Requires cs_ThreadMessing.
void OperationContext::releaseRunCount(task_status status)
{
	assert(!IsActiveOrRunning(status));

	assert(cs_ThreadMessing.isLocked());

	assert(m_Status <= task_status::running);
	if (IsActiveOrRunning(m_Status))
	{
		assert(s_NrActivatedOrRunningOperations[m_PhaseNumber] > 0);

#if defined(MG_TRACE_OPERATIONCONTEXTS)
		CheckNumberOfRunningOCConsistency();
		assert(s_NrActivatedOrRunningOperations[m_PhaseNumber] > 0);
#endif
		auto nrRunning = --s_NrActivatedOrRunningOperations[m_PhaseNumber];

#if defined(MG_TRACE_OPERATIONCONTEXTS)

		auto nrRunningOCs = sd_RunningOC.size();
		MG_CHECK(!(sd_RunningOC.find(this) == sd_RunningOC.end()));
		MG_CHECK(sd_RunningOC[this] == this->m_PhaseNumber);
		sd_RunningOC.erase(this);
		MG_CHECK(nrRunningOCs == sd_RunningOC.size() + 1);

		CheckNumberOfRunningOCConsistency();

#endif

		if (!nrRunning && m_PhaseNumber == s_CurrActivePhaseNumber)
			wakeUpJoiners();
	}
	if (status > task_status::running)
		s_CurrFinishedCount++;
	assert(s_NrActivatedOrRunningOperations[m_PhaseNumber] >= 0);
	m_Status = status;
	assert(!IsActiveOrRunning(m_Status));

}

// Core resource separation on final states: drop suppliers, task payload,
// locks, and FuncDC references. Schedules more contexts after release.
garbage_can OperationContext::separateResources(task_status status)
{
	assert(cs_ThreadMessing.isLocked());

	assert(status >= task_status::cancelled); // new status must be a final status
	if (m_Status >= task_status::cancelled) // don't change an already established final status
		return {};
	
	releaseRunCount(status);
	assert(getStatus() == status);

	garbage_can releaseBin;
	if (!m_Suppliers.empty())
	{
		assert(status != task_status::done);
		for (const auto& supplier : m_Suppliers)
			m_Waiters.erase(this->weak_from_this());

		releaseBin |= std::move(m_Suppliers);
		assert(m_Suppliers.empty());
		assert(getStatus() != task_status::waiting_for_suppliers);
	}
	if (m_TaskFunc) 
	{
		// when done, safe_run would already have destroyed m_TaskFunc in order to remove shared ownership of suppliers before notifying waiters
		assert(status == task_status::exception || status == task_status::cancelled);
		releaseBin |= std::move(m_TaskFunc); // may contain argInterests that may release a lot upon destruction.
	}
	assert(!m_TaskFunc);
	auto resultItem = GetResult();

	if (m_ResKeeper)
		releaseBin |= std::move(m_ResKeeper); // may release interest of FuncDC, probably not a big thing, but it may release ownership of this, which therefore should have been called by a shared_ptr copy.
	assert(!m_ResKeeper);

	assert(!m_WriteLock || status == task_status::cancelled || status == task_status::exception); // all other routes go through Run_with_cleanup, which alwayws release the writeLock on completion
	m_WriteLock = ItemWriteLock();

	if (m_FuncDC)
		releaseBin |= m_FuncDC->resetOperContextImplAndStopSupplInterest();

	assert(!m_FuncDC);

	releaseBin |= disconnect_waiters();
	releaseBin |= make_movable_scoped_exit( &StartOperationContexts ); // do this after releasing interests and related resources of suppliers

	return releaseBin;
}

// Attempt cancellation if no interest remains (or forced). Returns true if cancelled.
bool OperationContext::CancelIfNoInterestOrForced(bool forced)
{
	if (!forced)
	{
		if (!m_Result || m_Result->GetInterestCount())
			return false;
	}

	garbage_can separatedResources;

	leveled_std_section::scoped_lock lock(cs_ThreadMessing);
	separatedResources = separateResources(task_status::cancelled); // destroy after lock
	return true;
}

// Handle a failure coming from item; transition this context to 'exception' if needed.
bool OperationContext::HandleFail(const TreeItem* item)
{
	// note that all statusses of cancelled and higher: exception or done, are final, i.e. never change again once it is set.
	auto transientStatus = getStatus();
	if (transientStatus == task_status::exception)
		return true;
	if (transientStatus >= task_status::cancelled)
		return false;

	if (!item->WasFailed(FailType::Data)) // this precheck filters out most calls without needing a lock on cs_ThreadMessing
		return false;

	RequestMainThreadOperProcessingBlocker letTheNotificationsComeAfter;

	garbage_can separatedResources;

	leveled_std_section::scoped_lock lock(cs_ThreadMessing);

	auto recheckedStatus = getStatus();
	if (recheckedStatus == task_status::exception)
		return true;
	if (recheckedStatus >= task_status::cancelled) // ignore this Fail if Context is already canceled or done.
		return false; 

	m_Result->Fail(item);
	assert(m_Result->WasFailed(FailType::Data));
	separatedResources = separateResources(task_status::exception);
	assert(!m_ResKeeper);

	return true;
}

// Try to set a read lock for a supplier 'si'. On failure or cancellation,
// transitions state appropriately and/or propagates failure.
bool OperationContext::SetReadLock(std::vector<ItemReadLock>& locks, const TreeItem* si) 
{
	assert(m_PhaseNumber >= si->GetCurrPhaseNumber());

	assert(si);
	assert(si->HasInterest());

	assert(IsCalculatingOrReady(si));


	try {
		locks.emplace_back(si);
	}
	catch (const task_canceled&)
	{
		CancelIfNoInterestOrForced(true);
		throw;
	}
	catch (...)
	{
		auto hasError = HandleFail(si);
		assert(hasError);
		return false;
	}
	return true;
}

// Acquire read locks for all suppliers declared in allInterests.
// Returns empty on failure or cancellation.
std::vector<ItemReadLock> OperationContext::SetReadLocks(const FutureSuppliers& allInterests)
{
	std::vector<ItemReadLock> locks; locks.reserve(allInterests.size());

	for (const DataController* futureSupplier : allInterests)
	{
		assert(futureSupplier);
		if (!futureSupplier)
			continue;

		auto supplierItem = futureSupplier->GetOld(); // can be reference to default unit
		assert(supplierItem);
		supplierItem = supplierItem->GetCurrRangeItem();
		if (HandleFail(supplierItem))
			return{};

		if (!SetReadLock(locks, supplierItem))
			return {};

	}
	return locks;
}

// Connect to all argument suppliers by establishing waiter/supplier links.
bool OperationContext::connectArgs(const FutureSuppliers& allArgInterests)
{
	assert(cs_ThreadMessing.isLocked());

	DBG_START("OperationContext", "connectArgs", MG_DEBUG_FUNCCONTEXT);
	DBG_TRACE(("FuncDC: %s", m_FuncDC ? m_FuncDC->md_sKeyExpr : SharedStr()));

	bool connected = false;
	for (const auto& supplierInterest: allArgInterests)
	{
		if (!supplierInterest)
			continue;
		auto funcSupplier = dynamic_cast<const FuncDC*>(supplierInterest.get_ptr());
		std::shared_ptr<OperationContext> supplOperationContext;
		if (funcSupplier)
			supplOperationContext = funcSupplier->GetOperContext();
		else
			supplOperationContext = GetOperationContext(supplierInterest->GetOld()->GetCurrRangeItem());

		if (!supplOperationContext)
			continue;

		auto status = supplOperationContext->getStatus();
		if (status < task_status::cancelled)
		{
			assert(supplOperationContext->m_PhaseNumber <= m_PhaseNumber);
			connect(this, supplOperationContext);
			connected = true;
			dbg_assert(m_Suppliers.size());
		}
	}

#if defined(MG_DEBUG)
	if(sd_ManagedContexts.find(this) == sd_ManagedContexts.end())
		sd_ManagedContexts.insert(this);
#endif

	return connected;
}

// *****************************************************************************
// Section: ScheduleCalcResult
// *****************************************************************************
//
// Schedule operator result calculation:
//  - Gather suppliers from args and m_FuncDC->m_OtherSuppliers.
//  - Manage suppl interest and read locks.
//  - Prepare a payload functor (OC_CalcResultFunc) that runs the operator
//    with strong exception and failure handling.
//  - Either inline execute or enqueue depending on parallel capability.
//
// *****************************************************************************

// Payload functor that runs on OperationContext::Run_* path.
struct OC_CalcResultFunc {
	mutable ArgRefs argRefs;
	mutable FutureSuppliers allInterests;

	void operator () (OperationContext* self, explain_context_ptr_t context) const
	{
		assert(self);
		auto funcDC = self->GetFuncDC();

#if defined(MG_DEBUG_OPERATIONS)
		if (self->m_Result && self->m_Result->m_BackRef && !self->m_Result->m_BackRef->IsCacheItem() && IsDataItem(self->m_Result.get_ptr()) && AsDataItem(self->m_Result.get_ptr())->GetAbstrDomainUnit()->GetNrTiles() > 1)
			reportF(ST_MinorTrace, "Starting calculation of %s with KeyExpr %s"
				, self->m_Result->m_BackRef->GetFullName().c_str()
				, funcDC ? AsFLispSharedStr(funcDC->GetLispRef(), FormattingFlags::ThousandSeparator).c_str() : "(null)"
			);
#endif //defined(MG_DEBUG_OPERATIONS)

		if (!funcDC)
			return;

		assert(funcDC->DoesHaveSupplInterest() || !funcDC->GetInterestCount() || context);

		assert(self->m_Status == task_status::running);
		assert(!SuspendTrigger::DidSuspend());

		std::vector<SharedPtr<const SharedActor>> statusActors; statusActors.reserve(argRefs.size());
		for (const auto& argRef : argRefs)
			statusActors.emplace_back(GetStatusActor(argRef));

		// Validate status/failure on supplier actors before and after running.
		auto failureProcessor = [&]() -> bool
			{
				// forward FR_Validate and FR_Committed failures
				for (const auto& statusActor : statusActors)
				{
					if (statusActor && statusActor->WasFailed())
						funcDC->Fail(statusActor);

					if (funcDC->WasFailed(FailType::Data))
						return true;
				}
				return false;
			};

		if (!failureProcessor())
		{

			// Acquire read locks for suppliers and stop suppl interest on the FuncDC.
			auto readLocks = self->SetReadLocks(allInterests);
			allInterests.clear();

			funcDC->StopSupplInterest();
			self->RunOperator(std::move(argRefs), std::move(readLocks), context); // RunImpl() may destroy this and make m_FuncDC inaccessible // CONTEXT
			argRefs.clear();

			failureProcessor();
		}
		assert(!self->m_FuncDC || IsDataCurrCompleted(self->m_Result->GetCurrUltimateItem()) || self->GetResult()->WasFailed(FailType::Data) || s_OcTaskGroupIsCanceling);
	}
};

// High-level scheduling for CalcResult with arguments and optional context.
bool OperationContext::ScheduleCalcResult(ArgRefs&& argRefs, explain_context_ptr_t context)
{
	auto funcDC = GetFuncDC();
	assert(funcDC);
	assert(GetOperator());
	assert(!SuspendTrigger::DidSuspend());

	DBG_START("OperationContext", "ScheduleCalcResult", MG_DEBUG_FUNCCONTEXT);
	DBG_TRACE(("FuncDC: %s", funcDC->md_sKeyExpr));


	OperatorContextHandle operContext(true, funcDC.get());

	TreeItemDualRef& resultHolder = *const_cast<FuncDC*>(funcDC.get_nonnull());
	assert(resultHolder);

	MG_DEBUGCODE(const TreeItem * oldItem = resultHolder.GetOld());
	MG_DEBUGCODE(auto oper = GetOperator());
	dbg_assert(oldItem);

	assert(resultHolder.IsTmp() || resultHolder->GetInterestCount());
	assert(resultHolder.GetInterestCount());
	dbg_assert(!resultHolder.IsTmp()); // DEBUG 19/03/2020

	bool doASync = GetOperator()->CanRunParallel() && resultHolder.DoesHaveSupplInterest() && !context;

	assert(m_Result);

	if (m_Result->WasFailed(FailType::Data))
	{
		OnException();
		return false;
	}

	assert(IsMetaThread());
	if (!resultHolder.DoesHaveSupplInterest())
	{
		assert(!doASync);
		Actor::UpdateLock lock(&resultHolder, actor_flag_set::AF_ChangingInterest); // DEBUG, 19-03-2020
		resultHolder.StartSupplInterest();
	}

	FutureSuppliers allArgInterests;

	allArgInterests.reserve(argRefs.size() + (funcDC ? funcDC->m_OtherSuppliers.size() : 0)); // TODO G8: count better
	for (const auto& argRef : argRefs)
		if (argRef.index() == 0)
		{
			allArgInterests.emplace_back(std::get<FutureData>(argRef));
			assert(allArgInterests.back());
		}
	if (funcDC)
		for (const DataController* dcPtr : funcDC->m_OtherSuppliers)
		{
			auto otherSupplier = dcPtr->CallCalcResult();
			if (!otherSupplier)
			{
				if (dcPtr->WasFailed(FailType::Data))
				{
					resultHolder.Fail(dcPtr);
					break;
				}
				assert(SuspendTrigger::DidSuspend());
				break;
			}
			allArgInterests.emplace_back(otherSupplier);
		}

	task_status resultStatus = m_Status;
	if (!resultHolder.WasFailed(FailType::Data) && !SuspendTrigger::DidSuspend())
	{
		assert(funcDC);
		auto func = OC_CalcResultFunc{ std::move(argRefs), allArgInterests }; // TOD: move in, but keep view of ArgInterests array

		m_TaskFunc = std::move(func);
		resultStatus = Schedule(funcDC && funcDC->IsNew() ? funcDC->GetNew() : nullptr
			, allArgInterests
			, !doASync
			, context
		);

		assert(!argRefs.size()); // moved in ?

		assert(!SuspendTrigger::DidSuspend() || !doASync || !IsMultiThreaded2());

		dbg_assert(!resultHolder || resultHolder->GetDynamicObjClass()->IsDerivedFrom(oper->GetResultClass()));
		MG_DEBUGCODE(assert(resultHolder.GetOld() == oldItem));

		if (resultHolder && resultHolder->WasFailed())
			resultHolder.Fail(resultHolder.GetOld());
	}
	if (resultHolder.WasFailed(FailType::Data))
	{
		m_Result->Fail(resultHolder);
		assert(m_Status >= task_status::running);
		OnException();
		resultStatus = task_status::exception;
	}
	else if (SuspendTrigger::DidSuspend())
		return false;

	assert(resultStatus != task_status::suspended);

	return resultStatus != task_status::exception;
}

// Enforce supplier completion or return early on suspension; used by inline path.
task_status OperationContext::JoinSupplOrSuspendTrigger()
{
	assert(!SuspendTrigger::DidSuspend());

	SupplierSet suppliers;
	{
		leveled_std_section::unique_lock lock(cs_ThreadMessing);
		suppliers = m_Suppliers;
		for (auto& oc : suppliers)
		{
			OperationContext_scheduleThis(oc.get());
			assert(oc->getStatus() >= task_status::waiting_for_suppliers);
		}
	}
	if (!suppliers.empty())
		StartOperationContexts();

	for (auto& oc: suppliers)
	{
		assert(oc);
		assert(oc->GetStatus() >= task_status::waiting_for_suppliers);

		const TreeItem* supplResult = oc->GetResult();
		assert(supplResult); 
		if (SuspendTrigger::DidSuspend())
			return task_status::suspended;

		task_status ocStatus = oc->Join();
		assert(ocStatus > task_status::running);
		assert(CheckDataReady(supplResult->GetCurrUltimateItem()) || m_Status == task_status::exception || supplResult->WasFailed(FailType::Data) || !supplResult->GetInterestCount() || SuspendTrigger::DidSuspend());
		switch (ocStatus)
		{
		case task_status::done:
			continue;
		case task_status::suspended:
			return task_status::suspended;
		case task_status::exception:
			HandleFail(oc->GetResult());
			break;
		case task_status::cancelled:
			assert(m_Status == task_status::cancelled); // MUST HAVE BEEN ALL RESET AS ATOMIC OPERATION
			break;
		}
		return m_Status;
	}
	return task_status::done;
}

// Query MustCalcArg passthrough.
bool OperationContext::MustCalcArg(arg_index i, CharPtr firstArgValue) const
{
	auto funcDC = GetFuncDC();
	assert(funcDC);
	return funcDC->MustCalcArg(i, true, firstArgValue);
}

// Take and clear m_TaskFunc under lock to avoid races.
auto OperationContext_TakeTaskFunc(OperationContext* self)
{
	leveled_std_section::unique_lock lock(cs_ThreadMessing);
	auto taskFunc = std::move(self->m_TaskFunc);
	assert(!self->m_TaskFunc);
	return taskFunc;
}

// Execute payload with cancellation and exception swallowing, then release write lock.
// This part must not throw; Run_with_cleanup will handle final state.
void OperationContext::Run_with_catch(explain_context_ptr_t context) noexcept
{
	try {
		CancelableFrame frame(this);

		assert(m_Status == task_status::running);

		UpdateMarker::ChangeSourceLock tsLock(m_ActiveTimestamp, "safe_run");

		assert(m_TaskFunc);
		auto taskFunc = OperationContext_TakeTaskFunc(this);
		if (taskFunc)
			taskFunc(this, context); // run the payload functor, set by ScheduleItemWriter
		auto resultItem = GetResult();
		assert(resultItem);
		assert(!m_FuncDC || IsDataCurrCompleted(resultItem->GetCurrUltimateItem()) || resultItem->WasFailed(FailType::Data) || s_OcTaskGroupIsCanceling);
		if (!resultItem->Was(ProgressState::Validated))
			resultItem->SetProgress(ProgressState::Validated);
	}
	catch (const task_canceled&)
	{
		assert(getStatus() == task_status::cancelled); // clean-up was done ?
	}
	catch (...) {
		GetResult()->CatchFail(FailType::Data);
	}

	ItemWriteLock localWriteLock;
	leveled_std_section::unique_lock lock(cs_ThreadMessing);

	assert(!m_FuncDC || IsDataCurrCompleted(m_Result->GetCurrUltimateItem()) || s_OcTaskGroupIsCanceling || GetResult()->WasFailed(FailType::Data) || (getStatus() == task_status::cancelled));

	localWriteLock = std::move(m_WriteLock);
	assert(!m_WriteLock);
	assert(!localWriteLock || localWriteLock.GetItem() == GetResult());
	// writeLock release here before OnEnd allows Waiters to start
}

// Wrapper that runs payload and transitions to final states accordingly.
void OperationContext::Run_with_cleanup(explain_context_ptr_t context) noexcept
{
	assert(!SuspendTrigger::DidSuspend());

	Run_with_catch(context);
	assert(!m_TaskFunc);

	if (GetResult()->WasFailed(FailType::Data))
		OnException(); // clean-up
	else
	{
		assert(!SuspendTrigger::DidSuspend());

		OnEnd(task_status::done); // just set status to done and clean-up
	}

	// check that clean-up was done. This includes releasing the RunCount
	assert(!m_WriteLock);
}

// *****************************************************************************
// Section:     PopActiveSupplierss
// *****************************************************************************
//
// Utilities to prioritize and collect suppliers for inline execution, mainly
// intended for GUI-thread joins to reduce latency by running prerequisites.
//
// *****************************************************************************

struct prioritize_results
{
	WaiterSet waitingAndScheduledContexts;
	SupplierSet activatedContexts;
	context_array collectedActivations;
};

// DFS-like traversal to collect activated suppliers and collectable tasks
// while avoiding revisiting nodes.
void prioritize_impl(prioritize_results& results, OperationContextSPtr self)
{
	assert(!cs_ThreadMessing.try_lock());
	assert(self);
	assert(self->m_PhaseNumber == s_CurrActivePhaseNumber); // else we wouldn't be here.

	auto status = self->getStatus();
	assert(status != task_status::none);

	if (status >= task_status::running)
		return; // no more running of this task than already is or was going on.

	if (status == task_status::activated)
	{
		results.activatedContexts.insert(self);
		return;
	}

	if (status == task_status::scheduled)
		if (self->collectTaskImpl())
		{
			auto& vec = s_ScheduledContextsMap[self->m_PhaseNumber];
			auto it = std::find_if(vec.begin(), vec.end(), [&](const OperationContextWPtr& wptr) { return !wptr.expired() && wptr.lock() == self; });
			if (it != vec.end())
				vec.erase(it);
			status = self->getStatus();
			results.collectedActivations.emplace_back(self);
			return;
		}

	assert(status == task_status::waiting_for_suppliers || status == task_status::scheduled);

	auto [_, wasInserted] = results.waitingAndScheduledContexts.insert(self);
	if (!wasInserted)
		return;   // self was already visited and in this set.

	for (auto& s : self->m_Suppliers)
		prioritize_impl(results, s);
}

// Convenience wrapper to compute sets for prioritization.
auto prioritize(OperationContextSPtr waiter) -> std::pair<SupplierSet, context_array>
{
	// Build the two sets once per Join:
	prioritize_results results;
	prioritize_impl(results, waiter);
	return { std::move(results.activatedContexts), std::move(results.collectedActivations) };
}

// Pop currently activated suppliers or collect them if possible.
auto PopActiveSuppliers(OperationContextSPtr waiter) -> std::pair<SupplierSet, context_array>
{
	leveled_std_section::scoped_lock lock(cs_ThreadMessing);
	auto status = waiter->getStatus();
	if (status == task_status::running)
	{
		SupplierSet result; result.insert(std::move(waiter));
		return { result, {} };
	}

	return prioritize(waiter);
}

// *****************************************************************************
// Section:     StealOneTask
// *****************************************************************************
//
// Work stealing for OperationContexts and tile tasks:
//  - First try to steal a tile task.
//  - Then try to pick an activated OperationContext from s_RadioActives and
//    acquire the unique run license.
//  - Run inline if successful.
//
// *****************************************************************************

// Find and license a single activated OperationContext to run inline.
auto FindAndLicenceOnePriorityTasks() -> OperationContextSPtr
{
	std::vector<OperationContextSPtr> garbage;

	leveled_std_section::unique_lock lock(cs_ThreadMessing);

	while (!s_RadioActives.empty()) {
		auto oc = s_RadioActives.front(); 

		auto ocSPtr = oc.lock();
		if (!ocSPtr) // expired?
		{
			s_RadioActives.pop_front();
			continue;
		}

		if (s_CurrBlockedPhaseNumber && ocSPtr->m_PhaseNumber >= s_CurrBlockedPhaseNumber) // phase fence?
			return {};

		// take from the list
		s_RadioActives.pop_front();

		// try to grab the license—only one thread ever wins per OC
		if (ocSPtr->getUniqueLicenseToRun(false))
			return ocSPtr; // got it
		// otherwise, someone else is already running it; try the next one
		garbage.emplace_back(std::move(ocSPtr)); // maybe we-re the last owner and we dón't wat to destry here 
		assert(!ocSPtr);
	}
	return {};
}


// Try to steal and execute any single unit of work; returns true if did work.
bool StealOneTask()
{
	StealTasks();

	auto ocSPtr = FindAndLicenceOnePriorityTasks();
	if (!ocSPtr)
		return false; // no work to do

	ocSPtr->Run_with_cleanup({});
	return true;
}

// Returns whether the currently active context (if any) still has run-count.
bool CurrActiveTaskHasRunCount()
{
	auto ca = CancelableFrame::CurrActive();
	if (!ca)
		return false;
	auto status = ca->getStatus();
	return IsActiveOrRunning(status);
}

/// <summary>
///		Wait for the task to finish.
/// </summary>
/// <param name="self">The task to wait for</param>
/// <returns>task_status of OperationContext, which is always above task_status::running, or task_status::suspended</returns>
/// 
/// \exception task_canceled
/// 
/// Join waits for the task to finish. 
/// If it is activated, it will run the task inline.
/// If it is not yet activated, it will run other tasks until it is activated.
/// If the current thread is the GUI thread, it will pop up active suppliers that are not yet running and try to run them inline.
/// If it is already running, it will wait for the task to finish, while stealing other tasks if this is not the GUI thread.
//
// Core wait function with cooperative progress (stealing and pumping).
task_status OperationContext::Join()
{
	if (!IsMetaThread())
		StealTasks();

	if (CurrActiveTaskHasRunCount())
	{
		reportF(MsgCategory::other, SeverityTypeID::ST_MinorTrace, "OperationContext(%s)::Join called from Active Context %s"
			, GetResult()->GetFullName()
			, CancelableFrame::CurrActive()->GetResult()->GetFullName()
		);
	}
	if (IsMetaThread() && s_CurrBlockedPhaseNumber && s_CurrBlockedPhaseNumber <= m_PhaseNumber)
		throwErrorF("PhaseContainer", "Invalid Recursion, OperationContext(%s)::Join called from updating %s for %s"
		,	GetResult()->GetFullName()
		,	s_CurrBlockedPhaseItem->GetFullName()
		,	s_CurrPhaseContainer->GetFullName()
		);
	
	OperationContext_ScheduleThis(this, false, nullptr);

	//MG_CHECK(GetStatus() != task_status::none); // being scheduled is a precondition

	std::weak_ptr<OperationContext> firstSupplier;

	while (GetStatus() <= task_status::running)
	{
		if (auto fs = firstSupplier.lock())
		{
			fs->Join();
			firstSupplier = {};
		}

		if (IsMetaThread() && !SuspendTrigger::BlockerBase::IsBlocked())
		{
			if (SuspendTrigger::DidSuspend())
				return task_status::suspended;
		}
		else
			if (GetStatus() == task_status::activated)
				if (TryRunningTaskInline())
					return GetStatus();

		if (IsMetaThread())
		{
			SuspendTrigger::MarkProgress();
			ProcessMainThreadOpers();
			ProcessSuspendibleTasks();
			if (SuspendTrigger::DidSuspend())
				return task_status::suspended;
		}
/* REMOVE
		if (IsMetaThread() && m_PhaseNumber <= s_CurrActivePhaseNumber)
			while (true)
			{
				auto [activatedContexts, collectedTasksToRun] = PopActiveSuppliers(shared_from_this());
				if (activatedContexts.empty() && collectedTasksToRun.empty())
					break;
				StartCollectedOperationContexts(std::move(collectedTasksToRun));
				// run as much as possible and needed for this Join before giving up on task collection effort
				for (const auto& inlineTaskCandidate : activatedContexts)
					inlineTaskCandidate->TryRunningTaskInline();  
				for (const auto& inlineTaskCandidateWPtr : collectedTasksToRun)
					if (auto inlineTaskCandidate = inlineTaskCandidateWPtr.lock())
						inlineTaskCandidate->TryRunningTaskInline();  // already running elsewhere. go for something else before giving up on task collection effort
			}
		else			
*/
		StaticMtIncrementalLock<s_NrWaitingJoins> increaseTheNumberOfWaitingJoinsToAvoidWaitingForNothing;

		auto currentFinishCount = GetCurrFinishedCount();
		StartOperationContexts(); // OPTIMIZE, CONSISTENCY: Some tasks finished without calling this. Find out why and avoid wasting idle thread time; maybe all threads are waiting in a Join without new and current tasks being activated
		if (!IsMetaThread())
		{
			while (StealOneTask())
			{
				StartOperationContexts(); // OPTIMIZE, CONSISTENCY: Some tasks finished without calling this. Find out why and avoid wasting idle thread time; maybe all threads are waiting in a Join without new and current tasks being activated
				if (GetStatus() > task_status::running)
					goto exit;
			}
		}


		leveled_std_section::unique_lock lock(cs_ThreadMessing);
		if (m_Status > task_status::running)
			break;
		if (m_Status == task_status::activated)
			continue; // try to run it inline in the next iteration
		if (currentFinishCount != s_CurrFinishedCount)
			continue;
		if (m_Status == task_status::scheduled)
		{
			assert(m_Suppliers.empty()); // scheduled since the last call of RunOperations or not activated because of s_IsInLowRamMode
//			assert(IsDefined(getScheduledContextsPos(this->shared_from_this()))); // always true for scheduled tasks outzide cs_ThreadMessing
		}
		if (m_Status == task_status::waiting_for_suppliers)
		{
			UInt32 recursionCount = 0;
			// find first context without suppliers and wait for that one
			firstSupplier = this->weak_from_this();
			while (auto fs = firstSupplier.lock())
			{
				if (fs->m_Suppliers.empty())
					break;

				if (recursionCount++ > sd_OcCount)
					throwErrorF("OperationContext", "Invalid Recursion detected on OperationContext(%s)::Join"
						, GetResult()->GetFullName()
					);

				firstSupplier = *(fs->m_Suppliers.begin());
			}
		}

		if (IsMetaThread())
		{
			if (SuspendTrigger::MustSuspend())
				return task_status::suspended;
			if (HasMainThreadTasks())
				continue; // do that first 
		}

		// or wait for conditioin that was certainly not met just after setting the thread messing lock
		cv_TaskCompleted.wait_for(lock.m_BaseLock, std::chrono::milliseconds(500));
	}
exit:
	auto status = GetStatus();
	assert(status > task_status::running);
	dbg_assert((m_Result->m_ItemCount < 0) || CheckDataReady(m_Result->GetCurrUltimateItem()) || status == task_status::cancelled || status == task_status::exception || !m_Result->GetInterestCount() || !m_FuncDC);
	return status;
}

// *****************************************************************************
// Section:     DoWorkWhileWaiting
// *****************************************************************************
//
// Cooperative waiting helper to make progress while waiting for a fence status.
// Pumps main-thread tasks, steals background work, and honors suspend triggers.
//
// *****************************************************************************

TIC_CALL void DoWorkWhileWaiting()
{
	if (!IsMetaThread())
		StealTasks();

	if (IsMetaThread())
	{
		//			if (SuspendTrigger::MustSuspend())
		//				return false;
		ProcessMainThreadOpers();
		ProcessSuspendibleTasks();
		SuspendTrigger::MarkProgress();
	}

	auto currentFinishCount = GetCurrFinishedCount();

	StartOperationContexts();
	if (!IsMetaThread())
	{
		if (StealOneTask())
			return; // let caller reconsider after one tasks has been done as no explicit termination condition variable is provided
	}

	leveled_std_section::unique_lock lock(cs_ThreadMessing);

	if (currentFinishCount != s_CurrFinishedCount)
		return;

	if (IsMetaThread())
	{
		if (HasMainThreadTasks())
			return;

		if (SuspendTrigger::MustSuspend())
			return;
	}

	// wait for conditioin that was certainly not met just after setting the thread messing lock
	cv_TaskCompleted.wait_for(lock.m_BaseLock, std::chrono::milliseconds(500));
}

// *****************************************************************************
// Section:     DoWorkWhileWaitingFor
// *****************************************************************************
//
// Cooperative waiting helper to make progress while waiting for a fence status.
// Pumps main-thread tasks, steals background work, and honors suspend triggers.
//
// *****************************************************************************

TIC_CALL void DoWorkWhileWaitingFor(std::atomic<task_status>* fenceStatus)
{
	assert(fenceStatus);

	if (!IsMetaThread())
		StealTasks();

	while (IsActiveOrRunning(*fenceStatus))
	{
		if (IsMetaThread())
		{
//			if (SuspendTrigger::MustSuspend())
//				return false;
			ProcessMainThreadOpers();
			ProcessSuspendibleTasks();
			SuspendTrigger::MarkProgress();
		}

		auto currentFinishCount = GetCurrFinishedCount();

		StartOperationContexts();
		if (!IsMetaThread())
			while (IsActiveOrRunning(*fenceStatus))
			{
				if (!StealOneTask())
					break;
			}

		leveled_std_section::unique_lock lock(cs_ThreadMessing);
		if (*fenceStatus > task_status::running)
			break;

		if (currentFinishCount != s_CurrFinishedCount)
			continue;

		if (IsMetaThread() && HasMainThreadTasks())
			continue;

		if (SuspendTrigger::MustSuspend())
			return;

		// wait for conditioin that was certainly not met just after setting the thread messing lock
		cv_TaskCompleted.wait_for(lock.m_BaseLock, std::chrono::milliseconds(500));
	}
}

// *****************************************************************************
// Section:     RunOperator
// *****************************************************************************
//
// Execute the operator payload with argRefs and readLocks, converting exceptions
// into failures and managing interest/cancellation semantics.
//
// *****************************************************************************

void OperationContext::RunOperator(ArgRefs argRefs, std::vector<ItemReadLock> readLocks, explain_context_ptr_t context)
{
	SharedPtr<const FuncDC> funcDC = GetFuncDC();
	if (!funcDC || funcDC->WasFailed(FailType::Data))
		return;

	if (m_Result->WasFailed(FailType::Data))
	{
		funcDC->Fail(m_Result.get_ptr());
		return;
	}
	TreeItemDualRef& resultHolder = *const_cast<FuncDC*>(funcDC.get_nonnull());
	assert(resultHolder);
	bool actualResult = false;
	if (!CancelIfNoInterestOrForced(false))
	{
		bool newResult = false;
		if (funcDC->WasFailed(FailType::MetaInfo))
			return;
		try {

			ASyncContinueCheck();

			TreeItemDualRefContextHandle reportProgressAndErr(&resultHolder);

			auto op = funcDC->m_Operator;
			MG_CHECK(op);
			actualResult = op->CalcResult(resultHolder, std::move(argRefs), std::move(readLocks), context.get()); // ============== payload

			assert(resultHolder || IsCanceled());
			assert(actualResult || SuspendTrigger::DidSuspend());
			assert(!IsDataItem(resultHolder.GetUlt()) || AsDataItem(resultHolder.GetUlt())->m_DataObject
				|| !actualResult
				|| CheckCalculatingOrReady(resultHolder.GetUlt())
				|| resultHolder->WasFailed(FailType::Data)
			);
		}
		catch (const task_canceled&)
		{
			OnEnd(task_status::cancelled);
			assert(m_Status == task_status::cancelled || m_Status == task_status::exception);
		}
		catch (...)
		{
			resultHolder.CatchFail(FailType::Data); // Now done by TreeItemDualRef::DoFail
			auto errPtr = resultHolder.GetFailReason();
			errPtr->TellExtraF("in function %s", GetOperGroup()->GetName());
			if (resultHolder.HasBackRef())
				errPtr->TellExtraF("while calculating %s", resultHolder.GetBackRefStr());
			HandleFail(resultHolder);
		}
		assert(!resultHolder.IsNew() || resultHolder->m_LastChangeTS == resultHolder.m_LastChangeTS); // further changes in the resulting data must have caused resultHolder to invalidate, as IsNew results are passive
		if (actualResult)
		{
#if defined(MG_DEBUG)
			const TreeItem* ri = resultHolder.IsOld() ? resultHolder->GetCurrUltimateItem() : resultHolder.GetNew();
			assert(ri);
			assert(ri->GetIsInstantiated() || CheckCalculatingOrReady(ri) || resultHolder->WasFailed(FailType::Data));
//			assert(CheckDataReady(ri) || resultHolder->WasFailed(FailType::Data));

			assert(!resultHolder.IsNew() || resultHolder->m_LastChangeTS == resultHolder.m_LastChangeTS); // further changes in the resulting data must have caused resultHolder to invalidate, as IsNew results are passive
#endif
		}
	}
}
auto OperationContext::GetFuncDC() const -> SharedPtr<const FuncDC>
{ 
	leveled_critical_section::scoped_lock octmLock(cs_ThreadMessing); 
	return m_FuncDC.get(); 
}
