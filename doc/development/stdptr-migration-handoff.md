# std-pointer ownership migration — session handoff

Branch: `refactor_ownership`. This continues the migration of the TreeItem family off the intrusive
`SharedPtr<T>`/`WeakPtr<T>` onto std ownership. Read this together with
`doc/development/std-ptr-migration-plan.md` (§4 = the `DcRef` variant) and `CLAUDE.md` (build rules).

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

## Remaining steps

1. Root-cause + fix the arg-DC interest regression above; re-run `reverse.dms /test_log` to clean exit.
2. Then watch for the prior config-teardown heap-corruption (memory `project_stdptr_cascade_triage.md`).
3. Run `batch/TestDebugUnit.bat` to green.
3. Review the agents' `// TODO ownership:` stopgaps (a handful in Explain.cpp m_UltimateDomain/ValuesUnit,
   XmlTreeOut.cpp di, AbstrStreamManager/UsingCache held-vs-borrow) for whether they should be
   `weak_tree_ptr` members rather than `.get()` borrows.
4. Remove the temp teardown instrumentation (dcmap_trace/g_DBG_ConfigRoot in TreeItem.cpp ~line 410+) once
   the teardown leak is settled. Then commit.

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
