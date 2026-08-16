# Header hygiene: PCH contents, dead includes, split candidates, and renames

*2026-08-16, branch `lookahead-scheduling`, HEAD `39c0b5f`. Follow-up to
[compile-time-refactor-analysis-2026-07.md](compile-time-refactor-analysis-2026-07.md),
re-answering its questions on the current tree: which headers are included a lot or sit in a
PCH, whether less-used components should be separated out, which `#include`s are unneeded,
and which headers deserve renaming. Findings only — implementation is deferred; see the
prioritized ladder at the end. Two of the July conclusions are superseded here: `DataArray.h`
is no longer the churn hot spot (its interface/impl split exists and it has gone cold), and
the "keep `Unit.h` out of the PCHs" advice no longer holds.*

## Method

Three sweeps over the 9 module source trees (`rtc stx stg clc geo shv qtgui run python`;
348 `.cpp` TUs, 605 `.h/.ipp` headers), excluding `vcpkg*/ bin/ obj/ out/ build/`:

1. **Fan-in + closures.** Direct `#include` counts per header basename over all sources;
   transitive closures of the PCH/Base headers resolved against each project's real
   `AdditionalIncludeDirectories`; churn from `git log --since=2026-07-01 -- '*.h' '*.ipp'`
   (315 commits, 406 header-touch events).
2. **Dead-include hunt.** For each include of the hot headers, 2–6 distinctive symbols of
   the included header were grepped in the includer (and its `.ipp`); ambiguous verdicts
   hand-verified.
3. **Naming audit.** Contents of every suspicious header read and characterized; duplicate
   basenames and include-case mismatches scanned repo-wide.

Method caveats (same as July, and they bit again): macro-only, `operator<<`-only, and
template-inline headers are invisible to symbol greps (`ser/RangeStream.h`,
`mem/FixedAlloc.h`, and `geo/GeoSequence.h` all scan as dead but are used). Every DEAD
verdict below is a *direct-use* verdict; the tree leans on transitive includes (verified:
`shv/dll/src/Theme.cpp` uses `DataReadLock` obtained only via `DataArray.h → DataLocks.h`),
so **a compile is the only ground truth for any removal**. Fan-in is counted per basename as
written; LOC figures are raw `wc -l`.

## Finding 1 — the PCH landscape today

The July enablement left **8 live PCHs**; the rtc merge kept three inside one project
(`DmRtc.vcxproj` assigns `SymPCH.h`/`TicPCH.h` per-file with their own `.pch` outputs):

| PCH | TUs | extras over its `*Base.h` | closure hdrs | closure LOC |
|---|---:|---|---:|---:|
| `rtc/dll/src/RtcPCH.h` | 51 | `dbg/Check.h`, `dbg/debug.h`, `set/Token.h` | 58 | 9,263 |
| `rtc/dll/src/sym/SymPCH.h` | 6 | `LispRef.h` | 58 | 9,269 |
| `rtc/dll/src/tic/TicPCH.h` | 56 | `TreeItem.h` | 134 | 22,441 |
| `stx/dll/src/StxPch.h` | 13 | (none) | 45 | 7,595 |
| `stg/dll/src/StoragePch.h` | 22 | `dbg/Check.h`, `stg/AbstrStorageManager.h`, `stg/AsmUtil.h` | 145 | 24,148 |
| `clc/dll/include/ClcPCH.h` | 73 | `AbstrDataItem.h`, `DataArray.h`, `DataLocks.h`, `DataItemClass.h`, `Operator.h`, `<vector>` | 146 | 24,741 |
| `geo/dll/src/GeoPCH.h` | 29 | same 5 tic headers | 147 | 24,771 |
| `shv/dll/src/ShvDllPch.h` | 72 | (none) | 51 | 8,820 |

Union of all closures: **166 of 605 headers (27%)**. First-include discipline is intact
(348/348 TUs; sole deliberate exception `shv/WmsLayer.cpp` = NotUsing for boost/asio).

- **qtgui (21 TUs), run (1), python (1) have no PCH.** qtgui also has no common first
  include — its TUs start with 12 different headers — even though its include path already
  reaches all six module dirs and 7–8 Qt widget headers recur in ≥6 TUs each.
- `StgBase.h` is a 142-header closure (vs `StxBase.h`'s 44) because of a single edge:
  `stg/AbstrStorageManager.h` drags the whole TreeItem/AbstrDataItem/AbstrUnit graph in.
- Notable closure residents: **`pplinterface.h`** (ConcRT) reaches *all 8* PCHs via
  `set/Token.h → Parallel.h → parallel/portable_task_group.h` (WIN32-guarded);
  **`<strstream>`** (deprecated, with its silence-guard hack) sits in the Tic/Stg/Clc/Geo
  closures solely via `utl/mySPrintF.h`'s fixed-buffer family (see Finding 5B);
  **`windows.h`** is in exactly one PCH (ShvDllPch, via `ShvBase.h:225`). No boost, Qt,
  CGAL, or GDAL header is in any PCH closure — good.

## Finding 2 — dead files (delete; all confirmed unreferenced in vcxproj/CMake)

| file | evidence |
|---|---|
| `rtc/dll/src/RingIterator.h` | 469-line dead fork of `geo/BoostPolygon.h` carrying the **same include guard** `DMS_GEO_BOOSTPOLYGON_H` as the live file; fan-in 0; not in any build file. The real ring iterator (`SA_ConstRing`, `SA_ConstRingIterator`) is `geo/RingIterator.h`. Whichever of the two same-guard files is included first silently swallows the other. |
| `stg/dll/src/Stg2Pch.h`, `Stg2Impl.h`, `Stg2Base.h` | complete orphan DLL scaffold (pre-2004 GPL-v2 banner); declares `DMS_Stg2_Load` which is never defined or called. *Implementation note: one stale external includer existed after all — `shv/ShvDllInterface.cpp:40` included `Stg2Base.h` without using anything from it; removed together with the files.* |
| `rtc/dll/src/act/box.h` | fan-in 0 and does not compile standalone (`BoxBase::AddFiduciary` references `Box<T>` before declaration, calls `throwIllegalAbstract` without its header). Was listed in `DmRtc.vcxproj`(+filters) — entries removed with the file. |

**Correction (found during step-1 implementation): `rtc/dll/src/mci/SingleLinkedTree.h` is
NOT dead** — `mci/SingleLinkedTree.inc` (used by `shv/GraphicContainer.cpp`) includes it at
line 12, which the audit's grep missed. The header stays; only `TreeItem.h`'s unused include
of it was removed (TreeItem's own comments say the structure was inlined into TreeItem).

## Finding 3 — dead includes in the hot headers (symbol-verified)

Every removal below still needs a build to confirm no downstream TU used the edge as a
transitive crutch.

| header (PCH residency) | dead includes | notes |
|---|---|---|
| `tic/TreeItem.h` (Tic/Stg/Clc/Geo PCHs, 179 TUs) | `mci/SingleLinkedTree.h` (:21), `ptr/SharedTreePtr.h` (:25 — zero symbol use *and* already included via `TicBase.h`), `ptr/WeakPtr.h` (:27 — only a comment mentions it) | `mci/PropDef.h` (:42) and `xml/XMLOut.h` (:43) are pointer-only (`AbstrPropDef*`, `OutStreamBase*`) → forward-declare. Must **re-add** explicit `<atomic> <memory> <optional> <functional>`: the file uses all four but their includes sit in a commented-out block (lines 45–55) — today's compile relies on transitive luck. |
| `tic/Unit.h` (no PCH; 83 includers pay the 137-header closure each) | `geo/RangeIndex.h`, `geo/SequenceArray.h`, `ptr/LifetimeProtector.h`, `ser/PointStream.h` (lines 15–18) | `ser/RangeStream.h` scans dead but IS used (`AsString(Range<T>)`, operator-only). `geo/CheckedCalc.h` is used (`Cardinality`, line 139). |
| `tic/DataLocks.h` (Tic/Stg/Clc/Geo PCHs) | `ptr/InterestHolders.h` (also arrives via `AbstrDataItem.h`), `TileLock.h` | Of `ser/FileCreationMode.h` only `dms_rw_mode` is used — half that header is along for the ride. |
| `tic/AbstrDataItem.h` (Tic/Stg/Clc/Geo PCHs) | `dbg/DebugCast.h`; `act/InterestRetainContext.h` is dead *here* — its real user is `DataLocks.h:216`, move the include there | |
| `tic/Operator.h` (Clc/Geo PCHs, 11 edits since July) | `set/StackUtil.h`, `ptr/OwningPtrSizedArray.h`, `DataController.h` | `Explain.h` is pointer-only (`Explain::Context*` default-null arg) → `namespace Explain { struct Context; }`. |
| `tic/DataArray.h` (Clc/Geo PCHs) | `geo/StringBounds.h` | `geo/iterrange.h` likely redundant (reached anyway via `SequenceArray.h`) — compile-verify; `set/VectorFunc.h` is used only from `DataArray.ipp` → move the include there. |

Audited and **clean** (no action): `DataItemClass.h`, `geo/SequenceArray.h`,
`set/BitVector.h`, `ptr/SharedStr.h`, `dbg/Check.h`, `ser/format.h`, `set/rangefuncs.h`
(near-clean: only `typesafe_cast` of `dbg/DebugCast.h` is used).

~~Cross-DLL dead include: `shv/dll/src/ViewPort.cpp:36` includes stg's `gdal/gdal_base.h`
with zero `gdal/GDAL/OGR/CPL` references.~~ **Retracted during step-1 implementation:** the
include is load-bearing — ViewPort.cpp calls `GetUnitSizeInMeters(const AbstrUnit*)`
(declared `gdal_base.h:182`, `STGDLL_CALL`), which the gdal-pattern symbol probe missed.
Removal broke the shv build (C3861 at ViewPort.cpp:654/:1522); the include is restored with
a comment naming the used symbol. A cleaner follow-up would move that one declaration out of
the GDAL surface header, since three more shv TUs (PaletteControl, ResourceIndexCache,
ScaleBar) include `gdal_base.h` for the same single function.

### The TU-level pattern

An automated pass over the 196 TUs of clc/geo/shv/stg flagged 39% of project-header
includes as symbol-dead; hand-sampling 12 TUs shows roughly half are false positives, so a
realistic figure is **15–20% stale includes per TU**. The cause is visible in the data: the
most-frequently-stale headers are exactly the copy-paste boilerplate block at the top of
operator `.cpp` files — `TreeItemClass.h` (stale in ~41 TUs), `DataItemClass.h` (32),
`geo/Conversions.h` (30), `geo/PointOrder.h` (27), `UnitProcessor.h` (21), `Param.h` (19).
Independently, **52 of 887 files contain a literally duplicated `#include` line**
(`gdal_vect.cpp`, `XmlTreeOut.cpp`, `GraphVisitor.cpp`, and `clc/Index.cpp` includes
`UnitProcessor.h` twice). *Caveat found during step-1 implementation: a textual duplicate is
not always redundant — `utl/Environment.cpp` (one big `#if defined(_MSC_VER)`/`#else` split)
repeats five of its six "duplicates" deliberately, once per platform branch, and
`shv/GeoTypes.h` includes `geo/color.h` once per `#ifdef _WIN32` branch. Only duplicates
whose first occurrence is unconditional and earlier were removed (~55 lines across ~45
files, each checked against the `#if` nesting); the per-branch repeats stay.*

With PCH on, removing these buys almost no parse time (most are in the PCH anyway); the
value is clarity and freedom to re-shape PCHs later. Recommendation: fix the duplicated
lines as a one-off; clean the stale boilerplate opportunistically per file touched, not as
a big-bang sweep.

## Finding 4 — PCH membership vs fan-in vs churn (the build-time levers)

### 4a. High fan-in headers outside every PCH

**12 of the top-30 fan-in headers are in no PCH closure**:

| header | fan-in | standalone closure (hdrs/LOC) | marginal cost over ClcPCH |
|---|---:|---|---|
| `tic/UnitClass.h` | 94 | 61 / 10,123 | **+1 header** |
| `tic/Unit.h` | 85 | 137 / 23,236 | **+1 header** |
| `utl/Environment.h` | 76 | 46 / 7,997 | +1 (but see 4d) |
| `dbg/DmsCatch.h` | 74 | 62 / 9,604 | +3 |
| `shv/dataview.h` | 48 | 199 / 34,188 | (shv: +149 over ShvDllPch) |
| `shv/ShvUtils.h` | 45 | 171 / 29,851 | (shv: +121) |
| `tic/LispTreeType.h` | 43 | 75 / 12,587 | +2 |
| `tic/UnitProcessor.h` | 42 | 138 / 23,413 | +2 |
| `xct/DmsException.h` | 41 | 46 / 7,785 | +0 |
| `tic/TicInterface.h` | 37 | 85 / 13,888 | +2 |
| `tic/ParallelTiles.h` | 37 | 147 / 24,400 | +12 |
| `tic/TreeItemProps.h` | 34 | 80 / 12,969 | +1 |

The marginal costs are tiny because these headers' closures are already inside the tic-based
PCHs — the TUs pay the full parse of what the PCH almost entirely contains, per TU, today.

### 4b. Enrichment candidates — and the July advice that expired

July's Finding 5 kept `Unit.h` (26 edits then) out of the PCHs deliberately. Since
2026-07-01 `Unit.h` has **3** edits and `UnitClass.h` **0** — the ownership-refactor churn
is over. Candidates, in order of (fan-in × cheapness), all gated on the churn caveat below:

- **TicPCH + ClcPCH + GeoPCH ←** `Unit.h`, `UnitClass.h`, `DmsCatch.h`, `DmsException.h`,
  `LispTreeType.h`; clc additionally `UnitProcessor.h`, `ParallelTiles.h` (23–24 clc TUs
  use them).
- **ShvDllPch ← `dataview.h`** — the standout. Shv's PCH is a 51-header closure while
  **53 of its 72 TUs** include `dataview.h` = 199 headers / 34k LOC, i.e. +149 headers
  parsed per TU beyond the PCH. `ShvUtils.h` (39 TUs, +121), `Theme.h` (32, +120),
  `GraphVisitor.h` (28, +127) tell the same story, and all are low-churn. This is the
  single biggest parse win available in the solution.
- **qtgui: create a `GuiPch.h`** (the recurring Qt widget headers + `RtcInterface.h` /
  `ShvBase.h`), wire `DmsPchHeader` in `GeoDmsGuiQt.vcxproj` and
  `target_precompile_headers` in `qtgui/exe/CMakeLists.txt`, and normalize the 21 TUs'
  first include — the same recipe as the 2026-07 enablement.

### 4c. What must stay out

Churny headers correctly outside every PCH — adding them would trade parse time for
PCH-invalidation storms: `stx/ConfigProd.h` (18 edits since 2026-07), `tic/OperationContext.h`
(9), `clc/CastedUnaryAttrOper.h` (9), `clc/SeparableMapping.h` (8), `tic/OperSignature.h`
(7), `clc/OperConv.h` (5), and `utl/Environment.h` (5 — split it first, Finding 5A).

### 4d. Where PCH invalidation actually hurts now

Headers both in a PCH closure and edited since 2026-07-01 — each edit rebuilds the PCH plus
every TU of the affected projects:

| header | edits | TUs invalidated per edit |
|---|---:|---:|
| `tic/TreeItem.h` | 14 (+2 pre-merge) | 179 |
| `tic/Operator.h` | 11 | 102 |
| `tic/AbstrUnit.h` | 6 | 123 |
| `tic/AbstrDataItem.h` | 5 (+2) | 179 |
| `tic/OperGroups.h`, `tic/AbstrDataObject.h`, `tic/TreeItemFlags.h`, `tic/DataLocks.h` | 3 each | 179 |
| `tic/TicBase.h`, `set/BitVector.h`, `mem/FixedAlloc.h` | 3 each | **348 (all)** |

`TreeItem.h` dominates: 14 × 179 ≈ 2,500 forced TU compiles from header edits alone. This
motivates Finding 5C and general restraint about what else joins a PCH. `DataArray.h`, the
July doc's "real lever", had 0 edits on its post-merge path (2 pre-merge) — its
interface/impl split (`DataArray.ipp`, 511 lines, one includer) plus `DataArrayValue.h`
already exist, the remaining header is ~78% stable interface, and it costs only +7 headers
over TicPCH. **Leave DataArray.h alone.**

## Finding 5 — split candidates, ranked

- **A. `utl/Environment.h` — strongest.** 269 lines, ~40 unrelated free functions in seven
  clusters (registry/status flags, path helpers, file/dir ops, date/time, win32 errors,
  perf/scheduling, child process + UTF conversion), each cluster used by 1–15 files — but
  the union has 76 includers / 142 transitive TUs, no PCH membership, and ongoing churn (5
  edits). Split into `utl/Registry.h`, `utl/FileSystem.h`, `utl/PlatformError.h`,
  `utl/TimeFmt.h` (residual `Environment.h` keeps the session/platform state); the edit
  blast drops from 142 TUs to ~5–15 per part, after which the stable parts are PCH-addable.
- **B. `utl/mySPrintF.h` fixed-buffer extraction — near-zero risk.** Fan-out is starkly
  bimodal: `mySSPrintF` (a thin forward to `mgFormat2SharedStr`) is used by ~80 files; the
  fixed-buffer family (`myFixedBuffer*`, `myArrayPrintF`, `myVSSPrintF`, `RepeatedDots`) by
  ≤4 files each — and that family is the **sole** reason the deprecated `<strstream>` (plus
  its warning-silence hack) sits in 4 PCH closures / 296 TUs. Move lines 34–55 + 65–76 to a
  leaf header; do together with the rename (Finding 6).
- **C. `tic/TreeItem.h` — the real prize, a measured job.** 737 lines × 14 edits × 179 TUs.
  Before choosing a split shape, diff the 14 commits to see *which lines* churned. The
  cheap first move: the function-item / generic-parameter free-function API (~lines
  685–735, 51 lines, no TreeItem-member dependency) → `TreeItemFunctionSpec.h`.
- **D. `shv/Region.h` Qt leak.** `<QRegion>+<QRect>` reach 61 shv TUs outside the PCH via
  core headers (`GraphVisitor.h`, `scalableobject.h`, …). Only the `QRegion` member needs
  the type; the two 3-line inline converters `GRect2QRect`/`QRect2GRect` (lines 29–37) can
  move to the `.cpp`/Qt-aware call sites.

### Non-goals (recorded so they are not re-attempted)

- **Universal-prelude component moves.** July's Finding 5 stands: the prelude is cohesive
  (`BitVector.h` needed by ~291 TUs, `XMLOut.h` 248, `LispRef.h` 282, `act/any.h` a
  by-value TreeItem member), and with PCH on, removing a needed-by-N header converts one
  PCH parse into N per-TU parses.
- **Splitting `dbg/Check.h`.** 0 edits since July; every component family is widely used
  (`dms_assert` 294 files … `throwDmsErrF` 22); the genuinely rare pieces total ~30 lines.
  Rename it (Finding 6), don't split it.
- **Further `DataArray.h` work** (see 4d) and **decomposing `ptr/SharedTreePtr.h`** (98
  lines, 9 edits, in 7 closures — it needs API stabilization, not splitting).

## Finding 6 — renames and moves

High-value (the name actively misleads); cost = the touched include lines:

| current | actual contents | rename | fan-in |
|---|---|---|---:|
| `clc/dll/include/makeCululative.h` | `make_cumulative*` running-sum helpers — filename **typo** (symbols are spelled right) | `makeCumulative.h` | 2 |
| `rtc/dll/src/dbg/Check.h` | the diagnostics prelude: *all* `throw*` functions (`throwErrorD/F`, `throwDmsErrD/F`, `throwNYI`, …), *all* `report*` functions, `MG_CHECK*`/`MG_PRECONDITION*` | `dbg/Diagnostics.h` | 56 |
| `rtc/dll/src/ser/format.h` | `mgFormat*` — a `std::format` front-end; no stream/ser dependency; name collides with `<format>` | `utl/MgFormat.h`; move the stray `mgFormat2SharedStr` (now at `ptr/SharedStr.h:566`) beside it | 4 direct, ~everything transitively |
| `rtc/dll/src/utl/mySPrintF.h` | payload is std::format-based now; `my` prefix is a legacy artifact | `utl/StrFormat.h` (with split 5B) | 104 |
| `stg/dll/src/StgImpl.h` | free-function utility *declarations*, not an impl header; also malformed `#endif __STG_IMPL_H` (missing `//`, C4067) | `stg/StorageUtils.h` + fix the `#endif` | 4 |

Directory-level:

- **`rtc/dll/src/geo/` is two-thirds not geometry** and collides with the geo DLL. Of 52
  files, ~36 are the value-type/traits layer (`Undefined.h`, `ElemTraits.h`,
  `Conversions.h`, `MinMax.h`, `Round.h`, `CheckedCalc.h`, `color.h` fan-in 23,
  `StringBounds.h` fan-in 28 = `StrLen` helpers, `SequenceArray.h`, …) and ~16 genuinely
  geometric (`Point.h`, `Range.h`, `Transform.h`, `SpatialIndex.h`, …). Split into
  `rtc/dll/src/vt/` (value types/traits) and `rtc/dll/src/geom/` (primitives). ~250 include
  lines — the most expensive item; schedule standalone.
- **`clc/dll/include/` is mis-populated.** It is genuinely public (PUBLIC in CMake, on the
  include path of geo/shv/python), but 26 of its 46 headers have zero includers outside clc
  (`AggrBinStructNum.h`, `CalcFactory.h`, `ExprCalculator.h`, `OperConv.h`, …) and belong
  in `src/`; most conspicuously the *private* `ClcPCH.h` sits in the public API dir.
- **`set/Token.h` → `sym/Token.h`.** `TokenID`/`TokenStr` are the interned-symbol currency
  of the sym/LISP layer (`SymBase.h` includes it directly); `set/` itself is a
  misc-container grab-bag, not sets (optionally → `ctr/`, low priority).
- `cpc/transform.h` (typelist metaprogramming `ph::_1`/`tl::type_list` — nothing to do with
  compiler characteristics, and collides with `geo/Transform.h`) → `utl/TypeListTransform.h`;
  `mci/register.h` → `mci/ClassRegister.h`; `act/any.h` (generic type-erasure, no actor
  coupling) → `utl/Any.h`.
- **Case normalization — a latent Linux break.** 62 distinct `#include` directives spell a
  filename with the wrong case (`"geo/transform.h"` for `geo/Transform.h`, `"DataView.h"`
  for shv's `dataview.h` — fan-in 48, `"utl/noncopyable.h"`, `"xml/XmlOut.h"`, …) and 15
  headers differ from their `.cpp` sibling only in case. NTFS forgives all of it; a
  case-sensitive build will not. Fix with two-step `git mv` (case-only renames on Windows)
  and guard with the Linux build.
- Lower priority: `shv/Region.h` → `ClipRegion.h` (it wraps `QRegion` clip/dirty rects, not
  a spatial region; sibling `GdiRegionUtil.h` → `RegionUtil.h`), `geo/color.h` →
  `vt/Color.h`, `geo/StringBounds.h` → `utl/StrLen.h`, `geo/mpf.h` → `mth/BitMetaFunc.h`,
  `mem/MyAllocator.h`/`MyContainers.h` → `Dms*`, merge `utl/swap.h`+`swapper.h`, dbf/odbc
  `Imp` vs `Impl` naming, `buildstamp.h` → `RtcGeneratedBuildStamp.h`, `pCount.h` casing,
  and `StxBase.h` defining `SYNTAX_CALL` where the other five modules follow `<MOD>_CALL`.

## Finding 7 — header prolog convention (applies to every header touched)

Every header affected by any step of the ladder below — dead-include edits, splits,
renames, and all newly created headers — gets its prolog normalized to:

```cpp
// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
//////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  Brief free-form summary of what this header provides, explaining the key
 *  concepts a reader needs — e.g. for dbg/Check.h: the check/assert macro
 *  families, what a "check" vs a "debug check" is, and the throw*/report*
 *  diagnostic entry points that accompany them.
 */

#if !defined(__SUBSYSTEM_FILEBASENAME_H)
#define __SUBSYSTEM_FILEBASENAME_H
...
#endif // __SUBSYSTEM_FILEBASENAME_H
```

- Line 3 (the `///` rule) is trimmed to exactly the length of line 1, not the traditional
  77-slash full width.
- The comment block after `#pragma once` is deliberately liberal: a brief prose summary of
  what follows, with key concepts explained, written from the header's actual contents —
  not a rigid field template. The three headers carrying the old `Name/SubSystem/
  Description/Definition` field block (`dbg/Check.h`, `mci/PropDef.h`, `act/UpdateMark.h`)
  get their content carried over into this freer form.
- Include guard `__SUBSYSTEM_FILEBASENAME_H` — double-underscore prefix — where SUBSYSTEM
  is the rtc subdirectory (DBG, MCI, ACT, TIC, SYM, SER, PTR, SET, GEO, UTL, …) or the DLL
  module for flat modules (CLC, GEO, SHV, STG, STX): `__TIC_TREEITEM_H`, `__DBG_CHECK_H`.
  The closing `#endif` is commented with the guard name.
- Current state: only 3 of ~600 headers have any doc block, and guard styles are wildly
  inconsistent (`__TREEITEM_H`, `__RTC_MCI_PROPDEF_H`, `__TIC_USINGCACHE_H`,
  `DMS_GEO_BOOSTPOLYGON_H`). A systematic guard scheme also prevents collisions — the
  `RingIterator.h`/`BoostPolygon.h` duplicated-guard bug of Finding 2 is exactly what it
  rules out.
- All edits byte-exact (this is a CRLF repo — no sed/python text-mode scripts).

**Companion convention for `.cpp` files** (user-specified; applies to every `.cpp` changed
by any step): the same banner (rule line trimmed to the copyright line's length), then the
module's PCH include first, then the hdrstop block with a bare `#endif`, then a brief
summary comment of the TU:

```cpp
// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "XxxPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// brief summary of what this translation unit implements
```

PCH names per module: RtcPCH.h / SymPCH.h / TicPCH.h (rtc, per sub-tree), StxPch.h,
StoragePch.h, ClcPCH.h, GeoPCH.h, ShvDllPch.h; qtgui/run/python TUs have no PCH — for
those the prolog is banner + summary only. This also retires the remaining pre-2004
`//<HEADER>` YUSE GSO banners and stray field-style `Name/Description` blocks as files are
touched.

## Prioritized implementation ladder

*Status 2026-08-16: steps 1–3 are DONE — step 1 in commit `f3bb3adb` (with the corrections
noted inline above), step 2 in `89836638` (15 dead includes cut from the six hot headers;
10 TUs/headers needed a direct include for what they had been getting transitively —
including `geo/SpatialIndex.h`, which used `RangeFromSequence_SkipUndefined` without
including `set/VectorFunc.h`), step 3 in the follow-up commit: ShvDllPch ← `dataview.h`,
Tic/Clc/Geo PCHs ← the §4b set, and a new `qtgui/exe/src/GuiPch.h` injected via
`ForcedIncludeFiles` (msbuild; covers the QtMsBuild moc TUs) + `target_precompile_headers`
(CMake). Measured after enrichment: a full rebuild of tic+clc+geo+shv = 578 s wall; a full
qtgui rebuild = **21 s** (334 MB `.pch`). Gates: msbuild Release green, unit-suite
aggregate empty, testcases 200/200. Steps 4–5 remain open.*

1. **Zero-risk cleanup**: delete the Finding-2 dead files; remove the 52 duplicated
   `#include` lines; drop `ViewPort.cpp`'s gdal include; fix `StgImpl.h`'s `#endif`.
   Gate: one `.m` build.
2. **Dead-include removals** in the six hot headers (Finding 3), per-header batches with a
   compile after each (they live in PCHs, so each batch is a full rebuild anyway).
   Gate: `batch\TestReleaseUnit.bat`.
3. **PCH enrichment** (Finding 4b): ShvDllPch ← `dataview.h` first (biggest win), then
   Tic/Clc/Geo ← `Unit.h`/`UnitClass.h`/`DmsCatch.h`/…, then the qtgui `GuiPch.h`.
   Measure with a timed `.m` build before/after each step; revert any addition that later
   turns churny.
4. **Splits** (Finding 5): mySPrintF fixed-buffer extraction (+ rename to `StrFormat.h`),
   the Environment.h four-way split, then the measured TreeItem.h extraction.
5. **Renames/moves** (Finding 6), small → large: typo fix → `Check.h`/`format.h`/
   `StgImpl.h` → case normalization → `clc/dll/include` repopulation → `set/Token.h` move →
   the `rtc geo/` split (last, biggest). Renames are mechanical but PCH-invalidating:
   batch them per session, don't trickle.
6. Every step applies the Finding-7 prolog convention to each header it touches; renamed
   headers get the new `__SUBSYSTEM_FILEBASENAME_H` guard in the same edit.

Nothing here changes modeller-observable behaviour, so no wiki page is needed.
