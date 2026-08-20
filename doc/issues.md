# Open GitHub issues, classified

Snapshot of the 33 open issues at https://github.com/ObjectVision/GeoDMS/issues, updated 2026-08-20
(previous snapshots: 2026-07-31 with 48 listed, 2026-07-04 with 41, 2026-07-03 with 45 open; see
"Recently closed" at the bottom for the delta). Grouped by implementability. Buckets:

- **A. Low hanging fruit** — small, well-defined fixes; no design decisions needed.
- **B. Implementable after minor design choices** — clear scope; one or two decisions to settle first.
- **C. Refactoring** — the fix lives in internal mechanisms, not a local patch.
- **D. Needs design** — new algorithms, semantics, or architecture.
- **E. Test issues** — extending the test process.
- **F. Documentation issues** — docs/website work, no engine code.
- **G. Other** — roadmap, questions, investigations, likely-duplicates.

## A. Low hanging fruit

**Empty.** All three were implemented on 2026-08-20 (#859, #846, #828), after #1162 and #612 went
the same way earlier in the month and the 2026-08 operator sweep (#1168-#1178) took the rest of that
size class. Nothing in B is one decision away from dropping in here; the next candidates all need a
naming or scoping call first (#1161, #990, #973, #711). #1031 is back in E, but what it is waiting
for is a packaging decision, not an afternoon.

The one small thing left over is not an issue of its own, and was found while fixing #1186:
`tools/DeployResources.cmake` silently does nothing (exit 0, no copies) when `RUNTIME_DIR` is passed
with backslashes. The real call site always passes forward slashes, so nothing is broken today — it
is a two-line guard against an unpleasant failure mode for anyone invoking the script by hand.

## B. Implementable after minor design choices

| Issue | Design choice to settle |
|---|---|
| [#1165](https://github.com/ObjectVision/GeoDMS/issues/1165) Deprecate the point-valued `range` property | Rescoped by the 20.14.0 work (`7d1a336e`): every textual rendering of a point now states its coordinate order as `xy(x; y)`, so the property is no longer order-ambiguous, and reading an untagged range already warns from `RangeStream`. What is left is (a) property-level deprecation machinery — `AbstrPropDef::IsDepreciated()` is still consulted only at `rtc/dll/src/tic/Xml/XmlTreeOut.cpp:1016`, where it hides the property from the detail page, so configuring the deprecated `Expr` property warns nothing — and (b) migrating the 19 regression configurations that still use the brace form. Decide the choke point and the warn-now/error-later timing. |
| [#1161](https://github.com/ObjectVision/GeoDMS/issues/1161) Mitigate mixed-case deprecation warnings | Still a naming decision, but the inputs are now on the shelf: `DocData()/OperatorGroups/name` dumps the authoritative 2322 registered group names, and `data/operators.csv` is the curated user-facing list, resynced against it in `5a20c5a7`. Decide the canonical casing per operator/value-type name, and whether the sweep also touches the docs and the bundled configs or only the accepted spellings. |
| [#1145](https://github.com/ObjectVision/GeoDMS/issues/1145) Export Primary Data exports wrong (Buurt) geometry | Bug, not feature: needs repro/debug on OVSRV08; fix is picking the geometry of the right domain. |
| [#990](https://github.com/ObjectVision/GeoDMS/issues/990) `union_data` with unmatching but equal-sized domains | Hard error or warning? Might existing configs rely on it? |
| [#973](https://github.com/ObjectVision/GeoDMS/issues/973) Missing VAT in dbf/gpkg export | Always emit VAT, or an export-dialog option (with ArcGIS Pro note)? |
| [#711](https://github.com/ObjectVision/GeoDMS/issues/711) GDAL: writing column subset creates null columns | Skip other columns or write their data? Always include geometry? |
| [#694](https://github.com/ObjectVision/GeoDMS/issues/694) Show/hide items via model parameter | Property name and GUI-vs-config override semantics; use case (lus_demo) is clear. |
| [#471](https://github.com/ObjectVision/GeoDMS/issues/471) Edit Config Source on template items | The decision is settled — the maintainer confirmed the issue's own proposal of two menu entries. What is not settled is where the second location comes from: `openConfigSourceFor` (`qtgui/exe/src/DmsMainWindow.cpp:884`) reads `GetConfigFileName/LineNr/ColNr`, which climb to the nearest *located* ancestor, and `TreeItem::GetSourceItem()` follows *referred* items, not template instantiation. Identify that link first; the menu half is trivial. |
| [#421](https://github.com/ObjectVision/GeoDMS/issues/421) `sort_index` producing a new sorted domain | Operator name/signature (multi-criteria); semantics clear from the wiki workaround. |
| [#337](https://github.com/ObjectVision/GeoDMS/issues/337) `select_with_attr` variants (linked unit, parents, namespace) | Which variants, and the naming scheme. |
| [#319](https://github.com/ObjectVision/GeoDMS/issues/319) Improving icons | Decide the icon set (units vs containers, spatial-ref compass, grid domains); then asset work. |
| [#917](https://github.com/ObjectVision/GeoDMS/issues/917) `xx_minkowski_sum` with variant as argument | Argument shape and which backends (cgal/bg/geos) to ship first. |

## C. Refactoring

| Issue | Rationale |
|---|---|
| [#1183](https://github.com/ObjectVision/GeoDMS/issues/1183) Evaluate merging rtc+clc+geo into one DLL | New, and deliberately parked: after the rtc+sym+tic merge, ~2100 of Rtc.dll's 3178 live exports serve only Clc/Geo, which also generate 71% of all cross-DLL import traffic. The binding constraint is the 4 GB PDB cap (Debug Clc.pdb alone is 3.2 GB), mitigated by `/pdbpagesize:16384`, which is being adopted anyway. Decision input is post-de-export dumpbin counts and measured big-link wall time. Analysis in `doc/development/tu-reorg-and-export-surface-2026-08.md` §5. |
| [#1167](https://github.com/ObjectVision/GeoDMS/issues/1167) PhaseContainer silently inert when a sub-item is demanded | Same machinery as #1128: the phase's `OperationContext` is only joined for an item whose own `GetOrgDC()` is the `PhaseContainer` `FuncDC`, so a direct sub-item reference may resolve through `SupplCache` to the source and bypass the phase. Hypothesis, not yet a confirmed diagnosis — still no minimal repro. |
| [#1128](https://github.com/ObjectVision/GeoDMS/issues/1128) PhaseContainer progress deferred until final consumer joins | Interest/commit scheduling internals; the fix is in how phase results are committed. |
| [#1105](https://github.com/ObjectVision/GeoDMS/issues/1105) geodms.pyd Python ABI mismatch across .m/.c installers | Build/packaging restructuring for a consistent bundled Python and coexistence with user GDAL/QGIS. One concrete sub-defect is now pinned and is much smaller than the issue as a whole: `bin\Release\x64\geodms.pyd` imports `python313.dll` while the same directory ships `python312.dll` — whatever copies the Python runtime is not keyed to the Python that `python/dll` links against. |
| [#975](https://github.com/ObjectVision/GeoDMS/issues/975) Source description tab broken | Regression in supplier-traversal/source-description generation; likely needs reworking that traversal. |
| [#795](https://github.com/ObjectVision/GeoDMS/issues/795) Paths missing in log messages (esp. with indirection) | Requires threading item context through diagnostics for indirect expressions. |
| [#403](https://github.com/ObjectVision/GeoDMS/issues/403) Don't collect recollected items into subset | Changes how subitems are collected in `select_with_attr` subunits. |

## D. Needs design

| Issue | Rationale |
|---|---|
| [#1158](https://github.com/ObjectVision/GeoDMS/issues/1158) Out-of-memory kills GeoDMS silently | The two neighbours it was bracketed with are closed, and the machinery the July note asked for now exists — estimation, admission gate, drain mode — but it does not yet solve this. `ResourceAwareScheduling` ships **off** (`rtc/dll/src/utl/Environment.cpp`; 0=off, 1=shadow, 2=enforce, switched on with `/Sq`/`/SQ`): measured on t641_2, enforce parked 124 184 operations and left the live peak at 171.9 GiB, identical to the run without it, because 91% of the volume is deferred material produced tile-by-tile by already-admitted chains and 99.2% of refusals end in a lift. `WaitForAvailableMemory` (`rtc/dll/src/utl/MemGuard.cpp:195`) is still a pass-through to `ConsiderMakingFreeSpace` with its wait/backoff body commented out. So the open question is unchanged and now isolated: what should "this job does not fit" *do* — and whatever that is, it must end in a GeoDMS error rather than the process disappearing. |
| [#856](https://github.com/ObjectVision/GeoDMS/issues/856) 2-dimensional Dijkstra (time + cost) | Non-trivial pruning semantics (Pareto frontier over two criteria). |
| [#659](https://github.com/ObjectVision/GeoDMS/issues/659) R (or Python) integration for calculations | The linking route is closed for good and recorded on the wiki: R's C API needs the MinGW-w64 toolchain R itself was built with, and hosting a single-threaded, `longjmp`-based interpreter inside a thread-scheduling engine is not viable. The file-and-`exec_ec` route is the answer instead, and 20.16.0 makes it usable (`5a9d4478`: the child's stdout+stderr are captured on one pipe and reported line by line as `exec: <line>`, capped at 1 MB but still drained, and waited for in ticks). What remains under "design" is the ordering discipline — the NetworkModel_EU/Julia production example shows the batch file, not the configuration, must own any sequence that includes a GeoDMS *write*. |
| [#724](https://github.com/ObjectVision/GeoDMS/issues/724) Circular units (wrap-around grid/time) | New unit semantics rippling through operators and metric checking. |
| [#634](https://github.com/ObjectVision/GeoDMS/issues/634) Editable layer-control data via copy-on-write | Needs a copy-on-write design for calculated visualisation properties. |
| [#734](https://github.com/ObjectVision/GeoDMS/issues/734) Reuse classbreaks across mapviews | Where do classifications live, and how do views share them? |
| [#587](https://github.com/ObjectVision/GeoDMS/issues/587) Storage-read functions in keyExpr | Language-level change to make storage reads expressible in calculation rules. |
| [#302](https://github.com/ObjectVision/GeoDMS/issues/302) Winding-order reversal operator | May be subsumed by `split_polygon`/cleaning strategy — needs that decision first. |
| [#757](https://github.com/ObjectVision/GeoDMS/issues/757) Home-brewed polygon operators (remaining items) | The bg_* items are done; polygon_connectivity and fault-tolerant sweep overlay are algorithm design. |

## E. Test issues

- [#1031](https://github.com/ObjectVision/GeoDMS/issues/1031) — Include the bundled dms-files in the
  test process. Reopened the same day it was closed, because the first pass had the premise wrong:
  `prelude.dms` is not the only `.dms` the setup ships. `CopyResources.vcxproj` robocopies the repo's
  `library\` and `examples\` folders into `bin\<Config>d\`, and the setup script packages both
  wholesale with `File …\library\*.*` / `…\examples\*.*` — nine shipped `.dms` files, not one. All
  of them are covered now (`869e16da`, `b8d44470`; suite 207/207), and doing it turned up two breaks
  in shipped code, both fixed: `library/Units.dms` used `uur` and `yr`, defined nowhere in it, so
  three of its 42 units did not resolve; and `examples/function.dms` did not compile at all against
  the current definition check. **What is left is one decision**, not more work: `.c` and `.m` do not
  ship the same library. `tools/DeployResources.cmake` copies from both `CopyResources/<dir>` and the
  top-level `<dir>`, the msbuild project only from the top level, so a `.c` installation ships
  `library/geometry/Grid2Poly_ipoint.dms` and `examples/grid_to_vector.dms` that a `.m` installation
  does not. Drop the `CopyResources/` copies, or promote those two files so both flavours ship them
  and they can be tested like the rest. Same class of gap as #1186.

## F. Documentation issues

None open. #1080 (Academy on geodms.nl) was closed 2026-08-12.

## G. Other

- [#1186](https://github.com/ObjectVision/GeoDMS/issues/1186) — The `.c` setup shipped no MSVC runtime
  at all, so it borrowed whatever redistributable happened to be in `System32`; `arrow.dll` failing on
  `__std_calloc_crt` proved that copy was older than 14.40. dumpbin over the whole `.c` installation
  found 6 CRT DLLs and 297 imported symbols, and all eight redistributable versions available here
  cover them. Fixed in `4b95e5dc` (CMake `InstallRequiredSystemLibraries` + build-time
  `deploy_resources`, plus `File` lines in `nsi/DmsSetupScriptX64-cmake.nsi`); the reporter will retest
  the `.c` variant at the next release and deliberately will *not* update their system redistributable,
  so the problem stays visible until then. Open follow-up worth its own issue: nothing checks the
  import closure of `bin\` against the packaged file list, so the next runtime dependency a vcpkg port
  introduces is again a manual responsibility.
- [#949](https://github.com/ObjectVision/GeoDMS/issues/949) — Calculation-time difference between GUI
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
- [#830](https://github.com/ObjectVision/GeoDMS/issues/830) — Question/investigation: why is tif
  DialogData needed while the projection choice doesn't matter? Outcome determines whether there's a
  bug at all.

## Recently closed (delta since 2026-07-31)

Thirty-eight issues, in four groups. The version went 20.10.0 -> 20.16.0 over the same window.

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

- **Memory and liveness under load** (#1158, with #1156 and #1157 now closed and #949 pending a
  re-measure): the liveness half is done — the GUI no longer ghosts to "(Not Responding)", and the
  flush trigger is no longer the only brake. The memory half is not, and the interesting result is
  negative: resource-aware scheduling is implemented, measures well in shadow mode (98% derived
  estimates, booked-vs-cardinality ratio 1.00 over 107 494 results) and still ships off, because on
  t641_2 enforce parked 124 184 operations for an identical 171.9 GiB peak. Refusing individual
  operations is the wrong lever when 91% of the volume is deferred material from chains that were
  already admitted. `MemoryDrainage` is on by default; `WaitForAvailableMemory` remains disabled.
  Plan and measurements: `doc/development/schedule-with-lookahead.md`.
- **PhaseContainer** (#1128, #1167): progress reporting and, possibly, the fence itself hang off the
  same join hook, which only fires for the container's own `FuncDC`. §3 of the lookahead-scheduling
  plan (automated phasing) would make most manual fences unnecessary, so it is worth confirming
  #1167 before investing in the current mechanism. Still no repro, but #846's `CalcAndWrite` gives a
  cheaper vantage point on the same symptom: wrapping a *container* announces nothing when only a
  sub-item is demanded, because `wrapped/x` resolves straight to the source `x` and never asks the
  operator for its result. Not the same route as PhaseContainer's mirror tree, but the same
  sub-item-steps-over-the-wrapper shape the #1167 hypothesis is about.
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
- **Export-flow cluster**: unchanged. #411's pattern — route table exports through the Export Primary
  Data dialog via a constructed `Desktops/Default/ViewData` config table — is the base for what
  remains: #973 (VAT option) and #711 (column-subset behavior) are dialog/driver options on top of the
  same machinery.
- **Packaging as a blind spot** (#1186, #1105): both are the same shape — a runtime dependency that
  nothing verifies. The `.c` setup shipped no CRT and the packaging step could not notice, because
  NSIS only fails on a `File` line naming a missing file, never on a dependency that is named nowhere;
  `geodms.pyd` imports `python313.dll` next to a shipped `python312.dll`. An import-closure check of
  `bin\` against the packaged file list would catch both classes.
- **Least certain classifications**: #1145 and #975 still need a debugging session before it's clear
  whether they're an afternoon or a refactor; #1167 is an unconfirmed hypothesis without a minimal
  repro.
- **Reproducing a GUI issue** is cheap: `GeoDmsGuiQt.exe /L<log> /T<script> /S1 /S2 /S3 <config.dms>`
  (note: `/L` must precede `/T`) with `ActivateItem` + `ShowDetailPage` + `SaveDetailPage` for a detail
  page, `DefaultView` + `SEND 3 3 273 9 0` + `SaveValueInfo` for a value-info page, and
  `SEND 3 3 256 <VK> 0` to feed a virtual key straight into `DataView::OnKeyDown`. For a map view the
  requested tile-matrix level is a usable measure of the zoom level: the `GridCoord` traces name the
  world size per raster pixel (26.88 m/px = ngr_layer level 7, 13.44 = 8, ...). That is how #1129 was
  measured and how #1143 and #579 were captured.
