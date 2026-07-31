# Open GitHub issues, classified

Snapshot of the 48 open issues at https://github.com/ObjectVision/GeoDMS/issues, updated 2026-07-31
(previous snapshots: 2026-07-04 with 41 listed, 2026-07-03 with 45 open; see "Recently closed" at
the bottom for the delta). Grouped by implementability. Buckets:

- **A. Low hanging fruit** — small, well-defined fixes; no design decisions needed.
- **B. Implementable after minor design choices** — clear scope; one or two decisions to settle first.
- **C. Refactoring** — the fix lives in internal mechanisms, not a local patch.
- **D. Needs design** — new algorithms, semantics, or architecture.
- **E. Test issues** — extending the test process.
- **F. Documentation issues** — docs/website work, no engine code.
- **G. Other** — roadmap, questions, investigations, likely-duplicates.

## A. Low hanging fruit

| Issue | Rationale |
|---|---|
| [#1162](https://github.com/ObjectVision/GeoDMS/issues/1162) Settings option to skip the start-project dialog | One option flag plus a startup-path branch; the dialog also drops behind other windows and has no taskbar entry, which is what makes it annoying. |
| [#846](https://github.com/ObjectVision/GeoDMS/issues/846) Event-log message when item finishes | "Fence container without the fence" — stripped-down variant of existing machinery. |
| [#828](https://github.com/ObjectVision/GeoDMS/issues/828) Layer control: thicker 3D lines, darker selected grey | Pure UI tweak; the exact values are specified in the issue. |

## B. Implementable after minor design choices

| Issue | Design choice to settle |
|---|---|
| [#1166](https://github.com/ObjectVision/GeoDMS/issues/1166) Nested function body cannot see container scope | Root cause is pinned: `ResolveBodySymbol` (`rtc/dll/src/tic/AbstrCalculator.cpp:2597`) ascends exactly one level. Decide what a nested body *should* see — the full parent chain for container items is clearly a bug; whether the enclosing function's parameters/locals become visible too is the actual design call. |
| [#1165](https://github.com/ObjectVision/GeoDMS/issues/1165) Deprecate the point-valued `range` property | Needs a property-level deprecation warning first (`AbstrPropDef::IsDepreciated()` today only hides the property from the detail page). Decide the choke point (`ConfigProd::DoAnyProp` vs `SetValueAsCharRange`), and warn-now/error-later timing. Scalar `range` stays untouched, so the flag must be conditional on `Point<T>`. |
| [#1161](https://github.com/ObjectVision/GeoDMS/issues/1161) Mitigate mixed-case deprecation warnings | Mechanical once the canonical casing per operator/value-type name is fixed; decide whether the sweep also touches the docs and the bundled configs, or only the accepted spellings. |
| [#1159](https://github.com/ObjectVision/GeoDMS/issues/1159) Colour calculated attributes differently | Sorting them to the top already happens; pick the colour and the exact predicate ("configured calculation rule on a read item"). |
| [#1154](https://github.com/ObjectVision/GeoDMS/issues/1154) MMD reinterprets stale caches after values-unit element-type change | Fix is agreed in outline: record the concrete element size / `ValueType` id per mmd-backed attribute and verify on open. Decide the dictionary tag's name/format and what a *missing* tag means for pre-existing caches (proposed: regenerate-required). |
| [#1145](https://github.com/ObjectVision/GeoDMS/issues/1145) Export Primary Data exports wrong (Buurt) geometry | Bug, not feature: needs repro/debug on OVSRV08; fix is picking the geometry of the right domain. |
| [#990](https://github.com/ObjectVision/GeoDMS/issues/990) `union_data` with unmatching but equal-sized domains | Hard error or warning? Might existing configs rely on it? |
| [#973](https://github.com/ObjectVision/GeoDMS/issues/973) Missing VAT in dbf/gpkg export | Always emit VAT, or an export-dialog option (with ArcGIS Pro note)? |
| [#711](https://github.com/ObjectVision/GeoDMS/issues/711) GDAL: writing column subset creates null columns | Skip other columns or write their data? Always include geometry? |
| [#694](https://github.com/ObjectVision/GeoDMS/issues/694) Show/hide items via model parameter | Property name and GUI-vs-config override semantics; use case (lus_demo) is clear. |
| [#471](https://github.com/ObjectVision/GeoDMS/issues/471) Edit Config Source on template items | Proposed solution (two menu entries) is in the issue. |
| [#421](https://github.com/ObjectVision/GeoDMS/issues/421) `sort_index` producing a new sorted domain | Operator name/signature (multi-criteria); semantics clear from the wiki workaround. |
| [#337](https://github.com/ObjectVision/GeoDMS/issues/337) `select_with_attr` variants (linked unit, parents, namespace) | Which variants, and the naming scheme. |
| [#612](https://github.com/ObjectVision/GeoDMS/issues/612) Value info for null value | Labeled "tiny issue", but first decide what should be shown (reason for null? supplier?). |
| [#319](https://github.com/ObjectVision/GeoDMS/issues/319) Improving icons | Decide the icon set (units vs containers, spatial-ref compass, grid domains); then asset work. |
| [#859](https://github.com/ObjectVision/GeoDMS/issues/859) Qt color dialog with transparency | Pick the Qt dialog/widget replacing the CommonControl. |
| [#917](https://github.com/ObjectVision/GeoDMS/issues/917) `xx_minkowski_sum` with variant as argument | Argument shape and which backends (cgal/bg/geos) to ship first. |

## C. Refactoring

| Issue | Rationale |
|---|---|
| [#1167](https://github.com/ObjectVision/GeoDMS/issues/1167) PhaseContainer silently inert when a sub-item is demanded | Same machinery as #1128: the phase's `OperationContext` is only joined for an item whose own `GetOrgDC()` is the `PhaseContainer` `FuncDC`, so a direct sub-item reference may resolve through `SupplCache` to the source and bypass the phase. Hypothesis, not yet a confirmed diagnosis — no minimal repro. |
| [#1155](https://github.com/ObjectVision/GeoDMS/issues/1155) MMD: subunit `Range` missing from `0Dictionary.dms` | `AbstrStorageManager::OpenForWrite` writes the dictionary once, at first open; subunit ranges only known later never make it in, and a var-range subunit is then unreadable. Requires reworking when the dictionary is emitted over a write session (re-emit at close / on range availability). |
| [#1128](https://github.com/ObjectVision/GeoDMS/issues/1128) PhaseContainer progress deferred until final consumer joins | Interest/commit scheduling internals; the fix is in how phase results are committed. |
| [#1105](https://github.com/ObjectVision/GeoDMS/issues/1105) geodms.pyd Python ABI mismatch across .m/.c installers | Build/packaging restructuring for a consistent bundled Python and coexistence with user GDAL/QGIS. |
| [#975](https://github.com/ObjectVision/GeoDMS/issues/975) Source description tab broken | Regression in supplier-traversal/source-description generation; likely needs reworking that traversal. |
| [#795](https://github.com/ObjectVision/GeoDMS/issues/795) Paths missing in log messages (esp. with indirection) | Requires threading item context through diagnostics for indirect expressions. |
| [#403](https://github.com/ObjectVision/GeoDMS/issues/403) Don't collect recollected items into subset | Changes how subitems are collected in `select_with_attr` subunits. |
| [#298](https://github.com/ObjectVision/GeoDMS/issues/298) Optimize `mapping(D,V)` for coordinate-separable transformations | Performance refactor of coordinate transformation storage, tied to XY-order/EPSG cleanup. |

## D. Needs design

| Issue | Rationale |
|---|---|
| [#1158](https://github.com/ObjectVision/GeoDMS/issues/1158) Out-of-memory kills GeoDMS silently | The brake exists but is disabled: `WaitForAvailableMemory` (`MemGuard.cpp:186`) is a pass-through with its wait/backoff body commented out. Re-enabling it as-is only serialises-and-hopes; a real fix needs the memory budget of the lookahead-scheduling plan, plus a decision on what "job does not fit" should *do* (fail with a GeoDMS error rather than disappear). |
| [#1156](https://github.com/ObjectVision/GeoDMS/issues/1156) EmptyWorkingSet storm; `MemoryMaxRAM_GB` conflates flush trigger and concurrency throttle | The obvious fix (default the cap to 0) was measured to be harmful — `IsLowOnFreeRAM()` is also the activation/parallelism brake, and without the cap a Redevelopment run grew to 595.8 GiB of commit and was killed. Needs the estimate-based admission control from `doc/development/schedule-with-lookahead.md`. |
| [#1157](https://github.com/ObjectVision/GeoDMS/issues/1157) GUI killed by Windows during long computations | `SetMainThreadID` makes the meta thread the Qt GUI thread; with a `SuspendTrigger` Blocker on the stack `MustSuspend()` returns false and the wait loops in `OperationContext::Join()` / `tile_task_group::AwaitRunningSlots()` never pump messages. Who runs a calculation and when it yields is a scheduling question, so this is handled as part of the lookahead-scheduling work rather than as a separate Qt fix. |
| [#856](https://github.com/ObjectVision/GeoDMS/issues/856) 2-dimensional Dijkstra (time + cost) | Non-trivial pruning semantics (Pareto frontier over two criteria). |
| [#659](https://github.com/ObjectVision/GeoDMS/issues/659) R (or Python) integration for calculations | Whole interop architecture: data marshalling, process model, error handling. |
| [#724](https://github.com/ObjectVision/GeoDMS/issues/724) Circular units (wrap-around grid/time) | New unit semantics rippling through operators and metric checking. |
| [#634](https://github.com/ObjectVision/GeoDMS/issues/634) Editable layer-control data via copy-on-write | Needs a copy-on-write design for calculated visualisation properties. |
| [#734](https://github.com/ObjectVision/GeoDMS/issues/734) Reuse classbreaks across mapviews | Where do classifications live, and how do views share them? |
| [#587](https://github.com/ObjectVision/GeoDMS/issues/587) Storage-read functions in keyExpr | Language-level change to make storage reads expressible in calculation rules. |
| [#302](https://github.com/ObjectVision/GeoDMS/issues/302) Winding-order reversal operator | May be subsumed by `split_polygon`/cleaning strategy — needs that decision first. |
| [#757](https://github.com/ObjectVision/GeoDMS/issues/757) Home-brewed polygon operators (remaining items) | The bg_* items are done; polygon_connectivity and fault-tolerant sweep overlay are algorithm design. |

## E. Test issues

- [#1031](https://github.com/ObjectVision/GeoDMS/issues/1031) — Include the bundled dms-files in the test process.
- [#943](https://github.com/ObjectVision/GeoDMS/issues/943) — Add RS trede mapview to the GUI test.

## F. Documentation issues

- [#1080](https://github.com/ObjectVision/GeoDMS/issues/1080) — Publish the Academy wiki on geodms.nl so it's Google-findable (website/docs, no engine code).

## G. Other

- [#579](https://github.com/ObjectVision/GeoDMS/issues/579) — Value info shows the export-xml variables first. Does not reproduce on the current code: with a storable item plus an `ExportSettings/MetaInfo` container — also in the shape where an indirect `StorageName` is built from an `ExportInfo` container that feeds the same MetaInfo — the captured page shows only the item, its data suppliers and the value table. Consistent with the code: the page walks `SupplierVisitFlag::Explain` (`NamedSuppliers | SourceData`), while the sidecar items hang off the separate `ExportInfo` bit. The rule from the issue comment ("skip the items from ExportInfo unless the current item is a sub-item of ExportInfo") has nothing to filter today; it needs the reporter's configuration shape before it can be implemented against something real.
- [#1164](https://github.com/ObjectVision/GeoDMS/issues/1164) — Groupby on BGT vegetatieobject gives "factor expected" in 20.8.0 but not 17.9.6. Maintainer states the fix is in 20.9.0, which is released; only confirmation on the reporter's data is left before closing.
- [#1163](https://github.com/ObjectVision/GeoDMS/issues/1163) — Grow a grid by exactly one cell in 4/8 directions. Answered with `potential`/`proximity`/`or` recipes that work on older versions, plus a `function` wrapper that needs 20.9.0. Open question is only whether a dedicated scale-independent grow operator is still wanted, or the issue closes on the documented recipe.
- [#810](https://github.com/ObjectVision/GeoDMS/issues/810) — Component planning 2024/2025: roadmap umbrella, not an implementable issue.
- [#830](https://github.com/ObjectVision/GeoDMS/issues/830) — Question/investigation: why is tif DialogData needed while the projection choice doesn't matter? Outcome determines whether there's a bug at all.
- [#949](https://github.com/ObjectVision/GeoDMS/issues/949) — Performance investigation: ~100s difference between GUI and Run for the same model; #1157 now gives a concrete mechanism to check first (the GUI thread runs the calculation and stalls on non-yielding waits).

## Recently closed (delta since 2026-07-04)

Four of these were fixed on branch `lookahead-scheduling` on 2026-07-31 (#597, #292, #1143, #1129).

- [#597](https://github.com/ObjectVision/GeoDMS/issues/597) — `min_ifdefined` / `min_alldefined` / `max_ifdefined` / `max_alldefined` added (commit `af20071e`): the aggregation analogues of `min_elem_ifdefined` / `min_elem_alldefined`, total and partitioned, over numerics and points. `_ifdefined` is null only for an aggregation without any defined value (where min reports MAX_VALUE), `_alldefined` also as soon as one undefined value is met. Test: `testcases/oper_minmax_nulls.dms`.
- [#292](https://github.com/ObjectVision/GeoDMS/issues/292) — Mitigated (commit `d076cd43`): a parsed string literal containing an unknown escape code now warns, naming the code and quoting the literal; during config load with file, line and column. The meaning of a backslash is unchanged, so no config breaks. Test: `testcases/escape_codes.dms`.
- [#1143](https://github.com/ObjectVision/GeoDMS/issues/1143) — Fixed (commit `b9ee7e50`). The discriminator was not the indirect StorageName but mmd vs fss: an .mmd holder takes its range from the storage dictionary, which sets `USF_HasConfigRange` and hence `HasCalculator()`, and the detail page suppresses storage rows for a calculated item under a read-only storage. The rows are now shown whenever the item IS the storage holder, and a read-only storage is labelled DataSource instead of DataTarget.
- [#1129](https://github.com/ObjectVision/GeoDMS/issues/1129) — Fixed (commit `8e1e2bae`), two independent causes: only the numeric keypad zoomed (the main-row keys arrive as `VK_OEM_PLUS`/`VK_OEM_MINUS` and '+' needs Shift, which made `IsSpec()` false), and over a WMS background the tile-grid snap counted as the whole step at 1% change, so the first press did nothing. Measured over an `ngr_layer` background: 1..4 presses used to reach tile levels 7/8/9/10, now 8/9/10/11.
- [#1160](https://github.com/ObjectVision/GeoDMS/issues/1160) — Column width not adjustable after Toggle Rows and Cols. Closed 2026-07-31 as a duplicate of #1150: the same defect, already fixed by the axis-aware resize pipeline, but reported against a build without that fix.
- [#1153](https://github.com/ObjectVision/GeoDMS/issues/1153) — Alt+R froze the application. Opened and closed the same day (2026-07-13).
- [#1152](https://github.com/ObjectVision/GeoDMS/issues/1152) — Crash reading an mmd attribute (`ID.cpp:66`, null `trd` for an argument passed as `SharedTreeItem` under `oper_arg_policy::calc_as_result`). Now fails explicitly with "no primary data found in storage" — which is what exposed #1155.
- [#1151](https://github.com/ObjectVision/GeoDMS/issues/1151) — Panel slider widened for clickability.
- [#1150](https://github.com/ObjectVision/GeoDMS/issues/1150) — Toggle Rows and Columns left the table unresizable. The resize pipeline was hard-coded to the horizontal axis; it is now axis-aware in both orientations, with non-sticky `SIZENS`/`SIZEWE` cursors and viewport re-anchoring. #1160 was closed as its duplicate.
- [#1149](https://github.com/ObjectVision/GeoDMS/issues/1149) — Statistics copy-to-clipboard now starts with an `Item name <full path>` row (commit `871a9b59`, branch `refactor_ownership`), for both the statistics table and the frequency table; also fixed a latent infinite loop in `ReplaceChar` escaping.
- [#1124](https://github.com/ObjectVision/GeoDMS/issues/1124) — Closed as completed: superseded by #411. The floppy now opens the Export Primary Data dialog, and `ExportSettings/MetaInfo` sidecars are written for every storage-manager export from that dialog. (The native-CSV path `DoExportTableorDatabaseToCSV` still bypasses storage managers and writes no sidecar — file a fresh issue if that matters.)
- [#515](https://github.com/ObjectVision/GeoDMS/issues/515) — Overview map too zoomed out. Capped in `ViewPort::CalcMaxWorldRect()` + `GraphicRect::AdjustTargetVieport()` at the background layer's extent, falling back to the world-crd-unit's `range()` (commit `3d5b5c11`, branch `refactor_ownership`). This one was open at the 2026-07-04 snapshot but missing from that list.
- [#367](https://github.com/ObjectVision/GeoDMS/issues/367) — GDAL cannot process UNC paths. Root cause: `ConvertDosFileName` encodes `\\server\share` as the internal `file://server/share`, which two consumers handed straight to a third-party API — `GDALOpenEx` (`gdal_base.cpp`, plus the block-size probe in `gdal_grid.cpp`) and `StgOpenStorage`/`StgCreateDocfile` in `CompoundStorageManager`. Fixed by wrapping those call sites in `ConvertDmsFileName` (commit `d304f2d2`, branch `MapView_Tilting`); no-op for local paths, `/vsi…` prefixes and on Linux. End-to-end read from `\\OVSRV06\...` still to be verified.

Two fixes without their own issue, both found while working on the above (branch
`lookahead-scheduling`):

- Writing an `ExportSettings` sidecar read the MetaInfo's `FileName` and `FileType` without
  holding interest, so **every** sidecar write tripped `assert(adi->GetInterestCount())` and the
  `PrepareDataUsage` interest assert: GeoDmsRun exited with code 3 (after writing both files) and
  the GUI froze on the assert dialog for any config with an ExportSettings container. Commit
  `14beadc1` takes interest the way `GenerateMetaInfo` already does for sections and keys.
- The `SaveValueInfo` test-script command opened its output file and wrote nothing (the body was
  commented out), so no test could assert anything about a value-info page. Commit `176980a4`
  writes the rendered page; the `t1640_value_info` and `t1642_value_info_group_by` references
  compare real content for the first time and probably need to be re-recorded.

## Cross-cutting observations

- **Memory and liveness under load** (#1156, #1157, #1158, and #949 as a symptom): three separate
  reports of the same gap — the engine has one binary "is RAM low?" brake that doubles as the
  concurrency throttle, no cost/footprint estimate per task, and a disabled memory wait. Each
  local fix was measured to make something else worse. The design plan is
  `doc/development/schedule-with-lookahead.md` (resource estimation on `Operator`, admission
  control, automated phasing); implementation is under way on branch `lookahead-scheduling`
  (retained-result accounting, admission gate, drain mode, retry discipline). #1157 belongs to the
  same cluster: which thread runs a task and when it may yield is decided by the scheduler, so the
  message-pump starvation is addressed there rather than by a separate Qt-side fix.
- **PhaseContainer** (#1128, #1167): progress reporting and, possibly, the fence itself hang off
  the same join hook, which only fires for the container's own `FuncDC`. §3 of the
  lookahead-scheduling plan (automated phasing) would make most manual fences unnecessary, so
  it is worth confirming #1167 before investing in the current mechanism.
- **MMD storage robustness** (#1154, #1155, closed #1152): the dictionary records too little
  (no element type) and too late (only at first open-for-write), and stale or incomplete caches
  surface as internal check failures rather than storage errors. Both open items are contained
  fixes in the same file pair and are best done together.
- **Config language / syntax** (#1161, #1165, #1166, and #1163's function wrapper): fallout of the
  `hof_syntax` work. #1165 needs a property-level deprecation warning that does not exist yet;
  once built, #1161 can reuse it.
- **Export-flow cluster**: of the original seven (#823, #411, #1124, #630, #872, #973, #711), five
  are closed. #411's pattern — route table exports through the Export Primary Data dialog via a
  constructed `Desktops/Default/ViewData` config table — is the base for what remains: #973 (VAT
  option) and #711 (column-subset behavior) are dialog/driver options on top of the same machinery.
- **Least certain classifications**: #1145 and #975 still need a debugging session before it's
  clear whether they're an afternoon or a refactor; #1167 is an unconfirmed hypothesis without a
  minimal repro; #579 does not reproduce at all on the current code (see G).
- **Reproducing a GUI issue** is cheap now that the pieces are in place: `GeoDmsGuiQt.exe
  /L<log> /T<script> /S1 /S2 /S3 <config.dms>` (note: `/L` must precede `/T`) with
  `ActivateItem` + `ShowDetailPage` + `SaveDetailPage` for a detail page, `DefaultView` +
  `SEND 3 3 273 9 0` + `SaveValueInfo` for a value-info page, and `SEND 3 3 256 <VK> 0` to feed a
  virtual key straight into `DataView::OnKeyDown`. For a map view the requested tile-matrix level
  is a usable measure of the zoom level: the `GridCoord` traces name the world size per raster
  pixel (26.88 m/px = ngr_layer level 7, 13.44 = 8, ...). That is how #1129 was measured and how
  #1143 and #579 were captured.
