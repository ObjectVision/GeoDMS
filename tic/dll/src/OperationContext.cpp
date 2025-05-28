// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

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

#include "ParallelTiles.h"

static std::vector<tile_task_group*> s_TileTaskGroups;
std::mutex s_TileTaskGroupsMutex;

static int s_NrRunningTileTaskThreads = 0;

auto takeOneTileTask() -> std::pair<tile_task_group*, tile_task_group::IndexType>
{
	assert(!s_TileTaskGroupsMutex.try_lock());
	while (!s_TileTaskGroups.empty())
	{
		auto* taskGroup = s_TileTaskGroups.back();
		if (taskGroup)
		{
			auto i = taskGroup->getNextCommissioned();
			if (IsDefined(i))
				return { taskGroup, i };
		}
		s_TileTaskGroups.pop_back();
	}
	return { nullptr, UNDEFINED_VALUE(tile_task_group::IndexType) };
}

auto TakeOneTileTask() -> std::pair<tile_task_group*, tile_task_group::IndexType>
{
	auto lock = std::unique_lock<std::mutex>(s_TileTaskGroupsMutex);
	return takeOneTileTask();
}

auto TakeOneTaskOrDecommissionThread() -> std::pair<tile_task_group*, tile_task_group::IndexType>
{
	auto lock = std::unique_lock<std::mutex>(s_TileTaskGroupsMutex);
	auto tileTask = takeOneTileTask();
	if (!tileTask.first)
	{
		assert(s_NrRunningTileTaskThreads > 0);
		--s_NrRunningTileTaskThreads; // no task available, caller must decommission this thread
	}
	return tileTask;
}

bool StealOneTileTask(bool doMore)
{
	auto tileTask = TakeOneTileTask();
	if (!tileTask.first)
		return false;
	assert(IsDefined(tileTask.second)); // we assume starting with a valid ticket to a slot, or else this tile_task_group may already be destroyed.
	tileTask.first->DoWork(tileTask.second, doMore);
	return true;
}

void DoThisOrThatAndDecommission()
{
	while(true)
	{
		auto tileTask = TakeOneTaskOrDecommissionThread();
		if (!tileTask.first)
			return;
		assert(IsDefined(tileTask.second)); // we assume starting with a valid ticket to a slot, or else this tile_task_group may already be destroyed.
		tileTask.first->DoWork(tileTask.second, true);
	}
}

tile_task_group::tile_task_group(IndexType last, task_func func)
	: m_Last(last)
	, m_Func(func)
	, m_CallingContext(CancelableFrame::CurrActive())
{
	UInt32 nrThreadsToCommission = 0;
	if (m_Last > 0)
	{
		auto lock = std::unique_lock<std::mutex>(s_TileTaskGroupsMutex);
		s_TileTaskGroups.emplace_back(this);

		// now fire some workers, but not too many. Trust that existing workers will also pick up due to their DoThisOrThat before completion that is synchronized with this 
		// unless they have just completed their DoThisOrThat and are waiting for access to Decommissionn their thread.

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

	while (nrThreadsToCommission-- > 0)
		GetTaskGroup().run([] { DoThisOrThatAndDecommission(); });
}

tile_task_group::~tile_task_group()
{
	auto nrCommissioned = m_Commissioned;
	auto wasDecommissioned = (nrCommissioned == m_Last);
	assert(wasDecommissioned);
	if (!wasDecommissioned)
	{
		assert(nrCommissioned < m_Last); // consistency check
		auto lock = std::unique_lock<std::mutex>(s_TileTaskGroupsMutex);
		if (m_Commissioned < m_Last) // recheck if decommission is still required
		{
			auto remainingTasks = (m_Last - m_Commissioned); // current slot still has to be registered as completed, after which this task_group may be destructed.
			m_Commissioned += remainingTasks;
			assert(m_Commissioned == m_Last);
			decommission(); // stop handing out slots for this task group, so that no new tasks will be started, but the current ones can still finish.
			registerCompletions(remainingTasks); // other tasks in flight might still come in before m_TileTasksDone can be notified.
		}
	}
	if (m_NrCompleted < m_Last)      // any other task-slots still running
		AwaitRunningSlots();         // then wait for them. Silence possible excecptions.
	assert(m_NrCompleted == m_Last); // we now expect to have completed all commissioned task-slots.
}

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

auto tile_task_group::getNextCommissioned() -> IndexType
{
	assert(!s_TileTaskGroupsMutex.try_lock()); // don't let the notification fall outside a waiter lock

	if (m_Commissioned >= m_Last)
		return UNDEFINED_VALUE(IndexType);

	auto result = m_Commissioned++; // ownership of this slot of the task_group is now obtained and will continue until this thread causes m_NrCompleted to increment accordingly
	assert(result < m_Last);
	if (m_Commissioned >= m_Last)
		decommission(); // stop handing out slots for this task group, so that no new tasks will be started, but the current ones, including the slot that the caller must complete, can still finish.
	return result;
}

auto tile_task_group::GetNextCommissioned() -> IndexType
{
	auto lock = std::unique_lock<std::mutex>(s_TileTaskGroupsMutex);
	return getNextCommissioned();
}

void tile_task_group::registerCompletions(IndexType nr)
{
	assert(!s_TileTaskGroupsMutex.try_lock()); // don't let the notification fall outside a waiter lock
	assert(nr);
	m_NrCompleted += nr;
	assert(m_NrCompleted <= m_Last);
	if (m_NrCompleted == m_Last)
		m_TileTasksDone.notify_all();
}

void tile_task_group::RegisterCompletion()
{
	assert(m_NrCompleted < m_Last);
	auto lock = std::unique_lock<std::mutex>(s_TileTaskGroupsMutex); // don't let the notification fall outside a waiter lock
	if (++m_NrCompleted == m_Last) // don't increment outside lock as it may cause another thread to Join and destruct
		m_TileTasksDone.notify_all();
}

auto tile_task_group::RegisterCompletionAndGetNextCommissioned()->IndexType
{
	auto lock = std::unique_lock<std::mutex>(s_TileTaskGroupsMutex); // don't let the notification fall outside a waiter lock
	assert(m_NrCompleted < m_Last);
	if (++m_NrCompleted == m_Last)
	{
		m_TileTasksDone.notify_all();
		return UNDEFINED_VALUE(IndexType);
	}
	return getNextCommissioned();
}

void tile_task_group::DoWork(IndexType i, bool doMore)
{
	assert(!std::uncaught_exceptions());
	assert(IsDefined(i)); // we assume starting with a valid ticket to a slot, or else this tile_task_group may already be destroyed.
	try {
		CancelableFrame frame(m_CallingContext);
		while (IsDefined(i))
		{
			if (m_CallingContext)
				DSM::CancelIfOutOfInterest();
			ASyncContinueCheck();

			UpdateMarker::PrepareDataInvalidatorLock preventInvalidations;
			m_Func(i);
			if (!doMore)
			{
				RegisterCompletion(); // release this slot of the task_group, so from here, this may be destroyed
				break;
			}
			i = RegisterCompletionAndGetNextCommissioned(); // release this slot and obtain the next one, or none if this task group is done.
		}
	}
	catch (const task_canceled&)
	{
		assert(m_CallingContext);
		if (m_CallingContext)
			m_CallingContext->CancelIfNoInterestOrForced(true);
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
			assert(m_Commissioned == m_Last);
			decommission(); // stop handing out slots for this task group, so that no new tasks will be started, but the current ones can still finish.
			registerCompletions(1 + remainingTasks); // other tasks in flight might still come in before m_TileTasksDone can be notified.
		}
	}
}

void tile_task_group::AwaitRunningSlots() noexcept
{
	assert(m_Commissioned == m_Last); // post condition of DoWork

	while (m_NrCompleted < m_Last)
	{
		// TODO: this can cause other tiles to process that use the same m_Mutex in FutureTileFunctor::tile_record::GetTile
		// if (StealOneTileTask(false))
		//	 continue;

		auto lock = std::unique_lock<std::mutex>(s_TileTaskGroupsMutex);
		if (m_NrCompleted < m_Last)
			m_TileTasksDone.wait_for(lock, std::chrono::milliseconds(500));
	}
	assert(m_NrCompleted == m_Last); 
}

void tile_task_group::Join()
{
	IndexType nextSlot = GetNextCommissioned();
	if (IsDefined(nextSlot))
		DoWork(nextSlot, true);

	AwaitRunningSlots();
	// no more other worker threads can access this task group, so we can safely access m_ExceptionPtr now.

	if (m_ExceptionPtr) 
		std::rethrow_exception(m_ExceptionPtr);
}

// *****************************************************************************
// Section:     OperatorContextHandle
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

	for (auto argi = m_FuncDC->GetArgList(); argi; argi = argi->m_Next)
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

leveled_std_section cs_ThreadMessing(item_level_type(0), ord_level_type::ThreadMessing, "LockedThreadMessing");
std::condition_variable cv_TaskCompleted;

phase_number s_CurrBlockedPhaseNumber = 0;
const TreeItem* s_CurrBlockedPhaseItem = nullptr;
const TreeItem* s_CurrPhaseContainer = nullptr;

using context_array = std::vector<OperationContextWPtr>;
using contexts_within_one_phase = std::deque<OperationContextWPtr>;
using RunningOperationsCounter = Int32;

// protected by exclusive lock on cs_ThreadMessing
static std::map<phase_number, contexts_within_one_phase> s_ScheduledContextsMap;
static contexts_within_one_phase s_RadioActives; // contexts that were collected for running, some of them may be already running, made for work stealing and prioritizing in case the GUI thread calls Join on a specific context.
static std::map<phase_number, RunningOperationsCounter> s_NrActivatedOrRunningOperations; 
static phase_number s_CurrActivePhaseNumber = 0;
static bool s_IsInLowRamMode = false;
static UInt32 s_CurrFinishedCount = 0;

static std::atomic<UInt32> s_NrWaitingJoins = 0;
phase_number GetCurrActivePhaseNumber()
{
	leveled_critical_section::scoped_lock lockToAvoidHasMainThreadTasksToBeMissed(cs_ThreadMessing);
	return s_CurrActivePhaseNumber;
}

UInt32 GetCurrFinishedCount()
{
	leveled_critical_section::scoped_lock lockToAvoidHasMainThreadTasksToBeMissed(cs_ThreadMessing);
	return s_CurrFinishedCount;
}
// *****************************************************************************
// Section:     extra debug stuff
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

void CheckNumberOfRunningOCConsistency()
{
	MG_CHECK(!cs_ThreadMessing.try_lock());
		
	SizeT numberOfRunningOCs = 0;
	for (const auto& levelNumberPair : s_NrActivatedOrRunningOperations)
		numberOfRunningOCs += levelNumberPair.second;
	MG_CHECK(numberOfRunningOCs == sd_RunningOC.size());

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

void reportOC(CharPtr source, OperationContext* ocPtr)
{
	auto item = ocPtr->GetResult();
	reportF_without_cancellation_check(MsgCategory::other, SeverityTypeID::ST_MajorTrace, "OperationContext %s %d: %s", source, int(ocPtr->GetStatus()), item ? item->GetSourceName().c_str() : "");
}
auto reportRemaingOcOnExit = make_scoped_exit([]()
	{
		for (auto ocPtr : sd_OC)
			reportOC("Leaked", ocPtr);
	});

#endif

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
// note: caller is responsible for having exactly one tg_maintainer object in main() or at least during calls to GetTaskGroup();
// note: GetTaskGroup() returns a singleton, but calls to GetTaskGroup().run(...) can be made from different threads
// 
// *****************************************************************************

static concurrency::task_group* s_OcTaskGroup = nullptr;


TIC_CALL tg_maintainer::tg_maintainer()
{
	// build policy
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
	{
		leveled_std_section::scoped_lock lock(cs_ThreadMessing);

		dbg_assert(sd_runOperationContextsRecursionCount == 0);

		s_ScheduledContextsMap.clear();
		s_RadioActives.clear();

		assert(s_ScheduledContextsMap.empty()); ;
		assert(s_RadioActives.empty());
	}
	s_OcTaskGroup->cancel();
	s_OcTaskGroup->wait();

	assert(sd_RunningOC.empty() || g_IsTerminating);
	assert(sd_OC.empty() || g_IsTerminating);

	delete s_OcTaskGroup;
	s_OcTaskGroup = nullptr;
}

concurrency::task_group& GetTaskGroup()
{
	assert(s_OcTaskGroup);
	return *s_OcTaskGroup;
}

// *****************************************************************************
// Section: PhaseNumbers
// *****************************************************************************

auto GetNextPhaseNumber() -> phase_number
{
	static std::atomic<phase_number> s_SchedulingFenceNumber = 1;
	return ++s_SchedulingFenceNumber;
}

// *****************************************************************************
// Section: helper functions
// *****************************************************************************

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

void scheduleRunnableTask(OperationContext* self)
{
	assert(!cs_ThreadMessing.try_lock());
	assert(self->m_Status < task_status::scheduled);

	// next StartOperationContexts() must not start scheduling tasks with phase numbers higher than this task.
	auto fn = self->m_PhaseNumber;
	MakeMin<phase_number>(s_CurrActivePhaseNumber, fn);
	s_ScheduledContextsMap[fn].emplace_back(self->weak_from_this());
	self->releaseRunCount(task_status::scheduled);
//	self->m_Status = task_status::scheduled;
}

// *****************************************************************************
// Section: disconnect suppliers
// *****************************************************************************

garbage_t OperationContext::disconnect_supplier(OperationContext* supplier)
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

	garbage_t garbage;
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
		assert(IsDataReady(supplier->m_Result->GetCurrUltimateItem()));
		if (!m_Suppliers.empty())
			break;

		dbg_assert(m_Suppliers.empty());

		if (m_Status == task_status::waiting_for_suppliers)
			scheduleRunnableTask(this);
		break;

	case task_status::exception:
	{
		SharedTreeItem supplResult = supplier->GetResult();
		if (supplResult)
			garbage |= make_any_scoped_exit([self = weak_from_this(), supplResult]
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
		garbage |= make_any_scoped_exit([self = weak_from_this()]
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

garbage_t OperationContext::disconnect_waiters()
{
	DBG_START("OperationContextPtr", "disconnect_waiters", MG_DEBUGCONNECTIONS);
	DBG_TRACE(("this = %s", AsText(this)))

	assert(cs_ThreadMessing.isLocked());

#if defined(MG_DEBUG)
	assert( sd_ManagedContexts.find(this) != sd_ManagedContexts.end() || !m_FuncDC);
	sd_ManagedContexts.erase(this);
#endif

	assert(!cs_ThreadMessing.try_lock());

	garbage_t garbage;
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

auto collectOperationContexts() -> std::pair<context_array, garbage_t>
{
	assert(!cs_ThreadMessing.try_lock());

#if defined(MG_DEBUG)
	assert(!sd_runOperationContextsRecursionCount);
	DynamicIncrementalLock s_ActivationGuard(sd_runOperationContextsRecursionCount);
#endif

	context_array results;
	garbage_t cancelGarbage;

	while (!s_ScheduledContextsMap.empty())
	{
		auto nextPhaseNumber = s_ScheduledContextsMap.begin()->first;
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
			NotifyCurrentTargetCount();
		}

		auto& scheduledContexts = s_ScheduledContextsMap.begin()->second;

		auto currContext = scheduledContexts.begin();

		s_IsInLowRamMode = false; // reset for next activation round
		bool isLowOnFreeRamTested = false;

		for (; currContext != scheduledContexts.end(); ++currContext)
		{
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
			assert(operContext->m_PhaseNumber == s_CurrActivePhaseNumber);
			if (operContext->m_Status < task_status::activated)
			{
				assert(operContext->m_TaskFunc);
				if (DSM::IsCancelling())
				{
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
		scheduledContexts.erase(scheduledContexts.begin(), currContext);
		if (!scheduledContexts.empty())
			break;

		s_ScheduledContextsMap.erase(s_ScheduledContextsMap.begin());

	}
exit:
	return { std::move(results), std::move(cancelGarbage) };
}

static std::atomic<bool> s_RunOperationContextsScheduled = false;

auto CollectOperationContextsImpl() -> context_array
{
	context_array activatedOperationContextArray;
	garbage_t receivedGarbage;

	leveled_std_section::scoped_lock lock(cs_ThreadMessing);
	std::tie(activatedOperationContextArray, receivedGarbage) = collectOperationContexts();
	return activatedOperationContextArray;
}

void StartCollectedOperationContexts(context_array collectedActivatedContexts)
{
	for (auto activatedContextWPtr : collectedActivatedContexts)
		if (auto activatedContext = activatedContextWPtr.lock())
		{
			auto selfCaller = [activatedContextWPtr]()
				{
					if (auto self = activatedContextWPtr.lock())
						self->TryRunningTaskInline(GetCurrActivePhaseNumber());
				};

			GetTaskGroup().run(selfCaller);
		}
}

TIC_CALL void StartOperationContexts()
{
	s_RunOperationContextsScheduled = false;

	auto collectedActivatedContexts = CollectOperationContextsImpl();
	StartCollectedOperationContexts(std::move(collectedActivatedContexts));
}

void PostStartOperationContexts()
{
	if (s_RunOperationContextsScheduled.exchange(true))
		return;
	PostMainThreadOper(StartOperationContexts);
	StartOperationContexts();
}

inline bool IsActiveOrRunning(task_status s) { return s >= task_status::activated && s <= task_status::running; }

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

/// <summary>
///		This constructor is used to create a new OperationContext with a given FuncDC.
/// </summary>
/// <param name="self"></param>
/// 
/// Call ScheduleCalcResult() to start the calculation.
/// todo: integreate ScheduleCalcResult into the constructor.

void OperationContext_AddOcCount(OperationContext* self)
{

#if defined(MG_TRACE_OPERATIONCONTEXTS)

	leveled_critical_section::scoped_lock lock(cs_OcAdm);
	++sd_OcCount;
	sd_OC.insert(self);

#endif

}

OperationContext::OperationContext(const FuncDC* self)
	: m_FuncDC(self)
	, m_PhaseNumber(self->GetPhaseNumber())
{
	assert(m_PhaseNumber);

	m_Result = self->GetOld();

	OperationContext_AddOcCount(this);
}

OperationContext::OperationContext(task_func_type func)
	: m_TaskFunc( std::move(func) )
{
	OperationContext_AddOcCount(this);
}

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

bool OperationContext_ConnectArgs(OperationContext* oc, const FutureSuppliers& allArgInterest)
{
	leveled_std_section::scoped_lock lock(cs_ThreadMessing);

	return oc->connectArgs(allArgInterest);
}

void OperationContext_scheduleThis(OperationContext* self)
{
	assert(self);

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
			OperationContext_scheduleThis(s.get());
		self->m_Status = task_status::waiting_for_suppliers;
	}
}

void OperationContext_ScheduleThis(OperationContext* self)
{
	leveled_std_section::scoped_lock lock(cs_ThreadMessing);

	OperationContext_scheduleThis(self);
}

task_status OperationContext::Schedule(TreeItem* item, const FutureSuppliers& allArgInterest, bool runDirect, Explain::Context* context)
{
	assert(IsMetaThread());

	assert(UpdateMarker::IsInActiveState());
	assert(m_Status == task_status::none);

	assert(IsMetaThread());
	//	dms_assert(!m_TaskFunc);
	assert(m_Status == task_status::none);
	assert(m_TaskFunc);

	m_Context = context;
	if (item)
	{
		auto itemFN = item->GetPhaseNumber();
		assert(!m_PhaseNumber || m_PhaseNumber == itemFN);
		m_PhaseNumber = itemFN;
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
	assert(m_Status == task_status::none);

	m_ActiveTimestamp = UpdateMarker::GetActiveTS(MG_DEBUG_TS_SOURCE_CODE("Obtaining active frame for produce item task"));

	if (item)
		m_WriteLock = ItemWriteLock(item, weak_from_this()); // sets item->m_Producer to this; no future access right yet

	bool connectedArgs = (!allArgInterest.empty()) && OperationContext_ConnectArgs(this, allArgInterest);

	if (runDirect)
	{
		auto supplStatus = JoinSupplOrSuspendTrigger();
		assert(m_Suppliers.empty());
		assert(GetStatus() == task_status::none);
		{
			leveled_std_section::scoped_lock lock(cs_ThreadMessing);

			OperationContex_setActivated(this);
			bool running = getUniqueLicenseToRun(m_PhaseNumber); assert(running);
		}
		Run_with_cleanup();
	}
	else
	{
		assert(!m_FuncDC || GetOperator()->CanRunParallel());
		OperationContext_ScheduleThis(this);
	}
	return GetStatus();
}

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
task_status OperationContext::GetStatus() const
{ 
	leveled_std_section::scoped_lock lock(cs_ThreadMessing);
	return m_Status;
}

bool OperationContext::collectTaskImpl()
{
	assert(!cs_ThreadMessing.try_lock());

	auto funcDC = m_FuncDC;

	SharedActorInterestPtr resKeeper;
	if (funcDC)
		resKeeper = funcDC->GetInterestPtrOrNull();
	else
		resKeeper = GetResult()->GetInterestPtrOrNull();

	if (!resKeeper)
		return false;

	assert(s_NrActivatedOrRunningOperations[m_PhaseNumber] >= 0);
	assert(m_Status < task_status::activated);
	assert(m_TaskFunc);
	assert(!m_FuncDC || GetOperator()->CanRunParallel());

	assert(!IsActiveOrRunning(m_Status));
	assert(s_NrActivatedOrRunningOperations[m_PhaseNumber] >= 0);

	if (s_IsInLowRamMode && s_NrActivatedOrRunningOperations[m_PhaseNumber] > 0)
		return false;

	OperationContex_setActivated(this);

	m_ResKeeper = std::move(resKeeper);

	std::weak_ptr<OperationContext> selfWptr = shared_from_this();
	s_RadioActives.emplace_back(selfWptr);
	return true;
}

bool OperationContext::TryRunningTaskInline(phase_number targetPhaseNumber)
{
	assert(m_Suppliers.empty());
	StealOneTileTask(true);

	if (!GetUniqueLicenseToRun(targetPhaseNumber))
		return false;

	Run_Caller();
	return true;
}


bool  OperationContext::getUniqueLicenseToRun(phase_number targetPhaseNumber)
{
	assert(!cs_ThreadMessing.try_lock());
	auto status = getStatus();

	if (status == task_status::scheduled) // maybe it was placed back from active to scheduled because earlier phases became in focus again.
		return false;

	if (status >= task_status::running)
		return false;

	assert(status == task_status::activated);

	if (m_PhaseNumber > targetPhaseNumber)
	{
		scheduleRunnableTask(this); // place it back 
		return false;
	}

	DSM::CancelIfOutOfInterest(m_Result);

	m_Status = task_status::running;
	return true;
}

bool OperationContext::GetUniqueLicenseToRun(phase_number targetPhaseNumber)
{
	leveled_std_section::scoped_lock lock(cs_ThreadMessing);
	return getUniqueLicenseToRun(targetPhaseNumber);
}

void OperationContext::OnException() noexcept
{
	OnEnd(task_status::exception);
}

garbage_t OperationContext::onEnd(task_status status) noexcept
{
	if (m_Status >= task_status::cancelled)
	{
		assert(m_Suppliers.empty());
		return {};
	}

	assert(status != task_status::exception || m_Result->WasFailed(FR_Data));

	return separateResources(status);
}

void OperationContext::OnEnd(task_status status) noexcept
{
	assert(status >= task_status::cancelled);

	std::any garbage;

	leveled_std_section::scoped_lock lock(cs_ThreadMessing);
	garbage = onEnd(status);
	assert(getStatus() >= task_status::cancelled);
}

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
#endif
		auto nrRunning = --s_NrActivatedOrRunningOperations[m_PhaseNumber];

#if defined(MG_TRACE_OPERATIONCONTEXTS)

		auto nrRunningOCs = sd_RunningOC.size();
		MG_CHECK(sd_RunningOC.find(this) != sd_RunningOC.end());
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

garbage_t OperationContext::separateResources(task_status status)
{
	assert(cs_ThreadMessing.isLocked());

	assert(status >= task_status::cancelled); // new status must be a final status
	if (m_Status >= task_status::cancelled) // don't change an already established final status
		return {};
	
	releaseRunCount(status);
	assert(getStatus() == status);

	garbage_t releaseBin;
	if (!m_Suppliers.empty())
	{
		assert(status != task_status::done);
		for (const auto& supplier : m_Suppliers)
			m_Waiters.erase(this->weak_from_this());

		releaseBin |= std::move(m_Suppliers);
		assert(m_Suppliers.empty());
	}
	if (m_TaskFunc) 
	{
		// when done, safe_run would already have destroyed m_TaskFunc in order to remove shared ownership of suppliers before notifying waiters
		assert(status == task_status::exception || status == task_status::cancelled);
		releaseBin |= std::move(m_TaskFunc); // may contain argInterests that may release a lot upon destruction.
	}
	assert(!m_TaskFunc);
	auto resultItem = GetResult();
	if (resultItem)
		releaseBin |= std::move(resultItem->m_ReadAssets);
	if (m_ResKeeper)
		releaseBin |= std::move(m_ResKeeper); // may release interest of FuncDC, probably not a big thing, but it may release ownership of this, which therefore should have been called by a shared_ptr copy.
	assert(!m_ResKeeper);

	assert(!m_WriteLock || status == task_status::cancelled || status == task_status::exception); // all other routes outside Schedule go through Run_Caller, which alwayws release the writeLock on completion
	m_WriteLock = ItemWriteLock();

	if (m_FuncDC)
		releaseBin |= m_FuncDC->ResetOperContextImplAndStopSupplInterest();

	assert(!m_FuncDC);

	releaseBin |= disconnect_waiters();
	releaseBin |= make_any_scoped_exit( &StartOperationContexts ); // do this after releasing interests and related resources of suppliers

	return releaseBin;
}

bool OperationContext::CancelIfNoInterestOrForced(bool forced)
{
	if (!forced)
	{
		if (!m_Result || m_Result->GetInterestCount())
			return false;
	}

	std::any separatedResources;
	leveled_std_section::scoped_lock lock(cs_ThreadMessing);
	separatedResources = separateResources(task_status::cancelled); // destroy after lock
	return true;
}

bool OperationContext::HandleFail(const TreeItem* item)
{
	// note that all statusses of cancelled and higher: exception or done, are final, i.e. never change again once it is set.
	auto transientStatus = getStatus();
	if (transientStatus == task_status::exception)
		return true;
	if (transientStatus >= task_status::cancelled)
		return false;

	if (!item->WasFailed(FR_Data)) // this precheck filters out most calls without needing a lock on cs_ThreadMessing
		return false;

	RequestMainThreadOperProcessingBlocker letTheNotificationsComeAfter;

	std::any separatedResources;

	leveled_std_section::scoped_lock lock(cs_ThreadMessing);

	auto recheckedStatus = getStatus();
	if (recheckedStatus == task_status::exception)
		return true;
	if (recheckedStatus >= task_status::cancelled) // ignore this Fail if Context is already canceled or done.
		return false; 

	m_Result->Fail(item);
	assert(m_Result->WasFailed(FR_Data));
	separatedResources = separateResources(task_status::exception);
	assert(!m_ResKeeper);

	return true;
}

bool OperationContext::SetReadLock(std::vector<ItemReadLock>& locks, const TreeItem* si) 
{
	assert(m_PhaseNumber >= si->GetCurrFenceNumber());

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

bool OperationContext::connectArgs(const FutureSuppliers& allArgInterests)
{
	DBG_START("OperationContext", "connectArgs", MG_DEBUG_FUNCCONTEXT);
	DBG_TRACE(("FuncDC: %s", m_FuncDC->md_sKeyExpr));

	assert(cs_ThreadMessing.isLocked());
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
	assert(sd_ManagedContexts.find(this) == sd_ManagedContexts.end()); 
	sd_ManagedContexts.insert(this);
#endif

	return connected;
}

// *****************************************************************************
// Section: ScheduleCalcResult
// *****************************************************************************


struct OC_CalcResultFunc {
	mutable ArgRefs argRefs;
	mutable FutureSuppliers allInterests;

	void operator () (OperationContext* self, Explain::Context* context) const
	{
		SharedPtr<const FuncDC> funcDC = self->m_FuncDC.get();

#if defined(MG_DEBUG_OPERATIONS)
		if (self->m_Result && self->m_Result->m_BackRef && !self->m_Result->m_BackRef->IsCacheItem() && IsDataItem(self->m_Result.get_ptr()) && AsDataItem(self->m_Result.get_ptr())->GetAbstrDomainUnit()->GetNrTiles() > 1)
			reportF(ST_MinorTrace, "Starting calculation of %s with KeyExpr %s"
				, self->m_Result->m_BackRef->GetFullName().c_str()
				, funcDC ? AsFLispSharedStr(funcDC->GetLispRef(), FormattingFlags::ThousandSeparator).c_str() : "(null)"
			);
#endif //defined(MG_DEBUG_OPERATIONS)

		if (!funcDC)
			return;

		assert(funcDC && (funcDC->DoesHaveSupplInterest() || !funcDC->GetInterestCount() || context));

		assert(self->m_Status == task_status::running);
		assert(!SuspendTrigger::DidSuspend());

		std::vector<SharedPtr<const Actor>> statusActors; statusActors.reserve(argRefs.size());
		for (const auto& argRef : argRefs)
			statusActors.emplace_back(GetStatusActor(argRef));

		auto failureProcessor = [&]() -> bool
			{
				// forward FR_Validate and FR_Committed failures
				for (const auto& statusActor : statusActors)
					if (statusActor && statusActor->WasFailed())
					{
						funcDC->Fail(statusActor);
						return true;
					}
				return false;
			};

		if (failureProcessor())
			return;

		auto readLocks = self->SetReadLocks(allInterests);
		allInterests.clear();

		funcDC->StopSupplInterest();
		self->RunOperator(context, std::move(argRefs), std::move(readLocks)); // RunImpl() may destroy this and make m_FuncDC inaccessible // CONTEXT
		argRefs.clear();

		failureProcessor();
	}
};

bool OperationContext::ScheduleCalcResult(ArgRefs&& argRefs, Explain::Context* context)
{
	DBG_START("OperationContext", "ScheduleCalcResult", MG_DEBUG_FUNCCONTEXT);
	DBG_TRACE(("FuncDC: %s", m_FuncDC->md_sKeyExpr));

	assert(GetOperator());
	assert(!SuspendTrigger::DidSuspend());

	OperatorContextHandle operContext(true, m_FuncDC);

	TreeItemDualRef& resultHolder = *const_cast<FuncDC*>(m_FuncDC.get_nonnull());
	assert(resultHolder);

	MG_DEBUGCODE(const TreeItem * oldItem = resultHolder.GetOld());
	MG_DEBUGCODE(auto oper = GetOperator());
	dbg_assert(oldItem);

	assert(resultHolder.IsTmp() || resultHolder->GetInterestCount());
	assert(resultHolder.GetInterestCount());
	dbg_assert(!resultHolder.IsTmp()); // DEBUG 19/03/2020

	bool doASync = GetOperator()->CanRunParallel() && resultHolder.DoesHaveSupplInterest() && !context;

	assert(m_Result);

	if (m_Result->WasFailed(FR_Data))
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

	allArgInterests.reserve(argRefs.size() + (m_FuncDC ? m_FuncDC->m_OtherSuppliers.size() : 0)); // TODO G8: count better
	for (const auto& argRef : argRefs)
		if (argRef.index() == 0)
		{
			allArgInterests.emplace_back(std::get<FutureData>(argRef));
			assert(allArgInterests.back());
		}
	if (m_FuncDC)
		for (const DataController* dcPtr : m_FuncDC->m_OtherSuppliers)
		{
			auto otherSupplier = dcPtr->CallCalcResult();
			if (!otherSupplier)
			{
				if (dcPtr->WasFailed(FR_Data))
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
	if (!resultHolder.WasFailed(FR_Data) && !SuspendTrigger::DidSuspend())
	{
		auto func = OC_CalcResultFunc{ std::move(argRefs), allArgInterests }; // TOD: move in, but keep view of ArgInterests array

		m_TaskFunc = std::move(func);
		resultStatus = Schedule(m_FuncDC->IsNew() ? m_FuncDC->GetNew() : nullptr
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
	if (resultHolder.WasFailed(FR_Data))
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
		assert(CheckDataReady(supplResult->GetCurrUltimateItem()) || supplResult->WasFailed(FR_Data) || !supplResult->GetInterestCount() || SuspendTrigger::DidSuspend());
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

bool OperationContext::MustCalcArg(arg_index i, CharPtr firstArgValue) const
{
	return m_FuncDC->MustCalcArg(i, true, firstArgValue);
}
auto OperationContext_TakeTaskFunc(OperationContext* self)
{
	leveled_std_section::unique_lock lock(cs_ThreadMessing);
	auto taskFunc = std::move(self->m_TaskFunc);
	assert(!self->m_TaskFunc);
	return taskFunc;
}

void OperationContext::Run_with_catch() noexcept
{
	try {
		CancelableFrame frame(this);

		assert(m_Status == task_status::running);

		UpdateMarker::ChangeSourceLock tsLock(m_ActiveTimestamp, "safe_run");

		assert(m_TaskFunc);
		auto taskFunc = OperationContext_TakeTaskFunc(this);
		if (taskFunc)
			taskFunc(this, m_Context); // run the payload functor, set by ScheduleItemWriter
	}
	catch (const task_canceled&)
	{
		assert(getStatus() == task_status::cancelled); // clean-up was done ?
	}
	catch (...) {
		GetResult()->CatchFail(FR_Data);
	}

	ItemWriteLock localWriteLock;
	leveled_std_section::unique_lock lock(cs_ThreadMessing);
	localWriteLock = std::move(m_WriteLock);
	assert(!m_WriteLock);
	assert(!localWriteLock || localWriteLock.GetItem() == GetResult());

	// writeLock release here before OnEnd allows Waiters to start
}

void OperationContext::Run_with_cleanup() noexcept
{
	assert(!SuspendTrigger::DidSuspend());

	Run_with_catch();
	assert(!m_TaskFunc);

	if (GetResult()->WasFailed(FR_Data))
		OnException(); // clean-up
	else
	{
		assert(!SuspendTrigger::DidSuspend());
		OnEnd(task_status::done); // just set status to done and clean-up
	}

	// check that clean-up was done. This includes releasing the RunCount
	assert(!m_WriteLock);
}

void OperationContext::Run_Caller() noexcept
{
	DMS_SE_CALL_BEGIN

		assert(m_PhaseNumber >= s_CurrActivePhaseNumber);
		Run_with_cleanup();

	DMS_SE_CALL_END
}

// *****************************************************************************
// Section:     PopActiveSupplierss
// *****************************************************************************

struct prioritize_results
{
	WaiterSet waitingAndScheduledContexts;
	SupplierSet activatedContexts;
	context_array collectedActivations;
};

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

auto prioritize(OperationContextSPtr waiter) -> std::pair<SupplierSet, context_array>
{
	// Build the two sets once per Join:
	prioritize_results results;
	prioritize_impl(results, waiter);
	return { std::move(results.activatedContexts), std::move(results.collectedActivations) };
}

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

auto FindAndLicenceOnePriorityTasks(phase_number currFenceNumber) -> OperationContextSPtr
{
	std::vector<OperationContextSPtr> garbage;

	leveled_std_section::unique_lock lock(cs_ThreadMessing);

	while (!s_RadioActives.empty()) {
		auto oc = s_RadioActives.front(); s_RadioActives.pop_front();
		auto ocSPtr = oc.lock();
		if (!ocSPtr)
			continue; // expired

		// try to grab the license—only one thread ever wins per OC
		if (ocSPtr->getUniqueLicenseToRun(currFenceNumber))
			return ocSPtr; // we can do one piece of inline work—go do it (now you must) and re-check your fences/status
		else
			garbage.emplace_back(std::move(ocSPtr)); // maybe we-re the last owner and we dón't wat to destry here 
		// otherwise, someone else is already running it; try the next one
	}
	return {};
}


bool StealOneTask(phase_number currFenceNumber)
{
	StealOneTileTask(true)
		;
	auto ocSPtr = FindAndLicenceOnePriorityTasks(currFenceNumber);
	if (!ocSPtr)
		return false; // no work to do

	ocSPtr->Run_Caller(); // inline the work right here:
	return true;
}

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

task_status OperationContext::Join()
{
	if (!IsMetaThread())
		StealOneTileTask(true);

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
	OperationContext_ScheduleThis(this);

	MG_CHECK(GetStatus() != task_status::none); // being scheduled is a precondition

	while (GetStatus() <= task_status::running)
	{
		if (IsMetaThread() && !SuspendTrigger::BlockerBase::IsBlocked())
		{
			if (SuspendTrigger::DidSuspend())
				return task_status::suspended;
		}
		else
			if (GetStatus() == task_status::activated)
				if (TryRunningTaskInline(true))
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
					inlineTaskCandidate->TryRunningTaskInline(s_CurrActivePhaseNumber);  
				for (const auto& inlineTaskCandidateWPtr : collectedTasksToRun)
					if (auto inlineTaskCandidate = inlineTaskCandidateWPtr.lock())
						inlineTaskCandidate->TryRunningTaskInline(s_CurrActivePhaseNumber);  // already running elsewhere. go for something else before giving up on task collection effort
			}
		else			
*/
		StaticMtIncrementalLock<s_NrWaitingJoins> increaseTheNumberOfWaitingJoinsToAvoidWaitingForNothing;

		auto currentFinishCount = GetCurrFinishedCount();
		StartOperationContexts(); // OPTIMIZE, CONSISTENCY: Some tasks finished without calling this. Find out why and avoid wasting idle thread time; maybe all threads are waiting in a Join without new and current tasks being activated
		if (!IsMetaThread())
		{
			while (StealOneTask(m_PhaseNumber))
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
			assert(IsDefined(getScheduledContextsPos(this->shared_from_this()))); // always true for scheduled tasks outzide cs_ThreadMessing
		}
		if (m_Status == task_status::waiting_for_suppliers)
		{
			assert(!m_Suppliers.empty());
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
	dbg_assert((m_Result->m_ItemCount < 0) || CheckDataReady(m_Result->GetCurrUltimateItem()) || status == task_status::cancelled || status == task_status::exception || !m_Result->GetInterestCount());
	return status;
}

TIC_CALL void DoWorkWhileWaitingFor(phase_number maxPhaseNumber, task_status* fenceStatus)
{
	StealOneTileTask(true);

	while (!fenceStatus || IsActiveOrRunning(*fenceStatus))
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
		while (!fenceStatus || IsActiveOrRunning(*fenceStatus))
		{
			if (!StealOneTask(maxPhaseNumber))
				break;
			if (!fenceStatus)
				return; // let caller reconsider after one tasks has been done and no explicit termination condition variable is provided
		}

		leveled_std_section::unique_lock lock(cs_ThreadMessing);
		if (fenceStatus && *fenceStatus > task_status::running)
			break;

		if (currentFinishCount != s_CurrFinishedCount)
			continue;

		if (IsMetaThread() && HasMainThreadTasks())
			continue;

		if (SuspendTrigger::MustSuspend())
			return;

		// wait for conditioin that was certainly not met just after setting the thread messing lock
		cv_TaskCompleted.wait_for(lock.m_BaseLock, std::chrono::milliseconds(500));

		if (!fenceStatus)
			return; // let caller reconsider after one tasks has been done and no explicit termination condition variable is provided
	}
}

/// Wait for the task to finish or timeout
/// \param waitFor The time to wait for the task to finish
///
/// it assumes that the caller loops with a while condition that will only become true 
/// as a side-effect of a completing unknown task
/// if the caller knows which task to wait for, it should use the Join() method instead
///

void WaitForCompletedTaskOrTimeout()
{
	DoWorkWhileWaitingFor(phase_number(-1), nullptr);
}

void OperationContext::RunOperator(Explain::Context* context, ArgRefs argRefs, std::vector<ItemReadLock> readLocks)
{
	SharedPtr<const FuncDC> funcDC = m_FuncDC.get_ptr();
	if (!funcDC || funcDC->WasFailed(FR_Data))
		return;

	if (m_Result->WasFailed(FR_Data))
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
		if (funcDC->WasFailed(FR_MetaInfo))
			return;
		try {
			TreeItemDualRefContextHandle reportProgressAndErr(&resultHolder);

			actualResult = GetOperator()->CalcResult(resultHolder, std::move(argRefs), std::move(readLocks), context); // ============== payload

			assert(resultHolder || IsCanceled());
			assert(actualResult || SuspendTrigger::DidSuspend());
			assert(!IsDataItem(resultHolder.GetUlt()) || AsDataItem(resultHolder.GetUlt())->m_DataObject
				|| !actualResult
				|| CheckCalculatingOrReady(resultHolder.GetUlt())
				|| resultHolder->WasFailed(FR_Data)
			);
		}
		catch (const task_canceled&)
		{
			assert(m_Status == task_status::cancelled || m_Status == task_status::exception);
		}
		catch (...)
		{
			resultHolder.CatchFail(FR_Data); // Now done by TreeItemDualRef::DoFail
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
			assert(ri->GetIsInstantiated() || CheckCalculatingOrReady(ri) || resultHolder->WasFailed(FR_Data));
//			assert(CheckDataReady(ri) || resultHolder->WasFailed(FR_Data));

			assert(!resultHolder.IsNew() || resultHolder->m_LastChangeTS == resultHolder.m_LastChangeTS); // further changes in the resulting data must have caused resultHolder to invalidate, as IsNew results are passive
#endif
		}
	}
}
