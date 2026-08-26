# Open GitHub issues, classified

Snapshot of the **16 open issues** at https://github.com/ObjectVision/GeoDMS/issues, re-audited
against GitHub on **2026-08-26**. The previous header claimed 27; sixteen issues it still classified
had been closed and five open ones were missing, so the tables below were rebuilt from the live list
rather than edited in place; #1217 was filed later that afternoon and is added here.
Grouped by implementability. Buckets:

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

| Issue | Why it is small |
|---|---|
| **NEXT: [#1215](https://github.com/ObjectVision/GeoDMS/issues/1215) polygon_connectivity** | **Verify and close.** Split out of #757 on 2026-08-26 with an empty body, but `polygon_connectivity` is already implemented: `cogPC` at `geo/dll/src/BoostPolygon.cpp` plus the `bp_`/`bg_`/`cgal_`/`geos_polygon_connectivity` variants, all four registered and documented on the wiki. Unless the reporter meant something beyond the existing operator, this is a bookkeeping closure. |
| [#1217](https://github.com/ObjectVision/GeoDMS/issues/1217) Drag Layer Control functionality unclear | **Fixed in code, awaiting closure.** The two map-view pop-up items resize the legend column by 10 pixels a step (`MapControl::ShiftLayerControlSlider`), but their captions still named **Ctrl-S**/**Ctrl-D**, the keys that `f7e9a1a7c` (#1011) unbound in 18.2.1 because **Ctrl-D** had become Table View. Captions now say what they do and name **Ctrl-Shift-Left**/**Ctrl-Shift-Right**; the behaviour is documented on the wiki page *Map view Legend*. All three sub-questions answered; nothing further to implement. |
| [#1211](https://github.com/ObjectVision/GeoDMS/issues/1211) Icons for view menu options and view titles | Every chart window currently reuses the Statistics view icon. The reporter attached the icons they want; the work is wiring them into the view registry, the same table #319 touched for the tree-item icons (`518dc747`, `0ce9f427`). No semantics involved. |

## B. Implementable after minor design choices

| Issue | Design choice to settle |
|---|---|
| [#1165](https://github.com/ObjectVision/GeoDMS/issues/1165) Deprecate the point-valued `range` property | Rescoped by the 20.14.0 work (`7d1a336e`): every textual rendering of a point now states its coordinate order as `xy(x; y)`, so the property is no longer order-ambiguous, and reading an untagged range already warns from `RangeStream`. What is left is (a) property-level deprecation machinery — `AbstrPropDef::IsDepreciated()` is still consulted only at `rtc/dll/src/tic/Xml/XmlTreeOut.cpp:1016`, where it hides the property from the detail page, so configuring the deprecated `Expr` property warns nothing — and (b) migrating the 19 regression configurations that still use the brace form. Decide the choke point and the warn-now/error-later timing. |
| [#1161](https://github.com/ObjectVision/GeoDMS/issues/1161) Mitigate mixed-case deprecation warnings | Still a naming decision, but the inputs are on the shelf: `DocData()/OperatorGroups/name` dumps the authoritative registered group names and `data/operators.csv` is the curated user-facing list, resynced in `5a20c5a7`. The #917 work removed 48 mixed-case names from the problem set by depreciating the `bp_*_i4HV`…`_dXD` family, so the remaining set is smaller than the last count suggests — re-dump before deciding. Decide the canonical casing per operator/value-type name, and whether the sweep also touches the docs and the bundled configs or only the accepted spellings. |
| [#1145](https://github.com/ObjectVision/GeoDMS/issues/1145) Export Primary Data exports wrong (Buurt) geometry | Bug, not feature: needs repro/debug on OVSRV08; the fix is picking the geometry of the right domain. |
| [#990](https://github.com/ObjectVision/GeoDMS/issues/990) `union_data` with unmatching but equal-sized domains | Hard error or warning? Might existing configs rely on it? |

## C. Refactoring

| Issue | Rationale |
|---|---|
| [#1212](https://github.com/ObjectVision/GeoDMS/issues/1212) `geos_polygon` mis-reads a uniformly counter-clockwise polygon | Split off from the closed #302 while building the winding-order operators. `geos_create_polygons` decides shell-versus-hole from a ring's **absolute** orientation — a ring is a shell iff it is clockwise — so a feature whose rings are uniformly counter-clockwise loses its shell and promotes its lake. The fix is to derive ring roles from nesting, which is what `fix_winding_order` already does (`302`'s output); the refactor is sharing that determination between the GEOS and boost.geometry readers instead of each deciding for itself. |
| [#1191](https://github.com/ObjectVision/GeoDMS/issues/1191) Closing the GUI during calculation leaves the process alive | Repro and dump exist. Likely scheduler teardown ordering: scheduled suppliers are discarded before active joiners are released, followed by an unbounded task-group wait. Needs stack-backed shutdown/lifetime work, not a local GUI close patch. Note #1206, which shared a suspicion of process-lifetime cleanup, closed separately on 2026-08-24 (`12f6eb90`) without connecting the two stacks. |

## D. Needs design

| Issue | Rationale |
|---|---|
| [#1214](https://github.com/ObjectVision/GeoDMS/issues/1214) Fault-tolerant sweep for polygon_intersection and overlay | Split out of #757 on 2026-08-26; also the last unchecked box of the now-closed #917. Settle what "fault tolerant" promises (a valid result always, or a valid result plus a diagnostic saying where tolerance was applied — silently snapping geometry is the failure mode to avoid), where the tolerance comes from, and whether it replaces the per-backend cleanup pre-passes (`fix_polygon`/MakeValid, `clean_bg_geometry`, CGAL `Polygon_repair`), which today run *before* the overlay rather than being tolerance inside the sweep. Touches the same code as #1205 and should be designed with it: a tolerant sweep changes the per-feature cost model #1205's subdivision decisions rest on. Design notes are on the issue. |
| [#1205](https://github.com/ObjectVision/GeoDMS/issues/1205) Balance polygon overlay by geometric complexity | The current outer/inner tile loops expose only the first argument's element tiling as parallel work, so a few dissolved features with millions of vertices collapse to one worker. Needs a choice between feature/vertex subdivision, parallelising both tile dimensions, prepared geometry, and a user-visible `subdivide` operator; argument-order guidance can be documented independently. |
| [#1198](https://github.com/ObjectVision/GeoDMS/issues/1198) Resource-aware admission does not converge | Follow-up to the closed #1158: enforce mode churns without reducing the live peak, and its committed-memory figure can exceed physical memory. Requires a corrected accounting/admission model rather than another local threshold. |
| [#1196](https://github.com/ObjectVision/GeoDMS/issues/1196) `discrete_alloc` arithmetic can overflow at production sizes | The two cheap widenings are clear (`perturbation_type` and feasibility aggregates), but shadow-price bounds and hot-path checked signed arithmetic need a design/performance decision. The issue is an audit finding, not a reproduced wrong allocation. |
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

### Closed on 2026-08-26 (9)

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
  deprecation machinery that does not exist; once built, #1161 can reuse it.
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
