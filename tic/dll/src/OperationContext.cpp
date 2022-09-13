#include "TicPCH.h"
#pragma hdrstop

#include <windows.h>

#include "OperationContext.ipp"

#include <agents.h>
#include <deque>

#include "Parallel.h"
#include "dbg/Check.h"
#include "dbg/DmsCatch.h"
#include "dbg/SeverityType.h"
#include "ser/AsString.h"
#include "utl/memGuard.h"
#include "utl/mySPrintF.h"
#include "utl/scoped_exit.h"
#include "utl/IncrementalLock.h"

#include "LockLevels.h"

#include "Parallel.h"

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
	OperatorContextHandle(TokenID operID, bool mustCalc, const FuncDC* funcDC)
		: m_MustCalc(mustCalc), m_OperID(operID), m_FuncDC(funcDC)
	{}

protected:
void GenerateDescription() override;

private:
	bool              m_MustCalc;
	TokenID           m_OperID;
	const FuncDC*     m_FuncDC;
};

/********** Implementation **********/

void OperatorContextHandle::GenerateDescription()
{
	UInt32 c = 0;
	SharedStr msg = mySSPrintF("while operator %s is %s\n",
		GetTokenStr(m_OperID).c_str(),
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

#include "ASync.h"

leveled_std_section cs_ThreadMessing(item_level_type(0), ord_level_type::ThreadMessing, "LockedThreadMessing");
std::condition_variable cv_TaskCompleted;

std::deque<OperationContextWPtr> s_ScheduledContexts;

static std::atomic<Int32> s_NrRunningOperations = 0;
static Int32 s_nrVCPUs = GetNrVCPUs();

//using dep_link = Point<OperationContext * > ;

#if defined(MG_DEBUG)
static std::set<const OperationContext*> sd_ManagedContexts;

struct ReportOnExit {
	~ReportOnExit()
	{
		dms_assert(sd_ManagedContexts.empty());
		dms_assert(s_NrRunningOperations == 0);
		dms_assert(s_ScheduledContexts.empty());
	}

};
ReportOnExit exitHere;

#endif

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
	dms_assert(!sa && !sb || sa.get() == sb.get());
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
	dms_assert(ptr);

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

	dms_assert(cs_ThreadMessing.isLocked());
	if (isSupplier(waiter, supplier))
		return;

	waiter->m_Suppliers.insert(supplier);
	supplier->m_Waiters.insert(waiter);
}

bool WaitForCompletedTaskOrTimeout(std::chrono::milliseconds waitFor)
{
	leveled_std_section::unique_lock lock(cs_ThreadMessing);
	return cv_TaskCompleted.wait_for(lock.m_BaseLock, waitFor) != std::cv_status::timeout;
}

garbage_t OperationContext::disconnect_supplier(OperationContext* supplier)
{
	DBG_START("OperationContextPtr", "disconnect_supplier", MG_DEBUGCONNECTIONS);

	OperationContextWPtr wptr_this_waiter = weak_from_this();
	OperationContextSPtr sptr_supplier = supplier->weak_from_this().lock(); // inaccessible when called from supplier's destructor, i.e. when it is not owned anymore
//	dms_assert(wptr_this_waiter.lock()); // must have been disconnected before destroyed.
//	dms_assert(wptr_supplier.lock()); // must have been disconnected before destroyed.


#if defined(MG_DEBUG)
	DBG_TRACE(("waiter = %s", AsText(wptr_this_waiter)));
#endif
	dms_assert(!supplier->m_FuncDC || !supplier->m_FuncDC->m_OperContext);
	garbage_t garbage;
	if (sptr_supplier)
	{
		m_Suppliers.erase(sptr_supplier);
		garbage |= std::move(sptr_supplier);
	}

#if defined(MG_DEBUG)
	for (const auto& s : m_Suppliers)
	{
		dms_assert(s.get() != supplier);
	}
#endif

	auto supplierStatus = supplier->getStatus();
	switch (supplierStatus)
	{
	case task_status::done:
		dms_assert(IsDataReady(supplier->m_Result));
		if (!m_Suppliers.empty())
			break;

		dbg_assert(m_Suppliers.empty());
		DBG_TRACE(("add to queue %s", AsText(wptr_this_waiter)));

		if (m_Status == task_status::scheduled)
			s_ScheduledContexts.emplace_back(shared_from_this());
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
		dms_assert(supplierStatus != task_status::running);
		dms_assert(supplierStatus != task_status::suspended);
		dms_assert(supplierStatus != task_status::scheduled);
		dms_assert(supplierStatus != task_status::none);
	}

	return garbage;
}

garbage_t OperationContext::disconnect()
{
	DBG_START("OperationContextPtr", "disconnect", MG_DEBUGCONNECTIONS);
	DBG_TRACE(("this = %s", AsText(this)))

#if defined(MG_DEBUG)
	dms_assert( sd_ManagedContexts.find(this) != sd_ManagedContexts.end() || !m_FuncDC);
	sd_ManagedContexts.erase(this);
#endif

	dms_assert(cs_ThreadMessing.isLocked());

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

UInt32 s_RunOperationContextsCount = 0;

garbage_t runOperationContexts()
{
	dms_assert(cs_ThreadMessing.isLocked());

	dms_assert(!s_RunOperationContextsCount);

	DynamicIncrementalLock s_ActivationGuard(s_RunOperationContextsCount);

	garbage_t cancelGarbage;

	auto lastProcessedContext = s_ScheduledContexts.begin();
	auto currContext = lastProcessedContext;
	bool isLowOnFreeRamTested = false;

	for (; s_NrRunningOperations < s_nrVCPUs && currContext != s_ScheduledContexts.end(); ++currContext)
	{
		if (s_NrRunningOperations >= 2 && !isLowOnFreeRamTested) // HEURISTIC: only allow more than 2 operations when RAM isn't being exausted
		{
			if (IsLowOnFreeRAM())
				break;
			isLowOnFreeRamTested = true;
		}
		auto operContext = currContext->lock();
		if (operContext)
		{
			cancelGarbage |= operContext->shared_from_this(); // copy shared_ptr into container for destruction consideration outside current critical section
			dms_assert(operContext->m_Status >= task_status::scheduled);
			if (operContext->m_Status < task_status::activated)
			{
				dms_assert(operContext->m_TaskFunc);
				if (DSM::IsCancelling())
				{
					cancelGarbage |= operContext->separateResources(task_status::cancelled);
					continue;
				}
				auto funcDC = operContext->m_FuncDC;
				SharedActorInterestPtr resKeeper;
				if (funcDC)
					resKeeper = funcDC->GetInterestPtrOrNull();
				else
					resKeeper = operContext->GetResult()->GetInterestPtrOrNull();

				if (!resKeeper)
					continue;

				dms_assert(operContext->m_Status < task_status::activated);
				dms_assert(operContext->m_TaskFunc);

				operContext->activateTaskImpl(std::move(resKeeper));
				dms_assert(s_NrRunningOperations >= 0);
			}
		}
	}
	s_ScheduledContexts.erase(lastProcessedContext, currContext);

	dbg_assert(s_NrRunningOperations >= 0 || s_ScheduledContexts.size() == 0);
	if (!s_NrRunningOperations)
	{
		NotifyCurrentTargetCount();
	}

	return cancelGarbage;
}

void RunOperationContexts()
{
//	if (IsMainThread() && InterestRetainContextBase::IsActive())
//		return;
	if (s_RunOperationContextsCount)
		return;
	
	std::any receivedGarbage;
	leveled_std_section::scoped_lock lock(cs_ThreadMessing);
	receivedGarbage = runOperationContexts();
}

// *****************************************************************************
// Section:     OperatorContextBase
// *****************************************************************************

#if defined(MG_DEBUG)

	leveled_critical_section cs_OcAdm(item_level_type(0), ord_level_type::OperationContext, "OperationContextSet");
	UInt32 sd_OcCount;
	std::set<OperationContext*> sd_OC;
	void reportOC(CharPtr source, OperationContext* ocPtr)
	{
		auto item = ocPtr->GetResult();
		reportF_without_cancellation_check(ST_MajorTrace, "OperationContext %s %d: %s", source, int(ocPtr->GetStatus()), item ? item->GetSourceName().c_str() : "");
	}
	auto reportRemaingOcOnExit = make_scoped_exit([]()
		{
			for (auto ocPtr : sd_OC)
				reportOC("Leaked", ocPtr);
		});
	
#endif

OperationContext::OperationContext()
{
	#if defined(MG_DEBUG)

		leveled_critical_section::scoped_lock lock(cs_OcAdm);
		++sd_OcCount;
		sd_OC.insert(this);

	#endif
}

OperationContext::OperationContext(const FuncDC* self, const AbstrOperGroup* og)
	: m_FuncDC(self)
	, m_OperGroup(og)
{
	DBG_START("OperationContext", "CTor", MG_DEBUG_FUNCCONTEXT);
	DBG_TRACE(("FuncDC: %s", self->md_sKeyExpr));

#if defined(MG_DEBUG)

	leveled_critical_section::scoped_lock lock(cs_OcAdm);
	++sd_OcCount;
	sd_OC.insert(this);

#endif
}

OperationContext::~OperationContext()
{
	DBG_START("OperationContext", "DTor", MG_DEBUG_FUNCCONTEXT);
	DBG_TRACE(("FuncDC: %s", m_FuncDC ? m_FuncDC->md_sKeyExpr.c_str() : "(leeg)"));

	dms_assert(!m_FuncDC || !m_FuncDC->m_OperContext);

	dms_assert(m_Status != task_status::running);
	dms_assert(m_Status != task_status::suspended); // cancel, exception or done caught.

	OnEnd(task_status::cancelled);
	dms_assert(m_Status != task_status::scheduled); // cancel, exception or done caught.
	dms_assert(m_Status == task_status::exception || m_Status == task_status::cancelled || m_Status == task_status::done || m_Status == task_status::none); // cancel, exception or done caught.

#if defined(MG_DEBUG)

	leveled_critical_section::scoped_lock lock(cs_OcAdm);
	--sd_OcCount;
	sd_OC.erase(this);

#endif
}

task_status OperationContext::Schedule(TreeItem* item, const FutureSuppliers& allInterest, bool runDirect)
{
	dms_assert(IsMainThread());
	dms_assert(UpdateMarker::IsInActiveState());

	if (item)
		m_Result = item;
	else
		m_Result = m_FuncDC->GetOld();
	dms_assert(m_Result && m_Result->HasInterest());

	if (!runDirect && !IsMultiThreaded2())
		runDirect = true;

	dms_assert(m_Status != task_status::suspended || runDirect);
	m_ActiveTimestamp = UpdateMarker::GetActiveTS(MG_DEBUG_TS_SOURCE_CODE("Obtaining active frame for produce item task"));

	if (item)
		m_WriteLock = ItemWriteLock(item, std::weak_ptr(shared_from_this())); // sets item->m_Producer to this; no future access right yet

	bool mustConsiderRun = false;
	{
		leveled_std_section::scoped_lock lock(cs_ThreadMessing);
		if (m_Status >= task_status::scheduled)
			return m_Status;

		bool connectedArgs = connectArgs(allInterest);
		if (!runDirect)
		{
			dms_assert(!IsRunningOperation(m_Status));
			m_Status = task_status::scheduled;
			if (!connectedArgs)
			{
				s_ScheduledContexts.emplace_back(shared_from_this());
				mustConsiderRun = true;
			}
		}
	}
	if (runDirect)
	{
		auto supplStatus = JoinSupplOrSuspendTrigger();
		dms_assert(supplStatus != task_status::cancelled);
		if (supplStatus != task_status::done)
			return supplStatus;
		dms_assert(m_Status < task_status::scheduled);
		return TryActivateTaskInline();
	}

	if (mustConsiderRun)
		RunOperationContexts();
	return task_status::scheduled;
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

dms_task OperationContext::GetTask() const
{
	leveled_std_section::scoped_lock lock(cs_ThreadMessing);

	return m_Task;
}

void OperationContext::setTask(dms_task&& newTask)
{
	dms_assert(is_empty(m_Task));
	m_Task = newTask;
	dms_assert(!is_empty(m_Task));
}

void OperationContext::activateTaskImpl(SharedActorInterestPtr&& resKeeper)
{
	dms_assert(cs_ThreadMessing.isLocked());

	dms_assert(s_NrRunningOperations >= 0);
	dms_assert(m_Status < task_status::activated);
	dms_assert(m_TaskFunc);

	if (!getUniqueLicenseToRun())
		return;

	m_ResKeeper = std::move(resKeeper);

	std::weak_ptr<OperationContext> selfWptr = shared_from_this();
	auto selfCaller = [selfWptr]() { auto self = selfWptr.lock(); if (self) self->safe_run(); };

	setTask(dms_task(selfCaller));
}

bool OperationContext::getUniqueLicenseToRun()
{
//	dms_assert(m_Suppliers.empty());
	dms_assert(cs_ThreadMessing.isLocked());

	if (m_Status >= task_status::activated || !m_TaskFunc)
	{
		return false;
	}

	dms_assert(!IsRunningOperation(m_Status));
	dms_assert(s_NrRunningOperations >= 0);

	m_Status = task_status::activated;
	++s_NrRunningOperations;

	dms_assert(IsRunningOperation(m_Status));
	dms_assert(s_NrRunningOperations > 0);

	return true;
}

task_status OperationContext::TryActivateTaskInline()
{
	dms_assert(m_Suppliers.empty());
	dms_assert(m_Status < task_status::scheduled);

	SuspendTrigger::FencedBlocker blockGuiInterruptions;
//	std::function<void()> func;
	{
		leveled_std_section::scoped_lock lock(cs_ThreadMessing);
		dms_assert(m_Status != task_status::suspended);
		if (!getUniqueLicenseToRun())
			return m_Status;
	}
	dms_assert(!SuspendTrigger::DidSuspend());

	safe_run();
	return m_Status;
}


task_status OperationContext::OnStart()
{
	DSM::CancelIfOutOfInterest(m_Result);
	{
		leveled_std_section::scoped_lock lock(cs_ThreadMessing);

		dms_assert(m_Status >= task_status::activated || m_Status == task_status::none);
		dms_assert(m_Status != task_status::running);
		if (m_Status < task_status::running)
		{
			if (m_Status == task_status::none)
			{
				s_NrRunningOperations++;
				m_Status = task_status::activated;
			}
			dms_assert(m_Status != task_status::running);
			dms_assert(IsRunningOperation(m_Status));
			m_Status = task_status::running;
			return m_Status;
		}
	}
	return Join();
}

void OperationContext::OnSuspend()
{
	leveled_std_section::scoped_lock lock(cs_ThreadMessing);

	dms_assert(m_Status >= task_status::activated);

	if (m_Status >= task_status::suspended)
		return;

	releaseRunCount(task_status::suspended);
}

void OperationContext::OnException() noexcept
{
	OnEnd(task_status::exception);
}

garbage_t OperationContext::onEnd(task_status status) noexcept
{
	if (m_Status > task_status::suspended)
		return {};

	dms_assert(status != task_status::exception || m_Result->WasFailed(FR_Data));

	return separateResources(status);
}

void OperationContext::OnEnd(task_status status) noexcept
{
	dms_assert(status > task_status::suspended);
//	dms_assert(m_Status >= task_status::running);
	dms_assert(!SuspendTrigger::DidSuspend() || status == task_status::cancelled);

	std::any garbage;
	{
		leveled_std_section::scoped_lock lock(cs_ThreadMessing);
		if (m_Status > task_status::suspended)
			return;

		dms_assert(status != task_status::exception || m_Result->WasFailed(FR_Data));

		garbage = separateResources(status);
	}
}

/*
DataControllerRef OperationContext::getArgDC(arg_index i) const
{
	SharedPtr<const FuncDC> funcDC = m_FuncDC.get_ptr();
	if (!funcDC)
		return {};

	dms_assert(i < funcDC->GetNrArgs());
	if (!MustCalcArg(i))
		return {};

	auto argDcListElem = funcDC->GetArgList();
	while (i--)
	{
		dms_assert(argDcListElem);
		argDcListElem = argDcListElem->m_Next;
	}
	dms_assert(argDcListElem);

	auto dcRef = argDcListElem->m_DC;
	dms_assert(dcRef);

	return dcRef;
}

arg_index OperationContext::getNrSupplOC() const
{
	SharedPtr<const FuncDC> funcDC = m_FuncDC.get_ptr();
	return (funcDC ? funcDC->GetNrArgs() : 0) + m_OtherSuppliers.size();
}

std::shared_ptr<OperationContext> OperationContext::getSupplOC(arg_index i) const
{
	dms_assert(i <= getNrSupplOC());

	SharedPtr<const FuncDC> funcDC = m_FuncDC.get_ptr();

	arg_index n = funcDC ? m_FuncDC->GetNrArgs() : 0;
	if (i < n)
	{
		dms_assert(funcDC);
		auto argDC = getArgDC(i);
		if (!argDC) // maybe MustCalcArg returned false
			return {};
		while (auto argFuncDC = dynamic_cast<const FuncDC*>(argDC.get_ptr()))
		{
			auto interestCount = argFuncDC->GetInterestCount();
			auto result = argFuncDC->GetOperContext();
			if (result)
				return result;

			if (!argFuncDC->IsSubItemCall())
			{
				const TreeItem* argResult = argDC->GetOld()->GetCurrRangeItem();
				dms_assert((!interestCount) || CheckDataReady(argResult) || argResult->WasFailed(FR_Data) || m_Status >= task_status::activated);
				return {};
			}
			argDC = argFuncDC->m_Args->m_DC;
		}
		return GetOperationContext(argDC->GetOld()->GetCurrRangeItem());
	}
	dms_assert(i < n + m_OtherSuppliers.size());
	auto si = m_OtherSuppliers[i - n];
	dms_assert(si);
	if (CheckDataReady(si->GetCurrRangeItem()))
		return {};
	//	si->PrepareDataUsage(DrlType::Certain);
	if (si->WasFailed(FR_Data))
	{
//		const_cast<FuncDC*>(m_FuncDC.get_ptr())->Fail(si.get_ptr());
		return {};
	}
	dms_assert(!SuspendTrigger::DidSuspend());
	auto result = GetOperationContext(si->GetCurrUltimateItem());
	dms_assert(result || CheckDataReady(si->GetCurrRangeItem()));
	return result;
}
*/
void OperationContext::releaseRunCount(task_status status)
{
	dms_assert(!IsRunningOperation(status));

	dms_assert(cs_ThreadMessing.isLocked());

	dms_assert(m_Status <= task_status::running);
	if (IsRunningOperation(m_Status))
	{
		dms_assert(s_NrRunningOperations > 0);
		--s_NrRunningOperations;
	}
	dms_assert(s_NrRunningOperations >= 0);
	m_Status = status;
	dms_assert(!IsRunningOperation(m_Status));

}

garbage_t OperationContext::separateResources(task_status status)
{
	dms_assert(cs_ThreadMessing.isLocked());

	dms_assert(status > task_status::suspended);
	if (m_Status > task_status::suspended)
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
	dms_assert(!m_ResKeeper);


	dms_assert(!m_WriteLock || status == task_status::cancelled || status == task_status::exception); // all other routes outside Schedule go through safe_run, which alwayws release the writeLock on completion
	m_WriteLock = ItemWriteLock();

	if (m_FuncDC)
	{
		MG_DEBUGCODE(if (m_FuncDC) md_FuncDC = m_FuncDC);
		releaseBin |= m_FuncDC->ResetOperContextImplAndStopSupplInterest();
	}
	dms_assert(!m_FuncDC);

	releaseBin |= disconnect();
//	releaseBin |= runOperationContexts();
	releaseBin |= make_any_scoped_exit( &RunOperationContexts ); // do this after releasing interests and related resources of suppliers
	cv_TaskCompleted.notify_all();

	return releaseBin;
}

bool OperationContext::CancelIfNoInterestOrForced(bool forced)
{
	if (!forced)
	{
//		if (cs_ThreadMessing.isLocked()) // isLockedFromThisThread ?
//			return false;

//		leveled_std_section::scoped_lock globalSectionLock(sg_CountSection);
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
	std::any separatedResources;
	leveled_std_section::scoped_lock lock(cs_ThreadMessing);

	if (m_Status == task_status::exception)
		return true;

	if (!item->WasFailed(FR_Data))
		return false;

	m_Result->Fail(item);
	dms_assert(m_Result->WasFailed(FR_Data));
	if (m_Status < task_status::exception)
	{
		separatedResources = separateResources(task_status::exception);
		dms_assert(!m_ResKeeper);
	}
	return true;
}

bool OperationContext::SetReadLock(std::vector<ItemReadLock>& locks, const TreeItem* si) 
{
	try {
		locks.emplace_back(si);
	}
	catch (const concurrency::task_canceled&)
	{
		CancelIfNoInterestOrForced(true);
		throw;
	}
	catch (...)
	{
		auto hasError = HandleFail(si);
		dms_assert(hasError);
		return false;
	}
	return true;
}

std::vector<ItemReadLock> OperationContext::SetReadLocks(const FutureSuppliers& allInterests)
{
	std::vector<ItemReadLock> locks; locks.reserve(allInterests.size());

//	dms_assert(m_FuncDC);
	for (const DataController* futureSupplier : allInterests)
	{
		dms_assert(futureSupplier);
		if (!futureSupplier)
			continue;

		auto supplierItem = futureSupplier->GetOld(); // can be reference to default unit
		dms_assert(supplierItem);
		dms_assert(!dynamic_cast<const FuncDC*>(futureSupplier)
			|| !dynamic_cast<const FuncDC*>(futureSupplier)->m_OperContext
			|| dynamic_cast<const FuncDC*>(futureSupplier)->m_OperContext->m_Status == task_status::none
		); // supplier operation was completed before this one starts
		dms_assert(supplierItem->GetCurrRefItem() == nullptr);
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

	dms_assert(cs_ThreadMessing.isLocked());
	bool connected = false;
	for (const auto& supplierInterest: allInterests)
	{
		if (!supplierInterest)
			continue;
		auto funcSupplier = dynamic_cast<const FuncDC*>(supplierInterest.get_ptr());
		std::shared_ptr<OperationContext> oc;
		if (funcSupplier)
			oc = funcSupplier->GetOperContext();
		else
			oc = GetOperationContext(supplierInterest->GetOld()->GetCurrRangeItem());

		if (!oc)
			continue;
		auto status = oc->getStatus();
		if (status > task_status::none && status <= task_status::suspended)
		{
			connect(shared_from_this(), oc);
			connected = true;
			dbg_assert(m_Suppliers.size());
		}
	}

#if defined(MG_DEBUG)
	dms_assert(sd_ManagedContexts.find(this) == sd_ManagedContexts.end()); 
	sd_ManagedContexts.insert(this);
#endif

	return connected;
}

task_status OperationContext::JoinSupplOrSuspendTrigger()
{
	auto suppliers = m_Suppliers;
	for (auto& oc: suppliers)
	{
		if (!oc)
			continue;

		const TreeItem* supplResult = oc->GetResult();
		dms_assert(supplResult); 
		dms_assert(oc->GetStatus() >= task_status::scheduled || CheckDataReady(supplResult) || supplResult->WasFailed(FR_Data) || !supplResult->GetInterestCount());
		task_status ocStatus = oc->Join();
		dms_assert(ocStatus > task_status::running);
		dms_assert(CheckDataReady(supplResult) || supplResult->WasFailed(FR_Data) || !supplResult->GetInterestCount() || SuspendTrigger::DidSuspend());
		switch (ocStatus)
		{
		case task_status::done:
			continue;
		case task_status::suspended:
			OnSuspend();
			break;
		case task_status::exception:
			HandleFail(oc->GetResult());
			break;
		case task_status::cancelled:
			dms_assert(m_Status == task_status::cancelled); // MUST HAVE BEEN ALL RESET AS ATOMIC OPERATION
			break;
		}
		return m_Status;
	}
	return task_status::done;
}

bool OperationContext_CreateResult(OperationContext* oc, const FuncDC* funcDC) // TODO G8.1: Verwijderen uit OperationContext
{
	DBG_START("OperationContext", "CreateResult", MG_DEBUG_FUNCCONTEXT);
	DBG_TRACE(("FuncDC: %s", funcDC->md_sKeyExpr));

	dms_assert(IsMainThread());
	dms_assert(funcDC);

	MG_DEBUGCODE(const TreeItem* oldItem = funcDC->GetOld());

	SuspendTrigger::FencedBlocker lockSuspend;

	OperatorContextHandle operContext(oc->m_OperGroup->GetNameID(), false, funcDC);

	dms_assert(!funcDC->WasFailed(FR_MetaInfo));
	dms_assert(!SuspendTrigger::DidSuspend());

	TreeItemDualRef& resultHolder = *const_cast<FuncDC*>(funcDC);
	try {
		OArgRefs args = funcDC->GetArgs(true, false); // TODO, OPTIMIZE: CreateResult also sometimes calls GetArgs(false).
		dms_assert(!SuspendTrigger::DidSuspend());
		if (!args)
		{
			dms_assert(funcDC->WasFailed(FR_MetaInfo));
			return false;
		}
		oc->m_Oper = funcDC->GetCurrOperator();
		if (!oc->m_Oper) {
			oc->m_Oper = oc->m_OperGroup->FindOperByArgs(*args);
			if (funcDC)
				funcDC->SetOperator(oc->m_Oper);
		}
		dms_assert(oc->m_Oper);

		dms_assert(!funcDC->WasFailed(FR_MetaInfo));
		dms_assert(!SuspendTrigger::DidSuspend());

		oc->m_Oper->CreateResultCaller(resultHolder, *args, oc, funcDC->GetLispRef().Right());
	}
	catch (...)
	{
		if (resultHolder.IsNew())
			resultHolder->CatchFail(FR_MetaInfo); // also calls resultHolder->StopSupplierInterest() (the resulting data).
		resultHolder.CatchFail(FR_MetaInfo);
	}

	bool resultingFlag = !resultHolder.WasFailed(FR_MetaInfo);

	if (resultHolder)
	{
		if (!resultHolder->GetDynamicObjClass()->IsDerivedFrom(oc->m_Oper->GetResultClass()))
		{
			auto msg = mySSPrintF("result of %s is of type %s, expected type: %s"
				, oc->m_Oper->GetGroup()->GetName()
				, resultHolder->GetCurrentObjClass()->GetName()
				, oc->m_Oper->GetResultClass()->GetName()
			);
			resultHolder->Fail(msg, FR_MetaInfo);
		}
		if (resultHolder->WasFailed(FR_MetaInfo))
		{
			resultHolder.Fail(resultHolder.GetOld(), FR_MetaInfo);
			resultingFlag = false;
		}
	}
		
	dms_assert(resultingFlag != (SuspendTrigger::DidSuspend() || resultHolder.WasFailed()));
	return resultingFlag;
}


void OperationContext_AssignResult(OperationContext* oc, const FuncDC* funcDC)
{
	if (funcDC->IsNew())
	{
		// mark TimeStamp of result
		TreeItem* cacheRoot = funcDC->GetNew();
		TimeStamp ts = funcDC->GetLastChangeTS();
		for (TreeItem* cacheItem = cacheRoot; cacheItem; cacheItem = cacheRoot->WalkCurrSubTree(cacheItem))
			cacheItem->MarkTS(ts);
		dms_assert(!oc->m_Result);
		oc->m_Result = funcDC->GetNew();
		dms_assert(oc->m_Result == funcDC->GetNew());
	}
	else if (!funcDC->IsTmp())
	{
		oc->m_Result = funcDC->GetOld();
	}
	dms_assert(!funcDC->IsNew() || funcDC->GetNew()->m_LastChangeTS == funcDC->m_LastChangeTS); // further changes in the resulting data must have caused resultHolder to invalidate, as IsNew results are passive
}

void OperationContext::AddDependency(const DataController* dcRef)
{
	dms_assert(!IsScheduled());
	dms_assert(IsMainThread());
	dms_assert(dcRef);
	m_OtherSuppliers.emplace_back(dcRef);
}

bool OperationContext::MustCalcArg(arg_index i, CharPtr firstArgValue) const
{
	return m_FuncDC->MustCalcArg(i, true, firstArgValue);
}

void OperationContext::safe_run_impl() noexcept
{
	try {
		OperationContext::CancelableFrame frame(this);

		if (OnStart() == task_status::running) {
			UpdateMarker::ChangeSourceLock tsLock(m_ActiveTimestamp, "safe_run");
			dms_assert(m_TaskFunc);
			m_TaskFunc(m_Context);
		}
	}
	catch (const concurrency::task_canceled&)
	{
		dms_assert(getStatus() == task_status::cancelled); // clean-up was done ?
	}
	catch (...) {
		GetResult()->CatchFail(FR_Data);
	}
	m_TaskFunc = {};
	auto localWriteLock = std::move(m_WriteLock);
	dms_assert(!m_WriteLock);
	dms_assert(!localWriteLock || localWriteLock.GetItem() == GetResult());
	// writeLock release here before OnEnd allows Waiters to start
}

void OperationContext::safe_run() noexcept
{
	dms_assert(!SuspendTrigger::DidSuspend());

	safe_run_impl();

	if (GetResult()->WasFailed(FR_Data))
		OnException(); // clean-up
	else
	{
		if (SuspendTrigger::DidSuspend())
		{
			dms_assert(m_Status == task_status::suspended);
			return;
		}
		OnEnd(task_status::done); // just set status to done and clean-up
	}

	// check that clean-up was done. This includes releasing the RunCount
	dms_assert(!m_TaskFunc);
	dms_assert(!m_WriteLock);
}

bool OperationContext::ScheduleCalcResult(Explain::Context* context, ArgRefs&& argRefs)
{
	DBG_START("OperationContext", "CalcResult", MG_DEBUG_FUNCCONTEXT);
	DBG_TRACE(("FuncDC: %s", m_FuncDC->md_sKeyExpr));

	dms_assert(m_Oper);
	dms_assert(!SuspendTrigger::DidSuspend());

	OperatorContextHandle operContext(m_OperGroup->GetNameID(), true, m_FuncDC);

	TreeItemDualRef& resultHolder = const_cast<FuncDC&>(m_FuncDC.get_ref());
	dms_assert(resultHolder);

	MG_DEBUGCODE(const TreeItem* oldItem = resultHolder.GetOld() );
	MG_DEBUGCODE(auto oper = m_Oper);
	dbg_assert(oldItem);

	dms_assert(resultHolder.IsTmp() || resultHolder->GetInterestCount());
	dms_assert(resultHolder.GetInterestCount());
	dbg_assert(!resultHolder.IsTmp()); // DEBUG 19/03/2020

	bool doASync = m_Oper->CanRunParallel() && resultHolder.DoesHaveSupplInterest() && !context;

	dms_assert(m_Result);
//	bool dataReady = CheckDataReady(m_Result);
//	bool dataReady = CheckAllSubDataReady(m_Result);
//	dms_assert(context || !dataReady); future XXX

	if (m_Result->WasFailed(FR_Data))
	{
		OnException();
		return false;
	}

	dms_assert(IsMainThread() || doASync);
	dms_assert(IsMainThread()); // ???
	if (!resultHolder.DoesHaveSupplInterest())
	{
		dms_assert(!doASync);
		Actor::UpdateLock lock(&resultHolder, actor_flag_set::AF_ChangingInterest); // DEBUG, 19-03-2020
		resultHolder.StartSupplInterest();
	}

	FutureSuppliers allInterests;

	allInterests.reserve(argRefs.size() + m_OtherSuppliers.size()); // TODO G8: count better
	for (const auto& argRef : argRefs)
		if (argRef.index() == 0)
		{
			allInterests.emplace_back(std::get<FutureData>(argRef));
			dms_assert(allInterests.back());
		}
	for (const DataController* dcPtr : m_OtherSuppliers)
	{
		auto otherSupplier = dcPtr->CalcResult();
		if (!otherSupplier)
		{
			if (dcPtr->WasFailed(FR_Data))
			{
				resultHolder.Fail(dcPtr);
				break;
			}
			dms_assert(SuspendTrigger::DidSuspend());
			break;
		}
		allInterests.emplace_back(otherSupplier);
	}
	task_status resultStatus = m_Status;
	if (!resultHolder.WasFailed(FR_Data) && !SuspendTrigger::DidSuspend())
	{
		struct Func {
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
						, funcDC ? AsFLispSharedStr(funcDC->GetLispRef()).c_str() : "(null)"
					);
#endif //defined(MG_DEBUG_OPERATIONS)

				if (!funcDC) return; // TODO G8: Debug why.

				dms_assert(funcDC && (funcDC->DoesHaveSupplInterest() || !funcDC->GetInterestCount() || context));

				dms_assert(self->m_Status == task_status::running || self->m_Status == task_status::suspended);
				dms_assert(!SuspendTrigger::DidSuspend());

				auto readLocks = self->SetReadLocks(allInterests); // TODO: move this into CalcResult, replace argRefs
				allInterests.clear();
				self->RunOperator(context, argRefs); // RunImpl() may destroy this and make m_FuncDC inaccessible // CONTEXT

				// forward FR_Validate and FR_Committed failures
				for (const auto& argRef : argRefs)
				{
					auto statusActor = GetStatusActor(argRef);
					if (statusActor && statusActor->WasFailed())
						funcDC->Fail(statusActor);
				}

				argRefs.clear();
			}
		};
//		auto func = Func{ this, std::move(argRefs), std::move(allInterests) };
		auto func = Func{ this, std::move(argRefs), allInterests };

		task_status resultStatus = ScheduleItemWriter(MG_SOURCE_INFO_CODE("OperationContext::CalcResult") m_FuncDC->IsNew() ? m_FuncDC->GetNew() : nullptr
			,	func
			,	allInterests
			,	!doASync, context
		);
		dms_assert(!argRefs.size()); // moved in ?

		dms_assert(!SuspendTrigger::DidSuspend() || !doASync || !IsMultiThreaded2());

		dbg_assert(!resultHolder || resultHolder->GetDynamicObjClass()->IsDerivedFrom(oper->GetResultClass()));
		MG_DEBUGCODE(dms_assert(resultHolder.GetOld() == oldItem));

		if (resultHolder && resultHolder->WasFailed())
			resultHolder.Fail(resultHolder.GetOld());
	}
	if (resultHolder.WasFailed(FR_Data))
	{
		m_Result->Fail(resultHolder);
		dms_assert(m_Status >= task_status::running);
		OnException();
		resultStatus = task_status::exception;
	}
	else if (SuspendTrigger::DidSuspend())
	{
//		dms_assert(!doASync);
		return false;
	}
	return resultStatus != task_status::suspended && resultStatus != task_status::exception;
}

bool RunQueuedTaskInline()
{
	return false; // TODO
}

SizeT getScheduledContextsPos(OperationContextSPtr self)
{
	auto pos = s_ScheduledContexts.begin(), end = s_ScheduledContexts.end();
	for (; pos != end; ++pos)
		if (pos->lock() == self)
			return pos - s_ScheduledContexts.begin();
	return UNDEFINED_VALUE(SizeT);
}


void prioritize(SupplierSet& prioritizedContexts, OperationContextSPtr self)
{
	dms_assert(self);
	if (self->getStatus() != task_status::scheduled)
		return;

	auto ptr = prioritizedContexts.lower_bound(self);
	if (ptr != prioritizedContexts.end() && !ptr->owner_before(self))
		return;
	prioritizedContexts.insert(ptr, self);
	if (self->m_Suppliers.empty())
	{
		auto pos = getScheduledContextsPos(self);
		dms_assert(IsDefined(pos));

		if(IsDefined(pos))
			s_ScheduledContexts.erase(s_ScheduledContexts.begin() + pos);
		s_ScheduledContexts.push_front(self->weak_from_this());
		return;
	}

	for (auto& s : self->m_Suppliers)
		prioritize(prioritizedContexts, s);
}

task_status OperationContext::Join()
{
	if (OperationContext::CancelableFrame::CurrActiveHasRunCount())
		reportF(SeverityTypeID::ST_Warning, "OperationContext(%s)::Join called from Active Context %s"
			, GetResult()->GetFullName()
			, OperationContext::CancelableFrame::CurrActive()->GetResult()->GetFullName()
		);

	bool isFirstTime = true;
//	TryActivateTaskInline();
	while (m_Status <= task_status::running)
	{
		using DecCountType = StaticMtDecrementalLock<decltype(s_NrRunningOperations), s_NrRunningOperations>;
		std::optional<DecCountType> freediecount;
		if (OperationContext::CancelableFrame::CurrActiveHasRunCount())
			freediecount.emplace();

		if (!isFirstTime)
			RunOperationContexts(); // OPTIMIZE, CONSISTENCY: Some tasks finished without calling this. Find out why and avoid wasting idle thread time; maybe all threads are waiting in a Join without new and current tasks being activated

		leveled_std_section::unique_lock lock(cs_ThreadMessing);
		if (m_Status > task_status::running)
			break;

		dms_assert(m_Status > task_status::scheduled || !m_Suppliers.empty() || IsDefined(getScheduledContextsPos(this->shared_from_this())));

		if (isFirstTime && IsMainThread())
		{
			SupplierSet prioritizedContexts;
			prioritize(prioritizedContexts, this->shared_from_this());
		}
		cv_TaskCompleted.wait_for(lock.m_BaseLock, std::chrono::milliseconds(200), [this]() { return m_Status > task_status::running; });
		isFirstTime = false;
	}

	RunOperationContexts();

	dms_assert(m_Status > task_status::running);
	dbg_assert(CheckDataReady(m_Result) || m_Status == task_status::cancelled || m_Status == task_status::exception || !m_Result->GetInterestCount());
	return m_Status;
}

void OperationContext::RunOperator(Explain::Context* context, const ArgRefs& argRefs)
{
	SharedPtr<const FuncDC> funcDC = m_FuncDC.get_ptr();
	if (!funcDC || funcDC->WasFailed(FR_Data))
		return;

	if (m_Result->WasFailed(FR_Data))
	{
		funcDC->Fail(m_Result.get_ptr());
		return;
	}
	TreeItemDualRef& resultHolder = const_cast<FuncDC&>(funcDC.get_ref());
	dms_assert(resultHolder);
	bool actualResult = false;
	if (!CancelIfNoInterestOrForced(false))
	{
		bool newResult = false;
		try {
			if (funcDC->WasFailed(FR_MetaInfo))
				return;

			actualResult = m_Oper->CalcResult(resultHolder, argRefs, this, context); // ============== payload

			dms_assert(resultHolder || IsCanceled());
			dms_assert(actualResult || SuspendTrigger::DidSuspend());
			dms_assert(!IsDataItem(resultHolder.GetOld()) || AsDataItem(resultHolder.GetOld())->m_DataObject
				|| !actualResult
				|| CheckCalculatingOrReady(resultHolder.GetOld())
				|| resultHolder->WasFailed(FR_Data)
			);
		}
		catch (const concurrency::task_canceled&)
		{
			dms_assert(m_Status == task_status::cancelled || m_Status == task_status::exception);
		}
		catch (...)
		{
			resultHolder.CatchFail(FR_Data); // Now done by TreeItemDualRef::DoFail
			HandleFail(resultHolder);
		}
		dms_assert(!resultHolder.IsNew() || resultHolder->m_LastChangeTS == resultHolder.m_LastChangeTS); // further changes in the resulting data must have caused resultHolder to invalidate, as IsNew results are passive
		if (actualResult)
		{
#if defined(MG_DEBUG)
			const TreeItem* ri = resultHolder.IsOld() ? resultHolder->GetCurrUltimateItem() : resultHolder.GetNew();
			dms_assert(ri);
			dms_assert(CheckCalculatingOrReady(ri) || resultHolder->WasFailed(FR_Data));
//			dms_assert(CheckDataReady(ri) || resultHolder->WasFailed(FR_Data));
#endif
		}
	}
}
