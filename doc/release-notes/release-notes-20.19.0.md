**Pre-release.** GeoDms 20.19.0 follows **20.17.0** — 20.18.0 was built but never published, so these notes cover both. The headline work is a second Dijkstra engine (bi-criteria / pareto routing), nesting-based winding-order operators with a repair to the polygon readers themselves, a round of chart-view fixes, and IntegrityChecks that now also reach items read from a storage and items behind an `ExplicitSuppliers` edge. Beside that: a fourth build flavour, `.g` for GLOBIO, shipped here for the first time — and four hangs and crashes that ended a session with no error at all.

| Flavour | Asset |
|---|---|
| `.m` — Windows, MSBuild | `GeoDms20.19.0.m-Setup-x64.exe` |
| `.c` — Windows, CMake | `GeoDms20.19.0.c-Setup-x64.exe` |
| `.g` — Windows, GLOBIO compatibility | `GeoDms20.19.0.g-Setup-x64.exe` |
| `.l` — Linux, Ubuntu 24.04 | `GeoDms20.19.0.l-linux-x64.deb`, `.tar.gz` (+ `.sha256`, `.sha256.p7s`) |

## Networks and allocation

- **Bi-criteria (pareto) `impedance_matrix` (#856).** A second Dijkstra engine that keeps the pareto-optimal set over two impedances instead of collapsing them into one weighted cost, selected by the specification. The single-criterion engine is unchanged.
- **`od:StartPoint_rel` is written (#1210).** The member was created, allocated and committed but never filled, so requesting it yielded uninitialised memory — NYI since 2022. Origin provenance is now recorded per node in the heap, in the dense and the sparse two-pass path, and in pareto mode. Existing specifications allocate nothing extra.
- **`discrete_alloc` arithmetic audited (#1196).** The claim aggregates were summed in `UInt32`: two land use types at the "no limit" maximum claim sum to exactly 2^32, read back as **zero**, and the run was rejected as infeasible — four cells are enough to hit it, no grid needed. Shadow prices now go through checked arithmetic and report which component overflowed, and six `discrete_alloc_*_pi64` names offer a 64-bit perturbation for models that need it. `greedy_alloc` and `needy_alloc` deliberately have no such twin.
- **#403** — downstream collected attributes are no longer collected into the subset.

## Polygons

- **Nesting-based winding-order operators (#302)**, on GEOS: `fix_winding_order`, `fix_polygon`, `has_correct_winding` and `geos_polygons_by_nesting`.
- **A ring's role is read relative to the first ring, not to the compass (#1212).** The GEOS and CGAL polygon readers judged each ring absolutely — shell iff clockwise — so a feature whose rings are *uniformly* counter-clockwise, which is what a source listing coordinates in latitude/longitude order gives you, lost its shell and promoted the lake behind it. Nothing was rejected and nothing was empty: just ordinary-looking geometry with the wrong area (32 where 68 is right). **A configuration's areas may change on upgrade; the older numbers were the wrong ones.** `bg_polygon` already did this and is unaffected.
- **`points2polygon` no longer requires a closed ring (#1219)** — the readers close themselves; this was a Debug assertion that Release never needed.
- **Minkowski sums take the kernel as an argument (#917):** `{bp,bg,cgal,geos}_minkowski_sum` / `_minkowski_difference`, deprecating 48 `bp_*_i4HV`…`_dXD` names.

## IntegrityChecks and fences

- **A check now guards an item read from a storage (#1209).** Such an item has no calculation rule, so there was no DataController to fold ancestor checks into and consumers were handed an unguarded source reference; the check ran only out-of-band, after data preparation — the *"niet of te laat"* of the issue.
- **A check on an `ExplicitSuppliers` item guards the item that declares it (#1218).** Declaring a supplier means "evaluate me first", so whatever guards that supplier — its own check, its ancestors', its suppliers', transitively — is now folded into the declaring item's checked expression and fails it in-band. Selecting either item in the TreeView previously ran no check at all.
- **The phase scan honours the static argument policy (#1224).** A configuration computing an `ExplicitSuppliers` list from the member *names* of a phase-fenced container stopped loading: the fence scan walked an argument the engine declares `calc_never`, so the name list inherited the fence's phase and was rejected by the #1199 rule — which is aimed at expressions that read a *value* from behind a fence.
- **#1199** — indirect expressions whose supply chain runs behind a `PhaseContainer` are rejected, and phase-number determination is transactional.

## Charts and views

- **A new chart no longer waits for its own classification (#1221).** Creating a chart on an attribute whose values unit carries no cdf queued the Jenks-Fisher writer and then read-locked the very ClassBreaks it had just ordered, from a wait that dispatches no messages: one core pegged, and the GUI had to be killed.
- **Position axes fit their data (#1222).** Every axis was framed so that zero stayed in view, so a classification holding 755..764 packed all ten bars into the last 1.3% of the width. Zero is kept where an extent is read as a length — a histogram's bar height, a bar chart's baseline — and dropped everywhere it is a position.
- **Axis labels (#1207, #1223).** A row-number X axis is labelled from the domain's `Label` attribute where it has one, with overlapping labels skipped, and tick labels take the thousand separator, so a chart reads 3,600,000 like the table beside it.
- **Each chart kind has its own icon (#1211)**, and the View and Window menus and the window titles are drawn from the TreeView's icon set (#1220), where they had kept the old 16×16 bitmaps beside the glyphs #319 introduced. The entries are named after the view they open — *Default View*, *Table View*, *Map View* — and the four chart entries dropped *Create*.
- **Editing a calculated class break, colour or label (#634)** copies the values into the desktop's `ViewData` on first edit and re-themes the layer onto that copy, instead of refusing with *"Cannot change derived data"*. **#734** lets a classification be copied from one map view and applied in another.
- Row-height dragging distributes the cell *pitch* rather than one element extent, so a bordered symbol column no longer drifts out of sync (#1208); the EventLog filter panel is no longer squeezed and its `category` checkbox no longer clipped (#1213); the legend-width menu items say what they do and name the keys that actually work (#1217).
- **The main window's placement is recorded while it is up**, not only in the destructor, so a session that ends in a crash, Task Manager or an RDP logoff no longer restores a placement from days ago — and the start-menu shortcut's *Run: maximized* field no longer overrides the restored geometry.

## Export

- **Missing VAT when exporting to dbf or gpkg (#973)** — four separate defects, each of which on its own ends in a shapefile without its `.dbf` or a GeoPackage layer without attribute columns. Among them: the native shapefile branch compared against the wrong driver name and had been **dead since 2023**, and the resulting storage failure was reported to the user as *"Export ready"*.
- **A unit carrying an attribute of another domain exports as a table again (#1145).** The domain of a unit is the unit; a foreign attribute is now skipped instead of pushing the whole export down the database branch, which wrote a second layer with the wrong geometry.
- The export writes one line to the event log naming the item, the file and the driver, and the GUI test script gained an `ExportPrimaryData` verb.

## Configuration language and diagnostics

- **The engine no longer reports case mix-ups about its own names (#1161).** The regression battery emitted **2747** case warnings over 233 configurations with not one clean — about eleven per configuration fired before the modeller had done anything wrong, and ten of those eleven were the engine disagreeing with itself, in `res/RewriteExpr.lsp`, `res/prelude.dms` and three C++ literals. `unit`, `value`, `item`, `param`, `attr` and `nrofrows` are now canonically lower case, chosen by census over the 1146 `.dms` files of the regression suite; since the registry folds case these are display-only changes. `true`/`false` were deliberately left alone. Battery: 2747 → 40 lines.
- **`union_data` warns when its arguments do not match the parts of its result domain (#990)** — but only where the result domain's own `union_unit`/`combine` rule states the intended decomposition, read at meta-info without touching any data. Everything else stays silent.
- **The legacy brace range spelling is caught on either bound (#1165).** Only the lower bound was peeked at, so `[xy(0; 300000), {625000, 280000})` passed the deprecation warning without a word.
- **`table_spec` and stable `sort_index` helpers (#421)**; editor navigation into a template instantiation (#471); a worker thread's internal error now names the item it was working on (#1202); a configuration can decide whether its hidden items are shown, and `ShowHiddenItems` can no longer keep a configuration from opening (#694).

## Robustness

- **A GUI closed with work abandoned no longer ends in `uncaught DmsException: Cannot find Domain unit .` (#1225).** A data item re-resolved its domain unit by name whenever the weak member read as expired — which it also does *after* the unit has been destroyed. A dying data item must not bring units into existence.
- **The export dialog no longer hangs on a cold item (#1226).** `GetName()` returns an RAII holder of the token registry's shared lock; bound to a local, it stayed held over a walk that ends in an expression parse, which takes the registry exclusively — the main thread waited for a lock it held itself, at ~0% CPU, with the window reported as not responding.
- **That self-deadlock is now reported rather than parked on (#1227)**, naming the string being registered and what holds the registry. The same shape had cost three earlier debugging sessions.
- **The final allocator summary is reported at terminate, not from a static destructor.** Reporting it allocates, and allocating can post to the main-thread operation queue — which, by the time a static destructor of the same library runs, has itself been destroyed, so the post freed an already-freed buffer. The resulting heap corruption surfaced much later as an abort during process exit, with a stack naming an unrelated allocation. It cost a run in roughly ten on a loaded machine, and only on a loaded machine; on Linux it aborted the GUI, on Windows the heap may absorb it silently. The summary now comes from an explicit terminate step while the process is still whole.
- **#1191** — what aborts the process is named, and the cancellation paths no longer abort it. **#1206** — GDAL cleanup ordering at process shutdown.

## Packaging and build

- **A fourth flavour, `.g` (GLOBIO)**, shipped for the first time with this release. It is built from the same solution and the same sources as `.m`, with a committed `GeoDmsGlobio` switch that links it against the older GDAL/GEOS stack and a pinned toolset, and gives it its own output tree, vcpkg manifest and platform string. Install it only where that older stack is required; `.m` remains the ordinary Windows build.
- **The VS Code extension ships with the setup**, and the Notepad++/VS Code language definitions are generated from the operator registry: 23 names in the operator lists no longer resolved to anything registered (`point(`, `overlay_polygon(`, `EXEC(`, the `select_*` family, …) and were removed.
- **Multi-version Python bindings (#1105).**
- **Every text blob in the repository is now stored with LF**, with a `.gitattributes` policy and a one-time repair script for older clones. Source only; nothing about the shipped binaries changes.
