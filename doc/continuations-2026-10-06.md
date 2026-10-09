# Which continuation pays most: a meta list over the audits and plans (2026-10-06)

*Status (2026-10-06): written at `dcba9d9e0` (main, 20.22.1, still a pre-release). Five rows landed the
same day: A1 (the full.py round at 15651313f, 6bdbc0911) and A2 (#1284, 9a1182a1e) from other
sessions, then A3 (7cd3d09ac), A6 (0563aa5a0) and A11 (the documentation sweep, `git log --grep=A11`); in the evening A5 except STG-A12, and from A7 and A9 TIC-A33 and SHV-A08/A09 (0d99c9f0f..3100f7e99), and after it B7 (4639608b1..92e3a541e); on 2026-10-07 B11; on 2026-10-08 A4, its full.py round still to run, and STG-A12, which completes A5; on 2026-10-09 the rest of A7. When a row lands, strike it here and name the commit, as AGENTS.md asks of every
plan document.*

This is a ranking, not a new audit. It reads the ~40 audit and plan documents in `doc/`,
`doc/development/` and the repository root against the git history since each was written (199
commits since the audit of 2026-09-27 alone), and asks of every still-open item: what does
continuing it buy in **code clarity (C)**, **performance (P)** and **stability/correctness (S)**,
and for what effort.

Method. Nine read-only review agents each took one group of documents, mapped every finding,
phase or proposal to the commits that closed it, and grepped the code for what is still open.
The claims that head this list were then re-checked by hand at `dcba9d9e0`: the lost error
messages of A3 (`Diagnostics.h:137,143`), `strict_ureal_p` (A4), the serial bag loop (A8),
`MemoryMaxRAM_GB = 64` (B4), the commented-out `madvise` (B4), `verify_none`/`sslv23` (B10), the
two `RTC_ParseRegStatusFlag` (B8), and the callers of `analyze.bat` (A6, none). One agent claim
did not survive: STG-A05 is fixed (6c6afba91). Nothing was built or run.

Scales: benefit **H / M / L** per dimension, with the evidence that sets it. Effort **S** under a
day, **M** days, **L** a week or more. "Result-changing" means a wiki note and a full.py round
belong to the same change.

---

## 1. Where things stand

| Document | Claimed | Actual at `dcba9d9e0` |
|---|---|---|
| `doc/code-audit-2026-09-27.md` (288 findings) | no status line | 123 fixed, 3 partial, 2 reverted (CLC-A07, GEO-A19), 3 dropped (INF-A06, GEO-A42, STX-A15), **157 open**: 1 High (PLN-A02), 34 Medium, 122 Low/Low-Medium. Delivery steps 1-3 done; step 4 done but BAT-A10 and most of §5; step 5 open on GEO-A44/A45, SHV-A07/A08/A14, QT-A14; **step 6 (simplification) not started**. |
| `doc/code-fixes.md` (~130 IDs; now `doc/archive/`) | phases 0-5 done, residues listed | All fixed or refuted except 6 remnants (RTC-70 §9, RTC-12/TIC-10, STG-14 `Int32 read_result`, dyna_point `carry`, INF-A05, `Actor.cpp:318`). Archive after moving those. |
| `doc/cleanup-list.md` (16 items) | 3 done, 4 partial | 2 done, 4 partial, 9 open; item 1 (Linux options persist only in session) and item 5's note are wrong. |
| `TECH_DEBT_REVIEW.md` (now `doc/archive/`) | June, old branch | #1 recursion, #2 Win32 leakage, #4 Spirit V1, #6 CI open; red baseline fixed. |
| May security audit (memory only) | — | Also fixed: #13 (b55d2bb50), #14 (3d0db896e), #8, most of #23. **Open: #1/#10 WMS TLS, #3/#15 GDAL fallback and `/vsi*`, #7 WMTS size cap, #6 remainder, #9, #17-#22, #24.** |
| `doc/issues.md` | 9 open (08-29) | 7 of those 9 closed; 8 open issues missing. |
| `doc/deadlocks.md` | P1, P3, P4, B1 fixed | Also fixed: P5, P11-P19, B2; P8 superseded; P6 measured false. Open: P2, P7, P9, P10, nesting tables. Lock ordinals in the text are pre-e2c6033c. |
| `doc/Interest.md`, `interest-and-futures.md`, `incremental-updates.md`, `IntegrityCheck.md` | design notes | R1-R4 not started; `DetermineExternalChange` still commented out; #1202 general case open; B6, D18 done, E21 refuted. |
| `doc/development/schedule-with-lookahead.md` | P0, P1 done; P2 behind `/SQ` | Accurate, but P2's exit criterion was never met and four real-model studies show no effect; P3-P5 nothing. 2,180 of 3,278 lines are a dated log. |
| `doc/performance-test.md` | 20.22.1 "not releasable" | Overtaken by #1290/#1291 (tie fixes); the round at 15651313f (6bdbc0911, merged on 2026-10-06 after this survey) gives 27 of 27 ok with moved references. |
| `RECURSION_REFACTOR_PLAN.md` (now `doc/development/recursion-refactor-plan.md`) | 17 commits landed | 7 were reverted on 2026-05-22; C1b never started; recommends forbidden single-vcxproj builds (PLN-A01 open). |
| `doc/tile-data-retainment.md` | — | §2 class tree, §4.3 and §4.7 stale after U4, TIC-A10 and `materialization::spilled`. |
| `unit-hierarchy-collapse.md` | U1, U2, U4, U5 done | Correct; but the data side kept three names for one class (`DataArray<` 707 spellings and growing). |
| `config-cache-separation.md` | plan, no status | 0 of 7 stages; 821d19459 did half of C6. |
| std-ptr migration docs, `ptr-safety-review`, `teardown-leak` (now `doc/archive/`; successor `doc/development/ownership.md`) | complete / fixed / historical | Correct; raw borrows (`GetOld()` 57 sites) and the `WeakPtr` name remain. |
| `g8-todos.md` | 71 markers | ~63 real markers; starter batch untouched. |
| build docs (compile-time, header-hygiene, tu-reorg) | various | All ladders done. **`boost-format-to-std-format-migration.md` has no status line**: stages 0, 1, 3 done; 2 and 4 open. `build-tips.md` and `dev-environment-gotchas.md` contradict AGENTS.md. |
| typed-HOF docs, `k11`, `type-declaration-forms`, `function_serializer` | various | Feature work stopped 2026-07-29; remaining-work list partly stale (CRS, closure locals, alias type-vars are done). |
| `storage-read-operators.md` (#587) | S0-S5 done | Correct; #1284, a regression on this route, was fixed after this survey (9a1182a1e). |
| `dms-dissolve-single-noding.md`, `bicriteria-impedance.md`, `crs-metric-decoupling.md`, `transformation-complexity-plan.md`, `class-break`, `jenks-fisher` | implemented | Correct apart from small stale lines; follow-ups listed below. |
| `PORTING_STATUS.md` (now `doc/linux/porting-status.md`), `SaveLoadDesktop_findings.md` (now `doc/development/save-load-desktop-findings.md`) | April / June | Port merged and shipping; desktop-save decision still not taken (PLN-A15). |

---

## 2. The ranked list

### Tier A: small, high benefit, evidence in hand

Ordered by benefit for effort. Each is one commit or a small batch, with a testcase where a
configuration can reach it.

| # | Continuation | Sources | C | P | S | Evidence | Effort | Risk / precondition |
|---|---|---|---|---|---|---|---|---|
| A1 | ~~**Release baseline: a full.py round at HEAD and a GeoDMS-Test reference update**~~ DONE 6bdbc0911: the round at 15651313f on OVSRV05 gives 27 of 27 ok after GeoDMS-Test f7891d5 and f6fe770 moved the references (four of them by decision, as `point_in_polygon`'s visiting order); testcases 455/0 | performance-test.md (20.22.1 at 1315b0357); #1290, #1291 | | | **H** | The last recorded round has 9 red tests (t010, t060, t100, t101, t102, t301, t910, t2000, t641.2). Every #1290/#1291 commit says "Not run: full.py". 20.22.1 has no tag. `point_in_polygon` still takes its first hit, so t301/t060 may still differ. | S (machine time) | Done; A4 needs a round of its own. |
| A2 | ~~**#1284: a `subitem()` member of a `storage_read_table` under `for_each` is never read**~~ DONE 9a1182a1e: the request joins the attached operation until it is detached; testcase `stor_read_for_each_second_column.dms` | storage-read-operators.md; issue #1284; TIC-A04 (dbe256a84) | | | **H** | Regression in every release since 20.20.0; deterministic `Check Failed` at `OperMisc.cpp:287`; no commit and no testcase. TIC-A04 closed only the "while reading" case and itself names an open window (`Run_with_catch` releases the lock before `OnEnd(done)`). | S-M | OperationContext re-entry (#1167); add an offline CSV/gpkg testcase. |
| A3 | ~~**Error messages that lose their text**~~ DONE 7cd3d09ac: the eight sites fixed, `tools/check-format-args.ps1` checks every call with a literal format (1628) | boost-format migration stage 2; INF-A03; PLN-A04 | M | | **H** | `throwDmsErrF` takes `(format, args...)`, but `Voronoi.cpp:400` and `BoostPolygon.cpp:375` call it with `throwErrorF`'s `(context, format, args...)` convention, so the user sees just "voronoi" or "PolygonOverlayOperator". The surplus argument is dropped at `AggrFuncNum.h:50,55` and `AbstrStorageManager.cpp:644`. `polygon_arbitrary_formation.hpp:1748` uses printf `%d`. | S | None. B1 is what stops the class coming back. |
| A4 | ~~**Numeric literals and conversions are exact: STX-A14, CLC-A11, STX-A25**~~ DONE 2026-10-08: 4d4f7c640 (STX-A14), af38e4eed (CLC-A11) and the STX-A25 commit after them; its full.py round is still to run | code-audit §4 (held "until after the 20.22.1 full.py round") | | | **H** | `0.3` parses as 0.30000000000000004, and `[0.7,'0.7']` stores two different values (`ExprParse.h:369`, `DataBlockParse.h:170`). `string()` of a number depends on the thousand-separator option, so `/SH` and `/CH` produce different keys; `string(point)` is registered twice and static-init order picks the winner. STX-A25: `5000000000u`, `['300']` into uint8 and `[(40000,1)]` into spoint are silently narrowed, and in `a, b: …` the data block goes to `b` only. | S each | Result-changing: one wiki note and one full.py round after A1. |
| A5 | ~~**Input hardening batch for storage readers and raster/XML input**~~ DONE: 0d99c9f0f..3100f7e99 (GEO-A29, STG-A17 to A21, A23, security #6, #7, #9 with the STG-14 residue, INF-A05, RTC-A05, RUN-A12, SHV-A13); STG-A12 (the tif bit-size check) on 2026-10-08, with the expansions it had kept unreachable | code-audit STG-A12, A17-A21, A23, RTC-A05, RUN-A12; security #6, #7, #9; code-fixes STG-14 residue; INF-A05; bicriteria GEO-A29(c); SHV-A13 | | | **M-H** | Each is a crash, a heap overread or a silent truncation on ordinary third-party input: a point shapefile with a null shape is unreadable (`ShpImp.cpp:240`); a null colour table is dereferenced (`gdal_grid.cpp` ReadPalette); 32-bit dbf offsets break beyond 2 GB; narrow `TIFFOpen`/`rename` fail on non-ASCII paths (#1101 class); `UInt32 tile_wh` wraps (`GridStorageManager.h:153,535`); WMTS tiles have no size cap; XML numeric entities decode to NUL; `precalculated_NrDstZones` writes past `SetCount` on the last origin; `~GridCoord` erases a live registration. | S each (1-2 days for the batch) | Low; each fix is local. |
| A6 | ~~**Run the lock-discipline lint in the test gates**~~ DONE: `batch/run_source_checks.bat` runs the four checks from every `Test*Unit.bat` and setup script; its first run found a ceiling of 9a1182a1e declared over a whole function (135f1fc20) | deadlocks.md §3.9, §9.5; PLN-A06 | | | **M-H** | The three one-second syntactic checks live only in `analyze.bat`, which no launcher, setup script or skill calls (grep: 0 hits). The static pass names every P16-class site; P19-type edits change lock ceilings with nobody looking. | S | None. |
| A7 | ~~**"A question must not produce": the remaining raw reads that compute, and the readiness probes with side effects**~~ DONE 2026-10-09: TIC-A33 (33391fdc0), which also reset the size calculators in `DoInvalidate`; TIC-A14, the reset of `IsDataCurrCompleted` in assertions, the ledger calls (RTC-12/TIC-10), TIC-A29 and TIC-A35 in five commits, `git log --grep="A7)"` | deadlocks.md P18/R6; code-audit TIC-A29, TIC-A35, TIC-A14, TIC-A33; code-fixes RTC-12/TIC-10 | M | M | **M-H** | The properties page calls `HasNonDefaultValue` on computed properties (`XmlTreeOut.cpp:1058`): a Debug build exits 3, and Release computes them twice per render. `IsDataCurrCompleted` resets `m_DataObject` inside asserts, so Debug and Release diverge (two Debug stops on 2026-10-03). `DMS_IsConfigDirty` takes `sd_SessionDataCriticalSection` twice, which deadlocks on Linux. `DoInvalidate` does not reset `mc_SizeExpectation`/`mc_SizeUpperbound`. Ledger calls run outside `try` on a noexcept path. | S each | Low; same defect class as the four September fixes. |
| A8 | **Run dissolve phase B in parallel per slot** | dms-dissolve-single-noding.md; #1283 ("Fase B blijft serieel") | | **H** | | `dms_split_union_polygon(geometry, city_rel)` on NL31 takes 244 s, serially over 25 slots (`BoostPolygon.cpp:2021-2032`, one engine for all bags). The per-thread engines exist since a0800530c. | S-M | Results stay ordered per slot. Close #1283 in the same session (fixed, still open). |
| A9 | **GUI hot loops: SHV-A14 and SHV-A08/A09** (SHV-A08/A09 done, 3100f7e99; SHV-A14 open) | code-audit step 5 | | **M-H** | M | Grid-fill reuse is defeated for every tile not at offset 0 (about 100x the classify work when zoomed in, `GridFill.h:183` vs `:260`). Table loops re-prepare and re-lock per row, which takes seconds to minutes on 10M rows, and use an UNDEFINED row as an index. | S each | No result change. |
| A10 | **Write the K11 contracts in the DMS dump** | function_serializer.md; `TreeItemXmlDump.cpp:241-307` | | | M | By-example parameters and container member blocks are checked contracts since K11a-3.2/K11a-4. A dump and reload silently drops them, and neither round-trip battery notices because positives still compute. The XML writer already emits `paramex`. | S | Low. |
| A11 | ~~**One documentation sweep** (section 4)~~ DONE on 2026-10-06 (`git log --grep=A11`): status lines throughout, finished documents to `doc/archive/` with an index, plans from the root to `doc/development/`; section 4 says what was left | PLN-A08 rule; code-audit §5 | **M** | | | This survey met stale status lines in about 20 documents. Three of them steer the reader wrong: the recursion plan presents 7 reverted commits as landed and recommends a forbidden build, `issues.md` lists 7 closed issues as open, and `build-tips.md` contradicts AGENTS.md. | S | None. |

### Tier B: medium effort, high benefit

| # | Continuation | Sources | C | P | S | Evidence | Effort | Risk / precondition |
|---|---|---|---|---|---|---|---|---|
| B1 | **Compile-time format checking (`std::format_string`) on the ~460 sinks** | boost-format migration stage 2; INF-A03; PLN-A04 | M | | **H** | `std::vformat` silently drops surplus arguments, whereas Boost used to throw. A3 found one wrong-convention pair and three dropped-argument sites by grep alone. A consteval front-end makes the whole class a compile error. | M | C++23 in CMake (no `runtime_format`): keep a `vformat` escape hatch for the few runtime formats. Do A3 first. |
| B2 | **#1292: index queries scan every box straddling a centre line** | performance-test.md; #1289/#1290/#1291; issue #1292 | | **H** | M | `box_connectivity` of 2M tiling squares takes 40.2 s against 4.55 s spaced. A deterministic `point_in_polygon` costs 7x today (1.2 → 9.2 s), so it was not done; making it affordable ends the last tie that follows index order. `connect_info` lost 24 % at split threshold 16. | Step 1: parallel `box_connectivity` (S-M). Step 2: straddler lists per node or a loose quadtree (M). | Result order; tie testcases exist. A1 first. Do not grow the node struct again (GEO-A54 cost t410 11-14 s). |
| B3 | **MMD sequence pool: abandoned chunks and nondeterministic bytes** | issues #1280 + #1294 (one root: chunk relocation in `MappedFileHandle::allocChunk`/`ReallocChunk`) | | **M-H** | M | String and geometry attributes cost up to 2.0x their content, +5.5 GB on ~30 GB in NetworkModel_PBL. The `.seq` file ranges from 25.9 to 31.4 MB for the same content, so stores cannot be compared byte for byte. | M | Open mapped views; the Linux `allocAtEnd` path. |
| B4 | **Memory policy on large hosts and Linux** | schedule-with-lookahead-log §8.1.24, §8.1.36; performance-test "Known causes"; RTC-A06 | | **M-H** | M | (a) `MemoryMaxRAM_GB` defaults to 64 (`Environment.cpp:56`). On a 128 GB host every brake, trim and drain therefore fires at about 51 GB machine-wide. (b) Linux never returns freed stores: `madvise` is commented out (`FixedAlloc.cpp:310,342`). `.l` t641.2 takes 1:57:34 against 0:19:36 on `.m`, with commit capped near 110 GB under 137-170 GB live. (c) The non-power-of-2 store sizes of 87bdc62d1 hold 21-35 GB at t641's peak commit. | S to measure each (three A/Bs), S-M to change | Decide (a) on an A/B: more parallelism means higher peaks. (b) is a WSL cycle. |
| B5 | **Retire the `/SQ` admission gate (keep shadow mode at most) and the dead allocator/scheduler code** | schedule-with-lookahead-log §8.1.20/.33/.36; TIC-A18, A21, A25 | **H** | | M | About 800 lines, roughly 20 % of `OperationContext.cpp` (3,252 lines), plus flags in both Environment.cpp copies, registry rows and GUI checkboxes. Four real-model studies found no memory effect: t641_2 parked 124,184 operations for an identical 171.9 GiB, and the gate is a delay, not a limit. It has no testcase and caused two crash bugs in two months. Also dead: the ≥2 MB `release`/`recommit` branch, `MG_CACHE_COLLECTDATA` (~114 lines); `s_IsInLowRamMode` went with B7 (2ba5557d3). | S-M | Check first whether external users run `/SQ` (the BAG-Tools#2 reporter did); update the wiki page. |
| B6 | **Make a hang say what it waits for** | deadlocks.md B4/B5/P2/P7; incremental-updates E22; testing-strategy | | | **H** | Every real hang so far showed as silent 0 % CPU (P1, P15, #1227, #1144's MMD commit). The "progress watchdog" that `ItemLocks.cpp:117` relies on does not exist. `counted_mutex::lock` waits without a timeout. The stack-baton `std::async` in `UpdateMetaInfo` starts its child without the parent's thread-local guard state and blocks in an unchecked `future.get()`. Proposal: a wait registry at `WaitForTaskNotification`, `counted_mutex` and the hand-off; a stall report when no OC completes for N s; the hand-off passes or forbids guard state. | M | Low (reporting only). Retires or diagnoses P2, P7, B4, B5 and E22. |
| B7 | ~~**Delete dead code, one commit per module**~~ DONE 4639608b1..92e3a541e: about 6,200 lines in seven module commits (rtc, tic, the storage managers, clc and stx, geo, shv, qtgui and scripts), every `DMS_*`/`SHV_*` export and every `SM_Save` branch kept | code-audit step 6 (INF-A13, PLN-A16, SHV-A21, TIC-A21, A38-A40, CLC-A28, GEO-A48, A56, STG-A31, QT-A28, PY-A29, NSI-A38, REPO-A30); cleanup 6; g8 F; SaveLoadDesktop/PLN-A15 | **M-H** | | M | About 3,000 lines: `Win32ViewHost` (compiled, never constructed), `RunDllProc`, `domain_change_context` (never constructed, so g8 F's "silently wrong data" is dead machinery), `ApplyTopEnvFunc::operator()`, `ElementWeight`, `EasyRereadTiles`, `SingleLinkedTree`, unused scripts. Fixes have already landed in dead twins (SHV-41/52/55). The desktop-save decision would also retire 27 `SM_Save` branches. | S per module | Low. |
| B8 | **One `Environment.cpp` flag parser, and one CLI parser for GUI and GeoDmsRun** | PLN-A13; cleanup 5 + 8; tu-reorg row D | **M** | | **M** | 3,140 lines with 64 signatures defined twice (`RTC_ParseRegStatusFlag` at :695 and :2792), so a new `/S` flag must be added twice or one platform silently drops it; 15 commits to the file since 2026-08-17. On Linux `/L` collides with absolute paths, and an unknown `@cmd` is skipped silently. | M | Needs a `.l` build; static-init order of the registry section. |
| B9 | **#1205: divide overlay work by complexity** | issue #1205; GEO-A37 (f0efd51aa) | | **H** (that shape) | | Measured >10 h against 5.5 min (BBG). GEO-A37's blocks of 256 elements still leave a few hundred huge polygons on 1-2 threads. | M (vertex-weighted blocks) | The maintainer asked the reporter for feedback first. |
| B10 | **The last security P0s: WMS TLS verification, GDAL driver and `/vsi*` allowlist** | May security audit #1, #10, #3, #15 | | | **M-H** | `verify_none` and `sslv23` at `WmsLayer.cpp:133,158,180,530-536`: MITM'd tiles feed the GDAL decoders. Unknown extensions fall back to all drivers (`gdal_base.cpp:1808`). | S-M | Windows root store for OpenSSL; proxies; a wiki note for exotic formats. |
| B11 | ~~**SHV-A07: cache GDI pens, brushes and fonts**~~ DONE 2026-10-07: a GdiObjectCache in GdiDrawContext keyed by what each object is made from, wide dashed pens geometric again, QtDrawContext skips a repeated SetFont | code-audit; doc/linux/porting-status.md (`FontIndexCache`, `PenIndexCache`) | | M-H | M | About 3M GDI create/delete calls per redraw of 1M polygons (estimate); wide dashed and geometric pens draw solid. The dead `PenArray`/`FontArray` design, deleted with B7 (c0737bfe8), can be taken from the parent of that commit. | M | Measure one redraw first. |
| B12 | **Describe the operator families that still lack a signature, and run tst once in Debug** | operator-signature-interface §9; typed-hof-remaining §4 | M | | **M** | The audit's main bug class is twins that disagree. CLC-A01's out-of-bounds read was a missing shared-domain description plus a missing unify; once described, the Debug drift check catches it. About 34 clc/geo files have `CreateResult` without `DescribeSignature`. | M per batch | New Debug asserts (intended). |

### Tier C: large or strategic; start with the measurement that decides them

| # | Track | Sources | C | P | S | Why, and the first cheap step | Effort | Risk |
|---|---|---|---|---|---|---|---|---|
| C1 | **Recursion (PLN-A02, the only open High)** | recursion-refactor-plan; TECH_DEBT #1; cleanup 15 and 19 | M | | **H** | `SubstituteExpr_impl` has 13 call sites (it had 4 before the typed-HOF work); protection is the 64 MB reserve plus the 320 KB `std::async` hand-off, which nothing below full.py's t720 exercises. Steps: (1) a deep iterated-calc testcase (2-5k steps), a hand-off counter, and a WSL `ulimit -s 8192` / Python probe for PLN-A03 (S); (2) Spirit depth caps D2/D3 (S); (3) merge the triplicated function-application dispatch in `SubstituteExpr(_impl)` (M, a clarity gain on its own); (4) C1b, the fused iterative driver (L). | L overall | High: 7 sibling drivers were reverted in May. Do not propose removing `/STACK` before step 4 is validated. |
| C2 | **Object-model clarity** | unit-hierarchy-collapse U6; g8-todos; config-cache-separation; header-hygiene §4d | **H** | L | M | (1) U6a: one name for `DataArray<`/`DataArrayBase<`/`TileFunctor<` (707 + 119 + 104 spellings; the alias grew by 63 since August), batched with the g8 D/E forwarders and dead virtuals in one PCH rebuild (S-M, scriptable). (2) config-cache C1 (the latent assert at `TreeItem.cpp:84`) and C2 (`mc_DC`/`mc_RefItem` rename) (S). (3) C3-C7, then g8 C encapsulation (L). `TreeItem.h` sits in five PCHs and forced about 3,800 recompiles since 2026-08-17, and the C-series is the natural place to split it. | S → L | Merge conflicts with concurrent clc/geo work; full rebuilds. |
| C3 | **Memory-bounded concurrency across independent targets** | schedule-with-lookahead-log §8.1.37; performance-test | | **H** | | The only lever in the whole scheduling log with a large *measured* wall-time gain: t405.2 12:54 → 5:49, t641.1 51:50 → 24:11 under 20.20.0's deferral. That variant cost 346 GB live and was removed, together with two later variants. It needs a charge before start without `/SP`, the decision at producer scheduling, a release path, and an IntegrityCheck-safe start. Consumer synchronization (§4.4.1, inferred t641_2 wave 75-85 G → 15-20 G) is its memory counterpart. | L | High: three variants failed (#1181 backstop, t060 7 → 24 GB). |
| C4 | **Interest and futures: the R1 type split, then R2, then external change detection** | interest-and-futures §6; incremental-updates §3.4; Interest.md #1202 | **M-H** | | M | R1 changes no behaviour: a charged `FutureData` that only `CallCalcResult` can construct, separate from a plain retainer, makes the §2.5b table compiler-checked. It is the prerequisite for re-enabling `DetermineExternalChange` (commented out at `TreeItem.cpp:2483`; GUI users silently get stale data). Alongside: carry the real failtype through the two catch sites that stamp `Committed`/`Validate`, so an error is no longer demoted to an [E] followed by [W]s. | M → L | Medium (who relies on the current reset). |
| C5 | **Ownership residue of the std-ptr migration** | stdptr handoff #3-#5; ptr-safety review; PLN-A05 | M | | M | `GetOld()` (57 sites) returns a raw pointer taken from a temporary; `resultHolder->` (59) and `GetCurrUlt()` (12) also hand out raw pointers. `WeakPtr<T>` (92 uses) promises a liveness check it does not do. Record the §15.1/§15.2/§15.7 decisions (their premises no longer hold). | M | Low-Medium. |
| C6 | **An automated gate** | TECH_DEBT #6 | | | **H** (multiplier) | Only the jekyll workflow exists. A self-hosted Windows runner (or a scheduled local battery) running the testcases battery, the XML round trip and the A6 lint would have caught several of this month's twin regressions. | L | Pinned 14.50 toolset, vcpkg and the machine policy in AGENTS.md. |
| C7 | **Compile-time work, measured first** | compile-time-refactor-analysis; tu-reorg §6; boost-format stage 4 | M | | L | The last baseline (2026-08-16) is outdated: Geo objects grew from 373 to 791 MB, and new 50-100 MB stragglers appeared, among them `WindingOrder.obj` at 64 MB from a 41-line TU. First a fresh binlog baseline (S). Then a committed dead-export check plus folding `TICTOC_CALL`/`SYM_CALL` (S-M; export macros regrew by 96 lines), `<strstream>` out of `FixedBufferFormat.h` (S; removed in C++26, msbuild uses stdcpplatest), and CMake PCH parity for DmRtc (S). | S each | Low. |

Lower in the queue, still worth recording: the MMD source-version drafts of storage-read-operators
decision 12; the one-pass read for dbf/odbc (N columns = N scans); `storage_read_attrs`;
GEO-A44/A45; QT-A14 (tree model O(N²)); RTC-A07 (trivially copyable `Point`/`Range`, unmeasured);
SHV-A03 (WMS `io_context` on two threads); PLN-A17 (Python engine lifecycle); #1293
(`first_rel`/`second_rel` for a factor of a combined domain, which saves 11 hand-written decodes
in NetworkModel_PBL); #1276 (units in MMD); Jenks parallel `CalcRange` halves; class-break §6.

---

## 3. Through-lines: one change, many findings

1. **Twins drift apart.** The audit's recurring pattern was a fix applied to one twin only. The
   remaining twins are the two Environment.cpp flag parsers (B8), the dead copies that still
   receive fixes (B7), the triplicated function-application dispatch in `SubstituteExpr(_impl)`
   (C1 step 3), and operators whose description and implementation can disagree (B12). Merging a
   twin is a stability measure as much as a clarity one.
2. **Errors are silent where they should be located.** Lost format text (A3, B1), narrowed
   literals (A4), unchecked writes in the storage readers (A5), unknown `@commands` skipped (B8),
   a hang without a word (B6). Each conversion from silent to located reduces the cost of every
   later bug report.
3. **A question must not produce.** R2, R6, P17, P18, TIC-A29, TIC-A35, the reset in
   `IsDataCurrCompleted` and the bare `FutureData` retainers are one defect: a reader takes
   interest, resolves, resets or charges work. A7 removes the current cases; C4 (R1) makes the
   next one a type error. The Debug XML round trip is the runtime check, and it needs one more
   entry point that reaches the properties page.
4. **Guards are per thread, the remaining waits cross threads.** The level checker, the registry
   usage count and SuspendTrigger are `thread_local`; P2, P7, B4, the stack hand-off and the MMD
   commit hang are cross-thread waits. B6 gives them one place to be seen.
5. **Measure before building.** The scheduling log tried three deferral variants and an
   admission gate that measured flat on real models; the compile-time baseline is two months old;
   the recursion depth on real models was never measured. B4, C1 step 1, C3 and C7 each begin
   with a measurement that costs a day or less and decides whether the large step is worth it.

---

## 4. Documentation debt: status lines that misstate the state

Per the AGENTS.md rule, each needs a dated line naming what landed. A11 does them in one sweep.

*Done on 2026-10-06 (A11; `git log --grep=A11`): every row below, as written, with these differences.
Finished documents went to `doc/archive/`, whose README lists each with its reason and successor and
maps the old paths that code comments and testcase headers still cite; those comments were left
alone, since editing them costs a recompile or makes the shipped battery copy stale. The three
ownership documents and the teardown note became one reference, `doc/development/ownership.md`.
The development notes went into the skills (geodms-build, geodms-debug, geodms-commit), not into
AGENTS.md, and `doc/development/README.md` became a description of the folder. Plans at the root
moved to `doc/development/` (recursion, transformation, desktop save) and `doc/linux/`
(porting status). Not re-pinned: the line anchors of section 2 of the scheduling plan and of the
typed-HOF design document; their status lines say so. Found on the way: `batch/run_unit.bat` does
not re-root and passes `S1` where `unit_flagged.bat` now expects the flavour (BAT-A17), so the
AGENTS.md sentence that every moved script re-roots was false for it. Fixed the same day: it
re-roots and runs the unit suite through `batch/run_unit_suite.bat`, for any dev-tree selector.*

| Document | Fix |
|---|---|
| `RECURSION_REFACTOR_PLAN.md` | Mark the 7 reverts (a52f987b9, 5d8bc89af, 21cb802f2, de529f23e, e77506323, dce70dc29, 19ab9a351); delete the single-vcxproj build steps; move to `doc/development/`. (PLN-A01) |
| `doc/code-audit-2026-09-27.md` | Add a status line: the counts above; STX-A15 refuted; GEO-A51 resolved by #1290; GEO-A44/A45 deferral reasons. |
| `doc/development/boost-format-to-std-format-migration.md` | Add a status line: stages 0, 1, 3 done; 2, 4 open. |
| `doc/deadlocks.md` | Header lists P5, P11-P19 as fixed; update the lock ordinals (counted_mutex 95, not 99); `m_ItemLockCount`. |
| `doc/development/schedule-with-lookahead.md` | Archive §8.1 (lines 1035-3216); add a phase table. Fix `prioritize_impl` (deleted), #902/#1128 (closed), §8.1.35 (`/SP` is opt-in), and the claim that estimates exist "only under `/SP`". |
| `doc/performance-test.md` | Archive the #1259 half (:1-722). (The #1290/#1291 round was recorded in 6bdbc0911.) |
| `doc/tile-data-retainment.md` | Redraw §2 after U4; §4.3 has five sites, §4.7 five materialization values; TIC-A10. |
| `doc/development/config-cache-separation.md`, `unit-hierarchy-collapse.md` | Status lines: "0 of 7" and "U1, U2, U4, U5 done; U6 open". |
| `doc/development/g8-todos.md` | Recount (~63) and re-anchor. |
| `doc/development/crs-metric-decoupling.md` | "Pending prj_snapshots" is satisfied by every full.py round since 20.20. |
| `doc/development/storage-read-operators.md` | §1.3/§2.4 still describe the removed `CreateItemWriter`. (#1284 and TIC-A04 were added to the status line in 9a1182a1e.) |
| typed-HOF docs, `k11`, `type-declaration-forms.md`, `function_serializer.md` | CRS, closure locals and alias type-vars are done; `f: function` is rejected; anchors moved after 821d19459. |
| `doc/issues.md`, `doc/code-fixes.md`, `TECH_DEBT_REVIEW.md` | Regenerate `issues.md` from GitHub. Move code-fixes' 6 remnants into `cleanup-list.md` (PLN-A12: one live backlog) and archive the other two. |
| `doc/development/build-tips.md`, `dev-environment-gotchas.md`, `testing-strategy.md`, `doc/development/README.md` | Fold into AGENTS.md or the skills, or rewrite: they contradict the presets, the case renames and the `batch\` launchers. |
| Archive candidates | `doc/include-tree.txt`, `doc/FutureTileFunctor notes.txt`, `doc/MT2-issues.txt`, `PORTING_STATUS.md` (to `doc/linux/`), `ptr-safety-review`, `teardown-leak`, `compile-time-refactor-analysis`. |

---

## 5. Do not pick these up again

Measured, refuted or reverted, with the reason on record:

- **The #1259 deferral of commits and IntegrityChecks, in every variant** (20.20.0, check-deferral
  off, bounded deferral, ExplicitSuppliers pre-start): memory blow-ups and retry storms; removed
  in a7127224f/92eaa7150.
- **Enforcing the admission gate on real models**, its hysteresis, the phase-order grower
  deferral, decommitting every freed store (+7.7 % wall), per-class self-drain, and draining
  outside the lock.
- **INF-A06** (atomic `counted_mutex`, no gain); **GEO-A42** (slower); **CLC-A07** (Hestia relies
  on the null); **GEO-A19** (RSopen is calibrated on segments per quarter circle); **STX-A15**
  (refuted); the CLC-A17 and STG-A30 remainders.
- **Growing the quadtree node struct** (GEO-A54 cost t410 11-14 s), **MinObjectsToSplit back to 4**,
  **sorting leaves by area**, and a lowest-index `point_in_polygon` at today's 7x cost.
- **SMAWK for Jenks-Fisher** (0.96-1.20x); never add an early exit to `FindMaxBreakIndex`.
- **`SpecialSize` true for every size** (reverted 78503eedd); **removing `/STACK`** before C1b.
- **deadlocks P6 / ordering per-item locks by level** (measured false); **folding interest into
  `PrepareDataUsage`** (rejected by its own document); **std-ptr §15.1 and §15.7** (premises
  false).
- **Speculative language features without demand**: WP4.2 `applyF`, runtime lambdas,
  `filter`/`fold`, Category C/E rewrites, K6, WP4.3, the K11 leftovers. No issue asks for them,
  and the prelude, the only production consumer, needs none.

---

## 6. A suggested order

1. **This week:** A11. (A1, A2, A3 and A6 landed on 2026-10-06.)
2. **Next:** A4 as one result-changing round with its own full.py round, then A5, A7, A8, A9, A10,
   each with its testcase.
3. **Then the measurements** that decide the large items: B4's three A/Bs, C1 step 1, C7's
   baseline. Together they take about two days of machine time.
4. **Tier B by evidence:** B1 after A3; B2 step 1; B5 and B7 together (deletion first); B6;
   B3; B8.
5. **Tier C only where step 3 says so.** C2 (1)-(2) can start any time, since it is mechanical
   and needs no measurement.
