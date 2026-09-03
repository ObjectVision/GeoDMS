**Pre-release.** GeoDms 20.13.0 follows the **20.12.0** pre-release with a focused round of fixes: the GUI no longer ghosts during long computations (#1156), grid `mapping` / `mapping_count` got a separable fast path (#298), and a sweep over the registered operator list turned up several names that silently produced wrong or empty results.

| Flavour | Asset |
|---|---|
| `.m` — Windows, MSBuild | `GeoDms20.13.0.m-Setup-x64.exe` |
| `.c` — Windows, CMake | `GeoDms20.13.0.c-Setup-x64.exe` |
| `.l` — Linux, Ubuntu 24.04 | `GeoDms20.13.0.l-linux-x64.deb`, `.tar.gz` (+ `.sha256`, `.sha256.p7s`) |

## GUI: no more "(Not Responding)" during long computations

The still-open part of #1156. A multi-minute computation on the Qt main thread stopped retrieving messages, so Windows ghosted the window — and once the DWM ghost window exists, all further input is rerouted to it, starving the very check that would make the computation yield. Clicking the window to interrupt therefore did nothing.

Three measures, none of which dispatch a message mid-computation (a `PeekMessage`-based approach was abandoned because any retrieval call delivers pending cross-thread sent messages, causing reentrancy deadlocks):

- `HasWaitingMessages()` probes `QS_ALLINPUT` instead of `QS_ALLEVENTS`, so a pending incoming cross-thread `SendMessage` — shell icon probes, hang probes, blocking senders — also makes the engine yield. `GetQueueStatus` remains a pure status probe.
- `DisableProcessWindowsGhosting()` at start of `main()`: the frosted overlay, the caption and the input rerouting all belong to the ghost window. Without it, input keeps landing in the real queue, so the suspend trigger answers the first click within its 1 s tick.
- Where the main thread parks while joining work, it now blocks in `MsgWaitForMultipleObjectsEx(..., QS_ALLINPUT)` instead of a plain condition-variable wait. Windows then counts the thread as waiting for input — exempt from the not-responding verdict — and a newly arriving message ends the wait *without* being delivered.

## Grids: separable `mapping` / `mapping_count` (#298)

A `W × H` tile mapped between two grids contains only `W` distinct x's and `H` distinct y's, so a coordinate-separable transformation needs `W + H` transformations rather than `W × H`. Separability is decided by a structural CRS gate and then **verified numerically** on a probe lattice, compared exactly; anything unproven falls back to the generic loop.

The conversion state is now built once per invocation instead of once per tile — on a 12M-cell EPSG:4326 → EPSG:3857 mapping, ~220 ms of ~230 ms went into rebuilding the transformer per tile, under a global lock. It hands out one functor per *thread*, since `OGRCoordinateTransformation` is not thread-safe, which speeds up the non-separable path too.

The same-CRS case (grid to grid within one SpatialReference, by far the most common) is separable by construction and now takes the outer product as well:

| | before | after |
|---|---|---|
| `mapping_count`, 4M cells, EPSG:28992 10 m → 100 m | 23.4 ms | **0.2 ms** |
| `mapping`, 49M cells, EPSG:28992 10 m → 100 m | 23.0 ms | **11.0 ms** |

This also fixed a live correctness bug on the way: in cross-CRS `mapping(D,V)`, the source index restarted at 0 for every 1024-point block while the output iterator advanced correctly, so any tile larger than 1024 cells repeated the first 1024 cells' coordinates for the whole tile.

## Operators that did not do what their name promised

- **`modus_count_uint16`** was registered as `modus_count_uint17` (#1173). Operator lookup is by exact name, so the name every other part of the system spells did not resolve at all — and the table view generates precisely that name for domains whose cardinality fits `UInt16`, so the GUI's modus-count aggregation was broken for that whole cardinality band. Renamed without a transition alias: no configuration can have used the typo except by having been written against it.
- **`potentialPacked` / `potentialRawPacked`** convolved with a bit-punned kernel (#1174). The pre-computed kernel FFT read the `Float32` reversed-kernel buffer as an array of doubles, so each pair of adjacent taps decoded into one garbage double and the read ran exactly 2× past the allocation. Small grids underflowed to zero, larger ones picked up NaNs from out-of-bounds heap — and NaN is the `Float32` null, hence the all-null results reported.
- **`ordered_union_data`** now verifies its own promise (#1168). The operator is how a modeller *states* that the unioned values are non-decreasing; consumers read that flag and take a sorted-merge path instead of building a sort index. A false statement did not surface as an error downstream — it silently produced wrong results. The order is now checked as the values pass into the result, one comparison per element, carried across tile and argument boundaries; a descent fails the result naming both values involved.
- **`ipf_alloc` removed** (#1177). Its entire algorithm was commented out, so it built its result items and returned an all-zero landuse grid with an empty status and no warning. A silent wrong answer is worse than an error; the name is now unknown to the parser.
- **`bp_buffer_multi_polygon` unregistered** (#1177) — boost::polygon has no multi-polygon buffer here, so the group only ever produced "no implemented operator". Use `bg_buffer_multi_polygon` or `geos_buffer_multi_polygon`, which also accept float coordinates.
- **`UrlEncode`, `HtmlEncode`, `HtmlDecode` implemented** (#1177), alongside the existing `UrlDecode`. They were registered names with no operator behind them while the wiki documented them as working. `UrlDecode(UrlEncode(s)) == s` for every byte string including UTF-8; `HtmlEncode` escapes only the five predefined entities so UTF-8 stays UTF-8, and `HtmlDecode` is its inverse plus `&nbsp;` and numeric character references, lenient by design.
- The obsolete stubs that error by design (`claim_*`, `subset`, `dijkstra_s/_m/_m64`, `PartNr`) stay for v20, but their promised v21 removal was only a runtime throw inside a static initializer — at v21 that would escape through `DllMain` as `STATUS_DLL_INIT_FAILED` and every executable would fail to start with no message at all. Each site now also carries a `static_assert`, which fails the build at the exact line to delete.

## Geometry: two polygon-reader defects

Both turned out to sit in the code that *reads* a multi-polygon point sequence — one reader per geometry family — and not in the union algorithms.

- **`cgal_union_polygon` lost area** (#1178). The CGAL reader subtracted every inner ring from the whole accumulated polygon set instead of from the polygon that ring belongs to, so any polygon nested inside another polygon's hole — an annex on the courtyard of a merged building block — was erased. On 123,773 BAG building footprints in inner-city Amsterdam this cost 0.29% of the area when uniting partition unions; the idempotence invariant now holds to 2.9e-9. Elementary unions of single polygons were never affected.
- **`bg_union_polygon` could kill the process** (#1176). Geometry that GEOS `MakeValid` could not repair to Boost.Geometry's satisfaction was passed to `boost::geometry::union_` anyway — undefined behaviour, which ended the process with no error line and the log cut off mid-line. The repair result is now cleaned of degenerate rings (Boost reports a collapsed ring as "wrong orientation", which is what made this hard to read), the winding order Boost requires is restored, and a geometry that still cannot be repaired raises a regular error instead of continuing. Note that `bg_union_polygon` remains **not recommended**: on such data it now reports a clean failure rather than completing the union.

## Performance

**`join_equal_values`** sized its six counting arrays on the *range* of the join key's values unit rather than on the data (#1175). A key typed by its value type instead of by a domain unit — a plain `uint32` attribute — therefore reserved 2^32−2 slots per array: 60 GB and two minutes to join three rows against three rows. When the range is large the index is now sparse, holding only the distinct values that actually occur in the first argument.

## Potential: the FFTW3 port completed

`float32` has always been a valid element type for convolution data, and IPP had a genuine single-precision convolution to serve it. The FFTW3 migration only ported the double-precision API, leaving the single-precision backends running a double transform whose result was merely stored in `float32` buffers — half a port, and the half that produced #1174. They now use FFTW's single-precision API throughout, and the single-precision backends are available as **`potential32` / `potentialRaw32`**.

## Build and tooling

- Linux: a functional-style cast that only MSVC accepts (`unsigned(unsigned char(ch))` is not a simple-type-specifier) broke the GCC build; plus a round of GNU C++ warning mitigation.
- The VS Code language extension's `operators.csv` is back in sync with the registered operator groups: 31 names that were never listed (`CrsUnit`, `voronoi`, `geos_buffer`, the `greedy_alloc`/`needy_alloc` families, `points2arc_*`, `points2multi_point_*`, `max`/`min_{all,if}defined`, …) have been added. The grammar generator is reproducible again — its alternation ordering depended on Python's randomized string hashing, so every run reshuffled equal-length case variants and made regenerated output look dirty.
