# TreeItem config/cache separation — analysis and staged design

Branch `lookahead-scheduling` @ `b34b1267`, 2026-08-16. All `file:line` anchors below are valid at
that commit; prefer the named symbols when lines drift.

This note answers four questions posed for the `TreeItem` family:

1. Should `TreeItem` be separated into *CacheItems* and *ConfigItems* — possibly with `ConfigItem`
   deriving from `CacheItem`?
2. Should `check_set_ptr DataController::m_ImpliedChecks` move to the config side, given the working
   assumption that check_sets are only collected for config items at
   `AbstrCalculator::slSupplierExprImpl`?
3. Which member functions belong on which side?
4. How do `AbstrDataItem` and `Unit<V>` (both TreeItem descendants) cope — parallel type splits,
   config components by composition, or a template base class?

Short answers: **(1)** separate by *composition*, not by subclassing — grow the existing
`ConfigProperties` side-object into the full config-capability component; the subclass route is
recorded as a possible later stage with an explicit re-check list. **(2)** No — the assumption is
refuted; `m_ImpliedChecks` is a DC-graph memo and must stay on `DataController`; what *is*
config-side is the wrap decision (`TreeItem_CreateCheckedExpr` + `mc_IntegrityChecker`). **(3)** See
the grouping in §5; the surprises are the functions that straddle the boundary. **(4)** Composition;
a CRTP/template base eliminates no dispatch and doubles Class-registry entries; parallel splits are
blocked by the factory and metaclass machinery (§6).

The separable companion effort (collapsing the `Unit<V>`/`TileFunctor<V>` intermediate template
layers) is covered in [unit-hierarchy-collapse.md](unit-hierarchy-collapse.md).

---

## 1. The central finding: capability and location are orthogonal axes

Today "cache item" is not a type. It is the status flag `TSF_IsCacheItem` (`tic/TreeItemFlags.h:52`),
set **post hoc** by `TreeItem::SetIsCacheItem()` (`tic/TreeItem.cpp:566`) whose only caller is
`TreeItemDualRef::Set(ti, isNew=true)` (`tic/DataController.cpp:125`) — i.e. the moment a
DataController claims a freshly built result as its own. Children created afterwards inherit the flag
at creation (`InheritParentState`, `TreeItem.cpp:581-583`). Roots come from
`TreeItem::CreateCacheRoot()` (anonymous TokenID, `SetPassor()`, `TreeItem.cpp:2579`), used at ~13
operator sites in clc/geo.

Two independent properties emerge:

- **cache-location**: the item lives in an endogenous result tree (`TSF_IsCacheItem`); and
- **config-capability**: the item can carry a calculation rule / calculators / storage / usings /
  source location.

Three of the four quadrants are populated:

| quadrant | example | evidence |
|---|---|---|
| config-located, capable | every parsed .dms item | `stx/ConfigProd.cpp` |
| cache-located, incapable | plain data-operator results | children of the `CreateCacheRoot` sites |
| cache-located, **capable** | meta-operator / template instantiations | see below |
| config-located, incapable | (plain containers without config attrs — trivially) | |

The third quadrant is the decisive evidence against welding the two axes into one type hierarchy.
Verified path: in `SelectMetaOperator::CreateResultCaller` (`clc/Subset.cpp`), `resultHolder = res`
at `Subset.cpp:311` routes through `TreeItemDualRef::Set`, which calls `SetIsCacheItem()` on the
result unit when the holder is in `isNew` mode; `resSub->SetExpr(selectExpr)` at `Subset.cpp:354`
then writes a config attribute on a sub-item created under that flagged root (`:334`). `ForEach.cpp`
does the same (template expr copies via `DataCopyMode::CopyExpr` at `:293`, `iter->SetExpr(...)` at
`:323`). Conversely, instantiated-into-cache subtrees need name resolution
(`UsingCache::FindNamespace` has dedicated cache branches, `tic/UsingCache.cpp:488,505,511`),
copied source locations (`TreeItem.cpp:2729-2730`) and `mc_OrgItem` (`:2682-2683`).

Consequence for the current code: `GetOrCreateConfigProperties()`'s
`assert(!IsCacheItem())` (`TreeItem.cpp:179`) is **latently refutable** on the route where a meta
operator runs with a fresh (`isNew`) holder. It has not fired in practice because the usual route
instantiates via the `SetTmp` arm directly into the config item (DcRef arm 4,
`tic/TreeItemDualRef.h:72`) — the flag is then never set on the written items. Stage C1 below
resolves this deliberately rather than by accident.

## 2. Current state of the separation

`TreeItem.h:617` already declares the intent:

```cpp
public: // TODO G8: encapsulate and move config attr (aka mc_ ) into a separate ConfigTreeItem class
```

and five of the eight `mc_*` members were already extracted into a lazily-allocated side-object
(`TreeItem.h:626-634`):

```cpp
struct ConfigProperties
{
    SharedStr          mc_Expr;             // configuration-time calculation rule
    AbstrCalculatorRef mc_Calculator;
    AbstrCalculatorRef mc_IntegrityChecker;
    AbstrCalculatorRef mc_SizeExpectation;
    AbstrCalculatorRef mc_SizeUpperbound;
};
mutable std::unique_ptr<ConfigProperties> m_ConfigProperties;
```

with null-safe readers (`GetExprMember` etc., `TreeItem.cpp:185-225`) and the create-side assert at
`TreeItem.cpp:179`. **This is the model this design extends** — `ConfigProperties` *is* the
ConfigItem, as a component.

## 3. Data-member classification

| member | decl | verdict |
|---|---|---|
| `m_ItemCount`, `m_Producer` | `TreeItem.h:158-160` | shared runtime (production scheduling) — stay |
| `m_ID`, `m_Parent`, `m_FirstSub`, `m_Next` | `:587-607` | shared identity/tree — stay |
| `m_BackRef` | `:595` | cache-root-only wiring (back-pointer to the owning config item) — stay |
| `m_SupplCache` | `:612` | runtime, read on cache items (`TicInterface.cpp` error-source walk) — stay |
| `m_ReadAssets` | `:615` | cache/runtime (parked read contexts) — stay |
| `m_StatusFlags` | `:620` | shared 32-bit word; USF_*/DSF_* overlap is per-*descendant* (`TreeItemFlags.h:80-91`) — stay; never partition |
| `mc_DC` | `:649` | **shared wiring, not a config attr** → rename `m_DC` (C2) |
| `mc_RefItem` | `:650` | **shared wiring** (config→cache and operator-set on cache sub-items) → rename `m_RefItem` (C2) |
| `mc_OrgItem` | `:650` | config-side (set only under `InTemplate()` in `Copy`, `TreeItem.cpp:2682-2683`) → move into ConfigProperties (C3) |
| `m_Location` | `:654` | config-side → move into ConfigProperties (C3) |
| `m_StorageManager` | `:614` | config-side (creation location-gated at `TreeItem.cpp:5491`) → move (C4) |
| `m_UsingCache` | `:613` | config-capability (cache users are exactly the capability-bearing instantiations) → move (C5) |
| `m_ConfigProperties` | `:634` | the component itself — grows |

Evidence that `mc_DC`/`mc_RefItem` are not config attrs despite the prefix: operators write them on
cache items (`Subset.cpp:310` `SetDC`; `ConnectedParts.cpp:110,403`, `Connect.cpp:792`,
`PhaseContainer.cpp:113` `SetReferredItem`); `StartInterest`/`StopInterest` read both unconditionally
(`TreeItem.cpp:5584-5585, 5620-5624`); and the documented
`cacheItem.mc_DC → DC → DC.m_OwnedData → cacheItem` cycle
([teardown-leak-and-ownership-cycles.md](teardown-leak-and-ownership-cycles.md), §"2-cycle").

Memory effect of C3-C5: pure cache items and attr-less config containers lose 8+8+8+16 = **40 bytes
each**; items with any config attr pay only the already-existing single allocation. No population
gets bigger.

## 4. `m_ImpliedChecks` — verdict: stays on `DataController`

The working assumption — *"check_sets are only collected for ConfigItems at
`AbstrCalculator::slSupplierExprImpl`"* — is **refuted on both halves**:

- `slSupplierExprImpl` (`tic/AbstrCalculator.cpp:937-977`) contains no check_set code. A repo grep
  for `ImpliedChecks` hits only the declarations (`DataController.h:72,90`) and one function.
- That function, `DataController::GetImpliedChecks()` (`tic/MoreDataControllers.cpp:557-645`), is a
  meta-thread-only, lazily **memoized post-order fold over the DC argument graph** that writes
  `m_ImpliedChecks` on *every visited DC*: leaves get the shared empty set (`:575`), FuncDCs get the
  shared-or-union of contributing args plus their own condition when the node is an
  `integrity_check` application (`:622-638`). The visited DCs are sub-expression/cache DCs, not
  config items, and sets are deliberately shared between DCs (`:625`). Re-keying the memo from DC
  identity to config-item identity would destroy the sharing and the "each node folds once per DC
  lifetime" contract (`:495-497`) — strictly worse.

The kernel of truth in the assumption: integrity-check **wrapper nodes** enter key expressions
(almost) only at config-item references. `slSupplierExpr` rejects cache items outright
(`AbstrCalculator.cpp:905-922`), and the wrap is built by the static
`TreeItem_CreateCheckedExpr` (`TreeItem.cpp:3099-3137`) — the **sole consumer** of
`GetImpliedChecks()` — reached only from `TreeItem::UpdateDC` (config-gated at `:3141`) and
`TreeItem::GetCheckedKeyExpr`.

Corrected statement for future reference:

> IntegrityCheck wrapper nodes are introduced only for config-tree TreeItems, by
> `TreeItem_CreateCheckedExpr`, reached from `UpdateDC`/`GetCheckedKeyExpr`;
> `slSupplierExprImpl` merely splices the already-wrapped key expression of a referenced config item.
> The `check_set` itself is a lazily-derived, memoized property of **every** DataController in the
> contributing arg graph, computed and stored by `DataController::GetImpliedChecks`.

What moves instead (stage C6): the wrap logic (`TreeItem_CreateCheckedExpr`,
`TreeItem_HasIntegrityCheckerInclAncestors`, the `UpdateDC`/`GetCheckedKeyExpr` fold) relocates into
the config TU so the separation is visible in the file layout. `mc_IntegrityChecker` is already in
`ConfigProperties`.

Incidental observations recorded while verifying (not acted on):

- `slSupplierExprImpl` returns a bare `CreateLispTree(supplier, ...)` **without** a check wrap for
  suppliers that are not passors / have no calculator / are neither data item nor unit
  (`AbstrCalculator.cpp:963-964`), whereas `GetCheckedKeyExpr`'s structurally identical fallback
  (`TreeItem.cpp:3237` + `:3240-3241`) *does* wrap. Possibly an intentional exemption; worth a
  deliberate look someday.
- `doc/IntegrityCheck.md` documented a nonexistent `ImpliesCheck(LispPtr)` accessor (fixed
  2026-08-16; the real API is `GetImpliedChecks()` plus `InsertCheckAtoms`/`AreCheckAtomsImplied`,
  `DataController.h:49-50`).

## 5. Member-function placement

`TreeItem` has ~150 public member functions. They divide as follows (representative members; the
full inventory lives in the session exploration and can be regenerated by grepping the anchors):

**(a) Config-capability (~60)** — stay public methods of `TreeItem`, backed by the component;
read-side null-safe, write-side capability-creating; bodies consolidate into the config TU in C6:
expr/calculator family (`SetExpr`/`GetExpr`/`SetCalculator`/`MakeCalculator`/`HasCalculator*`,
member accessors `TreeItem.cpp:185-225`), integrity/size accessors, storage family
(`GetStorageManager`, `GetStorageParent`, `IsLoadable`/`IsStorable`/…), usings (`AddUsing*`,
`GetUsingCache`), location accessors, `CreateConfigRoot`, `RemoveFromConfig`, XML/DMS dump,
template/function flags, config-gated orchestration (`UpdateDC`, `GetCheckedKeyExpr`,
`GetCurrMetaInfo`, `UpdateMetaInfoImpl`).

**(b) Shared/cache (~70)**: tree building/navigation (`AddItem`, `CreateItem*`, walks), naming
(`GetFullCfgName` hops `m_BackRef` for cache roots), the whole Actor/update/interest surface, data
prep/cleanup and keep/free-data flags, referred-item chains (`SetDC`, `SetReferredItem`,
`GetCurrUltimateItem`, `GetCheckedDC`), blob streams (`LoadBlobBuffer` asserts `IsCacheRoot()`,
`TreeItem.cpp:5677`), `CreateCacheRoot`, `SetIsCacheItem`, `InheritParentState`.

**(c) Boundary cases — the surprises**:

| function | why it straddles |
|---|---|
| `SetExpr` | group-(a) function legitimately called on cache-*located* items (`Subset.cpp:354`, `ForEach.cpp:323`) — the single strongest argument against a hard type split |
| `Copy` / `CopyTreeContext` | crosses config→config, config→cache, and cache→config (`CopyTreeContext.h:27-28`; fence in `PhaseContainer.cpp:58`); uses `GetDynamicClass()` (`TreeItem.cpp:2596`) |
| `GetUsingCache` / `FindNamespace` | capability-side with explicit cache-context branches (`UsingCache.cpp:488-511`) |
| `FindItem` (absolute) | location-gated (`MG_CHECK(!IsCacheItem())`, `TreeItem.cpp:2243`) |
| `PartOfInterest` / `TryCleanupMem` / `DoFail` | one function, two behaviours branching on `IsCacheItem()` (`TreeItem.cpp:5014-5022, 5026, 4164-4171`) — stay shared |
| `HasCalculatorImpl` | null-safe capability read valid on all items; its `DC_BackPtr` comments were stale (fixed 2026-08-16) |

## 6. Why not the subclass split (Option A) now — and what would reopen it

What A gets right: capability-as-type is coherent with the quadrant model. `ConfigItem ⊂ CacheItem`
would read "every config-capable item is also cache-hostable", and `Copy` via `GetDynamicClass()`
would automatically produce capability-bearing instances when instantiating templates into cache.

Why not now:

1. **`Copy` crosses the boundary in both directions.** A capability-less cache item copied into the
   config tree (`DataCopyMode` "sub-items from cache") would yield a config item that can never
   receive an expr; A needs class-remapping logic inside the highest-risk function in the file.
2. **Factory/metaclass cost.** One `Class` object == one constructor
   (`mci/Class.h:85-86`; `CreateAndInitItem` → `Class::CreateSharedObj`). TreeItem's only
   descendants are `AbstrUnit` and `AbstrDataItem` (`IMPL_DYNC_TREEITEMCLASS` exactly twice);
   parallel splits or a CRTP base double ~22 `UnitClass` + `DataItemClass` registry entries, and
   `AbstrDataObject::base_type == AbstrDataItem` (`AbstrDataObject.h:66`) welds the data-object
   metaclass chain to the data-item class.
3. **C-API stability.** ~15 `CheckPtr(self, AbstrDataItem::GetStaticClass(), …)` guards and external
   `IsKindOf` expectations survive composition untouched.
4. **Enforcement honesty.** Location is assigned post hoc and inherited; the location-driven gates
   (`UpdateDC` `:3141`, `DoUpdate` `:3604`, blob streams `:5677`, …) remain runtime checks under A
   too — the compile-time win covers only the capability half.

Re-check list if A is attempted later (after C6): the `Copy` class-mapping problem; the Class-registry
doubling; C-API `IsKindOf`/`CheckPtr` compatibility; whether the C6 file layout has already made the
capability surface small enough that a type is worth it.

## 7. `AbstrDataItem` and `Unit<V>` under the recommendation

- **`AbstrDataItem`** — nothing to do beyond documentation. Its only config-authored members are the
  two TokenIDs `m_tDomainUnit`/`m_tValuesUnit` (`AbstrDataItem.h:179-180`; empty for cache items,
  `DataItemClass.cpp:153-154`, unit refs assigned directly at `:159-160`). ~8 bytes; a per-descendant
  config part would cost a pointer for zero net. Everything else is runtime/shared.
- **`Unit<V>`** — its "config" members are shared in practice and stay put: `SetMetric` is called on
  cache result units throughout clc; `m_Crs` lives on `AbstrUnit` *precisely so* worker threads can
  set it on cache units (`AbstrUnit.cpp:130-134`, and
  [crs-metric-decoupling.md](crs-metric-decoupling.md)); `m_RangeDataPtr` is dual-use — declared
  `range` property vs operator-computed — with `USF_HasConfigRange` (`TreeItemFlags.h:85`) as the
  discriminator. **Do not create the once-contemplated `mc_RangeDataPtr` twin** (the stale trailing
  comment at `Unit.h:62` is removed by the hierarchy-collapse work); a declared-vs-computed range
  twin would be a functional change, not a separation. `m_Metric`/`m_Projection` are config-declared
  but answered through cache units via referred-item delegation (`Unit.cpp:695-702, 1035-1060`),
  which any move would have to preserve — so they stay.
- **No template base / CRTP**: the config members are plain storage behind null-safe accessors; there
  is no static-polymorphism dispatch to eliminate, and every extra instantiated base costs a Class
  registration it cannot pay for.

## 8. Migration stages

Each stage is a separately buildable commit: build `all22.sln` only (VS18 msbuild, serial, Debug x64
on the laptop), user runs `testcases\run_testcases.bat` (mind its Release-exe default) and the batch
suites; Linux build on OVSRV10 before merging to main.

- **C1 — invariant repair + hygiene (no structural change).** Decide the ConfigProperties invariant:
  recommended `assert(!IsCacheItem() || TreeItem::s_MakeEndoLockCount)` at `TreeItem.cpp:179`, with a
  comment citing `Subset.cpp:354`/`ForEach.cpp:323`. (Rejected alternative: delaying
  `SetIsCacheItem` until `CreateResult` completes — it changes what `IsCacheItem()` observes during
  instantiation-time name resolution and interest propagation.) First *investigate* exactly when
  `SelectMetaOperator` runs with an `isNew` holder vs the Tmp arm, and pin the cache-DC route with a
  testcase. Update the `TreeItem.h:622-625` comment ("config-only" → "config-capability; also on
  meta-operator-instantiated endogenous items").
- **C2 — rename `mc_DC` → `m_DC`, `mc_RefItem` → `m_RefItem`.** Mechanical, ~72 occurrences, all
  under `rtc/dll/src` (clc/geo use accessors only); update doc mentions.
- **C3 — move `mc_OrgItem` + `m_Location` into ConfigProperties.** Null-safe readers
  (`GetLocation`/`GetConfigFileName`/… fall back to null/0); write sites `TreeItem.cpp:2682-2683`,
  `:2729-2730`, `:5742`; convert `Xml/XmlTreeOut.cpp` direct `mc_OrgItem` reads to accessors.
  Verify: battery + GUI smoke (source-location tooltips, "original item" display).
- **C4 — move `m_StorageManager`.** Keep `ASF_GetStorageManagerLock` on `Actor::m_State`; keep the
  `:5491` location asserts; audit the `AbstrStorageManager` friend (`TreeItem.h:659`). Verify with
  storage-heavy cases plus the teardown-sensitive configs from the teardown doc (leak counters).
- **C5 — move `m_UsingCache`.** `GetOrCreateUsingCache` becomes capability-creating (covers `Copy`
  `:2680` on function copies into cache); audit the `UsingCache` friend (`TreeItem.h:658`). Verify
  with fn_test_fe_* / fn_test_selmeta* / template regressions (name resolution in instantiated
  scopes).
- **C6 — file split (pure code motion).** New TU `rtc/dll/src/tic/TreeItemConfig.cpp` (add to the
  DmTic vcxproj *and* CMake — build both toolchains; file-list drift is the failure mode); move
  group-(a) bodies + `TreeItem_CreateCheckedExpr` + the `UpdateDC`/`GetCheckedKeyExpr` fold there;
  restructure `TreeItem.h` into labeled sections (shared core / config capability / cache wiring);
  resolve the `:617` TODO text. No signature or export changes; C-API untouched.
- **C7 — enforcement upgrade.** `m_ConfigProperties` private + friend audit; introduce
  `IsConfigCapable()` distinct from `!IsCacheItem()`; sweep the gates
  (`:3141, :3604, :4028, :2973, :4867, :1061, :2056, :2922, :2243, :5491`) and annotate each as
  LOCATION-gate (stays flag-based) or CAPABILITY-gate (becomes presence-based).

## 9. Risks

1. C1's invariant choice needs explicit owner sign-off — it concedes the "config-only" wording of
   the original ConfigProperties extraction was too strong.
2. Storage-manager move vs session-teardown ordering (C4 verification list).
3. `run_testcases.bat` defaults to the Release exe while the laptop builds Debug — always confirm
   the "Testing \<path\>" banner.
4. The `m_StatusFlags` USF/DSF bit overlap forbids any per-side flag partition.
5. Every moved member's write path must remain reachable under the endo-phase allowance
   (PhaseContainer copies use `cpy_mode` without expr copying — verified via
   `CopyTreeContext.h:64-69` + `PhaseContainer.cpp:58`).
6. Exported inline functions touching renamed members (audit the TIC_CALL surface in C2).

## 10. Stale artifacts

Fixed 2026-08-16 (with this doc): `TreeItem.cpp` invariant comment referencing removed
`TSF_AutoDeleteDisabled`; the two stale "mc_Calculator … DC_BackPtr" comments in
`HasCalculatorImpl`; `TreeItem.h` `// override PersistentSharedObj` (type no longer exists);
`doc/IntegrityCheck.md` `ImpliesCheck`; `schedule-with-lookahead.md` line-pinned "TreeItem.h:625".
Remaining, tied to later stages: the `TreeItem.h:617` TODO text itself (C6);
`tile-data-retainment.md` class-tree refresh (lands with the hierarchy-collapse U4).
