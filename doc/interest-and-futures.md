# Interest vs. futures: demand registration and the `PrepareDataUsage` contract

Design analysis of the relation between *being interesting*, `PrepareDataUsage`, and
`FutureData`, answering the question whether interest should be foldable into a future
returned by `PrepareDataUsage`. Based on code review of the `refactor_ownership` branch
(rtc, shv, qtgui), 2026-07-11.

Companion notes:
- [Interest.md](Interest.md) — the interest/readiness/failure contract as pinned by #1144.
- [incremental-updates.md](incremental-updates.md) — invalidation, timestamping, external
  change detection; section 5 below interacts directly with that plan.

---

## 1. The question

An item moves through these demand/calculation states:

1. **owned** — exists in the config/cache tree, no demand;
2. **interesting** — `m_InterestCount > 0`;
3. **PrepareDataUsage called** — an `OperationContext` (OC) exists; requires interest;
   from here on `IsCalculatingOrDataReady || WasFailed(FailType::Data)`;
4. OC `waiting_for_suppliers` → `scheduled` → `activated` → `running`;
5. OC `done`, data ready; the OC may be removed and the data survives as long as the item
   is interesting or `KeepData`,

with failure as a possible exit at every step.

Proposal under consideration: let `PrepareDataUsage` return a `SharedTreeItemInterestPtr`
that guarantees `IsCalculatingOrDataReady || WasFailed(FailType::Data)`, and make that the
**only** way a `TreeItem` becomes interesting — i.e. fold "announced demand" into "future to
primary data". Sub-questions: what guarantees should `FutureData` provide, does it provide
them today, and are `InterestPtr<DataControllerRef>` objects created without
`FuncDC::CallCalcResult` having been called?

**Conclusion up front:** keep the two states, but re-draw the boundary. Interest is the
*mutable subscription*; `FutureData` should be an *immutable snapshot of one evaluation
round*; `PrepareDataUsage` is the transition that consumes a subscription plus the current
source state and emits a round. Folding subscription into round does not remove the
announce state — it reappears inside the future object — while breaking five mechanisms
that key off the interest count (section 4). The real gains the proposal is after
(type-level honesty of `FutureData`, scheduler visibility of demanded work) are obtainable
without the fold (section 6).

## 2. Current semantics — what the code enforces

### 2.1 Interest is a precondition of `PrepareDataUsage`, not its effect

- `PrepareDataCalc`: `dms_check(self->HasInterest())`
  (`rtc/dll/src/tic/TreeItem.cpp:3658`) before `dc->CallCalcResult()`.
- `PrepareDataUsageImpl`: `assert(GetInterestCount() || !IsDataItem(this))`
  (`rtc/dll/src/tic/TreeItem.cpp:3947`); `PrepareDataRead` re-checks per refItem and
  domain/values unit (`TreeItem.cpp:3804,3809,3821`).
- Clients follow interest-first-then-prepare:
  `GraphicObject::PrepareDataOrUpdateViewLater` asserts interest, takes its own holder,
  then calls `PrepareDataUsageImpl` (`shv/dll/src/GraphicObject.cpp:291-315`);
  `NumericDataItem_GetStatistics` has `assert(item->HasInterest()); // PRECONDITION`
  (`clc/dll/src/GetStatistics.cpp:356`); `ItemUpdateImpl` sets `holder = self` before
  `Update` (`rtc/dll/src/tic/TicInterface.cpp:611-623`).

### 2.2 What becoming interesting does structurally

`Actor::IncInterestCount` serializes the 0→1 transition, runs `DetermineState()` first,
then `StartInterest()` (`rtc/dll/src/act/Actor.cpp:1108-1173`). For a `TreeItem`
(`rtc/dll/src/tic/TreeItem.cpp:4616-4657`), `StartInterest`:

- holds the **tree parent** (interest propagates *up* the ownership chain),
- holds **`mc_DC`** (`SharedActorInterestPtr calcHolder`) and the **referred item**,
- notifies the storage manager (`NonmappableStorageManager::StartInterest`),
- runs `Actor::StartSupplInterest` → `GetSupplInterest` visits all
  `SupplierVisitFlag::StartSupplInterest` suppliers and retains each non-passor
  (`rtc/dll/src/act/Actor.cpp:1285-1339`): std-managed suppliers by a weak interest,
  DataController suppliers by an *owning* intrusive interest
  (`rtc/dll/src/act/SupplInterest.h:20-33`).

So interest propagates transitively through the supplier graph **without any
`PrepareDataUsage`/`CallCalcResult` involvement**, and DC interest cascades further:
`TreeItemDualRef::StartInterest` takes data-interest on the result item `m_Data`
(`rtc/dll/src/tic/DataController.cpp:260-274`).

### 2.3 The `CallCalcResult` contract

`FuncDC::CallCalcResult` (`rtc/dll/src/tic/MoreDataControllers.cpp:360-469`):

- entry: `DetermineState()` — *"may trigger DoInvalidate → reset m_Data"* (`:371`);
- self-charges: `FutureData thisFutureResult = this` (`:381`) **before** the
  `assert(GetInterestCount())` at `:428`/`:443` — those asserts are therefore satisfied by
  the function's own local holder and do **not** enforce a caller-side interest
  precondition (same for the base `DataController::CallCalcResult`,
  `rtc/dll/src/tic/DataController.cpp:482-483`);
- `mustStartCalc` is decided by `IsAllInterestedCalculatingOrDataReady(curr)` /
  `IsAllDataCurrStandby(curr)` (`:449-452`);
- `CallCalcResultImpl` establishes `SupplInterest` itself if absent (`:762-766`), gets arg
  futures via `GetArgs(false, true)`, creates/reuses the OC, and schedules
  (`:790-805`);
- postcondition of a non-null return (`:466`):
  `m_OperContext || IsDataReady(curr) || curr->WasFailed(FailType::Data) || DidSuspend`
  — exactly the state-3 contract, **at the moment of return**.

### 2.4 How the guarantee is (and is not) maintained afterwards

- The returned future keeps the DC (and via 2.2 the result item) of-interest;
  `OperationContext::CancelIfNoInterestOrForced` cancels an OC only when
  `m_Result->GetInterestCount() == 0` (or forced by session teardown via
  `DSM::IsCancelling`, `rtc/dll/src/tic/OperationContext.cpp:1737-1750`,
  `rtc/dll/src/tic/DataStoreManager.cpp:42`). So *interest already is the OC survival
  contract*.
- The guarantee is **not invariant under invalidation**: `DetermineState` can
  `DoInvalidate → reset m_Data` even while futures are outstanding. The comments on
  `OperationContext::m_KeptResultUnits` / `m_KeptArgUnits` / `m_KeptArgInterests`
  (`rtc/dll/src/tic/OperationContext.h:322-341`) document the consequences — *"a
  meta-thread DoInvalidate→Clear() during the calculation frees the kept-alive units while
  a worker still reaches them"*, *"nothing on the invalidation path is consumer-aware"* —
  and are targeted, per-OC pinning patches for exactly the hole a real future guarantee
  would close (see R2).

### 2.5 `FutureData` is two things wearing one type

`FutureData = InterestPtr<DataControllerRef>` (`rtc/dll/src/tic/TicBase.h:114`). It is
used in two roles the type cannot distinguish:

**(a) charged future** — the non-null return of `CallCalcResult` (postcondition 2.3);
these are what `FutureSuppliers`/`ArgRef` (`rtc/dll/src/tic/OperGroups.h:32-37`) and
`OperationContext::m_KeptArgInterests` are meant to hold.

**(b) bare DC-interest retainer** — created *without* `CallCalcResult`:

| site | pattern |
|---|---|
| `rtc/dll/src/tic/TreeItem.cpp:3651` | `FutureData dc = self->GetCheckedDC();` before `dc->CallCalcResult()` at `:3662` |
| `rtc/dll/src/tic/TreeItem.cpp:3750,3813,3825` | `FutureData tmpFut = dc; // hold interest while obtaining the future` |
| `rtc/dll/src/tic/MoreDataControllers.cpp:566` | `FutureData fd = argIter->m_DC; fd = ...CalcResultWithValuesUnits();` |
| `rtc/dll/src/tic/AbstrCalculator.cpp:1092` | `FutureData dc = GetOrCreateDataController(...)` then `CalcResultWithValuesUnits` |
| `rtc/dll/src/tic/AbstrCalculator.cpp:193` | `CalledCalcHandle` returns `dc` as `calc_result_t` on the MetaInfo-failed path — a "settled-failed" future by convention only |
| `rtc/dll/src/tic/Explain.cpp:49,396,666` | `CalcInterestPtr`; `m_CalcInterests.push_back(dc)` holds every sub-expression DC long-lived; `CallCalcResult(context)` follows only later, per explained coordinate |
| `shv/dll/src/Theme.cpp:474-475` | `GetCheckedDC()` pre-charge, then `CalcResultWithValuesUnits`/`CallCalcResult` |
| `shv/dll/src/IndexCollector.h:44` | `SharedDcInterestPtr` member; `m_DC->CallCalcResult()` lazily (`IndexCollector.cpp:116`) |

So the answer to *"are `InterestPtr<DataControllerRef>` objects created without having
called `FuncDC::CallCalcResult`?"* is **yes, systematically** — transient pre-charge
idioms *and* long-lived retainers (`Explain`, `IndexCollector`). The announce-then-request
pattern exists at the DC level too, and today only convention (variable position in the
code) says which role a given `FutureData` plays.

### 2.6 Interest is a selector, not just a refcount

- **Which sub-results must be produced:** `IsAllInterestedCalculatingOrDataReady`
  (`rtc/dll/src/tic/ItemLocks.cpp:646-685`) walks a multi-item cache result and demands
  calculating-or-ready **only for sub-items with interest**. A single per-item future
  cannot express this; the per-sub-item interest count is the demand mask.
- **Which OCs survive:** `CancelIfNoInterestOrForced` (2.4).
- **Which data survives OC removal:** `StopInterest` → `TryCleanupMem` unless `KeepData`
  (`rtc/dll/src/tic/TreeItem.cpp:4659-4700`), including the interest-scoped release of
  parked read-OCs (`TSF_ReadAssetsInterestScoped`).
- **What gets idle-time preparation:** `TryPrepareDataUsage` starts with
  `if (!GetInterestCount()) return true;` (`rtc/dll/src/tic/TreeItem.cpp:4175-4186`) —
  interest is the prefetch filter. (Note: on this branch `TryPrepareDataUsage` currently
  has no callers; the shv components call `PrepareDataUsage` directly from their update
  rounds.)
- **UI pending state:** the Qt tree view colours "interesting but not ready" as
  `st_scheduled` by piggy-backing via `GetInterestPtrOrNull`
  (`qtgui/exe/src/DmsTreeView.cpp:303-313`), which deliberately returns null when no one
  else holds interest (`rtc/dll/src/tic/TreeItem.cpp:832-845`).

### 2.7 Interest survives invalidation; futures don't

`TreeItem::SetDC` (`rtc/dll/src/tic/TreeItem.cpp:789-815`): when an interested item's DC
is replaced (invalidation, expression change), the interest is *transferred* —
`newDC->IncInterestCount()`, old DC released. The subscription outlives the round. A
standing view stays demanded across a source change and the next GUI update round
re-prepares it; nobody has to re-subscribe.

### 2.8 Where the OC dependency graph comes from

`OperationContext::connectArgs` (`rtc/dll/src/tic/OperationContext.cpp:1856-1902`) maps
each `FutureData` in `FutureSuppliers` to its supplier OC (`FuncDC::GetOperContext()`, or
`GetOperationContext(rangeItem)` for non-FuncDCs) and `connect`s waiter↔supplier
(`:840-852`); `OperationContext_scheduleThis` then derives `waiting_for_suppliers`
(`:1299-1330`). So the scheduler's edge set is already built **from the futures vector**;
`m_Waiters`, `m_PhaseNumber` and the per-phase queues (`scheduleRunnableTask`, `:855-870`)
are the existing raw material for prioritisation.

## 3. Refinement of the Interest.md TL;DR

[Interest.md](Interest.md) states *"interest == a `std::shared_future` on the data"* and
immediately qualifies it: *"Interest + `PrepareData()` = start the future."* This note
makes that two-phase structure explicit, because the two phases have different lifetimes
and different invariants:

- **Interest = subscription.** Mutable, refcounted, transitive (2.2), survives
  invalidation (2.7), selects (2.6). Its invariant is *retention*: the demanded object
  graph stays alive and preferred-in-cache.
- **FutureData = one evaluation round.** Created by `CallCalcResult` from a subscription
  plus the current source state; its invariant should be *forward-only progress* of that
  round: calculating → ready, or → failed — never regression (see R2; today this holds
  only absent invalidation).
- **`PrepareDataUsage` = the transition** from subscription to round(s), suspendible and
  repeatable. It consumes interest (asserts in 2.1) and produces charged futures; it does
  not create the subscription.

## 4. Why "only `PrepareDataUsage` sets interest" does not simplify

1. **The meta-phase gap.** A future guaranteeing calculating-or-ready can only exist
   *after* `UpdateMetaInfo` / `MakeResult` / template instantiation — suspendible,
   meta-thread, repeatable work. During that window the demand must already be registered
   so suppliers, arg DCs, cache subtrees and storage sessions stay alive (2.2). A
   fold-in therefore needs a "future that isn't calculating yet" for this window — the
   announce state reappears inside the future object. The state moves; it does not vanish.
2. **The selector roles (2.6) need an aggregatable count on the item**, visible to code
   that holds no client handle: completion criteria for multi-item results, OC
   cancellation, retention, prefetch filtering, UI state. One-holder future handles
   cannot answer "does anyone still want this sub-item?".
3. **Invalidation must break rounds without erasing demand (2.7).** With futures as the
   only demand registration, every `DoInvalidate` (including every external-change tick
   once `DetermineExternalChange` is re-activated — see
   [incremental-updates.md](incremental-updates.md)) would drop all standing demand and
   push a re-subscribe burden into every client. The current transfer-on-`SetDC` semantics
   is the right one for a reactive update loop.
4. **The suspendible GUI loop needs standing demand between rounds.** `SuspendibleUpdate`
   rounds return without a future when suspended; meanwhile OCs must not be cancelled —
   which is exactly what the client's standing interest prevents (2.4).
5. **DataControllers and other actors already get interest transitively** (SupplInterest,
   `mc_DC` holds, `Explain`/`IndexCollector` retainers). Restricting *TreeItem* interest
   to `PrepareDataUsage` would leave all of that in place — the announce state would
   remain, just inconsistently reachable.

What *can* be tightened is the **root discipline**: every root interest holder today is a
view/theme, an export/commit target, a statistics/explain context, or an in-flight
prepare — all of them "will request data soon". That contract can be made auditable
(R3) without changing who may subscribe.

## 5. What guarantees should `FutureData` provide?

Target invariants for a **charged** future (non-null return of `CallCalcResult`):

1. **Aliveness/retention** — the DC, its result subtree, and their data-interest stay
   held. *Status: provided* (InterestPtr + `TreeItemDualRef::StartInterest`).
2. **Settledness at creation** — result `IsCalculatingOrDataReady || WasFailed(FR_Data)`.
   *Status: provided at return time* (postcondition assert
   `MoreDataControllers.cpp:466`); *not encoded in the type* — bare retainers (2.5b) are
   indistinguishable.
3. **Forward-only progress while held** — the round may complete or fail, never regress
   to not-ready. *Status: NOT provided.* Holds in the common case via the interest-based
   cancellation policy, but `DoInvalidate` is not consumer-aware (2.4); the `m_Kept*`
   members are per-OC patches. Session teardown (`forced` cancel) is a legitimate
   exception and should be an explicit, observable settled-state (`cancelled`).
4. **Staleness is detectable, not silent** — after invalidation the future should report
   "superseded", not dangle over a cleared `m_Data`. *Status: NOT provided* (`GetOld()`
   simply reads the current, possibly reset, `m_Data`).

## 6. Recommendations

**R1 — Split the type so the compiler answers "was `CallCalcResult` called".**
Reserve the name `FutureData` for charged futures: private constructor, friended to
`DataController::CallCalcResult` / `FuncDC::CallCalcResult`, plus a `settled-failed`
factory for the `CalledCalcHandle` MetaInfo-failed path. Reintroduce the bare retainer
under its own name — shv already defines it: `SharedDcInterestPtr`
(`shv/dll/src/IndexCollector.h:44`). Convert the pre-charge idioms, `Explain`'s
`m_CalcInterests`, and the SupplInterest DC arm to the retainer type. Zero behaviour
change; the 2.5b table becomes compiler-enforced. The self-satisfied
`assert(GetInterestCount())`s become meaningful again: charge via the retainer first,
assert, then construct the future.

**R2 — Formalize invariant 3/4 with generational pinning.**
Rule: `DoInvalidate` on a DC with outstanding charged futures does not clear the result in
place; it *detaches* it (the old generation stays owned by the futures, marked
superseded) and new demand creates a new generation. This generalizes
`m_KeptResultUnits`/`m_KeptArgUnits`/`m_KeptArgInterests` from three ad-hoc snapshots into
one rule, makes "consumer-aware invalidation" the definition instead of the missing
feature, and gives [incremental-updates.md](incremental-updates.md) a safe answer to
"what if a source file changes mid-calculation": the running round completes against its
pinned generation; the subscription triggers a fresh round against the new one.

**R3 — For throttling/prioritisation, make demand enumerable; don't merge the states.**
The scheduler needs two frontiers:
- *scheduled work*: OCs + edges — already present (`m_Suppliers`/`m_Waiters` built from
  `FutureSuppliers` in `connectArgs`, `m_PhaseNumber`, per-phase queues, 2.8);
- *demanded-but-unplanned work*: items/DCs whose interest exceeds their internal holds and
  that have no OC yet — i.e. precisely "interesting, `PrepareDataUsage` not yet called".

That second frontier is a *feature* for resource-aware scheduling: it is the backlog that
can be deferred, batched, or prioritised **before** committing memory to an OC's arg
pinning. Folding interest into `PrepareDataUsage` would force every demand to become a
scheduled OC immediately — the opposite of throttling. Concretely: keep a registry of
interest roots (or an interested-without-OC index next to `s_SupplTreeInterest`) and let
priorities flow along `m_Waiters` edges; `CancelIfNoInterestOrForced` remains the
back-pressure release valve.

**R4 — Optionally give the `Certain` path a real future.**
`PrepareDataUsage(DrlType::Certain)` returning a `SharedTreeItemInterestPtr` built on the
OC's `m_Result` (with invariants 1–4) removes the caller mistake of not holding interest
across `WaitForReadyOrSuspendTrigger`. Keep the suspendible `bool` variant for the GUI
loop, where the caller already owns a standing subscription and a per-call future would
only churn the count. `FuncDC::GetArgs` already demonstrates the two-level protocol
working internally: subscription asserted (`DoesHaveSupplInterest`,
`MoreDataControllers.cpp:548`) + per-arg charged futures (`:566-573`).

## 7. Answers in brief

- **Should `PrepareDataUsage` be the only way to become interesting?** No. The announce
  state is load-bearing in five places (meta-phase retention, sub-result selection, OC
  cancellation policy, data retention, invalidation survival) and would be reinvented
  inside the future. Tighten the boundary instead: R1 (type honesty) + R2 (real future
  invariants) + R3 (enumerable demand for the scheduler).
- **What should a `FutureData` guarantee?** Section 5: aliveness, settled-at-creation,
  forward-only progress, detectable staleness — the last two require R2.
- **Does it already?** Aliveness yes; settledness only at return time and only by
  convention; forward-only progress and staleness no (`DoInvalidate` is not
  consumer-aware; `m_Kept*` are point patches).
- **Are `InterestPtr<DataControllerRef>` created without `FuncDC::CallCalcResult`?**
  Yes — see the table in 2.5b: transient pre-charge holders, long-lived retainers
  (`Explain`, `IndexCollector`), and one settled-failed return path.
