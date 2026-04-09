// Copyright (C) 1998-2025 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
File: OperationContext.h
Purpose:
- Asynchronous operation orchestration for TreeItem computations and writes.

Summary:
- Encapsulates a unit of work (OperationContext) that produces/updates a TreeItem.
- Manages supplier/waiter dependency graphs, scheduling, activation, and execution.
- Enforces unique execution (license) per item; acquires read/write locks as needed.
- Supports cancellation, suspension, exception-safe completion, and diagnostics (explain).
- Integrates with PPL tasks (concurrency::task) for async composition and chaining.

Thread-safety:
- State transitions coordinated via atomics; dependency relations rely on locking in used primitives.
- Scoped helpers (tg_maintainer, CancelableFrame) manage thread-global and thread-local execution state.

Lifecycle (typical):
1) Create (CreateFuncDC/CreateItemWriter).
2) Schedule: register suppliers, set up locks, possibly run inline if policy allows.
3) Await: Join/JoinSupplOrSuspendTrigger while optionally doing other work.
4) Complete: transition to done/cancelled/exception via OnEnd; notify waiters and release resources.

Key types:
- OperationContext: orchestration object and state machine.
- CancelableFrame: current active context guard enabling cooperative cancellation queries.
- tg_maintainer: per-thread initialization/teardown while executing tasks.
- dms_task: task alias used to compose dependent async work.
- explain_context_ptr_t: captures "why/how" context for diagnostics.

Diagnostics:
- Phase numbering and blocked-phase globals aid UI/debugger insight into stalled operations.
*/

#if !defined(__TIC_OPERATIONCONTEXT_H)
#define __TIC_OPERATIONCONTEXT_H

#include "act/garbage_can.h"
#include "act/MainThread.h"
#include "dbg/DebugContext.h"

#include "ItemLocks.h"
#include "MoreDataControllers.h"

#if defined(_MSC_VER)
#include <ppltasks.h>
#endif
#include <optional>

// tg_maintainer
// RAII helper to install/uninstall thread-global state required for task execution.
// Exact responsibilities are defined in its implementation (e.g., init telemetry or TLS).
struct tg_maintainer
{
	TIC_CALL tg_maintainer();
	TIC_CALL ~tg_maintainer();
};

// CancelableFrame
// Scoped "active operation" guard. Tracks currently executing OperationContext.
// - On construction: registers 'self' as the current active context.
// - On destruction: restores previous active context.
// - CurrActive*: access/modify cancellation of the current context from nested code.
struct CancelableFrame {
	TIC_CALL CancelableFrame(OperationContext* self);
	TIC_CALL ~CancelableFrame();
	TIC_CALL static OperationContext* CurrActive();
	TIC_CALL static void CurrActiveCancelIfNoInterestOrForced(bool forceCancel);
	TIC_CALL static bool CurrActiveCanceled();
private:
	OperationContext* m_Prev; // previously active OperationContext (for nesting)
};

// dms_task
// Alias for a PPL task producing no direct value; used to chain async work units.
#if defined(_MSC_VER)
using dms_task = concurrency::task<void>;
inline bool is_empty(const dms_task& x) { return x == dms_task();  }
#else
// Linux placeholder: PPL not available, tasks run synchronously
struct dms_task {};
inline bool is_empty(const dms_task&) { return true; }
#endif

// explain_context_ptr_t
// Shared pointer to an explanation context used to capture "why/how" results are produced.
using explain_context_ptr_t = std::shared_ptr<Explain::Context>;

// task_status
// State machine for an OperationContext:
// - none:         not scheduled
// - waiting_for_suppliers: waiting on prerequisite operations to finish
// - scheduled:    scheduled to run (queued)
// - activated:    accepted for running (license acquired)
// - running:      actively executing task function
// - suspended:    temporarily suspended (awaiting external trigger)
// - cancelled:    explicitly cancelled (no longer will run)
// - exception:    faulted during execution
// - done:         completed successfully
enum class task_status {
	none, waiting_for_suppliers, scheduled, activated, running,
	suspended, cancelled, exception, done
};

// FutureResultData
// Interest holder to keep the resulting TreeItem alive and signal readiness.
using FutureResultData = SharedTreeItemInterestPtr;

// Pointer and relation sets used to model dependency graph of operations.
// - Suppliers: contexts providing inputs to this context.
// - Waiters:   contexts depending on this context (reverse edges).
using OperationContextSPtr = std::shared_ptr<OperationContext>;
using OperationContextWPtr = std::weak_ptr<OperationContext>;
using SupplierSet = std::set<OperationContextSPtr, std::owner_less<OperationContextSPtr>>;
using WaiterSet = std::set<OperationContextWPtr, std::owner_less<OperationContextWPtr>>;

// Task function signature bound to an OperationContext and optional explain context.
using task_func_signature = void(OperationContext* self, explain_context_ptr_t context);
using task_func_type = std::function<task_func_signature>;

// OperationContext
// Central orchestration object representing a single unit of work (operator evaluation,
// item write, etc.). Manages:
// - Scheduling and activation policies.
// - Concurrency/licensing (unique execution license per item).
// - Supplier/waiter dependencies and joining.
// - Cancellation and suspension.
// - Result lifetime via interests/locks.
// Thread-safety:
// - State transitions guarded via atomics and the locking primitives used by dependencies.
// Ownership:
// - Always managed via shared_ptr; use enable_shared_from_this for safe self references.
struct OperationContext : std::enable_shared_from_this<OperationContext>
{
	// Factory: create an OperationContext bound to a FuncDC (functional data controller).
	static std::shared_ptr<OperationContext> CreateFuncDC(const FuncDC* self)
	{
		return std::make_shared<OperationContext>(self);
	}

	// Factory: create a writer task for an item and schedule it (may run inline if allowed).
	// - item:           target TreeItem to write/update.
	// - func:           task body to execute.
	// - allArgInterests:suppliers this task depends on.
	// - runDirect:      if true, may execute on the caller thread if licensing allows.
	static std::shared_ptr<OperationContext> CreateItemWriter(TreeItem* item, task_func_type func, const FutureSuppliers& allArgInterests, bool runDirect)
	{
		auto result = std::make_shared<OperationContext>(func);
		result->Schedule(item, allArgInterests, runDirect); // might run inline
		return result;
	}

	TIC_CALL ~OperationContext();

	// Constructors
	// - FuncDC-bound constructor used for operator evaluations via data controllers.
	TIC_CALL OperationContext(const FuncDC* self);
	// - Task-func constructor used for ad-hoc writer contexts and other runnable work.
	TIC_CALL OperationContext(task_func_type func);

	OperationContext(const OperationContext&) = delete;
	void operator =(const OperationContext&) = delete;

	// ScheduleCalcResult
	// Connect arguments and schedule calculation of a result; returns true if scheduled.
	// May set up explain context for diagnostics.
	bool ScheduleCalcResult(ArgRefs&& argRefs, explain_context_ptr_t context);

	// Schedule
	// Primary scheduling entry. Registers suppliers, acquires write lock if needed,
	// and optionally runs inline based on 'runDirect'.
	// Returns final state after scheduling (may be running/done if inlined).
	TIC_CALL task_status Schedule(TreeItem* item, const FutureSuppliers& allArgInterest, bool runDirect, explain_context_ptr_t context = {});

public:
	// Join
	// Wait until this context reaches a terminal state (done/cancelled/exception/suspended).
	TIC_CALL task_status Join();

	// GetUniqueLicenseToRun
	// Acquire exclusive execution license to ensure only one active runner per context.
	TIC_CALL bool GetUniqueLicenseToRun();
	// getUniqueLicenseToRun (overload)
	// If runDirect is true, try to claim license immediately for inlining.
	TIC_CALL bool getUniqueLicenseToRun(bool runDirect);

	// OnException
	// Transition to exception state and perform minimal safe cleanup (no-throw).
	TIC_CALL void OnException() noexcept;

	// OnEnd
	// Transition to final status and notify waiters; no-throw.
	TIC_CALL void OnEnd(task_status status) noexcept;

	// onEnd
	// Variant returning a garbage_can that defers cleanup to caller scope.
	garbage_can onEnd(task_status status) noexcept;

	// CancelIfNoInterestOrForced
	// Cancel execution if there are no waiters/interests or if 'forced' is true.
	TIC_CALL bool CancelIfNoInterestOrForced(bool forced);

	// HandleFail
	// Handle failure for a particular item; returns true if handled (and error propagated appropriately).
	bool HandleFail(const TreeItem* item);

	// IsCanceled
	// Query helper for quick cancellation checks.
	bool IsCanceled() const { return m_Status == task_status::cancelled; }

	// GetResult
	// Return the result TreeItem (shared interest), if any.
	SharedPtr<const TreeItem> GetResult() const { return m_Result; }

	// GetStatus
	// Synchronizing accessor to current status (may include acquire semantics in impl).
	TIC_CALL task_status GetStatus() const;

	// getStatus
	// Relaxed load of current status for non-synchronizing checks.
	task_status getStatus() const { return m_Status.load(std::memory_order_relaxed); }

	// JoinSupplOrSuspendTrigger
	// Either waits for suppliers to complete or transitions to suspended based on triggers.
	TIC_CALL task_status JoinSupplOrSuspendTrigger();

	// ActivateOtherSuppl
	// Wake up other suppliers that may proceed after state changes in this context.
	void ActivateOtherSuppl();

	// TryRunningTaskInline
	// Attempt to execute the task on the caller thread if policy allows.
	bool TryRunningTaskInline();

	// collectTaskImpl
	// Prepare the task for execution: resolve locks, connect args, and ready the runnable.
	bool collectTaskImpl();

	// releaseRunCount
	// Release execution license and update status upon completion.
	void releaseRunCount(task_status status);

	// separateResources
	// Decouple heavy resources (locks/interests) for cleanup based on terminal status.
	garbage_can separateResources(task_status status);

	// connectArgs
	// Resolve supplier interests and build argument set for operator execution.
	bool connectArgs(const FutureSuppliers& allInterests);

	// disconnect_waiters
	// Remove all waiters and return a garbage_can for deferred cleanup.
	garbage_can disconnect_waiters();

	// disconnect_supplier
	// Remove a single supplier relation and return a garbage_can for deferred cleanup.
	garbage_can disconnect_supplier(OperationContext* supplier);

	// Result handle and its keeper to maintain actor interest until completion.
	FutureResultData m_Result;
	SharedActorInterestPtr m_ResKeeper;

	// Atomic task status; drives state machine transitions.
	std::atomic<task_status> m_Status = task_status::none;

	// Run_with_catch
	// Execute task with exception boundary; converts exceptions to OnException.
	void Run_with_catch(explain_context_ptr_t context) noexcept;

	// Run_with_cleanup
	// Execute task with guaranteed cleanup and final OnEnd transition (after license/locks).
	void Run_with_cleanup(explain_context_ptr_t context) noexcept;

public:
	// Task function that implements the computation or write logic.
	task_func_type                    m_TaskFunc;

	// Write lock for the destination item (held when needed).
	ItemWriteLock      m_WriteLock;

	// Timestamp when this context became active (for diagnostics/scheduling).
	TimeStamp          m_ActiveTimestamp = -1;

public:
	// Phase number used to coordinate group waits/blocks across contexts.
	phase_number m_PhaseNumber = 0;

	// SetReadLocks
	// Acquire read locks for all suppliers as needed for consistent reads.
	std::vector<ItemReadLock> SetReadLocks(const FutureSuppliers& allInterests);

	// SetReadLock
	// Helper to insert a single read lock for 'si' into 'locks'.
	bool SetReadLock(std::vector<ItemReadLock>& locks, const TreeItem* si);

	// MustCalcArg
	// Query operator policy to decide if argument 'i' must be (re)calculated based on policy and firstArgValue.
	bool MustCalcArg(arg_index i, CharPtr firstArgValue) const;

	// RunOperator
	// Execute the operator using resolved ArgRefs and acquired read locks, optionally emitting explain info.
	void RunOperator(ArgRefs allInterests, std::vector<ItemReadLock> readLocks, explain_context_ptr_t context);

	// GetFuncDC
	// Accessor for the bound function DataController, if any.
	auto GetFuncDC() const->SharedPtr<const FuncDC>;

	// Operator/Group accessors (valid only if m_FuncDC != nullptr).
	const Operator*       GetOperator () const { assert(m_FuncDC); assert(m_FuncDC->m_Operator);      return m_FuncDC->m_Operator; }
	const AbstrOperGroup* GetOperGroup() const { assert(m_FuncDC); assert(m_FuncDC->m_OperatorGroup); return m_FuncDC->m_OperatorGroup; }

public:
	// Weak reference to source FuncDC; may become null if DC is reset.
	WeakPtr<const FuncDC> m_FuncDC;

	// Suppliers this context depends on (strong references to keep them alive while needed).
	SupplierSet           m_Suppliers;

	// Waiters that depend on this context (weak references to avoid cycles).
	WaiterSet             m_Waiters;
};

// GetNextPhaseNumber
// Global monotonic counter to assign new phase numbers for batch scheduling/waiting.
TIC_CALL auto GetNextPhaseNumber() -> phase_number;

// DoWorkWhileWaitingFor
// Allow current thread to perform other work while waiting for a phase to complete.
TIC_CALL void DoWorkWhileWaiting();
TIC_CALL void DoWorkWhileWaitingFor(std::atomic<task_status>* phaseContainerStatus);

// WakeUpJoiners
// Notifies threads/contexts waiting on global join conditions to re-check status.
TIC_CALL void WakeUpJoiners();

// Diagnostics for current blocked phase (for UI/debugger integration):
// - s_CurrBlockedPhaseNumber: phase that is currently blocked
// - s_CurrBlockedPhaseItem:   item that caused the current block
// - s_CurrPhaseContainer:     container/owner of the current phase
TIC_CALL extern phase_number s_CurrBlockedPhaseNumber;
TIC_CALL extern const TreeItem* s_CurrBlockedPhaseItem;
TIC_CALL extern const TreeItem* s_CurrPhaseContainer;

#endif //!defined(__TIC_OPERATIONCONTEXT_H)
