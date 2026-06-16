# CRS / metric decoupling for coordinate units (#1119 follow-up)

Status: **design captured, not yet implemented.** Interim mitigation in place (see bottom).
Origin: t060 (BAG20 GeoPackage snapshot) regressed to `data error / exit 1` on 20.2.0.m.

## Symptom

`t060` (`C:/dev/tst/Projects/BAG20/cfg/BAG20_MakeSnaphot.dms`) fails at **config-compile
time** (0:00:00, 0.00 GB, exit 1). The fatal abort is `Cannot find Item selectie/org_rel`,
but that is a *cascade*. The real first error (log line ~54) is:

```
Operator area Error: the result unit (m²) is not compatible with the coordinate metric ^2 of <blank>
```

Dependency chain that turns one `area` error into a total compile failure:

1. `MakeSnapshot.dms:240` — `oppervlakte := area(src/geometry, m2)`  ← errors here
2. `:257` `geen_zeer_grote_panden_in_NL` depends on `oppervlakte`
3. `:267-269` `overig_filter/in_selectie` depends on that filter
4. `:274` passed into `datum_selectie(src, prik_datum, True, overig_filter/in_selectie)`
5. `BAG20_MakeSnaphot.dms:257` — that filter is `&&`-ed into `select_with_org_rel(...)`, which
   therefore can't build, so its auto-generated `org_rel` sub-item never exists
6. every `domain/…[selectie/org_rel]` downstream → `Cannot find Item selectie/org_rel` → exit 1

The passing 20.1.0.m run of the *same config* has none of these errors → genuine binary
regression, introduced by **#1119** (`5949df20`, "area/arc_length: convert result to the
requested unit; add unary auto-derive form").

## Root cause

Before #1119 the 2nd argument of `area(geom, m2)` was **a label only**. #1119 made it a real
result unit and added dimensional validation in `GeoMeasure_ValidateAndWarn`
(`geo/dll/src/OperPolygon.cpp`).

The BAG20 coordinate unit is:

```
// BAG20_MakeSnaphot.dms:119
unit<fpoint> rdc_base : SpatialReference = "EPSG:28992", DialogData = "wmts_layer";
unit<fpoint> rdc    := range(rdc_base, point_yx(300000f,0f), point_yx(625000f,280000f));
```

`rdc` has **no linear metric**. Instead, `UnitBase<V>::GetKeyExprImpl` (`tic/dll/src/Unit.cpp:131-146`)
synthesizes the unit's metric as a single base-unit whose symbol is
`"<SpatialReference>\xFF<DialogData>"`, i.e. `"EPSG:28992\xFFwmts_layer"`. So the unit's
**metric is a CRS identity tag, not a dimension.** The detail page confirms it: the derived
values unit of `area(src/geometry)` shows Metric `EPSG:28992·wmts_layer²`.

#1119's validation squares that tag (`(EPSG:28992·wmts_layer)²`) and compares it to `m²` →
mismatch → throws.

**Why the `0xFF` guard doesn't help here.** `GeoMeasure_GetCoordMetric` has a guard
(`OperPolygon.cpp:252-257`) that skips metrics carrying a `0xFF` CRS separator — but it lives in
the **metric branch**. BAG20's `src/geometry` coordinate unit is **projection-bearing** (a grid/mm
unit projecting onto world `rdc`), so its *own* metric is empty (units never carry both a metric
and a projection) and `GeoMeasure_GetCoordMetric` takes the **projection branch**
(`OperPolygon.cpp:262-280`): `L = compositeBase->GetCurrMetric()` = `rdc`'s CRS tag, returned
**unguarded**. That tag `L` is what gets squared. (Verified at runtime: the warning prints
`coordinate metric (EPSG:28992<0xFF>wmts_layer)^2`; `coordUnit`'s own `GetCurrMetricStr()` is
empty because it is projection-bearing — the squared base-units come from the composite base, and
the `^2` is `nrDims`, not part of the metric.) The decoupling fix handles this for free: once the
composite base is a real `m` unit, the projection branch yields `m` and `area` → `m²`.

## Key architectural fact

`tic/dll/src/UnitCreators.cpp:141` & `:201`:

```cpp
assert(IsEmpty(a1MetricPtr) || !a1ProjectionPtr); // units never have both a metric and a projection
```

A coordinate unit is **either** metric-bearing **or** projection-bearing. Today a world CRS
unit (`rdc`) is metric-bearing and that metric *is* the CRS tag — which is what forces the
conflation. The fix is to make CRS units **projection-bearing over a real `m` base**.

## Why not just delete the `SR\xFF DD` metric encoding

It is load-bearing. Three accessors decode it (each as a *fallback*, after their primary
per-item channel):

| Accessor | Primary source | Metric `0xFF` fallback |
|---|---|---|
| `GetSpatialReference()` / `GetCurrSpatialReference()` (`AbstrUnit.cpp:393`,`:409`) | `USF_HasSpatialReference` flag + `s_SpatialReferenceAssoc` | part before `0xFF` |
| `GetBackgroundReference()` (`AbstrUnit.cpp:376`) | `DialogData` property | part after `0xFF` |

The per-item channel propagates on unit copy/alias (`AbstrUnit::CopyProps` → `SetSpatialReference`,
`AbstrUnit.cpp:674`) and on GDAL vector read (`gdal_vect.cpp:2743`). The **metric** fallback is
the *only* carrier when the coordinate values unit is a fresh **cache unit produced by
unification inside the calc graph** (those inherit the metric but not the flag/DialogData) — e.g.
the coordinate unit of `union_data(., a, b)` over two `rdc` geometries, or an inline
`BaseUnit('EPSG:28992', fpoint)`. Viewing such a computed layer relies on the metric to recover
the `wmts_layer` topo background (`shv/dll/src/GraphDataView.cpp:151`) and the CRS (`:432`).

The CRS-in-metric also provides **unification distinctness**: two coordinate units in different
CRS have different metrics and won't silently unify. A plain `m` metric would collapse
EPSG:28992 and EPSG:3857 (both metres) to the same dimension. So distinctness must move to the
projection's SR field.

## Chosen design (preferred by author)

Let `UnitProjection::GetBaseUnit()` return a real `m` unit, and add specific functions for
SpatialReference and DialogData on the projection; fold SR into projection unification when new
point units are derived.

A CRS coordinate unit becomes **projection-bearing**: an identity `UnitProjection` whose base is
a canonical `m`-metric unit, carrying SR + DD as projection fields. `gridset` units chain onto
it, so `GetCompositeBase()` bottoms out at `m` → `area` yields `m²` automatically.

### Touch points
1. **`UnitProjection`** (`tic/dll/src/Projection.h`, `tic/dll/src/Metric.cpp:361`) — add
   `m_SpatialRef` (TokenID) + `m_BackgroundRef` (TokenID) + accessors; `GetBaseUnit()` returns
   the `m` unit. `GetUnitlabeledScalePair` (`Metric.cpp:420`) already returns `("metre",1)` for
   EPSG:28992 via the GDAL hook (`stg/dll/src/gdal/gdal_base.cpp:357`).
2. **Projection unification** — extend `AreEqual(UnitProjection*)` (`Metric.cpp:395`) to also
   compare `m_SpatialRef` (+ DD). Without this, different-CRS units both over `m` would wrongly
   unify. `compatible_values_unit_creator_func` (`UnitCreators.cpp:189-200`) then propagates with
   no further change — this is the "adapt unification when new point units are derived" piece.
3. **Config parse** — when a point unit gets `SpatialReference=` / `DialogData=`, build the
   identity projection over `m` instead of the metric tag. Redirect `SetSpatialReference`
   (`AbstrUnit.cpp:367`) and stop the `SR\xFF DD` synthesis in `GetKeyExprImpl`
   (`Unit.cpp:131-146`).
4. **Accessors** — `GetSpatialReference/GetCurrSpatialReference/GetBackgroundReference`
   (`AbstrUnit.cpp:376-424`) read from the projection chain; drop the `0xFF`-metric fallbacks.
   Existing consumers (`GraphDataView.cpp:151/432`, `OperConv.h:294`, `gdal_base`, `GridDist`)
   already go through these accessors, so redirecting the accessors keeps them working.
5. **`area`/`arc_length`** (`OperPolygon.cpp:234-339`) — projection branch already does
   `base->GetCurrMetric()`; with base = `m` it yields `m²` automatically. **Delete the `0xFF`
   guard (252-257)**; the original #1119 throw then only fires for genuinely non-metric coords.
6. **GDAL read** — `gdal_vect.cpp:2743` already calls `SetSpatialReference(wkt)`; route it into
   the new projection construction.

### Open decisions (need author confirmation before coding)
1. **Canonical `m` base unit.** Introduce one shared internal `BaseUnit('m', …)` that all
   metre-CRS projections point to (dimensions unify for `area`; CRS distinctness via projection
   SR). Non-metre CRS (US-feet) use the `GetUnitlabeledScalePair` scale as the projection factor
   while keeping base `m`. — OR make the base symbol the OGR linear-unit name (`metre`/`US survey
   foot`). *Recommendation: shared `m`.*
2. **Scope.** Only point value-types (`fpoint`/`dpoint`/`ipoint`) become projection-bearing-over-`m`;
   leave scalar units alone. *Recommendation: scope to point value-types.*
3. **Back-compat for stored units.** Keep the `0xFF` *parse* (read-only migration shim) for one
   version so old `.fss`/archives still display their background, while removing its *production*.
   — OR hard-cut both. *Recommendation: keep parse-only shim for one version.*

## Interim mitigation (IMPLEMENTED)

Until the above lands, `GeoMeasure_ValidateAndWarn` (`OperPolygon.cpp`) no longer **throws** on
an incompatible coordinate metric. It emits a **deprecation warning** and accepts the 2nd
argument as a label only (the pre-#1119 behavior; calc-phase `GeoMeasure_PureFactor` yields the
raw measure, which is already m² for EPSG:28992). This lets configs like BAG20 compile again so
t060 can be re-tested green, deferring the proper decoupling.
