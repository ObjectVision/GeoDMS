# Performance plan: the point-in-polygon operators and the other users of the spatial index

*Status (2026-10-17): plan, no code changed yet; item 1 is #1319, item 2 is #1320. Written after #1292
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

Items 1 and 2 have their issues, #1319 and #1320, with the probes of this page; items 3 and 4 get one
when a model gives the evidence.
