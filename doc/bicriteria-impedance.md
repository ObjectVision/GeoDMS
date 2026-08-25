# Bi-criteria (2D) impedance: assessment and proposal for issue #856

Status: v1 IMPLEMENTED (2026-08-25) as designed below; sections 1-4 are the assessment that led there.
Issue: https://github.com/ObjectVision/GeoDMS/issues/856 (jipclaassens, 2025-01-21, assigned MaartenHilferink)
Motivating use case: https://github.com/ObjectVision/NetworkModel_PBL/issues/19

Implementation summary (v1):
- Spec syntax: new ordered section `pareto`, optionally `pareto(OrgZone_max_imp2)` for the cutoff on the
  second criterion; the second per-link criterion rides `alternative(link_imp)`, the per-row cost output is
  its `alt_imp` production. Example:
  `'bidirectional;startPoint(Node_rel,OrgZone_rel);endPoint(Node_rel,DstZone_rel);cut(OrgZone_max_imp);alternative(link_imp):alt_imp;pareto;od:impedance,alt_imp,OrgZone_rel,DstZone_rel,EndPoint_rel'`
- Code: `BiCriteriaDijkstraHeap` in geo/dll/src/Dijkstra.h (beside the scalar heap), `BiNodeZoneConnector` +
  `ProcessBiDijkstra` in geo/dll/src/Dijkstra.cpp (sibling of the untouched scalar driver, sharing
  NetworkInfo/GraphInfo/inversions/ResultInfo and the sparse two-pass Counting/fill protocol),
  `DijkstraFlag::BiCriteria`/`Imp2Cut` in stx/dll/src/DijkstraFlags.h, one grammar clause in
  stx/dll/src/DijkstraString.cpp, and CheckFlags/CalcNrArgs/CreateResult/DescribeSpecSignature in lockstep.
- Preparatory refactor (same change set): ProcessDijkstra's per-origin task split into stage functions
  (WriteZonalResults, AccumulateInteraction + InteractionParams, WriteLinkSets, commitAndAccount), fill-pass
  ResultInfo assembly named (odResPtrs); scalar behavior unchanged.
- v1 admissibility exactly as section 5.3; both cutoffs, euclid(), bidirectional and start/end offsets
  (first criterion) supported; negative values rejected for the alternative impedances and offsets.
- Tests: testcases/fn_test_od_pareto.dms (fronts, dominance pruning, min-time equality against a plain
  impedance_matrix run, both cutoffs, endpoint parking-heap ties, multi-origin two-pass rows, and a
  function-body signature probe) plus _neg1/2/3 for the CheckFlags and negativity rejections.
- Still open: OVSRV10 CMake/Linux + full.py gate; wiki Impedance-options section on release; v2 items in 5.6.

Scope of the remainder of this document: analyse the request, assess "integrate into the existing operators
in Dijkstra.cpp with a specialized 2D OwningDijkstraHeap" versus "separate implementation", and record the
v1 design.

Decisions already taken (2026-08-25):
- v1 output = one result row per found (OrgZone, DstZone, imp, imp2) route, i.e. Pareto-front rows.
- National PBL-scale must be feasible in v1, so front-size controls are v1 requirements, not future work.


## 1. Problem statement

Issue #856 asks for a 2-dimensional Dijkstra variant in which travel time and travel cost are both carried in
the search, and a partial route is discarded only when it is strictly worse in BOTH dimensions — Pareto
dominance — rather than on a single scalar.

The concrete failure it addresses (NetworkModel_PBL#19): in transit routing with time as the impedance,
Dijkstra eliminates a fast-but-expensive first leg in favour of a slow-cheap one, yet the eliminated option may
be the only way to reach the destination within a maximum travel time. Fare structure (NS long-distance
discount, transfer tariffs, flat-rate ferries) makes cost non-proportional to time, so no single generalized
cost recovers the lost routes. Their validation criteria for a new model: minimum times must equal the current
operator's; costs of time-minimized routes consistent; new-model time <= old-model time.

What exists today: `alternative(link_imp):alt_imp` accumulates a SECOND link impedance along the tree found by
the FIRST — reporting only, no influence on pruning (see the wiki, Impedance-options.md: "The first impedance
is used for route decisions, while the alt impedance will be used to aggregate the interaction calculations").
Issue #856 is precisely the request to let the second dimension participate in pruning.


## 2. Why the current engine cannot do this (and what "a 2D heap" must therefore mean)

The engine (geo/dll/src/Dijkstra.h + Dijkstra.cpp) is a single-label-per-node Dijkstra:

- `DijkstraHeap::m_ResultDataPtr[v]` holds ONE tentative/final impedance per node; `IsBetter` (Dijkstra.h:160)
  keeps the minimum; `MarkFinal` (Dijkstra.h:169) finalizes a node exactly once per origin.
- `NodeZoneConnector::CommitY` (Dijkstra.cpp:342) commits each destination zone at most once per origin,
  relying on the frontier's monotonicity ("first commit is the cheapest").
- All tree-walking products (`UpdateALW`, link flow, LinkSet) assume ONE parent link per node
  (`m_TraceBackDataPtr[node]`), i.e. a unique shortest-path forest.

Two tempting shortcuts do not solve #856:

1. Instantiating the existing heap with a lexicographically ordered pair type. This is a mathematically valid
   Dijkstra instance (lexicographic order on pairs is total, and translation-invariant under componentwise
   addition of nonnegative pairs), but the single label per node keeps only the lexicographic minimum:
   "minimum time, and cheapest among exactly-minimum-time paths". Marginally better defined than today's
   alt_imp (which follows an arbitrarily tie-broken min-time tree), but it discards exactly the labels the
   issue needs (slower-but-cheaper alternatives).
2. Weighted-sum scalarization (several 1D runs over w*t + (1-w)*c) finds only supported Pareto points (on the
   convex hull of the front); non-supported optima — common under stepwise fares — are unreachable for any w.

Therefore a "specialized 2D OwningDijkstraHeap" must be a multi-LABEL structure: the heap holds labels
(t, c, node), several of which may refer to the same node, and per-node state supports a dominance test rather
than a single value. The heap ELEMENT can still be the existing template — `heapElemType<std::pair<ImpType,
ImpType>, NodeType>` (rtc/dll/src/vt/HeapElem.h:31 compares `m_Imp > b.m_Imp`, which for std::pair is
lexicographic) — what changes is the per-node state and the accept/prune protocol around it.


## 3. The algorithm: bi-criteria label-setting with O(1) per-node state

This is the classic bi-criteria shortest path label-setting method (Hansen 1980; Martins 1984 generalizes to
k criteria). The bi-criteria case admits a crucial simplification.

Invariant. Pop labels in lexicographically nondecreasing (t, c) order. Maintain per node v a single scalar
minCost[v] = minimum c over labels ACCEPTED at v so far (+inf if none). Then:

    a popped label (t, c) at v is Pareto-optimal among all v-paths  <=>  c < minCost[v]

- Discard direction: if c >= minCost[v], some earlier-accepted (t', c') at v has c' <= c and, by pop order,
  t' < t, or t' = t with c' <= c — so (t, c) is dominated or a duplicate. Using >= (not >) also terminates
  zero-weight cycles: a (0,0)-cycle re-delivers an identical label, which fails the strict test.
- Accept direction: suppose c < minCost[v] but some path label (t*, c*) <= (t, c) componentwise (one strict)
  existed. Then (t*, c*) precedes (t, c) lexicographically; induct along that path — every prefix label is
  componentwise <= (t*, c*) (nonnegative weights), pops earlier, and is either accepted or rejected in favour
  of a componentwise-better accepted label. Either way, by the time (t, c) pops, v has an accepted label with
  cost <= c* <= c, so minCost[v] <= c — contradiction.

Consequences:
- Accepted labels at each node are exactly the distinct Pareto front, first-of-duplicates, produced in
  strictly increasing t and strictly decreasing c.
- The FIRST accepted label per node (and per destination zone) has the 1D operator's minimum time. The
  NetworkModel_PBL#19 validation criterion ("min-time equality") is therefore a structural property of the
  algorithm, assertable in debug builds and testable in configs by joining against a plain impedance_matrix.
- Per-node state = ONE scalar + the existing zone-stamp lazy-reset trick (Dijkstra.h:119-149): stale stamp
  means minCost = +inf; ResetImpedances stays O(1) per origin. This part of the 1D design carries over free.
- The same test applies as a push-time pre-prune (minCost[v] is nonincreasing during a run, so a label failing
  it at push time also fails at pop time). Pending labels are NOT compared against each other — correct,
  merely a somewhat larger queue; this mirrors the existing IsBetter-before-push / MarkFinal-after-pop split.

Correctness conditions that must be made explicit in the implementation:

1. The heap comparator must be lexicographic on the FULL pair. A "heap on t with c as payload" is the trap:
   two equal-t labels at one node can pop in the wrong order and a dominated pair leaks into the output front.
   `heapElemType<std::pair<...>>` gets this right automatically.
2. BOTH link-weight arrays must be validated nonnegative. Today only the primary is checked
   (Dijkstra.cpp:1974 "Illegal negative value in Impedance data"); the alternative array is unchecked because
   it never drives pruning. In 2D, negative-cost cycles produce unbounded label sets (non-termination).
   The same holds for start/end point impedance offsets (the 1D release logic already silently assumes
   endpoint offsets >= 0; make it an explicit check).
3. The trick is intrinsically 2-dimensional. For >= 3 criteria the per-node scalar is insufficient and real
   per-node front storage is needed. Comment this at the definition so nobody "generalizes" it casually.

Endpoint-offset parking heap. The 1D engine parks endpoint candidates keyed by adjusted impedance and releases
them while `front().Imp() <= currImp` (Dijkstra.cpp:787-816, final flush :864-890); monotone frontier + first-
commit-wins makes per-zone results exact. In 2D this generalizes IF AND ONLY IF the parked key AND the release
comparison use the full pair (t+e, c). Counterexample when releasing on time alone: candidate (T, c) releases
at frontier (T, currC) with c > currC; a later frontier label (T, c''), currC <= c'' < c, with a zero offset
produces (T, c''), which also commits — the zone's committed "front" then contains (T, c) dominated by
(T, c''). With pair keys, every future released candidate is lexicographically >= every already-released one,
so a per-dstZone minCommittedCost with the same strict test yields the exact per-zone Pareto front. This is
literally the existing code shape with the pair-typed heap element substituted.

Result cardinality. Each OD pair yields a variable-size front, one row per accepted commit. The existing
sparse two-pass protocol — a Counting pass (Dijkstra.cpp:2056-2104), `make_cumulative_base`, `SetCount`, then
a fill pass addressed by `resCumulCount` bases (:892-910) — generalizes verbatim with
count = number of accepted commits. The dense one-cell-per-OD layout and impedance_table's
attribute<Imp>(DstZones) result shape structurally cannot hold a front.


## 4. Integration assessment

Three options were considered.

### Option A — weave dominance into the existing scalar loop (template or branch ProcessDijkstra)

Rejected. The ~500-line per-origin task (Dijkstra.cpp:677-1227) leans on single-label invariants throughout:
MarkFinal-once-per-node, CommitY first-commit-wins, TreeRelations' one-parent-per-node forest, interaction
potentials and link flow over a unique tree, LinkSet by walking node traceback. Threading dominance through
this multiplies invariants and puts branches in the hot loop of the performance-critical mainline (the t720/
2BURP-class regression workloads). The cost/benefit is upside-down: the 2D loop needs a fraction of the 1D
loop's products.

### Option B — integrate at the operator level; separate bi-criteria core loop  [RECOMMENDED]

Same operator family (impedance_matrix / impedance_matrix_od64), same spec-string dialect, same argument
extraction, unit unification, result-item creation, registration, and the same two-pass counting protocol —
plus a new heap sibling in Dijkstra.h and a compact sibling driver in Dijkstra.cpp. The scalar loop is not
touched, so existing workloads carry zero performance risk. Precedent in-repo: GridDist.cpp:415 already drives
the NON-owning DijkstraHeap with its own loop — "shared data structure, separate driver" is the established
pattern.

### Option C — separate operator / new file

Rejected. `NetworkInfo`, `GraphInfo`, `NodeZoneConnector`, `ResultInfo` are file-local templates in
Dijkstra.cpp; `CreateResult` (:1592-2206), `DescribeSpecSignature` (:1397-1590), `CalcNrArgs` and registration
are ~1500 lines of glue that a new file would duplicate or force hoisting of — for zero user-facing benefit —
while forking the spec-string dialect, wiki documentation and test surface.


## 5. Proposed v1 design

### 5.1 Data structures

`geo/dll/src/Dijkstra.h` — new `BiCriteriaDijkstraHeap<NodeType, LinkType, ZoneType, ImpType>` BESIDE
`DijkstraHeap` (sibling, not derived — the protocols differ; sharing by inheritance would obscure both):

- label min-heap: `std::vector<heapElemType<std::pair<ImpType,ImpType>, NodeType>>` with std::push_heap /
  std::pop_heap — lexicographic order falls out of the existing element template;
- per-node `minCost` array (OwningPtrSizedArray<ImpType>) + the existing zone-stamp members for O(1) reset;
- cutoffs maxT (mandatory, from cut(OrgZone_max_imp)) and maxC (optional);
- API mirroring the 1D shape: `InsertLabel(v, t, c)` (pre-prune: t < maxT, c < maxC, c < minCost[v]),
  `PopLabel()`, `AcceptLabel(v, c)` (pop-time test, then minCost[v] = c), `Empty()`, `Front()`.
- NO label pool in v1: without traceback/LinkSet, an accepted label can be forgotten after commit bookkeeping;
  the queue element is ~20-24 bytes and per-label storage is zero. A `useTraceBack`-style switch reserves the
  pool (t, c, node, parentLabelIdx, viaLink) for v2 — and its presence must never alter prune decisions
  (required anyway for two-pass consistency, since the counting pass runs without outputs).

While here: fix the stale Dijkstra.h comment claiming `m_AltLinkWeight` / `m_LinkAttr` are unused ("potential
future use") — Dijkstra.cpp:693-694, :714-722, :976-982 and :1159 use them as per-node accumulators for
UpdateALW and as the link-flow accumulator. They are single-label scratch, irrelevant to (not reusable by) the
2D heap.

`geo/dll/src/Dijkstra.cpp`:

- `ProcessBiDijkstra`, a sibling driver sharing `NetworkInfo`, `GraphInfo`, the inverted relations,
  `ResultInfo`, the dms_combinable worker pattern, parallel_for-per-origin, progress reporting, and the
  two-pass Counting protocol. Endpoint offsets via the same parking-heap code shape with pair keys for the
  element, the release test and the final flush. A periodic cancellation check every N pops inside the label
  loop (one check per origin is no longer sufficient — a single bicriteria origin can run orders of magnitude
  longer than a 1D origin).
- `BiNodeZoneConnector` (sparse regime only): per-dstZone `minCommittedCost` + the `m_LastCommittedSrcZone`
  tick-stamp trick; per-origin commit list of (y, t, c) replacing `m_FoundYPerRes`; Res2DstZone/Res2EndPoint
  mappings reproduced. Per-origin commit counts in SizeT — with fronts, the UInt32 `ZonalResCount` bound
  (today capped by nrDstZones) is no longer structural. Do not copy the dense-regime Res2EndPoint identity
  fallthrough documented as wrong at Dijkstra.cpp:428-434 (moot in a sparse-only connector, but the comment is
  the warning).

### 5.2 Spec-string surface

Recommendation (exact section name bikesheddable): ride the existing `alternative(link_imp)` argument slot for
the second link array — its extraction (Dijkstra.cpp:1632), `imp2Unit` values unification (:1727-1738) and the
`alt_imp` result item (:1852) already exist — and add a compact new ordered section that flips the engine:

    impedance_matrix_od64(
        'bidirectional;startPoint(Node_rel,OrgZone_rel);endPoint(Node_rel,DstZone_rel)'
        ';cut(OrgZone_max_imp);alternative(link_imp):alt_imp'
        ';pareto'                                -- or: pareto(OrgZone_max_imp2) for the optional cost cut
        ';od:impedance,alt_imp,OrgZone_rel,DstZone_rel,EndPoint_rel'
        , ...args... )

Under pareto mode the wiki's definition of alt_imp — "the total alternative impedance of the found route" —
stays literally true: each result row IS a found route. Per-row sub-items in v1: `impedance` (t), `alt_imp`
(c), `OrgZone_rel`, `DstZone_rel`, `EndPoint_rel`.

Plumbing: one new mode flag in stx/dll/src/DijkstraFlags.h (free bits exist: 0x40'0000, 0x80'0000,
0x2000'0000..0x8000'0000, >= 0x10'0000'0000'0000; bit values must remain stable — never renumber), plus one
new ordered clause in `paramRule` (stx/dll/src/DijkstraString.cpp:197-209), plus one optional argument if the
cost cut is taken as an argument of the pareto section.

The alternative naming — a self-describing section `bicriteria(link_imp2)` with its own argument slot — is
defensible (avoids overloading "alternative") at the cost of touching CalcNrArgs/extraction/signature for a
second array slot that duplicates what alternative(link_imp) already provides. Either choice is confined to
DijkstraFlags.h + DijkstraString.cpp + CheckFlags + the three lockstep sites (5.4).

### 5.3 v1 admissibility (CheckFlags)

pareto requires: OD (+ od_uint64 available for row counts), `alternative(link_imp)`, `cut(OrgZone_max_imp)`.
The cut is required, not merely tolerated, for two independent reasons: (a) layout — the dense branch
(resultCountBase = nrDstZones * orgZone, Dijkstra.cpp:896) is structurally single-label, and the sparse
machinery is keyed off SparseResult; (b) performance — without a time cut every origin explores the full
network with full fronts, and the motivating query always has a max time.

pareto forbids in v1: interaction/trip-distribution (all Interaction* / Prod{Org,Dst}* aggregates),
`Link_flow`, `LinkSet`, `link_attr` accumulation, `SumImp`/`SumLinkAttr`, `TraceBack`, `limit(...)`,
`precalculated_NrDstZones` (counts zones, not labels), and the impedance_table form. Note the tree-walking
products are structurally impossible as written, not merely ill-defined: TreeRelations/UpdateALW/link-flow
walk a per-NODE parent forest via `m_TraceBackDataPtr[node]`, whereas accepted bicriteria labels form a forest
over LABELS. `limit()`'s cumulative-mass semantics has no canonical front generalization (count zone mass at
first commit only? per label?) — any choice is defensible, none obviously right; defer rather than guess.

Start/end point impedance offsets apply to the t dimension only in v1 (cost offsets are v2 plumbing; the
engine is ready for them).

### 5.4 Hard invariants for the implementation

1. Three-way lockstep: every flag/argument/sub-item lands simultaneously in `CalcNrArgs`, `CreateResult`'s
   extraction order, and `DescribeSpecSignature` — including its `ResultMembersComplete()` claim, which
   declares the OD sub-item set COMPLETE for a given flag set. This is a third copy of the argument layout,
   guarded only by `assert(i == CalcNrArgs(df))` (Dijkstra.cpp:1533).
2. Two-pass determinism: the counting and fill passes must replay bitwise-identical searches — identical
   insertion order, identical heap tie resolution, and identical prune behaviour whether or not output
   structures are allocated. A count mismatch in the fill pass hits
   `MG_CHECK(flags(df & DijkstraFlag::PrecalculatedNrDstZones))` (Dijkstra.cpp:905) — an internal-error crash
   path in normal mode; it doubles as a determinism tripwire. parallel_for over origins does not perturb this
   (each origin task is self-contained), but any "optimization" that lets worker-carried state influence
   pruning would break it nondeterministically.
3. The OD result is written through untiled raw pointers (`GetDataWrite(no_tile, ...)`; the numResultTiles
   parameter is vestigial) — the whole result is one contiguous allocation. With fronts multiplying row
   counts, this buffer, not the algorithm, is the first memory ceiling at national scale.
4. Worker-memory: the queue is O(open labels); combinable buffers are grow-only across origins per worker
   (the existing my_vec_t reuse pattern). One pathological origin times 16-32 workers is the second memory
   ceiling; add a cheap shrink-if-huge policy per origin.

### 5.5 Performance and scale posture (national PBL-scale is a v1 requirement)

- Complexity: O(L log L) for L = pushed labels, L ~= sum over v of front(v) * deg(v). Fronts are worst-case
  exponential, pseudo-polynomial under integer costs: accepted labels per node have strictly decreasing cost,
  so front(v) <= number of DISTINCT reachable cost values in [0, maxC]. That bound — not the heap — is the
  whole performance story, and it is the user's main tuning knob.
- Primary mitigation, designed in from day 1 but implemented in USERSPACE: cost quantization. Supply costs as
  integer cents / eurodimes (or `round(cost / eps)`); with maxC = EUR 50 at EUR 0.10 resolution, fronts are
  hard-capped at 501 labels per node and typically far fewer. This keeps the operator EXACT with respect to
  its inputs (no approximate-result semantics inside the engine), keeps two-pass determinism trivial, and is
  GeoDMS-idiomatic — the DistTypeList already instantiates UInt32/UInt64 impedances, and Float64 holding
  integral values enjoys the same distinct-values bound. Document the front-size ∝ distinct-cost-values
  relationship prominently in the operator documentation.
- maxT (mandatory) + maxC (optional but recommended) box the label space; each is one comparison per push.
  The euclid() filter composes soundly (it rejects zones at commit time, orthogonal to labels).
- Honest planning number for continuous (unquantized) costs: 10-50x a 1D per-origin run, times 2 for the
  counting pass, across all origins. A naive v1 without quantization would be feasibility-limited on exactly
  the PBL workloads that motivated the issue — hence the posture above.
- Fallback if userspace quantization proves insufficient in practice: an in-engine epsilon-dominance
  parameter (bucketed minCost test). Reserve grammar space for it; do not build it in v1. Landmark/A*-style
  goal direction is a further future speedup, out of scope here (cf. doc note in wiki Impedance-future.md).

### 5.6 v2 candidates, in order

1. LinkSet per od-row via label parent chains (turns on the label pool; memory class changes).
2. Per-label StartPoint_rel (requires per-label origin tracking).
3. Cost offsets at start/end points (engine-ready; argument plumbing only).
4. limit() semantics for fronts (needs a semantic decision first).
5. In-engine epsilon-dominance; landmark pruning.


## 6. Validation plan (for the implementation phase)

- Structural assert (debug builds): the first accepted commit per destination zone equals the 1D operator's
  impedance for the same spec minus the pareto section.
- New regression config: the NetworkModel_PBL#19 counterexample in miniature — slow-cheap versus
  fast-expensive first leg, fares non-proportional to time — asserting (a) exact front contents per OD pair,
  (b) min-time label equality against a plain impedance_matrix run (join on OrgZone_rel/DstZone_rel),
  (c) two-pass count consistency (the MG_CHECK tripwire stays silent), (d) alt_imp of the min-time row equals
  the 1D alt_imp where the 1D tie-break is unique.
- Determinism: same config run twice, byte-identical results.
- Scale probe on OVSRV10 before release: national network, quantized costs, measured front-size distribution
  and worker memory (validates 5.5's assumptions).
- Build/verify per house policy: per-step msbuild Debug x64; scoped operator tests during development;
  TestDebugUnit.bat once at the end; CMake/WSL + full.py gate on OVSRV10.


## 7. Side-finding, independent of #856

`od:...,StartPoint_rel` (DijkstraFlag::ProdOdStartPoint_rel) creates its result item (Dijkstra.cpp:1907),
wires it into ResultInfo as od_StartPointIds with write_only_all (:2159) and commits the lock — but
ProcessDijkstra never writes od_StartPointIds (only od_SrcZoneIds, od_DstZoneIds, od_EndPointIds are filled).
Requesting StartPoint_rel therefore yields an uninitialized attribute. Until implemented, the flag should be
rejected in CheckFlags with a clear message. Tracked separately from #856.


## 8. Draft comment for issue #856 (to be reviewed and posted by a maintainer)

> **Assessment and proposed direction**
>
> Analysed against the current impedance engine (geo/dll/src/Dijkstra.h/.cpp). Summary:
>
> 1. This cannot be obtained by instantiating the existing heap with a (time, cost) pair: the engine keeps one
>    label per node, so only the lexicographic minimum survives — "min time, cheapest among min-time paths".
>    The request needs bi-criteria label-setting (Hansen 1980): multiple labels per node, a label pruned only
>    when Pareto-dominated.
> 2. The bi-criteria case is cheap on state: popping labels in lexicographic (time, cost) order, exact
>    dominance needs only one scalar per node (minimum accepted cost so far), which slots into the existing
>    zone-stamp reset machinery. A structural bonus: the first accepted label per destination zone equals the
>    current operator's min-time result — the validation criterion from NetworkModel_PBL#19 holds by
>    construction.
> 3. Proposed integration: same impedance_matrix/_od64 operator family and specification string — the second
>    link impedance rides the existing `alternative(link_imp)` argument — with a new spec section (working
>    name `pareto`, optionally `pareto(OrgZone_max_imp2)` for a cost cutoff) that switches to a separate
>    label-setting core loop and a new heap sibling in Dijkstra.h. The scalar hot path is untouched. A
>    separate operator/file would duplicate ~1500 lines of argument/unit/result glue and fork the spec
>    dialect; weaving dominance into the existing loop would put branches in the performance-critical
>    mainline.
> 4. Output: one od-row per Pareto-optimal (time, cost) label, via the existing sparse two-pass count+fill;
>    requires `od` + `cut(OrgZone_max_imp)`. v1 products: impedance, alt_imp, OrgZone_rel, DstZone_rel,
>    EndPoint_rel. Interaction/link_flow/LinkSet/limit() excluded in v1 (tree-shaped products need per-label
>    traceback; planned as follow-ups).
> 5. Scale: front size per node is bounded by the number of distinct reachable cost values below the cost
>    cutoff — so the intended usage at PBL scale is quantized costs (integer cents/eurodimes) plus the
>    time and optional cost cutoffs. The engine stays exact w.r.t. its inputs; "cheapest within a time budget"
>    is then a trivial post-processing selection on the result rows.
>
> Full design note with the correctness argument, code anchors and v2 roadmap: doc/bicriteria-impedance.md.
