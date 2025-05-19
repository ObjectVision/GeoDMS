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
static RunningOperationsCounter s_NrActivatedOrRunningOperations = 0; 
static phase_number s_CurrActivePhaseNumber = 0;
static bool s_IsInLowRamMode = false;


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

#if defined(MG_TRACE_OPERATIONCONTEXTS)

leveled_critical_section cs_OcAdm(item_level_type(0), ord_level_type::OperationContext, "OperationContextSet");

static UInt32 sd_OcCount;
static std::set<OperationContext*> sd_OC;
static std::set<OperationContext*> sd_RunningOC;

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
	auto oldCallback = SetWakeUpJoinersCallback(WakeUpJoiners);
	assert(oldCallback == nullptr);
}

TIC_CALL tg_maintainer::~tg_maintainer()
{
	assert(s_OcTaskGroup);

	auto oldCallback = SetWakeUpJoinersCallback(nullptr);
	assert(oldCallback == WakeUpJoiners);

//	dbg_assert(sd_RunningOC.empty());
//	dbg_assert(sd_OC.empty());
	{
		leveled_std_section::scoped_lock lock(cs_ThreadMessing);

		dbg_assert(sd_runOperationContextsRecursionCount == 0);

		s_ScheduledContextsMap.clear();
		s_RadioActives.clear();

		//	assert(s_NrActivatedOrRunningOperations == 0);
		assert(s_ScheduledContextsMap.empty()); ;
		assert(s_RadioActives.empty());
	}
	s_OcTaskGroup->cancel();
	s_OcTaskGroup->wait();

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

bool isSupplier(OperationContextSPtr waiter, OperationContextSPtr supplier)
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

void connect(OperationContextSPtr waiter, OperationContextSPtr supplier)
{
	DBG_START("OperationContextPtr", "connect", MG_DEBUGCONNECTIONS);
	DBG_TRACE(("waiter   = %s", AsText(waiter)));
	DBG_TRACE(("supplier = %s", AsText(supplier)));

	assert(!cs_ThreadMessing.try_lock());
	if (isSupplier(waiter, supplier))
		return;

	assert(waiter->GetOperator() && waiter->GetOperator()->CanRunParallel());

	waiter->m_Suppliers.insert(supplier);
	supplier->m_Waiters.insert(waiter);
}

void scheduleRunnableTask(OperationContext* self)
{
	assert(!cs_ThreadMessing.try_lock());

	// next StartOperationContexts() must not start scheduling tasks with phase numbers higher than this task.
	auto fn = self->m_PhaseNumber;
	MakeMin<phase_number>(s_CurrActivePhaseNumber, fn);
	s_ScheduledContextsMap[fn].emplace_back(self->shared_from_this());
	self->m_Status = task_status::scheduled;
}

// *****************************************************************************
// Section: disconnect suppliers
// *****************************************************************************

garbage_t OperationContext::disconnect_supplier(OperationContext* supplier)
{
	DBG_START("OperationContextPtr", "disconnect_supplier", MG_DEBUGCONNECTIONS);

	assert(!cs_ThreadMessing.try_lock());

	OperationContextWPtr wptr_this_waiter = weak_from_this();
	OperationContextSPtr sptr_supplier = supplier->weak_from_this().lock(); // inaccessible when called from supplier's destructor, i.e. when it is not owned anymore


#if defined(MG_DEBUG)
	DBG_TRACE(("waiter = %s", AsText(wptr_this_waiter)));
#endif

	assert(m_Status == task_status::waiting_for_suppliers);
	assert(!supplier->m_FuncDC || !supplier->m_FuncDC->m_OperContext);

	garbage_t garbage;
	if (sptr_supplier)
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
		DBG_TRACE(("add to queue %s", AsText(wptr_this_waiter)));

		if (m_Status == task_status::waiting_for_suppliers)
			scheduleRunnableTask(this);
		break;

	case task_status::exception:
		GetResult()->Fail(supplier->GetResult());
		[[fallthrough]];
	case task_status::cancelled:
	{
		garbage |= onEnd(supplierStatus);
		break;
	}
	default:
		assert(supplierStatus != task_status::running);
		assert(supplierStatus != task_status::suspended);
		assert(supplierStatus != task_status::scheduled);
		assert(supplierStatus != task_status::waiting_for_suppliers);
		assert(supplierStatus != task_status::none);
	}

	return garbage;
}

garbage_t OperationContext::disconnect()
{
	DBG_START("OperationContextPtr", "disconnect", MG_DEBUGCONNECTIONS);
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

	for (auto waiterPtr = m_Waiters.begin(); waiterPtr != m_Waiters.end();)
	{
		auto waiter = waiterPtr->lock();
		if (waiter)
		{
			garbage |= waiter->disconnect_supplier(this);
			garbage |= std::move(waiter);
		}
		m_Waiters.erase(waiterPtr++);
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
			if (s_NrActivatedOrRunningOperations)
				break; // try again later, when all operations of the current phase have completed
			s_CurrActivePhaseNumber = nextPhaseNumber;
			NotifyCurrentTargetCount();
		}

		auto& scheduledContexts = s_ScheduledContextsMap.begin()->second;

		auto currContext = scheduledContexts.begin();

		s_IsInLowRamMode = false; // reset for next activation round
		bool isLowOnFreeRamTested = false;

		for (; currContext != scheduledContexts.end(); ++currContext)
		{
			if (s_NrActivatedOrRunningOperations >= 1 && !isLowOnFreeRamTested) // HEURISTIC: only allow more than 1 operations when RAM isn't being exausted
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
				assert(s_NrActivatedOrRunningOperations >= 0);
			}
		}
		scheduledContexts.erase(scheduledContexts.begin(), currContext);
		if (!scheduledContexts.empty())
			break;

		s_ScheduledContextsMap.erase(s_ScheduledContextsMap.begin());

	}

	if (!s_NrActivatedOrRunningOperations)
		NotifyCurrentTargetCount();

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
						self->TryRunningTaskInline();
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
	assert(!cs_ThreadMessing.try_lock());
	self->m_Status = task_status::activated;
	++s_NrActivatedOrRunningOperations;

	assert(IsActiveOrRunning(self->m_Status));
	assert(s_NrActivatedOrRunningOperations > 0);

#if defined(MG_TRACE_OPERATIONCONTEXTS)
	sd_RunningOC.insert(self);
#endif

}

void OperationContex_SetActivated(OperationContext* self)
{
	leveled_std_section::scoped_lock lock(cs_ThreadMessing);
	OperationContex_setActivated(self);
}

// *****************************************************************************
// Section:     OperatorContext
// *****************************************************************************

OperationContext::OperationContext()
{
	assert(IsMetaThread());
	
	#if defined(MG_TRACE_OPERATIONCONTEXTS)

		leveled_critical_section::scoped_lock lock(cs_OcAdm);
		++sd_OcCount;
		sd_OC.insert(this);

	#endif
}

/// <summary>
///		This constructor is used to create a new OperationContext with a given FuncDC.
/// </summary>
/// <param name="self"></param>
/// 
/// Call ScheduleCalcResult() to start the calculation.
/// todo: integreate ScheduleCalcResult into the constructor.

OperationContext::OperationContext(const FuncDC* self)
	: m_FuncDC(self)
	, m_PhaseNumber(self->GetPhaseNumber())
{
	assert(m_PhaseNumber);

	DBG_START("OperationContext", "CTor", MG_DEBUG_FUNCCONTEXT);
	DBG_TRACE(("FuncDC: %s", self->md_sKeyExpr));

	#if defined(MG_TRACE_OPERATIONCONTEXTS)

		leveled_critical_section::scoped_lock lock(cs_OcAdm);
		++sd_OcCount;
		sd_OC.insert(this);

	#endif
}

OperationContext::~OperationContext()
{
	DBG_START("OperationContext", "DTor", MG_DEBUG_FUNCCONTEXT);
	DBG_TRACE(("FuncDC: %s", m_FuncDC ? m_FuncDC->md_sKeyExpr.c_str() : "(leeg)"));

	assert(!m_FuncDC || !m_FuncDC->m_OperContext);

	assert(m_Status != task_status::running);
	assert(m_Status != task_status::suspended); // cancel, exception or done caught.

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

task_status OperationContext::Schedule(TreeItem* item, const FutureSuppliers& allInterest, bool runDirect, Explain::Context* context)
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
		m_WriteLock = ItemWriteLock(item, std::weak_ptr(shared_from_this())); // sets item->m_Producer to this; no future access right yet

	if (runDirect)
	{
		assert(m_Suppliers.empty());
		auto supplStatus = JoinSupplOrSuspendTrigger();
		assert(supplStatus != task_status::cancelled);
		if (supplStatus != task_status::done)
			return supplStatus;

		OperationContex_SetActivated(this);
		if (TryRunningTaskInline())
			return GetStatus();
		return Join();
	}

	assert(!m_FuncDC || GetOperator()->CanRunParallel());

	bool mustConsiderRun = false;
	{
		leveled_std_section::scoped_lock lock(cs_ThreadMessing);
		if (m_Status < task_status::scheduled)
		{
			assert(!IsActiveOrRunning(m_Status));

			bool connectedArgs = connectArgs(allInterest);
			if (connectedArgs)
			{
				assert(m_PhaseNumber >= s_CurrActivePhaseNumber); // must be a Superior of the non-completed suppliers, which must have at least s_CurrActivePhaseNumber
				m_Status = task_status::waiting_for_suppliers;
			}
			else
			{
				scheduleRunnableTask(this); assert(m_Status == task_status::scheduled);
				assert(m_PhaseNumber >= s_CurrActivePhaseNumber); // scheduleRunnableTask will have decreasd the s_CurrActivePhaseNumber if this task needed that.
				mustConsiderRun = true;
			}
		}
	}
	if (mustConsiderRun)
		StartOperationContexts();
	return GetStatus();
}

THREAD_LOCAL OperationContext* sl_CurrOperationContext = nullptr;

OperationContext::CancelableFrame::CancelableFrame(OperationContext* self)
: m_Prev(sl_CurrOperationContext)
{
	sl_CurrOperationContext = self;
}

OperationContext::CancelableFrame::~CancelableFrame()
{
	sl_CurrOperationContext = m_Prev;
}

OperationContext* OperationContext::CancelableFrame::CurrActive()
{
	return sl_CurrOperationContext;
}

bool OperationContext::CancelableFrame::CurrActiveCanceled()
{
	auto ca = CurrActive();
	return ca && ca->IsCanceled();
}

void OperationContext::CancelableFrame::CurrActiveCancelIfNoInterestOrForced(bool forceCancel)
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

	assert(s_NrActivatedOrRunningOperations >= 0);
	assert(m_Status < task_status::activated);
	assert(m_TaskFunc);
	assert(!m_FuncDC || GetOperator()->CanRunParallel());

	assert(!IsActiveOrRunning(m_Status));
	assert(s_NrActivatedOrRunningOperations >= 0);

	if (s_IsInLowRamMode && s_NrActivatedOrRunningOperations > 0)
		return false;

	OperationContex_setActivated(this);

	m_ResKeeper = std::move(resKeeper);

	std::weak_ptr<OperationContext> selfWptr = shared_from_this();
	s_RadioActives.emplace_back(selfWptr);
	return true;
}

bool OperationContext::TryRunningTaskInline()
{
	assert(m_Suppliers.empty());

	if (!GetUniqueLicenseToRun())
		return false;

	safe_run_caller();
	return true;
}


bool  OperationContext::getUniqueLicenseToRun()
{
	assert(!cs_ThreadMessing.try_lock());
	auto status = getStatus();
	assert(status >= task_status::activated);

	if (status >= task_status::running)
		return false;

	assert(status == task_status::activated);
	if (m_PhaseNumber > s_CurrActivePhaseNumber)
	{
		scheduleRunnableTask(this); // move it back 
		return false;
	}

	assert(m_PhaseNumber == s_CurrActivePhaseNumber);

	DSM::CancelIfOutOfInterest(m_Result);

	m_Status = task_status::running;
	return true;
}

bool OperationContext::GetUniqueLicenseToRun()
{
	leveled_std_section::scoped_lock lock(cs_ThreadMessing);
	return getUniqueLicenseToRun();
}

void OperationContext::OnException() noexcept
{
	OnEnd(task_status::exception);
}

garbage_t OperationContext::onEnd(task_status status) noexcept
{
	if (m_Status >= task_status::cancelled)
		return {};

	assert(status != task_status::exception || m_Result->WasFailed(FR_Data));

	return separateResources(status);
}

void OperationContext::OnEnd(task_status status) noexcept
{
	assert(status >= task_status::cancelled);

	std::any garbage;

	leveled_std_section::scoped_lock lock(cs_ThreadMessing);
	garbage = onEnd(status);
}

void OperationContext::releaseRunCount(task_status status)
{
	assert(!IsActiveOrRunning(status));

	assert(cs_ThreadMessing.isLocked());

	assert(m_Status <= task_status::running);
	if (IsActiveOrRunning(m_Status))
	{
		assert(s_NrActivatedOrRunningOperations > 0);
		auto nrRunning = --s_NrActivatedOrRunningOperations;
#if defined(MG_TRACE_OPERATIONCONTEXTS)
		sd_RunningOC.erase(this);
#endif
		if (!nrRunning)
			wakeUpJoiners();
	}
	assert(s_NrActivatedOrRunningOperations >= 0);
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

	garbage_t releaseBin;

//	if (m_TaskFunc)
//		releaseBin |= std::move(m_TaskFunc); // may contain argInterests that may release a lot upon destruction.
//	m_TaskFunc = {};
	auto resultItem = GetResult();
	if (resultItem)
		releaseBin |= std::move(resultItem->m_ReadAssets);
	if (m_ResKeeper)
		releaseBin |= std::move(m_ResKeeper); // may release interest of FuncDC, probably not a big thing, but it may release ownership of this, which therefore should have been called by a shared_ptr copy.
	assert(!m_ResKeeper);

	assert(!m_WriteLock || status == task_status::cancelled || status == task_status::exception); // all other routes outside Schedule go through safe_run_caller, which alwayws release the writeLock on completion
	m_WriteLock = ItemWriteLock();

	if (m_FuncDC)
		releaseBin |= m_FuncDC->ResetOperContextImplAndStopSupplInterest();

	assert(!m_FuncDC);

	releaseBin |= disconnect();
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
		MG_CHECK(futureSupplier);
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

bool OperationContext::connectArgs(const FutureSuppliers& allInterests)
{
	if (!m_FuncDC)
		return false;

	DBG_START("OperationContext", "connectArgs", MG_DEBUG_FUNCCONTEXT);
	DBG_TRACE(("FuncDC: %s", m_FuncDC->md_sKeyExpr));

	assert(cs_ThreadMessing.isLocked());
	bool connected = false;
	for (const auto& supplierInterest: allInterests)
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
		if (status > task_status::none && status < task_status::cancelled)
		{
			assert(supplOperationContext->m_PhaseNumber <= m_PhaseNumber);
			connect(shared_from_this(), supplOperationContext);
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
	OperationContext* self;
	mutable ArgRefs argRefs;
	mutable FutureSuppliers allInterests;

	void operator () (Explain::Context* context) const
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

		auto readLocks = self->SetReadLocks(allInterests);
		allInterests.clear();

		funcDC->StopSupplInterest();
		self->RunOperator(context, std::move(argRefs), std::move(readLocks)); // RunImpl() may destroy this and make m_FuncDC inaccessible // CONTEXT
		argRefs.clear();

		// forward FR_Validate and FR_Committed failures
		for (const auto& statusActor : statusActors)
			if (statusActor && statusActor->WasFailed())
				funcDC->Fail(statusActor);
	}
};

bool OperationContext::ScheduleCalcResult(Explain::Context* context, ArgRefs&& argRefs)
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

	FutureSuppliers allInterests;

	allInterests.reserve(argRefs.size() + (m_FuncDC ? m_FuncDC->m_OtherSuppliers.size() : 0)); // TODO G8: count better
	for (const auto& argRef : argRefs)
		if (argRef.index() == 0)
		{
			allInterests.emplace_back(std::get<FutureData>(argRef));
			assert(allInterests.back());
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
			allInterests.emplace_back(otherSupplier);
		}

	task_status resultStatus = m_Status;
	if (!resultHolder.WasFailed(FR_Data) && !SuspendTrigger::DidSuspend())
	{
		auto func = OC_CalcResultFunc{ this, std::move(argRefs), allInterests };

		resultStatus = ScheduleItemWriter(MG_SOURCE_INFO_CODE("OperationContext::SheduleCalcResult") m_FuncDC->IsNew() ? m_FuncDC->GetNew() : nullptr
			, std::move(func)
			, allInterests
			, !doASync, context
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
	}

	for (auto& oc: suppliers)
	{
		if (!oc)
			continue;

		const TreeItem* supplResult = oc->GetResult();
		assert(supplResult); 
		assert(oc->GetStatus() >= task_status::scheduled || CheckDataReady(supplResult) || supplResult->WasFailed(FR_Data) || !supplResult->GetInterestCount());
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

void OperationContext::safe_run_with_catch() noexcept
{
	try {
		OperationContext::CancelableFrame frame(this);

		assert(m_Status == task_status::running);

		UpdateMarker::ChangeSourceLock tsLock(m_ActiveTimestamp, "safe_run");

		assert(m_TaskFunc);
		m_TaskFunc(m_Context); // run the payload functor, set by ScheduleItemWriter
	}
	catch (const task_canceled&)
	{
		assert(getStatus() == task_status::cancelled); // clean-up was done ?
	}
	catch (...) {
		GetResult()->CatchFail(FR_Data);
	}

	m_TaskFunc = {};  // clean-up, more to come in separateResources

	auto localWriteLock = std::move(m_WriteLock);
	assert(!m_WriteLock);
	assert(!localWriteLock || localWriteLock.GetItem() == GetResult());
	// writeLock release here before OnEnd allows Waiters to start
}

void OperationContext::safe_run_with_cleanup() noexcept
{
	assert(!SuspendTrigger::DidSuspend());

	safe_run_with_catch();

	if (GetResult()->WasFailed(FR_Data))
		OnException(); // clean-up
	else
	{
		assert(!SuspendTrigger::DidSuspend());
		OnEnd(task_status::done); // just set status to done and clean-up
	}

	// check that clean-up was done. This includes releasing the RunCount
	assert(!m_TaskFunc);
	assert(!m_WriteLock);
}

void OperationContext::safe_run_caller() noexcept
{
	DMS_SE_CALL_BEGIN

		assert(m_PhaseNumber >= s_CurrActivePhaseNumber);
		safe_run_with_cleanup();
		assert(m_PhaseNumber >= s_CurrActivePhaseNumber);

	DMS_SE_CALL_END
}

// *****************************************************************************
// Section:     PopActiveSupplierss
// *****************************************************************************

struct prioritize_results
{
	SupplierSet waitingAndScheduledContexts;
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
	leveled_std_section::unique_lock lock(cs_ThreadMessing);

	while (!s_RadioActives.empty()) {
		auto oc = s_RadioActives.front(); s_RadioActives.pop_front();
		auto ocSPtr = oc.lock();
		if (!ocSPtr)
			continue; // expired

		// try to grab the license—only one thread ever wins per OC
		if (ocSPtr->getUniqueLicenseToRun())
			return ocSPtr; // we can do one piece of inline work—go do it (now you must) and re-check your fences/status
		// otherwise, someone else is already running it; try the next one
	}
	return {};
}


bool StealOneTask(phase_number currFenceNumber)
{
	auto ocSPtr = FindAndLicenceOnePriorityTasks(currFenceNumber);
	if (!ocSPtr)
		return false; // no work to do

	ocSPtr->safe_run_caller(); // inline the work right here:
	return true;
}

bool CurrActiveTaskHasRunCount()
{
	auto ca = OperationContext::CancelableFrame::CurrActive();
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
	if (CurrActiveTaskHasRunCount())
		reportF(MsgCategory::other, SeverityTypeID::ST_MinorTrace, "OperationContext(%s)::Join called from Active Context %s"
			, GetResult()->GetFullName()
			, OperationContext::CancelableFrame::CurrActive()->GetResult()->GetFullName()
		);
	if (IsMainThread() && s_CurrBlockedPhaseNumber && s_CurrBlockedPhaseNumber <= m_PhaseNumber)
		throwErrorF("PhaseContainer", "Invalid Recursion, OperationContext(%s)::Join called from updating %s for %s"
		,	GetResult()->GetFullName()
		,	s_CurrBlockedPhaseItem->GetFullName()
		,	s_CurrPhaseContainer->GetFullName()
		);
	MG_CHECK(m_Status != task_status::none); // being scheduled is a precondition


	while (m_Status <= task_status::running)
	{
		if (SuspendTrigger::DidSuspend())
			return task_status::suspended;

		if (m_Status == task_status::activated)
			if (TryRunningTaskInline())
				return m_Status;

		if (IsMetaThread())
		{
			ProcessMainThreadOpers();
			ProcessSuspendibleTasks();
			if (SuspendTrigger::DidSuspend())
				return task_status::suspended;
		}

		if (IsMetaThread() && m_PhaseNumber <= s_CurrActivePhaseNumber)
			while (true)
			{
				auto [activatedContexts, collectedTasksToRun] = PopActiveSuppliers(shared_from_this());
				StartCollectedOperationContexts(std::move(collectedTasksToRun));
				if (activatedContexts.empty() && collectedTasksToRun.empty())
					break;
				// run as much as possible and needed for this Join before giving up on task collection effort
				for (const auto& inlineTaskCandidate : activatedContexts)
					inlineTaskCandidate->TryRunningTaskInline();  
				for (const auto& inlineTaskCandidateWPtr : collectedTasksToRun)
					if (auto inlineTaskCandidate = inlineTaskCandidateWPtr.lock())
						inlineTaskCandidate->TryRunningTaskInline();  // already running elsewhere. go for something else before giving up on task collection effort
			}
		else			
			while (m_Status <= task_status::running)
				if (!StealOneTask(m_PhaseNumber))
					break;

		StartOperationContexts(); // OPTIMIZE, CONSISTENCY: Some tasks finished without calling this. Find out why and avoid wasting idle thread time; maybe all threads are waiting in a Join without new and current tasks being activated

		leveled_std_section::unique_lock lock(cs_ThreadMessing);
		if (m_Status > task_status::running)
			break;
		if (m_Status == task_status::activated)
			continue; // try to run it inline in the next iteration

		if (m_Status == task_status::scheduled)
		{
			assert(m_Suppliers.empty()); // scheduled since the last call of RunOperations or not activated because of s_IsInLowRamMode
			assert(IsDefined(getScheduledContextsPos(this->shared_from_this()))); // always true for scheduled tasks outzide cs_ThreadMessing
			continue; // try to activate and run it
		}
		if (m_Status == task_status::waiting_for_suppliers)
		{
			assert(!m_Suppliers.empty());
			continue; // try to run suppliers inline in the next iteration
		}

		assert(m_Status == task_status::running);

		if (IsMetaThread() && HasMainThreadTasks())
			continue; // do that first 

		// or wait for conditioin that was certainly not met just after setting the thread messing lock
		cv_TaskCompleted.wait_for(lock.m_BaseLock, std::chrono::milliseconds(500));
	}
	assert(m_Status > task_status::running);
	dbg_assert(CheckDataReady(m_Result->GetCurrUltimateItem()) || m_Status == task_status::cancelled || m_Status == task_status::exception || !m_Result->GetInterestCount());
	return m_Status;
}

/// Wait for the task to finish or timeout
/// \param waitFor The time to wait for the task to finish
///
/// it assumes that the caller loops with a while condition that will only become true 
/// as a side-effect of a completing unknown task
/// if the caller knows which task to wait for, it should use the Join() method instead
///

void WaitForCompletedTaskOrTimeout(std::chrono::milliseconds waitFor)
{
	assert(!SuspendTrigger::DidSuspend());
	if (IsMetaThread())
	{
		ProcessMainThreadOpersAndTasks();
		if (SuspendTrigger::DidSuspend())
			return;
	}

	StartOperationContexts();

	leveled_std_section::unique_lock lock(cs_ThreadMessing);
	if (IsMetaThread() && HasMainThreadTasks())
		return; // don't wait for other threads if we have main thread tasks to do

	if (!s_NrActivatedOrRunningOperations)
		return; // no tasks to wait for, pingpong back to schedule some more without waiting first

	cv_TaskCompleted.wait_for(lock.m_BaseLock, waitFor);
}

TIC_CALL void DoWorkWhileWaitingFor(task_status* fenceStatus)
{
	while (IsActiveOrRunning(*fenceStatus))
	{
		if (IsMetaThread())
		{
//			if (SuspendTrigger::MustSuspend())
//				return false;
			ProcessMainThreadOpers();
			ProcessSuspendibleTasks();
		}

		while (IsActiveOrRunning(*fenceStatus))
			if (!StealOneTask(-1))
				break;

		StartOperationContexts(); // OPTIMIZE, CONSISTENCY: Some tasks finished without calling this. Find out why and avoid wasting idle thread time; maybe all threads are waiting in a Join without new and current tasks being activated

		leveled_std_section::unique_lock lock(cs_ThreadMessing);
		if (*fenceStatus > task_status::running)
			break;

		if (IsMetaThread() && HasMainThreadTasks())
			continue;

		// wait for conditioin that was certainly not met just after setting the thread messing lock
		cv_TaskCompleted.wait_for(lock.m_BaseLock, std::chrono::milliseconds(500));
	}
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
