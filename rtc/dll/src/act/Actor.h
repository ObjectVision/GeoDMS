// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////
//
// Actor.h
//
// Overview:
// - Actor is the core base class for the update mechanism, interest counting,
//   time-stamping, progress states, and failure reporting.
// - It represents a node that may depend on other "supplier" actors, propagates
//   invalidation, and can be updated in a suspendible manner.
// - The class provides:
//   * Update orchestration (SuspendibleUpdate/CertainUpdate/Update)
//   * State and progress tracking (m_State, timestamps, phases)
//   * Failure capture/reporting (Fail/ThrowFail/GetFailReason)
//   * Interest tracking (reference-like "interest" with start/stop hooks)
//   * Supplier traversal (VisitSuppliers/UpdateSuppliers)
//   * Meta-info updates separate from data updates (UpdateMetaInfo/Update)
//
// Thread-safety expectations:
// - Many methods are const but mutate via 'mutable' data members; actual
//   thread-safety depends on internal synchronization in m_State and other
//   components. Callers should ensure proper external synchronization if needed.
//
// Suggestions for improvements (non-breaking, consider for future):
// - Consider adding [[nodiscard]] where ignoring return values can cause logic
//   errors (e.g., Was/Is/WasFailed/IsFailed/UpdateSuppliers).
// - Prefer noexcept where it is guaranteed (e.g., ClearFail where safe,
//   getters, and Stop* methods). Audit exception guarantees to document them.
// - Unify overloads of Fail/ThrowFail to reduce API surface (e.g., use
//   std::variant<std::monostate, ErrMsgPtr, SharedStr, const Actor*>).
// - Clarify CharPtr/WeakStr/SharedStr usage; prefer std::string_view where
//   ABI permits, to reduce allocations and overload set size.
// - Consider making interest_count_t atomic if accessed cross-thread, or
//   document that Actor instances are confined to a thread.
// - Evaluate whether WasFailed(ProgressState) should be deprecated due to
//   ambiguity (already flagged by comment "DON'T CALL THIS ONE").
// - Use enum class for FailType/ProgressState to improve type safety if
//   compatible with existing code.
// - Add documentation on lifecycle expectations: when is UpdateMetaInfo called,
//   and when are Update and DetermineState invoked relative to interest changes.
// - Consolidate UpdateLock semantics and document re-entrancy constraints.
// - Where applicable, add tracing hooks (e.g., events) instead of debug macros.
//
//-----------------------------------------------------------------------------

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  Description : Actor serves as basis class for the Update Mechanism, interest counting, time-stamping, and fence numbers.
 */

#if !defined(__RTC_ACT_ACTOR_H)
#define __RTC_ACT_ACTOR_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "act/ActorEnums.h"
#include "ptr/InterestHolders.h"
#include "ptr/PersistentSharedObj.h"
#include "ptr/SharedStr.h"

#include <any>
struct ActorVisitor;
struct SupplInterestListPtr;

struct ErrMsg;
using ErrMsgPtr = std::shared_ptr<ErrMsg>;
using PersistentSharedActor = SharedObjWrap<Actor>;
using SharedActorInterestPtr = InterestPtr<SharedPtr<const PersistentSharedActor> >;

//  -----------------------------------------------------------------------
//  struct Actor interface
//  -----------------------------------------------------------------------

struct Actor: PersistentObject
{
	// Constructor/Destructor
	// Note: Many methods are logically const but mutate internal state via 'mutable'.
	// Ensure that concurrent access is externally synchronized or state is internally protected.
	RTC_CALL Actor();
	RTC_CALL virtual ~Actor();

	// Update entry points
	// SuspendibleUpdate:
	// - Performs an update that is allowed to suspend or fail gracefully.
	// - Return value indicates whether the update completed or was suspended/failed.
	// - Should be side-effect free on failure and leave state consistent on suspend.
	RTC_CALL virtual ActorVisitState SuspendibleUpdate(ProgressState ps) const;

	// CertainUpdate:
	// - Performs an update that must complete (does not suspend).
	// - Calls UpdateMetaInfo, acquires MustSuspend() interrupter lock, then DoUpdate.
	// - 'blockingAction' describes the action for diagnostic/progress reporting.
	RTC_CALL void CertainUpdate(ProgressState ps, CharPtr blockingAction) const;

	// Update orchestration helper:
	// - If mustUpdate is false: attempts SuspendibleUpdate; returns false only if suspended/failed.
	// - If mustUpdate is true: forces a CertainUpdate (non-suspendible) and returns true.
	bool Update(ProgressState ps, bool mustUpdate, CharPtr blockingAction) const
	{
		if (!mustUpdate)
			return SuspendibleUpdate(ps) != AVS_SuspendedOrFailed;
		CertainUpdate(ps, blockingAction);
		return true;
	}

	// Invalidation
	// - Marks this actor and its consumers as invalidated.
	// - Triggers re-computation on next Update call(s).
	RTC_CALL void Invalidate ();

	// Timestamp/Progress management
	// - MarkTS: explicitly mark lastChangeTS with a provided timestamp without updating.
	RTC_CALL void MarkTS(TimeStamp ts) const;
	// - Set progress timestamp for a specific ProgressState (e.g., meta/data).
	RTC_CALL void SetProgressAt(ProgressState ps, TimeStamp ts) const;
	// - SetProgress: record that progress reached 'ps' at the change source timestamp.
	RTC_CALL virtual void SetProgress(ProgressState ps) const; // set lastChangeTS to ChangeSource TimeStamp
	// - RaiseProgress: monotonically raise progress if lower than 'ps'.
	RTC_CALL void RaiseProgress(ProgressState ps = ProgressState::Committed) const { if (m_State.GetProgress() < ps) SetProgress(ps); }
	// State queries
	// - Was: true if m_State.GetProgress() >= ps (i.e., progress has reached ps in the past).
	RTC_CALL bool Was  (ProgressState ps) const; //	true if m_State.GetProgress() >= ps
	// - Is: true if still valid for >= ps (encapsulates GetState semantics).
	RTC_CALL bool Is   (ProgressState ps) const; //	true if still valid. Is encapsulates GetState unless it wasn't there yet
	// Failure state queries
	RTC_CALL bool IsFailed () const;  //	true if erroneous. IsFailed encapsulates GetState.
	FailType GetFailType() const { return m_State.GetFailType(); }
	RTC_CALL bool IsFailed (FailType ps) const;  //	true if erroneous. IsFailed encapsulates GetState.
          bool IsFailed(bool doCalc) const { return IsFailed(doCalc ? FailType::Data : FailType::MetaInfo); }
    bool IsDataFailed () const { return IsFailed(FailType::Data); }
	RTC_CALL bool WasFailed() const { return m_State.IsFailed(); }
	RTC_CALL bool WasFailed(FailType fr) const;
	// WARNING: ambiguous semantics; prefer WasFailed(FailType). See improvement notes.
	bool WasFailed(ProgressState ps) const; // DON'T CALL THIS ONE
          bool WasFailed(bool doCalc) const { return WasFailed(doCalc ? FailType::Data : FailType::MetaInfo); }
    bool WasValid (ProgressState ps = ProgressState::Committed) const { return m_State.GetProgress() >= ps && !m_State.IsFailed();  }

	// Trigger an evaluation/update cycle proactively (e.g., prefetch).
	RTC_CALL void TriggerEvaluation() const;

	// Failure reporting
	RTC_CALL ErrMsgPtr GetFailReason() const;
	RTC_CALL void ClearFail() const;

	// Change tracking
	// - GetLastChangeTS: computes and returns the last change timestamp across this actor and suppliers.
	RTC_CALL TimeStamp GetLastChangeTS () const;
	// - LastChangeTS: returns the cached last change timestamp; intended for persistence only.
	TimeStamp LastChangeTS () const { return m_LastChangeTS; } // for persistency only

	// Update all actors in the global actor list. (Invoker outside this class)

	// Change rights checks (security/permissions)
	RTC_CALL virtual void AssertPropChangeRights(CharPtr changeWhat) const;
	RTC_CALL virtual void AssertDataChangeRights(CharPtr changeWhat) const;

	// State flags
	// - Passor: indicates pass-through semantics or special update behavior.
	bool IsPassor() const { return m_State.IsPassor(); }
	RTC_CALL void SetPassor();
	// - PassorOrChecked: either pass-through or has determining check semantics.
	bool IsPassorOrChecked() const { return IsPassor() || m_State.IsDeterminingCheck(); }

	// Supplier traversal
	// - Override to enumerate/traverse suppliers; visitor returns an ActorVisitState to control traversal.
	RTC_CALL virtual ActorVisitState VisitSuppliers    (SupplierVisitFlag svf, const ActorVisitor& visitor) const;

	// Fail fast helpers (throwing)
	// - Provide rich context using ErrMsg, SharedStr, or source Actor.
	[[noreturn]] RTC_CALL void ThrowFail(ErrMsgPtr why, FailType ft) const;
	[[noreturn]] RTC_CALL void ThrowFail(SharedStr str, FailType ft) const;
	[[noreturn]] RTC_CALL void ThrowFail(CharPtr str, FailType ft) const;
	[[noreturn]] RTC_CALL void ThrowFail(const Actor* src, FailType ft) const;
	[[noreturn]] RTC_CALL void ThrowFail(const Actor* src) const;
	[[noreturn]] RTC_CALL void ThrowFail() const;

	// Fail capture without throwing (record and continue)
	RTC_CALL void Fail(WeakStr why, FailType ft) const;
	RTC_CALL void Fail(CharPtr str, FailType ft) const;
	RTC_CALL void Fail(const Actor* src, FailType ft) const;
	RTC_CALL void Fail(const Actor* src) const;
	// Catch/normalize a failure for a given fail type (e.g., clear or downgrade)
	RTC_CALL void CatchFail(FailType ft) const;

	// Recompute state based on suppliers and change detection heuristics.
	RTC_CALL void DetermineState () const;
	// Update suppliers for a given progress state; returns visit/update outcome.
	RTC_CALL ActorVisitState UpdateSuppliers(ProgressState ps) const;

protected:
	// Apply decision hook used by update flow to decide if an apply-like action is needed.
	RTC_CALL bool MustApplyImpl() const;

	// Actual update implementation for derived classes; default may orchestrate meta/data updates.
	RTC_CALL virtual ActorVisitState DoUpdate(ProgressState ps);
	// Handle failure reporting in derived classes; return true if failure is handled.
	RTC_CALL virtual bool DoFail(ErrMsgPtr msg, FailType ft) const;

	// Derived class hook for invalidation. Called during Invalidate path.
	RTC_CALL virtual void DoInvalidate () const;
	// Update meta-information only (e.g., schema, shape, headers). Must be noexcept to guarantee invariants.
	RTC_CALL virtual void UpdateMetaInfo() const noexcept;

	// Update meta-info of suppliers. Called by CertainUpdate/DetermineState when needed.
	RTC_CALL void UpdateSupplMetaInfo() const;

	// Detect last supplier change and potential failure type; default aggregates suppliers' timestamps.
	RTC_CALL virtual TimeStamp DetermineLastSupplierChange(ErrMsgPtr& failReason, FailType& failType) const; // noexcept;

	// Invalidate at a specific timestamp (used for partial/incremental invalidation).
	RTC_CALL void InvalidateAt(TimeStamp invalidate_ts) const;

public:
	// Interest management
	// - Interest indicates that the actor's value is needed/observed.
	// - Transition from 0->1 triggers StartInterest; 1->0 triggers StopInterest.
	RTC_CALL void IncInterestCount() const;
	RTC_CALL void DuplInterestCount() const;

	RTC_CALL garbage_can DecInterestCount() const noexcept;
	auto GetInterestCount() const noexcept { return m_InterestCount; }
	// - Returns true if any supplier interest is active.
	RTC_CALL bool DoesHaveSupplInterest() const noexcept;

	// Supplier interest list access (lifetime owned externally).
	RTC_CALL SupplInterestListPtr GetSupplInterest() const;
	// Returns the shared interest handle for this actor if any (or null).
	RTC_CALL SharedActorInterestPtr GetInterestPtrOrNull() const;

protected:
	// Lifecycle hooks when interest starts/stops on this actor itself.
	RTC_CALL virtual void StartInterest() const;
	RTC_CALL virtual garbage_can StopInterest () const noexcept;

public:
	// Supplier interest orchestration
	RTC_CALL void StartSupplInterest   () const;          friend struct FuncDC;
	RTC_CALL garbage_can StopSupplInterest() const noexcept; friend struct AbstrOperGroup;
	RTC_CALL void RestartSupplInterestIfAny() const;
	// Phase numbers
	// - Phase numbers tag update cycles to prevent re-entrancy issues and detect recursion.
	RTC_CALL auto GetPhaseNumber() const -> phase_number;
	// - Current phase number (valid for pass-through or when set).
	auto GetCurrPhaseNumber() const -> phase_number { MG_CHECK(m_PhaseNumber || IsPassor()); return m_PhaseNumber; }

	// Data Members
public:
	// State flags and progress tracking; implementation-defined bitset/flag container.
	mutable actor_flag_set m_State;
	// Timestamp of the last change (self or propagated); 0 means "unknown/not-set".
	mutable TimeStamp      m_LastChangeTS = 0;
	// Timestamp of last full GetState evaluation; used to avoid redundant recomputation.
	mutable TimeStamp      m_LastGetStateTS = 0;
	// Phase number used to identify update/evaluation cycles.
	mutable phase_number   m_PhaseNumber = 0;

#if defined(MG_ITEMLEVEL)
	// Optional: item-level granularity marker; semantics depend on build-time options.
	mutable item_level_type m_ItemLevel = item_level_type(0);
#endif

protected:
	// Number of active interests on this actor; see StartInterest/StopInterest hooks.
	mutable interest_count_t m_InterestCount = 0;
private:

	#if defined(MG_DEBUG_DATA)
		// Debug counters for lifecycle/invalidation tracking
		static UInt32 sd_CreationCount;
		static UInt32 sd_InstanceCount;

		static UInt32 sd_InvalidationProtectCount;
	#endif	// MG_DEBUG

public: // status info functions, Debug statistics
	// Global counters
	RTC_CALL static UInt32 GetNrInstances();
	RTC_CALL static UInt32 GetNrCreations();

	// RAII to guard update transitions; sets and clears transitional flags in m_State.
	struct UpdateLock //: ::UpdateLock
	{
		RTC_CALL UpdateLock(const Actor* act, actor_flag_set::TransState ps);
		RTC_CALL ~UpdateLock();

		const Actor*               m_Actor;
		actor_flag_set::TransState m_TransState;
	};
protected:

#if defined(MG_DEBUG)
	// Debug helpers
	RTC_CALL virtual bool IsShvObj() const; 
	RTC_CALL virtual SharedStr GetExpr() const { return SharedStr(); }
#endif

	// Allow internal components to manipulate Actor internals.
	friend struct OperationContext;
	friend struct DetermineStateLock;
};

#if defined(MG_ITEMLEVEL)
// Utility to fetch item-level for an actor; respects optional feature flag.
RTC_CALL item_level_type GetItemLevel(const Actor* act);
#endif

// Convenience predicate: whether the provided actor was in any failed state.
RTC_CALL bool WasInFailed(const Actor* a);


#endif // __RTC_ACT_ACTOR_H

