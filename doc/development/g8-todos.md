# The G8 TODO backlog

*Status: index, 2026-08-21. Branch `lookahead-scheduling`.*

## What "G8" means

`// TODO G8` marks work that was identified during the GeoDMS 8.0 renovation (first half
of 2022) and deliberately deferred past it. **It is not a version gate.** The product is
at 20.16.0; nothing is waiting for a release called "G8". The marker means "this is
structural debt we chose not to pay while landing 8.0", and it has been carried forward
unexamined ever since.

`// TODO G8.5` is a second tier: items deliberately sequenced *after* another G8 item,
because they depend on it. The G8.5 markers cluster on exactly two blockers:

- **CalcCache restoration** — retired, see [§4](#4-history-retired-mechanisms). The
  blocker is gone because the mechanism is gone, not because it was delivered.
- **The ownership-direction flip** (`mci/Object.h:108`) — *already landed* in the
  std-ptr migration. `TreeItem.h:598-607` documents the current state: a parent owns
  `m_FirstSub`, each child owns `m_Next`, and `m_Parent` is a `std::weak_ptr`.

So neither G8.5 blocker is live. The 5 remaining G8.5 markers should be re-tiered or
deleted rather than left waiting: `mci/Object.h:108` (stale, see §2), `sym/LispList.h:157`,
and the three in `tic/DataLocks.h` (`:31`, `:109`, `:203`).

**Current count: 71 markers in 37 files** — rtc 25, clc 4, geo 4, stg 2, shv 1, stx 1 —
plus references in `doc/`. It was 72 in 38 files before the CalcCache retirement (#1189)
removed the `ASF_WasLoaded` marker.

## Reading this index

Three clusters are **already designed elsewhere**. Do not re-plan them; follow the
existing ladder:

| cluster | authority |
|---|---|
| `TreeItem.h:617` — ConfigTreeItem split | `doc/development/config-cache-separation.md`, stages C1–C7 |
| `TicBase.h:84` — `DataArray<V>` → `TileFunctor<V>` | `doc/development/unit-hierarchy-collapse.md`, stage U6 |
| `TicBase.h:33` — `TICTOC_CALL` | `doc/development/tu-reorg-and-export-surface-2026-08.md` §2 |

Counts in the tables below are **live call sites** — declarations, commented-out code and
doc mentions excluded. That is the number that actually has to be edited.

---

## 1. Themes

### A. Retired-mechanism fossils — *closed*

| site | marker | resolution |
|---|---|---|
| `tic/TreeItemFlags.h:32` | `ASF_WasLoaded` — "G8.5 ? REMOVE AFTER CalcCache restoration" | **Removed** in #1189; the bit is reserved, not reused. See [§4](#4-history-retired-mechanisms). |
| `tic/DataLocks.h:31` | "FileData primitives; G8.5: Move to DataStoreManager" | **Retired** in #1189: there is no destination class to move them back to. `OpenFileData`/`CreateFileData` are live memory-mapped-storage primitives and tic is where they belong; the banner now says so. |

### B. Vocabulary renames — XL, mechanical

| site | asks for | live sites |
|---|---|---|
| `tic/TicBase.h:84`, `mci/CompositeCast.h:31` | `DataArray<V>` alias → `TileFunctor<V>` | **~645 spellings / ~101 files** (clc 259, geo 254, stg 51, rtc 47, shv 32, stx 2) |
| `tic/AbstrCalculator.h:96` | `AbstrCalculator` → `AbstrExprKey` | 161 occ / 32 files |
| `clc/ExprCalculator.h:19` | `ExprCalculator` → `ParsedExprKey` | — |
| `tic/DC_Ptr.h:43` | `DC_Ptr` → `AssignedExprKey`, "or remove" | — |
| `stx/DataBlockTask.h:38` | `DataBlockTask` → `DataBlockExprKey` | — |
| `tic/DataLocks.h:109,203` | `DataReadLock`/`DataWriteLock` → `…Handle` (G8.5) | **704 occ / 134 files**; the `…Handle` aliases already exist, so this can be adopted per TU |
| `tic/TicBase.h:33` | remove `TICTOC_CALL` | 31 uses, **all inside `tic/DataArray.h`** |
| `mci/Object.h:29` | rename `TreeObject`, separate containment from shared ownership | comment-only design note |

Notes:

- `DataArray<V>` is a type alias, so a global rename is semantics-preserving — aliases
  cannot be specialized. The costs are practical: it sits in both ClcPCH and GeoPCH, so
  every commit is a full clc+geo rebuild, the diff conflicts with any concurrent branch,
  and a few sites use it in dependent-type position (`typename
  DataArray<E>::locked_cseq_t`, `geo/Connect.cpp:823,839`).
- `TICTOC_CALL` looks like the cheapest item here and is not. On MSVC, dllexport forces
  instantiation of decorated template members, and these 31 are exactly the
  template-instantiation farm that serves Clc and Geo. It is a *measurement* task first
  — does clc/geo still link without them? — needing both an MSVC and an OVSRV10 Linux
  link. `tu-reorg` §2 excluded this family from its otherwise-completed step B for that
  reason.

### C. Encapsulation and class splitting — L

| site | asks for |
|---|---|
| `tic/TreeItem.h:617` | move config attrs (`mc_`) into a separate ConfigTreeItem — **designed**, see C1–C7 |
| `tic/TreeItem.h:520` | re-encapsulate: a `private:` immediately overridden by `public:` for 12 internal helpers |
| `tic/TreeItem.h:584` | encapsulate the identity/tree members — the `//private:` is itself commented out |
| `tic/AbstrDataItem.h:183` | re-encapsulate `m_DataObject`, `m_DataLockCount`; the 5 `friend` declarations below already describe the intended private surface |
| `tic/AbstrDataObject.h:37` | move to `rtc/ptr/`, or replace by the Good Coding Guide equivalent |
| `throwItemError.h:15` | "Why here" — move to a separate header |
| `tic/DataArrayValue.h:6,95` | move `GetValue`/`SetValue` to a separate header — partly done already |

Sequencing: do `TreeItem.h:520`/`:584` and `AbstrDataItem.h:183` **after** C6 has moved
the config bodies out. `config-cache-separation.md` §5 lists six genuinely ambiguous
boundary cases (`SetExpr`, `Copy`/`CopyTreeContext`, `GetUsingCache`/`FindNamespace`,
absolute `FindItem`, `PartOfInterest`/`TryCleanupMem`/`DoFail`, `HasCalculatorImpl`)
whose placement is not obvious; deciding them twice is wasted work.

### D. Lock/handle API surface reduction — M

| site | asks for | live sites |
|---|---|---|
| `tic/AbstrDataObject.h:95` + `tic/DataArray.h:109` | remove `GetReadableTileLock` | **1** (`stg/gdal/gdal_vect.cpp:2216`) |
| `tic/AbstrDataObject.h:96` + `tic/DataArray.h:110` | remove `GetWritableTileLock` | **4** (`geo/OperPolygon.cpp:990,1011`; `tic/TileChannel.h:80,101`) |
| `tic/AbstrDataObject.h:332` | remove `struct ReadableTileLock` | **22 constructions / 8 files**, plus a typedef |
| `tic/AbstrDataObject.h:339` | remove `struct WritableTileLock` | **3** (`geo/Connect.cpp:851,853`; `geo/OperPot.cpp:227`) |
| `tic/DataArray.h:112` | substitute away `GetLockedDataRead` → `GetDataRead` | **~107 / 28 files** |
| `tic/DataArray.h:113` | substitute away `GetLockedDataWrite` → `GetDataWrite` | **6** |
| `tic/DataLocks.h:105` | remove `DataReadLock::m_RefPtrLock` | see below |
| `tic/DataLocks.h:181` | remove `DataWriteLock::GetItem` | **1** (`tic/OperationContext.cpp:2582`, inside an assert) |

Two traps:

- `ReadableTileLock` is **not** a call-site sweep: `tic/TileIter.h:61` bakes it into
  `typedef locked_seq<cseq_t, ReadableTileLock> locked_cseq_t`. And most of its uses are
  "pin this tile while I hold raw pointers into it" inside tight geo loops — check each
  against `doc/tile-data-retainment.md` §5, because for a `LazyTileFunctor` source,
  dropping the lock frees the buffer.
- `m_RefPtrLock` is guarded by the 25-line comment at `DataLocks.h:80-103`, which explains
  that member *ordering* is load-bearing: `m_KeepItemAlive` is declared first so it
  destructs last, guaranteeing neither `m_DRLA` nor `m_RefPtrLock` is ever the item's
  last owner. Removing it means proving the `m_ItemCount` guard it provides is redundant
  — a lifetime argument, not a grep.

`GetLockedDataRead`/`GetLockedDataWrite` are one-line forwarders and by far the cheapest
volume win. One caveat from `unit-hierarchy-collapse.md` §U1: an argument-less
`GetLockedDataWrite()` against the two-parameter, no-default declaration was *the*
uninstantiable dead code that broke explicit instantiation. Verify each of the 6 sites
passes both arguments.

### E. Dead code — S

| site | note |
|---|---|
| `tic/DataController.cpp:519` | `CalcResultWithValuesUnits` is a husk: fail-check, `CallCalcResult()`, null-check, then a large commented-out block that used to do the `UpdateValuesUnits` work the name promises. **3 call sites** (`tic/MoreDataControllers.cpp:720`, `tic/AbstrCalculator.cpp:6257`, `shv/Theme.cpp:475`), all documented as bare-retainer idioms in `doc/interest-and-futures.md` §2.5b. Callers rely on the null return for `WasFailed(Data)`, which `CallCalcResult` does not reproduce. |
| `tic/TreeItem.cpp:988` | `CanSubstituteByCalcSpec` — substitute away; 3 occ / 2 files |
| `tic/AbstrDataItem.h:120` | `REMOVE` sits above two *already commented-out* one-liners (`IsTiled`, `IsCurrTiled`, both trivially `GetAbstrDomainUnit()->IsTiled()`). Delete the corpse. |
| `sym/LispList.h:157` | `REMOVE` (G8.5) |
| `dbg/DebugReporter.cpp:70` | move `ReportCount` into `DbgInterface.h` — flagged "TODO **RECOMPILE**", so batch it with another full-rebuild sweep |
| `mem/tiledata.h:27,34` | replace `tile`/`file_tile` container inheritance by `OwningArrayPtr` — **not cheap despite looking it.** `tile<V>` backs every heap-materialized result and `file_tile<V>` owns the mmap'd sequences with hand-managed re-mapping; the commented-out `, TileBase` base is evidence of an abandoned prior attempt. Low priority. |

### F. Correctness and robustness — M

| site | asks for |
|---|---|
| `tic/TicDataSupport.cpp:470` | **use `info->changePos`.** `CopyData` ignores its `DomainChangeInfo*` entirely and copies the whole array, truncating or zero-filling to the new size. It therefore *silently produces wrong data for any domain change that is not a pure append or truncate.* Its only caller, `AbstrDataItem::OnDomainUnitRangeChange:852`, is itself half-finished (a commented-out `MG_CHECK2(false, "NYI: …")`). Pin what `changePos` means for insertion vs deletion vs range-shift first: small implementation, real design question. |
| `tic/AbstrDataItem.cpp:868,893` | two blocks copied verbatim from `TreeItem::TryCleanupMemImpl` — reorder and de-duplicate |
| `tic/AbstrDataItem.cpp:886,902` | can the small-object early-out lean on `CleanupMem`; should `CleanupMem` merge with `ClearDataObject` |
| `tic/TreeItem.cpp:899` | re-evaluate for thread and exception safety: set up private, commit in a nothrow critical section or lock-free |
| `tic/TreeItem.cpp:894` | avoid constructing `SourceDescr(...)` at this phase |
| `tic/TreeItem.cpp:1086` | going to `variant` and back looks contrived; re-evaluate the types |
| `tic/TreeItem.cpp:4310` | unwind recursion — belongs to `RECURSION_REFACTOR_PLAN.md` |
| `tic/AbstrUnit.cpp:364` | two commented-out `Was(ProgressState::MetaInfo)` asserts to re-enable |
| `stg/gdal/gdal_vect.cpp:1800` | remove the lazy-init `if`, which `GdalVectlMetaInfo` should have made unnecessary. Carries a nested `// TODO: Lock.` — the lazy init is also unsynchronized, which is a latent MT bug either way. |
| `tic/OperPolicy.h:16` | reconsider `dont_cache_result` → `CompoundDC`, and the use of `calc_requires_metainfo`. **A design question, not a task** — and the `calc_requires_metainfo` half (it forces data generation onto the main thread) is a scheduling constraint that belongs in `schedule-with-lookahead.md`. Write the analysis note first. |

`AbstrDataItem.cpp:868-902` deserves particular care: it is the data-retention decision
point (`doc/tile-data-retainment.md` §1, `doc/interest-and-futures.md` §2.6). A change
there surfaces as memory regressions or recalculation storms — neither of which the unit
suite catches.

### G. Performance, per operator — S each

**These are the G8 items a contributor can pick up without cross-cutting knowledge.**
Each is local to one operator or one function, independently schedulable, and needs its
own before/after measurement.

| site | asks for |
|---|---|
| `geo/BoostPolygon.cpp:1238` | `parallel_for` + thread-local `clean_resources` over the result-extraction loop. Verify boost.polygon's `clean_resources` thread-safety rather than assuming it. |
| `geo/Poly2GridOper.cpp:782,868` | avoid re-rasterizing polygons that intersect multiple tiles, e.g. an ordered heap processing neighbouring tiles together. Highest payoff and highest effort in this group. |
| `geo/ConnectedParts.cpp:139` | "doe dit zoals in OperDistrict.cpp" — the union-find currently reads both node attrs whole and allocates 5 full-size vectors |
| `geo/Point.cpp:201` | generalize `DistOper` to `IPoint`/`UPoint`/`WPoint` and their square-dist types. Watch obj size and compile time; pick each `square_dist_type` so it cannot overflow. |
| `clc/Union.cpp:184` | copy tile by tile instead of a whole-array `GetDataWrite(no_tile, read_write)`. **Carries its own blocker:** "this will break non covering tilings and non-sequential tilings" — needs a tiling-compatibility predicate first. |
| `clc/OperUnit.cpp:256` | don't use `CreateTmpUnit` (it calls `SetMaxRange`); use `CreateResultUnit` and copy/intersect the range inside `DuplFrom` with `mustCalc` |
| `clc/OperUnit.cpp:257` | schedule no calc phase for operators that give a complete result at meta-info time. **This is the `materialization::meta` case — raise it into `schedule-with-lookahead.md`, don't fix it locally.** |
| `clc/OperAccUniNum.h:76` | "use parallel_for and ThreadLocal container" — **likely stale**, see §2 |
| `stg/str/StrStorageManager.cpp:122` | make `dataBegin` a tile handle with a void pointer, so the mapping stays pinned across the `fwrite`. Same fix as `AbstrDataObject.h:154`. |
| `tic/AbstrDataObject.h:154` | add a `TileCRef& resourceHolder` parameter to avoid file (un)mapping per row |
| `shv/ShvDesktopData.cpp:406` | avoid two heap allocations by making the posted action move-only instead of wrapping `ItemWriteLock`s in `make_shared` to satisfy a copyable lambda |
| `tic/DataArray.cpp:765` | merge with `slUnionDataLispExpr`; consider removing the function from the `AbstrDataObj.h` interface |
| `tic/DataArray.cpp:807` | drop the `has_fixed_elem_size_v<V>` restriction, especially for `SharedStr` parameters |
| `tic/OperationContext.cpp:2407` | count the reserve better |
| `tic/LispTreeType.cpp:332,335` | move to `token::` (note: spelled `TOOD` in both) |

---

## 2. Markers that are already stale

Three describe a world that no longer exists. Verify, then delete the comment — do not
schedule work.

| site | why it is stale |
|---|---|
| `mci/Object.h:108` | "now=subitems shared-own their parents" — the flip already landed. `TreeItem.h:598-607`: parent owns `m_FirstSub`, each child owns `m_Next`, `m_Parent` is a `std::weak_ptr`. |
| `clc/OperAccUniNum.h:76` | asks for `parallel_for` + a thread-local container; the code immediately below already does `MaxAllowedConcurrentTreads()` + `AggregateTiles(..., maxNrThreads)`. Establish whether the TODO predates `AggregateTiles`. |
| `tic/AbstrDataItem.h:120` | `REMOVE` over code that is already commented out |

`mci/Object.h:140` (`Move to AbstrDataItem`) is *not* stale but is easy to
under-estimate: `GetDynamicObjClass`/`GetCurrentObjClass` are two vtable slots on the
universal `Object` base that only `AbstrDataItem` meaningfully overrides (base impls in
`mci/MciInterface.cpp`, where `GetCurrentObjClass` just forwards). Removing two virtuals
from every `Object` in the system is a real win, but the surface is 50 uses across 21
files, and the blocker is not the count: a substantial share call them on `TreeItem`- or
`Object`-typed expressions rather than `AbstrDataItem*` — the `DMS_*` C-API in
`mci/MciInterface.cpp`, plus `tic/TreeItem.cpp`, `tic/AbstrCalculator.cpp`,
`tic/MoreDataControllers.cpp`, `tic/DataController.cpp`, `tic/OperationContext.cpp`,
`tic/TicInterface.cpp`, `tic/TreeItemSet.cpp` and `tic/OperGroups.cpp`. Those probably
need a non-virtual `TreeItem`-level shim, which is the design question to settle first.

---

## 3. Sequencing constraints

**`tic/AbstrCalculator.cpp` is contended.** At 7075 LOC it is the largest TU in the repo,
and three separate efforts stake a claim on the same functions:

1. the "dismantle Calculators" TODO (`AbstrCalculator.h:65`, `AbstrCalculator.cpp:176`),
2. the deferred TU split (`tu-reorg` step D — deferred precisely because of its dense
   file-local-static graph),
3. `RECURSION_REFACTOR_PLAN.md` open problem #1: the `SubstituteExpr_impl` ↔
   `slSupplierExprImpl` recursion, fix C1b, unstarted.

Whichever goes first must land alone. The *rename* half of theme B is mechanical and safe
at any time; the *dismantle* half should not start before C1b and the TU split.

**Suggested low-risk starter batch** — provably semantics-preserving, clears roughly 15%
of the marker count:

1. `DataWriteLock::GetItem` (`DataLocks.h:181`) — 1 call site.
2. `CalcResultWithValuesUnits` (`DataController.cpp:519`) — 3 call sites.
3. The three stale markers in §2.
4. `GetLockedDataRead`/`GetLockedDataWrite` → `GetDataRead`/`GetDataWrite`, one commit
   per DLL.

---

## 4. History: retired mechanisms

Kept so that a future effort does not rediscover this from scratch, and so that nobody
reads a surviving identifier as a live or planned feature.

### The CalcCache

The CalcCache was the automatic disk cache of the GeoDMS 7 series and earlier: calculated
results were written to a per-project directory and restored on a later run when the
expression key still matched. **It was retired with the 8.0 series** (2022) and has not
existed since. The wiki says so plainly — `Older-versions.md`, `Property.md`
("GeoDMS 7 and earlier only … the now retired CalcCache"), `Strategic-decoupling.md`.

Its replacement is *strategic decoupling*: you write stable intermediate results to
configured storages explicitly, and that choice is part of the configuration rather than
an automatic policy.

The name nevertheless survived in the source for years, in a form that read as current
mechanism — most damagingly `%localDataProjDir%/CalcCache<platform>.v<major>.<minor>`,
whose version suffix made it look like a deliberate per-release cache directory. Issue
[#1189](https://github.com/ObjectVision/GeoDMS/issues/1189) records the misreading that
followed: cold/warm run-time differences between two identical `GeoDmsRun` invocations
were reported as a GeoDMS-level cache effect while investigating #949. They were the
Windows filesystem cache on the source `.fss` files.

**Retired, and what it means:**

- `GetCalcCacheDir()` and the `%calcCacheDir%` placeholder (`tic/stg/AbstrStorageManager.cpp`).
  A configuration that still spells `%calcCacheDir%` now raises an unknown-placeholder
  error instead of silently resolving to a directory nothing reads.
- `ASF_WasLoaded` (`tic/TreeItemFlags.h`) marked an item restored from the cache. Nothing
  had set it for years, so its `Clear()` was a no-op and its debug assert was
  tautologically true. The bit is left documented rather than reused, because
  `Actor::m_State` values are persisted on some paths.
- `IsFileableSize` / `IsFileable` (`tic/FreeDataManager.h`, `tic/TicDataSupport.cpp`)
  selected cache candidates by size, together with the `SwapFileMinSize` registry
  setting and the exported `DMS_DataStoreManager_SetSwapfileMinSize`. All were already
  commented out or unreachable.

**What is *not* CalcCache residue**, despite the naming:

- `CreateCacheRoot` / `IsCacheRoot` / `GetCacheRoot` — in-memory anonymous result trees,
  owned by DataControllers. See `config-cache-separation.md`.
- `OpenFileData` / `CreateFileData` / `CreateFileTileArray` / `FileTileArray` /
  `MustStorePersistent` — the live memory-mapped-storage and temp-spill path, keyed by
  storage path plus relative item name, never by expression.
- `token::sourceDescr` DataController keying, and `materialization::spilled` / `IsInMMD`
  in the scheduler cost model.

### The DataStoreManager

`DataStoreManager` was the session-level owner of the CalcCache. Its header survived
until 2026-08 as a husk containing only forward declarations and design comments; the
comments are preserved here because they are the only surviving description of the
register:

> `DataStoreRegister` — maintains persistent assocs:
> `di-expr: LispRef -> (fileName, domain-expr)` and `terminal -> TimeStamp DataSource
> (config or storage)`; initialized by `DataStoreManager` on opening an existing Storage.
>
> `DataItemAssoc` (singleton assoc: `AbstrDataItem -> DataItemInfo`). Mappings:
> `DataObj -> Seq 0 := (rw_file_view | allocated_vector)`;
> `DC 0-> DataObj -> KeepDataStore: bool`; `DC 0-> file_name`.
> Overall: `DataItem -> DC -> RootItem -> DataStoreManager`.

The historical API is recorded in `doc/MT2-issues.txt` — `DSM::GetUnitDCPairPtr`
(issue 5, a deadlock caused by too wide a critical section on DSM),
`DSM::CreateFileData` and `DSM::OpenFileData` (the `DataWriteLock` → `ADI::CreateMemoryStorage`
→ `DSM::CreateFileData` chain, lines 60-78). The last two were lifted out of DSM verbatim
and now live in `tic/DataLocks.cpp`, serving memory-mapped storages.

What remained of `namespace DSM` after the cache was gone was a four-entry cancellation
façade over `SessionData` — `Curr`, `IsCancelling`, `CancelIfOutOfInterest`,
`CancelOrThrow` — in a header named `DataStoreManagerCaller.h` that 29 of its 36
includers never used. That has been folded into `SessionData`.

### Incremental updates — a live effort, not residue

`TreeItem::DetermineLastSupplierChange` still carries two commented-out lines naming the
old API (`GetCachedConfigSourceTS`, `Curr()->DetermineExternalChange(lastFileChange)`).
**These are deliberately kept.** They mark where external-source change detection has to
be re-implemented, which is the subject of `doc/incremental-updates.md`. Detecting that a
source file changed does not require a persistent result cache, so that work is
unaffected by the retirement above.
