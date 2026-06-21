# Interest, Readiness, and Failure

Architectural notes on what an *interest* on a `TreeItem` guarantees, how it
relates to data readiness and failure, and the invariants the calculation /
locking machinery relies on. Derived from the analysis and fix of issue #1144
(GeoDMS_2026, Jun 2026).

## TL;DR — the interest contract

An **InterestHolder** (`SharedTreeItemInterestPtr` / a non-zero
`m_InterestCount`) on an item is a *shared-future contract* over that item's
data. While at least one interest is held, the item is in exactly one of:

- **calculating** — a producer (`OperationContext`, via `m_Producer`) is or will
  be making it ready; or
- **ready** — `m_DataObject` is present (`IsDataReady` / `GetCurrRefObj`); or
- **failed, with its *actual* failtype recorded** — and if the failure means the
  data could not be produced, that failtype is `<= FailType::Data`
  (`None`/`Determine`/`MetaInfo`/`Data`), so `WasFailed(FailType::Data)` is true.

In other words: **interest == a `std::shared_future` on the data.** Interest +
`PrepareData()` = "start the future"; `WaitReady`/`Join` = "`.get()`"; the value
is either produced or the future holds a failure whose recorded failtype tells
*why*. The cardinal rule: **the recorded failtype must be the actual reason,
never one forced by which *role* a failing supplier played.**

## Three orthogonal supplier roles

An item's suppliers fall into roles that are **independent**, not sequential
stages:

- **DataPrep** — needed to compute the primary data.
- **Validation** — inputs to an IntegrityCheck.
- **Commit** — e.g. the expression that yields the storage name.

A failure in a *validation* or *commit* supplier says nothing about whether the
primary data can be computed, and a *data* supplier can itself carry a
`Validate` failure (its own integrity check failed) that this item should
inherit even though this item's data isn't calculated yet. So there is **no**
linear "metainfo → data → validate → commit, fail only after the previous level"
order across suppliers. The bug in #1144 was code that assumed exactly that
order and forced an item's failtype to a *role's* stage.

## `FailType` ordering

`rtc/dll/src/act/ActorEnums.h`:

```
None = 0  <  Determine = 4  <  MetaInfo = 8  <  Data = 12  <  Validate = 16  <  Committed = 20
```

`Actor::WasFailed(FailType fr)` returns true iff failed and `GetFailType() <= fr`.
So `WasFailed(FailType::Data)` is **true** for a `MetaInfo` failure (`8 <= 12`)
— a metainfo/type error is a valid "why the data is unavailable" — and **false**
for `Validate`(16)/`Committed`(20).

That asymmetry in `WasFailed(fr)` is real, but the read lock **no longer keys off
it**. The lock's rule is simpler: *calculating-or-ready → proceed; otherwise the
item is failed → rethrow its actual error* (see next section). The failtype still
governs **where the error surfaces**:

- A genuine "could-not-produce-data" failure is `<= Data` (a `Determine`,
  `MetaInfo`, or `Data` error): the item is data-less and the lock rethrows it.
- A `Validate` failure means computed data failed a check — the data exists and
  the item is *ready*, so the lock proceeds (the calculating-or-ready clause
  passes). `Committed` likewise: succeeded data that failed to persist is still in
  memory, again *ready*.

The state that used to crash — **data-less + not-calculating + failtype > Data** —
no longer asserts: the lock rethrows that item's `Validate`/`Committed` error like
any other failure. #1144's bug was *producing* that state by forcing a role's
failtype onto a data-less item; the producer fix stops that, and the rethrow
contract stops it from ever being a `Check Failed`.

## The `ItemReadLock` failure contract (rethrow the actual error)

`ItemReadLock::ItemReadLock(SharedTreeItemInterestPtr&&)`
(`tic/dll/src/ItemLocks.cpp:388`) is an always-on guard. Precondition: the
interest ptr must already have been `PrepareData()`'d (turned into a future);
`WaitReady` (`ItemLocks.cpp:846`) relies on this. For a data item or unit, once
the read lock is taken the item must be **calculating or ready**; if it is not, it
can only be **failed**, and the lock must not hand back a data-less item. So
instead of asserting on a narrow `WasFailed(FailType::Data)` predicate, the ctor
releases the lock and **rethrows the item's *actual* error**:

```cpp
if (IsDataItem(m_Ptr.get_ptr()) || IsUnit(m_Ptr.get_ptr()))
{
    if (!IsCalculatingOrReady(m_Ptr.get_ptr()))
    {
        cs_lock::ReadFree(m_Ptr);
        s_SessionUsageCounter.unlock_shared();   // a throwing ctor skips ~ItemReadLock — release by hand
        MG_CHECK(m_Ptr->WasFailed());            // not-calculating-or-ready => must be failed
        m_Ptr->ThrowFail();                      // surface the real failtype/message, at any level
    }
}
```

This **accepts a failure of any failtype** (`MetaInfo`, `Data`, `Validate`,
`Committed`) and propagates the recorded error to the caller, rather than crashing
with a generic `Check Failed` when the failtype happens to be `> Data`. The
`MG_CHECK(m_Ptr->WasFailed())` keeps the structural invariant — *not
calculating-or-ready implies failed* — while `ThrowFail()` gives the caller the
true reason. Because a throwing ctor's destructor does not run, the lock and the
session usage counter are released by hand first (mirroring `~ItemReadLock`).

The matching always-on/debug checks were relaxed the same way —
`WasFailed(FailType::Data)` → `WasFailed()`:

- `cs_lock::ReadLockInit` (`ItemLocks.cpp:334`): `assert(item->WasFailed() || CheckDataReady(item));`
- `WaitReady` (`ItemLocks.cpp:850`): `assert(CheckCalculatingOrReady(item) || item->WasFailed());`,
  and its negative case is now `if (!IsCalculatingOrReady(item)) return false;` — a
  failed item makes `WaitReady` return `false` *before* the lock is taken, so only
  a `PrepareData`→fail race reaches the throwing ctor.

This is a deliberate shift from an earlier draft of this note, which kept the
detector strict to `Data` and fixed only the *producer*. Both are now in place:
the producer fix (below) means #1144's `pand_type` fails at `MetaInfo` (renders
red, `WaitReady` returns false, the lock is never reached); the rethrow contract
is the backstop that turns any failed item that *does* reach a read lock into its
real error instead of an assert.

Caller impact: the ctor can now throw where it previously (for `<= Data` failures)
constructed silently. The call sites were reviewed — the throw is either guarded
away (`WaitReady`; `XmlTreeOut.cpp:217` via a preceding `CheckDataReady`), caught
(`TreeItem.cpp:3946`, inside a `try/catch(...)` → `DoFailCaller(err, Data)`), or
already on a throwing path (`ShvUtils.cpp:986`, immediately followed by a
`DataReadLock`). `AbstrStreamManager::WriteUnitRange` (`:137`) is guarded only by a
`dms_assert`; its storage-commit caller must tolerate the throw. The `try_token`
overload (`ItemLocks.cpp:412`, used at `ShvUtils.cpp:1020`) stays non-throwing.

## Case study: issue #1144 — root cause

Symptom: `Check Failed … IsCalculatingOrReady(...) || WasFailed(FailType::Data)
… ItemLocks.cpp(397)` on opening the table view of a memory-mapped, var-range
unit (`…/PrepBAG/Write_FinalMutationTable`) whose `pand_type` column depends on a
model expression with a type error.

Asserting item, from a full dump under cdb (release):

| field            | value |
|------------------|-------|
| type / `mc_Expr` | `AbstrDataItem`, `"PandTypering_Mutaties/pand_type"` |
| `m_InterestCount`| **3** (held — a `DataItemColumn`) |
| `m_DataObject` / `mc_DC` | **null** / **null** |
| `m_State = 0x413`| Progress = **Committed**, FailType = **Validate(16)**, `AF_SupplInterest` |

Interest-held, data-less, failtype `Validate(16) > Data(12)` → the lock asserts.

**The root model fault** is an operator-resolution (**metainfo**) error:
`AbstrOperGroup::FindOper` (`tic/dll/src/OperGroups.cpp:416`) throws
`"Cannot find operator … pointrow … arg1 DataItem<UInt64> … signature
DataItem<SPoint>"` while building a `FuncDC`. The `AfleidingPandType` chain that
`pand_type` depends on therefore fails at `FailType::MetaInfo`.

**Where the failtype got mislabeled** (pinned by breaking on
`Rtc!Actor::DoFailCaller` filtered to `ft == Validate`, full stack captured):

```
Actor::DoFailCaller(msg, failType = Validate)               Actor.cpp:1013
Actor::Fail(src, failType = Validate)                       Actor.cpp:1086
 (lambda) Actor::UpdateSupplMetaInfo::__l2                  Actor.cpp:644
AbstrCalculator::VisitSuppliers(UpdateSupplMetaInfoForValidation=0x92F)  AbstrCalculator.cpp:671
Actor::UpdateSupplMetaInfo                                  Actor.cpp:629
Actor::UpdateMetaInfo                                       Actor.cpp:606
… slSupplierExpr substituting (unique / pcount / add …)    AbstrCalculator.cpp
```

It is **not** a throw caught by `TreeItem::DoUpdate`. `Actor::UpdateSupplMetaInfo`
visited suppliers in three role passes and **forced the failtype by role**:

```cpp
// pass 1 ForDataPrep:    if (supplier->WasFailed()) this->Fail(supplier);                      // actual
// pass 2 ForValidation:  if (supplier->WasFailed()) this->Fail(supplier, FailType::Validate);  // FORCED <- bug
// pass 3 ForCommit:      if (supplier->WasFailed()) this->Fail(supplier, FailType::Committed);  // FORCED
```

`pand_type`'s failing supplier was reached in the **ForValidation** pass (it
feeds `pand_type`'s IntegrityCheck), so line 644 stamped the supplier's
`MetaInfo` failure as `Validate(16)` on `pand_type` — even though the real reason
is "metainfo couldn't be built". `WasFailed(FailType::Data)` then misses it
(`16 > 12`) and the later `ItemReadLock` asserts. (This corrects an earlier draft
that attributed the mislabel to a `DoUpdate` catch at `TreeItem.cpp:2887`; the
real site is `Actor.cpp:644`.)

## The fix (implemented)

Collapse the three role passes into one and **propagate the supplier's actual
failtype, only for metainfo-level failures** — `Actor::UpdateSupplMetaInfo`:

```cpp
void Actor::UpdateSupplMetaInfo() const
{
    VisitSupplProcImpl(this, SupplierVisitFlag::UpdateSupplMetaInfo, [this](const Actor* supplier)
        {
            assert(supplier);
            supplier->UpdateMetaInfo();
            if (supplier->WasFailed(FailType::MetaInfo))   // only <= MetaInfo
                this->Fail(supplier);                       // Fail(src) -> Fail(src, src->GetFailType()): actual
        }
    );
}
```

Why this is right:

- A **calc-rule (metainfo) error** in *any* supplier — DataPrep, Validation, or
  Commit role — is fatal: the item cannot be built, so it goes red and is not
  calculated. Propagating it as `MetaInfo` (`<= Data`) makes the lock check pass
  and surfaces the real error on the item. #1144's `pand_type` now ends
  `WasFailed(MetaInfo)`, renders red, no Check Failed.
- **No reorder needed.** Metainfo errors are handled here, at metainfo time;
  data/validate/commit-stage supplier failures are inherited later, with their
  *actual* failtype, by `Actor::DetermineLastSupplierChange` (`Actor.cpp:645`,
  which already collects the strongest/earliest supplier failure). The forced
  `Validate`/`Committed` stamping is gone.
- Paired with the `ItemReadLock` rethrow contract (see "The `ItemReadLock` failure
  contract" above): the producer fix keeps data-less items at `<= Data` so they
  surface normally, and the lock rethrows the actual error for anything that still
  slips through — no forced `> Data` failtype, no `Check Failed`.

Notes / follow-ups:

- Flag-set subtlety: `SupplierVisitFlag::UpdateSupplMetaInfo` (`= Parent | Update
  | ScanSupplTree = 0x192F`) is the old union **minus `Calc`(`DataController |
  DcArgs` = 0xC0)** that `…ForDataPrep` had. DataController/DcArg metainfo errors
  must still surface via the named/`SourceData` suppliers (which *are* visited) —
  worth a sanity check, but `mc_DC` is created *inside* this call so it is not a
  pre-existing supplier anyway.
- The now-unused `UpdateSupplMetaInfoFor{DataPrep,Validation,Commit}` enum values
  and the commented `static_assert` at `Actor.cpp:604` are dead — prune.
- The other forced-failtype sites (`DoUpdate` integrity-check `Fail(...)` calls;
  `FinalizeFailure(..., FailType::Committed)` in the commit path) follow the same
  "actual failtype" principle; revisit if a similar mislabel appears there.

## Other findings from this session

- **The model error itself** (project-side): `unique(<upoint attr>).Values` is
  `UInt64` in this build, not a point, so `pointrow`/`pointcol` no longer
  resolve. (`AfleidingPandtype.dms`: `coded_pair` is `upoint`, but
  `pointrow(values)` / `pointcol(values)` need a point.) Decode the packed pair
  without `pointrow`/`pointcol`.

- **The error surfaces from the table-view setup, at metainfo.** `Ctrl+D` →
  `MainWindow::tableView → createView → QDmsViewArea → TableDataView::AddLayer →
  TableControl::CanContain → SHV_DataContainer_GetItemCount → UpdateMetaInfo →
  DetermineState → MakeResult → FindOper → throw`. Captured directly (bp on
  `Rtc!throwErrorD` filtered on the message).

- **Deadlock variant of the var-range mmd commit.** Viewing the unit can hard-
  deadlock instead of asserting: `TreeItem::CommitDataChanges →
  WaitForReadyOrSuspendTrigger → ItemReadLock → treeitem_production_task::
  lock_shared → OperationContext::Join / TryRunningTaskInline` (producer run
  inline under the shared lock) while ~30 workers block in
  `RtlAcquireSRWLockExclusive`. The commit path should not run a producer inline
  while holding `lock_shared` against the pool's exclusive acquirers. (Related to
  #1126.)

- **GUI tree-expand cost.** Expanding the subtree drives eager tree-view state
  colouring through `Actor::DetermineState → DetermineLastSupplierChange →
  FuncDC::VisitSuppliers` over the `for_each_nedv`/`merge` supplier DAG; with
  shared suppliers this is super-linear and pegs the UI thread for minutes
  (distinct `Actor*` per level — a perf pathology, not a cycle/hang).

- **MMD dictionary timing (#1130 family).** `MmdStorageManager::DoWriteTree`
  (`tic/dll/src/stg/MemoryMappedDataStorageManager.cpp`) runs at `OpenForWrite`
  and serializes each unit's range; for a var-range unit the range may not be
  calculated yet. Deferring the dictionary write to `StorageManager` close is
  unsafe as-is (the manager can outlive sub-items of its storage holder) — buffer
  each unit's range string as the unit finishes, emit the dictionary at close.

## Debugging methodology (for reproducing engine-state bugs)

- Headless `GeoDmsRun` does **not** reproduce the GUI `UpdateMetaInfo`/redraw
  path; use the **GUI** build under cdb. Release (RelWithDebInfo) reproduces in
  ~30 min with usable symbols; Debug is ~10× slower. `MG_CHECK` is always-on, so
  the assert fires in release too.
- Repro recipe (#1144): launch `GeoDmsGuiQt.exe <config>
  /Analyse/Redev_obv_hele_bag/PrepBAG/Write_FinalMutationTable` (`StudyArea=AMS`,
  `BAG_file_date=20260108`), wait for config load (~25 s), press **Ctrl+D**. With
  the `…AMS_…mmd` Temp file holding every column except `pand_type`, only
  `pand_type` recomputes (~15 min, ~49 GB peak); a cold mmd recomputes all
  (~30 min). Needs ≥64 GB RAM or it swaps.
- Attach `cdb -p <pid>` to a **single** GUI (the launcher self-relaunches; reduce
  to one instance, and focus it with Win32 `SetForegroundWindow`, never
  `open_application`, which spawns another). Watch for the screen auto-locking
  mid-run (it pauses interaction; the process keeps running underneath).
- Break on the *non-template* Rtc throwers — `Rtc!throwErrorD`, `Rtc!throwDmsErrD`,
  `Rtc!throwCheckFailed` (note `throwCheckFailed → throwErrorF → throwErrorD`, and
  `Actor::ThrowFail → DmsException::throwMsg`); `throwErrorF`/`throwDmsErrF`/
  `throwMsgF` are templates with no single symbol. proj/gdal first-chance C++
  exceptions are benign — bp the Rtc throwers (proj never calls them) rather than
  `sxe e06d7363`.
- To capture **the moment a specific failtype is recorded**, bp the *recorder*
  with a cheap register filter — this is what pinned #1144:
  `bm Rtc!Actor::DoFailCaller ".if (@r8 == 0x10) { kpn 300; .dump /ma <f>; q } .else { gc }"`
  (`r8` = `FailType`; `0x10` = `Validate`). Far cheaper and more direct than
  trying to catch the throw.
- Pitfalls when filtering on "is function X on the stack":
  - The `$t0..$t19` pseudo-registers are **global, not per-thread** — a counter
    incremented in `DoUpdate` on a worker thread will read non-zero from a throw
    on the UI thread (false positive). Use a per-thread check (scan the *current*
    thread's stack) instead.
  - Scanning the stack on every hit of a **hot** thrower (`Actor::ThrowFail` /
    `DmsException::throwMsg` fire constantly during redraws of a failed column)
    crawls the process to a halt — whether `kn` (symbolized) or raw `dq` range
    scan. Prefer bp'ing a *cold* recorder (`DoFailCaller` with a failtype filter)
    over filtering a hot thrower.
- `.dump /ma` at the break preserves full state for offline inspection
  (`m_State`, `m_InterestCount`, `m_DataObject`, `mc_DC`, `mc_Expr`). Decode
  `m_State` against `actor_flag_set` (`ActorEnums.h`): progress = `& 0x3`,
  failtype = `& 0x1C`; flags incl. `AF_SupplInterest = 0x400`.
