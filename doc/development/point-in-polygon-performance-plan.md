# Performance plan: the point-in-polygon operators and the other users of the spatial index

*Status (2026-10-17): plan, no code changed yet; item 1 is #1319, item 2 is #1320, item 5 (the text of a
table, outside the spatial index) is #1321. Written after #1292
(the loose quadtree, 7c4000cdc, 20.25.0) closed. The measurements below were taken on OVSRV10 with a kept copy of the Release build of
c56fe51a2 (`C:\LocalData\GeoDMS_engine\20.25.0_c56fe51a2`), on an almost idle machine (2 to 7 % CPU
load), from `scratch\perf_idx\probe_idx.dms`.*

## What was measured

Item seconds from the log of one `GeoDmsRun` per item (the time of the operator and of the inputs it
computes), two runs where two are given:

| probe | seconds |
|---|---|
| 1 M unit-square polygons that tile and their bounding boxes, no index (`warm`) | 0.20 |
| the same and the spatial index over them with one query (`pip1`) | 0.23 |
| `point_in_polygon`, 10 M points in a pseudo-random order over those 1 M squares | 0.85 to 0.86 |
| the same with the points in the order of the squares (10 per square) | 0.45 |
| `point_in_all_polygons`, the 10 M points over the 1 M squares (10 M matches, one per point) | 78.0 to 78.6 |
| `point_in_polygon`, 10 M points over 400 squares of 50 x 50, 5 vertices each | 0.27 |
| the same 400 squares with 10 001 vertices on their boundary | 10.0 to 10.5 |

What this says:

- Building the index is cheap: about 0.03 s for 1 M polygons, against 0.20 s for computing them.
- The order of the points matters about twice for `point_in_polygon` on 10 M points.
- `point_in_all_polygons` takes 90 times as long as `point_in_polygon` for the same matches.
- A polygon with many vertices costs in proportion to its vertices: 36 times for 10 001 against 5.

## Ranked opportunities

### 1. `point_in_all_polygons` takes a lock for every match (#1319)

Cause, read from the code: `PointInAllPolygonsOperator::Calculate` (`geo/dll/src/OperPolygon.cpp:2197`)
runs a `parallel_for` over the points of a tile and appends each match to the tile's result vector
under `resLocalAdditionSection` (`:2209`), one `leveled_critical_section` lock per match. With 10 M
matches on 32 threads that is 7.8 µs per match. `StoreRes` then sorts each tile's pairs (`:2237`), so the
order in which they were appended does not matter for the result.

Change: let the loop over a tile's points work in blocks, each block appending to a vector of its own,
and concatenate the blocks in their order into the tile's vector, without a lock; the sort in
`StoreRes` stays, so the result is the same. Keep the per-tile lock that creates the tile's vector
(`:2186`), which is taken once per pair of tiles.

Before changing it, confirm the split with a stack sample of the probe (cdb attach, two samples):
the lock, not the vector growth or `StoreRes`, should dominate.

Expected: 78 s to about 1 to 2 s on the probe. Users: Hestia (`TVW.dms`: points in the heat-network
polygons, `WarmtenetTempl.dms`: vertices in the heat-source polygons), the operator tests in `tst`.
Effort: small. Risk: low; same result.

The same per-match lock is in the overlay operators (`geo/dll/src/BoostPolygon.cpp:737`, `:788`,
`:826`, `:868`, `:903`), where it is taken once per pair that intersects. There the intersection of two
polygons probably costs far more than the lock, so measure first (an overlay of 1 M squares with the
same squares moved by half a square: four pairs per square) and change it only when the lock shows.

### 2. A polygon with many vertices: `IsInside` reads every vertex (#1320)

Cause: `IsInside` (`rtc/dll/src/geom/IsInside.h`) counts the edges that cross the horizontal line
through the point by walking all k vertices of the polygon. `point_in_polygon`,
`point_in_ranked_polygon` and `point_in_all_polygons` call it for every polygon whose bounding box
holds the point, so a point costs about k per candidate polygon.

This is the common case in the models: many points against a few detailed boundaries. In the `tst`
projects: municipalities in RSopen (`WritePrivData.dms:44`), NUTS3, NUTS and countries in
NetworkModel_EU (`RegionalUnits.dms:42`, `Geometries.dms:55`), African countries in an `IsLand` test
(`geodms_africa_cusa2/Templates.dms:845`), the woonplaats of every address in the BAG snapshot
(`afleidingen.dms:213`), flood areas, study areas. A point outside every polygon but inside a bounding
box pays the full count too.

Change: for each polygon of more than a threshold number of vertices (to be measured, of the order of
64), an edge index built once with the spatial index: the y-range of the polygon cut into S slabs of
equal height (S about k / 8, at most a few thousand), and per slab the list of edges whose y-range
meets it, in compressed form (an offset array and one array of edge numbers). A query looks up the
slab of the point and applies the crossing test of `IsInside` to the edges of that slab only.

Why the result is exactly the same: an edge changes the count only when one of its ends lies strictly
above the point's row and the other does not, `(0 < this.Row()) != (0 < prev.Row())` in `IsInside`,
which needs the point's row to lie in the edge's y-range. A slab lists every edge whose closed y-range
meets it, so every edge that can count is tested, with the same arithmetic in the same order.
Points to keep exact: the closing edge from the last vertex to the first; the connecting edges of a
polygon with islands or holes, which the sequence runs through twice; and an edge with a null
vertex, which has no y-range: put such edges in a list tested for every point.

Where: in the resource that `CreatePolyHandle` makes beside the spatial index (`OperPolygon.cpp:1653`,
`:1950`, `:2135`), built for all polygons above the threshold in a `parallel_for`, so a query only
reads it. Memory: one entry per edge per slab it meets. A polygon with many long edges, which would
meet many slabs, gets fewer slabs, so that the entries stay below a small multiple of k.

Expected: the 10 001-vertex probe from 10 s to about the 0.3 s of the 5-vertex one; on real
boundaries a factor of the order of k / 10 for the points that test such a polygon. Effort: medium.
Risk: an inexactness at a slab boundary, a vertex on the point's row or a null vertex would change a
result, so the tests below compare with the walk over all vertices.

Tests: a battery case that runs `point_in_polygon` and `point_in_all_polygons` over polygons with many
vertices against the same polygons without the index (below the threshold, or a brute-force count in
the configuration): points on vertices, on horizontal and vertical edges, on slab boundaries, in holes
and islands, in `dpoint` and `ipoint`. The existing `oper_point_in_polygon_tiling.dms` and
`oper_point_in_polygon_ties.dms` must stay green.

Wiki: the performance sections of `Point_in_polygon`, `Point_in_ranked_polygon` and
`Point_in_all_polygons` (O(n log p + n k) today).

### 3. The order of the points

`point_in_polygon` on 10 M points took 0.45 s in the order of the squares and 0.85 s in a
pseudo-random order: the index nodes and polygons that a query reads are in the cache when the previous
point read them. Sorting each point tile by a Morton key of its points, querying in that order and
writing each result at the point's own position would give random input the time of sorted input,
less the cost of the sort.

Measure on a real model first: point sets read from a file are often in an order that is already
spatial (the BAG numbers addresses by municipality), and then the sort only costs. Effort: small to
medium. Priority: after 1 and 2.

### 4. `box_connectivity`: two serial passes

`BoxConnectivityOperator::Calculate` (`geo/dll/src/BoostPolygon.cpp:2733` and `:2754`) queries every
box twice, once to count the pairs and once to write them, on one thread. One pass in parallel blocks,
each block keeping its pairs (already sorted per F1), concatenated in block order, would halve the work
and spread it. Since #1292, 2 million squares take about 2.5 s, so the gain is about 2 s at that size;
it was dropped from #1292 for that reason. Do it only when a model has tens of millions of boxes.

### 5. Text of a table: `+` chains and `asList` copy and run on one thread (#1321)

Outside the spatial index, but found while looking at why OVSRV10 was a quarter busy during an
NM_PBL output stage (2026-10-17, run `y2023r11m_fiets`): per departure moment about 8 s at 10 to 15
cores and then about 7 s at about 2, ending when the main thread writes a CSV of 20 to 29 MB with 32
columns. Not confirmed with a stack sample yet; the CSV output stage of `y2023r11a_auto` (2026-10-18,
about 00:00 to 10:30) is the next chance.

The pattern is the TableComposer of the wiki, which NM_PBL follows
(`cfg/main/NetworkSetup/PublicTransport/PerDepartureMoment_T.dms:529-531`): a row per element from
`OrgName + ';' + DestName + ';' + string(col) + ...`, generated with `asList(ExportColumns/name, ' +
'';'' + ')`, then `asList(BodyLines, '\n')` and the `str` storage. What that costs, read from the code:

- `+` on strings is `StrConcatOperator` (`clc/dll/src/OperAttrBin.cpp:28`), a binary operator, and
  `add(a, b, c, ...)` is folded by the prelude into nested binary `add`s (`res/prelude.dms:208`). So a
  row of n pieces makes n - 1 string columns, each holding the row text so far: a row of 32 columns
  and 31 separators is copied about 30 times, of the order of 0.7 GB of copying and allocation for a
  22 MB file, and every `string(col)` is one more column.
- Each of them works per tile, so a domain of one tile runs on one thread.
- `asList` writes all elements into one growing `OutStreamBuff` (`unary_ser_aslist`,
  `clc/dll/include/AggrUniStructString.h`), tile after tile, on one thread.

The pattern also loses rows. A probe on c56fe51a2 (`scratch\perf_idx\probe_null_rows.dms`): `string()`
of a null number is a null string, so `'a;' + string(v) + ';z'` is null for that element, and `asList`
leaves null elements out. Of the values 1.5, null and 3.25 the text is `a;1.5;z|a;3.25;z`: the row with
the null is not in the file, and nothing says so. The prelude's `concat` (`res/prelude.dms:167`,
`MakeDefined(head, '') + concat(rest)`, documented on the wiki page Variadic-parameters) gives
`a;1.5;z|a;;z|a;3.25;z`: an empty field, the row kept.

The syntax to use exists, then: `concat(arg1, arg2, ...)`, with the separators as literal arguments
(`concat(OrgName, ';', DestName, ';', ...)`, generated as
`='concat(OrgName, '';'', DestName, '';'', ' + asList(ExportColumns/name, ', '';'', ') + ')'`). What it
lacks is speed: the prelude folds it into a `MakeDefined` column per piece and a binary `+` per step,
with the same copies as a `+` chain. Change, in three parts:

1. `concat` as an operator: any number of arguments, with a void domain (literals and parameters, such
   as the separators, repeated for every element) or one shared domain; a null piece is an empty string,
   as in the prelude function, so its results stay the same. Arguments of any scalar value type that
   `string` converts are formatted as `string(x)` formats them, so a row needs no `string()` per column:
   `concat(OrgName, ';', DestName, ';', Traveltime, ';', Price, ...)`. A tile is made per block of
   elements: each block formats its elements into a buffer of its own, in parallel, and the tile's
   sequence array is allocated once at the total size and filled with one copy per block. No column per
   piece and no copy of a partial row.
2. A chain of string `+` whose intermediate results are not items of their own is evaluated by the same
   kernel with the rule of `+` (a null piece gives a null result), so a model that already writes rows
   with `+` gets the speed unchanged, its results included, rows that a null drops included.
   `string(x)` stays its own column there.
3. `asList` in two passes: the length of each tile's (or block's) part with its separators, one
   allocation of the total, and the parts filled in parallel at their offsets; the result is the same
   string.

The wiki's TableComposer example then uses `concat`, and the pages Concatenation, AsList and
TableComposer say that a null field drops a row from a `+` row and not from a `concat` row. Changing
the prelude function into an operator changes the key of every `concat` expression; check before part 1
what depends on those keys (the comment above the prelude folds says their keys were kept equal to those
of the rewrite rules they replaced).

Expected: the 7 s tail per departure moment of the NM_PBL CSV output to well under a second (an
estimate), and the same gain for every CSV written this way. NM_PBL itself has written Parquet through
`gdalwrite.vect` for its 2024 and 2025 runs since 2026-10-16 (its commit 429186b) and keeps CSV for 2023,
validation runs and older data, so its Parquet run `y2024o_lopen` (after 2026-10-18) gives a
before/after of the tail without this change.

Tests: a battery case that compares the `concat` operator with the prelude fold it replaces (kept in the
case as a function of its own) and with `MakeDefined(string(x), '')` per piece, element for element,
over a tiled and an untiled domain, with nulls, empty strings, every scalar value type and void arguments
in front, between and behind; the `+` chain against its result before the change, null rows included;
`asList` against its result before the change. Probe: 200 000 rows of 32 columns through `+` + `asList` +
`str`, before and after, and the same rows through `concat`.

A fourth step, not planned: a storage that writes the columns of a unit as CSV in parallel blocks
straight to the file, with no string of the whole text at all. Only if the three parts above leave the
write itself as the bottleneck.

## Not worth doing now

- Building the index in parallel or in bulk: 0.03 s per million polygons.
- A different memory layout of the index (leaves contiguous per node instead of a linked list): since
  #1292 a query tests tens of leaves, not thousands; revisit only if a profile shows the walk over the
  nodes dominating.
- The distance bounds of the loose boxes in `connect` and `connect_info`: measured level in #1292.

## Where a gain will show

The full.py round has no experiment that these operators dominate (the OVSRV05 reading of 20.25.0), so
the round will not show them. Measure 1 and 2 on the probe above, and 2 also on the real items:
RSopen `Gem_rel`, NetworkModel_EU `NUTS3_rel` and `IsEU`, the Africa `IsLand`, BAG `woonplaatsnaam`,
with `run_exp.py` (`.claude\skills\geodms-perf`) and interleaved against the kept copy above.

## Order of work

1. Item 1: a stack sample to confirm the lock, then the change, the probe and the battery. One commit,
   one issue.
2. The overlay lock: measure; change only if it shows.
3. Item 2: the edge index with its battery case, the probe, and the real items above; the wiki pages.
   One issue, probably two commits (the index, then its use by the three operators).
4. Items 3 and 4 only on evidence from a model.
5. Item 5 independently of the others: first the stack samples of an NM_PBL CSV output stage, then
   `concat` with its battery case, the rewrite of `+` chains, and the two-pass `asList`; the wiki pages
   Concatenation, AsList, String-functions and TableComposer.

Items 1 and 2 have their issues, #1319 and #1320, with the probes of this page; items 3 and 4 get one
when a model gives the evidence.
