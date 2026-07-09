# Compile-time analysis: header inclusion graph, PCH, and the rtc+sym+tic+stx merge question

*2026-07-07, branch `refactor_ownership`. Re-examination of
[issue #462](https://github.com/ObjectVision/GeoDMS/issues/462) ("reduce compilation times by
splitting up atypically large code-units and merging small projects"), which was closed with
"large clc code units addressed; merging rtc/sym/tic/stx wouldn't yield much per Copilot".
This document re-derives the situation from the actual include graph and object sizes.*

## Method

A script parsed all `#include` directives of the 294 `.cpp` TUs and 572 headers under
`{rtc,sym,tic,stg,stx,clc,geo,shv,run,python}/dll`, resolved them against the projects'
include paths, and computed per-TU transitive closures. Object sizes are from the current
`obj/Release/x64` tree. Preprocessed-prelude sizes and front-end times were measured with
`cl /EP` / `cl /Zs /Bt` on probe TUs that include only the module PCH header.

## Data snapshot

Module sizes (Release `.obj` totals from the current tree):

| module | TUs | headers | obj total | avg transitive repo-hdr LOC/TU |
|--------|----:|--------:|----------:|-------------------------------:|
| clc    |  53 |  46     | **6,090 MB** | 29,381 |
| geo    |  29 |  17     | 1,157 MB  | 31,158 |
| shv    |  71 |  92     |   722 MB  | 33,982 |
| tic    |  52 |  83     |   405 MB  | 24,400 |
| rtc    |  50 | 219     |   200 MB  | 11,343 |
| stx    |  12 |  16     |   176 MB  | 20,925 |
| stg    |  21 |  31     |   143 MB  | 26,171 |
| sym    |   5 |  10     |    23 MB  | 11,776 |

DLL dependency DAG (from include paths and the sln): a mostly linear chain

```
rtc → sym → tic → { stg, stx } → clc → geo → shv → { qtgui, run, python }
```

Cross-module direct-include edge counts confirm the layering (tic→rtc 511, shv→rtc 428,
clc→tic 339, …) with exactly **one violation**: `rtc/dll/src/geo/SelectPoint.h` includes
clc's `OperRelUni.h` — a header physically in rtc that can only compile from clc/geo TUs.

## Finding 1 — PCH machinery exists everywhere but is switched off (biggest lever)

Every module already has a PCH-style header (`RtcPCH.h`, `SymPch.h`, `TicPCH.h`, `StxPch.h`,
`StoragePCH.h`, `ClcPch.h`, `GeoPCH.h`, `ShvDllPch.h`) and **every one of the 294 TUs includes
its module PCH header as its first include** (100% discipline, verified; a few case-variant
spellings like `ClcPch.h` vs `ClcPCH.h` only work because NTFS is case-insensitive).

But `DmsDef.props` sets `<PrecompiledHeader></PrecompiledHeader>` (= *Not Using*), and it has
been that way since the initial 8.035 import; the CMake build has no
`target_precompile_headers` either. So the PCH headers are recompiled from scratch by every TU.

Measured cost of that prelude, per TU:

- `RtcPCH.h` alone preprocesses to **103,220 lines / 4.8 MB**; `TicPCH.h` to
  **118,673 lines / 5.7 MB**.
- Front-end (c1xx) time for a TU containing nothing but `#include "TicPCH.h"`: **~1.6 s**.
- All 294 TUs transitively include a ~90-header universal rtc prelude plus `<thread>`,
  `<map>`, `<ranges>`, … and — notably — **`boost/format.hpp`**
  (via `dbg/Check.h → ser/format.h`), one of the heaviest Boost headers.

Lower bound of recoverable front-end work: 1.6 s × 294 TUs ≈ **8 CPU-minutes per full build
per configuration**, and in practice more because the effective shared prelude of tic/clc/shv
TUs is larger than the PCH header itself (see Finding 4). This applies to Debug and Release,
msbuild and CMake alike, and to every incremental `.cpp`-edit rebuild.

**Action:** enable real PCH per project (`/Yc` on a small `<Mod>PCH.cpp`, `/Yu<Mod>PCH.h` +
`/FI` or rely on the existing first-include discipline) in `DmsDef.props`/vcxprojs, and
`target_precompile_headers` in CMake (with `REUSE_FROM` between targets that share compile
options). Config-only change; no source restructuring required. Normalize the PCH include
spelling casing while at it (Linux/CMake correctness).

## Finding 2 — the atypically large clc/geo code units are back (or never fully addressed)

Issue #462's original request (split TUs with >50 MB release objects) is still actionable on
this branch. Current worst offenders:

| obj | size |
|-----|-----:|
| Clc/RLookup.obj | **371.9 MB** |
| Clc/lookup.obj | 220.3 MB |
| Clc/OperConvNumeric.obj | 161.3 MB |
| Clc/OperConvSequence.obj | 150.5 MB |
| Clc/OperAttrBin.obj | 82.6 MB |
| Geo/BoostGeometry.obj | 74.3 MB |
| Geo/BoostPolygon.obj | 64.7 MB |
| Clc/OperAttrUni.obj | 50.8 MB |

`RLookup.cpp` is only **309 source lines**; the 372 MB is pure template-instantiation
fan-out (operator × value-type × domain-type). clc is 6.1 GB of objects — 70% of all object
bytes in the solution — so clc's back-end codegen, not front-end parsing, dominates total
CPU. More importantly these TUs are **wall-clock stragglers**: `/MP` ends a project's compile
phase when its slowest TU finishes, and a 372 MB-obj TU takes minutes while the other cores
idle. Splitting each >80 MB TU into per-value-type-group TUs (as was done earlier for other
clc units) directly shortens the critical path.

### Update 2026-07-08 — the four largest clc TUs are split

All four are now split by value-type group, one group per TU. `RLookup.cpp` and `lookup.cpp`
each grew a shared implementation header (`RLookupImpl.h` / `lookupImpl.h`) holding the operator
templates plus a single **`inline` operator-group object** (`cog_rlookup`/`cog_classify`,
`cog_lookup`/`cog_collect_by_org_rel`); each sub-TU only instantiates
`inst_tuple_templ<value-type-chunk, …>`. Cross-TU operator registration keeps working because
`AbstrOperGroup::m_FirstMember` is zero-initialized and its ctor deliberately does not reset it,
so members register into the group regardless of static-init order across TUs. `OperConv*`
already funneled through `OperConv.h`, so those two just partition the outer typelist across
sibling TUs. Value-type chunks form an exact partition of the original typelist (no gaps, no
overlaps → no lost or duplicate operators), verified by TestDebugUnit.

Effect on the per-TU straggler (Debug `.obj`, ≈3.3× the Release sizes tabled above; the ~80 MB
Release ceiling ≈ ~260 MB Debug):

| source TU | before (1 obj) | after (max of N objs) | N |
|-----------|---------------:|----------------------:|--:|
| RLookup.cpp        | ~1.2 GB | 239 MB | 7 |
| lookup.cpp         | 719 MB  | 205 MB | 6 |
| OperConvNumeric.cpp| 505 MB  | 235 MB | 4 |
| OperConvSequence.cpp| 467 MB | 116 MB | 5 |

Every resulting sub-TU is under the ~260 MB Debug straggler ceiling, and — more to the point —
each source file's compile work now spreads across 4–7 cores under `/MP` instead of blocking the
clc compile phase on one multi-minute TU. (RLookup's string+point group was refined a second time:
`points` split into `int_points` and `strings`+`float_points` to pull its 284 MB outlier down to
165/126 MB.)

## Finding 3 — merging rtc + sym + tic + stx: worthwhile, but for different reasons than #462 discussed

Copilot's "wouldn't yield much" is correct about *total CPU compile time* — merging DLLs
doesn't remove any TU. But that was the wrong metric; the gains are elsewhere:

1. **Project-stage barriers.** MSBuild serializes dependent projects wholesale: sym's 5 TUs
   can't start until DmRtc has fully compiled *and linked*, tic waits for sym, stx for tic.
   Stages with fewer TUs than cores (sym: 5, stx: 12) leave most of the machine idle, and
   each stage pays its own link + import-lib + msbuild overhead. Merging
   rtc+sym+tic+stx (119 TUs, ~800 MB obj — still far smaller than clc alone) collapses four
   compile+link barriers into one `/MP` pool and shortens the project critical path from
   7 stages (rtc→sym→tic→stx→clc→geo→shv) to 5.
2. **One PCH instead of four.** After Finding 1 lands, a merged project shares a single PCH
   across 119 TUs (on CMake, `REUSE_FROM` can achieve this without merging).
3. **Export-surface shrink.** ~2,190 `RTC_CALL`/`SYM_CALL`/`TIC_CALL` decorated declarations
   currently cross DLL boundaries *within* this group; merged, those become direct
   (inlinable) calls and the export/import tables shrink. The macros can initially stay
   as-is: define all of `DMRTC_EXPORTS`/`DMSYM_EXPORTS`/`DMTIC_EXPORTS`/`DMSTX_EXPORTS` in
   the merged project and downstream consumers keep compiling unchanged (they just link one
   import lib instead of four).

Costs / risks:

- **Static-initialization order.** Today the loader guarantees rtc's statics run before
  sym's before tic's. In one DLL, order across the merged TUs is link-order-dependent. The
  codebase already manages init via component structs (`ElemAllocComponent`,
  `TokenComponent`, …), but given the recent teardown/ownership work this needs an explicit
  review of each module's `DllMain`/`*Main.cpp` and static tokens (`StaticTokenID` etc.).
- Slightly longer link for the merged DLL on every edit inside it (~800 MB obj vs 200–405 MB
  today); mitigated by incremental linking in Debug.
- Test projects (`rtc/tst`, `tic/tst` = TicTst) and the sln wiring need updating.
- stg could join too (#462 listed it), but stg sits beside stx in the DAG and already
  overlaps with it under `msbuild -m`; including it adds GDAL-adjacent storage code to the
  core DLL for little extra parallelism. Suggest leaving stg out initially.

Verdict: **do it, but after PCH and the clc TU splits** — those two dominate. Expected
wall-clock gain from the merge alone is modest (order of a minute on a full build, plus a
simpler deployment of 1 DLL instead of 4); it also permanently removes the temptation to
re-litigate per-module boundaries for tiny projects like sym (5 TUs / 3.6 k LOC — not a
sensible DLL on its own).

## Finding 4 — header-content refactors (what is defined where)

Top headers by *total compiled repo LOC* (TU-reach × own size):

| header | TUs | own LOC | product |
|--------|----:|--------:|--------:|
| rtc geo/SequenceArray.h | 254 | 1,195 | 303 k |
| rtc set/rangefuncs.h | 294 | 784 | 230 k |
| rtc set/BitVector.h | 294 | 768 | 226 k |
| rtc ptr/SharedStr.h | 293 | 611 | 179 k |
| tic TreeItem.h | 234 | 672 | 157 k |
| tic TiledRangeData.h | 234 | 480 | 112 k |
| tic DataArray.h | 179 | 463 | 83 k |
| clc AttrBinStruct.h | 99 | 655 | 65 k |

Observations and options, in rough payoff order:

- **Get `boost/format.hpp` out of the universal prelude.** `dbg/Check.h` (in every PCH)
  includes `ser/format.h` which includes `boost/format.hpp`. With `stdcpplatest` already on,
  migrating `ser/format.h` to `std::format` — or type-erasing the formatting into
  `.cpp` files — removes one of the heaviest third-party headers from all 294 TUs. (With PCH
  enabled this stops hurting compile time, but it still bloats every PCH and couples the
  whole codebase to Boost.Format.)
- **Once PCH is real, move more of the *de facto* shared set into it**: `DataArray.h` /
  `Unit.h` (175–179 TUs) into `TicPCH.h`+downstream PCHs, `AttrBinStruct.h` (99 TUs) into
  `ClcPch.h`. The measured 1.6 s/TU is a floor; the effective shared prelude of clc/shv TUs
  is ~30 k repo-LOC plus externals.
- **Slim the hot tic headers for incremental builds.** PCH does *not* help when
  `TreeItem.h` itself changes — that invalidates the PCH and recompiles 234 TUs. During
  header-heavy work (like the current ownership refactor) the lever is reducing fan-in:
  interface-split `TreeItem.h`/`DataArray.h` (e.g. hive off inline/template machinery into
  `.ipp` included only where instantiated) and prefer the existing fwd-decl bases
  (`RtcBase.h`, `TicBase.h`) in headers.
- **Fix the layering violation**: move `rtc/dll/src/geo/SelectPoint.h` (includes clc's
  `OperRelUni.h`) into clc or geo.
- Cosmetic but confusing: `rtc/dll/src/geo/` (geometry primitives) vs the `geo` DLL
  (geometric operators) share a name; and clc splits public headers into `clc/dll/include`
  while all other modules export from `dll/src`.

## Prioritized plan

*Status 2026-07-08: #1, #2, #3, and the bulk of #4 are DONE. The four largest clc TUs
(RLookup, lookup, OperConvNumeric, OperConvSequence) have been split into per-value-type
sub-units (see the Finding 2 update below); Debug build + TestDebugUnit green. See the per-row
notes below and the sibling docs `boost-format-to-std-format-migration.md` (std::format) and
the merge/StaticTokenID commits.*

| # | change | status | expected effect |
|---|--------|--------|-----------------|
| 1 | Enable PCH (msbuild `/Yc`/`/Yu` + CMake `target_precompile_headers`), normalize PCH name casing | ✅ **done** (`b4ded866`) | ≥8 CPU-min/full build; every cpp-edit rebuild ~1.6 s/TU faster; foundation for #2/#4 |
| 2 | Split clc TUs with >80 MB objs (RLookup, lookup, OperConvNumeric, OperConvSequence, …) into value-type-specific sub-units | ✅ **done** (2026-07-08) | removes `/MP` stragglers; the actual #462 ask |
| 3 | Merge rtc+sym+tic into one DLL | ✅ **done** (`089ebc9f`) | fewer stage barriers + one merged DLL + ~2000 internalized cross-DLL exports |
| 4 | Prelude hygiene: `std::format` migration, SelectPoint.h relocation, boost reduction | ✅ **mostly done** (`38820f35`, `e881ee4e`, move) | smaller PCHs; Boost.Format + random/locale/tuple/mpl removed from the prelude |

Notes on the completed items:

- **#1 PCH.** Fallout: pragma-warning state does not survive the PCH boundary under `/sdl`, fixed
  via `_CRT_NONSTDC_NO_WARNINGS`, `_SILENCE_CXX17_STRSTREAM_DEPRECATION_WARNING`, and libtiff
  `uint16`→`uint16_t` in TifImp.cpp. The `SelectPoint.h`/`CentroidOrMid.h` layering violation is
  fixed (moved to `geo/dll/src/`).
- **#3 Merge — scope was rtc+sym+tic (not +stx).** stx was left out: it sits beside stg in the
  DAG and adding it buys little extra parallelism. Static-init order was handled *before* the
  merge: all namespace-scope `TokenID` statics that used the direct ctor became `StaticTokenID`
  (`TokenComponent` base guarantees subsystem init first), and a full tic/sym audit confirmed
  every other registration static (PropDefs, class metaobjects, operator groups, locks,
  SharedStr) is already order-safe via Meyers singletons (`GetTokenID_st`, `GetStaticClass`,
  `GetFreeStackAllocatorArray`). One dup-symbol the merge surfaced (`s_IsDetectingIncInterest`,
  a tentative definition in tic/ItemLocks.cpp that stopped being a mere `dllimport` declaration
  once RTC_CALL became `dllexport` in one DLL) was fixed with `extern`. msbuild + CMake + the
  solution + 9 downstream projects + the NSIS installers were all repointed; `dumpbin` confirms
  downstream now depends only on `Rtc.dll`, and TestDebugUnit passes with `Sym.dll`/`Tic.dll`
  deleted from `bin`.
- **#4 Prelude hygiene.** `boost/format.hpp` replaced by `std::format` throughout (~1150 format
  literals rewritten); a follow-up commit removed boost random/locale/tuple/mpl (and
  core/cast/preprocessor) in favour of std/native. Not yet done: enriching PCHs with the *de
  facto* shared set (`DataArray.h`/`Unit.h`/`AttrBinStruct.h`) and slimming `TreeItem.h`/
  `DataArray.h` fan-in.

**New follow-up surfaced by the merge — reduce the export surface.** Now that rtc/sym/tic share
one DLL, `dumpbin` shows **2441 of `Rtc.dll`'s 5617 exports (43%) are never imported by any
downstream binary** — pure internal-only exports. ~1328 belong to 304 *entirely-dead*
identifiers (classes `XmlElement`/`XmlParser`/`FindFileBlock`/`AbstrPropDef`/…, templates
`CreateHeapTileArray_impl`/`AsCharArray`/…, and the tic PropDef-pointer globals) that are safe
to de-export wholesale; ~946 belong to 118 partially-dead identifiers needing per-overload care.
De-exporting must be driven by the binary import list (header-inclusion heuristics are unsafe
because of transitive inclusion) and verified by rebuild. Deferred as a dedicated pass.

### `/analyze` (PREfast) runs on every build — measured 2026-07-09

Confirmed empirically, not just from the props: `obj/Debug/x64/**/*.nativecodeanalysis.xml`
(418 files, one per TU) are regenerated on **every** Debug build with timestamps matching the
latest `.obj`s — i.e. the `/analyze` static-analysis pass runs on every compile. It is driven by
`EnablePREfast=true`, set in **both** `DmsDebug.props:23` and `DmsRelease.props:30`; the base
`DmsDef.props:8` `RunCodeAnalysis=true` is overridden to `false` only in Debug — but that does
**not** stop the compile-time `/analyze` pass, it only suppresses surfacing its warnings. Net
result in Debug: the full analysis cost is paid on every TU while **zero** PREfast warnings reach
the build output (verified across ~40 build logs: 0 `C6xxx/C26xxx/C28xxx`), even though the pass
does find defects (they are written to the unread `.xml` files). Debug is thus the worst case —
full cost, zero visible benefit.

`/analyze` is a second data-flow pass over every function; typical overhead is ~1.5–3× per-TU
compile time. **Recommendation:**
1. Turn it **off for routine dev + the `.m/.c/.l` setup builds**: set `EnablePREfast=false` (and
   keep `RunCodeAnalysis=false`) in Debug and Release. This is the single largest remaining
   wall-clock lever after PCH.
2. Preserve the quality signal in a **dedicated pass**, not on every build — a separate "Analyze"
   configuration or a CI/nightly job invoked with
   `msbuild all22.sln /p:Configuration=Release /p:EnablePREfast=true /p:RunCodeAnalysis=true`,
   where `RunCodeAnalysis=true` actually surfaces the warnings so they get triaged (unlike Debug
   today, which hides them).
3. Whatever is chosen, fix the Debug inconsistency: either turn analysis off (speed) or set
   `RunCodeAnalysis=true` there so the findings are visible instead of paying for hidden `.xml`.

A useful follow-up measurement once #1 is in: `msbuild all22.sln ... /p:CL_MPCount /bl` with
the MSBuild binary log viewer, comparing before/after wall-clock and per-project timelines to
validate #2/#3 priorities against real numbers.

## Finding 5 — re-examining the prelude for "move rarely-needed components out" (2026-07-09)

Direct question revisited after PCH landed: *can we shrink the often-included headers by moving
out components not needed everywhere?* The full inclusion graph was rebuilt (319 `.cpp` TUs, 575
project headers) and the top candidates were tested with five independent methods — transitive
fan-in, symbol-level true-need (with the header cut from every include path), single-edge and
all-paths blast-radius, and a "pure-win edge" scan (edges whose removal drops a header from some
TUs' prelude while **zero** of those TUs use its symbols). **Result: no safe, beneficial
component/header move is available. The prelude is genuinely cohesive.** Every plausible
candidate is either needed nearly everywhere or load-bearing:

| candidate (heavy prelude header) | why it can't leave the prelude |
|---|---|
| `set/BitVector.h` (768 LOC, fan-in 319) | `bit_value<N>` / `sequence_traits<bool>` are the bit/Bool value type — used by **291/319** TUs. Sole prelude edge is `ptr/PtrBase.h` (which itself needs only `sequence_traits`), but cutting it breaks 291 TUs. |
| `xml/XMLOut.h` (fan-in 256) | XML/HTML/DMS reporting streams referenced by **248/319** TUs (widely used for dumps/value-info). TreeItem.h already fwd-decls `XML_OutElement`, but the type is needed downstream. |
| `sym/LispRef.h` (206 LOC, closure ~9 k LOC) | the calculation-expression handle; needed by **282** TUs and multiply-reachable — cutting TreeItem.h's edge changes no closure. |
| `act/any.h` | `TreeItem` embeds `rtc::any::Any m_ReadAssets` **by value** (TreeItem.h:602) → complete type required wherever TreeItem is complete = the whole tic prelude. |
| `geo/CheckedCalc.h`, `utl/Environment.h`, `utl/IncrementalLock.h` | the pure-win scan flagged these (`need==0`), but that was a **symbol-extraction artifact**: their APIs are template-inline / util free functions the heuristic missed. Re-checked with hand-picked symbols, every losing TU uses them (`need == lose`: 64/64, 42/42, 15/15, 2/2). |

**Why moving-out is the wrong direction now that PCH is on.** A header in a PCH is parsed *once*
per DLL when the PCH is built. Removing a header used by *N* TUs from the prelude converts that
one parse into *N* per-TU parses — strictly more front-end work for any N≥2. Prelude-trimming
therefore only helps by (a) shrinking the PCH file marginally and (b) shrinking the
*invalidation blast radius* when the header is **edited**. Benefit (b) matters only for
frequently-edited headers.

**Where the real incremental-build cost actually is.** The churny hot headers of this branch —
`DataArray.h` (29 of last 200 commits), `Unit.h` (26), `TiledRangeData.h`, `AttrBinStruct.h`
(18) — are the ones whose edits hurt. `Unit.h`/`AttrBinStruct.h` are correctly **outside** every
PCH; but `DataArray.h` sits inside `ClcPCH` **and** `GeoPCH`, so each edit to it invalidates
those PCHs and recompiles all clc + geo TUs. That PCH-membership (not any rarely-needed
component) is the lever worth pulling — and the fix is an interface/impl split of `DataArray.h`
(hive the churny inline/template bodies into an `.ipp` included only where instantiated so the
PCH-resident interface stops changing), **not** removing headers from the prelude. This is real,
risky work best done deliberately, so it is left as an explicit follow-up rather than bundled
here.

Method caveat for anyone re-running this: automated symbol-reach **undercounts** template-inline
and utility headers (`set/rangefuncs.h`, `geo/CheckedCalc.h`, `geo/Round.h`) because their API
is not `XXX_CALL`-decorated, producing false "rarely used" positives; and it **overcounts**
headers with short/common type names. Always re-verify a candidate with hand-picked distinctive
symbols *and* a by-value-member/base-class check before cutting an edge — and confirm with a
build, which is the only ground truth for forward-declarability. (Scripts:
`scratchpad/{incgraph2,edges,blast2,trueneed,precise,purewin,verify_cc}.py`.)
