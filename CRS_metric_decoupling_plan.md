# CRS / metric decoupling for coordinate units (#1119 follow-up)

Status: **design agreed, not yet implemented.** Interim mitigation in place (see bottom).
Origin: t060 (BAG20 GeoPackage snapshot) regressed to `data error / exit 1` on 20.2.0.m.
Revised 2026-07-29: the storage model changed (see *Revision note*), two open decisions are
now settled by evidence, and every file:line anchor below was re-verified against the tree
at that date.

> **Revision note (2026-07-29).** The earlier version of this document chose to make CRS units
> **projection-bearing over a canonical `m` base**, carrying SpatialReference + DialogData as
> `UnitProjection` fields. That is superseded. SpatialReference now becomes a **peer property of
> Metric** — its own value object, its own slot, its own derivation and unification rules —
> and DialogData **leaves type identity entirely**. Rationale in §4 and §5.
> The old anchors also predated the tic→rtc merge (`tic/dll/src/...` no longer exists) and had
> drifted ~28 lines.

## Symptom

`t060` (`C:/dev/tst/Projects/BAG20/cfg/BAG20_MakeSnaphot.dms`) failed at **config-compile
time** (0:00:00, 0.00 GB, exit 1). The fatal abort is `Cannot find Item selectie/org_rel`,
but that is a *cascade*. The real first error was:

```
Operator area Error: the result unit (m²) is not compatible with the coordinate metric ^2 of <blank>
```

Introduced by **#1119** (`5949df20`, "area/arc_length: convert result to the requested unit;
add unary auto-derive form"), which turned `area(geom, m2)`'s 2nd argument from a label into a
real result unit with dimensional validation in `GeoMeasure_ValidateAndWarn`
(`geo/dll/src/OperPolygon.cpp`).

## Root cause: the packing

The BAG20 coordinate unit is

```
unit<fpoint> rdc_base : SpatialReference = "EPSG:28992", DialogData = "wmts_layer";
unit<fpoint> rdc    := range(rdc_base, point_yx(300000f,0f), point_yx(625000f,280000f));
```

`rdc` has **no linear metric**. Its "metric" is a CRS identity tag: a single base unit of
power 1 whose symbol is `"<SpatialReference>\xFF<DialogData>"`. #1119's validation squares
that tag and compares it to `m²` → mismatch → throw.

There is exactly **one producer, one materialiser, three decoders**, plus one sniff-to-reject.

| role | site | what it does |
|---|---|---|
| **producer** | `rtc/dll/src/tic/Unit.cpp:130` `UnitBase<V>::GetKeyExprImpl` | synthesises the DataController **key expression** `BaseUnit("<SR>\xFF<DD>", (vt))`; `MG_CHECK`s that SR itself contains no 0xFF |
| **materialiser** | `clc/dll/src/OperUnit.cpp:372` `AbstrBaseUnitOperator::CreateResult` | turns that string into `m_BaseUnits[name] = 1`. Sets **only** the metric — never `SetSpatialReference`, never DialogData |
| decoder | `rtc/dll/src/tic/AbstrUnit.cpp:404` `GetBackgroundReference` | DialogData first, else the part **after** 0xFF |
| decoder | `rtc/dll/src/tic/AbstrUnit.cpp:421` `GetSpatialReference` | side table first, else the part **before** 0xFF |
| decoder | `rtc/dll/src/tic/AbstrUnit.cpp:437` `GetCurrSpatialReference` | idem, on the Curr path |
| sniff | `geo/dll/src/OperPolygon.cpp:252` | rejects a metric containing 0xFF so #1119 does not square a CRS tag |

### Why the packing exists — what it buys that the side tables don't

The primary channels are structurally unreachable from the objects that must answer the question.
`SetSpatialReference` (`AbstrUnit.cpp:395`) writes only a pointer-keyed global side table
(`AbstrUnit.cpp:126` `static_quick_assoc<const AbstrUnit*, TokenID> s_SpatialReferenceAssoc`,
flagged by `USF_HasSpatialReference`, `rtc/dll/src/tic/TreeItemFlags.h:81`). DialogData is a
per-TreeItem `StoredPropDef`. **Cache units are in neither table**, and there is no cache→config
back edge.

The metric is the only value-carrying component that survives into cache-land, because
`RangedUnit<V>::GetMetric` **delegates to the referred item** (`rtc/dll/src/tic/Unit.cpp:679`)
— the mechanism `GetSpatialReference()` conspicuously lacks. That asymmetry is the whole bug.

So the packing buys **three** distinct things, and a replacement must supply all three:

1. **Transport** into cache units, via metric's referred-item delegation.
2. **Type identity.** Identical `(SR,DD)` ⇒ identical key expr ⇒ identical DataController ⇒ *the
   same cache unit object*. That pointer identity is what makes
   `AreEqual(const UnitProjection*)` (`rtc/dll/src/tic/Metric.cpp:396`, which compares the
   composite base **by pointer**) accept two same-CRS units and reject different-CRS ones.
   Dropping σ from the key would be an **over-strictness** regression, not a leak.
3. A readable value on an object that has no config TreeItem.

Traced end to end for `rdc`: `range()` → `AbstrUnit::DuplFrom` (`AbstrUnit.cpp:668`) makes the
projection base the cache unit produced by that `BaseUnit(...)` term, and
`shv/dll/src/GraphDataView.cpp:152` reads `GetBackgroundReference()` off it. The 0xFF string is
genuinely the only carrier.

**Also load-bearing:** `static_quick_assoc` is an unsynchronised `std::map`. The side table
therefore cannot be the home for a property that operators must write on worker threads — which
independently rules out "just make `SetSpatialReference` work on cache units".

### Accidental costs

- `AbstrBinUnitOperator::CreateResult` does *arithmetic* on the CRS tag as if it were a physical
  base unit (hence the `size()==1 && power==1` guards everywhere).
- The raw `0xFF` leaks into every rendered metric and from there into unification errors, detail
  pages, `CalcExpr` renders, `GetSourceName` and the scale bar — so **GeoDMS logs are not valid
  UTF-8**. This bit a regression run on 2026-07-29 (see `feedback_no_pythonutf8_for_fullpy`).
- A DialogData difference alone makes two units fail to unify with `" (incompatible Metrics)"` —
  a cosmetic presentation hint acting as a type error.

## Target model

### The template to copy

`Metric` already defines the pattern this design mirrors:

- **value object** — `rtc/dll/src/tic/Metric.h:18` `struct UnitMetric : SharedBase`, immutable,
  refcounted, unit-independent, `nullptr == absent`.
- **comparison** — `rtc/dll/src/tic/Metric.cpp:333` `AreEqual(const UnitMetric*, const UnitMetric*)`:
  pointer fast path; **empty unifies only with empty**.
- **unification** — `rtc/dll/src/tic/AbstrUnit.cpp:346` `UnifyValues`: type check → the
  `UM_AllowDefaultLeft/Right` leniency short-circuit → `AreEqual(metric)` → `AreEqual(projection)`.
- **derivation** — `rtc/dll/src/tic/UnitCreators.cpp:176`: two non-empty must be `AreEqual` (else
  `throwCompatibleError`), an empty one **adopts** the non-empty ("empty metrics are overruled").

Note the split: `AreEqual` is *strict*; the leniency lives outside it, in exactly two places.
SpatialReference copies this split verbatim.

### SpatialReference — a peer of Metric

New value object `UnitCrs` (`rtc/dll/src/tic/Crs.h/.cpp`), mirroring `UnitMetric`: refcounted,
immutable, holding a `TokenID m_SpatialRef`, with `AreEqual(const UnitCrs*, const UnitCrs*)`
strict and empty≡empty-only.

**Storage:** a `SharedPtr<const UnitCrs> m_Crs` member on `AbstrUnit` — not on `RangedUnit<V>`,
because `unit_traits<SharedStr>` and `unit_traits<Void>` resolve to `UnitBase`. Accessors
`GetCrs`/`GetCurrCrs` delegate to the referred item exactly as `GetMetric` does, which is what
restores transport. This **replaces** `s_SpatialReferenceAssoc` and `USF_HasSpatialReference`
outright — a strict simplification that also removes a destructor coupling and a thread-safety
hazard. `GetSpatialReference()` becomes a thin wrapper.

**Identity:** σ stays in the key expression, as its own nested term rather than a packed string:

```
(CrsUnit "EPSG:28992" (BaseUnit ... (fpoint)))
```

**Derivation:** absorption in `UnitCreators.cpp`, plus `DuplFrom`, `gridset`, and
`AbstrBinUnitOperator`. Under `*` and `/`: `crsPoints * 2.0` keeps σ; `crs * crs` and `crs / crs`
drop it (there is no `EPSG:28992²`). This matches what the packing does today via its
`size()==1 && power==1` guards, so it is behaviour-preserving.

**Unification:** a third check block in `UnifyValues` after the projection block, erroring with
`" (incompatible SpatialReferences)"`. `GetProjMetrString` must append σ, or the error shows two
identical-looking units with no visible reason.

### DialogData — leaves type identity (RULING, 2026-07-29)

DialogData does **not** get comparison or unification rules. It is carried by a σ→background
registry, consulted by `GetBackgroundReference` before the (eventually deleted) fallback.

**Why not symmetric with σ** — the two differ in kind:

- A **σ mismatch is a correctness bug**. `clc/dll/include/OperConv.h:288` `Type2DConversion`
  performs a **real OGR coordinate transformation** when two non-empty σ differ. Two σ-tagged
  attributes silently unified are two coordinate systems overlaid as one. Hence: strict, error,
  and `empty ≠ EPSG:28992` (a bare `dpoint` must not silently acquire a CRS from a sibling).
- A **background mismatch is cosmetic** — one consumer (`GraphDataView.cpp:152`), one effect
  (which WMTS tiles draw under the map). Making it a type error is what the packing does today,
  and that is a bug. Removing it is a *permissive* change, so it cannot break a config that
  compiles today.

This asymmetry is the main argument for separating the two rather than moving both onto one
comparable field.

### Linear metric for σ-bearing units — projected CRS only

With σ in its own slot, a coordinate base unit may carry a real linear metric **and** a σ. Derive
it from σ via the existing GDAL hook `s_GetUnitlabeledScalePairFunc` (`AbstrUnit.cpp:502`,
installed in `stg/dll/src/gdal/gdal_base.cpp`).

**Only when the CRS is projected.** For geographic CRS the hook returns `"degree"`, and a
`{degree:1}` metric would turn `area(geom, m2)` into a **hard error** for 2BURP, 2UP and
`tst/Unit/CRS`. Leave geographic CRS metric-free; geodesic area is a separate issue.

`tst/Storage/cfg/regression.dms` already proves the target shape works:
`unit<fpoint> point_rd := baseunit('m', fpoint), SpatialReference = "EPSG:28992";`

## Migration

Seven stages, each independently committable. **0xFF removal is Stage 7.** Stages 2–6 run the new
channel *in parallel* with the packing by nesting the key terms, so nothing is deleted until the
replacement demonstrably carries the information.

| Stage | Content | Behaviour | Size |
|---|---|---|---|
| **0** | Pre-existing defects (below) | bug fixes | small |
| **1** | `UnitCrs` value object + `AbstrUnit` slot + accessors; retire `s_SpatialReferenceAssoc` + `USF_HasSpatialReference`; keep 0xFF fallback last | preserving | medium |
| **2** | `CrsUnit` operator + token + **nested** key term; Debug drift detector | preserving (cache-key churn) | medium |
| **3** | σ→background registry; `GetBackgroundReference` consults it before the fallback | preserving | small |
| **4** | Derivation: `DuplFrom`, `gridset`, `AbstrBinUnitOperator`, absorption in `UnitCreators` | preserving (redundant 2nd channel) | medium |
| **5** | Unification block + `GetProjMetrString`; **`GetRawValue` override** on `SpatialReferencePropDef` | **changing**: stricter σ, looser DD | small-med |
| **6** | Derive linear metric from σ (projected CRS only) | **changing** | small |
| **7** | Delete producer, 3 decoders, the `OperPolygon` sniff, the drift detector | **changing** | medium |

**Stage 0 — pre-existing defects, unrelated but inside the blast radius.** Land first so any
fallout is attributable:
- `clc/dll/include/OperUnit.h:252` asserts `args.size()==2` but reads `args[2]` — out of bounds
  on `unit<Float64> x := u ^ n`.
- `rtc/dll/src/tic/UnitCreators.cpp` compares projections with `CrdTransformation::operator==`,
  which is **base-unit-blind**; unify onto `AreEqual(const UnitProjection*)`.
- `AbstrUnit.cpp:668`/`CopyProps` can pass an empty token into `SetSpatialReference`, whose first
  line is `dms_assert(!format.empty())`.

**The correctness proof for the cutover** is the Stage-2 Debug-only drift detector (modelled on
the SigUnitChecker replay in `rtc/dll/src/tic/OperSignature.cpp`): assert
`GetCrs()->m_SpatialRef == <0xFF-decoded value>` on every unit carrying both, then run the whole
regression corpus in Debug. Nothing is deleted until that is silent across the corpus.

**Testing.** Every stage: `testcases/run_testcases.bat` + `testcases/run_roundtrip.ps1`.
Stages 4, 6 and 7 additionally require the full 26-test `prj_snapshots` regression — grep cannot
settle whether a real config relies on two distinct same-σ units unifying. Stage 5 adds a new
testcase pair pinning: same-σ unifies; different-σ errors; σ-less unifies with σ-less; σ-less does
**not** unify with σ-bearing; same-σ with *different* DialogData **does** unify (the un-erroring).

**Stage 5 caveat — the raw/cooked seam.** `SpatialReferencePropDef::GetRawValue` must read only
the own slot, with no delegation and no projection walk. Without it the newly-effective getter
makes derived units dump a `SpatialReference = "..."` line they do not dump today, breaking the
roundtrip gate. This repo has been bitten by exactly this seam before (commit `c390758b`).

### Compatibility checklist

| Item | Status |
|---|---|
| On-disk unit blobs | **Unaffected** — `RangedUnit<V>::StoreBlobStream` (`Unit.cpp:602`) writes only the range; the metric is never serialised |
| Config dumps / roundtrip | Guard with the Stage-5 `GetRawValue` override |
| `.dms` config syntax | Unchanged — `SpatialReference=` / `DialogData=` keep working |
| Exported GPKG/GeoTIFF SRS | Byte-identical (same token → same WKT) |
| CalcCache | Invalidated ⇒ land Stage 2 behind a **minor version bump** |
| Two same-σ units unify | **Preserved** (σ stays in the key expr) |
| Two same-σ, different-DialogData units unify | **Changed: now succeeds** (was `" (incompatible Metrics)"`) — intended |
| `BaseUnit('EPSG:28992', fpoint)` hand-written in a config | Becomes a plain metric with no CRS meaning. No in-repo config does this; recommend an `ST_Warning` when a `BaseUnit` symbol matches `^EPSG:` |

## What this fixes for free

**#1119.** The 0xFF sniff (`OperPolygon.cpp:252`) becomes provably dead and is deleted. For a
projected CRS the composite base now carries `{m:1}`, so `area(geom, m2)` matches at factor 1 with
no diagnostic — the correct semantics, value-neutral for metre CRS. The warn-instead-of-throw
branch is **narrowed, not deleted**: it survives only for geographic/metric-less CRS, with a
message naming the real problem ("EPSG:4326 has no linear metric; reproject before requesting m²")
instead of the 0xFF story. Its eventual removal is a separate decision about geodesic area.

**UTF-8 logs.** `0xFF` is not legal in any UTF-8 sequence. Removing the packing makes every
rendered metric — unification errors, detail pages, `CalcExpr`, `GetSourceName`, scale bar — valid
UTF-8, with no encoding work anywhere.

**Three more:** the unsynchronised `s_SpatialReferenceAssoc` disappears along with a flag bit and a
destructor coupling; `GetSpatialReference()` gains the referred-item delegation whose absence
caused this whole mess; and presentation data leaves type identity, which
`doc/development/typed-hof-language-design.md` already flags as a defect.

## Open questions

1. **`GridDist.cpp` uses the immediate projection base, not `GetCompositeBase()`** — works for a
   single hop, silently returns empty for a chained gridset, and empty means "skip", not "fail".
   Convert in Stage 5 and add a `tst/Unit/CRS` case. Most likely site to break quietly.
2. **Registry conflict**: two units declaring the same σ with different backgrounds. Proposal:
   first non-empty wins, `ST_Warning` on conflict. Scan all `*.dms` for units carrying both
   `SpatialReference=` and `DialogData=` before coding.
3. **`GetUnitlabeledScalePair().second` has no consumer today.** Candidate use in Stage 6 as the
   projection factor for non-metre projected CRS (US survey foot).
4. **UNVERIFIED (needs a debugger, not a grep):** that a config unit's `mc_RefItem` chain really
   terminates at the `BaseUnit` cache unit for `rdc.dms` — one breakpoint at
   `GraphDataView.cpp:152` settles it; and whether the GDAL `SetFromUserInput` fallback currently
   misparses or silently drops a 0xFF-bearing string.

## Interim mitigation (IMPLEMENTED)

Until the above lands, `GeoMeasure_ValidateAndWarn` (`geo/dll/src/OperPolygon.cpp`) no longer
**throws** on an incompatible coordinate metric. It emits a **deprecation warning** and accepts the
2nd argument as a label only (pre-#1119 behaviour; calc-phase `GeoMeasure_PureFactor` yields the
raw measure, already m² for EPSG:28992). This let BAG20 compile again so t060 could be re-tested
green.
