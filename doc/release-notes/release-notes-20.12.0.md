**Pre-release.** GeoDms 20.12.0 rolls up everything since **20.8.0** and supersedes the 20.9–20.11 pre-releases. Headlines: **user-defined functions** in the DMS language, **free-store drainage** (memory pressure now returns freed memory to the OS), a **CRS/metric decoupling** of the unit system, and a batch of new and repaired operators including **`voronoi`**.

| Flavour | Asset |
|---|---|
| `.m` — Windows, MSBuild | `GeoDms20.12.0.m-Setup-x64.exe` |
| `.c` — Windows, CMake | `GeoDms20.12.0.c-Setup-x64.exe` |
| `.l` — Linux, Ubuntu 24.04 | `GeoDms20.12.0.l-linux-x64.deb`, `.tar.gz` (+ `.sha256`, `.sha256.p7s`) |

## Language: user-defined functions

The DMS language now has typed, user-defined functions — from plain helpers to higher-order composition:

- `function` definitions with typed signatures, applied inline or via `instantiate`/`apply`; anonymous function literals as arguments.
- Generic type variables with constraints, type aliases and refinements, type-dependent overloading (variants), partial application, `map(F, src)` over containers — including `map(F(k, _), src)`.
- Container/record types as parameters: structured `unit<...> { ... }` parameters with typed members, checked at definition time.
- Definition-time type checking of function bodies, so mistakes surface when the function is *defined*, not when a run finally reaches it. `GeoDmsRun @checkfunctions` audits every definition in a config; `@dumpconfig` round-trips the parsed config to DMS text.
- The old `RewriteExpr.lsp` rule base is largely retired in favour of a typed prelude written in the language itself, auto-imported as the outermost namespace.

Design and status: [typed-hof-language-design](https://github.com/ObjectVision/GeoDMS/blob/lookahead-scheduling/doc/development/typed-hof-language-design.md), [type-declaration-forms](https://github.com/ObjectVision/GeoDMS/blob/lookahead-scheduling/doc/development/type-declaration-forms.md), [operator-signature-interface](https://github.com/ObjectVision/GeoDMS/blob/lookahead-scheduling/doc/development/operator-signature-interface.md), [typed-hof-remaining-work](https://github.com/ObjectVision/GeoDMS/blob/lookahead-scheduling/doc/development/typed-hof-remaining-work.md). A ~260-config regression battery ships with the installers under `examples/testcases`.

## Memory: free-store drainage (on by default)

Freed object stores used to stay on the allocator's free stacks for the rest of the run — measured at 112 GB of retained-but-dead pool on a large allocation workload. Under memory pressure they are now decommitted back to the OS: array-wide, budgeted, largest classes first, with a keep-hot reuse band so steady-state performance does not pay for it. On the RSopen benchmark this cut peak commit charge from 193.5 GB to ~180 GB, leaving the peak bounded by genuinely live data.

Control: `MemoryDrainage` setting (default **on**), `/SF` / `/CF` switches; the trigger is `MemoryFlushThreshold` (default 80% of allowed RAM — note `MemoryMaxRAM_GB` defaults to 64, raise it on bigger machines). Full measurement trail: [schedule-with-lookahead](https://github.com/ObjectVision/GeoDMS/blob/lookahead-scheduling/doc/development/schedule-with-lookahead.md) §8.1.

## Scheduling: resource-aware admission (available, off by default)

The engine can now estimate each operation's memory impact, keep a ledger, and defer memory-adding work when a budget would be exceeded — with drainage coupled in, so a task waiting for memory actively frees it. Measured honestly: on the workloads tried it does **not** lower live-bound peaks (§8.1.29–8.1.32 document why), so it stays **off** by default. `ResourceAwareScheduling` = 1 (`/Sq`) logs what it *would* do; 2 (`/SQ`) enforces. The estimation work also produced always-on allocation accounting and per-operator working-memory estimates (GEOS buffers, connectivity, matrices).

## Units: CRS decoupled from the metric

A projection/CRS is now a first-class value on a unit, decoupled from the linear metric — stages 0–7 of [crs-metric-decoupling](https://github.com/ObjectVision/GeoDMS/blob/lookahead-scheduling/doc/development/crs-metric-decoupling.md): `CrsUnit` operator, derivation rules, unification with a projection fallback, background-layer registry keyed by CRS, and retirement of the old 0xFF-packed side table. Fixes the class of "area(geom, m2) rejects a CRS-tagged metric" problems at the root.

## Operators: new and repaired

- **`voronoi(points, extentUnit)`** — new: the Thiessen cell polygon per point, clipped to the extent unit's range. [wiki/voronoi](https://github.com/ObjectVision/GeoDMS/wiki/voronoi)
- **`triangualize(points)`** — was registered but unimplemented (silently empty); now returns the Delaunay edge network as a `unit<uint32>` with `F1`/`F2`. [wiki/triangualize](https://github.com/ObjectVision/GeoDMS/wiki/triangualize), #1172
- **`bp_buffer_multi_point`**, **`cgal_buffer_linestring`**, **`cgal_buffer_point`** — all three silently returned empty results (winding mismatch, wrong algorithm, mis-registration); fixed. #1172
- **`greedy_alloc` / `needy_alloc`** — greedy aborted after solving; both are now real allocation regimes with aggregate (`_np`) variants. #1171
- **`frequency_table_with_null`** — nulls now counted in the total and preserved across tiles; plus new `as_unique_list_with_null`. #1170
- **`perimeter`** — accumulated into uninitialised memory. #1169
- **`min_ifdefined` / `min_alldefined`** (and `max` variants) — new aggregations. #597
- **`discrete_alloc`** and the Dijkstra/connect families got name-directed, typed result members.

## Fixes

- Linux: every startup segfaulted before `main` (static-init order); two GCC-only compile errors. Both from 20.11.0, fixed in 20.11.1.
- GUI: `+`/`-` and mouse-wheel zoom in the map view (#1129), TableView follows TreeView text colors (#1159), storage of `.mmd`-backed units in the detail page (#1143), crashes on template-internal generic items, a glyph-cache corruption from a dangling font-name pointer.
- Language/engine: value types print lowercase (#1161), nested function bodies reach enclosing parameters (#1166), generic domain-point constraints (#1163), `boost::format` fully replaced by `std::format` (#1164), unknown string escape codes now warn (#292), rate-limited `EmptyWorkingSet` storm on large-RAM machines.

## Build

One version source of truth (`GeoDmsVersion.cmd`) for C++, batch and CMake; ISO 8601 buildstamps. One vcpkg world for MSBuild, CMake *and* IDE builds (shared binary cache, downloads root and pinned tools), with pre-build drift tripwires so an ABI change announces its rebuild cost up front instead of an hour in. `prelude.dms` is packaged in all three setups (the root cause of the 20.9.0.m unit-test failures).
