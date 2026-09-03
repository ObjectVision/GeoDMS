**Pre-release.** GeoDms 20.19.1 covers everything since **20.16.0** — 20.17.0 was published, 20.18.0 and 20.19.0 followed, and this release adds a teardown fix that made a process abort at exit and an allocation fix that stopped `discrete_alloc` from failing on a land use type that is no option. The `.g` (GLOBIO) flavour ships alongside `.m`, `.c` and `.l`.

| Flavour | Asset |
|---|---|
| `.m` — Windows, MSBuild | `GeoDms20.19.1.m-Setup-x64.exe` |
| `.c` — Windows, CMake | `GeoDms20.19.1.c-Setup-x64.exe` |
| `.g` — Windows, GLOBIO compatibility | `GeoDms20.19.1.g-Setup-x64.exe` |
| `.l` — Linux, Ubuntu 24.04 | `GeoDms20.19.1.l-linux-x64.deb`, `.tar.gz` (+ `.sha256`, `.sha256.p7s`) |

## Allocation

- **A land use type that is no option no longer fails the allocation.** Measuring the distance from optimum — which only builds the `DiscrAlloc completed …` status line — added the shadow price to an *undefined* suitability, where the bidding loop itself skips such a type before doing any arithmetic. On a model with null suitabilities that ended the whole run with `numeric overflow in the shadow price arithmetic`. The same addition used to wrap silently, so a type that was never an option compared as a **better bid**: the *"at most N from optimum due to M better options"* figures have been wrong for such models for as long as they have existed, and are now right.
- **The Simulation-of-Simplicity perturbation range is checked once per run** instead of at every use, and reported before the solve starts rather than partway through an allocation.
- **`discrete_alloc` arithmetic audited (#1196).** The claim aggregates were summed in `UInt32`: two land use types at the "no limit" maximum claim sum to exactly 2^32, read back as **zero**, and the run was rejected as infeasible — four cells are enough, no grid needed. Shadow prices now go through checked arithmetic naming the component that overflowed, and six `discrete_alloc_*_pi64` names offer a 64-bit perturbation.

## Robustness

- **A process no longer aborts at exit with a corrupted heap.** Reporting the final allocator summary allocates, and allocating can post to the main-thread operation queue — which, by the time a static destructor of the same library runs, has itself been destroyed, so the post freed an already-freed buffer. The damage surfaced much later as an abort during process exit, with a stack naming an unrelated allocation. It cost roughly one run in ten on a loaded machine and nothing at all on an idle one; on Linux it aborted the GUI, on Windows the heap may absorb it silently. The summary now comes from an explicit terminate step while the process is still whole.
- **#1191** — what aborts the process is named, and the cancellation paths no longer abort it. **#1206** — GDAL cleanup ordering at process shutdown. **#1225** — a GUI closed with work abandoned no longer ends in `uncaught DmsException: Cannot find Domain unit .`.
- **#1226 / #1227** — the export dialog no longer hangs on a cold item: `GetName()` hands out a lock on the token registry, and holding it across a walk that parses an expression made the main thread wait for itself. That self-deadlock is now reported rather than parked on.

## Polygons

- **A ring's role is read relative to the first ring, not to the compass (#1212).** The GEOS and CGAL readers judged each ring absolutely — shell iff clockwise — so a feature whose rings are *uniformly* counter-clockwise, which is what a source listing coordinates in latitude/longitude order gives you, lost its shell and promoted the lake behind it. Nothing was rejected and nothing was empty: ordinary-looking geometry with the wrong area. **A configuration's areas may change on upgrade; the older numbers were the wrong ones.** What that means in practice is measured on a real model in **#1230**: in the RSopen regression, 105,051 BAG building footprints stored counter-clockwise — none of which has a hole, so nothing was left to promote — were read as *empty* and are now read correctly, while 232 CBS land-use polygons whose rings are only *partly* flipped were read 2,298 ha *too large* and now match their stored area. Both directions occur, so an area can grow or shrink; a feature that is only partly flipped still needs `fix_winding_order`.
- **Nesting-based winding-order operators (#302)** on GEOS: `fix_winding_order`, `fix_polygon`, `has_correct_winding`, `geos_polygons_by_nesting`. **#1219** — `points2polygon` no longer requires a closed ring the readers close themselves. **#917** — Minkowski sums take the kernel as an argument, deprecating 48 `bp_*` names.

## Storages and export

- **A layer's companion columns come into play at write time (#1229).**
- **Writing vector layers (#711).** `gdalwrite.vect` created a field for every storable attribute and left the rest `<null>` when only one column was of interest. A column of interest now pulls in the other columns of its layer for exactly as long as the outside interest lasts. A unit that both declares a `SpatialReference` and has a calculation rule no longer loses that CRS, so a shapefile gets its `.prj` again.
- **Missing VAT when exporting to dbf or gpkg (#973)** — four separate defects, among them a native shapefile branch that compared against the wrong driver name and had been **dead since 2023**, with the resulting storage failure reported to the user as *"Export ready"*.
- **A unit carrying an attribute of another domain exports as a table again (#1145).**

## `.mmd` dictionaries

- **Unit paths are absolute, in declarations and checks alike (#1195).** A domain configured as `../RegioUnit` went into the dictionary verbatim, so a reader merging that store elsewhere resolved the dots against *its* structure: `Unknown identifier '../RegioUnit'`. The store could then not be read at all.
- **A differing dictionary root name is provenance, not a warning (#1194)**, and **a check that cannot be built now names itself (#1197)**.

## IntegrityChecks and fences

- **A check now guards an item read from a storage (#1209)** — such an item has no calculation rule, so consumers were handed an unguarded source reference and the check ran only after data preparation.
- **A check on an `ExplicitSuppliers` item guards the item that declares it (#1218).** Selecting either item in the TreeView previously ran no check at all.
- **A phase runs when a member is reached (#1167)**, collecting a phase member is re-entrant (#1201), indirect expressions behind a fence are rejected (#1199), and the phase scan honours the static argument policy (#1224).

## Charts and views

- **A new chart no longer waits for its own classification (#1221)**, position axes fit their data instead of forcing zero into view (#1222), row-number axes are labelled from the domain's `Label` attribute (#1207) and tick labels take the thousand separator (#1223).
- **Each chart kind has its own icon (#1211)**, the View and Window menus and window titles are drawn from the TreeView's icon set (#1220), and **tree icons say what an item is (#319)** rather than which views can be opened on it.
- Editing a calculated class break, colour or label copies the values first (#634); a classification can be copied from one map view and applied in another (#734); the EventLog filter panel is no longer squeezed (#1213); the legend-width menu items name the keys that actually work (#1217). The main window's placement is recorded while it is up, so a session ending in a crash no longer restores a placement from days ago.

## Networks

- **Bi-criteria (pareto) `impedance_matrix` (#856)** — a second Dijkstra engine keeping the pareto-optimal set over two impedances instead of collapsing them into one weighted cost.
- **`od:StartPoint_rel` is written (#1210)** — the member was created, allocated and committed but never filled, so requesting it yielded uninitialised memory. NYI since 2022.

## Configuration language and diagnostics

- **The engine no longer reports case mix-ups about its own names (#1161).** The regression battery emitted **2747** case warnings over 233 configurations with not one clean; ten of every eleven were the engine disagreeing with itself. `unit`, `value`, `item`, `param`, `attr` and `nrofrows` are now canonically lower case, chosen by census over 1146 `.dms` files. Battery: 2747 → 40 lines.
- **`union_data` warns when its arguments do not match the parts of its result domain (#990)**, the legacy brace range spelling is caught on either bound (#1165), and **`select_spec` / `collect_spec` (#337)** take their four choices as a `;`-separated word list.
- **`table_spec` and stable `sort_index` helpers (#421)**; editor navigation into a template instantiation (#471); a worker thread's internal error names the item it was working on (#1202); a configuration can decide whether its hidden items are shown (#694).
- **Source Description covers what a storage feeds (#975)**, and **the CalcCache machinery is retired (#1189)** — a configuration still spelling `%calcCacheDir%` now gets an unknown-placeholder error.

## Packaging and build

- **The `.g` (GLOBIO) flavour**, built from the same solution with a committed `GeoDmsGlobio` switch against the older GDAL/GEOS stack, with its own output tree and vcpkg manifest. Install it only where that stack is required; `.m` remains the ordinary Windows build.
- **The VS Code extension ships with the setup**, and the editor language definitions are generated from the operator registry: 23 names that no longer resolved to anything registered were removed.
- **Multi-version Python bindings (#1105)**, and **#1186** — the `.c` setup now ships the MSVC runtime instead of borrowing whatever redistributable happened to be in `System32`.
- **Every text blob in the repository is stored with LF.** Source only; nothing about the shipped binaries changes.
