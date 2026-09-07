# dms_union_polygon in bp's shape: one lattice, one noding, one sweep

Status: implemented in the working tree on 2026-09-07 (uncommitted), for GeoDMS #1214 follow-up.
Five incremental builds. On the fifth, tier 1 is 16 of 16: the eight `oper_dms_overlay*.dms`
cases (the parity path, untouched), `oper_dms_family.dms`, the new `oper_dms_union_counts.dms`
and `oper_dms_union_steep.dms`, `oper_minkowski.dms`, the ring_encoding and t020 synthetic
probes, and the 124,662 Amsterdam buildings with a clean log and an area 3e-10 from geos. The
round after that, on the same binaries: the battery 320 of 320, the t010 Operator run 4,860 of
4,879 tests executed with none failing, and the buildings timing below. The t020 column of the
unit suite has not been rerun on this path.

What the timing says: the single noding gained about a tenth on the buildings, not the several
times that bp's shape promised. On the one metric both paths share, the update time of the
union's area item, five repeats each on an idle machine: the tower path took 25.4 to 25.9 s
(median 25.8) in the afternoon and the bag path 21.9 to 23.5 s (median 23.0) in the evening.
geos on the same metric took 22.1 to 22.4 s in the afternoon and 29.5 to 30.4 s in the evening,
its slow mode, so the two windows differ by more than the gain and only same-window ratios mean
anything: dms against geos went from 1.17x to 0.76x. cpu/wall was 1.9 in most repeats and 1.1 to
1.2 in the others, in both windows and for both families; the fold is still single-threaded.
Where the 23 s go has not been profiled on this path. The tower path's profile put 41% of the
run in the segment index of `CollectCrossings`, which one noding of the whole bag still runs
once over every segment, so step 4 below is the next measurement as well as the next change.

Step 4, the crossing sweep, followed the same evening: `CrossingSweep` (DMS_Traits.h, before
`Noder`) replaces the box quadtree in `CollectCrossings` for every dms_ operator; the hot-pixel
quadtree in `SnapSegments` stands. Two builds: the first made a crossing behind the sweep line a
check failure, which it is not (the crossing finder section below has it); the second passes
tier 1, 17 of 17 with the new `oper_dms_crossings.dms`, and the buildings clean at the same area.
The t020 comparison on that build is at the end of this document.

What the five builds taught, in order, each of which a synthetic case did not show:

1. A values unit that declares no range reports its whole domain as one, and for dpoint that
   extent overflows to infinity, on which the first frame derivation looped forever. A declared
   range is trusted only when finite, non-empty and of a plausible extent (`DeriveFrame`,
   `BoostPolygon.cpp`); everything else derives the frame from the data; `DmsFrameFor` is bounded.
2. `reserve(size + n)` on every append grows the bag to exactly that size, so each of 124,662
   appends reallocated and copied the whole bag: quadratic, six minutes of memcpy per dissolve.
   The bag grows geometrically now.
3. The real defect. The count sweep's weight was defined relative to the fragment's lexicographic
   orientation, but `SnapSegments` creates the pieces of a snapped chain in the parent's travel
   direction, and a steep edge whose chain takes a vertical step gets that step travelling
   downward; `MergeSegments` then turned it upward with the weight unchanged, and the piece
   claimed the wrong side of itself as covered. Parity never noticed, XOR having no direction.
   The weight is now relative to the stored direction (`Segment`), and `MergeSegments` negates it
   whenever it turns a fragment round. Found by a closure check at the end of `UnionBag` that
   names the first vertex where kept edges do not balance, with every fragment touching it; the
   check stays, since without it the polygonizer fails in ways that name nothing. The
   polygonizer's vertex lookup also fails loudly now instead of walking on from a wrong vertex in
   Release. `oper_dms_union_steep.dms` reproduces the case: it fails on build 4 and passes on 5.
4. `@statistics` reports a calculation error inline and still exits 0. Every harness here now
   reads the log for `[E]`.

## Why

The profile of the t020 "Buildings Amsterdam" dissolve (124,662 BAG buildings, about 1.4 M
vertices, clean Release build of 20.20.0, ETW sampled) puts 41 % of the run inside the sweep's
own spatial index and the rest spread thin. The structural reason is not the index but how often
it runs. `DMS_PolygonOperator` folds through `assoc_tower<DmsPolySet, union_dms_polygons>`
(`BoostPolygon.cpp:1649`), and every one of the 124,661 merges:

- quantizes both operands from float sequences (`DmsPolySet::m_Poly` is `dms_polygon_t<P>`, the
  world-coordinate container, `DMS_Traits.h:1381`),
- builds one segment vector out of both (`Compute`, `DMS_Traits.h:1011`) and nodes it against
  itself, so an accumulator that is already clean is re-noded against its own segments,
- dequantizes the result into a float sequence again.

bp does none of that. Its reducer `union_bp_polygonsets` is a lazy `polygon_set_data::insert`
(`BoostPolygon.cpp`, an O(1) append), and its `StoreImpl` calls `get_result()` once, which runs
`polygon_set_data::clean()` once: one intersection pass, snap to the integer grid, re-node, then
one scanline with a `std::map` of active edges ordered by crossing with the sweepline. bp takes
3.1 s on the buildings where dms takes 23.0 s, on the same data in the same integers. That gap is
the number of nodings, not the arithmetic.

This plan gives the dms dissolve the same shape: quantize each element once onto one fixed
lattice, clean it, append its ring edges to a bag, and node, sweep and polygonize the bag once at
store time.

## Semantics: from parity to counts

The sweep computes face membership by the even-odd rule per operand: bit 0 of `Segment::mask`
is operand A, bit 1 is B, identical fragments merge modulo 2, and the face parity propagates
upward from the unbounded face (`ComputeFaceParity`, `DMS_Traits.h:650`). That is right for two
operands. It is wrong for one bag of N elements under one mask, which would compute the XOR of
the elements: two overlapping buildings would cancel where they overlap.

bp resolves the same problem with counts, and so does this plan, in two phases.

**Phase A, per element, unchanged semantics.** Each element is cleaned alone with the even-odd
rule, which is exactly what `dms_clean_into` does today and what `dms_polygon` is. Its output is
a set of simple rings on the lattice: shells clockwise (positive GeoDMS area), holes
counter-clockwise, every hole assigned to its shell. This is where the fault tolerance lives, and
it does not move.

**Phase B, the bag, by coverage count.** A cleaned element is a valid polygon, so its boundary
has winding number 1 inside and 0 outside. The sum over elements is the coverage count, and the
union boundary is every fragment where the count changes between zero and positive. For a
directed ring edge `from -> to` define

    delta = LexLess(from, to) ? -1 : +1

as the change of coverage when crossing the edge from the face below it to the face above it.
For a clockwise shell (y up) a rightward edge has the interior below, so crossing upward leaves
it: -1. A leftward edge has it above: +1. A counter-clockwise hole gives the opposite signs by
the same rule, which is correct since entering a hole leaves the polygon. One rule, no ring type
lookup.

The noder orients every fragment `lo -> hi` and merges identical fragments (`MergeSegments`,
`DMS_Traits.h:351`); with a weight it sums the deltas instead of XOR-ing masks, and a fragment
whose mask and weight are both zero disappears, which is the corridor cancellation generalised.
The sweep then runs as it does now, over the same `SweepLine` (`DMS_Traits.h:562`), propagating
a count instead of a parity:

    countBelow(e) = countAbove(pred(e))        // 0 for the unbounded face
    countAbove(e) = countBelow(e) + delta(e)
    keep e  iff  (countBelow(e) != 0) != (countAbove(e) != 0)
    direct e lo -> hi when countBelow(e) != 0 (the result on its right), hi -> lo otherwise

The membership test is the nonzero rule, `count != 0`, not `count > 0`. After phase A every ring is
canonically wound, so no count is ever negative and the two tests agree; the nonzero form costs
nothing and keeps a ring that reaches the bag with the wrong orientation on the inside instead of
subtracting it. That matters because the BAG carries such rings: #1230 counted 105,051 buildings
stored counter-clockwise, none with holes. Phase A is still what handles them, since the even-odd
rule does not look at orientation at all; the nonzero rule in phase B is the second line.

What the nonzero rule does NOT license is skipping phase A and pouring raw rings into the bag.
That would change the documented reading of invalid input: a ring wound twice round a region is
inside under nonzero and outside under even-odd, and two overlapping outer rings in one feature
give their union under nonzero (275 in the `ring_encoding` case) where the even-odd reading gives
their symmetric difference (250, which that test pins). The dms family promises even-odd per
element, so phase A stays.

The polygonizer and the hole assigner take directed edges with the result on the right and use no
masks (checked: no `mask` in `DMS_Traits.h:700-945`), so they are untouched.

Why the answer is the same as today's: the current tower unions cleaned elements pairwise, union
is associative and commutative, and the union of the cleaned regions is one set whatever the
grouping. The only difference is the lattice, which today is derived per tile and is fixed once
below; on a fixed lattice both formulations snap identically.

## The fixed lattice, and the fixed frame

Two things must be fixed once per operator call, not per pair and not per tile.

**The cell.** Derive it once from the geometry attribute's values unit when that unit declares a
non-empty range (`Unit<P>::GetRange()`, `rtc/dll/src/tic/Unit.h:90`; `Range::empty()` is
`!IsStrictlyLower(first, second)`), through the existing `CellFor(extent, maxAbs)`
(`DMS_Traits.h:1096`). An `rdc`-style unit gives 7.6 um for the Netherlands, 2^-17 m. When the
unit declares no range, one pre-pass over all tiles with `CoordStats` gives the data extent and
the same formula; the delay-store path already loops over tiles sequentially
(`BoostPolygon.cpp:903`), so the pre-pass is the same loop once more, reading only. Integer point
types keep cell 1. This is the earlier decision recorded in the #1214 notes: it makes the result
independent of the tiling at the price of a coarser lattice for a small tile, 59.6 nm to 7.6 um on
a 4 km Amsterdam tile against BAG's own accuracy.

**The origin.** `SetFrame` today floors the origin to a cell multiple at or below each operand
pair's joint minimum (`DMS_Traits.h:1142`). Two cleaned elements therefore live in two internal
frames, and their `GPoint`s are not comparable. A bag needs one frame: origin =
`floor(rangeMin / cell) * cell` per axis, from the same declared range or pre-pass, and the
internal budget of 2^36 cells must cover the whole range, which `CellFor` guarantees by
construction through its extent term. This is what "keep coordinates and intermediate results in
that single lattice" means in code: a new `SetFixedFrame(cell, originX, originY)` on the engine,
honoured by `SetFrame` in place of the per-pair derivation, and used by phase A and phase B alike.
`CoordStats` still runs per element for the undefined-or-infinite check, and asserts that every
element lies inside the frame.

## Data flow

Per domain element of the result (one for a plain dissolve, one per partition, one per element
for `dms_polygon`), the `ResourceArray` slot that holds a `PolygonTower` today holds a
`DmsSegmentBag<P>` instead:

    struct DmsSegmentBag { std::vector<Segment> segs; SizeT nrElements = 0; };

with the frame kept once on the operator call, not per bag.

**Calculate, per tile** (`BoostPolygon.cpp:1660`, the loop stays): for every element, phase A
through an engine on the fixed frame, then append the edges of every ring it produced, with
`mask = 0` and `weight = delta` as above, to the element's bag. The float sequence is never
rebuilt. A new engine entry, `CleanToRings(const R& poly) -> const std::vector<Ring>&`, is
`Compute(Union, poly, EmptyRange())` without `Store`.

**StoreImpl, once** (`BoostPolygon.cpp:1724`, on the delay-store path with `no_tile`): for every
bag, an engine on the fixed frame runs `m_Noder.Run(bag.segs)`, the count sweep, the kept-edge
selection, `Polygonizer::Run`, `HoleAssigner::Run`, and `Store` writes the sequence with the
fixed frame's `Dequantize`. The split variants keep `dms_split_count` and `dms_split_assign` on
the written value for now; they can read the rings directly later.

A bag with `nrElements == 1` skips the noder: its rings are already clean and on the lattice, so
`dms_polygon` costs one cleaning per element as it does today, not two.

**No intermediate reduction.** An earlier draft reduced a bag in place past 2^26 segments. That
was a memory bound on the segment COUNT, not a coordinate bound, and it is dropped: it bought
nothing that has been measured and it reintroduced a grouping dependence, since iterated snap
rounding of a grouping need not reach the same fixed point as one pass over everything. The bag
is noded once, whatever its size. The memory model is plain: 1.4 M segments at 40 bytes is about
56 MB, plus the noder's hot pixels and its index of the same order; a national BAG, 23.5 M
buildings and roughly 260 M segments, is about 10 GB before the noder's own structures, to be
measured before any bound is added.

There is no coordinate bound to consider either, and it is worth saying why, since bp has one.
GeoDMS refuses bp coordinates beyond 2^26 (`MAX_COORD`, `BoostPolygon.cpp:47`) and translates to
the centre of the extent first. That rule exists because boost::polygon computes the crossing
coordinate in `long double`, which on MSVC is the 64-bit double, and verifies the rounded pixel
afterwards; the bound is a margin that keeps that floating arithmetic inside one pixel. The dms
noder computes the crossing pixel exactly, as a 128-bit rational rounded half up, and its only
budget is the 2^36 internal cells that keep the numerator below 2^114 (`DMS_Traits.h:145`), which
the fixed frame satisfies by construction. Int64 differences and Int128 products have no
ambiguity to guard against.

## Changes by file

`geo/dll/src/DMS_Traits.h`

- `Segment` gains `Int32 weight` beside `UInt8 mask`; `MergeSegments` XORs masks and sums
  weights, drops fragments with both zero; `SnapSegments` copies both into the fragments it makes.
- `SetFixedFrame(cell, ox, oy)`; `SetFrame` uses it when set, keeping the usability check.
- `CountEdge { Int32 delta; Int32 below; }` and `ComputeFaceCounts` beside `ParityEdge` and
  `ComputeFaceParity`, on the same `SweepLine`.
- `CleanToRings` and `UnionBag(std::vector<Segment>&)` as engine entries; `Store` unchanged.
- `DmsSegmentBag<P>`, `dms_append_rings(bag, rings)`.
- `DmsPolySet`, `union_dms_polygons`, `dms_clean_into` stay for the Minkowski operators
  (`BoostGeometryImpl.h:490-607`), which are out of scope here and can migrate to bags later.

`geo/dll/src/BoostPolygon.cpp`

- `DMS_PolygonOperator`: `PolygonTower` becomes `ResourceArray<DmsSegmentBag<P>>`; a
  `Prepare` step derives the frame once from the values unit or the pre-pass; `Calculate` does
  phase A and appends; `StoreImpl` does phase B per bag. `AbstrPolygonOperator` gains a virtual
  `Prepare(polyDataA)` hook, default empty, called once before the tile loop of the delay-store
  path; the parallel per-tile path (`dms_polygon` with no union) also calls it, since its frame
  must be the same.
- The overlay and connectivity branch (`BoostPolygon.cpp:522`) and the binary operators in
  `PolygonOper.cpp` are untouched: `weight` is 0 there and the parity sweep stays theirs.

## The crossing finder: what the quadtrees do, and what replaces them

The noder uses two quadtrees (`geom/SpatialIndex.h`), both candidate generators for an
all-pairs question. `CollectCrossings` (`DMS_Traits.h:422`) indexes every segment's bounding box
and queries it per segment to find the pairs with a proper crossing; snap rounding needs the pixel
of every crossing as a hot pixel before any segment is re-routed. `SnapSegments`
(`DMS_Traits.h:446`) indexes the hot pixels and queries them per segment to find which pixels each
segment passes through. The profile puts the first at 32 % of the buildings run and the second at
5.7 %; the cost is `ReFit`, the loop that walks candidate leaves and tests each box, and a
bounding box is a poor filter for a diagonal segment.

Phase A does not remove the need for the first pass. Cleaned elements still cross one another
wherever footprints overlap, which in the BAG is common, and their shared edges are collinear
overlaps that the merge resolves at endpoints. What phase A removes is crossings within an
element, which a single bag no longer pays for anyway.

The first quadtree is replaced by a Bentley and Ottmann sweep in exact arithmetic: edges enter
at their lower endpoint and leave at their upper one, the active set is ordered by the edge's
height at the sweep position, compared as an exact rational, and every newly adjacent pair in the
active set is tested for a proper crossing whose exact crossing is pushed on a heap keyed by
position; at a crossing event the two edges swap and their new neighbours are tested. The output
is the set of crossing pixels, computed as now with `CrossingPixel`. No snapping happens inside
this sweep: it only reports, and `SnapSegments` and the fixed-point iteration follow unchanged.
That keeps the invariant that events are at or after the sweepline, which snapping would break,
since a snapped pixel can lie half a cell behind the line and a snapped vertex can create a
crossing anywhere. Cost O((n + k) log n) with no false candidates, which is what the municipality
case wants, where a few long segments cross a dense polygon and any bounding-box method fetches
much of the polygon for each of them.

Two things follow from the existing code. The comparator `IsBelow` (`DMS_Traits.h:538`) is
static: it decides the order of two edges once, where the later one starts, which is valid only
because the edges it sees are already noded and cannot cross. The crossing sweep sees edges
that do cross, so it needs a comparator evaluated at the current sweep position, an exact
rational comparison of two heights, and it cannot reuse `SweepLine`. It is a new component of a
few hundred lines beside it, and it is the whole of step 3.

Built as described, with three details worth recording. The active set is a std::set of slots,
not of segments: at a crossing, the segments through it are re-assigned to the consecutive nodes
they occupy, sorted by direction and index, so the tree is never asked to compare at a rational
position; every comparison it does make involves the segment being inserted, which starts at the
current integer position, and is an orientation test, plus a direction test when the point lies
on both segments. The heap of crossing events is ordered by column first and exactly, with
256-bit products, only when two crossings share a column; whether a segment passes through a
rational crossing, the test that bounds the run to reverse, is a 256-bit orientation as well.
And the lesson that cost one build: a pair that becomes adjacent again after its crossing, once
whatever separated them has left, still crosses geometrically, and the sweep must ignore a
crossing behind its position as already reported. The first build treated it as an invariant
failure; the steep case, the t020 probe and the buildings failed on it while every parity case
passed.

The fallback, if the sweep is deferred, is bp's finder: sort segments by their lower x, bin them
by y through a histogram, and for each segment scan forward while the next segment starts before
this one ends, testing the y overlap and then the exact crossing. No tree, no false-candidate
walk, cache-friendly, and it is what runs in 3.1 s on the same buildings. It keeps the bounding
box weakness for long segments, which the sweep does not.

The second quadtree, the hot-pixel query, stays for now at 5.7 %.

## What must hold, and how it is checked

- The result no longer depends on the tiling or the element order. Testcase: the same dissolve
  with two tilings of the domain gives byte-identical sequences.
- Overlap is union, not XOR. New `testcases/oper_dms_union_counts.dms`: two overlapping squares
  give 175 not 150; three pairwise overlapping squares; four squares whose overlaps enclose a
  hole; a square inside another (count 2 stays inside); an invalid bow tie overlapping a valid
  square, which `oper_dms_family.dms` already pins at 7500.
- Existing pins unchanged: `oper_dms_family.dms` (dissolve area 16, the partitioned 8s, the
  23-point layout, agreement with `geos_union_polygon`), `oper_dms_overlay*.dms`, the t020 dms
  column within 1e-6 of geos on all four scenarios, `ring_encoding`.
- Determinism across runs is what it is today: exact predicates, sorted containers, no threading
  in the fold.
- Performance: measured, see the status at the top. The expectation written before the build,
  two to three times bp's 3.1 s, was not met: the bag path's 23.0 s against the tower path's
  25.8 s, both the update time of the area item. The quadtree crossing finder remains (step 4),
  and the profile that would say how much of the 23 s it takes has not been made for this path.

## Sequencing

1. `DMS_Traits.h`: weight, fixed frame, count sweep, bag entries. Compiles alone with the binary
   operators still on the parity path; `oper_dms_overlay*.dms` must still pass, which proves the
   parity path is untouched.
2. `BoostPolygon.cpp`: the operator on bags. `oper_dms_family.dms` and the new counts testcase.
3. Battery, t010, t020 dms column, then the timing. Done 2026-09-07 but for the t020 column;
   the numbers are in the status at the top.
4. The crossing sweep of the section above, replacing `CollectCrossings`' quadtree, with the
   parity path's testcases as its proof since the binary operators share the noder; then the
   timing again, on the buildings and on the municipalities, which is the case it exists for.
   Done 2026-09-07, two builds; the timing is at the end of this document.

Each step is one build through the coordinator. No wiki change: the operators keep their names,
arguments and semantics; only the lattice derivation is a documented change, on the
`dms polygon operators` page under the grid section, once measured.

## The t020 comparison on the crossing sweep

Sweep build 2 (Geo.dll 23:44:02), one coordinator window on 2026-09-07 from 23:46 to 23:50,
machine idle, every log free of `[E]`, all exits 0. The figure is the update time of the union's
area item utA from the /SP log, the metric of round 4, for all five families; medians of 5
repeats, except bp and cgal on the buildings (1 each) and bg, which the suite skips there
(#1176). The "before" column is the same metric on the tower-path dms of the afternoon's clean
run (`scratch/perf_clean`), and for the buildings round 4's single-noding figure, with the tower
path's 25,830 in brackets.

| scenario | elements | geos | bg | bp | cgal | dms | dms before |
|---|---|---|---|---|---|---|---|
| ManySmall | 4,096 | 156 | 684 | 28 | 314 | 26 | 149 |
| FewLarge | 16 | 4 | 4 | 7 | 29 | 10 | 18 |
| Municipalities NH | 45 | 491 | 766 | 384 | 4,330 | 352 | 1,202 |
| Buildings Amsterdam | 124,662 | 30,823 | skipped | 5,991 | 47,445 | 6,780 | 23,001 (25,830) |

All in ms. The engine's per-operator line, which leaves out the shared read of the source, on the
buildings: geos 28,066, bp 3,042, dms 4,150; on the municipalities: geos 369, bp 258, dms 228.

Same-window ratios by update time: buildings dms/geos 0.22 (round 4: 0.76) and dms/bp 1.13;
municipalities dms/geos 0.72, dms/bp 0.92; ManySmall dms/geos 0.17, dms/bp 0.93; FewLarge
dms/geos 2.5, dms/bp 1.4. Against its own earlier figures on the same metric, dms is 5.7 times
faster on ManySmall, 1.8 on FewLarge, 3.4 on the municipalities and 3.4 on the buildings (3.8
against the tower path).

Spread: under 4 per cent in every cell but two. geos on the buildings did four repeats at 30.7
to 31.1 s and one at 23.0 s, the bimodality of rounds 3 and 4 inside one window this time. dms
on the buildings ranged from 5.96 to 7.27 s around its median of 6.78, wider than any dms cell
before, and not investigated. Every dms area agrees with geos to 3e-10 relative or better; bp's
differ from both at 1e-7, its own lattice.

Where this leaves the dissolve: within 13 per cent of bp on the buildings by update time and
36 per cent on the operator line, ahead of every other family there, and ahead of all five on
the municipalities, the case the sweep was built for. The rest of the buildings' 4.2 s is
unprofiled; the hot-pixel quadtree in `SnapSegments`, phase A's 124,662 small engine runs and
the count sweep are the candidates, in no measured order.
