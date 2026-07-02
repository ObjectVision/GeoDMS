# std-pointer ownership migration — session handoff

Branch: `refactor_ownership`. This continues the migration of the TreeItem family off the intrusive
`SharedPtr<T>`/`WeakPtr<T>` onto std ownership. Read this together with
`doc/development/std-ptr-migration-plan.md` (§4 = the `DcRef` variant) and `CLAUDE.md` (build rules).

## ★★ CURRENT STATE (2026-07-02, session 4) — MIGRATION FUNCTIONALLY COMPLETE ★★

Everything below this banner is the chronological work log; several of its "REMAINING / ⚠ OPEN"
sections are now CLOSED. Authoritative current status:

- **Wrappers removed** (commit `70c3f942`): `shared_tree_ptr`/`weak_tree_ptr` are gone — the codebase
  now uses plain `std::shared_ptr`/`std::weak_ptr` (`SharedTreeItem`, `WeakUnit`, … typedefs) plus the
  `make_shared_tree(p, newly_obj{}|existing_obj{}|no_zombies{})` / `make_weak_tree` / `lock_or_cancel`
  helpers in `SharedTreePtr.h`. See the "2026-07-02: wrappers REMOVED" section at the very bottom.
- **Debug unit suite GREEN** (HEAD `cc60a08f`, build 19:32): `batch/TestDebugUnit.bat` → `unit_flagged.bat`
  ran **61 configs across every section + both python-binding tests, 0 failures, 0 hangs**
  (`GeoDMSTestResults/unit/vD64.on_02-07-2026_19-32-46.70.txt`). This CLOSES the old open items:
  FindUnit/weak-values-unit expiry class, `centroid` teardown-order assert (assert removed, `AbstrUnit.cpp:55`),
  `ComplexNamespaces` namespace-teardown, `DoubleInstantiation`/`GridFromTemplate` AVs, `TemplDefinition`,
  `select_with_attr_by_org_rel_nested` — all now pass.
- **All 3 Release `.m` regression teardown hangs FIXED + verified** (see memory `project_206m_regression_hangs`):
  t611 (15.5s, n_diff=0), t810 (266s), t641_2 storage-write drain (clean, 14/14 indicators match). Report cells `ok`.
  Fixed by commit `2bbef28c` "fix 3 teardown drain hangs".
- **Rogue-control-block audit CLEAN**: `grep -nE "std::shared_ptr<(const )?(TreeItem|Abstr\w+|Unit<)[^>]*>\s*\(\s*[a-z_]\w*\s*\)"`
  (excl. `existing_obj`/`newly_obj`/`no_zombies`) → **0 sites**.

**Remaining = cleanup / hardening (non-blocking) + housekeeping:**
1. Broader confidence (optional): full `.m`/`.c`/`.l` regression re-baseline on current HEAD (Linux + cmake parity + perf).
2. Exit-leak residuals: commit `aac99d3c` fixed the Debug exit leaks; memory `project_debug_unit_leaks` says only
   third-party (gdald/Qt6d detach, pybind11) remain — believed closed, one confirming glance if desired.
3. DcRef kind-1 `m_Owned` `std::vector` → split into 1a/1b/1c/1d (`TreeItemDualref.h:53`); DcRef #2 (kind-2 owns
   the producing DC) deferred, not a blocker.
4. 6 `// TODO ownership:` stopgaps: `Explain.cpp:73/74`, `AbstrStreamManager.cpp:134`, `UsingCache.cpp:412`,
   `XmlTreeOut.cpp:819/839` — review to resolve or formally accept.
5. Guardrail is now **convention-only** (the `=delete` raw-ctor compile guard died with the wrappers) — keep the
   audit grep above (a lint/CI check would be ideal). Next hardening candidate: `TreeItemDualRef::GetNew()/GetOld()/operator->` raw borrows.

## Goal & invariants

The point of the operation is to **harden ownership**: no dangling raw pointers, no transient
liveness checks used in `if`s.

- `TreeItem` derives from **`Actor`** (NOT `SharedActor`) — it has **no intrusive refcount**. Family =
  `TreeItem`, `AbstrUnit`, `AbstrDataItem`, `AbstrParam`, `Unit<…>` and subclasses.
- `TreeItemDualRef` / `DataController` **intentionally stay `SharedActor`** (intrusive). They are NOT
  TreeItem-family. Other non-family types that stay intrusive: `AbstrDataObject`, `DataArray<…>`,
  `TileFunctor<…>`, `AbstrCalculator`, `AbstrStorageManager`, `SharedActor`.

## Wrapper types (`rtc/dll/src/ptr/SharedTreePtr.h`)

- `shared_tree_ptr<T>` — **owning**. `.get()/.get_ptr()/operator->/explicit operator bool/operator==(raw)`;
  tag ctors `newly_obj`/`existing_obj`/`no_zombies`. **No implicit raw ctor and no implicit `operator T*`**
  (both were tried and removed on purpose — every raw-needing site must be classified explicitly).
- `weak_tree_ptr<T>` — **non-owning**, drop-in for the old `WeakPtr`: raw-ptr ctor,
  `operator=(raw|nullptr|shared)`, `.get()/.get_ptr()/operator->/explicit operator bool/operator==(raw)/.reset()`
  — all via a momentary `lock()` (null if expired).
- **`ICanChoosePtr` was a temporary placeholder and has been DELETED. Never reintroduce it.**

## `DcRef` — `TreeItemDualRef::m_Data` (`tic/dll/src/TreeItemDualref.h`)

Replaces the old `(m_Data WeakPtr + m_OwnedData SharedTreeItem)` split. One member, a variant whose
`index()` is the state:

```
std::variant<
  std::monostate,                  // 0 empty
  std::shared_ptr<TreeItem>,       // 1 IsNew      — root cache result; DualRef is primary owner
  std::shared_ptr<const TreeItem>, // 2 IsOld-cache— owned ("like new")
  std::weak_ptr<const TreeItem>,   // 3 IsOld-config— tree owns it; lock to use
  std::weak_ptr<TreeItem>          // 4 IsTmp      — instantiation borrow; lock to use
>
```

- `get()` returns an **owning** `shared_tree_ptr<const TreeItem>` (weak arms lock into a held snapshot →
  no dangling). Callers MUST hold it: `if (auto p = m_Data.get()) p->…`.
- State = `kind()` (variant index). `clear()`, `use_count()` (owning arms only).
- The transient `has_ptr/is_null/get_ptr/operator bool/operator->` were **removed** from `DcRef`.
- `TreeItemDualRef` keeps `GetCurr()` (owning), `GetNew()/GetOld()/operator->` (raw borrows — valid only
  while an owner outlives the use; the next hardening candidate), and `IsNew/IsOld/IsTmp` from `kind()`.

## Why `m_Data` is a variant and not a plain `std::weak_ptr` (FAQ)

A pure `weak_ptr<const TreeItem>` works for **IsTmp** (the call site owns the tmp during use) but NOT for
**IsNew**: a freshly `make_shared`'d, parentless cache result has no other owner; the DualRef IS its
primary owner, and its lifetime (= the DataController = the config item's `mc_DC`) outlives the call site,
so ownership cannot be pushed there. Hence owning arms for IsNew/IsOld-cache, weak arms for
IsOld-config/IsTmp.

## Already done (whole repo, outside what's noted below)

- All intrusive `SharedPtr/WeakPtr<TreeItem-family>` and all `ICanChoosePtr` eliminated.
- Interest subsystem: `SupplInterest.h` → `InterestPtr<std::weak_ptr<const Actor>>`; `Actor::weak_from_actor()`
  virtual (TreeItem overrides via `weak_from_this`); DataControllers are NOT retained in the supplier list
  (their interest rides the ownership chain — `mc_DC` via `TreeItem::StartInterest`, arg DCs via
  `FuncDC::StartInterest`, plus the result-TreeItem entries). `InterestRetainContextBase::Add` and
  `UpdateMarker::ChangeSourceLock` take `const Actor*` now.
- Units `m_DomainUnit/m_ValuesUnit` → `weak_tree_ptr` (lock-at-use); `UsingCache` `m_Usings`/`m_SortedItemCache`
  → `vector<weak_ptr>` with a lock-aware comparator; `TileFunctorImpl::m_ResultAdi` → `weak_tree_ptr`.
- `TreeItem.h` members `m_Parent`/`m_BackRef`/`mc_RefItem`/`mc_OrgItem` → `weak_tree_ptr<const TreeItem>`.
- `ExtLockMgr` external `AddRef/Release` → owning `std::multiset<SharedTreeItem>` pin registry.

## Compile-cascade fix rules (from the architect)

- **C2440/C2664** (smart-ptr/`DcRef`→raw): BORROWED (passed/transient/compared) → append `.get()`;
  HELD (stored) → change the holder's type to `weak_tree_ptr<X>`. Never store a raw from a smart ptr.
- **C2679/C2678** (`=`/op from raw): **reroute the source** — make the producing local/function hand over a
  `shared_tree_ptr`/`weak_tree_ptr` from construction, not a raw rewrapped via `existing_obj`.
- **`GetRefCount()` on a TreeItem**: gone → `p->weak_from_this().use_count()` in debug/assert, or relax.
- **C2665 on `std::set/std::map<std::shared_ptr<const TreeItem/AbstrUnit>>`** keyed by raw → wrap the key
  `shared_tree_ptr<const X>(rawkey, existing_obj{})`.

## Progress log (2026-06-28, session 2)

Worked the cascade down from the stale 314 (`build_debug_stdptr4.log`) via repeated incremental Debug
rebuilds (`build_debug_stdptr<5..12>.log`). **DmRtc, DmTic, DmStx, DmStg all build & link green.** Only the
clc/geo operator cascade (~27 errors) + python remained at `build_debug_stdptr12.log`; two subagents are
finishing those leaf operator files.

Key structural fixes landed this session (all in-repo):
- **`DcRef`** got a kind-based `explicit operator bool()` / `operator!()` (arm-set STATE check, NOT a
  liveness probe — mirrors `TreeItemDualRef`'s own semantics). `DataController.cpp` /
  `MoreDataControllers.cpp` `m_Data` adapted: `m_Data->X()` → `m_Data.get()->X()`, `return m_Data;` →
  `return m_Data.get();`, raw-context `m_Data.get()` → `m_Data.get().get()`.
- **`createFromXmlFuncType` / `MetaClass::CreateFromXml` + 3 overrides + XmlTreeParser**: return type
  `std::shared_ptr<SharedActor>` → `std::shared_ptr<Actor>` (TreeItem is an Actor now, not a SharedActor).
  This also auto-resolved all the DebugCast.h/`<memory>` `static_cast SharedObjWrap<Actor>*→TreeItem*` errors.
- **`MakeSharedFromBorrowedObjectPtr`**: added a constraint-specialized overload in `SharedTreePtr.h` that
  returns `shared_tree_ptr<T>` for `enable_shared_from_this`-backed (TreeItem-family) types; the intrusive
  `SharedPtr<T>` version in `SharedPtr.h` still serves non-family types. This was the single fix for the
  `SharedPtr<const AbstrUnit>/<AbstrDataItem>` instantiation errors (domain_unit_creator, IndexGetterCreator1).
- **`SupplCache` `ActorCRef`**: `SharedPtr<const SharedActor>` → `shared_tree_ptr<const TreeItem>` (all
  cached suppliers are TreeItems); `GetSupplier()` returns `const TreeItem*`.
- **Debug leak registry** `s_TreeItems`: split off its own `TreeItemRegistryType : std::set<const TreeItem*>`
  (RAW identity keys — needed because items self-register in the ctor, before any shared_ptr owns them).
  `TreeItemSetType` (the visitor "doneItems" sets) stays `set<shared_ptr<const TreeItem>>`.
- **`TreeItem::GetInterestPtrOrNull`** reimplemented to build the interest ptr directly from `this`
  (lock `sg_CountSection` + manual `++m_InterestCount` + `already_incremented_tag`), since it can no longer
  route through `Actor::GetInterestPtrOrNull` (that dynamic_casts to SharedActor). Added an
  `already_incremented_tag` ctor for `shared_tree_ptr` in `InterestHolders.h`.
- **`OldRefDecrementer`** split: a std `shared_tree_ptr<const TreeItem>` holder for the old ref TreeItem
  (`SetReferredItem`), and an intrusive `OldDcInterestDecrementer : SharedPtr<const DataController>` for the
  old DataController (`mc_DC`). Both DecInterestCount on destruction.
- **`OperationContext::ScheduleCalcResult` statusActors**: `vector<SharedPtr<const SharedActor>>` →
  `vector<variant<DataControllerRef, SharedTreeItem>>` owning (no extra interest), with a `statusActorOf`
  helper → `const Actor*`. `GetStatusActor` return type `const SharedActor*` → `const Actor*`.
- **Signature alignments** (.cpp def → .h decl, now owning `shared_tree_ptr<const TreeItem>`):
  `TreeItem::GetUltimateItem`, `_GetCurrRangeItem`, `_GetUltimateItem`, `_GetHistoricUltimateItem` returns.
- **GetRefCount** (debug/teardown instrumentation) → `weak_from_this().use_count()`.
- The bulk leaf `.get()` borrowed edits + set-key `existing_obj{}` wraps were applied by subagents across
  tic (XmlTreeOut, ItemLocks, Explain, AbstrDataItem, DataLocks, AbstrUnit, storage leaf, …) and clc/geo.

## ★ FULL SOLUTION BUILDS GREEN (2026-06-28, session 2) — `build_debug_stdptr18.log`, 0 errors ★

`all22.sln` Debug x64 compiles AND links across ALL modules: DmRtc, DmTic, DmStx, DmStg, Clc, Geo,
DmsPython (geodms.pyd), GeoDmsRun.exe, DmTicTst, ShvDLL, **GeoDmsGuiQt.exe**. The entire compile cascade
(266 real errors at `build_debug_stdptr5.log` → 0) is done. Six subagents did the mechanical
borrowed/held `.get()` + set-key grinds (tic leaf, clc, geo, shv); the lead did all the structural/cross-cutting
pieces (see Progress log above). Build NOT yet committed.

Residual operator-cascade patterns resolved this session (beyond the Progress log): the `CreateFutureTile*`
AND `make_unique_{Lazy,Future,Const}TileFunctor` families take `shared_tree_ptr<AbstrDataItem>` arg1 — wrap
the raw result adi `SharedMutableDataItem(res, existing_obj{})` (or, when the arg is already the shared
`resultAdi` param inside `CreateFutureTile*`, just drop its `.get()`), and `.get()` sibling `AsUnit(...)`
unit args. `shv` `CreateContainer`/`CreateContainer_impl` returned `OwningPtr<TreeItem>` (intrusive) → now
plain `TreeItem*` (the created item is owned by its parent container; callers' `.release()` dropped).
qtgui `MainWindow::m_root`/`m_current_item` are `shared_tree_ptr<TreeItem>`; assign raw via
`shared_tree_ptr<TreeItem>(p, existing_obj{})`, and `setEnabled(bool(m_current_item))`.

## ⚠ RUNTIME PHASE — first finding (2026-06-28, session 2)

Smoke test `reverse.dms` (`GeoDmsRun.exe /L<log> <tst>\Unit\Operator\cfg\reverse.dms /test_log`, root
`reverse`, result `/test_log`) **compiles+loads the config but the COMPUTE hits a runtime assertion**:

```
Assertion failed: !doCalcData || argIter->m_DC->GetInterestCount(),
  file tic\dll\src\MoreDataControllers.cpp, line 553   (in FuncDC::GetArgs)
```

### ✅ ROOT-CAUSED + FIXED (line 553)

The violation: `Actor::GetSupplInterest`/`push_front` (SupplInterest.h) builds the supplier-interest list by
visiting suppliers and **incrementing each one's interest** (the InterestPtr ctor calls IncInterestCount).
The std-ptr migration changed that list element to `InterestPtr<std::weak_ptr<const Actor>>` and made
`push_front` **skip any supplier whose `weak_from_actor()` is empty — i.e. every DataController** (a
`DataController : TreeItemDualRef : SharedActor` is intrusive, not std-managed; only `TreeItem` overrides
`weak_from_actor()`). So an arg DC's interest was no longer incremented directly.

The header comment claimed two compensating mechanisms, **both insufficient**: (a) "a FuncDC's StartInterest
holds its arg DCs" — *that override never existed*; (b) transitively, because `FuncDC::VisitSuppliers` also
visits each arg DC's **result TreeItem**, and `TreeItem::StartInterest` (TreeItem.cpp:4589
`calcHolder = mc_DC.get_ptr()`) bumps the result's `mc_DC`. But (b) only re-holds the DC when the result is
a **cache item**; for an arg that resolves to a **config item** (`ok/A`, `UnTiled/reverse/att` in
reverse.dms) the result has **no `mc_DC`**, so the arg DC got zero interest → assert at line 553.

**Fix (landed, build green):** `SupplInterest.h` `push_front` now retains DataController suppliers via an
**owning intrusive interest** (`SharedActorInterestPtr m_DcValue` added to `SupplInterestListElem`), exactly
as the pre-migration design held all suppliers, while keeping TreeItem suppliers **weak** (no root retain
cycle — DCs don't own the config tree). Comments in `SupplInterest.h` + `Actor.cpp GetSupplInterest`
updated. Verified: the line-553 assert no longer fires; reverse.dms now proceeds further into compute.

### ✅ FIXED (line 69) — items may die with residual interest; dtor must undo its own supplier interest

Per the architect: an item may now be destructed while consumers still hold (non-owning, weak) interest in
it. Landed: `~TreeItem` (and `~Actor` for non-TreeItem actors e.g. DataControllers) now call
`StopSupplInterest()` early in destruction — undoing the item's OWN supplier interest so its suppliers are
decremented and its `s_SupplTreeInterest[this]` entry doesn't dangle (the usual undo, `StopInterest`, only
runs when interest hits 0, which never happens for an item destroyed with residual interest). The
"no interest at destruction" self-asserts were relaxed accordingly (`~AbstrDataItem` GetInterestCount,
`~TreeItem` HasInterest, `~Actor` m_InterestCount); `~TreeItem`'s `!AF_SupplInterest` assert now holds
because StopSupplInterest cleared it. Verified: reverse.dms now computes past line 69.

### ✅ reverse.dms now COMPUTES (2026-06-28, session 2 cont.) — abort + bad_weak_ptr fixed; teardown UAF remains

`reverse.dms /test_log` now runs the full compute to completion (`} Updating::[[/test_log]] (0.011 secs)`,
result written, logging ended cleanly). Fixes landed, in order discovered:

1. **Read/write lock vs reversed ownership (Approach A).** `ItemReadLock` now locks ONLY the item; instead of
   read-LOCKING the cache-ancestor chain (old `PrepareReadAccess`, which left an `m_ItemCount` on a now-unowned
   parent), `cs_lock::ReadLock` AWAITS any in-progress write on a cache ancestor (`AwaitAncestorWrites`: walk up
   via GetTreeParent, join the producer of any write-locked ancestor, no lock — lifetime-safe since a
   write-locked ancestor is kept alive by its own `ItemWriteLock`). `TryReadLock` uses `TryAwaitAncestorWrites`.
   (`SessionData`/`GetCacheRoot` not needed; the abandoned cache-root-retain variant used `GetRoot()`+`IsCacheItem()`.)
2. **★ THE rogue-control-block bug (real cause of the `ClearDataObject` `m_ItemCount==0` abort).**
   `DataReadLockAtom`'s ctor did `m_Item(item)` with a RAW pointer. `shared_tree_ptr` inherits `std::shared_ptr`'s
   raw ctor via `using base_type::base_type`, so this built a **separate control block with a `delete` deleter**
   instead of sharing the item's real one — and on destruction it `delete`d the item out from under its other
   owners/locks (m_ItemCount still > 0 → abort). Fix: `m_Item(item, existing_obj{})` (DataLocks.cpp). **This is a
   latent class of bug: any `shared_tree_ptr<X>(rawptr)` (no tag) silently makes a rogue control block — the
   wrapper's "no implicit raw ctor" claim is defeated by the inherited ctor. Consider `= delete`-ing the inherited
   raw ctor in `shared_tree_ptr` to surface all such sites at compile time.**
3. **`DataReadLock` keep-alive ordering.** Added `shared_tree_ptr<const AbstrDataItem> m_KeepItemAlive` as the
   FIRST member (destructed LAST) so neither count-bearing lock (`m_RefPtrLock` m_ItemCount / `m_DRLA`
   m_DataLockCount) is ever the item's last owner — when it drops, both counts are already 0.
4. **Teardown `bad_weak_ptr`.** My earlier signature-alignment made `_GetHistoricUltimateItem`/`_GetUltimateItem`
   re-own via `existing_obj{}` → `shared_from_this()` throws when called on a mid-destruction item
   (`~AbstrDataItem` → `SetKeepDataState(false)`). Fixed to `no_zombies{}` (owning when live, empty when dying);
   `SetKeepDataState` now guards the empty case.

**REMAINING: teardown UAF.** After a clean compute, exit fails with `OS Structured Exception 0xC0000005`
(read at a code-address ≈ a call through a dangling vtable) during final config-tree teardown — the documented
"heap corruption at config-tree teardown" (persistent intrusive `SharedPtr<TreeItem-family>` members / ownership
cycles outliving the std lifetime; see `teardown-leak-and-ownership-cycles.md` and memory
`project_treeitem_ownership_model`). Caught at Main → exit 2 (no dialog). To get its stack: cdb can't *launch*
this Debug binary (it crawls/deadlocks under the debugger), but ATTACHING works — run plain, and either attach +
`sxe av` then continue, or (easier) drive it under the VS debugger (F5). Then `batch/TestDebugUnit.bat`.

### (resolved) line 176 — `ClearDataObject` m_ItemCount check (root cause was the rogue control block, #2 above)

reverse.dms compute now fails `MG_CHECK(m_ItemCount == 0)` in `AbstrDataItem::ClearDataObject`
(AbstrDataItem.cpp:176), called from `~AbstrDataItem` (line 84). `m_ItemCount` (TreeItem.h:156) is NOT
interest — it is the atomic count of active item read/write **usage/production locks** (ItemLocks.cpp). So a
DataItem is being destroyed while a data **usage lock** is still active. ClearDataObject's three checks are
`GetDataObjLockCount()==0` (passed), `m_ItemCount==0` (FAILED), `m_InterestCount==0`. Open question for the
architect: is residual `m_ItemCount`/`m_InterestCount` at destruction-driven ClearDataObject now also benign
(relax these like the interest asserts), or does a live usage lock on a dying item indicate a genuine
premature-destruction bug (a read lock that should have kept the item alive, now not, because interest is
non-owning)? Captured via a temp `fprintf` in `throwCheckFailed` (reverted) — the message was
`m_ItemCount == 0 at AbstrDataItem.cpp:176`.

(Earlier text below kept for reference.)

### (ref) the line-69 assert as first observed — NOT yet fixed

After the line-553 fix, `reverse.dms /test_log` compute now hits, still early (right after `Item /test_log`):
```
Assertion failed: !GetInterestCount(), file tic\dll\src\AbstrDataItem.cpp, line 69   (AbstrDataItem::~AbstrDataItem)
```
i.e. a DataItem is **destroyed while its interest count is still > 0** — the opposite imbalance. Leading
hypothesis (static, unconfirmed): the **weak** supplier-interest on a result TreeItem is non-owning, so an
interest-incremented result can be destroyed before that weak interest is decremented (the weak InterestPtr
dtor only decrements if the target is still live), leaving the count > 0 at `~AbstrDataItem`. Could also be
an independent current-migration interest-balance bug merely unmasked by the 553 fix. **Get the stack via
the VS debugger (F5) — headless cdb is unworkable on this binary** (see the cdb gotcha note below; ~6
attempts this session never reached the assert under cdb — the Debug binary's behavior/timing changes under
a debugger and `g` runs minutes without hitting it).

**Debugging gotcha that cost this session — fix first next time:** cdb on this Debug binary is *unusably
slow* with the public symbol server and with first-chance-exception narration (GeoDMS uses C++ exceptions
for control flow). Before driving cdb: `set _NT_SYMBOL_PATH=C:\dev\GeoDMS_2026\bin\Debug\x64` (local PDBs
only — they sit next to the DLLs), and in the script `sxd *` (suppress first-chance stops/printing). Even
then a full `g` to the assert was not reached in 5 min in this session — consider a hardware breakpoint on
`MoreDataControllers.cpp:553` directly (`bp Tic!...` once symbols load) or run the assert site under the
VS debugger (F5 attach) to get the live stack + inspect `argIter->m_DC` / its supplier list / who consumes
it. Also: pass `/test_log` via PowerShell or `MSYS_NO_PATHCONV=1` — bash mangles `/test_log` into
`C:/Program Files/Git/test_log` (which itself causes an unrelated AV on the bad item path).

Investigation leads: (a) does `StartSupplInterest()` actually visit + IncInterestCount the arg DCs now?
(b) did the `SupplInterest.h` weak-Actor migration drop the arg DCs from the retained set? (c) does
`DataController::StartInterest` (DataController.cpp:206, which I adapted to `if (m_Data && !IsTmp())
IncDataInterestCount();` with the new kind-based `operator bool`) still fire for arg DCs? (d) the
`OldRefDecrementer`/`GetInterestPtrOrNull` reworks. Confirm against `teardown-leak-and-ownership-cycles.md`.

## ★ SESSION 3 (2026-06-29) — teardown UAF FIXED, canonical `=delete`, rogue-ctor + DualRef cascade

**1. Teardown UAF (the 0xC0000005) was NOT an ownership bug — it was the leftover TEMP instrumentation.**
The std-ptr migration's real weak liveness already resolves the cycles (`s_DcMap` drains to 0, no leaked Lisp,
`configRoot rootRc=1` → `ReleaseIt` cleanly destroys the root). The fault was `EnableAutoDelete` reading the
config root (`weak_from_this()`, `DBG_*` dumps, `g_DBG_ConfigRoot`) AFTER `SessionData::ReleaseIt` freed it.
Removed those blocks (do NOT touch `this` after ReleaseIt on a config root). **`reverse.dms /test_log` →
exit 0** (clean compute + clean teardown). Committed `ae5fecde`.

**2. All temp teardown-leak instrumentation removed** (DataController `DBG_*`/`g_DBG_ConfigRoot`,
OperationContext `~tg_maintainer` calls, MoreDataControllers `DBG_GetOtherSuppliers`, LispRef `~LispCaches`
restored to pristine — dropped `_set_error_mode/_set_abort_behavior` + `leaked_lisp.txt`). Committed `bae0848c`.

**3. "Remove all `SharedPtr<TreeItem-family>`" — ALREADY COMPLETE.** Every family typedef is `shared_tree_ptr`;
no lingering intrusive `SharedPtr<family>`/`WeakPtr<family>` members.

**4. Canonical `=delete` of the inherited raw ctor** (user decision, see [[project_stdptr_delete_raw_ctor]]):
`template <class Y> shared_tree_ptr(Y*) = delete;` in `SharedTreePtr.h`. shared_tree_ptr<T> is now a strict
`std::shared_ptr<T>` — no auto raw conversion (use `.get()`), construct only via make_shared Creators /
weak_ptr::lock() / interim tags. This surfaced 7 genuine rogue-control-block sites (a raw-ptr ctor builds a
SEPARATE control block w/ delete-deleter → double-free at teardown — the reg_count teardown AV was exactly
this), all fixed with `(rawptr, existing_obj{})`: RegCount.cpp:57 (RegionMeta m_Partition — **fixed a real
teardown AV; reg_count now passes**), Explain.cpp:848 (m_StudyObject) + 142 (ArgRef in_place), stg
DllMain.cpp:629 (m_ADI) + OdbcStorageManager.cpp:364 (m_TableHolder), shv IndexCollector.cpp:54/55.

**5. `TreeItemDualRef → TreeItem*` implicit conversion removed (user)** → ~50 operator sites fixed: pass
`resultHolder.GetNew()` (the new result being built; the universal case here) instead of bare `resultHolder`
as the `TreeItem*` parent/context to `Create{Result,Tmp,}Unit/CreateDataItem[FromPath]/CreateUnitFromPath/
CreateResultDomain/CopyTreeContext`. (`OperPolygon.cpp:756` `resultHolder` is a raw `TreeItem*` param — left.)

**Full all22.sln Debug x64 builds GREEN with all of the above** (`build_debug_stdptr48.log`). UNCOMMITTED beyond
`bae0848c` as of this writing → commit the green stepping stone.

### ⚠ OPEN — `FindUnit` GetTreeParent assert (the ~13 unit-sweep failures) = weak `m_ValuesUnit`/`m_DomainUnit`
A headless sweep of the operator/unit/other/integrity configs (mirrors `tst/batch/Unit/Instance.bat`:
`GeoDmsRun /S1 /S2 /S3 <cfg> <item>`) shows reverse/subitem/connect/reg_count/etc PASS, but ~13
template/select/union/namespace configs FAIL. Root-caused (temp diagnostics in FindUnit + AddItem, since removed):
`AbstrDataItem::GetAbstrValuesUnit()` (AbstrDataItem.cpp:109) — `m_ValuesUnit`/`m_DomainUnit` are
**`weak_tree_ptr`** now; when the weak EXPIRES (the values/domain unit is a parentless cache unit not pinned by
the config tree) it falls back to `FindUnit(token)`, which asserts `GetTreeParent()` because the item is a
parentless **cache root** (`isCache=1 isEndo=1 name='' m_Parent.has_ptr=0`). `AddItem` never fails to capture
m_Parent (verified) — the parent simply doesn't exist for these cache roots. **A failed CRT `assert` pops a
modal MessageBoxW dialog that HANGS headless Debug runs** (per CLAUDE.md) — so these configs hang, not exit.
This is exactly the prior-session finding that "m_DomainUnit/m_ValuesUnit are load-bearing during compute and
CANNOT be weakened" (weakening them crashed/asserted), now reproduced under the std migration. The std plan bet
that tree-shared-ownership keeps units alive so the weak wouldn't expire — that bet FAILS for cache/instantiation
result units.

**DECISIVE EVIDENCE (cdb dump of the failing result item at the assert, categorical_unit):** the FindUnit call
comes from `FuncDC_CreateResult` (MoreDataControllers.cpp:655) → `resultHolder->GetDynamicObjClass()` →
`GetAbstrValuesUnit()` during `FuncDC::MakeResult`. The result item is a parentless, unnamed, no-backref
operator result (m_Parent/m_BackRef/m_ID all null; held alive by `resultHolder`/DcRef). Its `m_tValuesUnit` and
`m_tDomainUnit` tokens are **0 (empty — nothing to FindUnit-resolve)**, but `m_ValuesUnit` (and m_DomainUnit) are
**SET yet EXPIRED**: `_Ptr`/`_Rep` non-null but strong use_count 0 → `is_null()` true. So the values unit was
ASSIGNED at result creation, then DESTROYED during compute — the WEAK `m_ValuesUnit` could not keep it alive and
it had NO other owner. The data item (held by DcRef) outlived its freshly-created values unit (which died with its
container/cache root). `FindUnit(token=0)` is a doomed fallback (empty token → would `ThrowFail "Undefined Values
unit"` even if GetTreeParent weren't null). **CONCLUSION: a lifetime bug, NOT "never set" and NOT cache-root-
released-transiently — the result's endogenous units have no owner with the result's lifetime; weak m_ValuesUnit/
m_DomainUnit are fundamentally insufficient for parentless operator-result units.** Fix = give those units an
owner for the result's lifetime: make m_ValuesUnit owning (cycle-free — a values unit is not the item's parent),
and/or have the DcRef/result hold its endogenous units; m_DomainUnit owning risks the table↔column cycle only when
the domain is the item's own container. **DECISION NEEDED (architectural, user's call):** make `m_ValuesUnit` (and/or `m_DomainUnit`)
OWNING `shared_tree_ptr` (m_ValuesUnit owning is cycle-free since the values unit is not the parent; m_DomainUnit
owning reintroduces the table↔column cycle = a std::shared_ptr LEAK, not a crash), vs. keep weak + pin cache
units another way (e.g. the result/DC holds its units), vs. a values-unit registry. The `role=Values` diagnostic
suggests m_ValuesUnit is the one expiring here.

### ⚠ OPEN — `centroid_or_mid_complex` teardown-order assert (`AbstrUnit.cpp:57` `DataItemRefContainer ~ !size()`)
The per-unit DataItemsOut registry (`m_DataItemsAssocPtr`) is an **AbstrUnit member**, so it is destroyed BEFORE
the `~TreeItem` base dtor destroys the unit's sub-item attributes (which are still registered in it) — a
parent-owns-child destruction-order problem. Fix candidates: relax the assert (registered items are sub-items
about to die), or clear/deregister in `~AbstrUnit` before members are destroyed, or make `~AbstrDataItem`'s
`DelDataItemOut` robust when its domain unit is mid-destruction.

### ✓ FIXED this session — `UsingCache::ClearUsings` teardown null-deref (ComplexNamespaces etc.)
`lock_raw(*i)->GetUsingCache()` deref'd a using-namespace already destroyed during teardown (expired weak →
null). Guarded: `if (const TreeItem* ns = lock_raw(*i++)) if (ns->CurrHasUsingCache()) ns->GetUsingCache()->
DelIncoming(this);`. (ComplexNamespaces no longer AVs there, but now hangs on the FindUnit assert above.)

### ◐ IN PROGRESS — DcRef kind-1/kind-2 redesign (cache result units kept alive by the DcRef, not the items)

Per the user's design (memory [[project_stdptr_cache_unit_ownership]]): `m_DomainUnit`/`m_ValuesUnit` stay
uniformly WEAK; the DcRef that owns a cache result owns unit-liveness. Implemented in `TreeItemDualref.h`/
`DataController.cpp`:
- **kind 1 (IsNew)** = `NewResult{ std::vector<std::shared_ptr<const TreeItem>> m_Owned }`: `[0]` = cache root
  result; `[1..]` = owning refs to the result subtree's cache items' domain/values units (collected at SetNew by
  `KeepResultUnitsAlive`/`CollectCacheUnitsToKeepAlive`, while the operator's unit locals are still alive; config
  units skipped to avoid the config-root retain cycle). TODO: split into 1a/1b/1c/1d to drop the vector.
- **kind 2 (IsOld cache)** = `OldCacheSubItem{ std::shared_ptr<const TreeItem> m_Root; const TreeItem* m_SubItem }`:
  owns the sub-item's cache ROOT (keeps the subtree + the sub-item's ancestor domain unit alive) + a borrowed
  ptr to the sub-item. `m_DomainUnit`/`m_ValuesUnit` now `WeakUnit` everywhere; added non-resolving
  `AbstrDataItem::GetCurrDomainUnit()/GetCurrValuesUnit()`.

Units are captured at TWO points (deduplicated, `DcRef::keepAlive`): at **SetNew** (universal/immediate, the
root's directly-set units) AND from **`FuncDC_CreateResult` right after `oper->CreateResultCaller`** via
`TreeItemDualRef::CaptureResultUnits()` (user's instruction #1 — by then the subtree is complete and the units are
still alive as sub-items of the cache root or held by `*args`).

**Result: unit sweep 34 → 39 pass** (categorical_unit, select_with_attr_by_org_rel_nested, reg_count,
**merge_indirect**, … now compute; reverse/subset clean). ⚠ NOTE: the headless sweep's per-config timeout MASKS
passes — Debug raster computations are slow; **merge_indirect passes in 46s** (rc=0) but looked like a hang at a
25s cap. Use a generous timeout (90s+) and distinguish "slow-pass" from genuine FindUnit (the `fu2.txt` diagnostic
confirms which configs actually hit the assert).

**Remaining 8 genuine failures** (verified still hit FindUnit, `vuExpired=1`, parentless cache result `isCache=1
isEndo=1 hasBackRef=0 name=''`): xml_parse, centroid, operator.dms, spoint_to_tiff, ComplexNamespaces,
TemplDefinition (FindUnit/values-unit) + 2 AVs (DoubleInstantiation, GridFromTemplate). These cache results are
created via **non-FuncDC paths** (template instantiation `CopyTreeContext`/`InstantiateTemplate`, namespaces) so
`CaptureResultUnits` (FuncDC-only) never runs for them, and their values unit still expires. NEXT: hook the
unit-capture into those result-creation paths too (or, per the kind-2/root-side discussion, a root-side keep-alive
list reachable by any owner). Also still open: **#2 kind-2 owns the producing DC** (`DataControllerRef`) of the
cache root instead of the root object — so kind-2 reaches the root's kind-1 unit vector — NOT yet implemented
(kind-2 via `SymbDC::SetOld` resolves config items today, so it's not the current blocker).

**GOTCHA that cost time:** a parked Debug **assert MessageBox** process holds `Tic.dll` and silently SKIPS the
next link (build reports 0 errors but Tic.dll is stale → you test the OLD binary). ALWAYS verify `Tic.dll`
mtime after building, and kill parked procs first. `taskkill`/`Stop-Process` can't kill them (JIT debug port);
use `cdb -p <pid> -c ".kill;q"`. Run asserting configs under `cdb -cf {sxd *; bu USER32!MessageBoxW ".kill;q"; g}`
so they die cleanly at the assert (no parked dialog, no DLL lock).

### ✅ ROOT FIX — UnitCreator-produced units were ownerless (user-diagnosed)

The dominant FindUnit/values-unit-expiry class was root-caused to a precise bug (via the FuncDC-result + FindUnit
unit-state dump): in operator `CreateResult`, the values unit produced by a **UnitCreator** was constructed and
dropped in the SAME statement:
```
resultHolder = CreateCacheDataItem(e, (*m_UnitCreatorPtr)(GetGroup(), args).get(), m_ValueComposition);
```
`CreateCacheDataItem` stores that unit only WEAKLY (`m_ValuesUnit`), and the UnitCreator's owning temporary dies at
the end of the statement -> the metric unit is **ownerless** and expires before the result is used (it dies WITHIN
the operator, which is why the during-operator SetNew capture masked it but the after-operator
`FuncDC_CreateResult` capture did not). Concrete repro: `categorical_unit.dms` `src/E := 1 * A`
(`mul(1u64, rlookup(...))`), values unit `[[/Kwartalen]]` = the `cat_range(...)` cache unit.

FIX (user-directed) at EVERY `m_UnitCreatorPtr` site: hold the unit, then have the kind-1 result own it:
```
auto v = (*m_UnitCreatorPtr)(GetGroup(), args);
resultHolder = CreateCacheDataItem(e, v.get(), m_ValueComposition);
resultHolder.KeepAlive(v); // TreeItemDualRef::KeepAlive -> DcRef::keepAlive (append to kind-1 vector)
```
Applied to all 8 sites: clc `OperAttrBin.h`, `OperAttrUni.h`, `OperAttrTer.h`, `OperAccBin.h` (x2),
`OperAccUni.h` (x2), `Cumulate.cpp` (x2). Added public `TreeItemDualRef::KeepAlive(shared_ptr<const TreeItem>)`.

**Result: the entire unit-expiry class is FIXED.** `categorical_unit`, `select_with_attr_by_org_rel_nested`,
`xml_parse`, `merge_indirect` etc. no longer hit FindUnit -- verified with the **SetNew capture REMOVED** (the
explicit KeepAlive replaces it; non-FuncDC NumbDC/StringDC params have statically unit-class-owned void/default
units, nothing to keep). `CaptureResultUnits()` at `FuncDC_CreateResult` is kept for non-UnitCreator result units.
Remaining sweep failures are now DIFFERENT classes: `ComplexNamespaces` = the teardown `NumbObjCache.empty()` Lisp
leak (namespace teardown cycle); `xml_parse` = rc=5 deprecation (poly/arc ValueComposition, pre-existing); a couple
of AVs (DoubleInstantiation/GridFromTemplate). NEXT: look for any non-`m_UnitCreatorPtr` ownerless-unit patterns,
then tackle the namespace teardown leak.

## Remaining steps

1. **Done: UnitCreator KeepAlive fix** (above) — the unit-expiry/FindUnit class. Re-run the full sweep for the count.
2. Fix the `centroid` DataItemRefContainer teardown-order assert.
3. Run `batch/TestDebugUnit.bat` to green (needs 1+2; note Debug asserts hang headless via MessageBoxW —
   every assert must be eliminated, OR route the CRT report mode to non-interactive for batch runs).
4. Review the agents' `// TODO ownership:` stopgaps (Explain.cpp m_UltimateDomain/ValuesUnit raw borrows kept
   alive by m_DataItem interest — acceptable; AbstrStreamManager/UsingCache locals owning — fine).

Headless unit-sweep driver (mirrors the harness, exit-code-checked, per-config `timeout -s KILL`) is in the
session scratchpad (`unit_sweep.sh`).

## Constraints

Build only via VS18 msbuild on `all22.sln` (Debug). **Never `git push`.** Edit in place (no worktrees).
Compiler pinned MSVC 14.50 (14.51 miscompiles geos). Update memory `project_stdptr_cascade_triage.md`.

## Delegation guidance

The cascade is large but rule-driven, so fan out to subagents — but agents can't run the canonical build,
so the loop is: **you build → bucket errors by file → delegate file-clusters with the rules → you rebuild
to verify.** Keep the intricate, cross-cutting pieces (DcRef/SupplInterest/interest accounting,
`DataController`/`MoreDataControllers` `m_Data`) yourself; delegate the mechanical borrowed/held `.get()` vs
`weak_tree_ptr` edits in leaf operator/storage/gui files. Tell agents: read `build_debug_stdptr<N>.log`,
grep for their files, apply the rules, do NOT build, mark anything ambiguous `// TODO ownership:` + `.get()`
stopgap, and never touch `*InterestPtr` typedefs or non-family pointers.

## 2026-07-02: shared_tree_ptr / weak_tree_ptr wrappers REMOVED (migration §15.5 executed)

User decision: reduce smart-pointer surprises by using the standard types directly. New canonical state:

- `SharedTreeItem` / `SharedMutableTreeItem` / `SharedDataItem` / `SharedUnit` / `SharedMutable*` /
  `ConstUnitRef` are now plain `std::shared_ptr<...>`; `WeakUnit` is `std::weak_ptr<const AbstrUnit>` (TicBase.h).
- `rtc/dll/src/ptr/SharedTreePtr.h` no longer defines wrapper types; it holds only free construction helpers:
  - `make_shared_tree(p, newly_obj{})` — adopt a freshly created object (first owner);
  - `make_shared_tree(p, existing_obj{})` — borrow an already-owned object (shared_from_this; throws bad_weak_ptr);
  - `make_shared_tree(p, no_zombies{})` — safe weak->strong, null if expiring;
  - `make_weak_tree(p)` — weak borrow from a raw pointer;
  - `MakeSharedFromBorrowedObjectPtr` (retargeted) and `lock_or_cancel(std::weak_ptr)` (throws task_canceled).
- All weak sugar (`operator->`, `get()/get_ptr()`, `operator bool`, `== raw`, `= raw`) is spelled out at call
  sites with `.lock()` / `.expired()` / `make_weak_tree(...)` — a hidden momentary lock no longer exists.
- `InterestPtr` (InterestHolders.h) routes raw-`T*` construction through `make_shared_tree` when `CPtr` is a
  `std::shared_ptr` (`is_std_shared_ptr_v`), never through `std::shared_ptr`'s raw ctor.

⚠ REGRESSION IN GUARDRAILS: the wrapper's `= delete` raw ctor used to make every rogue-control-block site
(`std::shared_ptr<T>(rawPtr)` double-managing a tree-owned object) a compile error. That guard is gone.
Discipline is now convention: NEVER construct a `std::shared_ptr` for a TreeItem-family object from a raw
pointer directly — always classify via `make_shared_tree`. Audit periodically:
`grep -nE "std::shared_ptr<(const )?(TreeItem|Abstr\w+|Unit<)[^>]*>\s*\(\s*[a-z_]\w*\s*\)" ...`
