// Copyright (C) 1998-2023 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

/*
PSEUDOCODE PLAN (documentation-only, no behavior changes)
- Goal: Add clarifying comments and light-weight TODO suggestions without changing logic.
- Strategy:
  1) Annotate high-level concepts: timestamps, progress states, fail types, interest counts, and supplier interest.
  2) Explain RAII locks (UpdateLock, DetermineStateLock) and concurrency/ordering guarantees.
  3) Document main flows:
     - InvalidateAt: how invalidation is applied and interest/suppliers are handled.
     - SetProgress/MarkTS: monotonic progress and timestamp semantics.
     - SuspendibleUpdate/CertainUpdate: update orchestration, suspension, and failure propagation.
     - DetermineState/DetermineLastSupplierChange: change detection across suppliers.
     - UpdateSuppliers: cascading updates with suspension/failure handling.
     - Interest management: Inc/Dec interest count, Start/Stop interest, supplier interest lists.
     - Fail handling/logging: ClearFail, DoFail, ThrowFail, concurrency guards.
  4) Identify improvement suggestions as TODO comments:
     - Consider std::scoped_lock/atomic where safe to simplify lock management.
     - Reduce duplication and cyclomatic complexity (especially in SuspendibleUpdate and DetermineState).
     - Narrow critical sections; ensure no callbacks in locked sections when avoidable.
     - Improve naming/enum conversions, and add stronger invariants/assertions where cheap.
     - Add unit tests around recursion, suspension, and failure edge-cases.
  5) Keep code identical except for comments. Build should remain unaffected.
*/

#include "RtcPCH.h"

#if defined(_MSC_VER)
#pragma hdrstop
#endif //defined(_MSC_VER)

#include "act/Actor.h"
#include "act/garbage_can.h"
#include "act/ActorEnums.h"
#include "act/ActorSet.h"
#include "act/ActorVisitor.h"
#include "act/InterestRetainContext.h"
#include "act/SupplierVisitFlag.h"
#include "act/TriggerOperator.h"
#include "act/UpdateMark.h"

#include "dbg/DmsCatch.h"
#include "mci/Class.h"
#include "ptr/InterestHolders.h"
#include "ptr/SharedStr.h"
#include "set/StaticQuickAssoc.h"
#include "set/VectorFunc.h"
#include "utl/Environment.h"
#include "utl/mySPrintF.h"
#include "utl/scoped_exit.h"
#include "xct/DmsException.h"

#include "RtcInterface.h"
#include "LockLevels.h"

#include <set>
//#include <ppltasks.h>

#if defined(MG_DEBUG_INTERESTSOURCE)

#define MG_DEBUG_INTERESTSOURCE_VALUE false

#else

#define MG_DEBUG_INTERESTSOURCE_VALUE false

#endif


// *****************************************************************************
//                                      TargetCount
// *****************************************************************************
// Global remaining-target counter helpers, guarded by flags to avoid double counting.
// NOTE: Inc/DecRemainingTargetCount() (free functions) are assumed to exist elsewhere.

inline void IncRemainingTargetCount(const Actor* self)
{
    dbg_assert(! self->m_State.Get(actor_flag_set::AFD_ProcessCounted) );

    IncRemainingTargetCount();

    MG_DEBUGCODE( self->m_State.Set(actor_flag_set::AFD_ProcessCounted); )
}

inline void DecRemainingTargetCount(const Actor* self)
{
    dbg_assert(  self->m_State.Get(actor_flag_set::AFD_ProcessCounted) );

    DecRemainingTargetCount();

    MG_DEBUGCODE( self->m_State.Clear(actor_flag_set::AFD_ProcessCounted); )
}


//  -----------------------------------------------------------------------
//  actor_flag_set
//  -----------------------------------------------------------------------
// Map transient state to a human-readable name, for diagnostics/logging.
CharPtr GetActorFlagName(actor_flag_set::TransState ts)
{
    switch (ts)
    {
        case actor_flag_set::AF_IsPassor:         return "Passor";
        case actor_flag_set::AF_DeterminingCheck: return "DetermineCheck";
        case actor_flag_set::AF_DeterminingState: return "DetermineState";
        case actor_flag_set::AF_UpdatingMetaInfo: return "MetaInfo";
        case actor_flag_set::AF_ChangingInterest: return "ChangeInterest";
        case actor_flag_set::AF_CalculatingData:  return "PrimaryData";
        case actor_flag_set::AF_Validating:       return "Validation";
        case actor_flag_set::AF_Committing:       return "Commit";
    }
    return "Unknown progress";
}

// FailType mapping for transient states. Keep aligned with enum layout.
const FailType failReasonArray[8] = { FailType::None, FailType::Committed, FailType::Validate, FailType::Data, FailType::MetaInfo, FailType::MetaInfo, FailType::MetaInfo, FailType::MetaInfo };

FailType TransState_FailType(actor_flag_set::TransState ts)
{
    dms_assert(ts == (ts & actor_flag_set::AF_TransientMask));
    return failReasonArray[ts / actor_flag_set::AF_TransientBase];
}

//  -----------------------------------------------------------------------
//  struct UpdateLock implementation
//  -----------------------------------------------------------------------
// RAII that enforces monotonic transient state progress within operations.
// On construction: pushes a stricter transient state (ts) onto the actor.
// On destruction: restores the original state.
// TODO: Consider logging reentrancy/call stacks for debugging recursive transitions.
namespace UpdateLockFuncs 
{
    void Set(actor_flag_set* afsPtr, actor_flag_set::TransState& ts)
    {
        assert(afsPtr);
        assert((ts <= actor_flag_set::AF_DeterminingState) || ! afsPtr->IsFailed(TransState_FailType(ts)));  // no fallback allowed without going through reset by Invalidation
        assert(! afsPtr->IsPassor() || (ts <= actor_flag_set::AF_ChangingInterest));
        actor_flag_set::TransState oldTransState = afsPtr->GetTransState();
        if (!(ts > oldTransState)) // requirement of strict monotounous increasing progress 
            throwDmsErrF("Cannot start %s while doing %s; check for recursive dependencies"
            ,   GetActorFlagName(ts)
            ,   GetActorFlagName(oldTransState)
            );
        afsPtr->SetTransState(ts);
        ts = oldTransState;
    }

    void Clear(actor_flag_set* afsPtr, actor_flag_set::TransState oldTransState)
    {
        dms_assert(afsPtr);
        dms_assert(oldTransState < afsPtr->GetTransState()); // Clear only reverses successful calls to Set, which guarantees this.
        afsPtr->SetTransState(oldTransState);
    }

} // namespace UpdateLockFuncs

// RAII wrapper around UpdateLockFuncs with an Actor context.
Actor::UpdateLock::UpdateLock(const Actor* act, actor_flag_set::TransState ts)
    :   m_Actor(act)
    ,   m_TransState(ts)
{
    assert(IsMetaThread());
    if (act && ts > actor_flag_set::AF_DeterminingState && act->WasFailed(TransState_FailType(ts)))
        act->ThrowFail();
    assert(!act || ts <= actor_flag_set::AF_DeterminingState || !act->WasFailed(TransState_FailType(ts)) ); //|| ps == PS_DetermineState || ps == PS_ChangeInterest); // no fallback allowed without going through reset by Invalidation
    if (m_Actor)
        UpdateLockFuncs::Set  (&(m_Actor->m_State), m_TransState);
}

Actor::UpdateLock::~UpdateLock()
{
    if (m_Actor)
        UpdateLockFuncs::Clear(&(m_Actor->m_State), m_TransState);
}

//  -----------------------------------------------------------------------
// SupplInterestListElem
//  -----------------------------------------------------------------------

#include "act/SupplInterest.h"

// Supplier interest registry. Holds which suppliers are observed by which targets.
RTC_CALL static_ptr<SupplTreeInterestType> s_SupplTreeInterest;

SupplInterestListPtr MoveSupplInterest(const Actor* self); // forward declaration

//  -----------------------------------------------------------------------
//  context for holding interest
//  -----------------------------------------------------------------------
// Thread-local interest retain mechanism to keep temporary supplier interest alive
// during complex traversals (e.g., DetermineState).
THREAD_LOCAL SupplInterestListElem* sc_RetainContextBuffer = nullptr;
THREAD_LOCAL UInt32                 sc_RetainContextCount  = 0;


// Accessor cast that overlays raw TLS storage with a strongly-typed Ptr.
SupplInterestListPtr& GetRetainContext()
{
    dms_assert(sizeof(SupplInterestListElem*) == sizeof(SupplInterestListPtr));
    return reinterpret_cast<SupplInterestListPtr&>(sc_RetainContextBuffer);
}

// RAII to increase/decrease a retain counter and flush the TLS list when last exit.
InterestRetainContextBase::InterestRetainContextBase()
{
    ++sc_RetainContextCount;
}

InterestRetainContextBase::~InterestRetainContextBase()
{
    --sc_RetainContextCount;
    if (!IsActive())
    {
        SupplInterestListPtr localStore = std::move(GetRetainContext());
    }
}

bool InterestRetainContextBase::IsActive()
{
    return sc_RetainContextCount != 0;
}

void InterestRetainContextBase::Add(const PersistentSharedActor* actor)
{
//  assert(IsActive()); // PRECONDITION
    if (IsActive() && actor) // else it will never be removed; REMOVE IF ASSERT IS PROVEN
        push_front(GetRetainContext(), actor);
}

//  -----------------------------------------------------------------------
//  struct Actor implementation: static, ctor/dtor
//  -----------------------------------------------------------------------

#if defined(MG_DEBUG_DATA)

    UInt32 Actor::sd_CreationCount = 0;
    UInt32 Actor::sd_InstanceCount = 0;
    UInt32 Actor::sd_InvalidationProtectCount = 0;

    bool sd_DebugInvalidations = false;

#endif  // MG_DEBUG

// ctor / dtor
Actor::Actor ()
{
    #if defined(MG_DEBUG)
        sd_InstanceCount++;   // Maintain statistics
        sd_CreationCount++;   // Maintain statistics
    #endif  // MG_DEBUG
}

Actor::~Actor ()
{
    dms_assert(!m_InterestCount);
    dbg_assert(!m_State.Get(actor_flag_set::AFD_ProcessCounted));

    ClearFail();
    #if defined(MG_DEBUG)
        sd_InstanceCount--;   // Maintain statistics
    #endif  // MG_DEBUG
}


//  -----------------------------------------------------------------------
//  struct Actor implementation: SetValid, Invalidate, SetChanged
//  -----------------------------------------------------------------------

// Mark this actor as having changed at timestamp 'ts'.
// Precondition: ts is monotonic (>= tsBereshit).
void Actor::MarkTS(TimeStamp ts) const
{
    DBG_START("Actor", "MarkTS", sd_DebugInvalidations);
    DBG_TRACE(("time = %u", ts));

    dms_assert(ts >= UpdateMarker::tsBereshit) ;

#if defined(MG_DEBUG_TS_SOURCE)
    if (m_LastChangeTS > ts)
        UpdateMarker::ReportActiveContext("MarkTS out of context");
#endif

    m_LastChangeTS = ts;
}

// Set progress and the timestamp it occurred at.
// NOTE: This does not force recomputation; it records where we are.
void Actor::SetProgressAt(ProgressState ps, TimeStamp ts) const
{
    MarkTS(ts);
    SetProgress(ps);
}

// Invalidate this actor at 'invalidate_ts' if newer than current.
// Clears failures, drops progress to PS_None, and (re)starts supplier interest if needed.
// TODO: Consider narrowing window of state changes to simplify reasoning under concurrency.
void Actor::InvalidateAt (TimeStamp invalidate_ts) const
{
    DBG_START("Actor", "InvalidateAt", sd_DebugInvalidations);
    DBG_TRACE(("time = %u", invalidate_ts));

    dms_assert(!m_State.IsCommitting());
    dms_assert(!m_State.IsCalculatingData());

#if defined(MG_DEBUG)
    MGD_CHECKDATA(!sd_InvalidationProtectCount);
#endif
    assert(m_LastChangeTS <= invalidate_ts); // invalidations must have a chronological cause

    if  (m_LastChangeTS == invalidate_ts)
    {
        //  new invalidation (after Update, Suspend or failure) must have a cause
        assert(! m_State.IsFailed() ); 
        assert(m_State.GetProgress() <= ProgressState::MetaInfo || invalidate_ts == UpdateMarker::tsBereshit); 
        return;
    }

    assert(m_LastChangeTS < invalidate_ts); // implied from above

    // don't Update or reCheckIntegrity until next Invalidation
    ClearFail();
    m_State.SetProgress(ProgressState::None);

    m_LastChangeTS = invalidate_ts;
    if (m_State.HasInvalidationBlock())
        return;

    // Move out supplier interest during invalidation, then call DoInvalidate() to restart interest if required.
    SupplInterestListPtr keepOldInterest = MoveSupplInterest(this);
    DoInvalidate();       // call-back for specific behaviour, which calls StartSupplInterest();
    assert(DoesHaveSupplInterest() || !m_InterestCount || IsPassor() || WasFailed(FailType::Data));
}

// Raise progress if strictly increasing; ensures monotonic progress and triggers suspendable fences.
void Actor::SetProgress(ProgressState ps) const // called by SetProgressAt( UpdateMarker::GetActiveTS() );
{
    assert(m_LastChangeTS || IsPassor()); // must have gone through DetermineState
//  dms_assert(ps >= m_State.GetProgress());  // activated at 21-08-2012, see if it holds, REMOVE COMMENT

    if (ps > m_State.GetProgress())
    {
        m_State.SetProgress(ps);
        SuspendTrigger::MarkProgress();
    }
}

// Decide whether a further apply/update is needed based on progress/failure state.
bool Actor::MustApplyImpl() const
{
    return (m_State.GetProgress() < ProgressState::Committed) && !WasFailed(FailType::Data); // is there still stuff to do with suppliers?
}

// Public invalidation entry that stamps current active TS and calls InvalidateAt.
// No-op for objects not yet timestamped (i.e., before initial DetermineState).
void Actor::Invalidate ()
{
    if (m_LastChangeTS != 0)
    {
        TimeStamp invalidate_ts = UpdateMarker::GetActiveTS(MG_DEBUG_TS_SOURCE_CODE("Invalidation"));
        InvalidateAt( invalidate_ts );
    }
}

// Has ever reached a given progress state (regardless of current validity).
bool Actor::Was(ProgressState ps) const
{
    return m_State.GetProgress() >= ps; // go back to invalidated when supplier has changed
}

// Is currently valid at or above 'ps'. May call DetermineState lazily.
// NOTE: This can run expensive dependency checks.
bool Actor::Is(ProgressState ps) const
{
    if (!Was(ps))
        return false;
    DetermineState();
    return Was(ps) && !WasFailed(); // go back to invalidated when supplier has changed
}

// Failure state queries are sensitive to timestamps and last get-state check.
// For FR <= FR_Determine track against last DetermineState epoch.
bool Actor::WasFailed(FailType fr) const 
{
    if (!m_State.IsFailed())
        return false;
    if (GetFailType() > fr)
        return false;
    if (fr > FailType::Determine)
        return true;
    return m_LastGetStateTS == UpdateMarker::GetLastTS();
}

bool Actor::IsFailed() const 
{
    if (!WasFailed())
        return false;
    DetermineState(); // go back to invalidated when supplier has changed
    return WasFailed();
}

bool Actor::IsFailed(FailType ft) const 
{
    if (!WasFailed(ft))
        return false;
    DetermineState(); // go back to invalidated when supplier has changed
    return WasFailed(ft);
}

// Ensure a fresh timestamp exists if last-change was equal to system last (to trigger reevaluation).
void Actor::TriggerEvaluation() const
{
    if (m_LastGetStateTS == UpdateMarker::impl::tsLast)
        UpdateMarker::TriggerFreshTS(MG_DEBUG_TS_SOURCE_CODE("TriggerEvaluation of the last change"));
}

// Determine and return last change timestamp, recalculating state as necessary.
TimeStamp Actor::GetLastChangeTS () const
{
    DetermineState();
    assert(m_LastChangeTS || IsPassor() || WasFailed() || m_State.IsDeterminingCheck()); // must have been set by DetermineState unless it was a Passor or DetermineState Failed
    return m_LastChangeTS;
}

//  -----------------------------------------------------------------------
//  struct Actor implementation: Update
//  -----------------------------------------------------------------------
// Main update driver that can suspend or fail. Will:
// 1) Ensure state is determined.
// 2) Early-exit if already at progress 'ps' or passor.
// 3) Update suppliers.
// 4) If suppliers ready, attempt own DoUpdate, respecting suspension.
// 5) Propagate failure or raise own progress to 'ps'.
// TODO: Reduce cyclomatic complexity (many branches). Consider splitting into helpers.
ActorVisitState Actor::SuspendibleUpdate(ProgressState ps) const // returns false in case of failed or suspended
{
    assert(!SuspendTrigger::DidSuspend()); // PRECONDITION

    assert(ps >= ProgressState::Validated); 

    assert(IsMetaThread());

    DetermineState(); // go back to US_Invalidated when supplier has changed, call DoInvalidate if nessecary

    if (m_State.GetProgress() > ps)
        return AVS_Ready;
    FailType ft = (ps== ProgressState::Committed) ? FailType::Committed : FailType::Validate;
    if (WasFailed(ft))
        return AVS_SuspendedOrFailed;
    if (m_State.GetProgress() == ps)
        return AVS_Ready;
    dms_assert(m_State.GetProgress() < ps);

    dms_assert(! IsPassor());
    if (IsPassor())
        return AVS_Ready;

    dms_assert(m_LastChangeTS); // must have been set by DetermineState

     // we can try (to resume) to update now
    MG_DEBUGCODE( TimeStamp lts = UpdateMarker::LastTS() );

    dms_assert(!WasFailed(ft));

    if (!MustApplyImpl())
    {
        SetProgress(ps);
        return AVS_Ready;
    }

    // Establish transient state (validating/committing) for duration of update.
    UpdateLock updateLock(this, (ps==ProgressState::Validated) ? actor_flag_set::AF_Validating : actor_flag_set::AF_Committing);

    dms_assert(!SuspendTrigger::DidSuspend()); // follows on precondition

    dms_assert(!WasFailed(ft));

    // ===========================
    ActorVisitState updateRes = UpdateSuppliers(ps); 
    // ===========================

    dms_assert(updateRes || WasFailed(ft) || SuspendTrigger::DidSuspend());
    // don't leave now on !updateRes since a failed supplier will have to cause CheckInvalidate to fail this

    // UpdateSuppliers may have resulted in a (new) fail reason: supplier failed, or any other fail
    // also (default) we want to detect failed supplier even when not yet processed 
    // or derived classes might decide to ignore supplier fails
    // also when updateRes == true, we need to CheckIntegrity since overridden impl may check preconditions for DoUpdate

    if (WasFailed(ft))
        return AVS_SuspendedOrFailed;

    dms_assert(m_State.GetProgress() < ps); // UpdateSuppliers only could affect Progress in case of failure

    if (updateRes == AVS_SuspendedOrFailed)
    {
        assert(SuspendTrigger::DidSuspend());
        return AVS_SuspendedOrFailed;
    }
    if (m_State.GetProgress() >= ProgressState::Committed)
        StopSupplInterest();

    if (m_State.GetProgress() >= ps) // a supplier could have been a creator/manager
        return AVS_Ready;

    assert(m_LastGetStateTS == UpdateMarker::LastTS());
    UpdateMarker::ChangeSourceLock changeStamp(dynamic_cast<const PersistentSharedActor*>(this),  "Update");

#if defined(MG_DEBUG_INTERESTSOURCE)
    DemandManagement::IncInterestDetector incInterestLock("Actor::SuspendibleUpdate()");
#endif

    try {
        assert(updateRes != AVS_SuspendedOrFailed); // follows from previous ifs
        MG_DEBUGCODE(int d_WTF = 0 );
        if (SuspendTrigger::MustSuspend())
        {
            updateRes = AVS_SuspendedOrFailed;
            MG_DEBUGCODE(d_WTF = 1);
        }
        else
        {
            MG_DEBUGCODE(d_WTF = 2);
            updateRes = const_cast<Actor*>(this)->DoUpdate(ps);
            if (WasFailed(FailType::Data))
            {
                updateRes = AVS_SuspendedOrFailed;
                MG_DEBUGCODE(d_WTF = 3);
            }
        }

        MG_DEBUGCODE( dms_assert( lts == UpdateMarker::LastTS() ); )
        if (updateRes != AVS_SuspendedOrFailed) // DoUpdate can change m_LastState (to valid or failed) and thereby unlock itself
        {
            assert( !WasFailed(FailType::Data) );     // ClearFail(); not failed is implied by positive updateRes
            if (m_State.GetProgress() < ps) // DoUpdate can raise progress itself
                SetProgress(ps);
        }
        else
        {
            dms_assert(SuspendTrigger::DidSuspend() || WasFailed(ft));
        }
    }
    catch (const DmsException& x)
    {
        dms_assert(ps == ProgressState::Validated || ps == ProgressState::Committed);
        ft = (ps == ProgressState::Committed) ? FailType::Committed : FailType::Validate;
        if (!WasFailed(ft))
            DoFail(x.AsErrMsg(), ft);
        return AVS_SuspendedOrFailed;
    }
    if (WasFailed(ft) || (m_State.GetProgress() < ps) || SuspendTrigger::DidSuspend())
        return AVS_SuspendedOrFailed;
    return AVS_Ready;
}

// Non-suspendible wrapper: fences suspension and throws on failure.
// 'blockingAction' describes operation for external observers.
void Actor::CertainUpdate(ProgressState ps, CharPtr blockingAction) const
{
    SuspendTrigger::FencedBlocker lock(blockingAction);
    assert(ps >= ProgressState::Validated);
    ActorVisitState result = SuspendibleUpdate(ps);
    assert(m_State.GetProgress() >= ps || (result==AVS_SuspendedOrFailed));
    if (result==AVS_SuspendedOrFailed)
    {
        // trigger lock should have prevented the following situation
        assert(WasFailed(ps <= ProgressState::Validated ? FailType::Validate : FailType::Committed));
        assert(m_State.GetTransState() < static_cast<UInt32>(ps) * actor_flag_set::AF_TransientBase);
        ThrowFail();
    }
}

// Default no-op update body. Derived classes override this.
ActorVisitState Actor::DoUpdate(ProgressState ps)
{
    return AVS_Ready;
}

// Invalidation hook: (re)start supplier interest when having observers.
// NOTE: Keep noexcept behavior: failures convert into failure state.
void Actor::DoInvalidate () const
{
    assert(IsMetaThread());
    assert(!DoesHaveSupplInterest());
    assert(!WasFailed(FailType::Data));

//    GetLastChangeTS();
    if (m_InterestCount)
    {
        UpdateLock lock(this, actor_flag_set::AF_ChangingInterest);
        StartSupplInterest();
    }
    assert(!WasFailed(FailType::Data) || !DoesHaveSupplInterest());
    assert(DoesHaveSupplInterest() || !m_InterestCount || IsPassor() || WasFailed(FailType::Data));
}

// Default meta-info update is empty. Derived classes may override.
// NOTE: Must be noexcept to preserve state invariants during metadata refresh.
void Actor::UpdateMetaInfo() const noexcept
{}

// Propagate meta-info update to suppliers and record failures.
void Actor::UpdateSupplMetaInfo() const
{
    VisitSupplProcImpl(this, SupplierVisitFlag::UpdateSupplMetaInfo, [this](const Actor* supplier)
        {
            assert(supplier);
            supplier->UpdateMetaInfo();
            if (supplier->WasFailed())
                this->Fail(supplier);
        }
    );
}

#include "act/ActorVisitor.h"

// Aggregate the last change timestamp over suppliers and collect earliest (strongest) failure.
// TODO: Consider early-out if strong failure encountered to reduce traversal time.
TimeStamp Actor::DetermineLastSupplierChange(ErrMsgPtr& failReason, FailType& failType) const //noexcept
{
    // ===== collect change information from suppliers

    TimeStamp lastChangeTS = m_LastChangeTS;
    MakeMax(lastChangeTS, UpdateMarker::tsBereshit);
#if defined(MG_ITEMLEVEL)
    MakeMax(m_ItemLevel, item_level_type(1));
#endif //defined(MG_ITEMLEVEL)
        dms_assert( UpdateMarker::CheckTS(lastChangeTS) );

    try {
        VisitSupplBoolImpl(this, SupplierVisitFlag::DetermineState,
            [&](const Actor* supplier) -> bool
            {
                if (supplier->IsPassor() || supplier->m_State.IsDeterminingCheck())
                    return true;
            #if defined(MG_DEBUG_UPDATESOURCE)
                dms_assert( SupplInclusionTester::ActiveDoesContain(supplier) );
            #endif
                MakeMax(lastChangeTS, supplier->GetLastChangeTS()); // recursive call
#if defined(MG_ITEMLEVEL)
                MakeMax(this->m_ItemLevel, supplier->m_ItemLevel);
#endif //defined(MG_ITEMLEVEL)
                dms_assert(UpdateMarker::CheckTS( lastChangeTS ) );
                FailType supplFailType = supplier->GetFailType();
                if ((supplFailType != FailType::None) && (supplFailType < failType))
                {
                    failReason = supplier->GetFailReason();
                    failType   = supplFailType;
                }
                return true;
            }
        );
    }
    catch (...)
    {
        failReason = catchException(false);
        failType   = FailType::Determine;
    }

    return lastChangeTS;
}

// Rights checks are hooks for policy/ACL enforcement in derived classes.
void Actor::AssertPropChangeRights(CharPtr changeWhat) const
{}

void Actor::AssertDataChangeRights(CharPtr changeWhat) const
{}

// state info
// Mark this actor as a pass-through. Precondition: no interest yet.
void Actor::SetPassor()
{
    dms_assert(!GetInterestCount());       // PRECONDITION, SetPassor is set only at the construction context
    dms_assert(!DoesHaveSupplInterest() ); // IMPLIED BY previous assert
    m_State.SetPassor();
}

// Default visitor adapter to allow null-supplier shortcuts.
ActorVisitState ActorVisitor::Visit(const Actor* supplier) const
{
    if (!supplier)
        return AVS_Ready;
    return operator ()(supplier);
}

// Default supplier visitation entry point. Derived classes typically override.
ActorVisitState Actor::VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const
{
    return AVS_Ready;
}

// Update all suppliers to at least the given progress state.
// Fails/suspends if any supplier fails/suspends.
// TODO: Consider batching/rescheduling to avoid deep recursion for large graphs.
ActorVisitState Actor::UpdateSuppliers(ProgressState ps) const // returns US_Valid, US_UpdatingElsewhere, US_Suspended, US_FailedData, US_FailedCheck, US_FailedCommit
{
    assert((ps == ProgressState::Committed) || (ps == ProgressState::Validated));
    assert(ps == ProgressState::Committed); // TODO: clean-up if this holds

    if (!DoesHaveSupplInterest() && GetInterestCount())
        return AVS_Ready;

    FailType ft = (ps == ProgressState::Committed) ? FailType::Committed : FailType::Validate;

    assert(!WasFailed(FailType::MetaInfo));
    assert(!WasFailed(ft)); // precondition
    assert(DoesHaveSupplInterest() || !GetInterestCount());

    ActorVisitState updateRes = 
        VisitSupplBoolImpl(this, SupplierVisitFlag::Update,
            [this, ps, ft] (const Actor* supplier) -> ActorVisitState
                {
                    if (supplier->IsPassorOrChecked())
                        return AVS_Ready;
                    if (supplier->SuspendibleUpdate(ps)!=AVS_SuspendedOrFailed)
                    {
                        dms_assert(!SuspendTrigger::DidSuspend());
                        dms_assert(supplier->m_State.GetProgress() >= ps || supplier->IsPassor());
                    }
                    else
                    {
                        dms_assert(supplier->WasFailed(ft) || SuspendTrigger::DidSuspend()); // POSTCONDITION of SuspendibleUpdate
                        // first, propagate err
                        if (supplier->WasFailed(ft))
                            this->Fail(supplier);

                        // then, suspend or fail
                        return AVS_SuspendedOrFailed;
                    }
                    return AVS_Ready;
                }
        );
    if (WasFailed(ft))
        updateRes = AVS_SuspendedOrFailed;

    dms_assert(updateRes || WasFailed(ft) || SuspendTrigger::DidSuspend());
    return updateRes;
}

// RAII that pairs a DetermineChangeLock with a transient DeterminingState,
// and ensures m_LastGetStateTS is updated upon scope exit.
struct DetermineStateLock : UpdateMarker::DetermineChangeLock, Actor::UpdateLock
{
    DetermineStateLock(const Actor* self)
        :   Actor::UpdateLock(self, actor_flag_set::AF_DeterminingState)
        ,   m_Self(self)
    {
        dms_assert(self);
    }
    ~DetermineStateLock()
    {
        m_Self->m_LastGetStateTS = UpdateMarker::GetLastTS();
    }
private:
    const Actor* m_Self;
};

// Determine current state by examining suppliers' last change timestamps and failures.
// Handles invalidation if suppliers changed since last check.
// TODO: This function is central and long; consider refactoring into smaller pieces.
void Actor::DetermineState() const
{
    dbg_assert( !SuspendTrigger::DidSuspend() );

    dms_check_not_debugonly; 
    dms_assert(!std::uncaught_exceptions());
    dms_assert(m_LastGetStateTS <= UpdateMarker::LastTS()); // LastTS is last ts in system
//  dms_assert(!m_State.IsDeterminingState()); RECURSION IS CAUGHT BY recursionLock

    if (IsPassor())
        return;
    if (!IsMetaThread())
    {
        assert(UpdateMarker::IsInActiveState());
        return;
    }
    if (m_State.IsDeterminingCheck())
        return;

    if ( m_LastGetStateTS >= UpdateMarker::LastTS() ) // recursive Change Determinations disabled if no new timestamp was issued
        return;
    m_LastGetStateTS = UpdateMarker::LastTS(); // avoid missing WasFailed(US_Determine) within DetermineLastSupplierChange();

retry_from_here_after_invalidation:
    ErrMsgPtr failReason;
    FailType failType = FailType::None;
    TimeStamp lastSupplierChange = UpdateMarker::tsBereshit;
    {   
        DetermineStateLock recursionLock(this);    // doesn't really need stack space other than EH frame, will set m_LastGetStateTS at any exit of this frame.
        FencedInterestRetainContext irc("Actor::DetermineState()");
        m_LastGetStateTS = UpdateMarker::GetLastTS(); // avoid missing WasFailed(DetermineState) during DetermineLastSupplierChange

        // ===== collect change information from suppliers or a change triggered by specific derivation
        lastSupplierChange = DetermineLastSupplierChange(failReason, failType);
        // ===== disable any recursive Change Determinations until a new timestamp is issued
    }
    // ===== change if supplier changed after this instance last invalidation
    dms_assert(lastSupplierChange);
    if (m_LastChangeTS < lastSupplierChange)
    {
        if (m_LastChangeTS)
        {
            const_cast<Actor*>(this)->InvalidateAt(lastSupplierChange);
            goto retry_from_here_after_invalidation;
        }
        else
        {
            assert(m_State.GetProgress() == ProgressState::None || m_State.IsFailed()); //  Progress is only allowed after DetermineState, which implies m_LastChangeTS
            m_LastChangeTS = lastSupplierChange;
        }
        assert(m_LastChangeTS == lastSupplierChange);
    }
    assert(m_LastChangeTS);
    if (failType != FailType::None)
    {
        assert(failReason);
        DoFail(failReason, failType);
    }
}

#if defined(MG_DEBUG_DATA)

UInt32 Actor::GetNrCreations ()
{
    return sd_CreationCount;
}

UInt32 Actor::GetNrInstances ()
{
    return sd_InstanceCount;
}

#endif  // MG_DEBUG_DATA

//  -----------------------------------------------------------------------
//  Event       : Actor::MustSuspend
//  -----------------------------------------------------------------------
// C API bridges to suspension primitives.

extern "C" RTC_CALL bool DMS_CONV DMS_Actor_MustSuspend()
{
    return SuspendTrigger::MustSuspend();
}

extern "C" RTC_CALL bool DMS_CONV DMS_Actor_DidSuspend()
{
    return SuspendTrigger::DidSuspend();
}

extern "C" RTC_CALL void DMS_CONV DMS_Actor_Resume()
{
    SuspendTrigger::Resume();
}

//----------------------------------------------------------------------
// FailType
//----------------------------------------------------------------------
// Global critical section and association to cache failure reasons per-Actor.
// NOTE: The association holds ErrMsgPtr references; ensure lifecycle correctness.
leveled_critical_section sc_FailSection(item_level_type(0), ord_level_type::FailSection, "FailSection");
static_quick_assoc<const Actor*, ErrMsgPtr> s_ActorFailReasonAssoc;

// Clear failure state and drop associated message.
void Actor::ClearFail() const 
{
    if (WasFailed())
    {
        leveled_critical_section::scoped_lock syncFailCalls(sc_FailSection);

        auto errMsgPtr = s_ActorFailReasonAssoc.GetExisting(this);

        s_ActorFailReasonAssoc.eraseExisting(this);
        m_State.ClearFailed();
    }
}

// Retrieve failure reason (requires WasFailed()).
ErrMsgPtr Actor::GetFailReason() const
{
    dms_assert(WasFailed()); // Catch in debug

    leveled_critical_section::scoped_lock syncFailCalls(sc_FailSection);
    return s_ActorFailReasonAssoc.GetExistingOrDefault(this, ErrMsgPtr());
}

// Record a failure with type and message; may stop supplier interest on data failures.
// Emits report based on severity.
// TODO: Consider rate-limiting repeated reports for noisy graphs.
bool Actor::DoFail(ErrMsgPtr msg, FailType ft) const
{
#if defined(MG_DEBUG_INTERESTSOURCE_LOGGING)

    if (m_State.Get(actor_flag_set::AFD_PivotElem))
        reportF(SeverityTypeID::ST_MajorTrace, "DoFail(%d) %s", int(ft), msg->Why());

#endif

    assert(msg);
    assert(ft != FailType::None);
    SupplInterestListPtr supplInterestWaste;
    {
        RequestMainThreadOperProcessingBlocker saveNotificationAfterFail;
        leveled_critical_section::scoped_lock syncFailCalls(sc_FailSection);
        auto prevFT = GetFailType();
        if ((prevFT != FailType::None) && (prevFT <= ft))
            return false;

        assert(msg->Why().IsDefined() && !msg->Why().empty());

        s_ActorFailReasonAssoc.assoc(this, msg);
        m_State.SetFailure(ft);
        try {
            msg->TellWhere(dynamic_cast<const PersistentSharedActor*>(this));
            if (msg->MustReport())
            {
                auto st = ft <= FailType::Data ? SeverityTypeID::ST_Error : SeverityTypeID::ST_Warning;
                if (msg->m_FullName.empty())
                    reportD(st, msg->m_Why.c_str());
                else
                    reportF(st, "[[%s]] %s", msg->m_FullName, msg->m_Why);
            }
        }
        catch (...)
        {}

        // data generation is no longer needed
        if (ft <= FailType::Data)
            supplInterestWaste.init( MoveSupplInterest(this).release());
    }
    return true;
}

// Throw with the currently recorded failure.
void Actor::ThrowFail() const
{
    dms_assert(WasFailed());
    DmsException::throwMsg(GetFailReason());
}

// Overloads: record then throw.
void Actor::ThrowFail(ErrMsgPtr why, FailType ft) const
{
    DoFail(why, ft);
    ThrowFail();
}

void Actor::ThrowFail(SharedStr str, FailType ft) const
{
    ThrowFail(std::make_shared<ErrMsg>( str, dynamic_cast<const PersistentSharedActor*>(this) ), ft);
}

void Actor::ThrowFail(CharPtr str, FailType ft) const
{
    ThrowFail(SharedStr(str MG_DEBUG_ALLOCATOR_SRC("ThrowFail")), ft);
}

void Actor::ThrowFail(const Actor* src, FailType ft) const
{
    DoFail(src->GetFailReason(), ft); 
    ThrowFail();
}

void Actor::ThrowFail(const Actor* src) const
{
    Fail(src, src->GetFailType()); 
    ThrowFail();
}

// Catch and normalize any exception into a failure of type ft.
void Actor::CatchFail(FailType ft) const
{
    if (!WasFailed(ft))
        DoFail(catchException(true), ft);
}

// Record a failure with a string reason.
void Actor::Fail(WeakStr why, FailType ft) const
{
    assert((static_cast<UInt32>(ft) & static_cast<UInt32>(FailType::Mask)) == static_cast<UInt32>(ft)); // Syntax
    assert(ft != FailType::None);                 // PRE

    DoFail(std::make_shared<ErrMsg>( why, dynamic_cast<const PersistentObject*>(this) ), ft);

    assert(WasFailed()); // follows from PRE2 and m_State.SetBits
}

void Actor::Fail(CharPtr str, FailType ft) const
{
    Fail(SharedStr(str MG_DEBUG_ALLOCATOR_SRC("Actor::Fail")), ft);
}

void Actor::Fail(const Actor* src, FailType ft) const
{
    DoFail(src->GetFailReason(), ft); 
}

void Actor::Fail(const Actor* src) const
{
    auto failType = src->GetFailType();
    auto failReason = src->GetFailReason();
    assert(failType != FailType::None);
    assert(failReason);
    DoFail(failReason, failType); 
}

//----------------------------------------------------------------------
//  InterestCount management
//----------------------------------------------------------------------

//=============================== ConcurrentMap (client is responsible for scoping and stack unwinding issues)

#include "act/ActorLock.h"
#include "LockLevels.h"

// define global mappings
leveled_std_section sg_CountSection(item_level_type(0), ord_level_type::CountSection, "LockCountSection");
actor_section_lock_map sg_ActorLockMap("ActorLockMap");


#if defined(MG_DEBUG_INTERESTSOURCE)
THREAD_LOCAL UInt32 sd_DecInterestCount = 0;
UInt32 s_IntCount = 0;
#endif


// Increment interest count (0->1 starts interest).
// Uses global + per-actor lock to serialize the 0->1 transition while allowing fast >1 increments.
// TODO: Consider replacing some counting with atomics if lock ordering guarantees remain intact.
void Actor::IncInterestCount() const // NO UpdateMetaInfo, Just work on existing structures, callee StartInterest can throw when allocating new interestholders.
{
#if defined(MG_DEBUG_INTERESTSOURCE_LOGGING)
    bool isPivotedForLogging = m_State.Get(actor_flag_set::AFD_PivotElem);
    if (isPivotedForLogging)
        reportF(SeverityTypeID::ST_MajorTrace, "IncInterestCount %s", GetInterestCount());
#endif
    dbg_assert( !SuspendTrigger::DidSuspend() );
    {
        leveled_std_section::scoped_lock globalSectionLock(sg_CountSection);
        if (m_InterestCount)
        {
            ++m_InterestCount;
            assert(m_InterestCount); // assume no overflow
            return;
        }
    }

    assert(IsMetaThread()); // Starting Interest only allowed from main thread. From other threads, DataReadLocks may increase, but not initiate interest.

#if defined(MG_DEBUG_INTERESTSOURCE_LOGGING)
    DBG_START("IncInterestCount", "Actor", MG_DEBUG_INTERESTSOURCE_VALUE);
    DBG_TRACE(("Ptr %x Inc %d HasSuppl %d", this, GetInterestCount(), DoesHaveSupplInterest()));
    dbg_assert(!sd_DecInterestCount);
#endif

#if defined(MG_DEBUG_INTERESTSOURCE)
    DemandManagement::IncInterestDetector::CheckInc();
#endif

    UpdateLock lock(this, actor_flag_set::AF_ChangingInterest);

    DetermineState(); // Calls CreateDataObj that is triggered by m_InterestCount (not by KeepData? and not by DoesHaveSupplInterest() )

    actor_section_lock_map::ScopedLock specificSectionLock(MG_SOURCE_INFO_CODE("IncInterestCount") sg_ActorLockMap, this);
    // change 0-> 1 is a local critical section
    {
        leveled_std_section::scoped_lock globalSectionLock(sg_CountSection);
        // maybe another thread already completed Increment under a specificSectionLock
        if (m_InterestCount)
        {
            ++m_InterestCount;
            dms_assert(m_InterestCount); // assume no overflow
            return;
        }
        // no other specific incrementer before us; we have incremented and this thread must execute the resourcefull StartInterest()
    }

    dms_assert( !DoesHaveSupplInterest() ); // PRECONDITION IF m_InterestCount was 0 when specificSectionLock was obtained
    dms_assert( !DoesHaveSupplInterest() ); // invalidation
    try {
        StartInterest(); // thread-safe (should leave things unchanged when throwing) ?
    }
    catch (const DmsException& x)
    {
        DoFail(x.AsErrMsg(), FailType::MetaInfo);
        throw;
        
    }

    leveled_std_section::scoped_lock globalSectionLock(sg_CountSection);
    // we already tested if another thread already completed Increment under a specificSectionLock, which wasn't and stil isn't the case
    dbg_assert(!m_InterestCount);
    ++m_InterestCount;
    dbg_assert(m_InterestCount);
}

// Helpers to decrement counts atomically under global lock,
// distinguishing the last-decrement path.
bool DecCount(interest_count_t* interestCount)
{
    // only one thread gets the change to decrease to zero, but other thread might have increased it again.
    leveled_std_section::scoped_lock globalSectionLock(sg_CountSection);
    return -- * interestCount;
}

bool DecCountIfAboveZero(interest_count_t* interestCount)
{
    // only one thread gets the change to decrease to zero, but other thread might have increased it again.
    leveled_std_section::scoped_lock globalSectionLock(sg_CountSection);
    if (*interestCount <= 1)
        return false;
    -- * interestCount;
    return true;
}

// Decrement interest count (1->0 stops interest).
// NOTE: Not main-thread only; must avoid heavy operations here.
// TODO: Consider deferring StopInterest work to a safe executor when not main-thread.
garbage_can Actor::DecInterestCount() const noexcept // nothrow, JUST LIKE destructor
{
#if defined(MG_DEBUG_INTERESTSOURCE)
    DynamicIncrementalLock<> incInterestCountDetector(sd_DecInterestCount);
    bool isPivotedForLogging = m_State.Get(actor_flag_set::AFD_PivotElem);
    if (isPivotedForLogging)
        reportF(SeverityTypeID::ST_MajorTrace, "DecInterestCount %s", GetInterestCount());
#endif

    assert(m_InterestCount);

    if (!m_InterestCount) 
        return {}; // DEBUG, MITIGATION OF ISSUE

    if (DecCountIfAboveZero(&m_InterestCount))
        return {};

#if defined(MG_DEBUG_INTERESTSOURCE_LOGGING)
    DBG_START("DecInterestCount", "Actor", MG_DEBUG_INTERESTSOURCE_VALUE);
    DBG_TRACE(("Ptr %x Dec %d HasSuppl %d", this, GetInterestCount(), DoesHaveSupplInterest()));
#endif

    garbage_can garbage;
    {
        actor_section_lock_map::ScopedLock specificSectionLock(MG_SOURCE_INFO_CODE("Actor::DecInterestCount") sg_ActorLockMap, this);
        if (DecCount(&m_InterestCount))
            return {}; // another thread did take interest after DecCountIfAboveZero

        // change 1-> 0 is a local critical section
    //  UpdateLock updateLock(this, actor_flag_set::AF_ChangingInterest); Maybe this is not the main thread

#if defined(MG_DEBUG_INTERESTSOURCE)
        DemandManagement::IncInterestDetector lock("Actor::DecInterestCount()"); // removal of interest should work like a destructor: no tricky resource locking; just let it go.
#endif

        garbage = StopInterest();

        dbg_assert(m_InterestCount == 0);
    }
    return garbage;
}

RTC_CALL bool s_IsDetectingIncInterest = false;


// Lifecycle start when first observer appears.
// Starts supplier interest accordingly.
void Actor::StartInterest() const
{
    assert(IsMetaThread());

    assert(m_InterestCount == 0); // PRECONDITION guaranteed by IncInterestCount
    assert( !DoesHaveSupplInterest() ); // PRECONDITION

    MG_CHECK(!s_IsDetectingIncInterest);

    StartSupplInterest();
    assert(m_InterestCount == 0); // no recursion

#if defined(MG_DEBUG_INTERESTSOURCE)
    DemandManagement::AddTempTarget(dynamic_cast<const PersistentSharedActor*>(this));
#endif
}

// Lifecycle stop when last observer disappears.
// Moves supplier interest out and returns garbage_can for cleanup.
garbage_can Actor::StopInterest() const noexcept
{
#if defined(MG_DEBUG_INTERESTSOURCE)
    DemandManagement::ReleaseTempTarget(dynamic_cast<const PersistentSharedActor*>(this));
#endif
    if (SuspendTrigger::DidSuspend() && DoesHaveSupplInterest()) // suspension shouldn't cause loosing interest
        ReportSuspension();

    dms_assert(m_InterestCount == 0); // only stop on critical state change
    auto garbageCan = StopSupplInterest(); // remove interestcount from suppliers

    dms_assert(!DoesHaveSupplInterest());
    return garbageCan;
}

// keep atomic: ownership of an element in s_SupplTreeInterest and m_State.Get(actor_flag_set::AF_SupplInterest);
// StopSupplInterest can be called Concurrent

leveled_critical_section sc_MoveSupplInterestSection(item_level_type(0), ord_level_type::MoveSupplInterest, "MoveSupplInterestSection");

// Build a supplier interest list by visiting suppliers that are not passor/checked.
// NOTE: This can throw on allocation; callers should be prepared to handle.
SupplInterestListPtr Actor::GetSupplInterest() const
{
    SupplInterestListPtr supplInterestListPtr;
    VisitSupplProcImpl(this, SupplierVisitFlag(SupplierVisitFlag::StartSupplInterest),
        [&supplInterestListPtr] (const Actor* supplier)
        {
            if (!supplier->IsPassorOrChecked())
				if (auto ps = dynamic_cast<PersistentSharedActor*>(const_cast<Actor*>(supplier)))
                    push_front(supplInterestListPtr, ps);
        }
    );
    return supplInterestListPtr;
}

// Start supplier interest:
// - Build interest list
// - Increment remaining target count (for global bookkeeping)
// - Register the list in global tree and set flag
void Actor::StartSupplInterest() const
{
    assert(IsMetaThread());

    if (IsPassor() || WasFailed(FailType::Data))
        return;

    assert(!DoesHaveSupplInterest() ); // PRECONDITION
    assert(m_State.GetBits(actor_flag_set::AF_TransientMask) == actor_flag_set::AF_ChangingInterest); // PRECONDITION

    //UpdateSupplMetaInfo();
    SupplInterestListPtr supplInterestListPtr = GetSupplInterest(); // can throw

    assert(!IsPassor()); // follows from previous if.
    IncRemainingTargetCount(this); // can throw
    auto undoTargetCount = make_releasable_scoped_exit([this]() { DecRemainingTargetCount(this); });

    leveled_critical_section::scoped_lock scopedLock(sc_MoveSupplInterestSection);
    if (!s_SupplTreeInterest)
        s_SupplTreeInterest.assign ( new SupplTreeInterestType );

    SupplInterestListPtr& supplInterestListRef = (*s_SupplTreeInterest)[this]; // can insert new and throw bad_alloc
    assert(!supplInterestListRef);

    assert(!DoesHaveSupplInterest()); // POSTCONDITION

    // nothrow from here
    if (!WasFailed(FailType::Data))
    {
        supplInterestListRef.init(supplInterestListPtr.release());
        m_State.Set(actor_flag_set::AF_SupplInterest);
        undoTargetCount.release();
        assert(DoesHaveSupplInterest()); // POSTCONDITION
    }
}

// Rebuild supplier interest if it exists, swapping out old for new list.
void Actor::RestartSupplInterestIfAny() const
{
    assert(IsMetaThread());

    if (!DoesHaveSupplInterest())
        return;

    SupplInterestListPtr supplInterestListPtr = GetSupplInterest(); // can throw
    SupplInterestListPtr oldSupplInterestListPtr;
    leveled_critical_section::scoped_lock scopedLock(sc_MoveSupplInterestSection);

    if (!DoesHaveSupplInterest())
        return;

    assert(s_SupplTreeInterest); // guaranteed by lock to follow from DoesHaveSupplInterest.
    SupplInterestListPtr& supplInterestListRef = (*s_SupplTreeInterest)[this]; // can insert new and throw bad_alloc

    // nothrow from here
    if (WasFailed(FailType::Data))
    {
        assert(!DoesHaveSupplInterest()); // reset by Failure in GetSupplInterest();
    }
    else
    {
        oldSupplInterestListPtr.init( supplInterestListRef.release() );
        supplInterestListRef.init(supplInterestListPtr.release());
//      m_State.Set(actor_flag_set::AF_SupplInterest);
        assert(DoesHaveSupplInterest()); // still set.
    }
}

// Move out supplier interest list for this actor (if any).
// Clears AF_SupplInterest flag and decrements global remaining target count.
// NOTE: Protected by sc_MoveSupplInterestSection.
SupplInterestListPtr MoveSupplInterest(const Actor* self)
{
    SupplInterestListPtr localInterestHolder;
    if ( self->DoesHaveSupplInterest() )
    {
        RequestMainThreadOperProcessingBlocker saveNotificationAfterMove;

        leveled_critical_section::scoped_lock scopedLock(sc_MoveSupplInterestSection);

        if ( self->DoesHaveSupplInterest() ) // 2nd check after lock
        {
            dms_assert(s_SupplTreeInterest);

            SupplTreeInterestType::iterator ptr= s_SupplTreeInterest->find(self);
            dms_assert(ptr != s_SupplTreeInterest->end());
            dms_assert(ptr->first == self);

            localInterestHolder.init( ptr->second.release() );
            s_SupplTreeInterest->erase(ptr);
            if (s_SupplTreeInterest->empty())
                s_SupplTreeInterest.reset();

            if (!self->IsPassor())
                DecRemainingTargetCount(self);
            self->m_State.Clear(actor_flag_set::AF_SupplInterest);
        }
#if defined(MG_DEBUG)
        else
        {
            reportD_without_cancellation_check(SeverityTypeID::ST_MinorTrace, "Concurrent MoveSupplInterest happening");
        }
#endif
        assert( !self->DoesHaveSupplInterest() ) ; // POSTCONDITION
    }

    return localInterestHolder;
}


// Stop Suppl interest must be called when
// a. interest count drops to zero
// b. DataController:  Data is generated (caused by 1. Commit data and/or 2. usage from consumer, including a validation rule (in that specific order)
//    if data is not generated, it might still be generated later since there is still interest
// c. failure in meta info (no data can be generated); other failures might still cause future data generation requests unless it was already generated.
// d. TreeItem that became PS_Committed; no further action will be asked on this nor its suppliers unless other counted routes exist (through different DataControllers)

// Stop supplier interest and return the garbage_can for deferred cleanup.
garbage_can Actor::StopSupplInterest() const noexcept
{
    garbage_can garbage;
    garbage |= MoveSupplInterest(this);
    return garbage;
}

// Cheap check for supplier-interest flag.
bool Actor::DoesHaveSupplInterest() const noexcept
{
    return m_State.Get(actor_flag_set::AF_SupplInterest);
}

MG_DEBUGCODE(

bool Actor::IsShvObj() const
{
    return false;
}


)

// Destroy list by iterative two-step forwarding to avoid creating self-owned cycles.
// Ensures proper deletion of list elements and their values.
SupplInterestListPtr::~SupplInterestListPtr()
{
    while (static_cast<bool>(get()))
    {
        // two step forwarding to prevent the creation of self-owned leaked SupplInterestListElem's
        assert(get()->m_Value); // class invariant
        base_type::reset( release_ptr(get()->m_NextPtr) );  // acquire nextPtr and destroy currently owned SupplInterestListElem, which includes destructing its m_Value
    }
}

// Obtain a shared-interest handle if this actor is currently observed.
// NOTE: Increments interest count while returning shared pointer wrapper.
SharedActorInterestPtr Actor::GetInterestPtrOrNull() const
{
    assert(this);

	auto psa = dynamic_cast<const PersistentSharedActor*>(this);
    assert(psa);
    if (!psa)
		return {};  

    assert(psa->IsOwned());

    leveled_std_section::scoped_lock globalSectionLock(sg_CountSection);
    if (!m_InterestCount)
        return {};

    auto result = SharedPtr<const PersistentSharedActor>(psa, existing_obj{});

    assert(m_InterestCount);
    ++m_InterestCount;

    return SharedActorInterestPtr(std::move(result), already_incremented_tag{});
}

#if defined(MG_ITEMLEVEL)

// Item-level aggregation utility; defaults to supplier-derived level.
item_level_type GetItemLevel(const Actor* act)
{
    if (!act || act->IsPassor())
        return item_level_type(0);
/*
    if (act->m_ItemLevel == item_level_type(0))
        act->DetermineState();
    dms_assert(act->m_ItemLevel != item_level_type(0));
*/
    return act->m_ItemLevel;
}

#endif

// Traverse up parent chain to see if any ancestor is failed.
bool WasInFailed(const Actor* a)
{
    assert(a);
    if (a->WasFailed())
        return true;
    auto po = dynamic_cast<const PersistentSharedActor*>(a);
    if (!po)
        return false;
    auto p = po->GetParent();
    if (!p)
        return false;
    return WasInFailed(dynamic_cast<const PersistentSharedActor*>(p));
}

// Phase numbers provide a simple topological-like ordering among actors.
// NOTE: first_phase_number starts at 1 to distinguish 'unset' from 'set'.
const phase_number first_phase_number = 1;

// Assign a phase number >= max(suppliers' phase numbers, 1).
// TODO: Consider caching invalidation policy when suppliers change.
void AssignPhaseNumber(const Actor* item) noexcept
{
    assert(item);

    if (item->m_PhaseNumber)
        return;

    item->m_PhaseNumber = first_phase_number;

    try {
        VisitSupplProcImpl(item, SupplierVisitFlag::FenceNumberScan, [item](const Actor* suppl)
            {
//              assert(suppl->m_State.GetProgress() >= ProgressState::ProgressState::MetaInfo);
//              suppl->UpdateMetaInfo();
                MakeMax<phase_number>(item->m_PhaseNumber, suppl->GetPhaseNumber());
            }
        );
    }
    catch (...) {}
}

// Public accessor that ensures assignment and returns cached phase number.
auto Actor::GetPhaseNumber() const -> phase_number
{
    assert(IsMainThread());
//  assert(m_State.GetProgress() >= ProgressState::ProgressState::MetaInfo);
    AssignPhaseNumber(this);
    return m_PhaseNumber;
}

/*
High-level Suggestions (non-exhaustive):
- Concurrency:
  - TODO: Review lock order (sg_CountSection, sg_ActorLockMap, sc_MoveSupplInterestSection, sc_FailSection) to document a deadlock hierarchy.
  - TODO: Explore using std::atomic for m_InterestCount with fetch_add/sub to shrink critical sections where feasible.
  - TODO: Ensure no callbacks that may re-enter Actor code are made while holding global locks.

- Complexity/Readability:
  - TODO: Extract sub-steps from SuspendibleUpdate and DetermineState to helpers to reduce cyclomatic complexity and improve testability.
  - TODO: Add structured logging (with IDs) to track update and invalidation flows for large graphs.

- Failure handling:
  - TODO: Consider throttling repeat error reports and deduplicating messages.
  - TODO: Make GetFailReason/DoFail behavior clearer with stronger guarantees around message lifetime.

- API clarity:
  - TODO: Clarify semantics between Was vs Is vs WasFailed vs IsFailed in documentation and possibly rename for discoverability.

- Interest management:
  - TODO: Consider centralizing supplier-interest tree ownership to a dedicated component for testability and lifecycle isolation.
*/
