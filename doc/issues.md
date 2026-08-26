# Open GitHub issues, classified

Snapshot of the 27 open issues at https://github.com/ObjectVision/GeoDMS/issues, updated 2026-08-24
(previous snapshots: 2026-08-20 with 33, 2026-07-31 with 48 listed, 2026-07-04 with 41,
2026-07-03 with 45 open; see
"Recently closed" at the bottom for the delta). Grouped by implementability. Buckets:

- **A. Low hanging fruit** — small, well-defined fixes; no design decisions needed.
- **B. Implementable after minor design choices** — clear scope; one or two decisions to settle first.
- **C. Refactoring** — the fix lives in internal mechanisms, not a local patch.
- **D. Needs design** — new algorithms, semantics, or architecture.
- **E. Test issues** — extending the test process.
- **F. Documentation issues** — docs/website work, no engine code.
- **G. Other** — roadmap, questions, investigations, likely-duplicates.

## A. Low hanging fruit

**Empty.** #1204 looks small but is not specified yet: the `.tfw` duplicates GeoTIFF metadata, but
some consumers still use the world file and the reporter has not said what harm its presence causes.
#1202 has two concrete diagnostic defects but still depends on the difficult worker-thread failure
from #1201. The best ready target is therefore #1200 in C, not a nominally smaller issue here.

The one small thing left over is not an issue of its own, and was found while fixing #1186:
`tools/DeployResources.cmake` silently does nothing (exit 0, no copies) when `RUNTIME_DIR` is passed
with backslashes. The real call site always passes forward slashes, so nothing is broken today — it
is a two-line guard against an unpleasant failure mode for anyone invoking the script by hand.

## B. Implementable after minor design choices

| Issue | Design choice to settle |
|---|---|
| [#1204](https://github.com/ObjectVision/GeoDMS/issues/1204) Stop writing `.tfw` beside GeoTIFF | Decide whether compatibility with consumers that still read world files outweighs removing redundant metadata, and whether `tif` and `gdalwrite.grid` should behave identically or expose an option. |
| [#1165](https://github.com/ObjectVision/GeoDMS/issues/1165) Deprecate the point-valued `range` property | Rescoped by the 20.14.0 work (`7d1a336e`): every textual rendering of a point now states its coordinate order as `xy(x; y)`, so the property is no longer order-ambiguous, and reading an untagged range already warns from `RangeStream`. What is left is (a) property-level deprecation machinery — `AbstrPropDef::IsDepreciated()` is still consulted only at `rtc/dll/src/tic/Xml/XmlTreeOut.cpp:1016`, where it hides the property from the detail page, so configuring the deprecated `Expr` property warns nothing — and (b) migrating the 19 regression configurations that still use the brace form. Decide the choke point and the warn-now/error-later timing. |
| [#1161](https://github.com/ObjectVision/GeoDMS/issues/1161) Mitigate mixed-case deprecation warnings | Still a naming decision, but the inputs are now on the shelf: `DocData()/OperatorGroups/name` dumps the authoritative 2322 registered group names, and `data/operators.csv` is the curated user-facing list, resynced against it in `5a20c5a7`. Decide the canonical casing per operator/value-type name, and whether the sweep also touches the docs and the bundled configs or only the accepted spellings. |
| [#1145](https://github.com/ObjectVision/GeoDMS/issues/1145) Export Primary Data exports wrong (Buurt) geometry | Bug, not feature: needs repro/debug on OVSRV08; fix is picking the geometry of the right domain. |
| [#990](https://github.com/ObjectVision/GeoDMS/issues/990) `union_data` with unmatching but equal-sized domains | Hard error or warning? Might existing configs rely on it? |
| [#973](https://github.com/ObjectVision/GeoDMS/issues/973) Missing VAT in dbf/gpkg export | Always emit VAT, or an export-dialog option (with ArcGIS Pro note)? |
| [#694](https://github.com/ObjectVision/GeoDMS/issues/694) Show/hide items via model parameter | Property name and GUI-vs-config override semantics; use case (lus_demo) is clear. |
| ~~[#917](https://github.com/ObjectVision/GeoDMS/issues/917) `xx_minkowski_sum` with variant as argument~~ | **DONE in 20.18.0** except its last checkbox. `{bp,bg,cgal,geos}_minkowski_sum` and `…_minkowski_difference` ship with two signatures each — `(geometry, kernel)` and `(geometry, size, variant)` — and the 48 `bp_*_i4HV`…`_dXD` names are now `oper_policy::depreciated`. Two of the six checkboxes (`geos_simplify_linestring`, `polygon_connectivity`) were already implemented before this work. The remaining one, "polygon_intersection and overlay with fault tolerant sweep operation", is an unrelated home-brewed sweep algorithm adjacent to #1205 and should become its own issue. |

## C. Refactoring

| Issue | Rationale |
|---|---|
| **NEXT: [#1200](https://github.com/ObjectVision/GeoDMS/issues/1200) Enumeration through the referred-item chain yields shadowed sub-items** | Best ready target after #1199/#421. Lookup is first-name-wins, but the stateless visible-sub-item iterator enumerates both the shadow and the unreachable referred item. Reproduce the duplicate in the XML detail page first, then introduce one stateful name-deduplicating traversal for the affected callers. Existing correct models are `TreeItem_VisitConstVisibleSubTree`, `TreeItem::Copy`, and #337's `EnumCollectCandidates`. |
| [#1202](https://github.com/ObjectVision/GeoDMS/issues/1202) Worker-thread internal errors lack item context and are duplicated as warnings | Concrete diagnostic defects, but the known reproduction is the large/heisenbug run from #1201. Trace the worker reporting and meta-thread replay paths before changing severity or deduplication. |
| [#1206](https://github.com/ObjectVision/GeoDMS/issues/1206) Spatial/CRS Debug runs leave 17 CRT blocks at shutdown | Already taken. The fixed signature across CRS/GDAL cases points at process-lifetime spatial-library cleanup; keep separate from the GUI nontermination in #1191 until stacks connect them. |
| [#1191](https://github.com/ObjectVision/GeoDMS/issues/1191) Closing the GUI during calculation leaves the process alive | Repro and dump exist. Likely scheduler teardown ordering: scheduled suppliers are discarded before active joiners are released, followed by an unbounded task-group wait. Needs stack-backed shutdown/lifetime work, not a local GUI close patch. |
| [#1105](https://github.com/ObjectVision/GeoDMS/issues/1105) geodms.pyd Python ABI mismatch across .m/.c installers | Regular Windows builds now target CPython 3.12/3.13/3.14. A separate `.g` GLOBIO build targets CPython 3.9 and the exact GDAL 3.1.4 conda stack, with isolated output/vcpkg roots, ABI-tagged modules, dependency-closure deployment, and both-import-order checks. Interactive `.m`/`.c`/`.g` release/setup validation remains open. |
| [#403](https://github.com/ObjectVision/GeoDMS/issues/403) Don't collect recollected items into subset | Changes how subitems are collected in `select_with_attr` subunits. |

## D. Needs design

| Issue | Rationale |
|---|---|
| [#1205](https://github.com/ObjectVision/GeoDMS/issues/1205) Balance polygon overlay by geometric complexity | The current outer/inner tile loops expose only the first argument's element tiling as parallel work, so a few dissolved features with millions of vertices collapse to one worker. Needs a choice between feature/vertex subdivision, parallelising both tile dimensions, prepared geometry, and a user-visible `subdivide` operator; argument-order guidance can be documented independently. |
| [#1198](https://github.com/ObjectVision/GeoDMS/issues/1198) Resource-aware admission does not converge | Follow-up to the now-closed #1158: enforce mode churns without reducing the live peak, and its committed-memory figure can exceed physical memory. Requires a corrected accounting/admission model rather than another local threshold. |
| [#1196](https://github.com/ObjectVision/GeoDMS/issues/1196) `discrete_alloc` arithmetic can overflow at production sizes | The two cheap widenings are clear (`perturbation_type` and feasibility aggregates), but shadow-price bounds and hot-path checked signed arithmetic need a design/performance decision. The issue is an audit finding, not a reproduced wrong allocation. |
| [#856](https://github.com/ObjectVision/GeoDMS/issues/856) 2-dimensional Dijkstra (time + cost) | Non-trivial pruning semantics (Pareto frontier over two criteria). |
| [#659](https://github.com/ObjectVision/GeoDMS/issues/659) R (or Python) integration for calculations | The linking route is closed for good and recorded on the wiki: R's C API needs the MinGW-w64 toolchain R itself was built with, and hosting a single-threaded, `longjmp`-based interpreter inside a thread-scheduling engine is not viable. The file-and-`exec_ec` route is the answer instead, and 20.16.0 makes it usable (`5a9d4478`: the child's stdout+stderr are captured on one pipe and reported line by line as `exec: <line>`, capped at 1 MB but still drained, and waited for in ticks). What remains under "design" is the ordering discipline — the NetworkModel_EU/Julia production example shows the batch file, not the configuration, must own any sequence that includes a GeoDMS *write*. |
| [#724](https://github.com/ObjectVision/GeoDMS/issues/724) Circular units (wrap-around grid/time) | New unit semantics rippling through operators and metric checking. |
| [#634](https://github.com/ObjectVision/GeoDMS/issues/634) Editable layer-control data via copy-on-write | Needs a copy-on-write design for calculated visualisation properties. |
| [#734](https://github.com/ObjectVision/GeoDMS/issues/734) Reuse classbreaks across mapviews | Where do classifications live, and how do views share them? |
| [#587](https://github.com/ObjectVision/GeoDMS/issues/587) Storage-read functions in keyExpr | Language-level change to make storage reads expressible in calculation rules. |
| [#302](https://github.com/ObjectVision/GeoDMS/issues/302) Winding-order reversal operator | May be subsumed by `split_polygon`/cleaning strategy — needs that decision first. |
| [#757](https://github.com/ObjectVision/GeoDMS/issues/757) Home-brewed polygon operators (remaining items) | The bg_* items are done; polygon_connectivity and fault-tolerant sweep overlay are algorithm design. |

## E. Test issues

None open. #1031 now runs the shipped testcase battery and real download-backed content for both
release flavours.

## F. Documentation issues

None open. #1080 (Academy on geodms.nl) was closed 2026-08-12.

## G. Other and recently resolved context

- [#1199](https://github.com/ObjectVision/GeoDMS/issues/1199) — Implemented and merged as
  `8b09c0f6`: indirect-expression evaluation now rejects a supply chain behind a PhaseContainer;
  phase-number determination is transactional, propagates its determining/failure sentinel, and is
  reset by invalidation. Regression battery 211/211. The issue is still open pending closure.
- **Closed:** [#1186](https://github.com/ObjectVision/GeoDMS/issues/1186) — The `.c` setup shipped no MSVC runtime
  at all, so it borrowed whatever redistributable happened to be in `System32`; `arrow.dll` failing on
  `__std_calloc_crt` proved that copy was older than 14.40. dumpbin over the whole `.c` installation
  found 6 CRT DLLs and 297 imported symbols, and all eight redistributable versions available here
  cover them. Fixed in `4b95e5dc` (CMake `InstallRequiredSystemLibraries` + build-time
  `deploy_resources`, plus `File` lines in `nsi/DmsSetupScriptX64-cmake.nsi`); the reporter will retest
  the `.c` variant at the next release and deliberately will *not* update their system redistributable,
  so the problem stays visible until then. Open follow-up worth its own issue: nothing checks the
  import closure of `bin\` against the packaged file list, so the next runtime dependency a vcpkg port
  introduces is again a manual responsibility.
- **Closed:** [#949](https://github.com/ObjectVision/GeoDMS/issues/949) — Calculation-time difference between GUI
  and Run, ~100 s. Less cheap than the July note assumed. The mechanism is indeed fixed (#1157 closed
  as a duplicate of #1156, whose GUI half shipped in 20.13.0), but the model is NetworkModel_PBL (ROO)
  and `%SourceDataDir%/NetworkModel_PBL` is not on the dev laptop, so the re-measure has to happen
  where the data lives. Attempting a substitute exposed the real trap, recorded on the issue
  2026-08-20: **GeoDmsRun and the GUI do not demand the same work from the same item**, so comparing
  two clocks compares two different calculations. `GeoDmsRun <cfg> /parameter` calculated nothing
  (0.001 s) until an IntegrityCheck forced it (6.2 s over 24M elements); the GUI's `ActivateItem` did
  not demand the value at all, and `DefaultView` computed only the visible rows of a 24M-row table.
  Both drivers must be pointed at the same *write*, and the numbers taken from the two `/L` logs.
- [#810](https://github.com/ObjectVision/GeoDMS/issues/810) — Component planning 2024/2025: roadmap
  umbrella, not an implementable issue.
- **Closed:** [#830](https://github.com/ObjectVision/GeoDMS/issues/830) — Question/investigation: why is tif
  DialogData needed while the projection choice doesn't matter? Outcome determines whether there's a
  bug at all.

## Recently closed (delta since 2026-07-31)

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
- **PhaseContainer** (#1128, #1167, #1199, all implemented; #1199 pending closure): demanded members
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
  deprecation machinery that does not exist; once built, #1161 can reuse it.
- **Export-flow cluster**: #711 is closed; #411's pattern — route table exports through the Export Primary
  Data dialog via a constructed `Desktops/Default/ViewData` config table — is the base for what
  remains: #973 (VAT option) is a dialog/driver option on top of the same machinery.
- **Packaging as a blind spot** (#1186, #1105): both had the same shape — a runtime dependency that
  nothing verified. The `.c` setup shipped no CRT and the packaging step could not notice, because
  NSIS only fails on a `File` line naming a missing file, never on a dependency that is named nowhere;
  `geodms.pyd` imported `python313.dll` next to a shipped `python312.dll`. The Python bindings now have
  targeted pre-packaging import/ABI checks; a general import-closure check would extend the same
  protection to every executable and DLL.
- **Least certain classifications**: #1145 still needs a debugging session before it is clear whether
  it is an afternoon or a refactor. #1204 needs the compatibility decision from its reporter.
- **Reproducing a GUI issue** is cheap: `GeoDmsGuiQt.exe /L<log> /T<script> /S1 /S2 /S3 <config.dms>`
  (note: `/L` must precede `/T`) with `ActivateItem` + `ShowDetailPage` + `SaveDetailPage` for a detail
  page, `DefaultView` + `SEND 3 3 273 9 0` + `SaveValueInfo` for a value-info page, and
  `SEND 3 3 256 <VK> 0` to feed a virtual key straight into `DataView::OnKeyDown`. For a map view the
  requested tile-matrix level is a usable measure of the zoom level: the `GridCoord` traces name the
  world size per raster pixel (26.88 m/px = ngr_layer level 7, 13.44 = 8, ...). That is how #1129 was
  measured and how #1143 and #579 were captured.
