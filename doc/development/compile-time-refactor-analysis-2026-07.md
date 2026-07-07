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

*Status 2026-07-07: #1 is DONE (verified full Release build; fallout: pragma-warning state
does not survive the PCH boundary under /sdl, fixed via `_CRT_NONSTDC_NO_WARNINGS`,
`_SILENCE_CXX17_STRSTREAM_DEPRECATION_WARNING`, and libtiff `uint16`→`uint16_t` in
TifImp.cpp). The SelectPoint.h layering violation is fixed: SelectPoint.h and
CentroidOrMid.h moved from rtc/dll/src/geo/ to geo/dll/src/. The std::format part of #4 is
analysed in `boost-format-to-std-format-migration.md`.*

| # | change | effort | risk | expected effect |
|---|--------|-------|------|-----------------|
| 1 | ✅ Enable PCH (msbuild `/Yc`/`/Yu` + CMake `target_precompile_headers`), normalize PCH name casing | small, config-only | low | ≥8 CPU-min/full build; every cpp-edit rebuild ~1.6 s/TU faster; foundation for #2/#4 |
| 2 | Split clc/geo TUs with >80 MB objs (RLookup, lookup, OperConvNumeric, OperConvSequence, OperAttrBin, BoostGeometry/Polygon if separable) | medium, mechanical | low | removes `/MP` stragglers; the actual #462 ask |
| 3 | Merge rtc+sym+tic+stx into one DLL (define all four `*_EXPORTS`; keep macros; review static-init order) | medium | medium (init order) | fewer stage barriers + one shared PCH + smaller export surface; 1 DLL instead of 4 |
| 4 | Prelude hygiene: `std::format` migration, SelectPoint.h relocation, enrich PCHs, slim TreeItem.h/DataArray.h fan-in | incremental | low-medium | smaller PCHs; faster header-edit turnaround |

One more observation, deliberately left as a user decision: `DmsDef.props` sets
`RunCodeAnalysis=true` + `EnablePREfast=true` for **every** build (Debug and Release), i.e.
full `/analyze` static analysis on each compile — typically a 1.5–3× compile-time
multiplier. If analysis-on-every-build is not intentional, moving it to a dedicated
configuration or CI job is likely the single largest remaining wall-clock lever after PCH.

A useful follow-up measurement once #1 is in: `msbuild all22.sln ... /p:CL_MPCount /bl` with
the MSBuild binary log viewer, comparing before/after wall-clock and per-project timelines to
validate #2/#3 priorities against real numbers.
