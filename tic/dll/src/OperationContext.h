#pragma once

#if !defined(__TIC_OPERATIONCONTEXT_H)
#define __TIC_OPERATIONCONTEXT_H

#include "act/any.h"
#include "act/MainThread.h"
#include "dbg/DebugContext.h"

#include "ItemLocks.h"
#include "MoreDataControllers.h"

#include <ppltasks.h>
#include <optional>


struct tg_maintainer
{
	TIC_CALL tg_maintainer();
	TIC_CALL ~tg_maintainer();
};

struct CancelableFrame {
	TIC_CALL CancelableFrame(OperationContext* self);
	TIC_CALL ~CancelableFrame();
	TIC_CALL static OperationContext* CurrActive();
	TIC_CALL static void CurrActiveCancelIfNoInterestOrForced(bool forceCancel);
	TIC_CALL static bool CurrActiveCanceled();
private:
	OperationContext* m_Prev;
};

void WaitForCompletedTaskOrTimeout();

using dms_task = concurrency::task<void>;
inline bool is_empty(const dms_task& x) { return x == dms_task();  }


enum class task_status {
	none, waiting_for_suppliers, scheduled, activated, running, 
	suspended, cancelled, exception, done
};

using FutureResultData = SharedTreeItemInterestPtr;

using OperationContextSPtr = std::shared_ptr<OperationContext>;
using OperationContextWPtr = std::weak_ptr<OperationContext>;
using SupplierSet = std::set<OperationContextSPtr, std::owner_less<OperationContextSPtr>>;
using WaiterSet = std::set<OperationContextWPtr, std::owner_less<OperationContextWPtr>>;

using task_func_signature = void(OperationContext* self, Explain::Context*);
using task_func_type = std::function<task_func_signature>;

struct OperationContext : std::enable_shared_from_this<OperationContext>
{
	static std::shared_ptr<OperationContext> CreateFuncDC(const FuncDC* self)
	{
		return std::make_shared<OperationContext>(self);
	}

	static std::shared_ptr<OperationContext> CreateItemWriter(TreeItem* item, task_func_type func, const FutureSuppliers& allArgInterests, bool runDirect, Explain::Context* context)
	{
		auto result = std::make_shared<OperationContext>(func);
		result->Schedule(item, allArgInterests, runDirect, context); // might run inline
		return result;
	}

	TIC_CALL ~OperationContext();

//private:
	// ctor / dtor
	TIC_CALL OperationContext(const FuncDC* self);
	TIC_CALL OperationContext(task_func_type func);

	OperationContext(const OperationContext&) = delete;
	void operator =(const OperationContext&) = delete;

	bool ScheduleCalcResult(ArgRefs&& argInterest, Explain::Context* context);

	TIC_CALL task_status Schedule(TreeItem* item, const FutureSuppliers& allArgInterest, bool runDirect, Explain::Context* context);

public:
	// main interface functions
	TIC_CALL task_status Join();

	TIC_CALL bool GetUniqueLicenseToRun(phase_number targetPhaseNumber);
	TIC_CALL bool getUniqueLicenseToRun(phase_number targetPhaseNumber);
	//REMOVE TIC_CALL void OnSuspend();
	TIC_CALL void OnException() noexcept;
	TIC_CALL void OnEnd(task_status status) noexcept;
	garbage_t onEnd(task_status status) noexcept;

	TIC_CALL bool CancelIfNoInterestOrForced(bool forced);
	bool HandleFail(const TreeItem* item);

	bool IsCanceled() const { return m_Status == task_status::cancelled; }
	SharedPtr<const TreeItem> GetResult() const { return m_Result; }
	task_status GetStatus() const;
	task_status getStatus() const { return m_Status.load(std::memory_order_relaxed); }

	TIC_CALL task_status JoinSupplOrSuspendTrigger();
	void ActivateOtherSuppl();

//private:
	bool TryRunningTaskInline(phase_number targetPhaseNumber);
	bool collectTaskImpl();
	void releaseRunCount(task_status status);
	garbage_t separateResources(task_status status);

	bool connectArgs(const FutureSuppliers& allInterests);

	garbage_t disconnect_waiters();
	garbage_t disconnect_supplier(OperationContext* supplier);

	FutureResultData m_Result;
	SharedActorInterestPtr m_ResKeeper;
	std::atomic<task_status> m_Status = task_status::none;

	void Run_with_catch() noexcept;
	void Run_with_cleanup() noexcept;
	void Run_Caller() noexcept;
public:
	task_func_type     m_TaskFunc;
	Explain::Context*  m_Context = nullptr;
	ItemWriteLock      m_WriteLock;
	TimeStamp          m_ActiveTimestamp = -1;

public:
	phase_number m_PhaseNumber = 0;
	std::vector<ItemReadLock> SetReadLocks(const FutureSuppliers& allInterests);
	bool SetReadLock(std::vector<ItemReadLock>& locks, const TreeItem* si);

	bool MustCalcArg(arg_index i, CharPtr firstArgValue) const;

	void RunOperator(Explain::Context* context, ArgRefs allInterests, std::vector<ItemReadLock> readLocks);

	const FuncDC* GetFuncDC() const { return m_FuncDC;  }


	const Operator*       GetOperator () const { assert(m_FuncDC); return m_FuncDC->m_Operator; }
	const AbstrOperGroup* GetOperGroup() const { assert(m_FuncDC); return m_FuncDC->m_OperatorGroup; }

public:

	WeakPtr<const FuncDC> m_FuncDC;
	SupplierSet           m_Suppliers;
	WaiterSet             m_Waiters;
};

TIC_CALL auto GetNextPhaseNumber() -> phase_number;
TIC_CALL void DoWorkWhileWaitingFor(phase_number maxPhaseNumber, task_status* phaseContainerStatus);


TIC_CALL void WakeUpJoiners();
TIC_CALL extern phase_number s_CurrBlockedPhaseNumber;
TIC_CALL extern const TreeItem* s_CurrBlockedPhaseItem;
TIC_CALL extern const TreeItem* s_CurrPhaseContainer;

#endif //!defined(__TIC_OPERATIONCONTEXT_H)
