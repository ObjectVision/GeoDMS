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


bool WaitForCompletedTaskOrTimeout(std::chrono::milliseconds waitFor = std::chrono::milliseconds(300));

using dms_task = concurrency::task<void>;
inline bool is_empty(const dms_task& x) { return x == dms_task();  }

template<typename Func>
auto start_dms_task(Func&& f)
{
	return dms_task(std::forward<Func>(f));
}

template <typename Func>
auto start_dms_task_with_context(Func&& f)
{
	return dms_task(CreateTaskWithContext(std::forward<Func>(f)));
}


enum class task_status {
	none, scheduled, activated, running, 
	suspended, cancelled, exception, done
};
inline bool IsRunningOperation(task_status s) { return s >= task_status::activated && s <= task_status::running; }

using FutureResultData = SharedTreeItemInterestPtr;

using OperationContextSPtr = std::shared_ptr<OperationContext>;
using OperationContextWPtr = std::weak_ptr<OperationContext>;
using SupplierSet = std::set<OperationContextSPtr, std::owner_less<OperationContextSPtr>>;
using WaiterSet = std::set<OperationContextWPtr, std::owner_less<OperationContextWPtr>>;

struct OperationContext : std::enable_shared_from_this<OperationContext>
{
	struct CancelableFrame {
		TIC_CALL CancelableFrame(OperationContext* self);
		TIC_CALL ~CancelableFrame();
		TIC_CALL static OperationContext* CurrActive();
		TIC_CALL static void CurrActiveCancelIfNoInterestOrForced(bool forceCancel);
		TIC_CALL static bool CurrActiveCanceled();
		static bool CurrActiveHasRunCount()
		{
			auto ca = CurrActive();
			if (!ca)
				return false;
			auto status = ca->getStatus();
			return IsRunningOperation(status);
		}
	private:
		OperationContext* m_Prev;
	};

	TIC_CALL OperationContext();
	TIC_CALL OperationContext(const FuncDC* self, const AbstrOperGroup* og);
	TIC_CALL ~OperationContext();

	OperationContext(const OperationContext&) = delete;
	void operator =(const OperationContext&) = delete;

	TIC_CALL task_status Join();

	template <typename Func>
	task_status ScheduleItemWriter(MG_SOURCE_INFO_DECL TreeItem* item, Func&& func, const FutureSuppliers& allInterest, bool runDirect, Explain::Context* context);

	TIC_CALL task_status Schedule(TreeItem* item, const FutureSuppliers& allArgInterest, bool runDirect);

	TIC_CALL dms_task GetTask() const;
	TIC_CALL task_status OnStart();
	TIC_CALL void OnSuspend();
	TIC_CALL void OnException() noexcept;
	TIC_CALL void OnEnd(task_status status) noexcept;
	garbage_t onEnd(task_status status) noexcept;

	TIC_CALL bool CancelIfNoInterestOrForced(bool forced);
	bool HandleFail(const TreeItem* item);

	bool IsScheduled() const { task_status status = m_Status; return status > task_status::none && status != task_status::suspended; }
	bool IsDone() const { return m_Status >= task_status::cancelled; }
	bool IsCanceled() const { return m_Status == task_status::cancelled; }
	SharedPtr<const TreeItem> GetResult() const { return m_Result; }
	task_status GetStatus() const;
	task_status getStatus() const { return m_Status; }

	TIC_CALL task_status JoinSupplOrSuspendTrigger();
	void ActivateOtherSuppl();

	friend garbage_t runOperationContexts();
//private:
	bool getUniqueLicenseToRun();
	task_status TryActivateTaskInline();
	void setTask(dms_task&& t);
	void activateTaskImpl(SharedActorInterestPtr&& resKeeper);
	void releaseRunCount(task_status status);
	garbage_t separateResources(task_status status);

	bool connectArgs(const FutureSuppliers& allInterests);

	garbage_t disconnect();
	garbage_t disconnect_supplier(OperationContext* supplier);

	FutureResultData m_Result;
	SharedActorInterestPtr m_ResKeeper;
	std::atomic<task_status> m_Status = task_status::none;

	void safe_run_impl() noexcept;
	void safe_run_impl2() noexcept;
	void safe_run() noexcept;
public:
	std::function<void(Explain::Context*)> m_TaskFunc;
	Explain::Context*               m_Context = nullptr;
	ItemWriteLock                   m_WriteLock;
	TimeStamp                       m_ActiveTimestamp = -1;

private:
	dms_task m_Task;

public:
	std::vector<ItemReadLock> SetReadLocks(const FutureSuppliers& allInterests);
	bool SetReadLock(std::vector<ItemReadLock>& locks, const TreeItem* si);

//	bool CreateResult();
	bool ScheduleCalcResult(Explain::Context* context, ArgRefs&& argInterest);

	bool MustCalcArg(arg_index i, CharPtr firstArgValue) const;

	TIC_CALL void AddDependency(const DataController* keyExpr);
	void RunOperator(Explain::Context* context, ArgRefs allInterests, std::vector<ItemReadLock> readLocks);

	const FuncDC* GetFuncDC() const { return m_FuncDC;  }

// private: TODO G8.5
//	DataControllerRef getArgDC(arg_index i) const;
//	arg_index getNrSupplOC() const;
//	std::shared_ptr<OperationContext> getSupplOC(arg_index i) const;

	std::vector<DataControllerRef> m_OtherSuppliers;

	WeakPtr<const FuncDC>         m_FuncDC; MG_DEBUGCODE(WeakPtr<const FuncDC> md_FuncDC; )
	WeakPtr<const AbstrOperGroup> m_OperGroup;
	WeakPtr<const Operator>       m_Oper;
public:
	std::any                      m_MetaInfo;

	SupplierSet m_Suppliers;
	WaiterSet   m_Waiters;
};

// TODO G8.1: Verwijderen uit OperationContext.cpp
bool OperationContext_CreateResult(OperationContext* oc, const FuncDC* funcDC); 
void OperationContext_AssignResult(OperationContext* oc, const FuncDC* funcDC);

#endif //!defined(__TIC_OPERATIONCONTEXT_H)
