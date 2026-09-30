# Code audit 2026-09-27: bugs, performance and unfinished work

*2026-09-27, branch `main`, HEAD `80039c90f` (GeoDMS 20.22.0). A read-only review: nothing was built,
run or edited. Ten review agents each took one slice (the containers and memory layer of rtc, the rest
of rtc, the scheduler half of tic, the tree and type-system half of tic, clc with stx, geo, stg, shv,
qtgui with python, run and the scripts, and one that measured every plan document against the code);
several split their slice further. Each agent read candidates in context, followed callers far enough
to know they are reachable, grepped the whole repository before calling anything dead, and looked
specifically for twins of the defects fixed since 2026-09-01. About forty of the highest findings were
re-read once more before this report was written. Line numbers are leads at this HEAD, not gospel;
every item names its function so it can be re-found.*

## How to read this

- **ID**: module prefix plus `A` and a number, so the IDs do not collide with those of
  `doc/code-fixes.md` (`GEO-32`, `TIC-N04`, …). Prefixes: `RTC` containers, memory and value types;
  `INF` the rest of rtc (act, utl, dbg, parallel, sym, mci, xct, xml); `TIC`; `CLC`; `STX`; `GEO`;
  `STG`; `SHV`; `QT`; `PY`; `RUN`; `BAT` and `NSI` for scripts and installer; `REPO`; `PLN` for the
  cross-cutting partial-implementation findings of section 3.
- **Category**: BUG (wrong result, UB, crash, hang, leak), PERF (a concrete mechanism, not a hunch),
  PARTIAL (an improvement started and not finished: two mechanisms coexisting, a fix applied to one
  twin, scaffolding of a removed feature, a status that the code contradicts), SIMPLIFY (dead code,
  duplication, over-general code).
- **Verdict**: CONFIRMED (trigger path read end to end), LIKELY (real, needs an unusual but reachable
  input or timing), SMELL (correct today, fragile or misleading).
- **Severity**: impact if triggered × plausibility, as in code-fixes.md. **Effort**: S (< 1 h),
  M (half a day), L (days).
- Each heading reads `ID · category · verdict · severity · effort — title`.
- **`dms_assert` is not a no-op in Release.** It is `CC_ASSUME` (`__assume` / `__builtin_unreachable`),
  so a false condition is undefined behaviour and the optimiser may delete the code that would have
  handled it. Many items below are "an invariant over reachable input is guarded only by `dms_assert`".
- **Not repeated here**: items already in `doc/code-fixes.md`, `doc/cleanup-list.md`,
  `TECH_DEBT_REVIEW.md`, `RECURSION_REFACTOR_PLAN.md`, `PORTING_STATUS.md`, `doc/issues.md`,
  `doc/deadlocks.md` and `doc/development/*.md`, unless the code shows their status is wrong. Each
  module section ends with a *tracked-item status* paragraph for those corrections; section 5 collects
  the documents to update. Refuted candidates are in appendix A, so nobody re-audits them.

## 1. Summary

| Section | Findings | High | Notes |
|---|---:|---:|---|
| 3 Partially implemented improvements | 19 | 1 | plans vs code; residue measurements |
| 4.1 RTC containers, memory, value types | 18 | 2 | conversions; siblings of the September container fixes |
| 4.2 RTC infrastructure | 24 | 1 | XML reader; format migration; dead scaffolding |
| 4.3 TIC calculation and scheduling | 27 | 0 | worker / meta-thread boundaries; low-RAM brake; storage base |
| 4.4 TIC tree, metadata, type system | 16 | 0 | #1268 siblings; C ABI; property copy |
| 4.5 CLC operators and STX parser | 30 | 2 | null and overflow policy of operator families |
| 4.6 GEO | 60 | 11 | narrow counters, unchecked relations, impedance |
| 4.7 STG | 34 | 6 | GDAL ownership; half-supported types; unchecked writes |
| 4.8 SHV | 22 | 2 | indirect grids; WMS; GDI caches lost in the DrawContext migration |
| 4.9 QT, Python, GeoDmsRun, scripts, installer | 38 | 3 | GUI lifetimes; release gates; repository hygiene |
| **Total** | **288** | **28** | 177 BUG, 49 PERF, 37 PARTIAL, 32 SIMPLIFY (some carry two categories) |

What recurs across modules:

1. **A fix stops at the first twin.** A good quarter of the new defects repeat a mistake that was
   fixed recently, in the sibling that was not touched: GEO-A09 (GEO-32), GEO-A12 and GEO-A16 (#1228),
   RTC-A01 (f46091d59 fixed `UpConvertFunc`, not `RoundedConvertFunc`), RTC-A03 and RTC-A10
   (05f27df92), INF-A10 (RTC-38), TIC-A11 (the t641 measure branch), TIC-A29 and TIC-A35 (#1268),
   STG-A21 (STG-23), STG-A33 (STG-N03), GEO-A26 (`NthElementPart`), GEO-A35 and GEO-A40 (#1283,
   `geos_minkowski_sum`), CLC-A18 (#298), BAT-A09 (#1231), SHV-A01 and SHV-A15 (c54394f32). Grepping
   for the twins of a defect in the same commit that fixes it would have caught most of these.
2. **`dms_assert` still guards input that configurations, files or clicks produce**: INF-A02, TIC-A24,
   TIC-A32, GEO-A17, GEO-A21, GEO-A24, STG-A09, STG-A22, SHV-A01, PY-A03. The rule that code-fixes.md
   applied module by module (`MG_CHECK` or a user error for anything reachable) is not finished.
3. **Members of one operator family disagree on nulls and overflow**: `corr` vs `cov` (CLC-A02), total
   vs partitioned `modus_weighted` (CLC-A03), `cumulate` vs `sum` (CLC-A04), `modus_count` vs
   `unique_count` (CLC-A05), `neg` vs `add` (CLC-A08), `reg_count` and `district` vs `SafeIncrement`
   (GEO-A22, GEO-A23), `get_x` vs `pointcol` (GEO-A28), the five polygon back-ends (GEO-A47), the
   `_with_null` lists (CLC-A10). A written family rule (null in, null out; overflow is an error) with
   one battery case per family would settle them.
4. **Fixes and cleanups landed in dead code.** The 2026-09-13 `DllHandle` cleanup polished a unit that
   nothing reaches (INF-A13); the SHV-41, SHV-52 and SHV-55 fixes went into `CopySelValuesToBitmap` and
   `PenArray`, dead since the DrawContext migration (SHV-A16, SHV-A21), while the live paths keep the
   defects. The dead code listed in this report is roughly 3,000 lines outside the vendored ipolygon
   Voronoi (another 4,477); deleting it first keeps effort on live paths.
5. **Migrations stopped at their hardest step**: C1b of the recursion plan, whose target grew from 4 to
   13 call sites (PLN-A02); compile-time format checking (PLN-A04, INF-A03); the ViewHost boundary
   (SHV-A20); the config / cache separation, 0 of 7 stages (PLN-A10); the `DataArray` alias (PLN-A11);
   two reference-counting systems (PLN-A05); the DrawContext migration without the GDI caches it
   replaced (SHV-A07, SHV-A11, SHV-A12); one-pass storage reads for dbf and odbc (STG-A15).
6. **Storage drivers support value types unevenly and report success after failed writes**: MultiPoint
   (STG-A02), 64-bit integers and NULLs in dbf (STG-A03, STG-A04), tif bit depths and rotation
   (STG-A11, STG-A12, STG-A34), unsigned and narrow types in gdalwrite (STG-A28, STG-A29); unchecked
   writes in tif, shp, dbf and the file stream (STG-A10, STG-A21, RTC-A05).
7. **The plan documents disagree with the code** in both directions: seven status headers call landed
   work unstarted (PLN-A08), the recursion plan lists seven reverted commits as landed (PLN-A01), 45
   anchors point past the end of split files (PLN-A09), and code-fixes.md marks GEO-32, GEO-36, RTC-12 /
   TIC-10 and STG-14 as done or refuted while the code still has the defect. Section 5 lists what to
   update, merge and archive.

## 2. Fix first

Ordered by harm × ease. All are independent; each can be one commit with a regression case in
`testcases/` where a configuration can reach it. Items that change a result need a wiki note
("Since GeoDMS 20.22.x …") in the same session, per AGENTS.md.

| ID | Sev | Eff | What |
|---|---|---|---|
| GEO-A01 | High | S | `join_equal_values_uint8/_uint16` sparse path: a wrapped counter makes the fill write past its buffer |
| GEO-A04 | High | S | `strongly_connected_components`: a null to-node is an unchecked heap write |
| SHV-A01 | High | S | Select District on an indirect grid, or a click outside the grid, corrupts the heap |
| PY-A03 | High | S | Python `set_values_from_float_list` / `_int_list` write past the tile buffer |
| GEO-A02, A03 | High | S | `canyon`: iterators into destroyed read locks; raw segment id as index |
| GEO-A05 | High | S | `bp_` split / union with a numeric attribute reads tile `no_tile` |
| GEO-A10 | High | S | `geos_` buffer / simplify of an empty arc dereferences null |
| INF-A01 | High | S | Loading an XML attribute that contains `&amp;` or `&quot;` hangs forever |
| QT-A01 | High | S | Reload or exit with Calculation times open: use after free, double delete |
| QT-A02 | High | S | CSV export of a database container loops forever on the GUI thread |
| STG-A01 | High | S | gdal.vect multipoint over a mixed-geometry layer: `toMultiPoint()` on the wrong type |
| STG-A05 | High | S | The dataset-information page swaps a live GDAL handle without the storage lock |
| CLC-A01 | High | S | `cov`, `corr`, `modus_weighted` (total) read the second argument out of bounds |
| INF-A02 | Medium | S | `unquote` / `undquote` of a single quote character reads out of bounds |
| RTC-A01 | High | S | `rounded_convert` to uint8 / 16 / 32 clamps at the signed maximum (200.2 → 127) |
| CLC-A02 | High | S | `corr` adds the squares of rows that have a null |
| GEO-A07, A08, A09 | High | S | `impedance_*`: Link_flow doubled and misrouted; end-point impedance indexed by zone |
| GEO-A11 | High | S | `points2sequence` family: a sequence unit not starting at 0 misplaces every point |
| GEO-A13 | Medium | S | `fixWindingOrders` deletes the polygon it has just repaired |
| STG-A03, A04 | High / Med | S | dbf: int64 columns read as zeros; one NULL numeric fails the column |
| STG-A07, A08 | High / Med | S | GDAL: one leaked geometry per written polygon / arc and per curve read |
| STG-A34 | Medium | S | A rotated GDAL georeference is read transposed |
| TIC-A01 | Medium | S | `/SQ` scheduling runs meta-thread-only `GetArgs` on pool workers |
| TIC-A02 | Medium | S | The low-RAM brake can park a supplier that a running operation is joining |
| TIC-A08 | Low | S | `separateResources` erases itself from its own waiter set (a one-word typo) |
| GEO-A06 | High | M | The `dms_` single-ring shortcut writes self-crossing rings (asymmetric bow tie) |
| STG-A02 | High | M | shp: a MultiPoint attribute is written as an empty shapefile, reported as success |
| SHV-A02 | High | M | WMS requests never time out; closing a map or exiting can hang the GUI |
| BAT-A09 | Medium | S | The .m and .c setups can install on a stale passing unit aggregate |
| BAT-A31, NSI-A32 | Medium | S | `analyze.bat` never returns its exit code; the uninstaller leaves the folder behind |
| PLN-A07 | Medium | S | 20.22.0 has no release notes; the #1259 deferral is still described as current |


## 3. Partially implemented improvements

This section measures the plan and backlog documents against the code: what was started and not
finished, where two mechanisms now coexist, and where a document claims a status the code does not
have. Module-level partial items (a fix applied to one twin only, scaffolding of a removed feature) are
in the module sections and carry the PARTIAL category there.

### 3.1 Plans and their actual state

| Plan / document | Claimed | Actual at HEAD (evidence) | Doc right? | Smallest next step | Effort |
|---|---|---|---|---|---|
| `RECURSION_REFACTOR_PLAN.md` (2026-05-21) | 17 commits landed; C1b next; D2/D3 open | **7 of the 17 were reverted the next night** (a52f987b9, 5d8bc89af, 21cb802f2, de529f23e, e77506323, dce70dc29, 19ab9a351); prioritize was later deleted (2d1e8ec51). C1b not started, and `SubstituteExpr_impl(` grew from 4 to 13 call sites. D2/D3 (Spirit depth caps) not done. `/STACK` is 64 MB reserve + 1 MB commit (`DmsDef.props:112, 117`), not the 8192 the doc says | **No** | Mark the 7 as reverted; fix the `tic/` / `sym/` paths and the DmTic / DmSym build steps; move to `doc/development/` | S (C1b: L) |
| `TECH_DEBT_REVIEW.md` (2026-06-07) | 8 debts | #1 recursion, #6 no CI (only `jekyll-gh-pages.yml`), #7 WMS `verify_none` (`WmsLayer.cpp:127, 174, 491, 497`) still true; #3 miscounts the CMakeLists; #5 calls a Release `dms_assert` a no-op (it is `__assume`) | Partly | Move #1, #6, #7 to cleanup-list; archive | S |
| `PORTING_STATUS.md` (2026-05-30) | 11 open | About 5 done (dead code 2d20c5ac1, clipboard, scroll bars, colour dialog, Windows build); open: `FontIndexCache`, `PenIndexCache`, `GetAsDDBitmap`; shv `_WIN32` guards rose from 155 to 163 | No | Trim to 3 items (see SHV-A07) and merge into `doc/linux/` | S |
| `Transformation_complexity_plan.md` (2026-06-18) | "No code changed yet" | Phases 1-6 landed (≈ 33 commits), 7 partly, 9 not started; §3.2 prescribed Qt / `PlgBlt`, a CPU resampler was built; 8 stale "STUB" comments | No | A per-phase status table | S |
| `SaveLoadDesktop_findings.md` (2026-06-13) | Decision (a)/(b) open | Undecided; `SHV_DataView_StoreDesktopData` has 0 callers, so 27 `SM_Save` branches are unreachable; std-ptr §15.4 plans to delete `GraphicObject::Sync`, which option (b) needs | Partly | Decide and record in both docs (PLN-A15) | S |
| `doc/code-fixes.md` (2026-09-06) | Phases 0-5 done; SHV-53, #1249, 4 dumper defects, RTC-70 §9, "activated" asserts open | 14 sampled fixes verified. Closed since: SHV-53 (50fb2b124), #1249 (4d422d9d9), the dumper defects (#1251-#1253, #1256). Really open: RTC-70 §9, 5 "activated DD-MM-YYYY" sites, STG-14 `Int32 read_result` (`GridStorageManager.h:185, 193`). This audit also found GEO-32, GEO-36, STG-14, RTC-C13 and RTC-12 / RTC-C14 / TIC-10 wrongly marked | Partly | Status line; move remnants to cleanup-list; archive | S |
| `doc/cleanup-list.md` (2026-09-13) | 1, 4, 11 done; 3, 5, 8, 12 partly; rest open | Matches the code except item 1 (Linux overrides are still lost at exit, see 4.9, tracked-item status) and item 14 (misses two v21 tripwires, GEO-A46, PLN-A14). Nothing moved on the open items | Mostly | Item 2 (Python `Engine`) or item 6 (delete `Win32ViewHost`) | S-M |
| `doc/issues.md` (2026-08-29) | 9 open issues | Frozen: 54 issue numbers appear in commits since, only #1226 recorded; #587 and #1214 still "needs design" though 20.20.0 shipped both; the #1165 row is stale (4.4, tracked-item status) | No | Regenerate from GitHub | M |
| `doc/tile-data-retainment.md` | §2 tree "refreshed when U4 lands" | U4 landed 2026-08-16; observation 2 cites an `if (true \|\| EasyRereadTiles())` bcf7317ec deleted | Partly | Redraw §2, fix observation 2 | S |
| `doc/incremental-updates.md` | Plan §3.4, step 1 not started | Not started (`DetermineExternalChange` commented out, `TreeItem.cpp:2482`); `ReportChangedFiles` on WindowActivate already exists and §3.2 misses it; 13 stale `tic/dll/src` paths; `DMS_IsConfigDirty` (TIC-A14) and `Renumber()` (INF-A14) described as working | Plan yes, details no | Step 1 on the existing hook | M |
| `doc/interest-and-futures.md`, `Interest.md` | Design R1-R4; #1202 open | Nothing implemented (`FutureData = InterestPtr<…>`, `TicBase.h:146`) | Yes | R1: a distinct retainer type | M |
| `doc/function_serializer.md` | "branch hof_syntax"; prelude cannot round-trip | Merged; #1253 (df0af919c) made dumps self-contained, but `testcases/run_roundtrip.ps1:45-50` still skips `fn_test_prelude` (PLN-A19) | Partly | Drop the skip | S |
| `doc/performance-test.md` (#1259) | Measurement log | No deferral code remains (`DeferScope`, `StartSupplierProduction`, `StartProductionForCommit`, `s_MaxDeferred`: 0 hits), but lines 1-155 read as current | Partly | A removal banner (PLN-A07) | S |
| `std-ptr-migration-plan.md` + `stdptr-migration-handoff.md` | TreeItem migration complete; §15 follow-ups | 0 rogue `shared_ptr<family>(raw)`; 2 of 6 `// TODO ownership` left (`UsingCache.cpp:445`, `AbstrStreamManager.cpp:139`); §15 items 3, 5 done; 1, 2, 4, 6, 7 open; `SharedPtr<` 202 uses | Yes | Merge into one ownership reference; wire the lint (PLN-A06) | S / L |
| `ptr-safety-review-2026-07-02.md` | Fixed | Verified | Yes | Archive into the ownership reference | S |
| `teardown-leak-and-ownership-cycles.md` | "All work uncommitted" | Instrumentation gone, superseded by the handoff; its "cycle cut" claim is contradicted by TIC-A10 | No | Archive | S |
| `boost-format-to-std-format-migration.md` | Stages 0-4 | 0, 1, 3 done; 2 (compile-time checking) and 4 (`<strstream>`) open | Reads as unstarted | Status line; stage 2 (PLN-A04) | S / M |
| `compile-time-refactor-analysis-2026-07.md`, `header-hygiene-2026-08.md`, `k11-container-types-scope.md` | Done / "deferred" | All landed (k11's intro still says deferred; header-hygiene:74 is wrong about `SingleLinkedTree.h`) | Mostly | Archive | S |
| `tu-reorg-and-export-surface-2026-08.md` | TreeItem / AbstrCalculator / Environment splits DEFERRED | TreeItem.cpp and AbstrCalculator.cpp were split in 821d19459; Environment.cpp still 3140 lines; :31 wrongly says RunDllProc is used | Partly | Update row D | S |
| `g8-todos.md` | 71 markers in 37 files | 61 `TODO G8` + 5 `TODO G8.5` = 66 in 35 files; anchors such as `TreeItem.cpp:4310`, `AbstrCalculator.cpp:6257` point past the end of the file | Partly | Refresh count and anchors | S |
| `typed-hof-remaining-work.md` | WP4.2, WP3.2, §5.9, closure locals, filter / fold, RewriteExpr C/D/E open | Consistent; 6 anchors stale | Yes | Re-pin to `Hof*.cpp` | S |
| `operator-signature-interface.md` | All batches shipped | `OperSigKind` retired; 27 family bases + 3 describe their signature against 148 `CreateResult` overrides (by design); :1180 eq/ne `ConnectPointOperator` still open and worse than recorded (4.6, tracked-item status); §12.3 is the root of CLC-A01 | Yes (anchors stale) | Anchors | S |
| `config-cache-separation.md` (2026-08-16) | Staged plan C1-C7 | **0 of 7 done**: no C1 assert; `mc_DC` 46 and `mc_RefItem` 44 against `m_DC` 53 (C2); no `TreeItemConfig.cpp`, no `IsConfigCapable` | Yes, as a plan | C2 rename, or mark parked (PLN-A10) | S / L |
| `unit-hierarchy-collapse.md` | U1, U2, U4, U5 done; U6 optional | Verified; U6 open: `DataArray<` 704 uses vs `TileFunctor<` 104 and `DataArrayBase<` 119; `[[no_unique_address]]` is ignored by MSVC (PLN-A11) | Yes | U6 substitution | M |
| `storage-read-operators.md` (#587) | "S0-S4 done, S5 in progress" | S5 done (60f6c28bd and follow-ups); open: one-pass reads for dbf / odbc, spec-only meta infos, full.py; :127-131, :236-239, :1165 describe removed code | Header stale | Status line | S |
| `schedule-with-lookahead.md` (42 commits) | "Design plan (no code changes yet)" | P0 done, P1 complete, P2 gate + budget + `/SB` landed (off by default); `prioritize_impl`, which P3 meant to revive, was deleted; P5 not started; §2.2 :93-94 still names the removed deferral and calls the floor of one a progress guarantee (TIC-A02) | **No** | A phase table; fix §2.2; move the ≈ 2,100-line §8.1 log to an archive file | S |
| `crs-metric-decoupling.md` | Stages 0-7 done, regression pending | Verified; ruling 1 is done but still listed; stale comment at `clc/dll/src/OperUnit.cpp:410-422` | Yes | Fix the comment; record the regression result | S |
| `dms-dissolve-single-noding.md` | "Implemented in the working tree (uncommitted)" | Committed as f136489d7 + four #1283 commits; single-ring shortcut only (4.6, tracked-item status); t020 unit-suite column not re-run | Header stale | Status line | S |
| `testing-strategy.md`, `build-tips.md`, `dev-environment-gotchas.md`, `doc/development/README.md` | Dev notes | Predate the `testcases/` battery (347 cases), the XML round trip, `run_unit_suite.bat`, the case renames, the RLookup split and the skills | No | Merge into AGENTS.md / skills; archive | S |

### 3.2 Residue measurements

Lines per module at HEAD (`tic` = `rtc/dll/src/tic`, `rtc` = the rest of rtc including `rtc/tst` and
the vendored `rtc/dll/src/ipolygon`). Counted over tracked sources with comment lines removed where
the pattern is code.

| Pattern | tic | rtc | stx | clc | geo | stg | shv | qtgui | py | total |
|---|---|---|---|---|---|---|---|---|---|---|
| `#include <boost/format…>` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | **0** |
| `mySSPrintF` + `mgFormat*(` (runtime `std::vformat` sinks) | 157 | 95 | 19 | 34 | 42 | 35 | 62 | 15 | 0 | 459 |
| `myFixedBuffer*` / `myArrayPrintF<` (on `<strstream>`) | 2 | 5 | 0 | 0 | 0 | 9 | 2 | 1 | 0 | 19 |
| C `(s\|sn\|vs\|vsn)printf` | 1 | 5 | 0 | 0 | 0 | 22 | 0 | 0 | 0 | 28 |
| `SharedPtr<` (intrusive) | 93 | 50 | 0 | 26 | 1 | 14 | 18 | 0 | 0 | 202 |
| `WeakPtr<` (intrusive, an unchecked raw pointer) | 6 | 18 | 6 | 4 | 5 | 8 | 45 | 0 | 0 | 92 |
| `std::shared_ptr<` / `std::weak_ptr<` | 186 / 40 | 43 / 13 | 0 / 0 | 35 / 3 | 10 / 16 | 7 / 6 | 206 / 79 | 6 / 1 | 5 / 0 | 498 / 158 |
| raw `new T` (excluding ipolygon; mostly adoption into `SharedPtr`, Qt parents in qtgui) | 43 | 43 | 3 | 18 | 2 | 16 | 40 | 118 | 0 | 283 |
| `#if 0` / `#if 1` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | **0** |
| `#if … MG_*` guards | 141 | 250 | 8 | 6 | 22 | 12 | 70 | 5 | 0 | 514 |
| MG_ toggles hard-wired off | 6 | 7 | 0 | 1 | 2 | 3 | 10 | 0 | 0 | 29 |
| `&& false` / `\|\| true` in code | 2 / 0 | 0 | 0 | 0 | 0 | 0 | 1 / 3 | 0 | 0 | 3 / 3 |
| NYI / "not (yet) implemented" (code) + `throwNYI` | 6 | 2 | 2 | 2 | 2 | 6 | 4 | 1 | 0 | 26 |
| `TODO` | 70 | 56 | 5 | 20 | 17 | 32 | 24 | 23 | 1 | 248 |
| `TODO G8` / `TODO G8.5` | 40 / 3 | 6 / 2 | 2 / 0 | 5 / 0 | 5 / 0 | 2 / 0 | 1 / 0 | 0 | 0 | 61 / 5 |
| Spirit Classic includes | 0 | 0 | 4 | 0 | 0 | 0 | 0 | 0 | 0 | 4 (10 stx files) |
| `#if…_WIN32` (+ `WIN32` without underscore) | 0 | 2 (+41) | 1 | 0 | 0 | 5 | 162 | 59 | 0 | 232 (+41) |
| `DataArray<` / `TileFunctor<` / `DataArrayBase<` | 47/76/104 | 4/16/0 | 2/0/0 | 268/3/11 | 297/2/0 | 53/7/3 | 33/0/1 | 0 | 0 | 704/104/119 |
| `mc_DC` / `mc_RefItem` (config-cache C2) | 42 / 41 | 3 / 0 | 0 | 1 / 2 | 0 | 0 | 0 | 0 | 0 | 46 / 44 (+1 in run) |

**MG_ macros tested in `#if` but defined nowhere** (12 macros, 48 guards): `MG_DEBUG_LISP_TREE` ×9,
`MG_CACHE_COLLECTDATA` ×9, `MG_DEBUG_REFCOUNT` ×7, `MG_DEBUG_UPDATESOURCE` ×5, `MG_CACHE_ALLOC_SMALL` ×5,
`MG_USE_LISPFUNCS` ×3, `MG_DEBUG_POLYGON`, `MG_DEBUG_LISPEVAL`, `MG_DEBUGDATA` ×2 each, `MG_DEBUG_SWAP`,
`MG_DEBUG_OPERATIONS`, `MG_ASSUME_CB_INC` ×1 each.

**Polygon back-ends.** Union, intersect, difference, xor, the dissolve family, overlay, connectivity and
Minkowski are each registered five times (bp, bg, cgal, geos, dms). bg and geos alone provide polygon
buffer and simplify, and bg alone `outer_*`, so no family is fully superseded. For overlay and
dissolve the measurements in `dms-dissolve-single-noding.md` (t020, Amsterdam buildings) give cgal
47.4 s, geos 30.8 s, dms 6.8 s, bp 6.0 s, and on ManySmall bg 684 ms against dms 26 ms: `cgal_` and
`bg_` overlay / union are dominated everywhere measured.

### 3.3 Findings

#### PLN-A01 · PARTIAL · CONFIRMED · Medium · S — The recursion plan lists 7 reverted commits as landed
- **Where**: `RECURSION_REFACTOR_PLAN.md:18-55`.
- R1, prioritize_impl, A1, A2 and R2 phases 1, 2 and 5 were reverted at 02:35 the night after the plan was committed; the "post-order driver pattern" section describes an approach that is gone. cleanup-list item 15, g8-todos §3 and code-fixes cite this document as the authority.
- **Fix**: mark the 7 as reverted, keep the 8 that stand (C1a, D1, F2, H1, H3, the restore, the Linux flag), fix paths, move to `doc/development/`.

#### PLN-A02 · PARTIAL · CONFIRMED · High · L — C1b never started, and its target grew from 4 to 13 call sites
- **Where**: `rtc/dll/src/tic/AbstrCalculator.cpp` (1 definition + 13 calls of `SubstituteExpr_impl`, `slSupplierExprImpl` ≈ :988-1056).
- The typed-HOF work (apply, closures, `resolveData` lambdas) added recursive call sites while C1b sat idle. Three stack-safety mechanisms now coexist as permanent fixtures: the 64 MB reserve (`DmsDef.props:112`, `run/exe/CMakeLists.txt:22`, `qtgui/exe/CMakeLists.txt:39`), the 320 KB `RemainingStackSpace` hand-off to `std::async`, and the Debug-only `MaxAllowedLevel` counters (`LispEval.cpp:23`). D2/D3 (≈ 30 lines of Spirit depth caps) was never done; the only depth cap in stx is `kMaxIncludeDepth = 64`.
- **Fix**: schedule C1b before the HOF surface grows further, or re-scope the document as a stack-budget policy naming the three mechanisms. D2/D3 is the cheap independent step.

#### PLN-A03 · BUG · LIKELY · Medium · S (verify) / M (fix) — The Linux and Python "64 MB stack parity" probably has no effect
- **Where**: `run/exe/CMakeLists.txt:23-30`, `qtgui/exe/CMakeLists.txt:40-43`, `python/dll/src/Bindings.cpp` (Engine constructor).
- `-Wl,-z,stack-size` sets the PT_GNU_STACK size, but glibc and the kernel size the main thread from RLIMIT_STACK (the CMake comment concedes that worker threads get RLIMIT_STACK); nothing calls `setrlimit` or `pthread_attr_setstacksize`, and a `.pyd`'s /STACK is ignored by the loader. Iterated-calc configurations that need ≈ 64 MB on Windows can therefore overflow on Linux and in Python.
- **Fix**: probe a deep iterated-calc configuration under `ulimit -s 8192` on WSL; if confirmed, run the meta work on a thread created with a 64 MB `pthread_attr_setstacksize`, and the same in the Python Engine.

#### PLN-A04 · PARTIAL / BUG · CONFIRMED · Low · M — The std::format migration stopped at runtime `vformat`; a printf `%d` already slipped through
- **Where**: `rtc/dll/src/utl/MgFormat.h:79-100`; `rtc/dll/src/ipolygon/detail/polygon_arbitrary_formation.hpp:1748`; `utl/FixedBufferFormat.h:27-31`.
- All ≈ 460 sinks take `CharPtr` and call `std::vformat`, which ignores surplus arguments: `reportF(ST_Warning, "…tailless hole detected at (%d, %d)", x_, currentY)` (from d558deb35, #918) prints `(%d, %d)` literally and drops both coordinates. Together with INF-A03 (five mismatches, two of which lose an error text) this is the case for stage 2.
- **Fix**: `{}, {}` now; then a `std::format_string<Args...>` overload for the variadic sinks, keeping `vformat` for the few runtime formats; stage 4 is INF-A11.

#### PLN-A05 · SIMPLIFY · CONFIRMED · Medium · L (S for the rename) — Two reference-counting systems coexist, and `WeakPtr<T>` is a raw pointer
- **Where**: `rtc/dll/src/ptr/{SharedPtr,WeakPtr,OwningPtr,PtrBase}.h`; `std-ptr-migration-plan.md` §14 / §15.2.
- TreeItems moved to `std::shared_ptr` / `std::weak_ptr` (498 / 158 uses); data objects, DataControllers, `LispObj`, storage managers and unit metrics stay on intrusive `SharedPtr` (202 uses), adopted through raw `new`. The in-house `WeakPtr<T>` (92 uses, 45 in shv) is a non-owning raw pointer without a liveness check, spelled like `std::weak_ptr`; the plan names it as a root cause of the teardown bugs. §15.2 (retire the toolkit) conflicts with §14 (keep DCs intrusive), and neither is scheduled.
- **Fix**: rename `WeakPtr<T>` to an honest observer type (or `T*`); record the decision to keep DCs, data objects and Lisp intrusive, retiring §15.2.

#### PLN-A06 · PARTIAL · CONFIRMED · Medium · S — The std-ptr guard-rail lint exists but nothing runs it
- **Where**: `tools/check-ptr-discipline.ps1` (55a3968ba); `analyze.bat:38-41`.
- Since the `=delete` wrapper went, the rogue-control-block guard is convention only, which this lint was written for; `analyze.bat` runs `check-lock-across-sink.ps1` and `check-lock-ceilings.ps1` but not this one, and nothing else references it. (And `analyze.bat` does not return its exit code at all: BAT-A31.)
- **Fix**: add it to `analyze.bat`; drop its stale `tic`, `sym` and `exe` source directories.

#### PLN-A07 · PARTIAL · CONFIRMED · Medium · S — #1259: the code is clean, but documents and release notes still carry the deferral
- **Where**: `doc/performance-test.md:1-155` (title; :9-15, 82-85, 95, 139-140, 246, 449-450); `doc/development/schedule-with-lookahead.md:93-94`; `doc/release-notes/` (the newest is 20.20.0, whose line 47 advertises "Memory under a deferred commit (#1259)").
- No 20.21.x or 20.22.0 notes exist although the version is 20.22.0 (1ec17232f), so the removal and the ExplicitSuppliers wiki round trip are unrecorded; performance-test.md itself measured t405.1 2.3× slower than under the commit deferral. `IntegrityCheck.md` and `deadlocks.md` are clean.
- **Fix**: a removal banner in performance-test.md; rewrite schedule-with-lookahead :93-94 to name only the #1255 view poll; write `release-notes-20.22.0.md`.

#### PLN-A08 · PARTIAL (docs) · CONFIRMED · Low · S — Status headers say "not started / uncommitted / deferred" for work that landed
- `schedule-with-lookahead.md:3` ("no code changes yet"; 42 commits), `Transformation_complexity_plan.md:3` (≈ 33 commits), `dms-dissolve-single-noding.md:3` ("uncommitted"; f136489d7), `storage-read-operators.md:3` ("S5 in progress"; done), `tu-reorg-and-export-surface-2026-08.md:457` ("DEFERRED"; 821d19459), `teardown-leak-and-ownership-cycles.md:3` ("all uncommitted"), `k11-container-types-scope.md:3` ("deferred"). A reader picking work from these would redo finished items.
- **Fix**: dated phase / status tables; a rule that a status line changes in the commit that changes the status.

#### PLN-A09 · PARTIAL (docs) · CONFIRMED · Low · S — 45 document anchors point past the end of split files; 34 paths still use `tic/` / `sym/`
- 13 of 47 `AbstrCalculator.cpp:<n>` anchors have n > 1826 and 32 of 107 `TreeItem.cpp:<n>` have n > 2830, after 821d19459 split both files (worst: g8-todos, typed-hof-remaining-work, typed-hof-language-design, code-fixes; g8-todos routes items by them). `tic/dll/src` / `sym/dll/src` paths, gone since 904525c4d: incremental-updates 13, teardown 6, ptr-safety 5, SaveLoadDesktop 3, RECURSION 3, Interest 3.
- **Fix**: symbol names instead of line pins, or one re-pinning sweep.

#### PLN-A10 · PARTIAL · CONFIRMED · Low-Medium · S (C2) / L (all) — Config / cache separation: 0 of 7 stages, while other documents defer to it
- **Where**: `doc/development/config-cache-separation.md` §8; `TreeItem.h:654`; the g8-todos "already designed" table.
- No C1 assert; the C2 rename is untouched (`mc_DC` 46 and `mc_RefItem` 44 beside `m_DC` 53); no C3-C5 moves, no `TreeItemConfig.cpp`, no `IsConfigCapable`. g8-todos treats the `TreeItem.h:617` TODO as "already designed, do not re-plan", which parks it indefinitely.
- **Fix**: do C2 (≈ 90 mechanical occurrences in `rtc/dll/src`), or mark the plan parked.

#### PLN-A11 · SIMPLIFY / PERF · CONFIRMED · Low · M — Unit / TileFunctor collapse: a 704-use alias and ignored `[[no_unique_address]]`
- **Where**: `rtc/dll/src/tic/TicBase.h:84` (`using DataArray = TileFunctor<V>; // TODO G8: SUBSTITUTE AWAY`), `Unit.h:84, 171, 172`, `DataArray.h:180`.
- `DataArray<` (704, mostly clc 268 and geo 297) names the same class as `TileFunctor<` (104), with `DataArrayBase<` (119) beside both; the ten `[[no_unique_address]]` members are ignored by MSVC, so every non-ranged or non-geo `Unit<V>` still pads for its `Void` metric and projection.
- **Fix**: a `DMS_NO_UNIQUE_ADDRESS` macro (`[[msvc::no_unique_address]]`); the scripted `DataArray<` → `TileFunctor<` substitution per module.

#### PLN-A12 · SIMPLIFY (docs) · CONFIRMED · Low-Medium · M — Four overlapping backlogs disagree
- `doc/code-fixes.md`, `doc/cleanup-list.md`, `TECH_DEBT_REVIEW.md`, `doc/issues.md`: code-fixes lists closed items as open (and this audit found five of its statuses wrong in the other direction); issues.md is frozen; TECH_DEBT #2 says ViewHost is unfinished for Qt, but Qt is the only host and `Win32ViewHost` is dead; only cleanup-list matches the code.
- **Fix**: make cleanup-list the single live backlog (with the remnants of code-fixes and TECH_DEBT #1, #6, #7, and the open items of this report), archive the rest, regenerate issues.md from GitHub.

#### PLN-A13 · SIMPLIFY · CONFIRMED · Low-Medium · M — `Environment.cpp` implements the flag parser and 63 other functions twice
- **Where**: `rtc/dll/src/utl/Environment.cpp` (3140 lines): `RTC_ParseRegStatusFlag` at :695 and :2792, session overrides at :381 and :2546, `RTC_GetRegDWord` at :753 and :2845.
- The Windows (:154-1696) and POSIX (:1697-3028) blocks share 64 signatures; the two parser copies differ only in whitespace, so a new `/S?` flag must be added twice or one platform silently drops it. cleanup-list item 5 (partly done) and INF status list the duplicated families.
- **Fix**: hoist the parser and the session-override family above the `#if defined(_MSC_VER)` at :154, leaving only platform primitives in the halves.

#### PLN-A14 · SIMPLIFY · LIKELY · Low · M — Five parallel polygon back-ends; the v21 list misses the 48 deprecated bp names
- **Where**: `geo/dll/src/BoostPolygon.cpp:2367-2667`; `doc/cleanup-list.md:185-195`.
- Each set operation is registered five times, each with its own near-identical `*_PolyOperatorGroup(s)` scaffolding; `cgal_` and `bg_` are dominated for overlay and dissolve (§3.2). The 12 kernel-suffix families (48 names, deprecated since #917) are still registered but not in cleanup-list item 14 (nor is `GetGeosNonDPointDeprecationFlag`, GEO-A46).
- **Fix**: add them to item 14; deprecate `cgal_*` / `bg_*` overlay and union in favour of `dms_*`, keeping bg / geos for buffer and simplify; fold the group structs into one template keyed by `geometry_library`.

#### PLN-A15 · SIMPLIFY · CONFIRMED · Low-Medium · S (decide) — Desktop save is unreachable, and two documents pull in opposite directions
- **Where**: `shv/dll/src/ShvDllInterface.cpp:126` (`SHV_DataView_StoreDesktopData`, 0 callers); 27 `SM_Save` branches in shv; `std-ptr-migration-plan.md` §15.4; `SaveLoadDesktop_findings.md`.
- New layer types keep adding untested save code (ChartLayer, PieLayer); the findings document builds option (b) on the path that std-ptr §15.4 plans to delete; `TreeItem::Reorder` stays exported only for `GraphicContainer::SaveOrder`.
- **Fix**: decide (a) or (b) in both documents; for (a), delete `StoreDesktopData` and the `SM_Save` halves, keeping `SaveOrder`.

#### PLN-A16 · SIMPLIFY · CONFIRMED (voronoi: LIKELY) · Low · S — Leftovers of finished migrations
- `shv/dll/src/Win32ViewHost.{cpp,h}` (constructed nowhere, still in ShvDLL.vcxproj); `EasyRereadTiles()` (0 callers since bcf7317ec; STG-A31); `rtc/dll/src/ipolygon/voronoi*.hpp` and `detail/voronoi_*` (8 files, 4,477 lines, not included from outside ipolygon, possibly the only reason `boost-polygon` is in vcpkg.json); `ApplyTopEnvFunc::operator()` (`LispEval.cpp:431-473`, duplicating the H1 loop at :515-534); `oper_policy::can_be_rewritten` ("NYI, WIP", `OperPolicy.h:42`, 0 uses); the 12 never-defined MG_ macros (48 guards); the unbuilt `rtc/tst` and `stg/tst` sources using retired APIs (`RefPtr`, `DBG_TRACE((…))`).
- **Fix**: one "retire finished-migration leftovers" commit; drop `boost-polygon` from vcpkg.json only if boost-geometry does not need it.

#### PLN-A17 · BUG · LIKELY · Medium · S-M — The Python `Engine` publishes itself before initialising and never unpublishes (cleanup-list item 2, still open)
- **Where**: `python/dll/src/Bindings.cpp:333-361`, class registrations ≈ :705-711.
- `s_currSingleEngine = this` runs before `DMS_Clc/Geo/Stx_Load` and the task-group maintainer; `~Engine(){}` is empty. A failed initialisation leaves a dangling singleton (every later `Engine()` fails); `del engine` leaves a dangling pointer and a registered callback; a Config can outlive its Engine (no `keep_alive`).
- **Fix**: publish after success (or via a scope guard); clear and unregister in `~Engine`; `py::keep_alive<0,1>` on the config-returning methods.

#### PLN-A18 · PERF / SIMPLIFY · CONFIRMED · Low-Medium · L — Spirit Classic parsing is serialised process-wide by one recursive mutex
- **Where**: `stx/dll/src/ConfigParse.cpp:459, 531`, `ExprParse.cpp:41`, `DataBlockTask.cpp:82`, `DijkstraString.cpp:72`; `SpiritTools.h`, `TextPosition.h`.
- The move off `BOOST_SPIRIT_THREADSAFE` put every expression, configuration, data-block and Dijkstra-string parse behind one `GetSpiritGrammarMutex()`, so meta-thread and worker parsing run one at a time (worse with the lock held across `Commit`, CLC-A29). Spirit V1 is deprecated and unmaintained and is also where D2/D3 would go (PLN-A02); no migration plan exists.
- **Fix**: measure contention on t641 / t720 with `/SP`; if significant, per-thread grammars; then a scoped plan for a hand-written recursive-descent `ExprParse` with explicit depth limits.

#### PLN-A19 · SIMPLIFY · LIKELY · Low · S — The round-trip battery still skips `fn_test_prelude`, whose cause #1253 fixed
- **Where**: `testcases/run_roundtrip.ps1:45-50`; `doc/function_serializer.md` "Accepted v1 limitations".
- df0af919c made dumps self-contained, removing the prelude-clash reason given in the skip. **Fix**: remove the skip, run `testcases\run_roundtrip.bat`, update the document.

## 4. Findings per module

Ordered within each module roughly by value. Each module ends with the status of the items that other documents already track, where the code disagrees with them.

### 4.1 RTC (containers, memory, value types): `rtc/dll/src/{ptr,mem,set,ser,geom,vt,mth,cpc}`

The recent container fixes (05f27df92, 72ed3b38e, dadf1121f, 0d21f86d1, 073947426, 184cbf1b0) are
correct at HEAD. The remaining defects are their unfixed siblings, and the numeric conversions.

#### RTC-A01 · BUG · CONFIRMED · High · S — `rounded_convert` to an unsigned target clamps at the signed maximum
- **Where**: `rtc/dll/src/vt/Conversions.h:183-199` (`RoundedConvertFunc::operator()`), `rtc/dll/src/vt/Round.h:35, 290-295` (`scalar_replace`, `Round<N>`); callers `clc/dll/src/OperConv.h:107, 116, 139, 219`; registered for float sources to every numeric target (`OperConvNumeric_float.cpp:18`, `OperConv.h:771-780`).
- `Round<sizeof(U)>` picks its result type from the source's signedness, and floats are signed, so a UInt8 target rounds in Int8, UInt16 in Int16 and UInt32 in Int32, and `RoundDown` clamps to that signed maximum after the range checks passed: `rounded_convert(200.2, uint8)` gives 127, 40000.4 → uint16 gives 32767, 3e9 → uint32 gives 2147483647. Point targets are hit per coordinate. f46091d59 fixed exactly this in `UpConvertFunc` (`RoundUpPositive`, `scalar_replace_u`), not here.
- **Fix**: for unsigned targets use `RoundPositive<sizeof(scalar_of_t<U>)>(val)`, as `UpConvertFunc` does; add testcases for uint8/16/32 targets.

#### RTC-A02 · BUG · CONFIRMED · Medium · S — Float → integer `Convert` skips range checks whenever the target has as many "value bits"
- **Where**: `vt/Conversions.h:110-124` (`check_min_mf`, `check_max_mf`), :250-261, :168-181 (`DefaultConvertFunc`); `Round.h:88-96` and siblings.
- The checks are skipped when `nrvalbits_of<U> >= nrvalbits_of<T>`, but a float's bit count is not its range: Float32 (31 value bits) → Int32/UInt32/Int64/UInt64 gets no max check, Float64 → Int64/UInt64 none, and Float64 → Int64 no min check either. `U(val)` out of range is UB; on x64 `UInt32(5e9f)` gives 705032704 and `ThrowingConvert` does not throw. In `RoundDown` / `RoundUp<8>`, `MAX_VALUE(Int64)` as a double is 2^63, so 2^63 passes `v > MAX`.
- **Fix**: always apply both checks for floating sources, against the exact power of two with a strict `<` (`v < 0x1p32`, `v < 0x1p63`).

#### RTC-A03 · BUG · LIKELY · High · S — `allocateSequenceRange` copies from freed memory when the source lies in the same `sequence_array`
- **Where**: `rtc/dll/src/vt/SequenceArray.cpp:694-704` (reallocating branch), :503-513 (`allocate_data(expectedGrowth)`); entries `SA_Reference::assign` / `operator=` (:158, 232, 241), `push_back_seq` (`SequenceArray.h:693`).
- The one-argument `allocate_data` swaps `m_Values` into a local that frees the old pool on return; `appendValues(first, last)` then copies from a range that pointed into it whenever the source was another sequence of the same array (`sa[i] = sa[j]`). The initializer variant `allocateSequence` (:636-645) keeps `oldData` alive for exactly this case: the family of 05f27df92 / 72ed3b38e. No live caller found, but `std::swap` / `std::sort` over `SA_Iterator` would reach it (there is no free swap for `SA_Reference`); `sequence_array::assign(*this)` (:316) is a related hazard.
- **Fix**: the two-argument `allocate_data(oldData, …)`, translating `first` / `last` into `oldData`; guard self-assignment.

#### RTC-A04 · BUG · LIKELY · Medium · S — `sequence_array::StreamIn` trusts index pairs read from a file
- **Where**: `vt/SequenceArray.cpp:801-828`, reached from `DataArrayBase<V>::DoReadData` (`tic/DataArray.cpp:466`) via `AbstrStreamManager::ReadDataItem` (fss, CompoundStorage).
- Each `(first, second)` is taken verbatim and `m_ActualDataSize` set, so the #1154 consistency check in `Lock` is skipped too; `checkActualDataSize` / `checkConsecutiveness` are `MG_DEBUGCODE`. A corrupt or truncated `.dmsdata` gives sequences that index outside the value pool.
- **Fix**: validate every defined pair and `calcActualDataSize() <= m_Values.size()` after reading; throw a storage error naming the file.

#### RTC-A05 · BUG · LIKELY · Medium · S — `FileOutStreamBuff` never reports an open or write failure
- **Where**: `rtc/dll/src/ser/FileStreamBuff.cpp:26-52`; users: the MMD dictionary (`MemoryMappedDataStorageManager.cpp:540-541`), fss non-data items (`FileSystemStorageManager.cpp:82`), GUI export (`DmsExport.cpp:193`).
- A failed open is ignored, `WriteBytes` does not check `fail()`, the destructor neither flushes nor checks: on disk-full or a network error an MMD dictionary or an export is silently truncated. The sibling `MappedFileOutStreamBuff` in the same file throws on both.
- **Fix**: throw on `!is_open()`, check state after each write, add a checked `Close()`.

#### RTC-A06 · PARTIAL / BUG · LIKELY · Medium (Linux) · M — Linux: the free-store drain decommits nothing but its bookkeeping says it did
- **Where**: `rtc/dll/src/mem/FixedAlloc.cpp:335-343` (`decommit_free_store`, POSIX branch commented out), :700-708; consumer `tic/OperationContext.cpp:1474-1488` (`LedgerObservedCommitBytes`).
- On POSIX the drain moves the store from `s_FreeStackDeadBytes` to `s_DrainedStackBytes` without releasing memory; the ledger computes occupancy as `VmRSS - deadBytes`, so every Linux drain raises apparent occupancy while RSS stays the same, and the gate throttles harder without getting memory back. The missing `madvise` is tracked (schedule-with-lookahead §8.1.24); this accounting consequence is not.
- **Fix**: enable `madvise(MADV_DONTNEED)`, or skip the drain and its dead-byte adjustment on POSIX.

#### RTC-A07 · PERF / SIMPLIFY · CONFIRMED · Medium · S — A hand-written copy assignment makes Point, Range, IndexRange and Pair non-trivially-copyable
- **Where**: `rtc/dll/src/vt/Couple.h:42-46`, `rtc/dll/src/vt/Pair.h:47-52`.
- `constexpr void operator=(const Couple&)` does what the default does but is user-provided, so `is_trivially_copyable` is false for every `Point<T>`, `Range<T>`, `IndexRange<T>`. `fast_copy` (memcpy), `fast_copy_backward` / `fast_move_backward` (memmove) and `fast_zero` never take their fast overloads for polygons, tiles and index arrays; `raw_movable<IndexRange>` is false, so `sequence_obj<IndexRange>::erase` relocates element by element; the standard library cannot memmove them either.
- **Fix**: delete or `= default` both operators (check that no caller uses the `void` return).

#### RTC-A08 · BUG · LIKELY · Low · S — Signed overflow left in `CheckedMul<Int64>`, `Size(Range)` and `Range_GetIndex`
- **Where**: `vt/CheckedCalc.h:257-264`, `geom/Range.h:327-329`, `vt/RangeIndex.h:24-31`.
- `CheckedAdd` / `CheckedSub` compute in unsigned; `CheckedMul<Int64>` still computes `a * b` signed and divides afterwards (`INT64_MIN / -1` raises #DE, reachable with the Int64 null). `Size` and `Range_GetIndex_naked_unchecked` subtract in the signed type before widening.
- **Fix**: `_mul128` / `__builtin_mul_overflow`; subtract in `unsigned_type_t<T>`.

#### RTC-A09 · BUG · SMELL · Medium · S — `fast_copy` uses `memcpy` although its assert admits overlapping ranges
- **Where**: `rtc/dll/src/set/rangefuncs.h:211-220`; reached from `my_vector::erase` (`ManagedAllocData.h:275-281`) via `raw_move` → `raw_copy`.
- `assert(!(first < target) || last <= target)` explicitly allows a forward overlap (erase-and-compact), but the `const T*` overload is `memcpy`. No live mid-vector erase on a trivially copyable `my_vector` found; GCC may exploit the assumption.
- **Fix**: `memmove`.

#### RTC-A10 · BUG · SMELL · Low · S — Appending one's own range is still unsafe in the three `append`s (siblings of 05f27df92)
- **Where**: `mem/ManagedAllocData.h:243-253`, `mem/SequenceObj.h:133-144`, `vt/SequenceArray.h:364-375`.
- Each grows first and then copies from a range that may lie in the released storage (`v.append(v.begin(), v.end())`). `push_back` / `emplace_back` were made safe and `SA_Reference::insert(range)` got a documented precondition; the appends got neither. No live self-append today.
- **Fix**: keep an index across the grow when `first` lies inside, or document the precondition.

#### RTC-A11 · BUG · SMELL · Low · S — Containers assign into raw storage and destroy twice
- **Where**: `ManagedAllocData.h:145-165` (`operator=(const&)`: `destroy_elements` then `fast_copy` onto destroyed objects), :183-195, :243-253; `OwningPtrSizedArray.h:140-152` (`grow` uses `fast_copy` into uninitialised storage; 073947426 fixed only `shrink`); `SequenceObj.h:184` (`raw_move` destroys the tail, then `cut` destroys it again).
- Correct only while element types are trivial, which holds today.
- **Fix**: `raw_copy` / `raw_init` for unconstructed targets, drop the extra `destroy_range`, or `static_assert` triviality.

#### RTC-A12 · BUG · SMELL · Medium · S — `file_tile::get` ignores a later caller's rwMode; the mapping destructor clobbers a newer cache entry
- **Where**: `rtc/dll/src/mem/tiledata.h:43-53, 75-90`; callers `tic/TileArrayImpl.h:501, 514`.
- While a read-only mapping lives, `GetWritableTile` gets the `PAGE_READONLY` view and faults on its first write; `~mapped_file_tile` resets `m_OpenFile` unconditionally, wiping a mapping created between the last release and the destructor.
- **Fix**: remap or fail when a write mode meets a read-only mapping; reset only when the entry still refers to this object.

#### RTC-A13 · BUG · CONFIRMED · Low · S — `CheckAllocSize` narrows its size to UInt32, so the guard never fires
- **Where**: `rtc/dll/src/set/VectorFunc.h:114-123`, called at :152, 163, 174, 185 with `SizeT` sizes from shapefiles, bmp and gdal.
- On 64-bit, `600e9 / objSize` exceeds `UINT32_MAX` for any objSize ≤ 139.
- **Fix**: take `SizeT`, or delete it and rely on `safe_size_n` / `MemoryAllocFailure`.

#### RTC-A14 · BUG · LIKELY · Low · S — `FormattedInpStream` treats byte 0xFF as end of file
- **Where**: `rtc/dll/src/ser/FormattedStream.cpp:284, 290` (`ReadDQuote`), :345 (`NextWord`).
- `char nextChar == EOF` is true for 'ÿ' in Latin-1 / CP1252 text: a quoted field containing it aborts the read.
- **Fix**: use `AtEnd()` instead of comparing with `EOF`.

#### RTC-A15 · BUG · SMELL · Low · S — The Linux string-compare shims redefine libc `strncmp` with signed-char semantics
- **Where**: `rtc/dll/src/ptr/SharedStr.h:35-83`; users `SharedStr::operator<(CharPtr)` (:264-278), `sym/LispRef.cpp:242, 251`, `ser/FormattedStream.cpp:688`, `utl/Environment.cpp:1099`.
- Under GCC `inline int strncmp(CharPtr, CharPtr, SizeT)` redefines libc's function, comparing signed `char`: non-ASCII orderings differ from Windows, and an emitted weak copy can interpose libc's for the process. `strnicmp` / `stricmp` call `tolower` on negative chars (UB).
- **Fix**: remove the `strncmp` shim; implement the case-insensitive ones over `unsigned char` or map them to `strncasecmp` / `strcasecmp`.

#### RTC-A16 · SIMPLIFY · SMELL · Low · S — `alloc_data` move assignment copies and then swaps nothing
- **Where**: `rtc/dll/src/mem/AllocData.h:36-43`.
- The base assignment copies `first` / `second`, so the following swaps are no-ops: the source keeps pointers into the moved buffer with capacity 0, and the target's old range is lost. All callers move into empty objects and reset the source.
- **Fix**: implement as a swap on an emptied `*this`, or `std::exchange`.

#### RTC-A17 · BUG · SMELL · Low · S — ser helpers
- `ser/ReadValue.h:101`: non-inline `bool ReadNull` defined in a header included by Rtc and Stg (ODR hazard); :68-95: numeric `ReadValue(FormattedInpStream&)` needs `GetDataBegin()`, so it throws IllegalAbstract over a `FileInpStreamBuff` (as TestScript and XmlTreeOut construct); `ser/MoreStreamBuff.cpp:122-134`: `CheckEqualityOutStreamBuff` compares with `strncmp`, stopping at the first NUL.
- **Fix**: `inline`; parse from a local buffer; `memcmp`.

#### RTC-A18 · SIMPLIFY · CONFIRMED · Low · S — Dead or uninstantiable code (repo-wide grep)
- Unused: `mth/BigInt.h/.cpp` `Big::UInt` except `RTC_UInt32xUInt32toUInt64` / `RTC_Int32xInt32toInt64` (its `operator/=` is an NYI stub guarded by `dms_assert(false)`); `SharedStr.h:111-183` `lex_compare_cs/ci`, `equal_cs/ci`; `SharedStr.cpp:355-437` `decode_utf8`, `ascii_case_fold`, `Utf8CaseInsensitiveEqual/Hasher`, :606 `SharedStr::ci_hasher`; `BitVector.h:69` `elem_mask` (also wrong for N > 1); `HeapSequenceProvider` `Shrink`, `max_size`, `iter()` helpers, `throwInsertError`; `AbstrSequenceProvider.h:21` `SaSizeT`; `MappedSequenceProvider.h:114-118` `Grow`; `FileView.h:93-111, 138-168` the `Open` members; `VectorFunc.h:332-336` `vector_copy`; `cs_lock_map.h:18-22` `MG_DEBUG_LOCKS` (false in both branches).
- Would not compile if used: `VectorFunc.h:422-434` `vector_erase_last`; `PtrBase.h:115-118` `ref_base::operator->/*`; `OwningPtrSizedArray.h:171-172` `IsDefined`; `Point.h:266-278` `DEFINE_UNARY_FUNC(con)` (typo for `cos`; the other three truncate for integral T).

**Tracked-item status (RTC core)**

- Fixed as code-fixes.md claims: RTC-30, RTC-31, RTC-48 to RTC-53, RTC-05, RTC-22.
- Still open and tracked: the POSIX `madvise` (schedule-with-lookahead §8.1.24; RTC-A06 adds the accounting consequence) and `MG_DEBUG_REFCOUNT` disabled during the std-ptr migration.

### 4.2 RTC (infrastructure): `rtc/dll/src/{act,utl,dbg,parallel,sym,mci,xct,xml,dllimp,gen}`

**Status of the boost::format → std::format migration.** No `<boost/format>` include remains and
`utl/MgFormat.h` is the std::format engine; literal format strings with printf directives remain only
in `stg/tst/src/main.cpp:107, 115, 223, 241`. Stage 2 (compile-time `std::format_string`) and stage 4
(`<strstream>` in the fixed-buffer helpers) are open. A script over every literal-format call in rtc,
clc, geo, shv, stg, stx, qtgui, python and run found five live argument mismatches (INF-A03).

#### INF-A01 · BUG · CONFIRMED · High · S — The XML reader hangs on any attribute value that contains an entity reference
- **Where**: `rtc/dll/src/xml/XmlParser.cpp:341-360` (`HtmlDecodeInPlace`), called from :430 (`ReadAttrValue`); comparator `xml/XmlConst.h:20-31` (`CompCharPtr`), used by `SymbolGetChar` (`XmlParser.cpp:530-538`).
- `CompCharPtr` is not symmetric between ';' and NUL: `comp("amp;b","amp")` is true and `comp("amp","amp;b")` false, so `map::find` never matches the ';'-terminated slice and `SymbolGetChar` returns 0 for every entity, `&amp;` included. `HtmlDecodeInPlace` then leaves the '&' and restarts `std::find` from the beginning: an endless loop whenever a ';' follows. Since #1261 (42b8da98e) every attribute value passes through it, so `a="R&amp;D.csv"`, `&quot;` or even `a&b;c` hang the load, and the writer's own `WriteAttr` produces such entities: a write-read round trip of an attribute with `&` or `"` hangs. Element text is unaffected (`TransformChar` passes a NUL-terminated name).
- **Fix**: compare names up to the first ';' or NUL on both sides; decode forward into a new buffer so an unknown entity cannot loop; add a battery case with `&` and `"` in an attribute.

#### INF-A02 · BUG · CONFIRMED · Medium · S — `unquote` / `undquote` of a one-character `'` or `"` reads outside the element
- **Where**: `rtc/dll/src/utl/Quotes.cpp:831-842` (`SingleUnQuote(StringRef&,…)`), :844-855 (`DoubleUnQuote`); callers `clc/dll/include/AttrUniStructStr.h:149-152, 171-175`.
- The clc guard checks only that the first and last characters are quotes, which is one character for a one-character value; the only length check is `dms_assert(begin+2 <= end)`. `++begin, --end` leaves begin past end and `while (begin != end)` walks through the neighbouring strings of the unterminated sequence buffer.
- **Fix**: `MG_PRECONDITION(end - begin >= 2)` in both overloads, or return undefined in clc when `arg.size() < 2`.

#### INF-A03 · PARTIAL / BUG · CONFIRMED · Medium · S (sites) / M (stage 2) — Without compile-time format checking, surplus arguments vanish and two error messages lose their text
- **Where**: `rtc/dll/src/utl/MgFormat.h:78-99` (`std::vformat` at run time), wrappers `dbg/Diagnostics.h:137-205` take `CharPtr`. Sites: `geo/dll/src/Voronoi.cpp:400` and `geo/dll/src/BoostPolygon.cpp:368` (`throwDmsErrF("voronoi", "…{}", x)` makes the type name the format); `clc/dll/include/AggrFuncNum.h:50, 55` (value argument dropped); `tic/stg/AbstrStorageManager.cpp:644` (extra argument dropped).
- boost threw `too_many_args`; `std::vformat` ignores surplus arguments. The voronoi range error now reads only "Error: voronoi", the GEOS deprecation error only "Error: PolygonOverlayOperator".
- **Fix**: `throwErrorF(type, fmt, …)` at those two calls; then stage 2 of the migration doc: `std::format_string<Args...>` parameters with a runtime-format escape.

#### INF-A04 · BUG · CONFIRMED · Low-Medium · S — `SetPriority` lowers the calling thread, so the GUI thread loses its raised priority after the first stack-depth hand-off
- **Where**: `rtc/dll/src/act/MainThread.cpp:45-54`; callers `SetMainThreadID` (:82-96), `SetMetaThreadID` (:98-102); hand-off `tic/TreeItemMetaInfo.cpp:938-980`.
- `GetCurrentThread()` returns the pseudo-handle meaning "the calling thread", and it is stored: "lower the previous meta thread" lowers the current one, and after the hand-off the GUI thread stays at NORMAL. `IsElevatedThread`, the only reader, has no callers. On POSIX it requests `SCHED_RR`, which fails silently for non-root users.
- **Fix**: a real handle (`DuplicateHandle`) or an RAII priority guard around the `std::async` hand-off.

#### INF-A05 · BUG · CONFIRMED · Low-Medium · S — In XML element text, unknown and numeric entity references become an embedded NUL
- **Where**: `XmlParser.cpp:130-155` (`TransformChar`), :203; `xml/XmlConst.h:12-15` claims numeric references are accepted.
- `&#169;`, `&#10;` or `&nbsp;` append a '\0', and every `c_str()` consumer (Descr, expressions) silently truncates there.
- **Fix**: reuse `html::ParseNumericRef` and `append_utf8` (`utl/Encodes.cpp:226-360`); report an unknown named entity as an XML error.

#### INF-A06 · PERF · LIKELY · Medium · M — Every token-string read and every item lock goes through one process-wide mutex
- **Where**: `rtc/dll/src/ptr/SharedBase.cpp:154-244` (`counted_mutex`: global `s_CountedMutexSection` and one shared condition variable); reached from `sym/Token.cpp:173-237` (`GetStrLock`, `AsSharedStr`, …), `mci/MciInterface.cpp:355-392` (`GetFullName`: three lock round trips per ancestor), :404-441 (`GetRelativeName`: four per level), and `ItemReadLock` / `ItemWriteLock` through the session usage counter.
- Each shared acquire and release locks one global `std::mutex`, and each release to zero `notify_all`s a condition variable shared by every `counted_mutex`: formatting a TokenID, every `GetFullName` and every item lock on the workers serialise here.
- **Fix**: an `std::atomic<int>` per `counted_mutex` (CAS for shared ≥ 0, CAS 0 → -1 for exclusive, per-instance `atomic::wait` / `notify_all`), keeping the reader preference `doc/deadlocks.md:736` relies on; one `AsStrRangeLock` per level in the name functions.
- **Measured 2026-09-29, not done**: the fix above was built exactly as described (a per-instance `std::atomic<int>`, reader-preferring, `try_lock_for` polling for the teardown drain, one `AsStrRangeLock` per level) and timed against its parent dbe256a84 on OVSRV10 with the machine otherwise idle, three rounds, interleaved, `Measure-Command` around `cmd /c GeoDmsRun`: a scratch probe of 20 million elements in 200000 tiles of 100 with a five-step chain and a sum, `/S1 /S2`, 15.44 s median before and 15.51 s after; t300_xml_ReadParse as full.py runs it, 42.09 s before and 42.06 s after (the result-folder write at its end failed in both variants, the calculation before it ran in full). The first round of each was an outlier on both sides; rounds 2 and 3 agree within 0.1 s. The global mutex is no bottleneck on these workloads, so the change to the lock primitive was reverted rather than committed. Not measured: many threads locking many distinct items at once, where a per-instance wait would matter most. The patch is kept out of the tree; re-measure on such a workload before reviving it.

#### INF-A07 · BUG · LIKELY · Medium (Linux) · S — POSIX `exec` with an explicit module passes the whole command line as one argument
- **Where**: `rtc/dll/src/utl/Environment.cpp:2274-2311`, :2370-2394; reached from `clc/dll/src/OperExec.cpp:182`.
- Windows builds the child's argv from `cmdLine`; POSIX passes `{module, cmdLine, nullptr}`, so argv[1] is the whole string. `strstr(moduleName, "cmd")` matches any path containing "cmd" and runs /bin/sh instead; a `waitpid` failure other than EINTR is reported as exit code 0.
- **Fix**: `/bin/sh -c` or split with Windows argv rules; match `cmd` / `cmd.exe` on the basename; return failure when `waitpid` fails.

#### INF-A08 · BUG · LIKELY · Medium-Low · S — `GetLastErrorMsgStr` dereferences a null or stale last-handled message
- **Where**: `rtc/dll/src/xct/DmsException.cpp:46-50, 528-540`; callers `run/exe/src/MainRun.cpp:234`, `stg/dll/src/GeoRef.cpp:131`.
- `g_LastHandledErrMsgPtr` is set only by `~DmsException` on the meta thread; a configuration that fails to load with `bad_alloc` or a library `std::exception` before any DmsException makes GeoDmsRun's "Last ErrMsg:" path dereference null; later it returns an unrelated older message.
- **Fix**: null check; record the `ErrMsg` built in `catchException` / `catchAndProcessException` for every exception type.

#### INF-A09 · PARTIAL / BUG · LIKELY · Low · S — Exception reporting differs per platform: on POSIX every nested C-API boundary calls the client's handler
- **Where**: `rtc/dll/src/dbg/DmsCatch.h:55` vs :64; `xct/DmsException.cpp:441-460, 508-521, 954-969` (`CppTranslatorContext` only under `_MSC_VER`); client handler `MainRun.cpp:524-527, 552`.
- On Windows each `DMS_CALL_BEGIN` nulls the handler for its own try, so only the outermost boundary reports; off MSVC `DMS_EH_CONTEXT` is empty, so on Linux every nested `DMS_CALL_END` on the main thread prints "Caught at Main:" to stderr only, and the /L log misses those errors. `DMS_SetGlobalSeTranslator` / `s_SeTrGlobalFunc` are written, never read, and undeclared.
- **Fix**: enable `CppTranslatorContext` everywhere (it is plain C++); delete the SE-translator setter.

#### INF-A10 · BUG · LIKELY · Low · S — The Lisp string-constant cache compares keys with `strncmp` (the unfixed twin of RTC-38)
- **Where**: `rtc/dll/src/sym/LispRef.cpp:239-253`.
- RTC-38 made the key the whole range but equality still stops at the first NUL: `"a\0b"` and `"a\0c"` compare equal, so a string literal with `\0` can get another constant. Related: `utl/Encodes.cpp:370, 433` (`to_utf` / `from_utf` build via `c_str()`), `ser/AsString.h:57` (`AsDataStr(WeakStr)`) truncate at an embedded NUL.
- **Fix**: `memcmp` over full sizes; build these strings from ranges.

#### INF-A11 · PARTIAL · SMELL · Low · S — Fixed-buffer formatting: stage 4 open, and the helper can return an unterminated string
- **Where**: `rtc/dll/src/utl/FixedBufferFormat.h:22-78`.
- Still `std::ostrstream` (removed in C++26), fed from a heap-allocated string while the header says it avoids the heap; `myFixedBufferAsCString` leaves the buffer unterminated when the text fills it (its assert cannot fail); `myFixedBufferWrite` can return `size`, so callers must reserve size+1 (today all do).
- **Fix**: `std::format_to_n` into `size-1`, always terminate, return the written length.

#### INF-A12 · BUG / SMELL · LIKELY · Low · S — The message dispatcher cuts every line to 256 bytes for all receivers, splitting UTF-8
- **Where**: `rtc/dll/src/dbg/MsgDispatch.cpp:129-130`, :165 (`FlushMsg`).
- The /L log loses the tail of long error lines too, and a multibyte character at byte 256 is split. `(i != e && e[-1] == 0) || e[-1] == '\n'` reads `e[-1]` when `i == e`.
- **Fix**: truncate in the GUI receiver only, at a UTF-8 boundary; `i != e && (e[-1] == 0 || e[-1] == '\n')`.

#### INF-A13 · SIMPLIFY · CONFIRMED · Low · S — `dllimp/RunDllProc` is unreachable; the 2026-09-13 `DllHandle` cleanup polished dead code
- **Where**: `rtc/dll/src/dllimp/RunDllProc.cpp:1-223`, `RunDllProc.h:15-51`; the only caller `clc/dll/src/OperExec.cpp:193-249, 385-399` is behind a commented-out `//#define OPER_EXECDLL` (:26).
- **Fix**: delete, moving the live `g_IsTerminating` and `DMS_Terminate` (:225-230) elsewhere; or re-enable `exec_dll` deliberately (then the `std::map` cache needs a lock).

#### INF-A14 · SIMPLIFY · CONFIRMED · Low · S — Dead scaffolding in sym and act: Prolog.cpp, the `MG_USE_LISPFUNCS` evaluator, `UpdateMarker::Renumber`
- **Where**: `rtc/dll/src/sym/Prolog.cpp` and `Prolog.h:31-33` (never called; the exported declarations do not match the definitions); `sym/LispEval.cpp:20-29, 58-192, 254-350` (never enabled and would not compile); `act/UpdateMark.cpp:190-265`, `UpdateMark.h:114` (`Renumber`: no caller, and it would compress nothing).
- `typed-hof-language-design.md` keeps Prolog as reference for WP3.2.
- **Fix**: delete, or fix the Prolog header if WP3.2 keeps it.

#### INF-A15 · SIMPLIFY · CONFIRMED · Low · S — Dead or broken leftovers in utl, gen and mci
- `Quotes.cpp:602-614` `SingleUnQuoteMiddle` (its assert is always false), with its only caller `SingleUnQuote(CharPtr)` (:793) uncalled; also :801, :810, :818, :857, :870; `Quotes.h:54` declares an undefined `SingleUnQuote(SharedStr&,b,e)`. `Encodes.cpp:20-65` `url::impl` / `url::IsSafeChar` unused. `Environment.cpp:1084-1156` `MoveFileOrDir` unused on both platforms (its Windows branch :1118-1125 is inverted); `CopyFileOrDir`'s `mayBeMissing` is ignored on Windows and swallows every error on POSIX (:2205-2214); :2057-2166 POSIX `FindFileBlock` unused and leaks its `DIR*`. `Token.h:338` / `Token.cpp:168-171` `GetExistingTokenID_st` dead. `gen/General.cpp:187` prints "_MSC_VER = _MSC_VER" on GCC; `DMS_RTC_Test` declared (`RtcInterface.h:64`) but defined only in Debug. `mci/SingleLinkedTree.h/.inc` unused (`shv/GraphicContainer.cpp:14` includes the .inc without using it).

#### INF-A16 · SMELL · LIKELY · Low · S — Exported name functions return registry pointers after releasing the lock that protects them
- **Where**: `mci/PropDef.cpp:267-276, 300-309`, `mci/MciInterface.cpp:747-757`, `mci/ValueWrap.cpp:413-423`, `tic/TicInterface.cpp:408-416`.
- `return self->GetNameLock().c_str();` drops the registry's shared lock at the end of the statement; the registry reallocates on the next token creation. No in-repo callers.
- **Fix**: delete them or return pointers into stable storage.

#### INF-A17 · SIMPLIFY · CONFIRMED · Low · S — DmsException's "exception being unwound" bookkeeping is written but never read
- **Where**: `xct/DmsException.cpp:37-44, 187-198, 542-549`, `DmsException.h:39`.
- Every throw swaps a thread-local `shared_ptr` and every destructor restores it, but `GetUnrollingErrorMsgPtr` has no caller; the value is also wrong for exceptions rethrown on another thread.
- **Fix**: delete.

#### INF-A18 · PARTIAL · CONFIRMED · Low · S — The "no fallback after failure" guard in `Actor::UpdateLock` can never fire
- **Where**: `act/Actor.cpp:83-89, 103, 130-132`.
- All three test `ts > AF_DeterminingState`, the largest transient state, so the `ThrowFail()` and both asserts are dead and `TransState_FailType` is never called; its table maps CalculatingData to Validate and ChangingInterest to Data, so flipping the comparison alone would throw the wrong failure type.
- **Fix**: restore with `<` and a corrected mapping, or delete.

#### INF-A19 · PERF · LIKELY · Low · S — The rewrite-rule result cache grows for the process lifetime
- **Where**: `sym/LispEval.cpp:473`, :503-547 (`g_applyTopEnvCache`).
- It owns every expression ever rewritten and every intermediate step; in a GUI session the expressions of every configuration loaded stay alive until exit.
- **Fix**: clear it when a session closes, or bound it.

#### INF-A20 · BUG / SMELL · LIKELY · Low · S — Suspend and progress throttles use the wall clock and CPU time
- **Where**: `dbg/Timer.h:31-41` (`time()`: 1 s resolution, wall clock; the only suspend trigger on Linux, `act/TriggerOperator.cpp:413`); `TriggerOperator.cpp:97-127` (`clock()`).
- A backward clock step stops GUI suspension on Linux; POSIX `clock()` is CPU time over all threads, so the throttle window scales with the thread count and progress messages are dropped while the meta thread idles.
- **Fix**: `std::chrono::steady_clock` in both.

#### INF-A21 · SMELL · Low · S — The first status-flag read publishes `RSF_WasRead` before the flags
- **Where**: `utl/Environment.cpp:558-591` (Windows), :2711-2728 (POSIX). Correct today only because the first read happens during static initialisation.
- **Fix**: compute in a local, store once.

#### INF-A22 · BUG · LIKELY · Low · S — `ctype` functions called on plain `char`
- **Where**: `sym/Token.cpp:251-257` (`Trim`, used for ExplicitSuppliers names at `tic/TicDataSupport.cpp:185`, `tic/TicInterface.cpp:1129`), `utl/Case.cpp:20-39` (UpperCase / LowerCase operators), `ptr/SharedStr.h:58, 73`.
- A negative `char` is UB; MSVC's Debug CRT asserts in `isspace`, so a Debug run exits on an ExplicitSuppliers entry that starts or ends with a non-ASCII letter.
- **Fix**: cast to `unsigned char`.

#### INF-A23 · SMELL · Low · S — The config-file registry is iterated without its lock
- **Where**: `utl/SourceLocation.cpp:60-84, 108-132` iterate `s_FDS` unlocked while `FileDescr`'s constructor and destructor (:34, :42) take `cs_FDS`. Either the lock is unneeded or the iteration must take it.

#### INF-A24 · SMELL · Low · S — The debug-reporter list cannot unlink its tail (Debug builds)
- **Where**: `dbg/DebugReporter.cpp:42-47`, `DebugReporter.h:34-45`. `clear()` treats "has a next pointer" as "is linked", so the tail is never removed and destroying it leaves the head dangling (benign while every reporter is static). `ReportCount` (:68-80) has no caller (g8-todos.md:140).

**Tracked-item status (RTC infrastructure)**

- **cleanup-list item 5** (split `Environment.cpp`): open. Still duplicated per platform: the session-local override quartet; `RegAccessSection` with its comment; the status-flag family (`DMS_Appl_SetRegStatusFlags`, `ReadOnceRegisteredStatusFlags`, `Get/SetCachedStatusFlag/Mask`, `SetRegStatusFlags`, `SetStatusFlag`, `IsMultiThreaded0-3`, `ShowThousandSeparator`, `EventLog_HideDeprecatedCaseMixupWarnings`); the 45-line `RTC_ParseRegStatusFlag` switch and `ParseRegStatusFlags`; `GetLocalDataDir` / `GetSourceDataDir`; `MakeDirsForFile(Impl)`, `GetWritePermission`; `ManageSystemError` and the main-window handle.
- **cleanup-list item 11** (one DllHandle): done, but the whole unit is unreachable (INF-A13). **Item 4** (DmsYield): done.
- **RTC-18**: partial; `OperationContext.cpp:753` still ignores the `bool`.
- **RTC-38**: fixed for building the key, not comparing it (INF-A10).
- **RTC-01/03/04/N02** (XML entities): overflow and dangling key fixed; decoding still broken (INF-A01, INF-A05). **RTC-C13** (code-fixes.md:792) is wrong: the `XmlConstMap` keys are `"lt"`, `"amp"`, not `"name;"`; and the Appendix entry "refuted: XmlParser.cpp:256 range end" (code-fixes.md:893) is a live defect (INF-A01).
- **RECURSION_REFACTOR_PLAN.md:297-300**: `ApplyTopEnvFunc::operator()` still dead (`LispEval.cpp:431-471`, duplicating :506-541).
- **boost-format-to-std-format-migration.md**: stages 0, 1, 3 done without a status line; stages 2 and 4 open; `stg/tst/src/main.cpp:107, 115, 223, 241` still use printf directives, which now print verbatim.
- Wrong doc statements: `header-hygiene-2026-08.md:74` ("SingleLinkedTree.h is NOT dead"); `tu-reorg-and-export-surface-2026-08.md:31` ("RunDllProc used by clc's exec operator"); `doc/incremental-updates.md:23` (`Renumber()` as live). g8-todos.md:140 (`ReportCount`): still open.

### 4.3 TIC (calculation and scheduling): `rtc/dll/src/tic` scheduler, locks, tiles and storage base

The #1259 commit and IntegrityCheck deferral is fully removed from the code: `DeferScope`,
`LedgerNote*`, `CountedAsWaitingJoin`, `StartSupplierProduction` and the retry loop are gone. What
remains are stale comments and a half-reverted ledger-pressure path (TIC-A18 to TIC-A20).

#### TIC-A01 · BUG · CONFIRMED · Medium · S — `RefreshEstimateForAdmission` calls the meta-thread-only `FuncDC::GetArgs` on pool workers
- **Where**: `rtc/dll/src/tic/OperationContext.cpp:1913-1963` (`GetArgs(false,false)` at :1946), reached from `TryRunningTaskInline` :1244 on workers via `selfCaller` :745; `GetArgs` asserts `IsMetaThread()` at `MoreDataControllers.cpp:680`.
- With resource-aware scheduling (`/SQ`, `/Sq`) every context started by a worker re-estimates itself by calling `funcDC->GetArgs` on that worker. Debug asserts on the first one; Release runs `MakeResult` / `DetermineState` / `RestartSupplInterestIfAny` of the argument DCs off the meta thread, and for `calc_always` arguments reaches `CallCalcResult`, which can schedule from a worker. `EstimateOperPerformance` also asserts `IsPerformanceLogging()` (`PerfMeasurement.cpp:77`), which this path does not check.
- **Fix**: estimate from the `ArgRefs` the context already owns (`OC_CalcResultFunc::argRefs`, read under `cs_ThreadMessing`), or take the estimate on the meta thread only.

#### TIC-A02 · BUG · LIKELY · Medium · S — The low-RAM brake parks a supplier that a running operation is joining
- **Where**: `OperationContext.cpp:661-676` (`collectOperationContexts`), `Join` :2856-2937 (`s_NrWaitingJoins` at :2882, wait at :2936).
- A worker running context A joins a same-phase supplier B that is still `scheduled` (e.g. `ItemReadLock` → `lock_shared` → `producer->Join()`). The worker adds one waiting join, but A itself is counted as running, so `running >= Max(joins, 1)` holds and above `MemoryFlushThreshold` the pass stops before B is activated. `Join` only runs an `activated` context inline, so the worker polls every 500 ms until RAM load drops, which may never happen when this process holds the memory. The floor of one of 53c0931b4 does not help: the joiner's own context fills that slot. `doc/development/schedule-with-lookahead.md` §2.2 item 1 calls this a progress guarantee.
- **Fix**: when `Join` finds its target `scheduled`, activate it regardless of the brake (the joining thread runs it inline), or do not count a running context whose thread waits in a `Join`.

#### TIC-A03 · BUG · LIKELY · Medium · S — A lazy grid read keeps a `StorageMetaInfo` that closes the shared storage without its lock
- **Where**: `rtc/dll/src/tic/stg/StorageReadOperators.cpp:299-323` (`tileGenerator` captures `smi`, kept by the `LazyTileFunctor`), `AbstrStorageManager.cpp:61-67` (`~StorageMetaInfo` → `CloseStorage`), :1235-1244.
- `ReadPendingMembers` (:519, :562) makes a meta info die while the storage's critical section is held. The lazy multi-tile path (tif, gdal.grid, bmp with `AllowRandomTileAccess`) lets it die whenever the data object is released, on any thread, without that section. If another attribute of the same storage is being read at that moment, `DoCloseStorage` closes the file under the running reader; the `m_IsOpen` bitfield races too.
- **Fix**: capture a meta info that does not own a close of the main manager (clear its `m_StorageManager`, or build one for the reader clone), or take the section in `~StorageMetaInfo`.

#### TIC-A04 · BUG · LIKELY · Medium · M — A member that gains interest while its table read runs is never read
- **Where**: `ItemLocks.cpp:678-700` (`IsAllInterestedCalculatingOrDataReady_impl`), `MoreDataControllers.cpp:429-433`, :977-992, `StorageReadOperators.cpp:741-765`.
- `storage_read_table` collects the interested members in `PreCalcUpdate`; re-entry for later members (#1167) only works after the running operation ends. A member that gains interest after collection but before the end sees a write-locked root, the check returns true, no calculation starts, and consumers connect to an operation that does not read it. After it ends, `SetReadLock` fails with "neither calculating nor ready nor failed". Views recover by polling; a consuming operation fails.
- **Fix**: when the root is calculating, still look for interested members that were not collected and schedule a follow-up operation that the consumer waits on; or re-collect in `CalcResult` before releasing the request.

#### TIC-A05 · BUG · CONFIRMED · Medium · S — `GetNrNulls` calls `GetTile(no_tile)` on tilings with gaps
- **Where**: `DataArray.cpp:554-581` (`GetTile` at :574); caller `clc/dll/src/GetStatistics.cpp:461`.
- The non-covered branch (every `IrregularTileRangeData`) walks every range row; a row in a gap after a real tile has `tl.first == no_tile`, and `GetTile(no_tile)` indexes `m_Seqs` / `m_ActiveTiles` far out of bounds. Trigger: statistics of a string attribute on a `TiledUnit(lb, ub)` domain with gaps; a case the #1242 fixes missed. `SetNull` (:526-532) and `SetIndexedValue` (`DataArray.ipp:19-32`) have the same pattern behind a `dms_assert`, and they index by range row (`GetTiledLocation`) where `GetIndexedValue` indexes by data row (`GetTileDataLocation`), so setter and getter address different elements on such tilings.
- **Fix**: `if (!IsDefined(tl.first)) { ++count; continue; }`; make the setters' checks `MG_CHECK` and settle on one index convention.

#### TIC-A06 · PERF · CONFIRMED · Medium · S — Every `HeapTileArray` tile access takes one process-wide mutex, and allocates and zeroes under it
- **Where**: `TileArrayImpl.h:151-188` (`InitTile` under `s_mutableTileRecSection`, defined `DataArray.cpp:46`).
- `GetTile` / `GetWritableTile` of every multi-tile heap array in the process lock the same `std::mutex`, also after the tile exists, and the first touch runs `reallocSO(tile, size, mustClear)` inside it. A `parallel_tileloop` writing a tiled result serialises its tile allocations, and all readers of all heap arrays contend on one lock. Tiles are never reset after creation.
- **Fix**: a per-tile `std::once_flag`, or build the tile outside the lock and publish it with an atomic compare-and-swap; read without locking once published.

#### TIC-A07 · BUG · LIKELY · Medium · S — `GetDataControllerImpl` inserts with a hint that another thread may have invalidated
- **Where**: `DataController.cpp:476-528`.
- `lower_bound` is taken under the lock, the lock is released for `CreateDC` (long, recursive), then `insert(dcPtrLoc, …)` runs under a new lock. `~DataController` erases its map entry from any thread (the wait loop at :490-509 exists because it does); if the hinted neighbour is erased in between, the hint is a dangling iterator and the tree can be corrupted. The comment at :514 covers concurrent inserts, not erases.
- **Fix**: `emplace` without a hint, or redo `lower_bound` under the second lock.

#### TIC-A08 · BUG · CONFIRMED · Low · S — `separateResources` erases itself from its own waiter set instead of its suppliers'
- **Where**: `OperationContext.cpp:2128-2129`: `for (const auto& supplier : m_Suppliers) m_Waiters.erase(this->weak_from_this());`.
- The loop does nothing useful; the intent is `supplier->m_Waiters.erase(...)`. A cancelled or failed waiter stays in each supplier's waiter set; its weak reference keeps the `make_shared` block alive, and when that supplier later fails, `disconnect_supplier` queues `GetResult()->Fail(...)` against the already cancelled waiter (:533-546).
- **Fix**: `supplier->m_Waiters.erase(weak_from_this());`.

#### TIC-A09 · PERF · LIKELY · Low–Medium · S — `s_RadioActives` only shrinks when a worker steals
- **Where**: `OperationContext.cpp:195`, push :1217-1218, only pop :2759-2790.
- Every activation pushes a weak pointer; entries are popped only on the work-stealing paths of a worker's `Join` / `DoWorkWhileWaiting*` / `StealOneTask`. Contexts run by the pool through `selfCaller` never pop, and each stale weak pointer pins the whole `make_shared` allocation (≈ 350-400 B). A blocked-phase entry at the front also stops all stealing (:2776).
- **Fix**: drop the entry when the context gets its licence or ends, or trim expired and no-longer-activated entries from the front in each collect pass.

#### TIC-A10 · BUG / PERF · LIKELY · Medium · S — Tile functors keep their apply function for life; OperAttrBin's capture re-forms the item ↔ functor cycle
- **Where**: `TileFunctorImpl.h:121-131` (member `ApplyFunc aFunc`, not read after construction), :149-152, :204; `clc/dll/include/OperAttrBin.h:157` captures `std::shared_ptr<AbstrDataItem> resultAdi`.
- Each tile record copies `aFunc` and drops it after computing, but the functor's own copy lives as long as the data object, pinning its captures (argument handles, `LookupImpl` value arrays, `OperConv` units). OperAttrBin captures the result item strongly: item → data object → functor → `aFunc` → item, the cycle that weak `m_ResultAdi` / `ImLosingIt` was designed to cut; it is cut only if `ClearDataObject` runs, so KeepData results and teardown leak. The capture is redundant: both `GetTile`s already check `WasFailed`. `teardown-leak-and-ownership-cycles.md:91-94` and `schedule-with-lookahead.md:1511` call the cycle cut.
- **Fix**: remove the `FutureTileFunctor::aFunc` member; drop the `resultAdi` capture in OperAttrBin.

#### TIC-A11 · BUG · LIKELY · Low · S — `RunOperator`'s catch block re-reads `m_FuncDC` unlocked, the sibling of the fixed measure branch
- **Where**: `OperationContext.cpp:3149-3150`.
- `if (m_FuncDC) … GetOperGroup()->GetName()` reads the weak pointer twice without the lock; a tile worker's `CancelIfOutOfInterest` or completion can reset it in between. The measure branch (:3108-3113) was fixed for exactly this (t641).
- **Fix**: use the local `funcDC` that is already held: `funcDC->m_OperatorGroup->GetName()`.

#### TIC-A12 · PERF · CONFIRMED · Low–Medium · S — The garbage returned by `TryCleanupMem` is destroyed while the item's lock is held
- **Where**: `DataLocks.cpp:154-162`, `TreeItem.cpp:1531-1533`, `TreeItemMetaInfo.cpp:1267-1269` (only `TreeItem.cpp:2697-2699` keeps the value).
- `TryCleanupMem` returns a `garbage_can` so that callers free the data object outside their locks; three of four callers discard it, so gigabytes of tiles, file unmaps and cascading functor releases are freed under `specificSectionLock`, and the cascade takes further item locks while holding this one.
- **Fix**: `[[nodiscard]]` on the function, and a local declared before the scoped lock in each caller.

#### TIC-A13 · PERF · CONFIRMED · Low (Windows) / Medium (Linux) · S — Memory-status system calls under the global scheduling mutex
- **Where**: `OperationContext.cpp:668-676` (`IsLowOnFreeRAM` in `collectOperationContexts`), :1665, :1715, :1731 (`TotalAllowedPhysicalMemory`); `utl/MemGuard.cpp:85-113` (Linux parses `/proc/meminfo` with an `ifstream`).
- Nearly every activation pass (every `Join` iteration, every context end) probes free RAM under `cs_ThreadMessing`, and with the ledger on each admission calls `TotalAllowedPhysicalMemory` two or three more times.
- **Fix**: cache `TotalAllowedPhysicalMemory` (it only changes with `MemoryRAM_MAX_GB`) and rate-limit the free-RAM probe to every 10-50 ms, as `LedgerObservedCommitBytes` already does.

#### TIC-A14 · BUG · CONFIRMED · Low · S — `DMS_IsConfigDirty` locks the same `std::mutex` twice
- **Where**: `SessionData.cpp:131-142` → `IsConfigDirty` :123-129; `sd_SessionDataCriticalSection` is a plain `std::mutex` (:30).
- MSVC throws `resource_deadlock_would_occur` (the call reports an error and returns false); on Linux it deadlocks. Exported, no caller in the tree, but `doc/incremental-updates.md:119` documents it as working.
- **Fix**: drop the outer lock, or call an unlocked `isConfigDirty()` from it.

#### TIC-A15 · BUG · CONFIRMED · Low · S — `DMS_StorageManager_GetName` returns a pointer into a freed temporary
- **Where**: `stg/StorageSupport.cpp:114-124`.
- `GetNameStr()` builds a new `SharedStr`; `.c_str()` of that temporary is returned after it is freed. The twin `DMS_StorageManager_GetType` was made safe. Exported, no caller in the tree.
- **Fix**: return the interned token string of `m_ID`, or an `IStringHandle`.

#### TIC-A16 · BUG · CONFIRMED · Low · S — `gdalwrite.*` storage-type messages are inverted and always name "vect"
- **Where**: `stg/AbstrStorageManager.cpp:660-667`, and :644-646.
- `StorageReadOnly=true` on gdalwrite says it "does not allow for writing, yet StorageReadOnly is specified as true"; `typeID == s_gdalGridToken ? "grid" : "vect"` is never true in that branch, so gdalwrite.grid users read about gdalwrite.vect and gdal.vect. The odbc message passes an argument its format string has no `{}` for.
- **Fix**: compare with `s_gdalWriteGridToken` and reword; drop the stray argument.

#### TIC-A17 · BUG · CONFIRMED · Low · S — `GetValuesRangeCount` returns the number of bits instead of the number of values
- **Where**: `DataArray.h:233-242` (`else return nrbits_of_v<V>;`).
- For fixed-range bit types the count is `1 << nrbits` (`FixedRange<N>::GetRangeSize` does so): Bool gives 1 instead of 2, UInt4 gives 4 instead of 16. Consumer: `BmpStorageManager.cpp:202` (`SetClrImportant`).
- **Fix**: `return row_id(1) << nrbits_of_v<V>;`.

#### TIC-A18 · PARTIAL · CONFIRMED · Low · S — The ledger's commit-pressure coupling is half reverted
- **Where**: `OperationContext.cpp:1423-1444`, :1788-1799, :1812.
- Comments still say the pressure signal "arms the phase-hygiene deferral" and defers growers when the process commit exceeds the budget; the drain clause at :1800 tests only `sd_LedgerClaimant`, so the "process commit exceeds the {} B budget" message branch is unreachable, and `UpdateLedgerCommitPressure` runs on every admission only to feed a log line.
- **Fix**: remove the unreachable branch and correct the comments, or make pressure a log-only sampler as §8.1.30 of the design doc says.

#### TIC-A19 · PARTIAL / SIMPLIFY · SMELL · Low · S — `StartOperationContextsAsWaiter` is mostly redundant since the brake's floor of one
- **Where**: `OperationContext.cpp:766-781`, `OperationContext.h:416-419`; callers `shv/dll/src/DataView.cpp:2220,2281`, `shv/dll/src/GraphicObject.cpp:296`.
- 53c0931b4 measured that the floor of one alone removes the stall; the per-pass `+1` now only adds one extra activation for the view pass. The header speaks of "the one such consumer" and misses GraphicObject.
- **Fix**: fold it into `StartOperationContexts`, or document it as a one-pass priority boost with both callers.

#### TIC-A20 · PARTIAL · CONFIRMED · Low · S — Stale references to the removed item-writer read path; `SupportsReadOperator` is vestigial
- **Where**: `stg/AbstrStorageManager.cpp:772`, `stg/AbstrStorageManager.h:209-212`, `TreeItemMetaInfo.cpp:216` ("stays on the item-writer read path of PrepareDataRead", removed in #587 S3); `OperationContext.cpp:2669` ("set by ScheduleItemWriter"); `OperationContext.h:19,35` (PPL tasks, `dms_task`); `OperationContext.cpp:1741` ("estimates are only taken when measuring"); `AbstrStorageManager.h:287-289`.
- An empty `ReadCallSpec` now means the item has no read at all, not a fallback; the comments mislead whoever debugs a stored item that is never read. Every reader's `SupportsReadOperator` returns true; only `GdalWritableVectSM` returns false.
- **Fix**: rewrite the comments; replace `SupportsReadOperator` by `!IsWriteOnlyStorage()`.

#### TIC-A21 · SIMPLIFY · CONFIRMED · Low · S — Dead code in the scheduler and lock layer
- `ParallelTiles.cpp:68` `s_IsCancelled`; `OperationContext.cpp:198/657/672` `s_IsInLowRamMode` (written, never read); `ItemLocks.cpp:716-726` `CheckAllSubDataReady`, :840-845 `IsCalculatingOrReady(DC*, …)` (exported, ignores two parameters), :854-868 `IsInWriteLock` (no callers in the repo) and their `ItemLocks.h` declarations; `MetaFuncApply.cpp:444, 487-494` (uninitialised `result`, unreachable tail); unused locals `OperGroups.cpp:112-113`; the `storage_read_attrs` token (`LispTreeType.cpp:216`) with no operator behind it; commented-out blocks at `ItemLocks.cpp:204-213`, `ParallelTiles.cpp:342-350`, `DataController.cpp:577-587`, `DataLocks.cpp:338-351`, `TileArrayImpl.h:140-147`, `OperGroups.cpp:185-209`, `MoreDataControllers.cpp:1203-1216`.

#### TIC-A22 · SMELL · CONFIRMED · Low · S — `DelayedTileFunctor`'s constructor check is a tautology and comes after the pointer was used
- **Where**: `TileFunctorImpl.h:36-47`: `MG_CHECK(trd->GetNrTiles() == trd->GetNrTiles())`; `MG_CHECK(tiledDomainRangeData)` runs after the base constructor and the `m_ActiveTiles` initialiser dereferenced it.
- **Fix**: check for null before use; delete the tautology or state the intended invariant.

#### TIC-A23 · SMELL / PERF · LIKELY · Low · S — `IsInMMD` locks a weak pointer twice and keeps a raw pointer; runs per operator
- **Where**: `stg/MemoryMappedDataStorageManager.cpp:656-667`.
- `expired()` followed by two `lock().get()` calls is check-then-use: an expiry in between gives `AsDataItem(nullptr)`. It runs on every multi-tile operator and walks `GetCurrStorageParent` each time.
- **Fix**: lock once into a local; consider caching the answer on the item.

#### TIC-A24 · BUG · LIKELY · Low · S — `UsingCache::FindNamespace`'s `dms_assert` is false on the BUSY lookup path
- **Where**: `UsingCache.cpp:496-505`, via `FindItem`'s BUSY branch :520-528 with `allowAbsolutePath = false`.
- For a parentless cache context without usings, `dms_assert(url[0] == '/')` is evaluated on a plain identifier: `CC_ASSUME(false)` in Release, after which the identifier is resolved from the root as if it were a path.
- **Fix**: return `{}` when the URL is not absolute; make the check an `MG_CHECK`.

#### TIC-A25 · SMELL · CONFIRMED · Low · S — The read estimate says "eager" while a random-access grid read streams
- **Where**: `PerfMeasurement.cpp:224-228` vs `StorageReadOperators.cpp:299-323`.
- `EstimateReadResources` sets `regime = eager` and the whole array as resident, but a multi-tile read from random-access storage installs a `LazyTileFunctor`; the admission gate and the `!regime=` diagnostics are wrong for every tif / gdal.grid read.
- **Fix**: mirror the `IsMultiThreaded3() && tn > 1 && AllowRandomTileAccess()` test.

#### TIC-A26 · SMELL · CONFIRMED · Low · S — `dms_assert` with a side effect in `CopyData`
- **Where**: `TicDataSupport.cpp:406` (`dms_assert(writeChannel.IsEndOfChannel())`).
- `IsEndOfChannel` advances tiles (`GetWritableTile`); the expression is evaluated under GCC's `__builtin_unreachable` form but not under MSVC's `__assume`, so behaviour differs per platform.
- **Fix**: evaluate into a local, then check it.

#### TIC-A27 · BUG · CONFIRMED · Low · S — `GetProjDir` drops a leading dot from a name
- **Where**: `stg/AbstrStorageManager.cpp:279-310`.
- `".x"` and `"..x"` both resolve to `<base>/x`: any leading run of dots is taken as navigation, also when it is not followed by `/` or the end.
- **Fix**: accept a dot run as navigation only when followed by `/` or NUL.

**Tracked-item status (TIC)**

- **RTC-12 / RTC-C14 / TIC-10** (code-fixes Phase 2, marked done) are only partly done: `MemoryLedger_Retain` (:1665, `raw > TotalAllowedPhysicalMemory()`) and `MemoryLedger_Release` → `MemoryLedger_ConsiderSample` → `LedgerBudgetBytes` / `LedgerEffectiveCommittedBytes` (:1518-1549, :1731) still run outside any `try` inside `separateResources`, which is reached from the `noexcept` `onEnd`; a throw there is `std::terminate`.
- **RTC-18**: `StartCollectedOperationContexts` (:740-755) ignores a false `run()`; an activated context whose task was dropped stays counted (shutdown only).
- **R3** (try_lock probes): owner `try_lock` asserts remain in `OperationContext.cpp` (:297, 376, 451, 462, 491, 581, 616, 795, 922, 1188, 1274) and `ParallelTiles.cpp` (:79, 91, 265, 283, 309, 324); `assert(!m_CriticalSection.try_acquire())` in `AbstrStorageManager.cpp` (:745, 1194, 1215, 1240, 1314) and `StorageReadOperators.cpp:283` acquires the semaphore when the probe fails in Debug.
- **g8 `CalcResultWithValuesUnits`**: still open; its commented-out body sits at `DataController.cpp:577-587`.
- **TIC-N04**: fixed.
- Wrong doc statements: `schedule-with-lookahead.md` §2.2 item 1 (TIC-A02); `storage-read-operators.md` :127-131, :236-239, :1165 describe `CreateItemWriter` and a non-FuncDC early return of `RefreshEstimateForAdmission` that went with #1248; `teardown-leak-and-ownership-cycles.md:91-94` (TIC-A10); `incremental-updates.md:119` (TIC-A14).

### 4.4 TIC (tree, metadata, type system): the rest of `rtc/dll/src/tic`

Coverage: TreeItem, TreeItemPath, TreeItemMetaInfo (1-940), TreeItemProps, TreeItemXmlDump, TreeItemSet,
CopyTreeContext, StoredPropDef, UnitClassReg, TicInterface, AttrInterface, SourceDescr, UnitClass,
DedicatedAttrs, Xml/XmlTreeParser and HofTypeUnifier in full; AbstrUnit, AbstrDataItem, Explain,
Metric, XmlTreeOut and HofClosure / HofTypeChecker / HofApplication in part. HofOperSignatureInfer,
most of Unit.cpp, IndexGetterCreator and most of TreeItemDataUsage were not reviewed.

#### TIC-A28 · BUG · CONFIRMED · Medium · S — The error-source search computes the failed function's own result instead of reading its first argument
- **Where**: `rtc/dll/src/tic/TicInterface.cpp:1069-1070` (`DataController_GetErrorSource`), reached from `TreeItem_GetErrorSource` :1227-1232, which the GUI's "go to error source" calls (`DmsMainWindow.cpp:995`).
- For `argNr == 0` of an operator with dynamic argument policies (`for_each_*`, a Subset variant) it runs `dc->CalcCertainResult()`, where `dc` is the FuncDC being diagnosed, not the argument's DC (the line was copied from `MoreDataControllers.cpp:1110`, where `dc` is the argument). Asking for the error source of a failed `for_each` result recalculates the failed result, then passes a container result through `AsDataItem` (a static cast in Release) into `DataReadLock<SharedStr>`: a throw, or an access violation that `catch(...)` does not catch.
- **Fix**: read `args->m_DC->GetOld()` when ready, as `MoreDataControllers.cpp:712-713` does; calculate nothing in a diagnosis path.

#### TIC-A29 · BUG + PERF · CONFIRMED (path) / LIKELY (Debug stop) · Medium · S — The "non-default properties" page calls raw accessors on computed properties (a #1268 sibling)
- **Where**: `rtc/dll/src/tic/Xml/XmlTreeOut.cpp:1058` (`WritePropValueRows`, via `DMS_TreeItem_XML_DumpAllProps`, `DmsDetailPages.cpp:313` with "non default" selected); base `PropDef::HasNonDefaultValue` at `mci/PropDef.h:177-183`. Properties without a raw override: `TreeItemProps.cpp` `NrSubItems` :405 (→ `UpdateMetaInfo`), `CaseDir` :418, `FullSource` / `ReadOnly` / `WriteOnly` / `AllStorageManagers` :471-521 (a full `SourceCalculator` walk), `StorageTileSizeX/Y` :731-759 (opens the dataset), `IsCalculable/IsLoadable/IsStorable` :805-807; `AbstrDataItem.cpp:1118-1149` (`DomainUnit_FullName` / `ValuesUnit_FullName` → `FindUnit`); `AbstrUnit.cpp:1154-1172` (Metric / Projection).
- `HasNonDefaultValue` enters the IndexedString ceiling and forwards to `GetRawValue`, whose default is the cooked `GetValue`. #1268 fixed this only for the properties the configuration dump writes; the property table calls it for every PropDef. In Debug the lock-ceiling check should stop GeoDmsGuiQt (exit 3 since #1265); in Release every expensive property is computed twice per render. `doc/deadlocks.md` P18 does not cover this path.
- **Fix**: give read-only computed properties a raw form that reads nothing, or let `WritePropValueRows` compute the cooked value once and test it for emptiness.

#### TIC-A30 · BUG · CONFIRMED (UB) · Medium (Debug) / Low (Release) · S — The XML reader dereferences `begin()` of an empty vector for a self-closing property element
- **Where**: `rtc/dll/src/tic/Xml/XmlTreeParser.cpp:208, 219, 245` (`&*element.m_EnclText.begin()`); cause `xml/XmlParser.cpp:286-296, 483-491`.
- `ReadText(m_EnclText)` runs only for paired elements; `<Descr/>`, `<CalcRule/>`, `<FunctionSpec/>`, `<DataBlock/>` are legal and reach the callback with an empty vector. The writer never emits empty elements, so the #1261 round trip cannot see this; hand-written or third-party .xml can.
- **Fix**: push a terminating 0 for unpaired elements, or read through a helper that returns `""`.

#### TIC-A31 · BUG · LIKELY · Low · S — `CopyPropsContext::MustCopy` asks the source twice; the second question was meant for the destination
- **Where**: `rtc/dll/src/tic/CopyTreeContext.cpp:214-221`, used at :289 and by `TreeItem::Copy` (`TreeItem.cpp:1927`).
- `return propDef->HasNonDefaultValue(m_Src) == m_DoClearDest;` repeats the first call, so the function reduces to `srcHasValue && m_DoClearDest`: in MergeProps mode (the cache-root merge in `TreeItem_MergeReferredCacheRoot`) no non-stored copyable property is ever copied (KeepData, IsHidden, FreeData / StoreData, DisableStorage, ExplicitSuppliers); in clearing mode a destination that differs from a default source is never reset. Present since 2022.
- **Fix**: decide the intended semantics; most likely `HasNonDefaultValue(m_Dst)` in the second call. Run the battery.

#### TIC-A32 · BUG · CONFIRMED / SMELL · Low-Medium · S — Release guards that the preceding `dms_assert` (= `__assume`) removes
- **Where**: `TicInterface.cpp:831-833` (`dms_assert(si != ti); if (si != ti)`), `UnitClass.cpp:217-225` (`dms_assert(g_UnitClassRegister.Empty()); if (!…Empty()) DropDefault loop`), `TreeItemDataUsage.cpp:367-371` (`dms_assert(!SuspendTrigger::DidSuspend());` before the load-bearing `if (SuspendTrigger::DidSuspend()) goto suspended;`).
- The optimiser may fold the following `if` to a constant; the shutdown fallback that drops default units is dead exactly when it is needed. The third case is vulnerable only under LTCG inlining.
- **Fix**: `assert` / `dbg_assert` (no assume), or drop the assert and keep the check.

#### TIC-A33 · BUG / PARTIAL · LIKELY · Low · S — The cached SizeExpectation / SizeUpperbound calculators are never reset
- **Where**: `TreeItem.cpp:1019-1051`; `DoInvalidate` :2398-2400 resets only `mc_IntegrityChecker` and `mc_CheckGuardians`; PropDefs at `TreeItemProps.cpp:812-813` use `chg_mode::none`; consumer `AbstrUnit::EstimateCount` (`AbstrUnit.cpp:899-911`).
- #1218 added `mc_CheckGuardians` to the reset but not the two size calculators of 3ac27228f, built the same way. After a SetExpr on a supplier, estimates evaluate a stale calculator for the item's life.
- **Fix**: reset both in `DoInvalidate`; `chg_mode::invalidate` on the two PropDefs.

#### TIC-A34 · BUG · LIKELY · Low-Medium · S — The MMD dictionary writes `range` for a categorical unit, losing the categorical flag on reading
- **Where**: `TreeItemXmlDump.cpp:556-559` (hard-coded `"range"`); `UnitClassReg.h:71-76` (`SetValue` assigns `TSF_Categorical` from the tag).
- The ordinary dump selects `cat_range` or `range`; the dictionary always writes `range`, so a stored categorical domain (unique, subset, union results) reads back as non-categorical, which changes `AbstrDataItem::CheckResultItem` (`AbstrDataItem.cpp:529-548`).
- **Fix**: `au->GetTSF(TSF_Categorical) ? "cat_range" : "range"`.

#### TIC-A35 · PARTIAL · CONFIRMED · Low-Medium · S — The function-declaration writer still reads cooked values while writing (#1268 not applied there)
- **Where**: `TreeItemXmlDump.cpp:282-294` (`DMS_WriteFunctionUsings` → `GetNrNamespaceUsages` / `GetNamespaceUsage` → `UsingCache::UpdateUsings`), :214 (`adi->GetAbstrDomainUnit()` resolves), :275 (`m->GetExpr()` runs the parent's `UpdateMetaInfo`); compare `TreeItemProps.cpp:697-720`.
- For a function whose `using` does not resolve, `UpdateUsings` throws at write time (the pre-resolve swallowed it), and `TreeItem_XML_DumpSubItemSafe` replaces the whole function with an "ERROR dumping" comment where an ordinary item is written with its configured using URL.
- **Fix**: the raw reads of `UsingPropDef::GetRawValue`, `GetCurrDomainUnit()`, `GetExprMember()`.

#### TIC-A36 · BUG · LIKELY · Low · S — `UsingCache::UpdateUsings` leaves resolved URLs pending after a failure
- **Where**: `UsingCache.cpp:344-361`.
- URLs are added one by one but `m_UsingUrls` is cleared only after all succeed; when URL k throws, 0..k-1 stay pending, are re-resolved on every call, and `UsingPropDef::GetRawValue` writes them twice.
- **Fix**: erase each URL as it resolves, or commit only when all resolved.

#### TIC-A37 · BUG (latent) · CONFIRMED · Low · S — Defects in the kept C ABI
- `TicInterface.cpp:938-957` `DMS_TreeItem_GetTemplSource`: the recursion's result is discarded, so it returns null, and `DMS_TreeItem_GetTemplSourceItem` (:966-969) dereferences it. `AttrInterface.cpp:129-146, 165-184` `DMS_NumericAttr_GetValuesAsFloat64Array/Int32Array`: `index` never advances, so a range spanning tiles repeats the first tile (the Python binding's own loop, `Bindings.cpp:585-593`, advances it; its comments at :577 and :636 document a null `GetTiledRangeData()` and a read-write clone crash of the Set variants, never fixed at the source). `TicInterface.cpp:408-417, 555-569` return `TokenStr::c_str()` after the registry lock ended; :546 and `SourceDescr.cpp:302` return function-static buffers; :332-342 `DMS_TreeItem_VisitSuppliers` always returns false and ignores `ps`; :768, :917-928 guard C-ABI input with `dms_assert` only; `TreeItemSet.cpp:190-208` two entry points throw not-implemented, :241-242 has no bounds check.
- `tu-reorg-and-export-surface-2026-08.md` §1c keeps every `DMS_*` export for out-of-tree consumers; that only holds if they work.
- **Fix**: the missing `return` and `index +=`; IString handles; `MG_CHECK` input guards; or retire them explicitly as `DedicatedAttrs.cpp:220-262` does.

#### TIC-A38 · SIMPLIFY · CONFIRMED · Low · S — 42 prototypes in `TicInterface.h` have no definition anywhere
- `DMS_UnitClass_GetFirstInstance` / `GetNextInstance` (:55), `DMS_CRuntimeClass_GetName` (:104), `DMS_TreeItem_TotalNrOfItems` (:123), `DMS_TreeItem_Commit` (:130), `DMS_DataItem_SetMemoDirty` (:257), and the whole `DMS_{UInt32,…,Bool}Attr_{Get,Set}Value{,Array}` family (36 names, ~:265-310). Not exports, so the keep-all decision does not cover them. **Fix**: delete.

#### TIC-A39 · SIMPLIFY (latent BUG) · CONFIRMED · Low · S — The dead `StoredPropDef<ItemType, PropBool>` specialisation would corrupt the map if used
- **Where**: `rtc/dll/src/tic/StoredPropDef.h:152-257`, `SetValueImpl` :184-212: `lower_bound(item)` is not compared with `item` (unlike :78), so setting false erases a neighbour's entry. Never instantiated; `NonDefaultBoolPropDef` is used. **Fix**: delete.

#### TIC-A40 · SIMPLIFY / PERF · CONFIRMED · Low · S — Dead helpers and token-creating lookups in TreeItemPath
- `TreeItemPath.cpp:149-167` `TreeItem::GetBestItem` has no caller; :133-183 `GetItem` / `GetBestItem` / `GetCurrItem` use the token-creating `GetTokenID(...)` where `ResolveItemPath` / `FindBestItem` use `GetExistingTokenID`, so a lookup of a missing name (`subitem()`, `clc/OperMisc.cpp:271`) permanently registers it under the exclusive registry lock; `TreeItemSet.cpp:58-63` `storeAllSubItemSuppliers` unused. `TreeItemPath.cpp:328`: `assert(ids.second.first = subItemNames.first + 1);` assigns inside an assert (harmless today; `==` was meant).
- **Fix**: delete the dead functions; `GetExistingTokenID` in the lookups; `==`.

#### TIC-A41 · PERF · CONFIRMED · Low · S — Marking storage-read members rebuilds and re-tokenises a path per member
- **Where**: `TreeItemMetaInfo.cpp:128-167` (`TreeItem_FindRawSubItem`, `TreeItem_MarkStorageReadMembers`): O(members × depth × siblings) with registry traffic, where a lockstep walk over both trees by `GetNameID()` does.

#### TIC-A42 · BUG · LIKELY · Low · S — Null-unit guards added to one twin only
- `DedicatedAttrs.cpp:184-195` (`DMS_DomainUnit_VisitPaletteCandidates`: `GetAbstrDomainUnit()->UnifyDomain` unguarded) vs :113-119; `AbstrUnit.cpp:607` (`GetLabelAttr`) and :699 (`GetMissingValueLabel`) vs `GetCurrLabelAttr` :627-628. The null arises for in-template items with a generic domain. **Fix**: the same null checks.

#### TIC-A43 · SIMPLIFY · CONFIRMED · Low · S — Leftovers: a debug report in a destructor, user-visible typos, unguarded lazy default unit, SourceCalculator waste
- `Explain.cpp:81-84` (`~AbstrCalcExplanation() { reportD(ST_MinorTrace, "Byte"); }`, since 2022: a message per destroyed explanation); typos "IntegryCheck" (`TreeItemMetaInfo.cpp:657, 666`), "UnitClass for found for" (`UnitClass.cpp:184`), "occured" (`SourceDescr.cpp:307`); `UnitClass.cpp:100-121` double-checked lazy creation with its mutex commented out, while the MMD dump on the commit thread (#1155) reads `m_DefaultUnit` via `IsDefaultUnit` (`AbstrUnit.cpp:425-432`): a data race (use `std::call_once` or an atomic `shared_ptr`); `SourceDescr.cpp:63-68, 158-159, 275-276` (`Assign0` appends a new undefined sequence per hidden / template / failed actor; an `empty()` test that can never be true).

**Tracked-item status (TIC tree)**

- **doc/issues.md #1165 row** is stale: since d5dc30520 the bit is `IsAlias()`, consulted at `XmlTreeOut.cpp:1056`, and that commit abandoned the property-level warn path, so item (a) is decided; only (b), migrating the brace-form configurations, remains.
- **incremental-updates.md #20** (SetDC exception safety): open at `TreeItem.cpp:881-887`. **#22** (the `std::async` baton in `UpdateMetaInfo`): open, now at `TreeItemMetaInfo.cpp:938-958` and :960-978; thread-local state (token-registry usage counters, `SuspendTrigger`, the lock-level stack) does not follow the baton.
- **deadlocks.md P18 "FIXED"**: accurate for the dump; the same class remains on the properties page (TIC-A29).
- **tu-reorg §1c** (C-ABI keep-all): 129 of 165 names in `TicInterface.h` have no in-repo caller; covered by the decision except TIC-A37 and TIC-A38.

### 4.5 CLC and STX: `clc/` (operators) and `stx/` (parser)

The recent run of clc fixes (Classify*, modus / unique / ValuesTable binning, invert, diversity,
discrete_alloc) is correct at HEAD. What remains is mostly the same kind of sibling inconsistency: a
null, overflow or domain check that one member of an operator family has and its twin lacks.

#### CLC-A01 · BUG · CONFIRMED · High · S — The total forms of `cov`, `corr` and `modus_weighted` never unify their argument domains
- **Where**: `clc/dll/src/OperAccBin.h:73-107` (`AbstrOperAccTotBin::CreateResult`: no `UnifyDomain`; the comment at :43-45 calls it deliberate), :142-159 (loop :156) → `clc/dll/include/AggrFunc.h:28-32` (`aggr2_total` walks arg1's range only); `clc/dll/src/Modus.cpp:408-431, 435-469` (`WeightedModusTot*`, weights read per tile, `ValueGetter::Get` guarded by `dms_assert` only).
- arg2 is read at arg1's tile and index, so a smaller or differently tiled arg2 is read out of bounds (`HeapSingleArray::GetTile(t>0)` only asserts, `HeapTileArray` indexes `m_Seqs[t]`). Trigger: `cov(A/x, B/y)` with #A = 10, #B = 5. The partitioned twin (:239-240) does unify; `operator-signature-interface.md` §12.3 records this only as a signature-honesty point.
- **Fix**: `e1->UnifyDomain(e2, "e1", "e2", UM_Throw)` in `CreateResult` (allowing a void right side only if a parameter weight is meant), and describe a shared `D` in `DescribeSignature`.

#### CLC-A02 · BUG · CONFIRMED · High · S — `corr` includes the squares of rows with a null in either argument
- **Where**: `clc/dll/src/AggrBinStructNum.h:188-194` (`binary_assign_corr::operator()`) vs the guard in `binary_assign_cov` (:123). DMS: `corr`, total and partitioned.
- `m_CovAssign` skips a row with an undefined x or y, but `a.xx += x*x` and `a.yy += y*y` run unconditionally: float data with one null gives a null `corr` where `cov` is fine; int32 data adds 2^62 per null to `xx` (null = INT_MIN), so the result is silently near 0. Trigger: `corr(x, y)` with x = [1,2,3,null], y = [2,4,6,8] (expected 1).
- **Fix**: return early when `!IsDefined(x) || !IsDefined(y)` before all three accumulations.

#### CLC-A03 · BUG · CONFIRMED · Medium · S — `modus_weighted` without a partition lets a null weight poison its value
- **Where**: `Modus.cpp:421` (`WeightedModusTotBySet`), :458 (`WeightedModusTotByTable`) vs the partitioned twins :514-515, :570, which test `IsDefined(weight)`; `arg_max` :51-64.
- A NaN total wins `arg_max` (`c <= maxC` is false for NaN), after which every later value beats it: values [1,1,2], weights [5,null,1] give 2 in the total form and 1 in the partitioned form.
- **Fix**: skip undefined weights in both total variants; optionally make `arg_max` ignore NaN.

#### CLC-A04 · BUG · CONFIRMED · Medium · S — `cumulate` accumulates in a wide type and silently narrows each partial sum
- **Where**: `clc/dll/src/Cumulate.cpp:103, 121, 127` (`CumulateTot`), :258, 272 (`CumulatePart`); accumulator `unary_assign_add<acc_type_t<T>, T>` at :323.
- `*resPtr = value` stores a SizeT / DiffT / Float64 into T: `cumulate` on uint8 [200, 100] gives [200, 44]; on int32 a sum past 2^31 wraps; on uint32 a partial sum of 0xFFFFFFFF becomes null. `sum` accumulates in T with `SafeAccumulate` and throws (`AggrUniStructNum.h:214`).
- **Fix**: store through `ThrowingConvert<ValueType>`, or accumulate as `sum` does.

#### CLC-A05 · BUG · CONFIRMED · Medium · S — `modus_count_uintN` silently truncates the modal count
- **Where**: `Modus.cpp:80-91` (`return countF(p);` converts SizeT to `Counter` implicitly).
- 300 equal values give 44 for `modus_count_uint8`; the twin `uniqueCountFunc` (:94-106), `pcount_uint8` and `count_uint8` throw on overflow.
- **Fix**: `return ThrowingConvert<Counter>(countF(p));`.

#### CLC-A06 · BUG · CONFIRMED · Medium · S — `ordinal` throws an internal error for 8-, 16-, 64-bit and ipoint / upoint arguments
- **Where**: `clc/dll/src/Index.cpp:222` (`result_type = cardinality_type<V>::type`), :227 (registered result `DataArray<UInt32>`), :238, :260 (`mutable_array_cast<result_type>`), registration :360; also reached from the GUI via `shv/dll/src/IndexCollector.cpp:78-79` for any non-numeric or non-zero-based entity unit.
- The data object is always `DataArray<UInt32>`, but `cardinality_type` is UInt8 / UInt16 / UInt64 for other widths (`ElemTraits.h:300-302`), so `debug_cast`'s `MG_CHECK` fails: `ordinal(x)` on a uint8 attribute is a logic error.
- **Fix**: write through `UInt32` (checked for the 64-bit and ipoint cases), or choose the result values class from the cardinality type.

#### CLC-A07 · BUG · CONFIRMED · Medium · S — `max_index` / `min_index` return null when the extreme equals the sentinel start value
- **Where**: `clc/dll/src/OperAccMinMax.cpp:98`, :115 (strict comparison), :136-137.
- For unsigned and bool data the start value is 0 and `0 < 0` is false, so `max_index` of an all-zero attribute (or partition) stays undefined; `min_index` likewise when all values equal `MAX_VALUE(T)`. `arg1HasUndefined` (computed by a possible full scan at :67) is unused.
- **Fix**: a per-partition "seen" flag, or accept the first defined value unconditionally (as `unary_assign_minmax_ifdefined`); drop `arg1HasUndefined`.

#### CLC-A08 · BUG · CONFIRMED · Medium · S — `neg` on unsigned values wraps
- **Where**: `clc/dll/include/AttrUniStructNum.h:65` (`-signed_type_t<T>(x)`), registered for `num_objects` at `OperAttrUni.cpp:122`.
- `neg(uint8(200))` gives 56; `neg(uint32(3e9))` gives 1294967296. `add`, `sub`, `mul` throw on overflow.
- **Fix**: throw or return null when `x > MAX_VALUE(signed_type_t<T>)`, or widen the result for unsigned arguments.

#### CLC-A09 · BUG · CONFIRMED · Medium · S — `regex_search` and `regex_match` ignore their flags argument
- **Where**: `clc/dll/src/Regex.cpp:64-69`, :147-151: `args.size() >= 4` and `args[3]`, copied from the quaternary `regex_replace`; these take (str, regex, [flags]).
- **Fix**: `args.size() >= 3` and `args[2]`.

#### CLC-A10 · PARTIAL · CONFIRMED · Medium · S — `asExprList_with_null` and `asItemList_with_null` are identical to the plain forms
- **Where**: registered with `valueMustBeDefined=false` at `clc/dll/src/OperAccUniStr.cpp:259-282`; the flag (`OperAccUni.h:127, 303`) is never read by `OperAccTotUniStr::Calculate` (`OperAccUniStr.cpp:63-77`) or `CalcOperAccPartUniSer` (`OperAccUni.h:527-566`).
- The wiki (`String-functions.md:37, 39`) says the `_with_null` forms include nulls and the plain ones exclude them; in fact `asExprList` always includes them and `asItemList` always drops them (and empty strings). This is the one part of the #848 null-option rollout (4be0905b4) that was never wired up.
- **Fix**: pass the flag to the serialiser functors, or unregister the `_with_null` names.

#### CLC-A11 · BUG · CONFIRMED · Medium · S — String conversions depend on the thousand-separator setting; `string(point)` is registered twice
- **Where**: `rtc/dll/src/vt/Conversions.h:330-334` (`Convert4(…, SharedStr*)` calls `AsString(val)` with the default `FormattingFlags::ThousandSeparator`, `ser/AsString.h:26`; stripped only when the option is off, `FormattedStream.cpp:33-35`), reached from `clc/dll/src/OperConv.h:136-142, 719-729`; the duplicate: `OperAttrUni_str.cpp:51` (`asstring_assign<points>`) and `OperConvPoint.cpp:23`, both in the `string` group.
- Computed data changes with a GUI option (and with GeoDmsRun `/SH` vs `/CH`): `string(dpoint(463000,155000.5))` gives `xy(155,000.5; 463,000)`, which does not parse back and changes keys and storage names. Which point → string member wins depends on static-init order; the cast one lacks `AF1_HASUNDEFINED`, so a null point becomes `xy(null; null)`, and the builds can disagree.
- **Fix**: `FormattingFlags::None` in `Convert4`; keep only the null-aware `asstring_assign`.

#### CLC-A12 · BUG · CONFIRMED · Medium · S — `interpolate_linear` is wrong for signed integer y and for null x
- **Where**: `clc/dll/src/OperLinInterpol.cpp:116-120` (SizeT intermediate when both types are integral), :66-85, :101.
- xs = [0,10], ys = [-10,-20] (int32) at x = 5: the negative numerator wraps as unsigned and gives null instead of -15. A null x at the front of the chart breaks the `lower_bound` order.
- **Fix**: an Int64 or Float64 intermediate; drop undefined x values from the chart.

#### STX-A13 · BUG · CONFIRMED · Medium · S — `container`, `template` and `entity` match as keyword prefixes
- **Where**: `stx/dll/src/ConfigParse.cpp:356, 364, 368` (`itemSignature`); only ITEM (:361) has a word-boundary guard.
- `Templates: container {…}` parses as `template` + name `s`, `ContainerTerminals: container;` as `container` + `Terminals`: an item with the wrong name, then "property definition expected". The same for `TemplateSettings`, `entityTypes: unit<uint8>`, parameters named `templateX`.
- **Fix**: the `lexeme_d[… >> epsilon_p(anychar_p - itemNameNextChar)]` guard on all three; the ITEM guard should also exclude `@` and bytes ≥ 128. No configuration in the repo or in `tst` uses such a name yet; the old `container Templates {}` style hides the problem.

#### STX-A14 · BUG · CONFIRMED · Medium · S — Float literals in `.dms` files are not correctly rounded
- **Where**: `stx/dll/src/DataBlockParse.h:170`, `stx/dll/src/ExprParse.h:368-369` (Spirit classic `strict_ureal_p`, which computes `n += frac*pow(10,-len)`).
- `0.3` becomes 0.30000000000000004 and `0.7` becomes 0.7000000000000001, while every other text-to-number path uses `std::from_chars`: `float64('0.7') == 0.7` is false, and `[0.7, '0.7']` in one data block stores two different values.
- **Fix**: use Spirit only for the extent and convert the matched text with `std::from_chars`.

#### STX-A15 · BUG · CONFIRMED · Medium · S — `nrofrows` silently narrows to the unit's value type
- **Where**: `stx/dll/src/ConfigProd.cpp:564-579` → `rtc/dll/src/tic/Unit.cpp:947-958` (`SetRangeAsUInt64` with the non-throwing `Convert<V>`).
- `unit<uint8> u: nrofrows = 300;` gives 255 rows; `unit<uint32>` with 5e9 gives a 4-billion-row domain; `unit<int8>` with 200 gives [-128,0); no error. The Appendix-A refutation of `ConfigProd.cpp:566` covers only the value-type assert.
- **Fix**: `ThrowingConvert` in `SetRangeAsUInt64`, or a semantic error when the count exceeds the type.

#### CLC-A16 · BUG · CONFIRMED · Medium · S — `parse_xml` fills a missing numeric attribute with null or 0 depending on position
- **Where**: `clc/dll/src/BoostXML.cpp:132-146`, :267-279 (writes `m_Data.size()` values into a `write_only_mustzero` lock).
- `<b x="1"/><b/>` gives [1, 0], `<b/><b x="1"/>` gives [null, 1]; an attribute that never occurs gives all 0.
- **Fix**: pad with undefined up to the entity count before storing.

#### CLC-A17 · PERF / PARTIAL · CONFIRMED · Medium · M — `overlay` uses a dense table over the product of all region counts; `overlay64` cannot exceed uint32
- **Where**: `clc/dll/src/Overlay.cpp:50` (`ProdID = UInt32`), :108 (`MG_CHECK` product ≤ 2^32-1), :214, :352, :439 (`OverlayOperator<UInt64>`).
- Memory is 4 × ∏(nrRegions) bytes whatever the cell count: three partitionings of 1500 regions allocate 13.5 GB, and 2000³ fails the check even when only a few thousand combinations occur. With the product capped at 2^32-1, `overlay64` can never do more than `overlay32`.
- **Fix**: when the product exceeds a multiple of the cell count, collect observed products by sort + unique or a hash map (the table-vs-set trade of b932df281); then widen ProdID or drop `overlay64`.

#### CLC-A18 · PERF · CONFIRMED · Medium · M — `convert` to a projected unit builds the transformation per tile under a global lock
- **Where**: `clc/dll/src/OperConv.h:678-695` → `do_convert` (:582-588) → `Type2DConversion` (:302-345, lock :313).
- Per tile: two `SetFromUserInput` calls plus `OGRCreateCoordinateTransformation`, about 1.1 ms, serialised on `cs_SpatialRefBlockCreation`. The `mapping` twin was fixed in #298 with a `MappingState` built once (`SeparableMapping.h`).
- **Fix**: a shared state built in `CreateResult` (validated SRS pair plus per-thread transformers), as `mapping` does.

#### CLC-A19 · PERF · CONFIRMED · Low · S — `OperAccPartUniDirect` holds every partition tile until the aggregation ends
- **Where**: `clc/dll/include/OperAccUni.h:485` vs the Buffered twin at :408 (`pdi.part_fta[t] = nullptr;`). DMS: partitioned `min`, `max`, `first`, `last`, `any`, `all` and their `_ifdefined` forms. **Fix**: reset `pdi.part_fta[t]` after creating the `IndexGetter`.

#### CLC-A20 · PERF · CONFIRMED · Low · S — Total `asExprList` / `asItemList` grow the result string once per tile
- **Where**: `clc/dll/include/AggrUniStructString.h:46-61`, called per tile from `OperAccUniStr.cpp:74-76`: quadratic in the number of tiles. `OperAsListTot` (:136-151) measures all tiles first. **Fix**: follow `OperAsListTot`.

#### CLC-A21 · PERF · CONFIRMED · Low · S — `cumulate(a, rel)` rereads every data tile per partition tile
- **Where**: `Cumulate.cpp:224-277`: O(#partition tiles × n) with a lock per data tile each time. **Fix**: one pass over the data with a checked index into the whole partition range.

#### CLC-A22 · BUG · CONFIRMED · Low · S — `mod` on floats casts the quotient to Int32 / Int64
- **Where**: `clc/dll/include/AttrBinStruct.h:501-517` (`qint_t`, `mod_func_impl`).
- `mod(1e10f, 3f)`: the quotient does not fit Int32, the cast gives INT_MIN on x64 (UB), and the result is 1e10 + 6.4e9; Float64 fails above 9.2e18; precision is lost well before. **Fix**: `std::fmod`.

#### CLC-A23 · BUG · LIKELY · Low · S — `unique_with_null` of a sorted float attribute returns one null row per null
- **Where**: `clc/dll/src/Unique.cpp:301-318` (the sorted path compares with `==`; `NaN == NaN` is false). The unsorted path uses `areEqual`. **Fix**: `DataEqualityCompare<V>` in that loop.

#### CLC-A24 · BUG · CONFIRMED · Low · S — Seeded `rnd_uniform(seed, seedAttr, V)` skips its twin's range validation
- **Where**: `clc/dll/src/Random.cpp:448-460` vs :375-377: an empty integral range wraps to the whole type, the default float range yields ±1.8e308, and a `mt19937` is re-seeded from a `seed_seq` per element. **Fix**: share the validation; a cheaper per-element engine.

#### STX-A25 · BUG · CONFIRMED · Low · S — Literal and element narrowing is checked on some parse paths and silent on others
- `stx/dll/src/DataBlockProd.cpp:82-84` → `DataArray.ipp:596-602` (`SetValueAsDPoint` uses `Convert`: `[(40000,1)]` into spoint gives a null coordinate while `[40000]` into int16 throws); `ExprProd.cpp:240-277` (`ProdSuffix` unchecked: `5000000000u`, `300b` become null) vs :167-181 (checked); `ConfigProd_functions.cpp:436` with `ConfigProd.cpp:536`, `DataBlockProd.cpp:153` (`a, b: attribute<int32>(d): [1,2,3];` gives the data block, IntegrityCheck and `nrofrows` to `b` only).
- A string element into a numeric attribute is silent too (`attribute<uint8> x (d): ['300']` gives null), and a negative integer below −2^63 becomes null even in a float64 block (`AbstrDataBlockProd.cpp:85-90`). The `no_tile` error at `DataBlockProd.cpp:70` reports `m_nIndexValue` (i+1); narrowing errors name C++ types ("unsigned char") without the element index. Plain numbers in data blocks are range-checked (`ThrowingConvert` via `FixedRangeConverter::GetValue`, `TiledRangeData.h:439`), with the parse position.
- **Fix**: range-check in `DoArrayAssignment`, `SetValueAsDPoint`, `ProdSuffix`, with the DMS type name and element index; apply every property to all names of a multi-name declaration, or reject it.

#### STX-A26 · BUG · CONFIRMED · Low · S — Expression-parser edge cases
- `stx/dll/src/ExprParse.h:265-267, 227-231`: in `1.5EUR` the `E` is read as an exponent and the configuration does not load (`1.5 EUR` works). :150-164 with `ExprParse.cpp:79`: with an item named `apply`, `apply and b` pushes an identifier on an alternative Spirit abandons, failing an `MG_CHECK` at calculation time.
- **Fix**: fall back to a real without exponent when exponent digits are missing; guard the keyword forms with a pure lookahead.

#### CLC-A27 · BUG · LIKELY · Low · S — Operators shaped for a parameter or `weeded_counts` input do not check that shape
- `clc/dll/src/OperMappings.cpp:190-244` (ternary `Classify*(values, count, C)`: no check that values are sorted and defined and counts positive: NaN breaks without error); `OperPropValue.cpp:184-252` (`PropValue` with more than one name: `ri` never advances and `Commit` runs inside the loop); `OperAccUniStr.cpp:126-134` (`asList` separator) and `OperExec.cpp:340-353` (`expand`) read element 0 under a CRT `assert` only.
- **Fix**: an `MG_USERCHECK` pass or `checked_domain<Void>`; for `PropValue`, write `resData[p]` and move the lock out of the loop.

#### CLC-A28 · SIMPLIFY · CONFIRMED · Low · S — Dead code in clc and stx (whole-repo grep)
- clc: `OperRelUni.h:99-122, 342-349` (`IndexPCompareOper`, `make_indexP`); `PartitionTypes.h:90` (`both_checker_t`); `CastedUnaryAttrOper.h:355-421` (`CastedUnaryAttrSpecialFuncOperator`); `OperConv.h:181-188, 280-291, 355-396` (the `std::true_type` `DispatchMapping` overloads, `ApplyProjection`, `ApplyScaledProjection`); `ClcInterface.h:15, 25-26` (three typedefs).
- stx: `DoUnitRangeProp` (`ConfigProd_functions.cpp:111-149`); `DoFirst/SecondIntervalValue` and members (`AbstrDataBlockProd.cpp:109-195`, `DataBlockProd.h:28-29, 54-57`); `DataBlockTask::m_NrElems` (`CalcFactory.cpp:62-66` admits nothing reads it); `m_TileLock`, `SafeSetValue`, `nrLineBreaks`, `string_grammar`, `exprLW`, `ParseExprFunctor` / `g_Cache` (`ExprParse.cpp:132-153`, never consulted); `DMS_ProcessPostData` (`StxInterface.cpp:357-424`, reachable only through the always-false `IsPostRequest`; if revived, it looks up the URL-encoded `name` instead of `decodedName` (:361-362) and asserts a NUL the caller's `QByteArray::size()` excludes (:389)); the duplicate `DMS_CreateTreeFromString` declaration (`StxInterface.h:22-23`, marked "TODO: duplicate, remove"); `itennameNextChar_parser` (`ExprParse.h:28-37`, its CRTP base names the wrong class); the padding loop after the throw in `DataBlockProd::Commit` (:132-133); the lines after the unconditional throw in `OnFunctionResultIsFunction` (`ConfigProd_functions.cpp:746-750`); `AuthErrorDisplayLock` / `s_AuthErrorDisplayLockCatchCount` (vestigial since STX-19). `parseExpr` rebuilds about 45 Spirit rules and 2 token lookups per call (`ExprParse.cpp:43-44`): one meta-thread `expr_grammar` would do.

#### CLC-A30 · PERF · CONFIRMED · Low-Medium · S-M — Per-element copies and double work in the text and XML operators
- `clc/dll/src/BoostXML.cpp:256-265` (`StoreValues<SharedStr>`: `Convert<SharedStr>(v)` per value, then a second copy into the result), :267-279 (`AssignFromCharPtrs` plus a virtual `SetAbstrValue(i)` per attribute value; the captured values are about 0.6 × the input bytes for the BAG `pand` schema). `Regex.cpp:83-103`, :255-285: `regex_search` and `regex_replace` run the regex twice per element (size pass, write pass). `AttrUniStructStr.h:179-248`: `UrlDecode`, `UrlEncode`, `HtmlEncode/Decode`, `to_utf`, `from_utf`, `AsItemName` build two `SharedStr`s per element without `data_reserve`. `ReadData.cpp:52-73, 238-270`: `ReadElems` calls `InviteUnitProcessor` and `GetDataWrite(t, read_write)` per element, `ReadArray` uses a `read_write` shadow on a fresh result (:159 "NYI"), and the split operators dispatch `visit<domain_elements>` plus `SetIndexedValue` per output element (:442-451).
- **Fix**: assign strings straight from the source reference; one regex pass recording offsets; write the string functors into the result `StringRef`; resolve tile span and value type once per tile.

#### CLC-A29 · Various · Low · S — One-line items
- `ReadLines(str)` splits CRLF into an extra empty line while `ReadLines(str, D, pos)` collapses runs of line breaks (`ReadData.cpp:287-290` vs :356-369). Float → uint32/uint64 lets exactly 2^32 / 2^64 through `check_max_func` (`Conversions.h:93-100`; `uint32(4294967296f)` gives 0; see RTC-A02). `ramp` loops with `UInt32 i` against a SizeT `n` (`OperMisc.cpp:651`): an endless loop and out-of-bounds write beyond 4G elements. `DataArrayOperator` holds the Spirit mutex across `Commit()` and while data locks are held (`DataBlockTask.cpp:74-94`, contrary to `SpiritTools.h:36-40`), so a large data block parsing on a worker blocks every GUI `parseExpr`; scope the lock to `parse()` and `CheckInfo`. Every calculation-time evaluation of a string data block re-issues the escape-code warnings of the load, without location, on a worker, and reads the meta-thread static `s_LastFileNameLock` (`SpiritTools.cpp:184-204`); give `StringProd` a "don't warn" flag for data blocks. `Type0DConversion` calls the resolving `GetMetric` / `GetProjection` from worker threads (`OperConv.h:127-134`), the 08b089d02 pattern; `Type1DConversion` uses the `Curr` variants.

**Tracked-item status (CLC, STX)**

- Fixed as the docs say: CLC-25, CLC-N01 / N02 / N03, CLC-26, the commented block in `Overlay.cpp`, cleanup-list item 12's loop branch (the wiki `Loop` question stays open).
- Partly open: **CLC-28** (`ReadLines<UInt32>`, `ReadData.cpp:374`, still uses a generic `ThrowingConvert` error; the shared message says "ReadElems:" for ReadArray too); **STX-16** (`ConfigProd_functions.cpp:249` still a `dms_assert`); the 1d assert list (`ExprProd.cpp` now :170 / :244, still `dms_assert`); **STX-N01** fixed at the four listed sites, but `StringProd::ProdStringLiteral1/2` (`SpiritTools.cpp:209, 218`) reads `*last`, one byte past a mapped `.dms` that ends inside an unterminated string; **STX-19** fixed, its lock and two atomics now vestigial, and the C2 item "`ConfigParse.cpp:485-487, 561-563` unreachable if" is stale; **STX-21** fixed at `SpiritTools.cpp:107`, but `bolpos` (:106) is unclamped and returns UInt32.
- Stale line numbers: C1 `DataBlockProd.cpp:35` is now :77; the BoostXML markers are at :286, :294.
- `operator-signature-interface.md` §12.3 treats `AbstrOperAccTotBin`'s missing unification as honesty only; it is the root of CLC-A01.

### 4.6 GEO: `geo/`

The geo slice has the most high-severity findings of this audit. Four of them (GEO-A01, A04, A07-A09)
are variants of patterns that were fixed elsewhere in the last weeks: a counter that wraps in a narrow
type, a relation that is not range checked, a twin that was not fixed along with its sibling.

**Critical / High**

#### GEO-A01 · BUG · CONFIRMED · High · S — `join_equal_values_uint8/_uint16` sparse path: counters wrap, the fill writes past the buffer
- **Where**: `geo/dll/src/JoinEqualValues.cpp:123-132` (`sparse_x_index::count`: `++counts[slot]`), :161 (`aCounts` / `aUsed` are `my_vec_t<ResultElement>`), fills at :202-208, :230-240, :258-266.
- The dense path counts with `pcount_best` (`SafeIncrement`, throws on overflow); the #1175 sparse path, taken for any X that is not zero based or has a large range (a plain uint32 key), uses a bare `++`. A key occurring 300× in A and once in B makes `aCount` wrap to 44, `nr_AB = 44`, and `resIndex = abOffsets[x] + aUsed[x]++ * b_count` writes up to index 255 into a 44-element result: heap corruption.
- **Fix**: `SafeIncrement` in `sparse_x_index::count`, or count in `SizeT` and `ThrowingConvert`; keep `aUsed` / `bUsed` in `SizeT`.

#### GEO-A02 · BUG · CONFIRMED · High · S — `canyon` keeps iterators into destroyed temporary read locks
- **Where**: `geo/dll/src/Canyon.cpp:215-219` (`segmentIdPtr = arg3->GetDataRead().begin();`, same for arg4, arg5).
- `GetDataRead(no_tile)` on a multi-tile or lazily computed array returns a shadow tile that the data object holds only weakly (`DataArray.cpp:201-217`, :321-333); the `locked_cseq_t` temporary dies at the end of the declaration, so the loop reads freed memory. `hoogtePtr` (:213) survives only because `arg2Data` holds the same tile.
- **Fix**: named holders (`auto arg3Data = arg3->GetDataRead();` …) for the whole loop, as for arg1 / arg2.

#### GEO-A03 · BUG · CONFIRMED · High · S — `canyon` indexes segment arrays with the raw, unchecked segment id
- **Where**: `Canyon.cpp:232-234`.
- A null segment relation (0xFFFFFFFF, from an unmatched `rlookup`) or an id ≥ count reads far out of bounds; a segment unit whose range does not start at 0 is off by `range.first` (the eddf106f2 pattern). Null heights flow into `hoogteDiff`, and `complexmul` (:30-34) multiplies in the coordinate type, overflowing for SPoint / IPoint.
- **Fix**: `Range_GetIndex_checked(segmentRange, id)` and skip or raise on undefined; skip null heights; compute `complexmul` in `acc_type`.

#### GEO-A04 · BUG · CONFIRMED · High · S — `strongly_connected_components` never range-checks the to-node
- **Where**: `geo/dll/src/ConnectedParts.cpp:255-256`, :265, :310-311 (`stronglyConnectedComponentsIterativeWithInvertedLinks`).
- `InvertIntoLinkedList` skips out-of-range from-nodes, but `NodeType w = node2Data[currentLink];` then feeds `indices[w]`, `pushNode(w)` (a write) and `resSubData[ww]` unchecked: a null `to_node_rel`, which the wiki allows, is an out-of-bounds heap write in Release. The sibling `connected_parts` guards with `if (otherNode < nrV)` (:177, :185).
- **Fix**: skip links with `node2Data[e] >= nrV` when building the link lists, or test `w < nrV` at both reads.

#### GEO-A05 · BUG · CONFIRMED (multi-tile) · High · S — `bp_` split/union with a numeric attribute read it at tile `no_tile`
- **Where**: `geo/dll/src/BoostPolygon.cpp:954` (delayed path calls `Store(..., no_tile, 1, ...)`), :991 (`ReadableTileLock(..., HasVoidDomainGuarantee ? 0 : t)`), :1142 (`GetTile(argNum->HasVoidDomainGuarantee() ? 0 : t)`). Operators: `bp_split_polygon_{filtered,inflated,deflated}`, `bp_split_union_polygon_*`, partitioned `bp_union_polygon_*` and the deprecated kernel suffixes.
- `DoDelayStore()` holds for every split / union, so `bp_split_polygon_inflated(g, perElementSize)` on a tiled domain reaches `HeapTileArray::GetTile(0xFFFFFFFF)` → `m_Seqs[t]` behind a plain `assert`. It only works for a materialised single-tile array.
- **Fix**: on the delayed path read the numeric argument with `GetDataRead(no_tile)` (shadow tile).

#### GEO-A06 · BUG · CONFIRMED · High · M — The `dms_` single-ring shortcut (2ccb28116) writes self-crossing rings unrepaired
- **Where**: `geo/dll/src/DMS_Traits.h:1553-1591` (`DmsOverlayEngine::SingleRingToRings`), used by `CleanSingle` and the bag phase (`BoostPolygon.cpp:1860, 1901, 1934`). Operators: `dms_polygon`, `dms_split_polygon`, and one-element slots of `dms_union_polygon` / `dms_split_union_polygon`.
- "Single ring" is decided by no repeated vertex, ≥ 3 points and nonzero signed area; crossing edges that share no vertex are never tested. The asymmetric bow tie (0,0),(10,10),(10,0),(0,20) has nonzero area and is written as one self-crossing shell, while the wiki promises valid output and a bow tie split into two triangles. The battery misses it because its bow ties are point-symmetric (area 0 → sweep): `oper_dms_family.dms:46`, `oper_dms_crossings.dms:69`.
- **Fix**: prove simplicity first (pairwise proper-crossing test for small n, else the noder's `CrossingSweep` with zero crossings required); add an asymmetric bow tie to `oper_dms_single_ring.dms`.

#### GEO-A07 · BUG · CONFIRMED · High · S — `impedance_*` `Link_flow` is accumulated twice when a start point is not a tree root
- **Where**: `geo/dll/src/Dijkstra.cpp:1098-1141` (`AccumulateInteraction`); compare the guard in `UpdateALW` at :746.
- Both walks start at every start point of the origin zone and run `WalkDepthFirst_BottomUp` to a parentless node; a start node reached more cheaply through another start point has a parent, so its walk runs on through the other root's post-order and adds `pot_ij`, `nodeALW` and `resLinkFlow` again. Two start points on one node double everything. No battery case uses `Link_flow`.
- **Fix**: skip start nodes that have a parent and deduplicate start nodes per origin, as `UpdateALW` does.

#### GEO-A08 · BUG · CONFIRMED · High · S — `Link_flow` routes a zone's demand to every reached end point of that zone
- **Where**: `Dijkstra.cpp:477-498` (`NodeZoneConnector::Y2Res`), used at :1122-1131.
- `Y2Res(y)` does not check that `y` is the end point that claimed the zone. Dense: any reached end point returns `dstZone`, so with `endPoint(Node_rel,DstZone_rel)` flows are multiplied by the connectors per zone. Sparse: `m_FoundResPerY[y]` is written only for the claimant, so other end points read a stale slot of an earlier origin.
- **Fix**: return undefined unless `m_FoundYPerDstZone[dstZone] == y`.

#### GEO-A09 · BUG · CONFIRMED · High · S — End-point impedance indexed by destination zone (the GEO-32 consumer that was not fixed)
- **Where**: `Dijkstra.cpp:958-959` (`impedance += ni.endPoints.Impedances[dstZone];`); the attribute's domain is the end points (:2252).
- With `endPoint(Node_rel,impedance,DstZone_rel)` plus `interaction(...)` or `:max_imp`, every potential, D_i, M_ix, C_j, M_xj, Link_flow and OrgZone_MaxImp uses another end point's offset, and reads out of bounds when there are more zones than end points. GEO-32 fixed `Res2DstZone` but not this site, which its own entry lists.
- **Fix**: `ni.endPoints.Impedances[nzc.DstZone2EndPoint(dstZone)]`, or the committed `m_ResImpPerDstZone[dstZone]`.

#### GEO-A10 · BUG · CONFIRMED · High · S — `geos_buffer_linestring`, `geos_simplify_linestring` and `geos_buffer(arc)` dereference null on an empty arc
- **Where**: `geo/dll/src/BoostGeometryImpl.h:1605-1612`, :2102-2107, :2395-2401; the null comes from `GEOS_Traits.h:106-107`.
- An empty or all-null arc (routine: null shape records, subset results) makes `lineString->buffer(...)` or `DouglasPeuckerSimplifier(lineString.get())` dereference null. The polygon twin at :2241 guards with `if (mp && !mp->isEmpty())`.
- **Fix**: `if (!lineString) { resData[i].clear(); continue; }` at all three sites.

#### GEO-A11 · BUG · CONFIRMED · High · S — `points2sequence` / `points2polygon` / `points2arc` with a sequence relation ignore the range offset when filling
- **Where**: `geo/dll/src/OperPolygon.cpp:718-719` (count pass subtracts `polyIndexRange.first`) vs :773-782 (fill pass uses the raw value), `Points2SequenceOperator::Calculate`.
- With a sequence unit on e.g. `range(uint32, 1, 11)` points are counted under v−1 and written to v. Sequences were sized with `resize_uninitialized` (:745), so the result throws a misleading "unexpected orderNr" or silently holds uninitialised points. Duplicate ordinals in the `_o` forms also leave slots unwritten without detection.
- **Fix**: subtract `polyIndexRange.first` in the fill pass; track written slots and throw on a duplicate or a gap.

**Medium**

#### GEO-A12 · BUG · CONFIRMED · Medium-High · S — `join_near_values` misses pairs at exactly `maxDist` and truncates the distance for integer points
- **Where**: `geo/dll/src/JoinNearValues.cpp:102`, :125.
- The index tests a point leaf half-open (`Range.h:154`), so a point at exactly `ap + d` on an axis is never visited although `SqrDist <= sqrDist` would accept it (A=(0,0), B=(10,0), d=10 finds nothing; swapped it does). For IPoint, d = 1.5 becomes ±1 and misses (a+1, a+1). #1228 fixed this for `connect` with `InflatedSearchBox`; this is the unfixed twin.
- **Fix**: `InflatedSearchBox(Range(ap, ap), dist)` and keep the exact `<=` filter.

#### GEO-A13 · BUG · LIKELY · Medium · S — `fixWindingOrders` deletes the polygon whose outer ring it has just reversed
- **Where**: `geo/dll/src/BoostGeometry.h:587-617`; used by `bg_checked_operation` (:654, :658: `bg_intersect/union/xor/difference`, bg union towers, minkowski) and `clean_bg_geometry` (`CGAL.cpp:281`).
- After `std::reverse(polygon.outer())` the local `outerArea` stays negative, so `if (outerArea < 0) polygon = {};` always fires and drops the polygon with a misleading warning. `outerArea -= innerArea` also grows the sum for correctly wound holes; the same arithmetic in `checkWindingOrders` (:575-583) makes its third warning unreachable.
- **Fix**: `outerArea = -outerArea` after the reversal and `outerArea += innerArea` once holes are negative.

#### GEO-A14 · BUG · CONFIRMED · Medium · S — `*_overlay_polygon` / `*_polygon_connectivity` write `first_rel` / `second_rel` through a UInt32 channel whatever the domain type
- **Where**: `BoostPolygon.cpp:213-214`, :593-595 (`locked_tile_write_channel<UInt32>`).
- `mutable_opt_array_cast<UInt32>` runs `debug_cast`, whose `MG_CHECK(dynamic_cast)` survives in Release: any uint8/uint16/uint64/int domain (12 provinces in `unit<uint8>`) fails with an internal error; on a uint32 domain not starting at 0 the index is written instead of the value. The unprefixed `polygon_connectivity` / `box_connectivity` (:2180, :2305) do it right through `IndexAssigner32`.
- **Fix**: write through `IndexAssigner32`.

#### GEO-A15 · BUG · CONFIRMED · Medium · S — A `dms_` dissolve throws "no lattice can be derived" on empty input or one undefined coordinate
- **Where**: `BoostPolygon.cpp:1748-1765` (`DeriveFrame`); `DMS_Traits.h:1784-1803` (`CoordStats::Add` stops at the first null or non-finite point).
- With an unranged fpoint/dpoint values unit, an empty or all-empty domain (`dms_union_polygon(dms_intersect(a,b))` without overlap) or one NaN point fails the whole dissolve, while the other back-ends return empty results. The bag path also reads `bagResourcePtr->begin()->frame` (:1846) of an empty ResourceArray when the partition unit is empty.
- **Fix**: merge stats over usable elements only; pick any frame when none is usable; return early when `domainCount == 0`.

#### GEO-A16 · BUG · CONFIRMED · Medium · S — `connect_info` leaves InArc / InSegm uninitialised for unconnected points (#1228 fixed only the reversed twin)
- **Where**: `geo/dll/src/Connect.cpp:1079-1085` (forward not-found path), :1011-1020, :1158-1167; compare :1233-1234.
- Bool tiles come from `GetWritableTile(t)` with `write_only_all`, so null points, points beyond maxDist and points without an eq/ne match get indeterminate bits.
- **Fix**: write `false` to r4/r5 on the forward not-found path and in both "other side empty" branches.

#### GEO-A17 · BUG · CONFIRMED · Medium · S — `dyna_segment_with_ends`: the chord after an emitted end point starts at a stale location
- **Where**: `OperPolygon.cpp:1323-1333` (the `withEnds` branch writes `ri2.Write(prevLoc)` but never sets `prevLoc = *i2`).
- On a connected next segment its first chord starts at the last interior sample of the previous one. The `dms_assert(carry >= 0)` (:1321) and `dms_assert(segmLength+carry >= dist*n)` (:1282) assume exact floating division and are UB in Release when it rounds up.
- **Fix**: `prevLoc = *i2;` in that branch; clamp `carry` to 0 instead of asserting.

#### GEO-A18 · BUG · CONFIRMED · Medium · S — `bg_` / `geos_` simplify accept a per-element tolerance but apply element 0's value to all
- **Where**: `BoostGeometryImpl.h:1324` (`UM_AllowVoidRight`), :1333 (`GetLockedDataRead()[0]`), `AbstrSimplifyOperator::CreateResult`.
- Silently wrong, and not validated: a negative tolerance makes GEOS throw, a null is undefined for boost. The buffer twins read per element.
- **Fix**: reject a non-void tolerance or read it per element; `MG_USERCHECK(IsDefined(maxError) && maxError >= 0)`.

#### GEO-A19 · BUG · CONFIRMED · Medium · S — `geos_buffer_multi_polygon` and `geos_buffer(polygon)` pass points per circle as quadrant segments
- **Where**: `BoostGeometryImpl.h:2244`, :2390 vs `(pointsPerCircle + 3) / 4` at :1857, :2003, :2106, :2399, :2409.
- GEOS's argument is segments per quarter circle: polygon buffers get 4× the documented vertex count and cost (`geos_buffer(g, d, 16)` makes 64-gons on polygons, 16-gons on arcs).
- **Fix**: `(pointsPerCircle + 3) / 4` in both; this changes results, so add a "Since 20.22.x" wiki note.

#### GEO-A20 · BUG · LIKELY · Medium · S (reject) / M (convert) — Allocation, graph and network operators assume zero-based units
- **Where**: `DiscrAlloc.cpp:2071-2077` (counts at `v - range.first`) vs raw use at :784, :810-812 (`GetRegionID`), :1106 (`GetClaim`), :3320, :3339; `ConnectedParts.cpp:25, 177, 185, 256`; `Dijkstra.cpp:2224-2229` with `InsertNode` (`Dijkstra.h:189-207`, guarded only by `assert`).
- With 1-based units (`range(uint32,1,N+1)`, common for imported ids) the claims of region v+1 are enforced on the land units of region v and the top value is rejected; in Dijkstra F2 = N passes validation and writes `m_ResultDataPtr[N]`, one past the end. 9248f6a97 noted only the rejection.
- **Fix**: `MG_USERCHECK` that these units are zero based, naming the unit, or index everywhere through `Range_GetIndex_checked`.

#### GEO-A21 · BUG · CONFIRMED · Medium · S — Every `discrete_alloc` / `greedy_alloc` / `needy_alloc` asserts on an empty land-unit set
- **Where**: `DiscrAlloc.cpp:1146` (`htp_info_t::GetN`: `dms_assert(this->m_N)`), first reached at :4044.
- `m_N == 0` is legitimate (an empty per-region subset). Debug exits 3; Release gets `__assume(false)`, which can fold away `CheckPerturbationRange`'s `if (!n || !k) return;` and report a bogus overflow.
- **Fix**: drop the assert or return empty results early.

#### GEO-A22 · BUG · CONFIRMED · Medium · S — `district_uint8/_uint16` with exactly 2^bits districts return an empty unit
- **Where**: `geo/dll/src/SpatialAnalyzer.h:251-260`, `OperDistrict.cpp:103`.
- With 256 districts the last gets id 255 (= null) and the counter wraps to 0, so the unit is `[0,0)` while cells carry ids 0-254, without an error; 257 or more do throw.
- **Fix**: throw before labelling when `resNrDistricts == MAX_VALUE(D)`.

#### GEO-A23 · BUG · CONFIRMED · Medium · S — `reg_count_uint8/_uint16` store a count equal to the null value
- **Where**: `geo/dll/src/RegCount.cpp:153-155` (`++(counts[j]); if (!counts[j]) throw…`).
- Exactly 255 (65535) cells of one class in a region are stored as null. `SafeIncrement` (`clc/dll/include/AggrFuncNum.h:25`) exists for this.
- **Fix**: `SafeIncrement(counts[j])`.

#### GEO-A24 · BUG · LIKELY · Medium · S — `discrete_alloc` `ClaimScaler`: `muldiv_u32(...) + 1` wraps for a null maximum claim
- **Where**: `DiscrAlloc.cpp:3299-3303`.
- A null maximum means "no limit"; in a scaled round (N > 1000) a fully sampled small region gives 0xFFFFFFFF, `+1` wraps to 0, max < min, and `dms_assert(first <= second)` is UB in Release; with min 0 the region is forced empty for that round and distorts the warm-start prices.
- **Fix**: `Min<UInt64>(UInt64(muldiv_u32(...)) + 1, orgMax)`, or map a null / ≥ N maximum to N in `PrepareClaims`.

#### GEO-A25 · BUG · LIKELY · Medium · S — `FindMstDown` / `FindMstUp` add shadow prices unchecked (a gap in #1196)
- **Where**: `DiscrAlloc.cpp:2393-2396`, :2492-2495; results feed `CheckedSub` / `CheckedAdd` at :2583, :2726.
- An overflowed cost is accepted and no `shadow_price_overflow` is raised: the silent misallocation #1196 set out to remove. The file header (:238-240) claims C++20 defines the wrap; it does not (signed overflow is still UB), so `compare_oper::GetC` (:558-561) is UB on overflow too.
- **Fix**: `CheckedAdd` / `CheckedSub` in both; correct the comment.

#### GEO-A26 · BUG · LIKELY · Medium · S — `rth_element(a, r, rel)` counts and offsets in UInt32; the Tot siblings too
- **Where**: `geo/dll/src/nth_element.cpp:784-805` (`RthElementPart<V, I=UInt32>`), :347 (`NthElementTot`), :570-580 (`NthElementWeightedTot`), :826 (`UInt32 pos = rr` with a NaN ratio).
- Beyond 4.29e9 defined values (a median per region of a 5-gigacell raster) the wrapped total under-allocates and `copy[cumul2[p]++]` writes past it. `NthElementPart` and `NthElementWeightedPart` already use `SizeT`: a partial fix of the family. A null ratio converts NaN to an integer (UB).
- **Fix**: `SizeT` throughout; null for an undefined or out-of-[0,1] ratio.

#### GEO-A27 · BUG · LIKELY · Medium · S — `potential` family: data nulls are zeroed, kernel nulls are not
- **Where**: `geo/dll/src/OperPot.cpp:388-398` vs the data cleansing at :421-435.
- A NaN in the FFT'd kernel makes every bin NaN, so all tiles come out null; `potentialSlow` too (NaN × 0), while `proximity` ignores it. A `1/dist` kernel has a null centre.
- **Fix**: cleanse kernel nulls to 0 or reject them, and document the rule.

#### GEO-A28 · BUG · CONFIRMED · Medium · S — `get_x` / `get_y` turn null integer points into numbers
- **Where**: `geo/dll/src/Point.cpp:309-319` (`p.X() * fx + tx`, built with `ArgFlags()`).
- `get_x(null ipoint) = -2147483648`; the same for spoint/wpoint/upoint. Float points propagate NaN; `pointcol` / `pointrow` keep the null.
- **Fix**: map `!IsDefined(p)` to `UNDEFINED_VALUE(ResCoordType)`.

#### GEO-A29 · BUG · CONFIRMED · Medium · S — `impedance_*`: three option combinations crash or overflow instead of erroring
- (a) `Dijkstra.cpp:2228`, :2247: `impedance_table` with `startPoint(Node_rel,OrgZone_loc)` + euclid passes `CheckFlags`, but `orgZones` is null for a table → access violation; use `orgZonesOrVoid` with `UM_AllowVoidRight`.
- (b) :2474-2476: `interaction:Link_flow` without v_i dereferences a null `adiOrgMass`; use `orgMassUnit` (:2441) or require v_i.
- (c) :819-829: with `precalculateted_NrDstZones` (sic, `stx/dll/src/DijkstraString.cpp:195`; the wiki spelling does not parse) the last origin's count is never compared, so a larger count writes past `SetCount(nrRes)`, and shorter origins leave rows uninitialised although the wiki promises undefined. The pareto twin at :1769 has the same gap. Accept the correct spelling too.

#### GEO-A30 · BUG · CONFIRMED · Medium · S — `interaction:NrDstZones` reports every destination zone in the dense regime
- **Where**: `Dijkstra.cpp:500-506` (`ZonalResCount`), :910-911, item at :2458-2460.
- Without `cut()` / `limit()` every origin gets `nrDstZones`, including unreachable and euclid-rejected zones; the wiki says "reachable destination zones", and the value lies outside its own values unit's range.
- **Fix**: count commits in the dense regime; give the item a plain count unit.

#### GEO-A31 · BUG / PERF · LIKELY · Medium · S — `diversity`: a null radius or one above 32767 hangs; counters sized by values-unit cardinality
- **Where**: `SpatialAnalyzer.cpp:26-46` (`TForm::Init`, UInt16 radius / Int16 form, plain `assert`), :125-147, :179, :189; radius read at `OperDistrict.cpp:191`.
- Radius 65535 (null) wraps `-m_Radius-1` to 0 and the loop never ends; a `unit<uint32>` without a range allocates a zero-filled 17 GB counter vector for a dozen codes.
- **Fix**: `MG_USERCHECK2(IsDefined(radius) && radius <= MAX_VALUE(FormType))`; size counters to the observed min..max.

#### GEO-A32 · PARTIAL · CONFIRMED · Medium · S (reject) / M (implement) — `connect` / `connect_info` / `dist_info` accept `minSqrDist` and ignore it
- **Where**: `Connect.cpp:867, 901, 954, 982-985, 1088-1089`, :1428, :1454-1459; instances at :2020-2062; the comment at :2031-2032 says "not implemented in either direction".
- The argument is unified, locked and stepped per row but never read: unfiltered results without a warning.
- **Fix**: drop the `HasMinDist` instantiations or throw "not implemented" until the filter exists.

#### GEO-A33 · BUG · CONFIRMED · Low-Medium · M — `spatialIndex` computes its root bounding box per tile
- **Where**: `Connect.cpp:1961-1990` ("TODO: Make Tile aware" at :1967).
- Quad ids in different tiles use different roots and are not comparable; with void lb/ub and a non-void level only element 0 is written; undefined lb/ub reach a `dms_assert(IsIncluding(...))`.
- **Fix**: bounds over all tiles in a pre-pass; loop over the result size; skip undefined rows.

#### GEO-A34 · BUG · LIKELY · Medium-Low · M — Distance kernels subtract in the coordinate type, so unsigned or narrow points wrap
- **Where**: `rtc/dll/src/geom/Point.h:286-304`, :370-374 (`SqrDist = Norm(p1-p2)`), used by `GeoDist.h:57-85`, `JoinNearValues.cpp:125,130`; registrations include WPoint / UPoint (`Connect.cpp:2096`, `JoinNearValues.cpp:212`).
- `join_near_values` on WPoint with A=(10,10), B=(9,10), d=2 gives SqrDist ≈ 4.3e9 and drops the pair; `connect` projections on arcs with decreasing unsigned coordinates are wrong.
- **Fix**: convert each coordinate to the result type before subtracting, or restrict the registrations to signed and float points.

**Performance**

#### GEO-A35 · PERF · CONFIRMED · Medium · M — #1283 "one engine per operation" was not applied to `dms_minkowski_sum` / `_difference`
- **Where**: `DMS_Traits.h:2300` (`union_dms_polygons::operator()` constructs a `DmsOverlayEngine` per fold), :2336-2341; `BoostGeometryImpl.h:521, 528-552, 569, 586`; the `PolygonTower` fold at `BoostPolygon.cpp:1695`.
- Each element costs about (ring edges × kernel parts) engine constructions, each with three pmr pools and ~30 buffers grown from zero, and every fold re-nodes the growing union. a0800530c measured about 2× from removing exactly this elsewhere.
- **Fix**: an engine member in `MinkowskiEngine<dms>` passed to a stateful reducer; better, append all convex cells to one `DmsSegmentBag` and call `UnionBag` once.

#### GEO-A36 · PERF · CONFIRMED · Medium-High · S-M — `bg_` / `geos_` binary operators re-validate and repair a parameter operand per element
- **Where**: `BoostGeometryImpl.h:951-965` → `BoostGeometry.h:626-660`; GEOS `BoostGeometryImpl.h:1073-1085` → `GEOS_Traits.h:1004-1021`.
- `geos_intersect(parcels, country_param)` runs a full IsValidOp over the large parameter for each of millions of parcels, and an invalid parameter is repaired and warned about per element.
- **Fix**: validate and clean the void-side operand once per tile and pass an "already clean" flag.

#### GEO-A37 · PERF / SIMPLIFY · CONFIRMED · Medium · M — `*_overlay_polygon` converts the second operand per candidate pair, runs serially, and builds a dead CGAL set
- **Where**: `BoostPolygon.cpp:408-580` (`PolygonOverlayOperator::Calculate`): :464, :496-497, :528; an unused `CGAL_Traits::Polygon_set poly2;` in the GEOS branch at :527; `serial_for` with a per-result lock.
- Each arg2 polygon is converted once per overlapping arg1 element (for CGAL an exact arrangement each time); a single-tile first argument runs single-threaded.
- **Fix**: convert arg2 once per tile next to its `SpatialIndex`; delete :527; `parallel_for` over element blocks (`StoreRes` already sorts per (t,u)).

#### GEO-A38 · PERF · CONFIRMED · Medium · M — The split-only delayed path runs its tiles serially
- **Where**: `BoostPolygon.cpp:940-956`. Operators: `{bp,bg,cgal,geos,dms}_split_polygon`.
- Split is delayed only because the result count must be known; `Calculate` writes disjoint slots (`tileOffset + i`, :791, :1827), so tiles could run in parallel with the existing per-thread contexts.
- **Fix**: for split without union, `parallel_tileloop` over `Calculate`, then `Store` once.

#### GEO-A39 · PERF · CONFIRMED · Medium · S-M — `impedance` interaction takes two global locks per (origin, destination) pair
- **Where**: `Dijkstra.cpp:1041-1054` (`writeBlocks.dstFactor`, `writeBlocks.dstSupply` inside `for (j …)`), :1160-1194 (`WriteLinkSets` walks the traceback again under `writeBlocks.od_LS`).
- A dense 10k × 10k run makes about 2e8 lock acquisitions around one `+=` each, serialising all workers.
- **Fix**: accumulate per worker in `dms_combinable<my_vec_t<MassType>>` (as `resLinkFlowC` does) and sum afterwards; collect link sets thread-locally.

#### GEO-A40 · PERF · CONFIRMED · Medium · S — `geos_` / `cgal_buffer_multi_point` union pairwise, unlike their fixed twins
- **Where**: `BoostGeometryImpl.h:1999-2011`, :2405-2415, :1972-1988.
- Quadratic; the same file already fixed this in `geos_minkowski_sum` (:410-412) and `cgal_buffer_multi_linestring` (:164-165).
- **Fix**: GEOS: one `MultiPoint->buffer()` or a collection `Union()`; CGAL: `join(begin, end)` once.

#### GEO-A41 · PERF · LIKELY · Medium (High for degree coordinates) · S — `InflatedSearchBox` widens by a whole coordinate unit even for float coordinates
- **Where**: `geo/dll/src/geom/SpatialSearchBox.h:37-43`, used at `Connect.cpp:610, 633`, `ConnectMatrix.cpp:326`.
- For lat/lon data every candidate box is at least 2-4 degrees wide, so each query measures most of a country's points.
- **Fix**: floor/ceil ± 1 only for integral T; for floating T widen by one ulp.

#### GEO-A42 · PERF · LIKELY · Medium · S-M — `potential`: every data tile waits on every result tile; FFT buffers kept for the process lifetime
- **Where**: `OperPot.cpp:300-318` (`AwaitAccumulationTurn` for all result tiles, also non-intersecting), `Potential.cpp:99-171`, :229-278, :761-779 (FFT length not rounded to a smooth size).
- te² handoffs with convoying; a 10000² grid with a 201² kernel leaves ≈ 5 GB committed after the operator ends.
- **Fix**: wait only on intersecting tiles in ascending order; free plan scratch and thread-local buffers after `CreateResult`; round `fftLen` up to a 7-smooth size.
- **Measured 2026-09-30, not done**: the first part of the fix was built (a data tile takes a turn only on the result tiles its overlap rect reaches, for its rank among the data tiles that reach each, with the tile rects asked once) and timed against its parent 81ccead5d on OVSRV10, interleaved: `potential` of a 12000 x 12000 float32 grid in 576 tiles of 500 x 500 with a 201 x 201 kernel took 1.76 to 1.85 s before and 2.10 to 2.21 s after, the same result. Letting more data tiles accumulate at once made it slower, plausibly because the convoy kept fewer FFT passes competing for memory bandwidth; that was not verified. Reverted; the patch is `scratch\audit5_g4\geo_a42_reverted.patch` on the machine that measured it. The other two parts were not tried: freeing the buffers saves memory, not time, and a 7-smooth `fftLen` changes the rounding of the results.

#### GEO-A43 · PERF · LIKELY · Medium · M-L — Tiled `griddist` runs tiles serially and recomputes every dirty tile from scratch
- **Where**: `geo/dll/src/GridDist.cpp:403-524`, :536-643.
- **Fix**: bucket start points per tile once; keep tile results and seed only new border cases; `parallel_tileloop` over dirty tiles; exchange only pairs where a tile changed.

#### GEO-A44 · PERF · CONFIRMED · Medium · M — `mid` / `centroid_or_mid` are O(n²) per polygon with an allocation per vertex
- **Where**: `geo/dll/src/SelectPoint.h:64-67`, `rtc/dll/src/geom/CalcWidth.h`.
- A 100k-vertex polygon costs about 5e9 edge tests and 50k vector allocations.
- **Fix**: one row sweep with an active-edge list, or at least reuse a `cols` buffer.

#### GEO-A45 · PERF · CONFIRMED · Low-Medium · S — The bp dissolve's "translate to zero" wraps only lazy inserts
- **Where**: `BoostPolygon.cpp:691-737`, :1067-1103; the real clean runs in `StoreImpl`'s `get()` at :1253-1254 on untranslated coordinates.
- `polygon_set_data::insert` / `move` only push or shift edges, so the move-insert-move sequence does no arithmetic in translated coordinates; it only costs extent scans.
- **Fix**: translate around `get()` / `clean()` in `StoreImpl`, as `ProcessNumOperImpl` does, and remove the dead moves.

**Partial and simplification**

#### GEO-A46 · PARTIAL · CONFIRMED · Low · S — `geos_polygon_connectivity` is still instantiated for all six point types
- **Where**: `BoostPolygon.cpp:2620, 2631` vs the DPoint-only overlay at :2612, :2627; the constructor calls `GetGeosNonDPointDeprecationFlag()` (:317-320), which throws when the major version > 20 (:105-111).
- The ten non-DPoint instances always throw "no longer supported"; at the v21 bump they would throw during `Geo.dll` static initialisation. The tripwire is missing from cleanup-list item 14.
- **Fix**: `tl::type_list<DPoint>` for both; add the tripwire to cleanup-list item 14.

#### GEO-A47 · PARTIAL · CONFIRMED · Low · M — Null operands mean different things in twin polygon back-ends
- **Where**: `PolyOper.h:163-170` (bp_: null → null), `DMS_Traits.h:1497-1503` (dms_: null → null), `GEOS_Traits.h:923-938` (`geos_union(null, B) = B`); bg_ and cgal_ read null as empty; `dms_polygon` / `dms_split_polygon` store an empty polygon for an undefined element (`BoostPolygon.cpp:1900-1902`), though the wiki says undefined.
- **Fix**: pick "null in, null out" (the documented dms_ rule), apply it in the shared converters, document it.

#### GEO-A48 · SIMPLIFY · CONFIRMED · Low · S — Dead or uncompilable code (verified by repo-wide grep)
- `BoostGeometryImpl.h:1352-1396` `cgal_douglas_peucker` and :1465-1476 call functions that do not exist (never instantiated: no `cgal_simplify_*` is registered); `BufferMultiPolygonOperator` (:2189-2259) lacks an `else static_assert`; `GEOS_Traits.h:113-140` `geos_circle` unused; `BoostGeometry.h:619, 631` `static SizeT d_DebugCount` in a header, incremented non-atomically, never read; `CGAL_60/Polygon_repair/repair.h` vendored "until CGAL 6.0" while vcpkg ships 6.1.1; `Connect.cpp:506-522` unused constructor, :78-83 `CutInfo::operator<`, :1891 `m_CompareType`; `bi_graph.h:556-572` `directed_dijkstra::run_tree` would not compile if instantiated; `OperPot.cpp:246, 382-385`; `Poly2GridOper.cpp:779, 865` (unused, inverted `isAllDefined`); `GridDist.cpp:133-135`; the `verboseLogging` Dijkstra flag only allocates.

**Raster operators, additional**

#### GEO-A57 · BUG · LIKELY · Low-Medium · S — The spatial analyser uses 32-bit cell positions
- **Where**: `geo/dll/src/SpatialAnalyzer.h:74-81` (`Pos` returns `SizeType` = UInt32, `SpatialBase.h:23`), :176-188 (`FindFirstNotProcessedPoint`). DMS: `district*`, `diversity`.
- `m_NrCols * p.Row() + p.Col()` is computed in UInt32: on grids with 2^32 or more cells `++pos == end` (a SizeT) never matches, so the scan wraps and hangs and the flood fill marks the wrong cells. (GEO-A22 is the separate district-counter wrap.)
- **Fix**: `SizeType = SizeT`; compute `Pos` in SizeT.

#### GEO-A58 · BUG · CONFIRMED · Low-Medium · S — `nth_element_weighted` returns an arbitrary pivot for a null target
- **Where**: `geo/dll/src/nth_element.cpp:515-528`; also :727-731 (`RthElementTot`, `SizeT pos = rr`) next to the :826 case of GEO-A26.
- A NaN `cumulWeight` fails both comparisons, so the function returns a median-guess pivot instead of null (Tot, and any partition with two or more positively weighted elements). A ratio outside [0, 1] also depends on an undefined float-to-integer conversion.
- **Fix**: return `UNDEFINED_OR_MAX(V)` for an undefined target or ratio; validate `0 <= r <= 1`.

#### GEO-A59 · PERF · LIKELY · Low-Medium · M — `merge` / `raster_merge` rescan the index per argument, run tiles serially and re-unify domains per tile
- **Where**: `geo/dll/src/RasterMerge.cpp:84-137` (`UnifyDomain` at :98, :102 inside the tile loop), :173-256 (`CopyWhere`: `*indexIter == a` at :204, :233, :248).
- For each result tile and each argument the index tile is scanned twice (O(nrArgs × cells), not the wiki's O(n)); a `ViewPortInfoEx` is built per (t, a) or (t, a, u); the tile loop is serial; `UnifyDomain` runs in the calc phase and may create data controllers from a calc task.
- **Fix**: decide `isSame` per argument before the calc phase; one pass per tile dispatching on the index value; `parallel_tileloop`.

#### GEO-A60 · BUG / SMELL · LIKELY · Low · S — `griddist_zonal` accepts a negative or null boundary impedance; the latitude factor differs between tiled and untiled
- **Where**: `geo/dll/src/GridDist.cpp:302-307`, :508-509, :613-614 (boundary factor added after the `MakeMax(deltaCost, 0)` clamp), :486 vs :594 (latitude row), :357-371.
- A negative factor gives negative edges, breaking Dijkstra (guarded only by `assert(deltaCost >= 0)`); a null one silently blocks every zone crossing. Across horizontal tile borders the exchange uses the t2 row for both directions, so diagonal moves cost slightly differently from `griddist_untiled_latitude_specific`; the factor seems evaluated at the row edge, not `y + 0.5` (unconfirmed).
- **Fix**: `MG_USERCHECK2(IsDefined(f) && f >= 0, …)`; the origin row in both exchange directions.

Also dead in this group: `Poly2GridOper.cpp:736-738` (the obsolete-third-argument warning is unreachable: a `BinaryOperator` without `allow_extra_args`); `geom/NeighbourIter.h:34-43, 70-77` (`SqrMaxDistTo`, `MaxDist`: unused, and would not compile); `GridDist.cpp:443` (`(!firstDist)` can never be true). `Potential.cpp:321-332` `AlignedArray::reserve` frees without nulling before an allocation that may throw (unreachable today: only called on empty arrays).

**Points, arcs and search (Connect family, SpatialIndex, Voronoi, BoundingBoxCache)**

#### GEO-A49 · BUG · CONFIRMED (Debug) · Medium-Low · S — Reversed `connect` asserts on a zero bound once a point lies on an arc
- **Where**: `rtc/dll/src/geom/GeoDist.h:257-263` (`SafeBet`: `assert(val > 0)`), :286-288 (`CalcDist`); from `geo/dll/src/Connect.cpp:618` (reversed handle) and :530 with a user maxSqrDist ≤ 0. Operators: reversed `connect`, `connect_info`, `dist_info` (+ `_eq` / `_ne`).
- When a point lies exactly on the arc, `m_MinSqrDist == 0` and the next candidate builds `aph(*pointPtr, 0, …)` → `SafeBet(sqrt(0))`: the CRT assert fires. Points placed on the network (nodes, stations) make this routine; the Release battery never sees it. With a zero bound in the forward direction `Inflate(p, 0)` is an empty box that can never match an axis-parallel arc through p.
- **Fix**: `CalcDist` returns 0 for a zero bound (as `SqrtBet` does); treat a non-positive maxSqrDist explicitly.

#### GEO-A50 · BUG · CONFIRMED (Debug) / LIKELY · Low · S — Null or empty inputs to index construction
- (a) `Connect.cpp:530` (filtered forward `IndexedArcProjectionHandle`, via :1060 / :1592) → `SpatialIndex.h:444-451` `GetSqrProximityUpperBound`, whose `dms_assert(nodePtr->IsNonEmpty())` fails on an empty root when all arcs are empty or null (Debug exit 3; Release `CC_ASSUME(false)` and, for IPoint, a signed overflow on the inverted default box); the reversed handle guards with `capBox.inverted()` (:595-597). (b) `Connect.cpp:98-104`: for integer point types the null sentinel compares equal, so `connect(dest, org)` / `connect_neighbour` with two null destination points throws "Multiple destinations with the same location found".
- **Fix**: the `inverted()` guard in the forward paths; filter `IsDefined` before the duplicate test.

#### GEO-A51 · BUG (contract) · CONFIRMED · Low · S — Tie-breaking contradicts the wiki's "lowest index"
- **Where**: `GeoDist.h:121-129` (first found wins), `geo/dll/src/geom/SpatialIndex.h:266-272` (LIFO leaf list), `NeighbourIter.h` (heap without index tie-break); the comment at `Connect.cpp:622` claims "keeps the point with the lowest index, as connect does".
- Vertical arcs at x = -1 and x = 1 and a point at (0,0): arc 1 is projected first and the refined box excludes arc 0, so `connect_info` reports arc_rel = 1. **Fix**: prefer the smaller index on equal distance in `MakeSafeMin`, the reversed handle and `neighbour_iter`, or correct the wiki and comment.

#### GEO-A52 · BUG · LIKELY · Low · S — `voronoi` truncates cell vertices for integer point types
- **Where**: `geo/dll/src/Voronoi.cpp:379-382` (`shp2dms_order<CoordType>(p.X(), p.Y())` converts Float64 to Int32 implicitly).
- Adjacent cells compute a shared vertex independently; truncation maps 499.99999 and 500.0 to 499 and 500, opening 1-unit gaps or overlaps, while the wiki promises an exact partition (its example uses ipoint). **Fix**: round to nearest for integral types.

#### GEO-A53 · PERF · CONFIRMED · Medium-Low · M — `connect` (FastConnect) phases 2-3: a map of vectors, then a second full copy of the result
- **Where**: `Connect.cpp:1746-1765` (`my_map_t<R, my_vec_t<CutInfoType*>> cutsPerArc` plus a sort per arc), :1773-1791, :1866-1875 (the network is built in `resultSubData`, then copied into `resSubData`); `CutInfo::operator<` (:78-83) already encodes (arc, segm, fraction) but is unused.
- One map node and one vector per cut arc; peak memory holds the result geometry twice (hundreds of MB extra for national networks).
- **Fix**: sort the cuts once with `CutInfo::operator<` and walk the runs; write straight into the result's sequence array.

#### GEO-A54 · PERF · LIKELY · Low-Medium · S — `SpatialIndex` construction is O(k²) in coincident objects
- **Where**: `geo/dll/src/geom/SpatialIndex.h:203-225` (`MustSplit`), :256-264, :489-490.
- A leaf holding only identical extents never splits, and each insert rescans the whole leaf list: k coincident points cost k²/2 comparisons (join_near_values B, reversed connect, defaulted or geocode-failed locations).
- **Fix**: a per-node "all extents equal" flag updated in O(1).

#### GEO-A55 · PERF · CONFIRMED · Low · S — Point-to-point `connect` allocates per point; `connect_neighbour` is serial
- **Where**: `Connect.cpp:478` (a new `neighbour_iter` with two growing heaps per point), :238-276 (`ConnectNeighbourPointOperator::Calculate`, single-threaded). **Fix**: one reused iterator per chunk (`Reset`, as `connect_neighbour` does); parallelise `connect_neighbour` the same way.

#### GEO-A56 · SIMPLIFY / SMELL · CONFIRMED · Low · S — Dead code and cache smells in the search family
- `GeoDist.h:290-298` (the derived `MakeSafeMin`, private and never called); the eq/ne filter lambda written four times (`Connect.cpp:1029, 1176, 1575, 1669`); `BoundingBoxCache.h:193, 227` (`mustPrepare` unused), :146-152 (`GetTile(t)` twice, unused locals), :33 (point block bounds built with `mustCheckUndefinedValues=false`, so null integer points inflate them and defeat GUI culling); the cache registry keyed by a raw `AbstrDataObject*` (ABA-prone while `FeatureLayer::m_BoundingBoxCache` keeps a cache alive) and `g_BB_Register[featureData]` inserting an empty entry that stays when the build throws; `shv/dll/src/FeatureLayer.h:287-299` (two cache getters calling a non-existent overload, never instantiated).
- **Fix**: delete; one `make_key_filter<CT>` helper; `find` instead of `operator[]`, or pin the data object from the cache.

**Tracked-item status (GEO)**

- **deadlocks.md P11** is fixed as stated, but the doc's claim that `~AbstrBoundingBoxCache` runs "when a feature's data object dies" is inaccurate: the cache dies with its last holder.
- **g8-todos.md:174** (generalise `Dist2Operator`, `Point.cpp:201`): still open; status correct.

- **GEO-32** is marked implemented but `Dijkstra.cpp:959` still indexes `endPoints.Impedances` by zone (GEO-A09).
- **GEO-36** is partly done: `Canyon.cpp:176-177` still call `GetDataRead()` inside `dms_assert`.
- **code-fixes.md:679** (dyna_point: `carry` not reset when `isFirstPoint` starts a new polyline): still open (:1260-1263).
- **doc/issues.md #1196** ("everything that produces a shadow price is covered"): incomplete (GEO-A24, GEO-A25); the header's C++20-wrap claim is wrong.
- **operator-signature-interface.md:1180** (eq/ne `ConnectPointOperator`): still open and worse than a describe issue: `cp_eq` / `cp_ne` register a 4-argument `connect(pts, key, pts, key)` on `cogCON` (`Connect.cpp:292`) that `CreateResult` (:343) treats as capacitated, so the keys silently become capacity weights; the 6-argument `ccp_eq` / `ccp_ne` hit `dms_assert(args.size()==2||isCapacitated)`.
- **dms-dissolve-single-noding.md**: the one-element shortcut exists for single rings only (multi-ring one-element slots are noded twice, `BoostPolygon.cpp:1933-1936`); the Prepare hook for the per-tile `dms_polygon` frame is not implemented, so the float lattice depends on the tiling; the split count/assign triple extraction ("for now") and Minkowski on `DmsPolySet` (GEO-A35) are open.
- **cleanup-list item 14** misses `GetGeosNonDPointDeprecationFlag()` (GEO-A46).
- Stale text: `bicriteria-impedance.md` §7 ("StartPoint_rel is never written", implemented in 987730a64; also the comment at `Dijkstra.cpp:180-182`), §5.4.2 (the last origin is not checked, GEO-A29c), §5.4.3 (`nrResultTiles` still vestigial); `code-fixes.md:53` (diversity values are now offset from `range.first` since eddf106f2); `crs-metric-decoupling.md` ruling 1 is done (`GridDist.cpp:315`) but still listed as work; line drift in g8-todos and code-fixes (content still valid).
- Fixed and correctly recorded: GEO-27, GEO-29, GEO-30, GEO-38, deadlocks P1 and P11, the ptr-safety `OperDistrict.cpp:156` and `RegCount.cpp:273` items; cleanup-list item 12's `BorderCase` was left on purpose.

### 4.7 STG: `stg/` (storage managers)

Three patterns recur: GDAL ownership mistakes (leaks on every written or linearised geometry), value
types or compositions that one driver supports and its twin silently mishandles, and write paths that
report success after a failed write. The dbf reader is both the least complete (no 64-bit integers, no
NULLs) and the slowest (a seek per field).

#### STG-A01 · BUG · LIKELY · High · S — gdal.vect: reading a multipoint attribute from a layer with other geometry types is UB
- **Where**: `stg/dll/src/gdal/gdal_vect.cpp:921-1026` (`ReadMultiPointData`, :957-1007), dispatched from `ReadGeometry` :1565-1588.
- Every branch of the switch calls `geo->toMultiPoint()`, also for Point, LineString, Polygon, CurvePolygon, MultiPolygon, MultiLineString, MultiSurface, CircularString and CompoundCurve. `toMultiPoint()` is `cpl::down_cast`, a plain `static_cast` without `CPLAssert`, so `AddMultiPoint` reads coordinate bytes as a count and a pointer. Trigger: an attribute with MultiPoint composition over a mixed-geometry layer (GeoJSON, CSV with WKT, PostGIS generic geometry); `CompareConfiguredGeometryWithGdal` returns early for `wkbUnknown` layers.
- **Fix**: handle each geometry type explicitly and call `toMultiPoint()` only for the wkbMultiPoint family.

#### STG-A02 · BUG / PARTIAL · CONFIRMED · High · M — shp: writing a MultiPoint attribute produces an empty shapefile; gdalwrite.vect refuses it
- **Where**: `stg/dll/src/shp/ShpStorageManager.cpp:396-425` (`WriteDataItem`), `WriteSequences` :278-352, `ShpImp.cpp:355-373`; `gdal_vect.cpp:2606-2611` (`CheckVCAndVCIForGeometry`), :2593-2604, :2358-2389.
- In the shp writer `shapeType` stays `ST_Point` for MultiPoint; the data goes through `WriteSequences` into polygon records and `ShpImp::Write` writes the empty `m_Points`: a valid 0-record shapefile and a successful write (Debug asserts). The shp reader does support `ST_MultiPoint`. In gdalwrite.vect, `vc <= Sequence` excludes MultiPoint (3), so the column is refused as an unsupported field type; `WriteGeometryElement` and `DmsType2OGRGeometryType` have no MultiPoint case.
- **Fix**: map MultiPoint to `ST_MultiPoint` and to `wkbMultiPoint` with a write case; throw on any unhandled composition instead of falling back to `ST_Point`.

#### STG-A03 · BUG / PARTIAL · CONFIRMED · High · S — dbf: int64 / uint64 attributes read as zeros; N(10..19,0) columns are auto-typed int32
- **Where**: `stg/dll/src/dbf/dbfImp.cpp:649-695` (`ReadDataElement`: no Int64 / UInt64 case), :697-744 (`WriteDataElement` returns false), :91-102 (`DbfTypeToValueClassID`); `dbfStorageManager.cpp:212-217` instantiates Int64 / UInt64.
- Every element keeps the zero of `write_only_mustzero`, without an error: exactly what a modeller configures for 16-18-digit ids in N(18,0) columns. `DoUpdateTree` types any `N(len>4, dec=0)` column as int32, so values of 10 or more digits overflow.
- **Fix**: add Int64 / UInt64 cases; parse integers with `from_chars` into the target type rather than via Float64 (exact only to 2^53); choose Int64 for len > 9.

#### STG-A04 · BUG · CONFIRMED · Medium · S — dbf: one NULL numeric field fails the whole column
- **Where**: `dbfImp.cpp:633-647` (`ReadAsFloat64` throws), :686-693; `rtc/dll/src/ser/ReadValue.h:49-56`.
- dBase, ESRI and shapelib / GDAL store a NULL numeric as blanks or `*****`; the parse fails and throws "unexpected character in parsing … as numeric". String columns already map "null" to undefined.
- **Fix**: an all-blank or all-`*` numeric field is `UNDEFINED_VALUE(T)`; throw only on real garbage.

#### STG-A05 · BUG · CONFIRMED · High · S — gdal.vect / gdal.grid `GetPropTables` swaps the live dataset handle without the storage lock, opens twice and leaks
- **Where**: `stg/dll/src/gdal/gdal_vect.cpp:3337-3386`, `gdal_grid.cpp:550-599`; caller: the GUI dataset-information page (`qtgui/exe/src/DmsDetailPages.cpp:251-261, 330`).
- On the GUI thread `DoOpenStorage` / `DoCloseStorage` assign and null the manager's `m_hDS` without `m_CriticalSection`: a worker reading the same storage has its dataset closed under it. In Debug `assert(!m_CriticalSection.try_acquire())` fires and leaves the lock acquired; in Release `dms_assert(m_hDS == nullptr)` is UB. Each call opens the dataset a second time and never frees the WKT of `exportToPrettyWkt`. `GdalGridSM::EnsureBlockSizeCached` (:87-113) and `TiffSM::EnsureBlockSizeCached` (`TifStorageManager.cpp:101-119`), reached through the `StorageTileSizeX/Y` properties, touch `m_hDS` / `m_pImp` unlocked too, and their GDAL fallback open bypasses `Gdal_DoOpenStorage` (no #1140 sidecar filter, driver list or config options).
- **Fix**: a local `GDALDatasetHandle` from `Gdal_DoOpenStorage`, never touching `m_hDS`, or take the section; `CPLFree` the WKT.

#### STG-A06 · BUG · CONFIRMED · Medium · S — gdal.vect `DoUpdateTree` crashes when the first feature has no geometry, and leaks that feature
- **Where**: `gdal_vect.cpp:3123-3138` (`GetValueComponsitionFromFirstGdalFeature`), from :3141-3142 and :3193.
- For a `wkbUnknown` layer without a configured geometry item, `first_feature->GetGeometryRef()->getGeometryType()` is not null-checked: an empty first WKT or `"geometry": null` dereferences null during meta info. The `OGRFeature*` is never destroyed; `layer->GetGeometryColumn();` has no effect.
- **Fix**: wrap in `FeaturePtr`, loop to the first feature with a geometry, null-check.

#### STG-A07 · BUG · CONFIRMED · High · S — gdalwrite.vect leaks one geometry per written polygon or arc feature
- **Where**: `gdal_vect.cpp:2341, 2344` (`feature->SetGeometry(ogrMultiPoly.release())` / `ogrPoly.release()`), :2279-2288 (the factory `OGRLine` in `SetArcGeometryForFeature` is never freed).
- `SetGeometry(const OGRGeometry*)` copies; the `unique_ptr` overload does not match a raw pointer. `.release()` drops ownership of the original, so an export leaks a full copy of the geometry column and can run out of memory on large layers. The point writer uses a stack object and is fine.
- **Fix**: `SetGeometryDirectly(ptr.release())` or `SetGeometry(std::move(ptr))`; hold `OGRLine` in a `unique_ptr`.

#### STG-A08 · BUG · CONFIRMED · Medium · S — gdal.vect read: curve geometries leak their linearised copy per feature
- **Where**: `gdal_vect.cpp:884, 900, 1115, 1131, 1200, 1205, 1316, 1321` (`geo->getLinearGeometry()->toX()`).
- `getLinearGeometry()` returns a new geometry that is never deleted: reading CurvePolygon, MultiSurface, CircularString or CompoundCurve data (common in GML sources such as BGT / BRK) leaks one geometry per feature per attribute read.
- **Fix**: `std::unique_ptr<OGRGeometry> lin(geo->getLinearGeometry());`.

#### STG-A09 · BUG · CONFIRMED · Medium · M — bmp: GridData read fails on its first row, and every bmp / pal read asserts on the tile id
- **Where**: `stg/dll/src/bmp/BmpStorageManager.cpp:170-174` (`GridDataHandler::ReadData`), :321 (`dms_assert(t == no_tile)`); `BmpImp.cpp:799`.
- The loop runs `r = height .. 1` with `GetRow(Mirror(r, height))`, so the first row is `UInt32(-1)` and the target offset is one row off (the writer twin :219-244 decrements first). Since the Phase 1 `MG_CHECK` (c75d98039) every bmp GridData read throws; before it was UB. `ReadDataItemInto` passes `t = 0..n-1`, never `no_tile`, so :321 stops a Debug run for every bmp or pal read and is UB in Release. No battery or tst case uses the `bmp` / `pal` storage types.
- **Fix**: `for (r = 0; r != height; ++r)`; honour `t` and drop the assert; add a battery case.

#### STG-A10 · BUG · CONFIRMED · Medium · S — tif: tile read and write errors are silently ignored (STG-14's refutation rests on a false premise)
- **Where**: `stg/dll/src/tif/TifImp.cpp:689-700` (`ReadTile`, `WriteTile`), :120-132, :32-42 (errors recorded only into an active `TifErrorFrame`); `GridStorageManager.h:193-216, 571-574`.
- A `TifErrorFrame` exists only in `Open` and `Close`, so libtiff errors during `TIFFReadTile` / `TIFFReadEncodedStrip` are dropped and a failed tile becomes `defaultColor`: a corrupt TIFF reads as nodata without a message. `TIFFWriteTile` returning -1 and `TIFFClose` flush errors are ignored. The gdal.grid twin throws on both. `TiffSM::WriteDataItem` (:271) also calls `m_pImp->GetNrBitsPerPixel()` before the file is opened (:281): a null dereference for an int32 PaletteData.
- **Fix**: a `TifErrorFrame` around `ReadTile` / `WriteTile`, throw on a negative result or a recorded error; throw from `Close` when not unwinding; take the palette bit depth after opening.

#### STG-A11 · BUG / PARTIAL · CONFIRMED · Medium · S — tif: the rotation from ModelTransformationTag (and from a .tfw) is read and discarded
- **Where**: `stg/dll/src/tif/TifStorageManager.cpp:439-453` uses `[0] [3] [4] [5]` of the 6-vector from `TifImp.cpp:220-231`; `GeoRef.cpp:156-163` reads the .tfw rotation into `dummy`.
- A rotated GeoTIFF read through `tif` gets a silently wrong axis-aligned georeference, while gdal.grid builds the full affine (`gdal_base.cpp:1937-1954`). The STG-N02 regression case compares sums only.
- **Fix**: build a `CrdTransformation::Affine2x3`, or fail when b or e ≠ 0; the same in `ReadGeoRefFile`.

#### STG-A12 · BUG / PARTIAL · CONFIRMED · Medium · S — tif: the bit-size check blocks the conversions TifImp implements
- **Where**: `TifStorageManager.cpp:199-209` vs `TifImp.cpp:557-566` (`UnpackCheck`), :589-686 (`UnpackStrip`).
- `GetValueClassFromTiffDataTypeTag` returns VT_Unknown for 24 bits (an RGB TIFF into a uint32 colour attribute throws), and a 4-bit palette into uint8 or an 8-bit mask into bool throw "Mismatch in number of bits": exactly the pairs `UnpackCheck` admits and `UnpackStrip` expands, unreachable since #506 (30b4a0bd6, 2023-11).
- **Fix**: accept the pairs `UnpackCheck` accepts.

#### STG-A13 · BUG · CONFIRMED · Medium · S — gdal.vect one-pass read (#587 S4): one incompatible column fails every member of the table
- **Where**: `gdal_vect.cpp:1920-1969` (`MakeFieldWriter` throws the type conflict inside the tile loop, :2043-2046); `StorageReadOperators.cpp:506-511`.
- The exception leaves `ReadDataItemsAtOnce`, and `ReadMembersAtOnce` fails every target not yet done: all other columns and the geometry. Before S4 only the offending attribute failed. The message prints raw enum ints (`int(fieldType)`), as does `ReadAttrData` (:2175).
- **Fix**: validate types in the column-collect loop (:1989-2008) and fail only that target; print value class names.

#### STG-A14 · BUG / PERF · CONFIRMED (commit on throw) / LIKELY (cost) · Medium · S — gdalwrite.vect commits a half-written tile; OpenFileGDB copies the layer files per tile
- **Where**: `stg/dll/src/gdal/gdal_base.h:224-237` (`GDAL_TransactionFrame`), used per tile at `gdal_vect.cpp:2852`.
- The destructor always commits, also while unwinding from `CreateFeature` / `SetFeature` failures (:2920-2923), so a failed export leaves a partial GPKG; the `StartTransaction` result is ignored. `StartTransaction(true)` makes OpenFileGDB emulate transactions by copying all the layer's table files on the first write of each transaction: once per tile, quadratic I/O on large `.gdb` exports.
- **Fix**: roll back when `std::uncaught_exceptions()` rose; one transaction per layer, or force only for drivers without `ODsCTransactions`.

#### STG-A15 · PERF · CONFIRMED · High · M — dbf: every element costs a seek and a buffer refill; the stdio buffer size is ignored
- **Where**: `dbfImp.cpp:649-661` (`fseek` + `fread` per field), :773-777, :799-806; `ImplMain.cpp:59-89` (`nrPagesInBuffer` unused since `setvbuf` was commented out).
- Reading one column seeks once per record and each seek discards the read buffer: a 1M × 20 dbf costs 20M syscalls, each refilling a full buffer for 10 bytes. shp and bmp run through the default 4 KB CRT buffer. The per-column open is the S4 "dbf at once: not done" item; the per-element seek is not tracked.
- **Fix**: read whole records in blocks and slice the columns out (or map the file); restore `setvbuf` with `nrPagesInBuffer*4096` in `FilePtrHandle::OpenFH`.

#### STG-A16 · PERF · CONFIRMED · Medium · M — gdalwrite.vect: dispatch, error frame and tile lookup per element
- **Where**: `gdal_vect.cpp:2458-2479` (`WriteFieldElement`), :2347-2394, :2882-2924.
- Per (feature, field): a `weak_ptr` lock, a `GDAL_ErrorFrame` (a PROJ errno query under an SEH frame, twice), a `visit<field_types>` dispatch and `GetDataRead(t)` for a tile already held in `tileReadLocks` (:2853): 20M of each for 1M features × 20 fields.
- **Fix**: per tile, one typed writer per field capturing the tile view, and one error frame per tile.

#### STG-A17 · BUG · CONFIRMED · Medium · S — gdal.grid `ReadPalette` dereferences a null colour table and writes past the palette domain
- **Where**: `stg/dll/src/gdal/gdal_grid.cpp:134-151`; the tif twin `TifStorageManager.cpp:223-244` is correct.
- `GetColorTable()` is null for a raster without a palette, so a configured PaletteData crashes; `nrElems` is unused and the loop runs to `GetColorEntryCount()` (up to 65536) past a smaller domain.
- **Fix**: mirror `TiffSM::ReadPalette` (null check, clamp, fill).

#### STG-A18 · BUG · LIKELY · Medium · S — WMTS tile reader dereferences null for an out-of-range palette index
- **Where**: `gdal_grid.cpp:901-915` (`GDAL_SimpleReader::ReadGridData`).
- `GetColorEntry(idx)` returns null for an index beyond the table and is dereferenced; the input is a downloaded, untrusted tile. `ReadBand` (:799-819) sizes its buffer from the file's width × height (security item #7).
- **Fix**: null-check (treat as transparent); bound the raster size before `vector_resize`.

#### STG-A19 · BUG · CONFIRMED (mechanism) · Medium · S — Non-ASCII paths still fail in tif and in dbf's commit rename (#1101 siblings)
- **Where**: `stg/dll/src/tif/TifImp.cpp:383, 407` (`TIFFOpen` → `CreateFileA`); `dbfImp.cpp:194-202` (`CommitFile` with narrow `remove` / `rename`).
- `FilePtrHandle` and `BmpImp` moved to wide APIs; tif did not, so tif reads and writes fail under Greek or other non-CP1252 paths. `CommitFile` removes the original, then renames through ANSI APIs; with such a path both fail, the message prints `strerror(-1)`, and with an ASCII path a failed rename after a successful remove loses the original.
- **Fix**: `TIFFOpenW(Utf8_2_wchar(...))`; `std::filesystem::rename` in `CommitFile`.

#### STG-A20 · BUG · CONFIRMED · Medium · S — shp: a point shapefile with any null-shape record cannot be read
- **Where**: `stg/dll/src/shp/ShpImp.cpp:240` (`MG_CHECK(rhead.ContentLength*2 == 4+2*8)`), :251; the dead null handling at :254-255.
- The ESRI spec allows null records (4 bytes, type 0), and shapelib / GDAL write them for features without geometry; both checks fire before the `IsNone` branch. The polygon path handles null records (:651-655).
- **Fix**: read the shape type first; for `ST_None` require 4 bytes and emit undefined.

#### STG-A21 · BUG · CONFIRMED · Medium · S — shp and dbf writers ignore write errors
- **Where**: `ShpImp.cpp:325-399` (`pos` summed, never compared); `dbfImp.cpp:572-607`, :741-742; `cfs/CompoundStorageManager.cpp:121-124` (destructor `Commit` result ignored).
- The defect STG-23 fixed in the str writer: a full disk or dropped share gives a truncated .shp / .shx / .dbf reported as written, and the shp writer writes in place (`FCM_CreateAlways`), so the old file is gone too.
- **Fix**: compare byte counts, `MG_CHECK` every `fwrite`; write to a temp file and rename.

#### STG-A22 · BUG · LIKELY · Medium · M — dbf, odbc and xdb assert the tile id instead of handling tiled domains
- **Where**: `dbf/dbfStorageManager.cpp:195` (`dms_assert(!t)`), `odbc/OdbcStorageManager.cpp:739`, `xdb/XdbStorageManager.cpp:40`; the caller loops tiles at `StorageReadOperators.cpp:333-336`.
- For an attribute on a tiled domain, `ReadDataItem` is called for `t = 1..n-1`: UB in Release, and each call rewrites the whole column through `GetDataWrite(no_tile)`. The shp twin returns early for `t != 0` (`ShpStorageManager.cpp:208-209`).
- **Fix**: at least `MG_CHECK(t == 0)`; better, read the tile's range as gdal.vect does.

#### STG-A23 · BUG · LIKELY · Medium · S — dbf record offsets are 32-bit and `fseek` takes a `long`
- **Where**: `dbfImp.cpp:341-351` (`UInt32 ActualPosition`), :660 (result ignored), :801, :848, :913.
- Beyond 2 GB `fseek` fails and the read continues from the stale position; beyond 4 GB the offset wraps. Shapefile .dbf companions above 2 GB exist.
- **Fix**: `SizeT` offsets with `_fseeki64` / `fseeko`, checked.

#### STG-A24 · BUG / PERF · CONFIRMED · Low · S — gdal.vect's single-attribute int64 reader emits a GDAL warning per feature
- **Where**: `gdal_vect.cpp:1763-1765`: `GetFieldAsInteger` (32-bit) for the "genuine value" test, then `GetFieldAsInteger64`; for OFTInteger64 values beyond int32 GDAL warns on every call. The interleaved branch and the S4 path are correct.
- **Fix**: `GetFieldAsInteger64` once.

#### STG-A25 · PERF · CONFIRMED · Medium · S — gdal.vect keeps the last tile's geometry buffer until the storage manager dies
- **Where**: `gdal_vect.h:115` (`m_ReadBuffer`), `gdal_vect.cpp:789-795`, :360-373 (`OnClose` does not release it).
- After a layer read one tile's worth of geometry stays allocated for the session; for an untiled domain that is a second copy of the whole column, and since b3a6ff34a the next unrelated read reserves that size up front.
- **Fix**: reset `m_ReadBuffer` in `OnClose` or after the last tile.

#### STG-A26 · BUG · LIKELY · Low · S — gdal.vect cursor bookkeeping for repositioned and interleaved reads
- **Where**: `gdal_vect.cpp:2178-2194` (`SetCurrFeatureIndex` never sets `m_CurrFeatureIndex = firstFeatureIndex` after `SetNextByIndex`); :698-718, :318-336.
- After a backward seek the index stays too high, so every later tile re-seeks (for CSV / GeoJSON a rescan: quadratic) and a later forward skip can land on the wrong features, e.g. two leftover geometry reads after the S4 pass. For OSM, `SetNextByIndex` does not move the dataset cursor; with a `SqlString` on an OSM source the interleaved reader never sees the result layer's features. The comment at :2028-2030 calls geopackage a random-layer-read dataset; only GMLAS, NGW and OSM are.
- **Fix**: set the index after `SetNextByIndex`; `ResetReading()` when seeking back on interleaved datasets; `layer->GetNextFeature()` for result layers.

#### STG-A27 · BUG · LIKELY · Low · S — gdal.vect: an unclosed ring fails the whole read; an empty ring reads out of bounds
- **Where**: `gdal_vect.cpp:483-510` (`AddLinearRing`, and its ZM twin).
- `MG_CHECK(getX(0) == getX(numPoints-1))` throws for any unclosed ring from third-party GeoJSON / WKT; the shp reader closes such rings (`ShpStorageManager.cpp:150-169`). With `numPoints == 0`, `getX(-1)` reads `paoPoints[-1]`.
- **Fix**: skip empty rings; close open rings.

#### STG-A28 · PARTIAL · CONFIRMED · Low · S — gdalwrite.grid refuses bool / uint2 / uint4 while NBITS options exist for them (uint4 set to 3 bits)
- **Where**: `stg/dll/src/gdal/gdal_base.cpp:1740-1743` vs :1878-1882, :1617-1624.
- `gdalRasterDataType(vc, write=true)` gives `GDT_Unknown`, so the NBITS lines are dead; enabled, `NBITS=3` for uint4 would clamp 8..15. :1621 builds a `std::string` from a possibly null `GDAL_DMD_CREATIONDATATYPES`.
- **Fix**: `GDT_Byte` + NBITS 1/2/4; null-check the metadata item.

#### STG-A29 · PARTIAL · CONFIRMED · Low · S — gdalwrite.vect writes uint32 / uint64 as signed, so large values throw
- **Where**: `gdal_vect.cpp:2445-2455` (`ThrowingConvert<Int32>(UInt32)`), :2560-2561.
- A uint32 value ≥ 2^31 (hashes, OSM ids) aborts the export. **Fix**: map `VT_UInt32` to `OFTInteger64`.

#### STG-A30 · PERF · CONFIRMED · Low · S — Missing reserves (siblings of b3a6ff34a / 3fd3cb90c) and per-feature overheads
- `shp/ShpStorageManager.cpp:128-156` (`ReadSequences`: no `data_reserve` although the total is known); `gdal_vect.cpp:1610-1643`, :1892-1901 (string tiles grow from nothing); `odbc/OdbcStorageManager.cpp:562, 585-586` (with `recordsPerFrame = 1` it reserves only for one-row tables and fetches row by row; `GetActualSizeEstimate` :472-482 would then read out of bounds if only the condition is fixed); `gdal_vect.cpp:740, 772, 818, 941, 1049, 1173, 1289, 1404, 1623` (`TestCapability(ODsCRandomLayerRead)` per feature); :1619 (`if (!(i & 0xf000))` calls `ASyncContinueCheck` on 4096 of every 65536 rows; `0x0fff` was meant).

#### STG-A31 · PARTIAL / SIMPLIFY · CONFIRMED · Low · S — Dead virtuals in `AbstrStorageManager` after #587 S3
- **Where**: `rtc/dll/src/tic/stg/AbstrStorageManager.h:283` (`EasyRereadTiles`), :284 (`CanWriteTiles`), :433 (`DropStream`): overridden, never called; :321 (`GetUrl`), :326 (`VisitSuppliers`), :351 (`DoCreateStorage`): virtual, never overridden. Not listed in storage-read-operators.md or cleanup-list.md.
- **Fix**: delete the uncalled three with their overrides; de-virtualise the other three.

#### STG-A32 · BUG · LIKELY · Low · S — `GdalVectlMetaInfo`'s constructor can loop forever
- **Where**: `gdal_vect.cpp:303`: `adiParent = adi->GetTreeParent().get();` should walk from `adiParent`. For a data item nested under another data item below the storage holder, without a unit or SqlString in between, it loops on the meta thread.

#### STG-A33 · BUG · LIKELY · Low · S — cfs: the reader did not get the writer's 1 GB chunking (STG-N03 sibling)
- **Where**: `stg/dll/src/cfs/CompoundStorageManager.cpp:185-192`: one `IStream::Read` with `ThrowingConvert<ULONG>(size)` throws for blocks ≥ 4 GiB; `pcbRead` is null, so a short read goes unnoticed. **Fix**: loop in 1 GB chunks and check `pcbRead`.

#### STG-A34 · BUG · CONFIRMED (read, by algebra) / LIKELY (write) · Medium · S — A rotated GDAL georeference is read transposed and written without its rotation
- **Where**: `stg/dll/src/gdal/gdal_base.cpp:1937-1953` (`GetTransformation`), `stg/dll/src/GeoRef.cpp:118-125`, `rtc/dll/src/geom/Transform.h:382, 440-445`.
- This build is col/row (`point.first` = X / column; `DMS_POINT_ROWCOL` is not defined), which the axis-aligned branch assumes via `shp2dms_order`. The rotated branch assumes the opposite ("in.first=L=row, out.first=Yworld") and builds `Affine2x3({tr[5], tr[4], tr[3], tr[2], tr[1], tr[0]})`; with `u = a*x + b*y + c` that is transposed, and at zero rotation it disagrees with the axis-aligned branch. On write, `Factor()` / `Offset()` guard axis-separability with a Debug `assert` only, so a rotated grid is written with defaults in Release. Together with STG-A11 (tif drops the rotation), no reader handles a rotated raster correctly.
- **Fix**: `Affine2x3({tr[1], tr[2], tr[0], tr[4], tr[5], tr[3]})`; `MG_CHECK(IsAxisSeparable())` on write or write the off-diagonal terms; a gdal-read testcase checking cell-centre world coordinates of a rotated raster.

**Tracked-item status (STG)**

- **STG-14** (code-fixes, marked refuted): the refutation says libtiff's message "is captured by the TifErrorFrame"; no frame is active during `ReadTile`, so the status is wrong (STG-A10).
- **STG-N02**: the tag is read, but the rotation terms are dropped (STG-A11); its test compares sums only.
- **STG-16, STG-17**: fixed. **STG-23**: done for str; the shp, dbf and tif writers are still unchecked (STG-A10, STG-A21).
- **cleanup-list item 9** (GDAL conversion matrix): open, and S4 added a second copy of the matrix (`MakeFieldWriter`, `gdal_vect.cpp:1920-1969`) beside `ReadAttrData` (:2099-2170); the doc's anchor (2072-2171) is stale.
- **storage-read-operators S4 "dbf and odbc at once: not done"**: open (and see STG-A15). **g8-todos.md:155** (`m_CurrFieldIndex` lazy init, "TODO: Lock"): open, now at `gdal_vect.cpp:2082-2089`.
- **May-2026 security backlog**: #3 (`GDALAllRegister` fallback) open (`gdal_base.cpp:1773`, :845); #6 (raster overflow) partly fixed: `UInt32 tile_wh = tw_aligned*tileSize.Y()` in `Grid::ReadTiles` / `WriteTiles` (`GridStorageManager.h:153, 535`) still wraps for single-strip images above 4 G pixels, after which `RectCopy` reads beyond `strip`; #7 (decompression bomb) open (`gdal_grid.cpp:805`); #8 (`RasterIO` bounds) fixed; #9 (band index `std::stoi`) partly: `stoi` still throws `std::invalid_argument` / `out_of_range` instead of a DmsException (`gdal_grid.cpp:220-240`); #15 (`/vsicurl`, `/vsis3`, `/vsizip`) open.
- Stale comment: `gdal_base.cpp:735-736` ("runs once per tile read"; reads now open once per reader clone).

### 4.8 SHV: `shv/`

Two themes dominate: the grid-layer commands that assume a direct grid (the siblings of the paste
fixes c54394f32 and d10f00368), and the per-call GDI object creation that the DrawContext migration
(Step 4a/4c) left behind when it retired `PenArray` and `SelectingFontArray`. The WMS layer holds the
last detached thread in shv after #1255.

#### SHV-A01 · BUG · CONFIRMED · High · S — `GridLayer::SelectDistrict` corrupts the heap on an indirect grid and for a click outside the grid
- **Where**: `shv/dll/src/GridLayer.cpp:508-573` (`SelectDistrict`), :466-506 (`District`), :516 (`dms_assert(IsIncluding(gridRect, gridLoc))`); pop-up item :1588-1596; `Controllers.cpp:780-792`; `geo/dll/src/SpatialAnalyzer.h:74-81, 191-197` (bounds only by `assert`).
- `District` flood-fills a grid of `Size(gridRect)`, but on an indirect grid the theme attribute and the selection / edit attribute are defined on the entity domain, so the districter reads the entity array as a grid and writes selection bits at grid offsets: out of bounds whenever there are more cells than features. The menu item is offered whenever `HasClassIdAttr()`, indirect grids included. The seed `gridLoc` is guarded only by `dms_assert`, so a click outside the grid writes outside `output` and `m_Processed`; and it is absolute, so a grid whose range does not start at (0,0) gets a shifted seed.
- **Fix**: refuse on `m_Themes[AN_Feature]` and leave the menu item out (as c54394f32 did for paste); return when `!IsIncluding(gridRect, gridLoc)`; pass `gridLoc - gridRect.first` and translate `changedRect` back.

#### SHV-A02 · BUG · CONFIRMED · High · M — WMS tile requests never time out; closing a map view, exiting or copying the view can hang the GUI
- **Where**: `shv/dll/src/WmsLayer.cpp:183-191` (`TileLoader::SetTimer`: the expiry handler only logs), :296-305 (`ProcessPendingTasks`), :326-336 (`~TileCache`), :392-393 (cap of 8 loaders), :954-969 (non-suspendible `Draw` loop), :485-535 (`FetchUrlToFile`: synchronous, no deadline), called from `ShvControlsSupport.cpp:176, 198`.
- Nothing cancels a stalled TLS handshake or HTTP read (dropped Wi-Fi, VPN reconnect, sleep). A stalled loader keeps `s_InstanceCount > 0`; after 8 of them no tile loads for the rest of the session. `~TileCache` (view close, exit) and the non-suspendible draw (copy view, `ViewPort::Export`) loop `restart(); run();` until the count is 0, blocking the GUI thread forever. The WMS legend is fetched synchronously on the GUI thread from a layout call and a paint.
- **Fix**: on timer expiry `cancel()` / `close()` the socket and mark the tile undefined; give `ProcessPendingTasks` a deadline or cancel all loaders of a dying cache first; fetch the legend asynchronously.

#### SHV-A03 · BUG · LIKELY · Medium · M — The WMS `io_context` runs on two threads, and its handlers touch GUI objects off the GUI thread
- **Where**: `WmsLayer.cpp:215-225` (a detached `dms_task` runs the io_context), :296-305, :961 (the GUI thread runs the same io_context), :434-447 (`~TileLoader` → `owner->RunTileLoads(true)` on the io thread), :388-430, :204/240/257/272/549.
- asio forbids `restart()` while another thread is inside `run()`, and here both the loader thread and the GUI thread call `restart(); run();` on one static io_context. Handlers read `m_ZoomLevel` and the ViewPort's `m_w2vTr` unsynchronised, and a handler's temporary `owner` can become the last owner when the view closes, so `~WmsLayer` runs on the io thread and `~TileCache` re-enters `run()` from inside a handler. This is the one detached thread left in shv after #1255 (SHV-53).
- **Fix**: one owned loader thread (or the #1255 poller); post every completion to the DataView as a GUI operation that re-checks the layer there.

#### SHV-A04 · BUG · CONFIRMED · Medium · S — `GridLayer::CalcSelectedGeoRect`: the indirect-grid branch never advances its cell index
- **Where**: `GridLayer.cpp:595-614` (`SizeT i = 0;` is never incremented; `sdb[sdIndex]` without `IsDefined`); reached via `ViewPort::AL_ZoomSel` (`ViewPort.cpp:545-554`: zoom-to-selection toolbar, Ctrl+F, show first selected row).
- Every cell tests the entity of cell 0: Debug stops on the `assert` at the second cell; Release zooms to the whole grid or nothing, and when cell 0 has no feature `sdb[UNDEFINED]` reads far past the bit array (access violation). The other `GetEntityIndex` callers (`DrawPolygons.h:243`, `GridFill.h:213-215`) check `IsDefined`.
- **Fix**: advance `i` with `c` (and skip ahead when `c` jumps past `selectRect`); skip undefined `sdIndex`.

#### SHV-A05 · BUG · CONFIRMED (leak, null) / LIKELY (cross-process) · Medium · M — Grid cell copy/paste uses `CF_PRIVATEFIRST`
- **Where**: `GridLayer.cpp:61` (`CF_CELLVALUES = CF_PRIVATEFIRST`), :751-804 (`CopySelValues`), :67-80 (`PasteHandler` trusts the handle and `m_Size`), :814/:834; `Clipboard.cpp:148-155`, :165-172 (`GetData` returns the handle after `CloseClipboard`), :174-181 (looks only at the first format), :157-163 (unchecked `GlobalAlloc`).
- Windows neither frees nor marshals private-range clipboard handles: every Copy Selected Cells leaks its block, and pasting in a second GeoDMS instance hands `GlobalLock` another process's handle value. `GetData` returns null when another process holds the clipboard, and `PasteHandler` dereferences `nullptr->m_Size`. `m_Size` / `m_Rect` are not checked against `GlobalSize`.
- **Fix**: `RegisterClipboardFormat("GeoDMS.CellValues")` + `IsClipboardFormatAvailable`; copy while the clipboard is open; validate the header against `GlobalSize`; `GlobalFree` on failure.

#### SHV-A06 · BUG · LIKELY · Medium · S — Layer Control keeps the old legend when the layer's legend domain changes
- **Where**: `shv/dll/src/LayerControl.cpp:671-676` (`SetPaletteControl` returns when `NrEntries()==3`), :741-762 (`DoUpdateView`); trigger :541-560 → `ClassBreakClipboard.cpp:896-918`; `PaletteControl.cpp:40-64` (domain fixed at construction).
- `DoUpdateView` sees a new legend domain and calls `SetPaletteControl()`, which returns at once because the old palette container is still entry 2; nothing is replaced and every later `DoUpdateView` repeats the work. Trigger: Paste Classbreaks (#734) on a layer with a configured classification: the map redraws with the new classes while the legend lists the old ones.
- **Fix**: remove the old palette container before rebuilding, or rebuild the whole LayerControl.

#### SHV-A07 · PERF · CONFIRMED · Medium · M — Draw loops create a GDI pen, brush or font per feature, cell or label
- **Where**: `shv/dll/src/GdiDrawContext.cpp:111-138` (new pen and brush per `DrawPolyline` / `DrawPolygon` / `DrawEllipse`), :35-72 (`FillRect`; semi-transparent colours add a DC and bitmap per call), :169-215 (`CreateFontW` per `SetFont` / `SetBold`); callers `DrawPolygons.h:285-290, 523-529`, `FeatureLayer.cpp:1183, 1420, 1950, 2479`, `LabelDrawer.h:47-54, 80-86`, `GraphVisitor.cpp:667`, `DataItemColumn.cpp:1100/1134/1142`.
- Step 4a/4c (ca9bb9da0, 922fb95bc) replaced `PenArray` and `SelectingFontArray` by `DrawContext` calls that create and delete a GDI object per call: about 3M create/select/delete calls per redraw of a 1M-polygon layer, and a `CreateFontW` plus a token-registry lock per feature for a layer with a font-size theme. `PenIndexCache` / `FontIndexCache` still compute the distinct keys; nobody caches the handles.
- **Fix**: small pen / brush / font caches in `GdiDrawContext`, keyed by (colour, width, style) and (name, height, angle), freed in its destructor; or revive `PenArray` / `FontArray` per draw pass (see SHV-A21).

#### SHV-A08 · PERF · CONFIRMED · Medium · S — TableControl row loops re-prepare the row entity and sort index for every row
- **Where**: `shv/dll/src/TableControl.cpp:276-298` (`GetRecNo` → `NrRows()` → `PrepareDataOrUpdateViewLater` + `GetDataCount`, a second prepare and a `PreparedDataReadLock` on `m_SelIndexAttr`), :262-274; loops `GoToFirstSelected` :976-978, `SelectRows` :945-947, `TableControl_SaveTo` :1009-1010, `DataItemColumn::FindNextValue` (`DataItemColumn.cpp:1636-1660`), every painted cell (:1076).
- Each `GetRecNo(row)` does 2-3 prepares (interest inc/dec, `SuspendibleUpdate`, a `FencedBlocker`) and a read lock; go-to-first-selected, find-next and copy-table are O(rows) over this, seconds to minutes on 10M rows.
- **Fix**: prepare and lock `m_SelIndexAttr` and read `NrRows()` once outside the loop, then call the private `getRecNo(row, nrRows)`.

#### SHV-A09 · BUG · LIKELY · Low · S — A row number that is not ready yet (UNDEFINED) is used as an array index
- **Where**: `TableControl.cpp:947` (`b[GetRecNo(rowNr)] = isSelected`), :978; `DataItemColumn.cpp:1639-1641, 1653-1656`.
- `GetRecNo` returns `UNDEFINED_VALUE(SizeT)` while `m_SelIndexAttr` is being produced (just after a sort); Select Rows then writes bit 2^64-1 and find-next reads out of bounds.
- **Fix**: skip or abort when `!IsDefined(recNo)`; falls out of SHV-A08.

#### SHV-A10 · BUG · LIKELY · Low · S — The #1248 settle overwrites a classification edited before the counts arrived
- **Where**: `shv/dll/src/Theme.cpp:597-664` (`settleGeneratedClassification`: unconditional `SetCount(nrBreaks)` + `CreatePaletteData`), :672-704; competing edits `EditPalette.cpp:481-485, 503-507, 532-536, 624-628, 647-651`, `ClassBreakClipboard.cpp:756-776`.
- The settle is posted once `weeded_counts` is readable, which can take seconds; a classification the user made meanwhile in Edit Palette or by pasting breaks is silently replaced by min(#distinct, 8) classes.
- **Fix**: remember the provisional count and timestamp at `Theme::Create` and skip when the palette domain changed since; or clear `m_ClassCounts` in the palette-edit paths.

#### SHV-A11 · BUG · LIKELY · Low · S — Pen styles lost since Step 4a: wide dashed pens draw solid; caps, joins, INSIDEFRAME, USERSTYLE ignored
- **Where**: `shv/dll/src/DrawPolygons.h:490-493`, `FeatureLayer.cpp:1417`; `PenIndexCache.cpp:193-202`; `GdiDrawContext.cpp:97-109` (`CreateDmsPen`: unknown values become `PS_SOLID`, uses `CreatePen`); the dead `ExtCreatePen` path `PenIndexCache.cpp:238-251`.
- `CreatePen` supports dashes only at width ≤ 1, so every dashed border wider than one device pixel is solid, including every dashed pen on a 200 % display; a style with `PS_ENDCAP_*` / `PS_JOIN_*` bits becomes solid. The old `PenArray` used geometric `ExtCreatePen` for these.
- **Fix**: map the full `PS_*` value and use `ExtCreatePen(PS_GEOMETRIC|style, …)` for width > 1 or geometric flags; fits in the pen cache of SHV-A07.

#### SHV-A12 · BUG · LIKELY · Low · S — Draw-context state leaks between layers (text alignment, background mode and colour)
- **Where**: `shv/dll/src/FeatureLayer.cpp:1098, 1855` (`SetTextAlign(true,true)` never restored), `LabelDrawer.h:32-35`, `GraphVisitor.cpp:653-676`; consumers assuming left/top: `PieLayer.cpp:502, 521`, `TextControl.cpp:117`, `DrawPolygons.h:597`.
- 922fb95bc replaced the scoped `DcTextAlignSelector` by a bare `SetTextAlign`. One draw pass draws all objects through one context, so after a point layer later polygon and arc labels and pie captions draw centred, depending on which layers intersect the update region, so labels shift between partial repaints.
- **Fix**: a scoped state guard in `DrawContext`, or reset the state in `GraphDrawer::DoLayer`.

#### SHV-A13 · BUG · LIKELY · Low · S — A stack `GridCoord` in the rotated grid draw erases the live registration
- **Where**: `GridLayer.cpp:1240`, `GridCoord.cpp:48-56` (`~GridCoord` erases `m_GridCoordMap[m_Key]` unconditionally), `ViewPort.cpp:273-275, 1471-1473`, `GridLayer.cpp:1635-1645`.
- For a grid without georeference the per-tile temporary has the same key as the layer's real GridCoord; its destructor removes the real one, which is then no longer re-initialised on zoom or pan. Trigger: rotate the view (Shift+arrow) and back.
- **Fix**: in `~GridCoord` erase only when the entry points to this object; construct the temporary without an owner.

#### SHV-A14 · PERF · CONFIRMED · Low · S — `GridFill`'s duplicate row / column reuse is defeated for every tile not at offset 0
- **Where**: `shv/dll/src/GridFill.h:183` (`currGridRow -= rowOffset`) vs :260 (`currGridRow != currGridRowPtr[-1]`); :207 vs :236/:243.
- The duplicate checks compare a tile-relative row / column with the absolute value in the GridCoord arrays, so for every tile except the top-left one (and every paste block) zoomed-in views recompute each device pixel: about 100× more classify / palette work at 10 pixels per cell.
- **Fix**: compare the absolute values.

#### SHV-A15 · PARTIAL · CONFIRMED · Low · S — Commands offered for layer kinds that cannot handle them (siblings of c54394f32)
- **Where**: `GridLayer.cpp:1609-1620` (Copy Selected Cells on an indirect grid takes Certain read locks :755-771 before it throws at :767-768); `FeatureLayer.cpp:2283-2286, 2777-2780` (`SelectPolygon` NYI for arc and polygon layers, tool enabled through `ViewPort.cpp:1277-1282` → `GraphicObject.cpp:724-727`); `GraphicLayer.cpp:516-519` (`SelectDistrict` throws for non-grid layers).
- Drawing a selection polygon on a polygon or arc layer, the most common case, ends in "Not Yet Implemented" after the user has drawn it. `PieLayer::SelectPolygon` (`PieLayer.cpp:671-676`) already has an intersection implementation.
- **Fix**: override `OnCommandEnable` to disable unsupported tools; leave the Copy item out for indirect grids; implement polygon / arc `SelectPolygon`.

#### SHV-A16 · BUG · LIKELY · Low · S — `CopySelValues` computes the clipboard size in UInt32 (the live twin of SHV-41)
- **Where**: `GridLayer.cpp:782-788`: `UInt32 selSize = Cardinality(selRect); UInt32 dataSize = …`.
- Above about 4 G cells the size truncates, `GlobalAlloc` gets a small block and the copy loop writes the full selection. SHV-41's `SizeT` fix went into `CopySelValuesToBitmap`, which has no callers.
- **Fix**: compute in `SizeT`, refuse selections above a sane limit.

#### SHV-A17 · BUG · CONFIRMED · Low · S — `SHV_DrawInHDC` deletes the clip region twice
- **Where**: `shv/dll/src/ShvDllInterface.cpp:363-381`; `GdiRegionUtil.h:21-48` (`RegionToHRGN` returns an owning `GdiHandle<HRGN>`).
- `::DeleteObject(hrgn)` at the end, then `~GdiHandle` deletes it again, on every caret move, blink and region redraw of the Qt host on Windows. A handle value reused in between would delete an unrelated GDI object.
- **Fix**: drop the explicit `DeleteObject`.

#### SHV-A18 · BUG · LIKELY · Low · S — The Win32 scroll-bar page size is not scaled by the thumb-tick factor (the Qt twin is)
- **Where**: `shv/dll/src/ScrollPort.cpp:459-486` (`nPage = m_NettSize` while `nMax` / `nPos` are divided by `m_NrLogicalUnitsPerTumpnailTick`) vs :494-505.
- Beyond ~107M rows the tick factor is ≥ 2: the thumb is too large and page-down jumps several pages.
- **Fix**: divide `nPage` by the factor, as the Linux path does.

#### SHV-A19 · PARTIAL · LIKELY · Low · S — Linux: the DataView tree is never populated, so broadcasts reach no view
- **Where**: `shv/dll/src/DataView.cpp:2119-2144` (the non-Win32 `CreateMdiChild` never calls `AddChildView`), :1987-1993, :165-191 (`BroadcastUpdateRequest`), :155-163 (`BroadcastCmd`).
- On Linux a selection change refreshes no other view and Ctrl+Shift+1/2/3 does nothing.
- **Fix**: register the view when the Qt ViewHost is attached (`SetViewHost`), unregister in `~DataView`.

#### SHV-A20 · PARTIAL · CONFIRMED · Low · L — ViewHost boundary (cleanup-list item 6): residue and the defects it still carries
- **Where**: `DataView.cpp` (43 `#if…_WIN32`, 85 `m_ViewHost` references, 29 `if (m_ViewHost)` guards, 33 `m_hWnd` uses); 163 `#if…_WIN32` lines across `shv/dll/src`; `Win32ViewHost.cpp/.h` (300 lines, never instantiated); `DataView.cpp:1462-1476` (`SafeMenuHandle = GdiHandle<HMENU>` destroys menus with `DeleteObject` instead of `DestroyMenu`); :1914, :2044, :2072 (`if (MG_DEBUG_INVALIDATE || true)`: Debug builds paint red / orange / blue onto the window on every invalidation); `ViewPort.cpp:1153` (a DLL cursor resource loaded with a NULL instance); `MovableObject.cpp:678-701` (two cursor systems; the paste pan cursor of `ViewPort.cpp:1386` does not show while a selection or zoom tool is active).
- The null-ViewHost fallbacks are reachable only between `CreateWindowEx` and `SetViewHost` and after `~QDmsViewArea`: dead in practice, but they still carry these defects.
- **Fix**: make the host mandatory, delete `Win32ViewHost` and the fallbacks, fold `HCURSOR` into `DmsCursor`.

#### SHV-A21 · SIMPLIFY · CONFIRMED · Low · S — About 900 lines of dead code (verified by repo-wide grep)
- `GridLayer.cpp:926-1002` (`CopySelValuesToBitmap`, which would also crash on `GridCoord mapping(nullptr, …)`); `PenIndexCache.cpp:214-300` (`PenArray`); `FontIndexCache.cpp:218-330` (`FontArray` / `SelectingFontArray`, used only by `DataItemColumn::GetFont` `DataItemColumn.cpp:1440-1483`, which has no callers, plus `m_FontArray` / `m_FontIndexCache`); `DcHandle.h/.cpp`: `CaretDcHandle`, `ClippedDC`, `DirectDC` and eight `Dc*Selector` classes; `DataView.h:279` (`RemoveAllCarets`, declared, never defined); `ViewPort.cpp:389` (`g_CurrVpZoom`); `ShvCompat.h:195` (a `MessageBoxA` stub returning IDYES); `GridLayer.cpp:1310-1313` (unreachable since c54394f32).
- Two tracked fixes (SHV-41, SHV-52) landed in this dead code, which makes their tracked status misleading.
- **Fix**: delete, or reuse `PenArray` / `FontArray` as the caches of SHV-A07.

#### SHV-A22 · SMELL · Low · S — Coordinate-frame smells in the grid paste and selection-caret paths
- `GridLayer.cpp:1329-1351` (`DrawPaste` passes the absolute rect to `GridDrawer`, which expects relative coordinates, then adds the offset again; `DrawAllRects` :1074 subtracts it); `SelCaret.cpp:207-210` (relative clip rect intersected with the absolute update region) vs :124-125; `GridFill.h:179-186` (`continue` without advancing `resultingClassIds`); `GridDrawer.cpp:485` (passes `m_SelValues->m_Size`, a byte count with header, as element count).
- Correct only because the map ViewPort sits at (0,0) in `MapControl` (`MapControl.cpp:108`); a host with an offset ViewPort (`ChartControl`) would misplace the preview or trip `GetGridRowPtr`'s checks.
- **Fix**: subtract the viewport offset as `DrawAllRects` does; advance by `viewColSize` in the `continue`s; pass `Cardinality(m_Rect)`.

**Tracked-item status (SHV)**

- Closed and verified: SHV-42, SHV-43, SHV-44/45, SHV-46/73, SHV-54.
- **SHV-53**: closed by #1255 for `GraphicObject`, `GraphDataView` and `TableControl`; `WmsLayer`'s `dms_task` remains (SHV-A02, SHV-A03).
- **SHV-41**: the fix is in dead code; the live `CopySelValues` still truncates (SHV-A16).
- **SHV-52 / SHV-55**: the fixes are in `PenArray`, dead since Step 4a; the live `GdiDrawContext::CreateDmsPen` creates pens unchecked, per call.
- **PORTING_STATUS.md "Portable pen/font caching"** is framed as Linux-only, but Windows lost `PenArray` / `SelectingFontArray` too (SHV-A07, SHV-A11); its dead-code list and the "Remove dead code: TextControl.cpp GDI free functions" item are stale (removed in 2d20c5ac1).
- **cleanup-list item 6**: open (SHV-A20).
- **class-break-operator.md** open items are accurate; SHV-A10 is new.

### 4.9 QT, PY, RUN and scripts: `qtgui/`, `python/`, `run/`, `batch/`, CMake, `nsi/`

#### QT-A01 · BUG · CONFIRMED · High · S — `CloseConfig` deletes the Calculation times window while a `unique_ptr` still owns it
- **Where**: `qtgui/exe/src/DmsMainWindow.cpp:1359-1364` (`CloseConfig` deletes every sub-window); owners `DmsMainWindow.h:350-351` (`m_calculation_times_window`, `m_calculation_times_browser`), added at :2518-2519, read at :1862 (`end_timing`).
- View > Calculation times adds a `unique_ptr`-owned window to the MDI area; `CloseConfig` (every reload and exit, `main_qt.cpp:550`) `close()`s and `delete`s it, browser included. The next calculation of 2 s or more crashes in `end_timing` → `isVisible()`, so does reopening the view, and `~MainWindow` deletes both again on exit.
- **Fix**: skip it in the `CloseConfig` loop (`removeSubWindow` only), or make it an ordinary delete-on-close window created on demand.

#### QT-A02 · BUG · CONFIRMED · High · S — CSV export of a database container loops forever
- **Where**: `qtgui/exe/src/DmsExport.cpp:208`, reached from :846.
- `for (auto tableItem = …GetFirstSubItem(); tableItem; tableItem->GetNextItem())` discards the increment, so the export rewrites the first table forever (or spins) on the GUI thread; CSV with the native driver is the default for a non-mappable item. The per-table name `DelimitedConcat(fullFileName, name)` also turns `file.csv` into a folder and drops the extension (since ca591b6f3).
- **Fix**: `tableItem = tableItem->GetNextItem()`; name sub-files `<folder>/<name>.csv`; add a test-script export case.

#### PY-A03 · BUG · CONFIRMED · High · S — Python `set_values_from_float_list` / `_int_list` write past the tile buffer
- **Where**: `python/dll/src/Bindings.cpp:639-659`; the bound is only a Debug `assert` at `rtc/dll/src/tic/DataArray.ipp:426`.
- `SetValuesAsFloat64Array(lock->GetTiledLocation(0), data.size(), data.data())` passes the list length unchecked into tile 0: a longer list, or any multi-tile domain, is heap corruption from ordinary Python input; a shorter list leaves the rest of a write-only allocation undefined. The getters beside it loop over tiles correctly (:589-594). `DMS_Unit_GetCount(...)` is called and its result discarded.
- **Fix**: `MG_USERCHECK2(data.size() == count, …)` and write tile by tile; make the `DataArray.ipp` bound an `MG_CHECK`.

#### PY-A04 · BUG · CONFIRMED · Medium · S — Python `asDataItem` / `asUnitItem` cast unchecked and the wrappers dereference null
- **Where**: `Bindings.cpp:741, 743, 783, 785` (`AsDataItem` / `AsUnit` are `static_cast` in Release, `TicBase.h:190-191`); unchecked `m_au->` / `m_adi->` at :791-799, :805-817, :825-851.
- `root.find("/missing")` returns null by design; `.asDataItem().name()` then crashes the interpreter with an access violation; `asDataItem()` on a container mis-casts in Release.
- **Fix**: `AsCheckedDataItem` / `AsCheckedUnit` or `AsDynamic*` plus `MG_USERCHECK2`; null-check every wrapper method.

#### QT-A05 · BUG · CONFIRMED (wiring) / LIKELY (crash) · Medium · S — Config Options enablement hangs on the Tools menu; its probe crashes on a bad values unit
- **Where**: `qtgui/exe/src/DmsActions.cpp:431` connects `m_tools_menu` (not `m_settings_menu`) `aboutToShow` to `updateSettingsMenu`; `DmsMainWindow.cpp:2363-2365`; `DmsOptions.cpp:659-666`, :760-787.
- Settings > Config Options keeps whatever state it had when the Tools menu was last opened. The probe runs in a slot without `try` and calls `adi->GetAbstrValuesUnit()->GetUnitClass()`, which is null for an unresolved values unit (`AbstrDataItem.cpp:160-165`): an overridable parameter with a mistyped unit crashes the GUI when Tools is opened.
- **Fix**: connect `m_settings_menu`; null-check the unit; try/catch in the slot.

#### QT-A06 · BUG · LIKELY · Medium · S — The app-wide `EditorNavigationMenuFilter` dereferences `MainWindow::TheOne()` after the window is gone
- **Where**: `qtgui/exe/src/DmsActions.cpp:113-125` (installed on `qApp` at :222); `main_qt.cpp:539-561`.
- The filter reads `mainWindow->m_edit_config_source_action` for every Show event of every widget, and outlives `MainWindow`. When an exception leaves `main_without_SE_handler` after the window was built (a `/T` script that could not be opened, `CloseConfig` throwing), the catch's `QMessageBox::critical` Show event dereferences null: the error report becomes an access violation.
- **Fix**: `if (!mw) return false;`, test `menu` first; better, remove the filter in `~MainWindow`.

#### QT-A07 · BUG · LIKELY · Medium · S — Pop-up menus of map and table views open at the wrong position
- **Where**: `qtgui/exe/src/DmsViewArea.cpp:1481` (`VH_ShowPopupMenu`: `mapToGlobal(QPoint(clientPoint.x, clientPoint.y))`); compare `shv/dll/src/DataView.cpp:1541-1545`.
- `clientPoint` is in device pixels relative to the DataView HWND, which sits at `contentsRectInPixelUnits()`; treating it as widget DIPs drops the frame and title-bar offset in Tile/Cascade mode and ignores DPI, so at 125 / 150 % the menu opens away from the click. `VH_ClientToScreen` (:907-923) converts correctly.
- **Fix**: convert through `VH_ClientToScreen` and divide by the device pixel ratio, or use `QCursor::pos()`.

#### QT-A08 · BUG · CONFIRMED · Medium · S — Linux: opening Local Machine Options writes Windows defaults into the settings file
- **Where**: `qtgui/exe/src/DmsOptions.cpp:420-437`, written by `setInitialStringValue` from `restoreOptions` (:518-520) in the constructor (:409); POSIX engine defaults at `rtc/dll/src/utl/Environment.cpp:2651-2671`.
- On open, even with Cancel, an empty key is filled with `C:\LocalData`, `C:\SourceData` or Notepad++; the next Linux session gets a relative folder literally named `C:\LocalData`. The editor browse starts at `C:/Program Files` with an `*.exe` filter (:621).
- **Fix**: show the engine's effective value (`GetLocalDataDir()` / `GetSourceDataDir()`), never persist on open, per-platform defaults.

#### BAT-A09 · BUG · LIKELY · Medium · S — The .m and .c setup install gates accept a stale unit aggregate
- **Where**: `batch/BuildSignAndCreateSetup.bat:216-217`, `batch/BuildSignAndCreateSetupCmake.bat:236-237`; compare `batch/run_unit_suite.bat:81-95`, which the .g script uses.
- The gate takes the newest `v<ver>.<flavor>_*.txt` and fails only on a FAILED line; re-running a setup script for the same version (the usual fix-and-rebuild cycle) while `unit.bat` fails to start grades the previous run's passing aggregate and installs. The #1231 guard reached .g and the launchers, not these two siblings.
- **Fix**: call `run_unit_suite.bat <ver> m|c "<install dir>"` from both, as .g does.

#### BAT-A10 · PARTIAL · CONFIRMED · Medium · M — The Linux setup's "post-install" unit gate tests the build tree
- **Where**: `batch/BuildSignAndCreateSetupLinux.bat:142-150` → `TestLinuxReleaseUnit.sh:28` → `unit_linux.sh linux-x64-release` (`tst/batch/unit_linux.sh:16-24`).
- The comment says it mirrors the Windows `unit.bat` call, which tests the installed copy; Linux tests `build/linux-x64-release/bin`, so a package missing a `.so` or data file passes, and when dpkg is skipped nothing installed is tested at all.
- **Fix**: point `unit_linux.sh` at `/opt/ObjectVision/GeoDms<ver>.l` (or the unpacked tarball); fail when the install was skipped.

#### BAT-A11 · BUG · CONFIRMED · Medium · S — `RunGUITests.bat` never fails on GUI behaviour and passes a result path as the GUI's item
- **Where**: `batch/RunGUITests.bat:38-41, 66-69` (GUI exit code only echoed), verdict at :49, :77 from GeoDmsRun only; stray argument at :66.
- Since #1239 GeoDmsGuiQt exits 1 when a `/T` script reports an error, but PASS/FAIL comes from a GeoDmsRun run of the same configuration. The second GUI command ends with `…DPGeneral_ES_error.txt`, which `main_qt.cpp:131-133, 520-523` takes as the item to activate.
- **Fix**: fail on a non-zero GUI exit, compare the GUI's output file, drop the stray argument.

#### RUN-A12 · BUG · CONFIRMED · Low-Medium · S — GeoDmsRun `@file` output: an open failure is ignored and the run exits 0
- **Where**: `run/exe/src/MainRun.cpp:339-354`.
- `outstream = std::ofstream(...)` is never checked; with a missing folder every `@statistics` / `@valueinfo` result is discarded silently.
- **Fix**: report an error and return non-zero when `!outstream`, and check it after writing.

#### QT-A13 · PARTIAL · CONFIRMED · Low-Medium · S — Linux: every clipboard copy of a view is also saved to `/tmp/geodms_copy_N.png`
- **Where**: `qtgui/exe/src/DmsViewArea.cpp:1522-1531` (`VH_CopyToClipboard`).
- Test scaffolding in production: each copy writes map or table content to world-readable `/tmp` with a growing counter.
- **Fix**: behind an explicit test flag, or delete.

#### QT-A14 · PERF · CONFIRMED · Medium · M — The tree model is quadratic in the number of siblings and posts tasks from paint
- **Where**: `qtgui/exe/src/DmsTreeView.cpp:183-210` (`index`: linear walk to `row`), :212-236 + :59-81 (`parent` → `GetRow`: linear walk), :238-251, :294-307 (`getTreeItemIcon`: `SHV_GetViewStyleFlags` and a `PostMainThreadTask` per DecorationRole query).
- QTreeView calls `index(i)` for every row and `parent()` constantly, so containers with thousands of items lay out in O(N²); each repaint of a data-ready, uncommitted item posts another `SuspendibleUpdate`.
- **Fix**: a per-parent row vector invalidated on `NC_NewSubItem`, the show-hidden toggle and reset; one deduplicated update task per item.

#### QT-A15 · BUG · LIKELY · Low-Medium · S — Linux `QtDrawContext::FillRegion` / `InvertRegion` discard the caller's clip
- **Where**: `qtgui/exe/src/QtDrawContext.cpp:29-36`, :63-73; clip set in `DmsViewArea.cpp:1406-1407`.
- `setClipRegion(qrgn)` replaces instead of intersecting, and the following `setClipping(false)` removes the invalidation clip for everything drawn next: overdraw and smears on Linux.
- **Fix**: `save(); setClipRegion(qrgn, Qt::IntersectClip); …; restore();`.

#### QT-A16 · BUG · CONFIRMED · Low · S — The CMake (.c) GUI lacks the resource script and the Common-Controls manifest
- **Where**: `qtgui/exe/CMakeLists.txt:13-18` vs `GeoDmsGuiQt.vcxproj:226` (`GeoDmsGuiQt.rc`) and :86 (`/MANIFESTDEPENDENCY`).
- The .c setup's exe has no embedded icon, so the Start-menu shortcut (`nsi/DmsSetupScriptX64-cmake.nsi:247`) and Add/Remove Programs (:164) show the generic icon; the runtime window icon hides it in testing.
- **Fix**: `target_sources(GeoDmsGuiQt PRIVATE GeoDmsGuiQt.rc)` and the same link option under `WIN32`.

#### BAT-A17 · BUG · CONFIRMED · Low · S — `batch/run_unit.bat` was moved without re-rooting
- **Where**: `batch/run_unit.bat:2-5`.
- `geodms_rootdir=%~dp0` now resolves to `<repo>\batch`; it hard-codes `C:\dev\tst\batch` and calls `unit_flagged.bat` directly, bypassing the `run_unit_suite.bat` guards: exactly the silent-pass mode AGENTS.md warns about. AGENTS.md:168 claims every moved script re-roots.
- **Fix**: delete it, or make it a thin `run_unit_suite.bat` caller.

#### BAT-A18 · BUG · LIKELY · Low · S — `TestShippedContent.bat` resolves `SourceDataDir` differently from the engine
- **Where**: `batch/TestShippedContent.bat:57-78`; the engine reads `GEODMS_directories_SourceDataDir` first (`Environment.cpp:492-497`).
- With that variable set, the script renames a geopackage the run never reads, so the download path it exists to test is not walked, without error.
- **Fix**: check `%GEODMS_directories_SourceDataDir%` first.

#### QT-A19 · BUG · LIKELY · Low · S — GUI test-script shutdown race on a plain `bool`
- **Where**: `qtgui/exe/src/TestScript.cpp:596-623`; `main_qt.cpp:482-549`.
- `mustTerminateToken` is a non-atomic `bool` shared with the `std::async` script thread; if that thread posts its next line after the final `ProcessAppOpers()` flush, `testResult.get()` and the script thread wait on each other: a hung headless run at exit. A non-zero `exec()` skips the flush while the future's destructor still joins.
- **Fix**: `std::atomic<bool>`; after `exec()`, loop `ProcessAppOpers()` until the future is ready.

#### QT-A20 · BUG · LIKELY · Low · S — `showStatisticsDirectly` checks the fail state of the current item instead of the requested one
- **Where**: `DmsMainWindow.cpp:1761-1785` (calls `openErrorOnFailedCurrentItem`, :421-430); callers :2110, :2159.
- A statistics request from a table-view column opens the current item's error, and proceeds for a failed column; `tiContext->PrepareData()` is unguarded in a slot; `openErrorOnFailedCurrentItem` does not null-check.
- **Fix**: check `tiContext` itself and wrap the body in try/catch.

#### QT-A21 · SMELL · Low · S — `QDmsViewArea` construction failure skips the host detach; `CloseWindow` comments are wrong
- **Where**: `DmsViewArea.cpp:323-333` vs :451-457; :325, :457 (`CloseWindow(...) // calls SHV_DataView_Destroy`).
- When `AddLayer` throws, the DataView keeps `this` as host and status-text client until WM_DESTROY; Win32 `CloseWindow` minimises, it does not destroy.
- **Fix**: in the catch, `SetViewHost(nullptr)`, `SetStatusTextFunc(nullptr)`, `DestroyWindow`; correct the comments.

#### QT-A22 · SMELL · Low · M — The state-change callback runs on calculation threads and reads GUI state unsynchronised
- **Where**: `DmsMainWindow.cpp:2131-2186` (`AnyTreeItemStateHasChanged`), fed from `TicCalcSupport.cpp:256-270` via `SetProgress` / `DoFail`.
- `getCurrentTreeItem()` (a `shared_ptr` the GUI thread reassigns) and `m_active_detail_page` are read from workers: a data race.
- **Fix**: marshal the tail with `QMetaObject::invokeMethod(…, Qt::QueuedConnection)`.

#### QT-A23 · PERF · CONFIRMED · Low · S — QObject leaks: a delegate per configuration load and actions per Window-menu open
- **Where**: `DmsMainWindow.cpp:1663` (`setItemDelegate(new TreeItemDelegate(m_treeview))` per load), :2371-2398 (removed actions never deleted; a new separator each time).
- **Fix**: create the delegate once; delete the removed actions or use a `QActionGroup`.

#### QT-A24 · PERF · CONFIRMED · Low · M — The event log keeps every message forever and calls `currentDateTime()` per message
- **Where**: `qtgui/exe/src/DmsEventLog.cpp:328-368`, `DmsEventLog.h:57` (`std::vector<MsgData>`, `int` index).
- Filtered-out trace lines are retained too, without a cap; every `refilter()` rescans all; `QDateTime::currentDateTime()` does a local-time conversion per message.
- **Fix**: a capped ring buffer (keeping followup groups intact); `QElapsedTimer`.

#### QT-A25 · PERF · SMELL · Low · S — ValueInfo and Statistics browsers re-render their HTML on every retry
- **Where**: `DmsSmallWindows.cpp:580-605` (`restart_updating` → `singleShot(0)`), `StatisticsBrowser.h:24-33`, `DmsValueInfo.cpp:150-167`.
- **Fix**: `setHtml` only when done or changed; back off to ~200 ms.

#### BAT-A26 · PARTIAL · CONFIRMED · Low · S — Test launchers leak environment; the .c flavour lacks parts of the battery
- **Where**: `batch/TestReleaseUnit.bat`, `TestDebugUnit.bat`, `TestGlobio*Unit.bat`, `TestCMake*Unit.bat`.
- None has `setlocal`; `geodms_rootdir` then silently steers later hand-run tst scripts. `TestCMakeReleaseUnit` stops at the first failure where its twins run everything; neither CMake launcher runs the XML round trip, and `TestCMakeDebugUnit` runs no testcases battery.
- **Fix**: `setlocal`; align the CMake launchers (can be folded into cleanup-list item 13).

#### QT-A27 · PARTIAL · CONFIRMED · Low · S-M — Placeholder features and never-implemented commands
- `DmsActions.cpp:247-250` (Invalidate: Ctrl+I, no slot, in no menu, yet enabled / disabled at `DmsMainWindow.cpp:440, 482, 2336`); :323-326 (Process Schemes); `main_qt.cpp:243-245` (`miExportViewPorts` accepted and ignored); `DmsMainWindow.cpp:1987-1989, 2019-2023` (`IsPostRequest` always false, `DMS_ProcessPostData` unreachable); :2049-2050, :2076-2084 (`EditPropValue` / `PopupTable` do nothing; the `dms:edit` generator is commented out at `XmlTreeOut.cpp:400-401`); `DmsDetailPages.cpp:296, 380-381` (`ready` always true: a detail page is never re-scheduled); `TestScript.cpp:446-488` (Linux SEND handles codes 1, 3, 4 only; Windows forwards any code); `DmsToolbar.cpp:201-203` (zoom icons as placeholders).
- **Fix**: implement or delete each; route Linux SEND codes 5-18 to the methods Windows `WmCopyData` calls.

#### QT-A28 · SIMPLIFY · CONFIRMED · Low · S — Dead code in qtgui (grep-verified)
- `DmsMainWindow.cpp:562-569` `removeTreeItem` (only caller commented out, :2139) and `DmsTreeView.cpp:947-974` `removeItem`; empty `onFocusChanged` connected to `focusChanged` (:231, :773); `onHeaderSectionClicked` on a hidden header (`DmsTreeView.cpp:818, 983`); `QDmsMdiArea::testCloseSubWindow` (`DmsViewArea.cpp:269`); `RegisterScaleChangeNotifications` / `UM_SCALECHANGE` (:410-413, :456; "never received" per `main_qt.cpp:340`); `QDmsViewArea::nativeEvent`'s `WM_QT_ACTIVATENOTIFIERS` loop (:475-495; a latent hang if `this` were not in the list); `DmsDetailPages` `m_current_width`, `leaveThisConfig`, `update()`; the `DmsRecentFileEntry` Tab branch (`DmsMainWindow.cpp:737-749`) and a `toggled` connection on a non-checkable action (:1944).

#### PY-A29 · SIMPLIFY · CONFIRMED · Low · S — `python/dll/create_dms_wheels.py` is stale and unused
- It copies `geodms.pyd` (gone since #1105's ABI-tagged names), hard-codes `C:\Users\Cicada\…`, version 14.17.3, a VS2022 14.39 `cl.exe` and the dropped `BOOST_SPIRIT_THREADSAFE`; `setup.py` has no version. Nothing references it.
- **Fix**: delete, or rewrite against `PythonVersions.txt` and the tagged outputs.

#### REPO-A30 · SIMPLIFY · CONFIRMED · Low · S — Repository root hygiene
- **Untracked, ignored**: about 85 files in the root: `build_*.log` (26 of them `build_debug_stdptr*`), `msbuild_*.log`, `test_*.log`, `testdebugunit_*.log`, `build_after_chain.ps1`, `build_qt_after_shv.ps1` (hidden by a broad `build*.ps1` ignore rule that would also hide a real script), `filelist19.2.0.txt`; the setup scripts themselves write `filelist<ver>.<f>.txt` into the root (`BuildSignAndCreateSetup.bat:~196`). All against the AGENTS.md `scratch/` rule; TECH_DEBT_REVIEW §8 counted about 30.
- **Tracked, orphaned**: `RunIssue1100Tests.ps1` (one-off #1100 harness, hard-coded paths, a 19.2.0 reference exe), `RunLinuxGUITests.sh` (unreferenced).
- **Tracked design notes without inbound links**: `SaveLoadDesktop_findings.md`, `Transformation_complexity_plan.md` belong in `doc/development/`; `TECH_DEBT_REVIEW.md` and `RECURSION_REFACTOR_PLAN.md` are referenced from `doc/` and can move with their links. `analyze.bat` and `LOC/` belong in `batch\` / `tools\`. `TestLinux*.sh` stay only because `BuildSignAndCreateSetupLinux.bat:149` calls them at the root.
- **Fix**: delete the ignored artifacts and route logs to `scratch/`; narrow `build*.ps1` in `.gitignore`; move or delete the orphans.

#### BAT-A31 · BUG · CONFIRMED · Medium · S — `analyze.bat` never returns its exit code
- **Where**: `analyze.bat:63`: `endlocal ^& exit /B %RC%`.
- The caret escapes the `&`, so the line is one `endlocal` command and `exit /B` never runs; the script exits with the last `findstr`'s errorlevel (0 when PREfast warnings exist, 1 when there are none). The msbuild result and `LINT_RC` (the #1227 / #1233 lock checks) never reach the caller; so since a63315b0b.
- **Fix**: `endlocal & exit /B %RC%`.

#### NSI-A32 · BUG · CONFIRMED · Medium · S — The .m / .g uninstaller leaves `prelude.dms`, `library\**` and `examples\*.dms` behind
- **Where**: `nsi/DmsSetupScript.nsh:286-291` (`Delete $INSTDIR\library\geometry`, `…\basedata_nl\rdc`, `…\basedata_nl`, `Delete $INSTDIR\examples` target folders, which NSIS `Delete` ignores); `prelude.dms` (installed at :40) is never deleted.
- The `RMDir`s fail, including `RMDir $INSTDIR` at :323, so every uninstall leaves `GeoDms<ver>.<f>\` behind. The cmake .nsi (:294-300) uses the correct `\*.*` form (but also misses `prelude.dms`).
- **Fix**: copy the .c uninstall lines, add `Delete $INSTDIR\prelude.dms` to both; longer term let the cmake .nsi include the .nsh, as the globio one does.

#### BAT-A33 · BUG · CONFIRMED · Low · S — The shipped `testcases/run_roundtrip.bat` / `.ps1` lack their siblings' fallbacks
- The .bat has no `%~dp0..\..\GeoDmsRun.exe` fallback, the .ps1 (:21) no TEMP fallback; run from `<install>\examples\testcases` they stop with "not found" or fail to create `_out_rt` under Program Files.
- **Fix**: copy `run_testcases.bat:15` and the TEMP fallback of `run_testcases.ps1`.

#### BAT-A34 · BUG · CONFIRMED · Low · S — The .m / .g setups wipe the build output before a prompt that can abort them
- **Where**: `batch/BuildSignAndCreateSetup.bat:98` (`rmdir`) before the drift-check CHOICE at :115-119 (N → `goto :eof`); same order in `BuildSignAndCreateSetupGlobio.bat:56` vs :58-62. The .c script has the right order.
- Answering N leaves `bin\Release\x64` deleted and nothing rebuilt.
- **Fix**: move the `rmdir` after the drift check; `goto :build_failed` in .m.

#### BAT-A35 · BUG · CONFIRMED (quoting) / LIKELY (effect) · Low-Medium · S — The .m setup installs over the old folder
- **Where**: `BuildSignAndCreateSetup.bat:188-190` (the CHOICE message nests quotes and its answer is ignored); unlike .c (:205-209) and .g it never runs the old uninstaller, so a DLL dropped from the .nsh survives and the post-install suite misses it. makensis (:176), signtool (:184) and the installer (:190) are unchecked.
- **Fix**: `"%INSTALL_DIR%\uninstaller.exe" /S _?=%INSTALL_DIR%` first; `if errorlevel 1` checks.

#### BAT-A36 · BUG · LIKELY · Medium · S — Linux signing can leave the previous `.p7s` and report success
- **Where**: `nsi/CreateLinuxSetup.sh:451-471`: no `$ErrorActionPreference='Stop'`, and the old `.p7s` is not removed; with no token or a cancelled PIN on a same-version re-run the final `Write-Host (Get-Item ...).Length` succeeds on the old file, so a new `.sha256` ships beside a stale signature.
- **Fix**: `rm -f "$SIG"`, `$ErrorActionPreference='Stop'`, and check `[[ -s "$SIG" ]]`.

#### BAT-A37 · BUG · LIKELY · Low · S — `Build.bat:30` calls `GeoDmsVersion.cmd` by bare name, unchecked
- 39c0b5fed fixed every other bare-name call; under `NoDefaultCurrentDirectoryInExePath=1` this one fails, `GEODMS_VERSION_HEADER_DONE` stays unset and every flavour rewrites `buildstamp.h`. `GeoDmsVersion.cmd:54-56` ignores a failing `call :write_buildstamp`; `Build.bat` has no `setlocal`.
- **Fix**: `call "%~dp0GeoDmsVersion.cmd"`, check `DMS_VERSION_MAJOR`, `call :write_buildstamp || exit /B 1`.

#### NSI-A38 · SIMPLIFY · CONFIRMED · Low · S — Dead installer and test files
- `nsi/DmsSetupScriptW32.nsi`: unreferenced; includes the .nsh with a nonexistent Win32 bin folder, and as flavour "m" would collide with the x64 install folder and registry key. `RunLinuxGUITests.sh`: unreferenced, hard-codes `/mnt/c/dev/tst`, duplicates what `unit_linux.sh` runs. The Test*Unit launchers pass "off" / "on" as the flavour (`GeoDmsFlavor=off`, aggregates `vR64.off_*`); harmless, only `g` is special-cased.

**Tracked-item status (QT, PY, RUN, scripts)**

- **QT-59, QT-57, QT-58, QT-67, QT-56/60/61/62/63/65/69** and the R1 renames: fixed.
- **cleanup-list item 1** (Linux overrides) is marked done but is only partly done: the in-session clear is fixed, but on Linux a non-session override still lives only in the session cache (`DmsOptions.cpp:913-916`), although `GetRegConfigSetting` reads the ini store (`AbstrStorageManager.cpp:179`), so it is lost at exit; ini values are invisible to the dialog (`hasRegistryOverride = false`, :809, :834, :889).
- **Items 2, 3, 7, 8, 10, 13, 16**: open. `MainWindow::TheOne()` calls rose to 165. For item 8: on Linux `-x` is rejected as unknown while every real option needs `/`, which collides with absolute paths starting `/L`, `/T`, `/S`, `/C` (`main_qt.cpp:87-126`, `MainRun.cpp:200-219`, `Environment.cpp:2798-2800`); GeoDmsRun's usage marks items optional, but a configuration alone exits 2 (`MainRun.cpp:164, 221-225`).
- **TECH_DEBT_REVIEW §6** ("no CI"): still true; `.github/workflows` holds only `jekyll-gh-pages.yml`. **§8** (hygiene): open and worse.
- **AGENTS.md:168** ("every moved script re-roots"): wrong for `batch/run_unit.bat` (BAT-A17).

## 5. Documentation to update, merge or archive

Collected from section 3 and from the tracked-item status paragraphs of section 4.

| Document | Action |
|---|---|
| `doc/code-fixes.md` | Status line naming what closed since 2026-09-06 (SHV-53, #1249, the dumper defects). Correct the statuses the code contradicts: GEO-32 (GEO-A09), GEO-36 (`Canyon.cpp:176-177`), RTC-12 / RTC-C14 / TIC-10 (the ledger calls outside `try`, section 4.3), STG-14 (refuted on a false premise, STG-A10), RTC-C13 (the keys are `"lt"`, `"amp"`), the Appendix-A entry for `XmlParser.cpp:256` (live: INF-A01), :53 (diversity is offset from `range.first` since eddf106f2), :679 (dyna_point `carry`, still open). Then move the remnants to cleanup-list and archive |
| `doc/cleanup-list.md` | Make it the single live backlog (PLN-A12). Item 1 is only partly done (Linux overrides are lost at exit, section 4.9). Item 14 misses `GetGeosNonDPointDeprecationFlag` (GEO-A46) and the 48 deprecated bp names (PLN-A14). Item 11 is done on dead code (INF-A13) |
| `RECURSION_REFACTOR_PLAN.md` | Seven reverted commits listed as landed; stale paths and build steps; move to `doc/development/` (PLN-A01) |
| `doc/performance-test.md`, `doc/development/schedule-with-lookahead.md` | A banner that the #1259 deferral was removed (a7127224f, 92eaa7150); §2.2 :91-96 (the floor of one is not a progress guarantee: TIC-A02) and :93-94 (the deferral); header "no code changes yet" → a phase table; the ≈ 2,100-line §8.1 log to an archive file (PLN-A07, PLN-A08) |
| `doc/release-notes/` | `release-notes-20.22.0.md` recording the removal of the deferral and the ExplicitSuppliers change (PLN-A07) |
| `doc/development/storage-read-operators.md` | "S5 in progress" → done; :127-131, :236-239, :1165 describe `CreateItemWriter` and a non-FuncDC early return that went with #1248 |
| `doc/development/teardown-leak-and-ownership-cycles.md` | :91-94 says the tile-functor cycle is cut; OperAttrBin re-forms it (TIC-A10). Then archive |
| `doc/incremental-updates.md` | :119 `DMS_IsConfigDirty` (TIC-A14) and :23 `Renumber()` (INF-A14) described as working; #22 location moved to `TreeItemMetaInfo.cpp:938-978`; 13 stale `tic/dll/src` paths |
| `doc/issues.md` | Regenerate from GitHub; the #1165 row is decided except the brace-form migration (section 4.4) |
| `doc/deadlocks.md` | P11: the cache does not die with the data object but with its last holder. P18 is fixed for the dump, not for the properties page (TIC-A29) |
| `doc/development/operator-signature-interface.md` | :1180 (eq/ne `ConnectPointOperator`) is a wrong registration, not a describe issue (section 4.6 status); §12.3 is the root of CLC-A01 |
| `doc/bicriteria-impedance.md` | §7 "StartPoint_rel is never written" (implemented in 987730a64); §5.4.2 (the last origin is never checked, GEO-A29); §5.4.3 |
| `doc/development/crs-metric-decoupling.md` | Ruling 1 is done; stale comment at `clc/dll/src/OperUnit.cpp:410-422`; record the prj_snapshots result |
| `doc/development/dms-dissolve-single-noding.md` | "uncommitted" → f136489d7; the one-element shortcut exists for single rings only; the Prepare hook is not implemented |
| `doc/development/boost-format-to-std-format-migration.md` | Status line: stages 0, 1, 3 done; 2 and 4 open |
| `doc/development/tu-reorg-and-export-surface-2026-08.md` | Row D (the TreeItem / AbstrCalculator split happened in 821d19459); :31 (RunDllProc is unreachable) |
| `doc/development/header-hygiene-2026-08.md` | :74 (`SingleLinkedTree.h` is dead after all); then archive |
| `doc/development/g8-todos.md` | 66 markers in 35 files; re-pin anchors (several point past EOF) |
| `doc/tile-data-retainment.md`, `doc/function_serializer.md` | Redraw §2 after U4 and fix observation 2; drop the `fn_test_prelude` limitation (PLN-A19) |
| `PORTING_STATUS.md` | Trim to three items; Windows lost the pen / font caches too (SHV-A07); merge into `doc/linux/` |
| `Transformation_complexity_plan.md`, `SaveLoadDesktop_findings.md` | Phase table; record the (a)/(b) decision (PLN-A15); both to `doc/development/` |
| `TECH_DEBT_REVIEW.md` | Move #1, #6, #7 to cleanup-list; archive |
| Merge into one ownership reference | `std-ptr-migration-plan.md`, `stdptr-migration-handoff.md`, `ptr-safety-review-2026-07-02.md` |
| Merge into AGENTS.md and the skills, then archive | `testing-strategy.md`, `build-tips.md`, `dev-environment-gotchas.md`, `doc/development/README.md` |
| Archive as completed or historical | `compile-time-refactor-analysis-2026-07.md`, `k11-container-types-scope.md`, `doc/FutureTileFunctor notes.txt`, `doc/MT2-issues.txt`, `doc/include-tree.txt` |
| Re-pin anchors only | `typed-hof-remaining-work.md`, `typed-hof-language-design.md`, `type-declaration-forms.md`, `operator-signature-interface.md` |
| `AGENTS.md:168` | "Every moved script re-roots" is not true for `batch/run_unit.bat` (BAT-A17); fix the script rather than the sentence |

## 6. Suggested delivery order

1. **Memory safety and hangs.** The S rows of section 2 that crash, corrupt or hang: GEO-A01 to A05,
   GEO-A10, SHV-A01, PY-A03, STG-A01, STG-A05, INF-A01, INF-A02, QT-A01, QT-A02, CLC-A01, then the
   remaining `dms_assert`-as-guard items (TIC-A24, TIC-A32, GEO-A17, GEO-A21, STG-A09, STG-A22). One
   commit per item; a battery case wherever a configuration reaches it.
2. **Silent wrong results.** RTC-A01, RTC-A02, CLC-A02 to A12, GEO-A07 to A09, GEO-A11 to A34,
   STG-A03, STG-A04, STG-A11, STG-A34, TIC-A34, STX-A14, STX-A15. These change outputs that
   configurations may depend on: each needs a wiki note, and the family rule of theme 3 should be
   written first so the fixes agree with each other.
3. **Scheduler, locks and lifetimes.** TIC-A01 to A13, TIC-A28, INF-A06, SHV-A02, SHV-A03. They are
   timing dependent: verify with the unit suite and full.py, not with the battery alone.
4. **Release pipeline and documents.** BAT-A09 to A11, BAT-A31, BAT-A34 to A37, NSI-A32, PLN-A06,
   PLN-A07, and section 5 in one documentation sweep; then adopt the rule that a status line changes in
   the commit that changes the status (PLN-A08).
5. **Performance.** GEO-A35 to A45, GEO-A53 to A55, GEO-A59, STG-A15, STG-A16, STG-A25, STG-A30,
   SHV-A07, SHV-A08, SHV-A14, TIC-A06, TIC-A12, TIC-A13, CLC-A17 to A21, CLC-A30, QT-A14. Measure each
   before and after, as `doc/performance-test.md` does.
6. **Simplification.** First delete the dead code, one commit per module (RTC-A18, INF-A13 to A17,
   TIC-A21, TIC-A38 to A40, CLC-A28, GEO-A48, GEO-A56, STG-A31, SHV-A21, QT-A28, PY-A29, NSI-A38,
   PLN-A16, REPO-A30); then the structural items, each on its own: the Environment.cpp split
   (PLN-A13), the `WeakPtr` rename (PLN-A05), the ViewHost boundary (SHV-A20), the config / cache C2
   rename (PLN-A10), the `DataArray` substitution (PLN-A11), the polygon back-end consolidation
   (PLN-A14), and the scheduling of C1b (PLN-A02).

## Appendix A. Refuted candidates

Checked and found correct at this HEAD; listed so they are not re-audited.

**RTC core**: `my_vector::insert` / `emplace_back`, `SharedStr::resize`, `OwningPtrSizedArray::shrink`, `SharedArrayPtr::operator[]` (the recent fixes are correct); `BitVector` insert / push_back / erase / resize and `bit_sequence::set_range` / `fast_zero` (a `bit_sequence` starts block-aligned); `raw_move` chunking; `my_allocator::capacity_for` vs `deallocate`; `SharedArray::erase` / `BitVector::erase` overlap (non-const iterators select `std::copy`); `SA_Reference::push_back(value)` (captured by value); `allocateSequence` and `mappable_sequence::reserve` (keep old storage alive); `substr` overflow; mapped / memo `ReadBytes` past the end (EOF-fills by design); UInt64 stream compression; `IndexedStrings` self-range insert.

**RTC infrastructure**: the `buffer[n]=0` after `myFixedBufferWrite` (callers allocate size+1); a race on `TriggerOperator`'s `s_buff`; `dms_combinable::local()` after a rehash (`unordered_map` references are stable); `RepeatedDots`' static vector (meta thread only); concurrent `StaticRegister` (static init); a deadlock from nested shared locks in `GetRelativeName` (reader preference; perf only, INF-A06); `garbage_can` relocation (RTC-36 fixed); the 0 ↔ 1 race of `Inc/DecInterestCount`; Windows `Utf8_2_wchar` sizing; `ConvertDmsFileNameAlways` mutating a shared name (private copy first); `as_item_name` compaction; `DMS_GetLastErrorMsg` returning a temporary; `DmsException::what()` lifetime; `RegistryHandle::ReadString` (RTC-33 fixed).

**TIC (calculation and scheduling)**: UB in `CalcResultWithValuesUnits` / `ApplyMetaFunc_GetArgs` for a DC failed on meta info (fail types are ordered, `WasFailed(FailType::Data)` covers it); null `res` from `CalledCalcHandle` in `EvaluateExpr` / `VisitImplSuppl` / `GetErrorSource` (`FencedInterestRetainContext` blocks suspension); `tile_read_channel` stopping at an empty tile (`TiledUnit` rejects empty tiles); `JoinSupplOrSuspendTrigger` after a cancelled supplier (the waiter ends cancelled); `collectTaskImpl` dropping a scheduled context without interest (unreachable while a waiter keeps `m_KeptArgItems`); `CollectOperationContextsImpl` freeing garbage under the lock (declaration order releases the lock first); `FindOper`'s `nr_best_match`; the floor-of-one brake for non-joining consumers; leftover #1259 deferral scaffolding in code.

**TIC (tree, metadata, type system)**: `LinkSignatureBinding` reading `typeArgs` out of bounds (every caller checks the sizes); `IsFunctionResultMetaCall` with a null group (`FindName` falls back to `theTemplGroup`); `CdfPropDef`'s raw read resolving during a dump (gated on the stored value); `XmlElement::m_Parent` dangling (documented, no reallocation while open); `LispCalcExplanation` on a null data item; `EvalDeclaredSizeRule` with a null future; `DMS_TreeItem_GetExpr` returning a dangling `c_str`; `ExplicitSuppliersPropDef`'s raw read; the assert-then-if at `TreeItemDataUsage.cpp:186` and `Unit.cpp:993` (different conditions).

**CLC, STX**: `pcount` / `has_any` with a null inside the values range (counted as a value, consistent with 29b7ec449); partition aggregations reading only tile 0 (none do); `GetTile(0)` in OperAttrVar (void-domain parameters only); `assert(p < pCount)` in `WeightedModusPartBySet` (IndexGetters map out-of-range to undefined); `GetRefObj` in GetStatistics (meta thread only); the sticky `m_CanContainNulls` in overlay; the AggregateTiles thread split; scalar data-block narrowing (`ThrowingConvert`); `uint64_p` overflow (a parse error); matr_* tiled order; the shared `dms_transform` functor (copied per block); multi-tile `GetDataWrite(no_tile)`; UrlDecode's trailing `%`; a shared `boost::regex` across tasks; the unary rlookup split of 0929ad45d.
STX, second pass: `1e3` without a dot (an exponent satisfies `expect_dot`); `&*first` / `&*last` in `ProdIdentifier` (address only); `[&]` lambdas capturing `currDBP` / `cp`; a throwing `ConfigurationFilenameLock` constructor; stale Dijkstra option flags after backtracking; the `GetDataRead()[0]` temporary in `DataArrayOperator`; re-entering the `ConfigurationFilenameContainer` singleton; hex literals absorbing `b`/`c`/`d`/`f` and `4294967295` becoming null (both documented on the wiki `Literal` page).

**GEO**: reading only tile 0 in Dijkstra, TraceBack, DiscrAlloc, district, diversity, perimeter, connect's indexed side or the potential kernel (`GetDataRead()` defaults to `no_tile`, a whole-array shadow); overflow of the dense OD size (`SizeT`, `ThrowingConvert`); null or out-of-range F1/F2, nodes or zone relations in Dijkstra (`CheckNoneMode` throws, except the non-zero-base case GEO-A20); Dijkstra heap decrease-key (lazy deletion is correct); races in `thread_scratch` / `dms_combinable` and the per-origin loop; tile-local ids written as global in overlay / connectivity / partitioned union; Int128 overflow in the dms `CrossingSweep` (bounded by the 2^36 lattice checks); `SpatialIndex::Rebuild` invalidating leaf pointers; division by zero on zero-length segments; `join_near_values_uint8` count overflow; the `join_equal_values` dense path; GEOS factories per element or leaked GEOS handles; untranslated CGAL exceptions; races on `ScanPointCalcResource` in `mid`; undefined cells or recursion depth in district; degenerate polygons in poly2grid; `raster_merge`'s raw index (documented); `PotentialDefault` (alias of `PotentialFft64`); Voronoi on degenerate input; narrow node/zone types in Dijkstra (refused by `const_unit_cast`); the pareto_optimal sweep and epsilons.
Search family: ConnectNeighbour's `dms_assert(pointData.begin()==destBegin)` on a tiled domain (the cached shadow tile); tile-local vs global index mix-ups in ConnectInfo / FastConnect; the SpatialIndex range iterator skipping quadrants; the filtered forward arc search not terminating; a BoundingBoxCache build race; the ConnectPointOperator progress timer (atomic); FastConnect duplicate in-segment cuts (a harmless zero-length tail).

**STG**: `DoCheck50PercentExtentOverlap` with negative Y factors (`Range` normalises); `ShpPolygonHeader::Write` padding (`#pragma pack(push,4)`); per-tile transaction cost for Shapefile / GeoJSON (only OpenFileGDB emulates); a race on `gdalThread::s_TlsCount` (thread-local); shp MultiPoint `nrExtraParts` underflow; an empty shapefile part reaching `IsRingClosed`; `SetField(i, size, const char*)` hitting the binary overload; `StrStorageManager` using the file index as tile id; ODBC `GetActualSizeEstimate` out of bounds today (only reached when `recordCount == 1`); STG-20 (`GetFieldAsString` lifetime).

**SHV**: a second owner from a `shared_ptr` built on a raw pointer (none; draggers take proper shared pointers); an `HRGN` leak in `GdiDrawContext` region calls (`RegionToHRGN` owns); `CopySelValues` / `PasteNow` / `SelectRegion` on tiled grids (`no_tile` gives a range-ordered shadow tile); `~DataView` → `RemoveAllControllers` touching the dying view (owners are locked); `PasteGridController` using freed `m_SelValues` (every use goes through the locked layer); `WM_COPYDATA` overread; Ctrl+L zoom-all not redone when extents arrive (unchanged behaviour); `GridCoord` update cost; the `RoiCaret` DPI TODO; the `DataView::OnPaint` recursion guard (needs a nested `WM_PAINT` no path produces); `PieLayer::DoInvalidate` inside a draw (same design as `FeatureLayer`); `MovableObject::CopyToClipboard` deleting the `HBITMAP` after `SetClipboardData` (against the contract, but works in the GUI tests; not pursued). Not covered in depth: FeatureLayer label / symbol internals, AxisControl / ChartControl, ItemSchemaView, TextEditController.

**QT, PY, RUN, scripts**: `DmsConfigOptionsWindow` writing to HKLM (it opens `HKCU\Software\ObjectVision\<machine>\GeoDMS`); the native Ctrl+W filter closing a map while a ValueInfo window has focus; `StartEditor` passing a runtime string as a `reportF` format (`mgFormat2string` catches `format_error`); the event-log link `string_view` not NUL-terminated (cosmetic); the Python `Config` leaving `currSingleConfig` dangling on a null root (`CreateTreeFromConfiguration` throws); exception-model drift between CMake and msbuild (both `/EHsc`); run/exe and python/dll source-list drift (identical); `activeDetailPageFromName` prefix matching; `%G_VER%` / `%SDD_LM%` expansion in parenthesised blocks (set before the block); cross-thread `QTimer::singleShot` with a context object. Not read line by line: `tools/*.ps1` and the other `BuildSignAndCreateSetup*` bodies.
Scripts, second pass: delayed expansion in `TestGlobioReleaseUnit` / `TestShippedContent`; `_out` folders leaking into NSIS; the `v%1*` pattern in `run_unit_suite`; `LocalDataDir` propagation into the setup gates; the gating `tools/*.ps1` (all set `$ErrorActionPreference='Stop'` except the advisory drift check). Every file named in `DmsSetupScript.nsh` exists in `bin\Release\x64`; the .c and .g NSIS lists were not checked (no output folders on disk).

