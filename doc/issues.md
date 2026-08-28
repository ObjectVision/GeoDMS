# Open GitHub issues, classified

Snapshot of the **14 open issues** at https://github.com/ObjectVision/GeoDMS/issues, re-audited
against GitHub on **2026-08-26**. The previous header claimed 27; sixteen issues it still classified
had been closed and five open ones were missing, so the tables below were rebuilt from the live list
rather than edited in place. #1215 was closed that afternoon and #1217 was filed and closed after
it, so both are recorded below rather than in the tables. Since that audit, #1212 and #1219 were
both closed (2026-08-27, recorded below); #1219 was filed and fixed after the audit and so never
entered the tables, and neither did #1220, which was filed and closed on 2026-08-27 as well.
#1211, which the tables did classify, was closed right after it by the same work. #1196 has
since been closed as well (2026-08-27, recorded below), and #1221 was filed that afternoon and has
not been classified here yet. Live count re-checked against GitHub after that: **11 open** — #587,
#724, #990, #1145, #1161, #1165, #1191, #1198, #1205, #1214, #1221. Note #973 and #659 have both
closed since the audit and their rows below are stale. Grouped by implementability. Buckets:

- **A. Low hanging fruit** — small, well-defined fixes; no design decisions needed.
- **B. Implementable after minor design choices** — clear scope; one or two decisions to settle first.
- **C. Refactoring** — the fix lives in internal mechanisms, not a local patch.
- **D. Needs design** — new algorithms, semantics, or architecture.
- **E. Test issues** — extending the test process.
- **F. Documentation issues** — docs/website work, no engine code.
- **G. Other** — roadmap, questions, investigations, likely-duplicates.

Previous snapshots: 2026-08-24 claiming 27, 2026-08-20 with 33, 2026-07-31 with 48 listed,
2026-07-04 with 41, 2026-07-03 with 45 open. See "Recently closed" at the bottom for the delta.

## A. Low hanging fruit

None open. #1211, the last one, was closed on 2026-08-27 (recorded below).

## B. Implementable after minor design choices

| Issue | Design choice to settle |
|---|---|
| [#1165](https://github.com/ObjectVision/GeoDMS/issues/1165) Deprecate the point-valued `range` property | Rescoped by the 20.14.0 work (`7d1a336e`): every textual rendering of a point now states its coordinate order as `xy(x; y)`, so the property is no longer order-ambiguous, and reading an untagged range already warns from `RangeStream`. What is left is (a) property-level deprecation machinery — `AbstrPropDef::IsDepreciated()` is still consulted only at `rtc/dll/src/tic/Xml/XmlTreeOut.cpp:1016`, where it hides the property from the detail page, so configuring the deprecated `Expr` property warns nothing — and (b) migrating the 19 regression configurations that still use the brace form. Decide the choke point and the warn-now/error-later timing. |
| [#1161](https://github.com/ObjectVision/GeoDMS/issues/1161) Mitigate mixed-case deprecation warnings | Half done in 20.19.0; the engine-level naming decision is made and corpus-backed. Measured first: every configuration emitted eleven case-mixup warnings before it had done anything wrong, and ten of them were the engine disagreeing with its own shipped `res/RewriteExpr.lsp` and `res/prelude.dms` (in `t050_fixed.log`, 39 of 56 log lines were this noise). Those are fixed, and `unit`, `value`, `item`, `param`, `attr` and `nrofrows` are now canonical lower case -- the census over the 1146 `.dms` files in `C:/dev/tst` says `unit` 13829 vs `Unit` 69, `value` 2333 vs `Value` 52, `nrofrows` 1256 vs `NrOfRows` 356. The `testcases` battery went from 2747 case-mixup lines to 511, and from 0 to 36 of its 233 configurations emitting none at all. What is left: (a) the ~86 hand-written mixed-case operator names in `clc/` and `geo/`, internally inconsistent (`IsNull`/`IsDefined` vs `isPositive`/`isNegative`, `Round` vs the config-written `roundUp`, `UpperCase`, `asDataString`, `LowerBound`, `spatialIndex`), which also touch `data/operators.csv`, the Notepad++ grammar and the wiki -- and note `AbstrOperGroup` keeps both `m_OperName` (`GetNameStr()`, signature text) and `m_OperNameID` (`GetName()`, DocData), so a rename must change every registration site or the engine spells one operator two ways; (b) the warning itself, which names no config file or line, fires for whole item PATHS (34 of the 229 distinct warnings in the regression corpus contain a `/`), and cannot tell a mis-spelled engine name from two unrelated tree items that fold to one token -- Renaming the short local names in `res/prelude.dms` and the battery's own configurations took it further, to 281 lines and 92 clean configurations. The remaining floor was then settled the OTHER way, by decision: symbols in the HOF checker are now uniformly `TokenID` rather than `SharedStr` -- `SignatureRecord::varRoles`/`ResultMember::path`/`ResultMemberSet::prefix`, the `canon()` role vectors, and the `DefType::members` maps, which had hand-rolled the token table's ASCII fold in a `MemberPathLess` comparator (now deleted; lookups probe with `GetExistingTokenID`, so data-derived names create no registry entries). `SameShape` congruence is integer compares now, and the fold survives only in the string-prefix scans over materialized key text. Interning the operator-signature role labels at describe time then widened their collision reach (306 lines / 80 clean), so they are now interned `sig_`-prefixed by `SignatureRecorder::NewVar` -- one central site, describe sites still write plain `"D"`/`"Imp"`. The prefix was CHOSEN BY MEASUREMENT: `sig_*` occurs nowhere in the regression corpus or the battery, where a trailing `_` still collided (`U_` 10 case-mismatches, `E_` 9, `A_` 8, `D_` 2) and a leading one did too (`_P`, `_B`, `_A`); the earlier idea of a word (`Res` 6589 mismatches, `Un` 127, `Sel` 97, `Arg` 86) was hopeless. Synthetic skeleton roles are spelled out (`sig_arg0`, `sig_res`) because a one-letter synthetic folds onto a describe-side label -- caught in measurement as a new `sig_R -> sig_r` line. That took the battery to 147 lines / 156 clean configurations, and the config-side sweep then took it to **40 lines, 24 distinct pairs, 214 of 233 configurations emitting none** (from 2747/0 at the start of the session), all 233 still passing. The config-side work: boolean literals `True`->`true` (211 occurrences), the `Units` container -> `units` (SI symbols `W`/`K`/`A` keep their case), file-local renames driven by a fold-conflict scanner (`A`/`a`, `V`/`v`, `Cells`/`cells`, the OD relational attributes -> `N1_rel`/`N2_rel`, the discrete_alloc type names -> `LuA`/`LuB`/`LuC` renamed together with the `['A','B']` data that resolves them), case-normalisation of `Grid2Poly.dms` (safe for shipped content: every spelling is already ONE token, so a case change cannot alter what a reference denotes), and prelude locals that collided with ordinary config names (`src`->`srcCont`, `TP`->`TPoint`, variant `scaled`->`scaledBy`). Two renames INTRODUCED conflicts before the scanner caught them (`P`->`Px` onto an existing `px`), which is why the scanner checks the target name too. The remaining 40 are engine names that 3rd-party scripts depend on -- operator names and composite-result sub-item names, out of scope by decision -- plus `w`/`W` where `W` is the SI symbol for Watt and must stay capitalised. `true`/`false` need an exemption either way: the corpus writes `True` 2830 + `TRUE` 456 against `true` 863. |
| [#1145](https://github.com/ObjectVision/GeoDMS/issues/1145) Export Primary Data exports wrong (Buurt) geometry | Bug, not feature: needs repro/debug on OVSRV08; the fix is picking the geometry of the right domain. |
| [#990](https://github.com/ObjectVision/GeoDMS/issues/990) `union_data` with unmatching but equal-sized domains | Hard error or warning? Might existing configs rely on it? |

## C. Refactoring

| Issue | Rationale |
|---|---|
| [#1191](https://github.com/ObjectVision/GeoDMS/issues/1191) Closing the GUI during calculation leaves the process alive | Repro and dump exist. Likely scheduler teardown ordering: scheduled suppliers are discarded before active joiners are released, followed by an unbounded task-group wait. Needs stack-backed shutdown/lifetime work, not a local GUI close patch. Note #1206, which shared a suspicion of process-lifetime cleanup, closed separately on 2026-08-24 (`12f6eb90`) without connecting the two stacks. |

## D. Needs design

| Issue | Rationale |
|---|---|
| [#1214](https://github.com/ObjectVision/GeoDMS/issues/1214) Fault-tolerant sweep for polygon_intersection and overlay | Split out of #757 on 2026-08-26; also the last unchecked box of the now-closed #917. Settle what "fault tolerant" promises (a valid result always, or a valid result plus a diagnostic saying where tolerance was applied — silently snapping geometry is the failure mode to avoid), where the tolerance comes from, and whether it replaces the per-backend cleanup pre-passes (`fix_polygon`/MakeValid, `clean_bg_geometry`, CGAL `Polygon_repair`), which today run *before* the overlay rather than being tolerance inside the sweep. Touches the same code as #1205 and should be designed with it: a tolerant sweep changes the per-feature cost model #1205's subdivision decisions rest on. Design notes are on the issue. |
| [#1205](https://github.com/ObjectVision/GeoDMS/issues/1205) Balance polygon overlay by geometric complexity | The current outer/inner tile loops expose only the first argument's element tiling as parallel work, so a few dissolved features with millions of vertices collapse to one worker. Needs a choice between feature/vertex subdivision, parallelising both tile dimensions, prepared geometry, and a user-visible `subdivide` operator; argument-order guidance can be documented independently. |
| [#1198](https://github.com/ObjectVision/GeoDMS/issues/1198) Resource-aware admission does not converge | Follow-up to the closed #1158: enforce mode churns without reducing the live peak, and its committed-memory figure can exceed physical memory. Requires a corrected accounting/admission model rather than another local threshold. |
| [#659](https://github.com/ObjectVision/GeoDMS/issues/659) R (or Python) integration for calculations | The linking route is closed for good and recorded on the wiki: R's C API needs the MinGW-w64 toolchain R itself was built with, and hosting a single-threaded, `longjmp`-based interpreter inside a thread-scheduling engine is not viable. The file-and-`exec_ec` route is the answer instead, and 20.16.0 makes it usable (`5a9d4478`: the child's stdout+stderr are captured on one pipe and reported line by line as `exec: <line>`, capped at 1 MB but still drained, and waited for in ticks). What remains under "design" is the ordering discipline — the NetworkModel_EU/Julia production example shows the batch file, not the configuration, must own any sequence that includes a GeoDMS *write*. |
| [#724](https://github.com/ObjectVision/GeoDMS/issues/724) Circular units (wrap-around grid/time) | New unit semantics rippling through operators and metric checking. |
| [#587](https://github.com/ObjectVision/GeoDMS/issues/587) Storage-read functions in keyExpr | Language-level change to make storage reads expressible in calculation rules. |

## E. Test issues

None open. #1031 now runs the shipped testcase battery and real download-backed content for both
release flavours.

## F. Documentation issues

None open. #1080 (Academy on geodms.nl) was closed 2026-08-12.

## G. Other

The umbrella and question issues that used to sit here — #810 (component planning), #830 (tif
DialogData), #949 (GUI-versus-Run calculation time), #1186 (`.c` setup CRT), #1199 (indirect-property
dependency) — have all been closed; their write-ups are under "Recently closed" below.

One small thing is not an issue of its own, and was found while fixing #1186:
`tools/DeployResources.cmake` silently does nothing (exit 0, no copies) when `RUNTIME_DIR` is passed
with backslashes. The real call site always passes forward slashes, so nothing is broken today — it
is a two-line guard against an unpleasant failure mode for anyone invoking the script by hand.

## Recently closed (delta since 2026-07-31)

### Closed on 2026-08-27 (5)

- #1196 (`0209878c`, `d7ffb9f4`, `c37b773c`, wiki `d970ae1d`, `53d9ab5d`) — an audit of four places
  where `discrete_alloc` did 32-bit arithmetic that production grid sizes reach, none of it checked.
  All four are addressed; none of them was a reported wrong allocation, and the one that turned out
  to bite in practice was not the one the issue led with.

  **The perturbation (§1)** is no longer a file-level `using perturbation_type = Int32` but the
  template parameter `P`, and the six `discrete_alloc` names are instantiated at both widths: the
  plain ones at `Int32`, and `discrete_alloc_pi64`, `_16_pi64`, `_sp_pi64`, `_sp_16_pi64`,
  `_np_pi64` and `_np_16_pi64` at `Int64`. Same arguments, same results; the wider term costs eight
  bytes per shadow price, on a value that `claim` holds twice and every splitter step copies.
  `greedy_alloc` and `needy_alloc` deliberately have **no** `_pi64` twin: they rank the land units
  once and serve them from the raw suitabilities, so a wide-perturbation name would have differed
  from the plain one in nothing but those eight unused bytes. That asymmetry is structural rather
  than a convention — `hitchcock_operators` is instantiated over both perturbation types and
  `heuristic_operators` over `Int32` only, so there is no template that could produce the name.

  **The prices (§3)** now go through `CheckedAdd`/`CheckedSub`, which `rtc/dll/src/vt/CheckedCalc.h`
  gained for signed integrals (forming the result in the unsigned representation, where wrapping is
  defined; `CheckedSub` is new for both signednesses). Everything that produces a shadow price is
  covered: the facet cost, both splitters' price assignments, the bid, the Dijkstra path cost in
  `bi_graph.h`, and the `DistFromOpt` reporting totals — which also moved from `UInt64` to `Int64`,
  so a net negative total reports as negative instead of wrapping. An overflow throws a
  `shadow_price_overflow` carrying *which* component failed, and `CalcResult` turns it into a config
  error naming the `_pi64` variant to switch to, with a separate note when it was the price rather
  than the perturbation, since a wider perturbation does not help there.

  Two things are deliberately **not** checked, both commented in place. `compare_oper::GetC` only
  *orders* a facet queue and runs O(n·k·log n) times; the same difference is checked in
  `priority_heap::GetC`, which is the one that becomes a cost, so a suitability range wide enough to
  wrap is still reported the moment such a land unit reaches the top of a queue. And every
  `dms_assert` reads `GetLinkCostUnchecked` instead of `GetLinkCost` — see the trap below.

  **The claim aggregates (§2) were the finding that actually bit, and it needs no grid at all.**
  Individual claims are bounded by the land unit count; the aggregates are not, and were summed in
  `UInt32`. `testcases/fn_test_da_claimsum.dms` is four cells and two land use types, each with the
  "no limit" maximum claim of 2^31: their sum is exactly 2^32, the aggregate read **zero**, and the
  run was rejected with *"there are 4 cells that should be allocated while the total of the maximum
  claims is only 0"*. Verified failing on the parent commit and passing on `c37b773c`.
  `IsFeasible`'s four totals and two link sums became `SizeT` locals, which is free.
  `FeasibilityTest`'s per-unique-region aggregates needed more thought, because they feed the
  max-flow search in `bi_graph.h`, which counts in `UInt32` end to end — widening the arrays would
  have rippled into `push_node_pos`/`augment`/`heap_elem`. They are summed in `SizeT` and narrowed
  instead, which is exact in both directions: no region can absorb more land units than exist, so a
  **maximum** aggregate above N means what N means and is clamped to it, while a **minimum** above N
  is reported as the infeasibility it is rather than hidden by the clamp. §4 (`muldiv_u32`) is a
  three-line check on a narrowing that was an invariant of the caller, not of the function.

  **A latent bug fell out of the second instantiation**, and it is the argument for having built the
  variant rather than only widening the type: `priority_heap`'s perturbation factor was
  `src->m_ggTypeID - dst->m_ggTypeID`, an **unsigned** difference of two `UInt32` ids that only came
  out right because the result was narrowed back to `Int32`. At `P == Int64` the wrap survives and
  the facet's tie-break direction inverts. The existing
  `dms_assert((m_PerturbationFactor > 0) == m_Compare.LhsDominated())` caught it on the very first
  run of `discrete_alloc_np_pi64`.

  **Two traps worth keeping, both costing a build cycle.** `dms_assert`, `assert`, `MAX_VALUE`,
  `MIN_VALUE` and `UNDEFINED_VALUE` are *macros*: a template argument containing a comma splits
  their arguments, so turning `shadow_price<S>` into `shadow_price<S, P>` produced some forty
  `warning C4002` lines followed by a cascade of syntax errors many lines away. `MAX_VALUE(T)`
  expands to `MaxValue<T>()`, which can be called directly; assertions took a local
  `using price_type = ...` alias. And **an expression inside `dms_assert` must not be able to
  throw**: the macro installs a `DebugOnlyLock` around it, and formatting a `DmsException` message
  goes through `StringStream`, whose `dms_check_not_debugonly` then fires `__debugbreak()` — the
  process dies with exit `-2147483645` (0x80000003) and no error text at all. Hence the
  checked/unchecked pair. In Release `dms_assert` is `CC_ASSUME`, which does not evaluate the
  expression, so a throwing assertion would have been a genuine Debug/Release divergence as well.

  **What is not covered.** The overflow the `_pi64` names exist for needs ~2^31 land units and stays
  untested; `testcases/fn_test_da_pi64.dms` pins the equivalence of the two widths instead, which is
  what a small configuration can say. A config extreme enough to overflow the *price* component also
  violates the algorithm's own Debug invariants before the checked arithmetic runs — an attempted
  negative testcase died on `exit 3` and was dropped — so that path has no regression case either.
  This is the "not covered by the regression suite" note the issue itself opened with, and it stands.

  Documented on the wiki: [Allocation functions](https://github.com/ObjectVision/GeoDMS/wiki/Allocation-functions)
  gains the section on when a model needs `_pi64` and what it costs; the discrete_alloc, greedy_alloc
  and needy_alloc pages point at it, the last two stating that no such name exists for them and why.


- #1211 (`a5c57688`, wiki `7f33d44b`) — the four chart kinds all reach `getIconFromViewstyle` as
  `tvsHistogram`: the `ViewStyle` says that a chart window is meant, not *which* chart, which is why
  all four had worn one icon — the Statistics sigma until #1220, a single bar chart after it. The
  switch now takes the `ChartKind` that `createView` already holds (and that `SetViewContextChartKind`
  puts on the view context), and `ChartKindGlyph` answers with one of four: `bar-chart-2-line`,
  `bubble-chart-line`, `line-chart-line`, `bar-chart-line`. The two bar glyphs differ in what the two
  charts themselves differ in — a histogram's bins touch, a bar chart's bars stand apart — which is
  the one pair a reader could confuse at 16 px.
  No second sub-window property was added for this. `updateWindowMenu` stopped recomputing the icon
  from the `viewstyle` property and takes each sub-window's own `windowIcon()` instead, with the view
  style as a fallback for a window that never set one; the chart kind is known where that icon is
  set, and a menu entry can no longer come to disagree with the title bar it stands for.
  The reporter's four attached bitmaps were deliberately **not** used, and the issue says so: #1220
  had just moved this set from `.bmp` pairs to font glyphs, and a bitmap pair per icon is exactly the
  per-icon cost that kept the set small for years. Each glyph is a single codepoint to change if the
  drawn shapes are preferred.
  Documented on [Main menu](https://github.com/ObjectVision/GeoDMS/wiki/Main-menu), section View, and
  on [TreeView](https://github.com/ObjectVision/GeoDMS/wiki/TreeView), section pop-up/context menu —
  the same two pages #1220 touched, both screenshots retaken from the current build.
  Noted in passing and *not* changed: in the View menu `&Statistics` and `&Scatter Chart` share the
  `S` mnemonic, so Alt+V,S cycles between the two instead of activating either. It predates this work
  (`Create &Scatter Chart` clashed the same way). It also breaks index-driven menu capture, because
  Qt's arrow keys skip *disabled* items: which entry `{DOWN}`×n lands on depends on how many chart
  entries the current item has enabled, so drive a chart from its shortcut, not from its index.


- #1220 (`71282962`, `362d0eba`, wiki `8ae06d3d`, `e24084e7`) — the TreeView has drawn its icons as
  remixicon glyphs since #319, but `getIconFromViewstyle` still answered with the 16x16 bitmaps, so
  the View menu, the TreeView's pop-up menu and every window title kept the old set beside the new
  one. The renderer and its cache moved out of `DmsTreeView` into a `DmsIcons` unit both callers
  reach; the cache key became (glyph, letter, colour, dpr) instead of (item kind, in-template, dpr),
  which is what lets a menu icon share an entry with a tree icon. A view kind that exists in the tree
  wears exactly its icon and colour (map = earth/blue, table = table/teal, palette = palette/magenta);
  the rest take the slate of a plain attribute with the glyph their bitmap drew.
  What settled the one open question in the issue — whether *View > Default* should keep its place —
  was the distinction drawn in its comment thread: **a tree icon states an immutable categorisation,
  a menu entry is an applicable action**. So the entry stays, renamed *Default View*, and is the only
  one with no icon of its own: `updateActionsForNewCurrentItem` gives it the icon of the view
  `defaultView()` will create, through one `defaultViewStyleOf` that both use, so the icon cannot come
  to disagree with the action. *Table* and *Map* became *Table View* and *Map View* and the four chart
  entries dropped *Create*, all in the shared `QAction`s, so the pop-up menu follows for free.
  Found in passing, worth remembering because it was invisible on Windows: the four entries that set
  an icon directly did so with `QIcon::fromTheme("backward", <bitmap>)`. The bitmap is only the
  *fallback*, so on any desktop with an icon theme installed — i.e. Linux — *Default*, *Table*, *Map*
  and *Statistics* would each have drawn a **back arrow**. Nobody would have seen it here.
  On verification: drive the GUI from a `.dmsscript` and capture the menus with `PrintWindow` on the
  process's **own** window handles (`EnumWindows` filtered on the process id gives the menu pop-ups).
  A full-screen `CopyFromScreen` grab is the wrong tool twice over — `Start-Process` cannot take the
  foreground, so the first attempt photographed whatever else was on top — and the capturing process
  must call `SetProcessDpiAwarenessContext` before `GetWindowRect`, or the rect under-reports on a
  scaled display and the bitmap clips the menu's shortcut column.
  Left open: #1211 wants a distinct icon per chart kind; the four still share one bar-chart glyph.

- #1219 (`1b0f6b5c`, wiki `05537e72`) — a Debug run aborted on
  `assert(begin[0] == beyond[-1]); // closed ?` in `geos_create_linear_ring` while reading a
  sequence that `points2polygon` had built from *n* corner points. The precondition was never
  needed: the function appends the first point to its own copy and ends with
  `MG_CHECK(result->isClosed())`, which runs in Release too, so Release computed the right answer
  all along and only Debug fell over. `remove_adjacents_and_spikes` moreover already drops a
  closing duplicate itself (`if (last[-1] == first[0]) --last;`) before any spike handling, so
  trimming it at the call site is one element less to copy rather than a change in behaviour —
  which is what made replacing the assert with an explicit skip safe, in both readers as well.
  Battery 231/231 in Debug **and** Release; the Release run matters because the skip is ordinary
  code, unlike the assert it replaced.
  Two false trails are worth remembering. The isolation first appeared to implicate the
  IntegrityCheck route, because `GeoDmsRun <cfg> /item` **creates an item without calculating it** —
  so the check was the only thing forcing any calculation and every "clean" control run had done
  nothing at all (zero breakpoint hits). That is the #949 trap again, one level down. A five-build
  bisect resting on that artefact was equally void; it tracked which commits happened to force a
  calculation, and exonerated #1218, #1209 and #302 for no reason. Also noted in passing: an
  unrelated `IsMetaThread()` assert at `AbstrDataItem.cpp:192` masks this configuration shape on
  builds around 2026-08-12 and older, so read the assert text when bisecting in that range, not the
  exit code. The wiki page [[points2polygon]] now states that the operator repairs nothing and what
  to use instead.

### Closed on 2026-08-27 (1, earlier)

- #1212 (`6d5cf8cf`, wiki `3938ee73`, tst `17cad21`) — "geos_polygon silently mis-reads a
  uniformly counter-clockwise polygon: shell dropped, lake promoted", split off from the closed
  #302. Two readers, not one: `geos_create_polygons` in `geo/dll/src/GEOS_Traits.h` and
  `assign_multi_polygon` in `CGAL_Traits.h` both judged a ring **absolutely** — shell iff
  clockwise — and both threw away a ring that failed that test while no polygon was open yet.
  On a feature whose rings are uniformly counter-clockwise, which is what a source listing
  coordinates in latitude/longitude order gives you, the shell was skipped with `isFirstRing`
  still true and the lake behind it was promoted to shell, with the island inside that lake as
  its hole: 36 − 4 = 32 where 100 − 36 + 4 = 68 is right. Nothing was rejected and nothing was
  empty — ordinary-looking geometry with a smaller area, and every `geos_*` and `cgal_*`
  operator goes through these two readers. The issue reported the cgal symptom without claiming
  its cause; it is the same one, reached differently, because that reader reverses each ring on
  the way in, so the flipped shell arrives clockwise, falls into the hole branch and is dropped
  for want of an open polygon.

  Both now do what `assign_multi_polygon` on the boost side already did, which is why
  `bg_polygon` was the only one of the three that kept the shape: the first ring that survives
  defines what "outer" means for that feature, a ring wound the other way is a hole of it, and a
  ring wound like it opens the next polygon. Nothing can change for a correctly wound feature —
  when the first surviving ring is clockwise, `outerOrientationCW` is `true` for the whole loop,
  so `currOrientationCW == outerOrientationCW` reduces to the old `currOrientationCW`, branch for
  branch. The results come back correctly wound as well: `geos_create_polygons` already ends in
  `normalize()`, which orients shells clockwise and holes counter-clockwise, and a CGAL
  `Polygon_set` is counter-clockwise by construction, which the writer reverses back. The CGAL
  hole branch's `reverse_orientation()` became conditional on the ring actually being clockwise:
  the same call it always made in the normal case, and the correct no-op when the whole feature
  arrives flipped.

  Verified on a Release build with a scratch probe over seven fixtures × four backends
  (`scratch/issue_1212_probe.dms`): correctly wound 68/68/68/68 and two disjoint squares
  50/50/50/50 unchanged; the flipped fixture now geos **68**, cgal **68**, bg −68 against −68 in,
  where the issue measured 32 and 32; a flipped two-square multi-polygon likewise 50/50/−50; and
  `geos_union_polygon`/`cgal_union_polygon` of the flipped feature 68, so the repair carries
  through a real set operation and not just the reader round-trip. Testcases battery 231/0. The
  `tst` polygon-family comparison (`Polygons/cfg/compare.dms`, ManySmall + FewLarge across geos,
  bg, bp and cgal) `/results/syntOk = 1`, and `ring_encoding` passes whole, `island_in_hole/ok_cgal`
  included — the #1178 case the CGAL edit sits next to. `winding_order` now carries the assertion
  it was deliberately written without (`ok_readers`: geos 68, cgal 68, |bg| 68).

  **Deliberately not delivered:** ring roles from nesting, which is what this entry's old
  classification under C called for. `geos_polygons_by_nesting` already exists next door and
  `fix_winding_order`/`fix_polygon`/`has_correct_winding` expose it, but it costs a containment
  test per ring, which #302 kept out of the readers on purpose. The relative rule is free and
  fixes the reported data loss; a feature whose rings are only **partly** flipped is beyond any
  orientation rule and still needs `fix_winding_order`. Recorded on the wiki
  (`Point-order-in-polygons`, new "what the readers accept" section, plus condition 2 on
  `geos_polygon` and `cgal_polygon`), including that a configuration's areas may change on
  upgrade and that the older numbers were the wrong ones.

### Closed on 2026-08-26 (12)

- #1218 (`58c3c1a4`, wiki `926a07e7`) — "IntegrityCheck op ExplicitSupplier uitvoeren juist
  voordat berekening van item start", the supplier variant of #1209. The issue's pair —
  `exec_ec` with a check, `x := 2+3, ExplicitSuppliers = "exec_ec"` — was measured first:
  requesting `x` in a **table view** did run the supplier's check, but out-of-band (the
  validate phase of the actor update; two threads, `add [[/x]]` computed regardless, verdict
  reaching `x` only afterwards via the F2 chain), and selecting either item in the
  **TreeView**, detail page included, ran it not at all — the TreeView demands metadata only,
  and `x`'s CheckedKeyExpr simply did not contain the guard, because the fold collected checks
  from the parent chain alone.

  The fix extends the collection side of the #1180 fold: the checks that apply to an item are
  now the transitive closure along **GetTreeParent ∪ ExplicitSuppliers** — declaring a supplier
  means "evaluate me first", and whatever guards the supplier (its own check, its ancestors',
  its suppliers', transitively) is folded into the declaring item's checked expression and
  fails it in-band. The closure is reduced to a per-item guardian list memoized in
  `ConfigProperties::mc_CheckGuardians` (meta-thread only; reset in DoInvalidate and
  ResetSubTreeConfigData, which is what breaks the cross-branch SharedTreeItem cycles the list
  can otherwise hold at teardown). An item that adds nothing shares its parent's instance and
  is not memoized, so a chain without supplier edges folds byte-identically to before —
  `fn_test_icheck_dedup` still instantiating exactly its 4 recorded check DCs is the
  DataController-moniker compatibility tripwire. Redundancy stays decided at the wrap site
  against the #1182 implied-atom sets: an item that both references and declares the same
  supplier carries the guard once, through the reference (verified: no CheckedKeyExpr wrap
  appears). Supplier cycles are broken with an in-progress set and incomplete closures are
  never memoized; fence SupplCaches (PhaseContainer `InitAt` mirrors on cache items) are
  engine bookkeeping and excluded.

  Verified: testcases battery 231/0 with three new cases (`fn_test_icheck_suppl` — two-hop
  supplier chain plus a supplier under a checked container, the Debug trace showing
  `IntegrityCheck(IntegrityCheck(add(2,3), eq(22,22)), eq(11,11))` in closure fold order —
  and `_neg1`/`_neg2` for violated direct and transitive supplier checks); Debug runs
  assert-free; GUI probes confirm the guard in the declaring item's CheckedKeyExpr and the
  in-band failure on its own DC.

  **Deliberately not delivered by this change:** the title's strict ordering. The guard gates
  the *delivery* of the declaring item's result, not the start of its calculation — condition
  and org expression are sibling CheckOperator args and still compute concurrently (measured
  with a 20M-element check against a trivial `add`). Making the org expression's
  OperationContext depend on the check future is the lookahead-scheduling follow-up, recorded
  in `doc/IntegrityCheck.md` §#1218 with the rest of the design.

- #1217 (`9f89ddf8`, wiki `77922c0c`) — the map view pop-up offered *Drag LayerControl Left
  (Ctrl-S)* and *Drag LayerControl Right (Ctrl-D)*, and the reporter could work out neither what
  they did nor why the keys were dead. Both halves of the caption were wrong, in different ways.

  The **function** is real and has no other entry point: the two items move the virtual splitter
  between the map and the legend by 10 pixels a step (`MapControl::ShiftLayerControlSlider`).
  There is no draggable splitter in a map view, which is what #511 added them for. Past the
  minimum width, narrowing calls `ToggleLayerControl()` and hides the legend outright; widening a
  hidden legend re-opens it. Neither limit was discoverable from the caption, and *"Drag ... Left"*
  reads backwards — dragging left makes the legend **wider**.

  The **keys** were stale documentation inside the binary. `c7e35b9f6` bound Ctrl-S/Ctrl-D in
  17.9.5 and `3f374867b` added the menu items beside them; `f7e9a1a7c` (#1011) then unbound both
  in 18.2.1, because Ctrl-D had meanwhile become the Qt **Table View** shortcut
  (`DmsActions.cpp`) and pressing it in a map view opened a table instead of resizing — #1011's
  actual complaint. The width control moved to Ctrl+Shift+Left/Right in `MapControl::OnKeyDown`,
  where `qtKeyToVK` translates the arrows and folds the modifiers in, but the two `MenuItem`
  strings were never touched. Worth remembering as a class of defect: an accelerator named in a
  caption is documentation with no compiler behind it, so re-binding a key silently falsifies
  every menu that mentions it. Ctrl-G survived only because nothing else claimed it.

  Verified against `bin\Release\x64\GeoDmsGuiQt.exe` 20.18.0 on a self-contained grid map view,
  one screenshot per step: Ctrl+Shift+Right narrows 10 px a press and hides the legend after ~30;
  Ctrl+Shift+Left widens and re-opens it at its previous width; Ctrl+S does nothing; Ctrl+D opens
  a table view (`[commands] tableView // for item /domain/Value`); Ctrl+G still copies a 246x46
  bitmap of the legend to the clipboard. The captions now name the effect and the working keys,
  following the `DataItemColumn` precedent of spelling the effect out after a colon. **The
  caption change is committed but not yet built into a binary.**

  Documented on the wiki, [Map view Legend → width of the
  legend](https://github.com/ObjectVision/GeoDMS/wiki/Map-view-Legend#width-of-the-legend), with
  a pointer from Map View's keyboard table saying Ctrl-Shift-arrow resizes the legend rather than
  moving the map. The page records the 17.9.5 / 18.2.1 / 20.18.0 caption history, since a reader
  on an older build will see the old menu.

- #1215 — closed by the reporter at 16:14, before this snapshot's bucket A reached him: the
  `polygon_connectivity` verify-and-close listed there was already done. `cogPC` plus the
  `bp_`/`bg_`/`cgal_`/`geos_` variants exist on all four backends and are on the wiki.

- #694 (`da6bb6cc`, `1ae70e5c`, `24863f95`), #757, #810 and #1105 (`b0fa05eb`, `565e9ef4`) were
  closed here. #757 was split into #1214 (fault-tolerant sweep, still open in D) and #1215
  (polygon_connectivity, already implemented — see A); #810 was a roadmap umbrella rather than an
  implementable issue.

- #634 (`a7475fac`) — copy-on-write for the data a layer control shows, implemented in samenhang
  with the just-closed #734 as its comment asked. A colour, a label or a class break whose attribute
  the configuration calculates used to be uneditable: `TreeItem::IsEditable()` is false as soon as an
  item carries a non-data-block rule, so the Change Color submenu was never built, the double-click
  and F2 paths fell through, and Ramp reported *"Cannot change derived data; try to copy the
  attribute and change the copied data"*. The first edit now makes that copy: the values the rule
  currently computes are copied into `/Desktops/<desktop>/ViewData/<path>/`, the layer is re-themed
  onto the copy and the edit is applied there, so the configured item — shared with every other view
  that uses the same classification — is never written. A second edit of the same cell reuses the
  copy. Scope is per cell: the *number* of classes resizes the class unit itself and is not covered,
  which stays #734's Copy/Paste Classbreaks route. A plain reference rule (`PenColor := BrushColor`)
  is not copied, because `GetCurrSourceItem` resolves it to the referred item, which is writable.
  Table views are unaffected: only a `PaletteControl` copies.

- #917 (`2c82cb27`, `30e3da16`) — `{bp,bg,cgal,geos}_minkowski_sum` / `_minkowski_difference` with
  the kernel as an argument, and the 48 `bp_*_i4HV`…`_dXD` names depreciated. Five of its six boxes;
  two of those (`geos_simplify_linestring`, `polygon_connectivity`) turned out to be implemented
  already. The sixth is #1214, listed in D above.

- #1216 — closed as **not planned**: a duplicate of #1214, filed 45 minutes later while splitting
  #917 without noticing that #757 had just been split into #1214 and #1215 covering the same ground.
  Its design notes were moved to #1214 before closing. Worth remembering that #757 and #917 shared a
  checkbox, so splitting either one can collide with the other.

- #1213 (`e958f8f4`, `e60b3ddd`) — the `category` checkbox at the bottom right of the EventLog
  filter panel rendered clipped. Not a styling or a DPI problem, and worth recording because the
  obvious fix was only half of it.

  `DmsTypeFilter` places every child at an absolute position from `DmsEventLogSelection.ui`, so the
  two columns under *Filter message contents* began at y=40, 30 px lower than every other column in
  the form, and their fourth row landed at y=100..120 against the bottom of a 121 px panel. `line_6`,
  the separator between those columns, had the same defect in the other direction: it ran to y=140
  and was cut off. Moving the block up to y=26 at the form's own 20 px row pitch put `m_category` at
  86..106, the bottom margin the left group box's last row already had, without making the panel
  higher (`e958f8f4`).

  That unclipped `category` and exposed the real defect: the group box's own last row, at y=110, was
  clipped too, because the panel was being drawn at roughly 107 px for a form that needs 121.
  `toggleFilter` computed the new size as `height() + 150` — the height of the DmsEventLog *widget*
  — and handed it to `resizeDocks`, which sizes the *dock*, so the dock's title bar was counted
  twice and the widget grew by well under 150 while the panel wanted 121 plus the layout spacing.
  Nothing stopped the `QVBoxLayout` from taking that shortfall out of the panel: a widget whose
  children are all placed absolutely has no layout, so its `minimumSizeHint()` is effectively zero
  and it is the cheapest thing in the box to shrink. `e60b3ddd` pins it with
  `setFixedHeight(groupBox->height())` and takes the resize delta from the dock itself, so the list
  absorbs nothing and the shrink path is the exact inverse of the grow path. Verified on
  `escape_codes.dms`: the group box border and its bottom row render in full, and an off/on toggle
  cycle returns the panel and the list to the same geometry.

  The lesson is the second half, not the first: an absolutely-positioned Qt form inside a layout
  advertises no minimum, so it is squeezed rather than clipped by its own geometry, and the symptom
  surfaces at whichever control happens to sit lowest. A further entry in those columns should
  convert the block to a real layout rather than take the remaining row of headroom.

- #973 (`8412b7f2`) — missing VAT when exporting to dbf or gpkg. The issue asked for a case to reproduce; building one
  turned up **four** defects, each of which on its own ends in a shapefile without its `.dbf` or a
  GeoPackage layer without attribute columns. QGIS opens those, ArcGIS Pro does not, which is why the
  reporter saw it and we did not.
  1. `DoExportTable` collected the columns by walking the sub-tree of the *selected* item, and
  `WalkConstSubTree(nullptr)` returns that item, so for a data item the loop yields exactly one
  candidate — the geometry — which the next line skips *as* the geometry. The reported case reaches
  this whenever the domain has no map relation: `IsThisMappable` is `HasMapType || GeometrySubItem`
  and `GeometrySubItem` looks for a sub-item named literally `geometry`, so with a feature attribute
  called `pand_geometry` the only item of the table that opens as a map is that attribute itself, and
  that is what gets exported. The map relation is not what the *exporter* needs — exporting any other
  attribute of the same unmapped domain already worked, because a later fallback scans the domain.
  Settled as **always emit the VAT**, not as a dialog option: when the exported item is the feature
  attribute, the value attributes now come from the table it belongs to.
  2. `nativeShapeFile` compared `storageTypeName` with `"ESRI Shapefile"`, but under the native driver
  that string is `driver.nativeName`, `"shp"`. Always false, so the branch was **dead since
  `ca591b6f` (2023-06-28)** and the container got one `shp` storage manager instead of `shp` on the
  geometry plus `dbf` on the container — the geometry was written and every value attribute reported
  *Failure during Writing*. Three years in which no native shapefile export carried a `.dbf`.
  3. That failure was invisible: the dialog closed with *"Export ready"*.
  `Tree_Update_Or_Return_Failer` only reports the item it **suspended** on — `ItemUpdateImpl` returns
  `true` for an item that is already failed (`rtc/dll/src/tic/TicInterface.cpp:613`) — so a storage
  failure leaves the walk empty. Fixed in `exportImpl`, not in the shared walker whose contract other
  callers rely on: after the update it walks the generated config for a recorded failure.
  4. *Use native driver* was unconditionally unchecked by `showEvent` and checked only when the driver
  combo actually changed index, which `showEvent` triggers only on the first opening of the dialog per
  GUI session. Combined with (2) this meant the export a modeller got depended on how many exports
  they had already done that session. `setNativeDriverCheckbox` is now the single decision point, and
  a tick or untick by the user is remembered per driver.

  Reproducing case in `scratch/issue973/` (gitignored): three polygons from literals, six export
  containers reproducing what the dialog builds for each choice of item and driver, plus a
  `Pand_failing` table with `IntegrityCheck = "1 == 0"` to exercise (3). Case F is (2) on its own and
  exits 1 on 20.18.0.m. Worth generalising from: the export dialog builds a config subtree and hands
  it to the ordinary update machinery, so every dialog path can be emulated by a plain `.dms` and run
  under `GeoDmsRun` — but only the GUI shows which subtree it actually builds, and here the emulation
  of the *intended* native wiring was right while the shipped dialog was not.

### Closed on 2026-08-25 (3)

- #302 (`4e152d11`, `f2db238e`, `771d950a`) — the winding-order operators. Its counter-clockwise
  reader defect was split off as #1212, still open in C.
- #734 (`a7475fac`, `771d950a`, `f66c5c46`) — reuse classbreaks across mapviews; #634's
  copy-on-write landed in samenhang with it.
- #856 (`fd09c8a9`, `975d4410`, `98d217d2`) — 2-dimensional Dijkstra.

### Closed on 2026-08-22 through 2026-08-24, but still classified as open until the 2026-08-26 re-audit (7)

These are the entries the previous snapshot carried in its A/B/C/D tables after they had already been
closed; they are the bulk of the drift that re-audit found.

- #403 (`a6b9fe61`, `8c980d05`) — don't collect recollected items into subset.
- #1199 (`8b09c0f6`) — indirect-expression evaluation now rejects a supply chain behind a
  PhaseContainer; phase-number determination is transactional and reset by invalidation.
- #1200 (`f539d3ef`) — the referred-item-chain enumeration that yielded shadowed sub-items. It had
  been marked **NEXT** in the C table for two days after it was closed.
- #1202 (`781f033c`, `26265cda`) — worker-thread internal errors now carry item context.
- #1204 — the `.tfw` written beside GeoTIFF.
- #1206 (`12f6eb90`) — the 17-block CRT leak in spatial/CRS Debug runs.
- #1186 (`565e9ef4`, `4b95e5dc`) — the `.c` setup shipped no MSVC runtime at all, so it borrowed
  whatever redistributable happened to be in `System32`; `arrow.dll` failing on `__std_calloc_crt`
  proved that copy was older than 14.40. dumpbin over the whole `.c` installation found 6 CRT DLLs
  and 297 imported symbols, and all eight redistributable versions available here cover them. Fixed
  with CMake `InstallRequiredSystemLibraries` + build-time `deploy_resources`, plus `File` lines in
  `nsi/DmsSetupScriptX64-cmake.nsi`. The reporter will retest the `.c` variant at the next release
  and deliberately will *not* update their system redistributable, so the problem stays visible until
  then. Open follow-up worth its own issue: nothing checks the import closure of `bin\` against the
  packaged file list, so the next runtime dependency a vcpkg port introduces is again a manual
  responsibility.

### Closed on 2026-08-21 through 2026-08-24 (24)

- #421 (`3b8d464c`), #471 (`a8579ebc` and predecessors), #711 (`00623367`, `8d530a40`),
  #337 (`fd75a48e`), #1167 (`1edd357c`), #1201 (`4fefe3d7`), #975 (`75c33978`),
  #1197 (`2fc5476c`), #1195 (`06c2217e`, `cbea24ae`), #319 (`518dc747`, `0ce9f427`),
  #1194 (`c73b613d`), #1189 (`dc8ee2d1`, `9b36c68c`) and #1031 (`ed468f6c`, `c64d4dc7`)
  were implemented in this checkout.
- #1128, #1203, #830, #1183, #949, #1158 and #795 were also closed after their outcome was
  implemented, superseded, or recorded; their former open-list rationales are no longer carried
  above.
- #1190, #1192, #1193 and #1188 opened and closed inside the same interval.

The earlier snapshot contained thirty-eight closures in four groups. The version went
20.10.0 -> 20.16.0 over that window.

### Closed on 2026-08-20, working this list (4)

- [#859](https://github.com/ObjectVision/GeoDMS/issues/859) — The palette colour picker is a
  `QColorDialog` with `ShowAlphaChannel` (`58506a70`). Shv links Qt core+gui without widgets on
  purpose, so the GUI registers the picker through a new `SHV_SetChooseColorFunc`, shaped like the
  `SHV_SetCreateViewActionFunc` already there; the `// TODO: implement with QColorDialog` stub on
  non-Windows is gone with it. Only fully opaque and fully transparent render — every consumer of a
  palette colour either tests the `DmsTransparent` sentinel or calls `CheckColor()`, and
  `GdiDrawContext` blends in `FillRect` only — so an intermediate opacity is snapped to the nearer
  state with a warning. Making a partial alpha render is a drawing-layer change and wants its own
  issue.
- [#846](https://github.com/ObjectVision/GeoDMS/issues/846) — `CalcAndWrite(item, message)`
  (`f16e940b`), the PhaseContainer message without the fence. The 2025-03-03 blockreason asked
  whether there were still use cases; `tst/Operator/cfg/Fencetest.dms` contains the workaround that
  proves there are — a `Say_t` template that wraps a value in a container, phases it and reads it
  back, buying serialisation nobody asked for. The result IS the argument
  (`oper_policy::existing`), so nothing is copied or scheduled.
- [#828](https://github.com/ObjectVision/GeoDMS/issues/828) — Layer controls get a three-pixel 3D
  border and a 20-per-channel darker background when selected (`6506404e`). The border width is now
  `MovableObject::GetBorderLogicalWidth()`, so TableHeaderControls keep their two pixels untouched,
  and `DrawButtonBorder` takes the ring count with 2 as the default, leaving every other control
  byte-identical. The *look* is unverified: three pixels is a choice the issue does not specify.
- [#1031](https://github.com/ObjectVision/GeoDMS/issues/1031) — Closed on the prelude, then reopened
  on the wider shipped set; see E. The prelude half (`869e16da`) recorded two findings rather than
  fixing them, since the prelude's header declares bodies to be interface: `EK` reads an
  `ExternalKeyData` *property* that no configuration can set (the name is a sub-item name, which is
  how `LayerInfo.cpp` looks it up), and the `_or_rhs_null` comparisons mirror by swapping operands,
  not by flipping the operator.

### From the 2026-07-31 open list (14)

- [#1162](https://github.com/ObjectVision/GeoDMS/issues/1162) — Start-project dialog. Decision of
  18-08: the dialog goes away, the default start is an empty project, reopening the last configuration
  becomes an opt-in under Settings > GUI options > Startup, and Alt-R loads the top recent file. The
  debrief also explains why it was so annoying: it was raised from the `MainWindow` constructor while
  only the splash was visible, and a modal box whose parent top-level is still hidden gets no taskbar
  button and cannot claim the foreground. Making the auto-load opt-in also keeps the security property
  that motivated the prompt (`HKCU\...\LastConfigFile` is per-user-writable). Commits `b1613d74` +
  `bffb6233`, the latter showing the main window *before* a configuration is parsed, with splash and
  the one-second delay only for an idle start.
- [#1166](https://github.com/ObjectVision/GeoDMS/issues/1166) — Nested function body cannot see
  container scope. The July note called out one design call — whether the enclosing function's
  parameters and locals become visible too — and it was answered yes: `dbb3d96a` resolves function-body
  identifiers against the whole definition scope, `32def18b` lets a nested body reach the enclosing
  function's parameters and locals.
- [#1159](https://github.com/ObjectVision/GeoDMS/issues/1159) — Calculated attributes now coloured
  differently; shipped in the v20.10.0 preview.
- [#1154](https://github.com/ObjectVision/GeoDMS/issues/1154) — MMD reinterprets stale caches after a
  values-unit element-type change. Fixed by having the dictionary record restrictions on units external
  to it (`1710625b`) and the extent of an external domain, which needed the write session rather than
  the storage (`21e5e41c`).
- [#1155](https://github.com/ObjectVision/GeoDMS/issues/1155) — MMD subunit `Range` missing from
  `0Dictionary.dms`. Fixed with the same write-session rework; the follow-up `7d1a336e` introduced the
  unambiguous `xy(x; y)` point syntax and plain-notation numbers, which is what rescoped #1165.
- [#1156](https://github.com/ObjectVision/GeoDMS/issues/1156) — EmptyWorkingSet storm /
  `MemoryMaxRAM_GB`. Closed with the GUI-responsiveness half implemented for 20.13.0 (`2cfd7681`):
  `HasWaitingMessages()` probes `GetQueueStatus(QS_ALLINPUT)` instead of `QS_ALLEVENTS`, so a
  cross-thread *sent* message also makes `MustSuspend()` yield — without ever dispatching
  mid-computation, which is what ruled out `PeekMessage`: any retrieval call (`GetMessage` /
  `PeekMessage` / `WaitMessage`, `PM_NOREMOVE` or not) delivers sent messages and used to re-enter
  window procedures and deadlock.
- [#1157](https://github.com/ObjectVision/GeoDMS/issues/1157) — GUI killed by Windows during long
  computations. Closed 2026-08-12 as a duplicate of #1156.
- [#612](https://github.com/ObjectVision/GeoDMS/issues/612) — Value info for a null value. Implemented
  in `151a6497` on branch `issue_612_null_value_info` (not yet merged). Scope was decided by what the
  page already did: for `lookup(relation, values)` it already follows the index into the domain it
  points at, so every null *that has a row* was already explained, and `LookupImpl.h` now records why
  no operator support was added. The gap was the absence of a row.
- [#943](https://github.com/ObjectVision/GeoDMS/issues/943) — RS trede mapview added to the GUI test.
- [#1080](https://github.com/ObjectVision/GeoDMS/issues/1080) — Academy published on geodms.nl.
- [#579](https://github.com/ObjectVision/GeoDMS/issues/579) — Value info shows the export-xml variables
  first. Closed 2026-08-01, consistent with the July finding that it does not reproduce on current code.
- [#1164](https://github.com/ObjectVision/GeoDMS/issues/1164) — Groupby on BGT vegetatieobject:
  confirmed fixed in 20.9.0.
- [#1163](https://github.com/ObjectVision/GeoDMS/issues/1163) — Grow a grid by one cell: closed on the
  documented `potential`/`proximity`/`or` recipe.
- [#298](https://github.com/ObjectVision/GeoDMS/issues/298) — `mapping(D,V)` for coordinate-separable
  transformations. Done as a series: separable fast path plus a block-offset fix (`b01be60a`),
  conversion state built once per invocation rather than per tile (`6ff33f10`), `mapping_count` reduced
  to one addition per cell and turned into an outer-product histogram without a shadow copy
  (`a9136975`, `b3f54d48`), result tiles on demand and per-source-tile cross data for irregular tilings
  (`1ba7854d`, `27382690`), extended to same-SpatialReference grids (`6ad8a6f5`, `ba25e76d`), with the
  separable-mapping test added to the suite (`719b8506`).

### Opened and closed inside the window (19)

The operator-correctness sweep, all found and fixed in the first half of August:

- [#1168](https://github.com/ObjectVision/GeoDMS/issues/1168) — `ordered_union_data` typo
  (`1f03690e`); `9c6f6602` additionally verifies the non-decreasing promise in `CalcResult`.
- [#1169](https://github.com/ObjectVision/GeoDMS/issues/1169) — `perimeter` non-deterministic in a
  large configuration: it accumulated into uninitialised memory (`41ebc732`). Flaky since 17.4.6.
- [#1170](https://github.com/ObjectVision/GeoDMS/issues/1170) — `frequency_table_with_null` did not
  count nulls in the total; `as_unique_list_with_null` added alongside (`f2ffe940`).
- [#1171](https://github.com/ObjectVision/GeoDMS/issues/1171) — `greedy_alloc` hung on a 33-cell input
  that `discrete_alloc` solved instantly: it aborted *after* solving. Greedy and needy are now real
  regimes (`6f8143a6`).
- [#1172](https://github.com/ObjectVision/GeoDMS/issues/1172) — `triangualize`,
  `bp_buffer_multi_point` and `cgal_buffer_linestring` silently returned empty (`6c8f1428`); the
  Delaunay code is now shared with a new `voronoi` operator (`0a698129`).
- [#1173](https://github.com/ObjectVision/GeoDMS/issues/1173) — `modus_count_uint16` was registered as
  `modus_count_uint17` (`50795d2b`).
- [#1174](https://github.com/ObjectVision/GeoDMS/issues/1174) — `potentialPacked` /
  `potentialRawPacked` convolved with a bit-punned kernel (`6dd5156a`); the IPP -> FFTW3 port was
  completed and the single-precision backends revived as `potential32` / `potentialRaw32`
  (`d21c94f9`).
- [#1175](https://github.com/ObjectVision/GeoDMS/issues/1175) — `join_equal_values` sized six buffers
  on the values-unit range (60 GB for a 3x3 join); it now indexes the occurring values when the range
  of X is large (`805aeb0e`). The huge-alloc attribution that made it visible — exact size,
  element-count shapes, call stack — is in `4ec4ee1d`.
- [#1176](https://github.com/ObjectVision/GeoDMS/issues/1176) /
  [#1178](https://github.com/ObjectVision/GeoDMS/issues/1178) — `bg_union_polygon` crashed on real BAG
  pand polygons and `cgal_union_polygon` lost ~0.3% area where geos was exact on the same data. One
  commit for both (`8339e025`): cgal hole scoping fixed, and boost is no longer fed invalid geometry.
- [#1177](https://github.com/ObjectVision/GeoDMS/issues/1177) — Registered operator names that did not
  work (`ipf_alloc` silently returned zeros; reserved/obsolete stubs errored): removed, implemented, or
  guaranteed removal (`53dca389`). `data/operators.csv` was resynced against the 2322 registered groups
  in `5a20c5a7`.

The IntegrityCheck cluster:

- [#1179](https://github.com/ObjectVision/GeoDMS/issues/1179) — An IntegrityCheck on a stored attribute
  suppressed `0Dictionary.dms` entirely, silently producing an unreadable `.mmd`: restrictions on MMD
  content belong on the common ancestor (`fd251b12`).
- [#1180](https://github.com/ObjectVision/GeoDMS/issues/1180) — Using a nested item did not validate its
  ancestors, so a container's IntegrityCheck never fired (`9b76395b`, plus `8ef86c09`, `42441a58`,
  `abaf2447`, `fcc9893b`: fold ancestral checks into the checked expression, hoist a check out of a
  destructured subset argument, do not guard an expression that already enforces the same check,
  validate the own check only).
- [#1181](https://github.com/ObjectVision/GeoDMS/issues/1181) — Any IntegrityCheck evaluation asserted
  `IsMetaThread` in Debug (`856283e1` plus five follow-up sites; `48c9d507` adds a Release backstop
  pinning checker evaluation to interest + scheduling).
- [#1182](https://github.com/ObjectVision/GeoDMS/issues/1182) — Memoize which IntegrityChecks a
  calculation already enforces (`e375c521`).

Remaining:

- [#1184](https://github.com/ObjectVision/GeoDMS/issues/1184) — Zoom stopped working after clicking in
  the tree view: keyboard focus now stays on the view that was clicked (`0de99024`).
- [#1185](https://github.com/ObjectVision/GeoDMS/issues/1185) — Avast blocked the `.c` setup.
  Environmental; it played no role in #1186.
- [#1187](https://github.com/ObjectVision/GeoDMS/issues/1187) — MMD read a stored array *longer* than
  its domain silently, catching only the too-short direction; a length mismatch is now refused
  (`a54decee`).

### Open at the 2026-07-31 snapshot but missing from its tables (1)

- [#1034](https://github.com/ObjectVision/GeoDMS/issues/1034) — The recent-configurations list contained
  duplicates (created 2025-12-03, so it belonged in the previous snapshot). Fixed by collapsing
  duplicate entries (`f07c9c66`), with `e0dc29ef` additionally pinning configurations to the top of the
  recent-files list.

### Work in the window without an issue of its own

Not classified above, but it is where most of the window went, and it explains why several rationales
moved: header hygiene and PCH normalization; the TU split/merge reorg; the export-surface pass (~1150
entirely-dead exports removed) with `/pdbpagesize:16384`; the `StaticLateTokenID` sweeps; the U1-U5
collapses of the `DataArray` adapter layers and the `Unit<V>` hierarchy; one vcpkg world for scripts
and IDE, with drift tripwires and a self-healing Qt deploy; two Linux-only build breaks and a startup
segfault from a config read during static initialization; and the retained-result accounting /
admission gate / drain-mode series (SS8.1.21-SS8.1.33).

## Cross-cutting observations

- **Memory and liveness under load** (#1198, following the now-closed #1158/#1156/#1157): the GUI
  responsiveness and out-of-memory diagnostic work landed, but the admission model is not done. The
  flush trigger is no longer the only brake. The memory half is not, and the interesting result is
  negative: resource-aware scheduling is implemented, measures well in shadow mode (98% derived
  estimates, booked-vs-cardinality ratio 1.00 over 107 494 results) and still ships off, because on
  t641_2 enforce parked 124 184 operations for an identical 171.9 GiB peak. Refusing individual
  operations is the wrong lever when 91% of the volume is deferred material from chains that were
  already admitted. `MemoryDrainage` is on by default; `WaitForAvailableMemory` remains disabled.
  Plan and measurements: `doc/development/schedule-with-lookahead.md`.
- **PhaseContainer** (#1128, #1167, #1199, all implemented and now all closed): demanded members
  now reach and selectively collect the phase, completion messages are emitted when the phase
  finishes, and phase-backed values are rejected while evaluating an indirect expression. The
  remaining work is no longer in this cluster.
- **MMD storage robustness**: closed out. #1154 and #1155 were fixed together as the July note
  predicted, and the same pass turned up #1179 (an IntegrityCheck suppressed the dictionary) and #1187
  (an over-long stored array read silently). The dictionary now records element restrictions and
  external-domain extents, and is written when those become known rather than only at first open.
- **Operator correctness** (#1168-#1178, all closed): a new cluster, and the largest single source of
  closures this window — registered-but-broken names, silently-empty results, a bit-punned kernel, a
  range-sized allocation, uninitialised accumulation. Worth repeating as a sweep: the authoritative
  name list (`DocData()/OperatorGroups/name`) makes "which registered names do not actually work" an
  enumerable question, which is exactly how #1177 was framed.
- **Config language / syntax** (#1161, #1165): #1166 is closed with the wider answer — a nested body
  sees the whole definition scope, including the enclosing function's parameters and locals. #1165 is
  now the narrower "untagged `{a, b}` spelling" deprecation and still blocks on property-level
  deprecation machinery that does not exist. #1161 is half done: measuring it first is what made it
  tractable, because it showed that ten of the eleven warnings every configuration got were the
  engine disagreeing with its own two shipped files, not with the modeller. Those are fixed and the
  engine-level names are lower case; what remains is the operator-name sweep and the warning
  mechanism itself, which still cannot tell a mis-spelled engine name from two unrelated tree items
  that happen to fold to one token.
- **Export-flow cluster**: closed out. #711 and #411 were already done, and #973 (`8412b7f2`) turned
  out to be four defects in the same dialog rather than the VAT option it was filed as. The pattern
  #411 established — the dialog builds a `Desktops/Default/Exports` config subtree and hands it to the
  ordinary update machinery — is what made it diagnosable: every path can be emulated by a plain
  `.dms`. It is also what hid the defects, because nothing verifies that the subtree the dialog builds
  is the one the code was written for; a comparison against `"ESRI Shapefile"` that should have read
  `"shp"` disabled the whole native branch for three years without a single test noticing.
- **Packaging as a blind spot** (#1186, #1105): both had the same shape — a runtime dependency that
  nothing verified. The `.c` setup shipped no CRT and the packaging step could not notice, because
  NSIS only fails on a `File` line naming a missing file, never on a dependency that is named nowhere;
  `geodms.pyd` imported `python313.dll` next to a shipped `python312.dll`. The Python bindings now have
  targeted pre-packaging import/ABI checks; a general import-closure check would extend the same
  protection to every executable and DLL.
- **Least certain classifications**: #1145 still needs a debugging session before it is clear whether
  it is an afternoon or a refactor. #1215 is classified as a bookkeeping closure on the strength of
  the operator already existing, which the reporter should confirm — its body is empty.
- **Reproducing a GUI issue** is cheap: `GeoDmsGuiQt.exe /L<log> /T<script> /S1 /S2 /S3 <config.dms>`
  (note: `/L` must precede `/T`) with `ActivateItem` + `ShowDetailPage` + `SaveDetailPage` for a detail
  page, `DefaultView` + `SEND 3 3 273 9 0` + `SaveValueInfo` for a value-info page, and
  `SEND 3 3 256 <VK> 0` to feed a virtual key straight into `DataView::OnKeyDown`. For a map view the
  requested tile-matrix level is a usable measure of the zoom level: the `GridCoord` traces name the
  world size per raster pixel (26.88 m/px = ngr_layer level 7, 13.44 = 8, ...). That is how #1129 was
  measured and how #1143 and #579 were captured.
