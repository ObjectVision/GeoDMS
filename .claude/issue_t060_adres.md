# Title

BAG snapshot t060: 407 301 of 1 138 872 `adres` rows lose geometry — secondary-address fallback broken since `polygon_i4HV` → `geos_buffer_multi_polygon` migration

# Body

## Summary

Regression test **t060** (`Storage_BAGSnapshot_Utrecht_GeoPackage_Compare`) generates `snapshot_Utrecht_20210701.gpkg`. After the v19→v20 migration of `polygon_i4HV(geom, sz)` → `geos_buffer_multi_polygon(geom, sz, 16b)` (boost::polygon Minkowski → GEOS), **35.76 % of rows in the `adres` layer have collapsed to coordinates `(-0.001, -0.001)`** instead of receiving geometry from their backing entity (vbo / ligplaats / standplaats / nevenadres).

Affected: **407 301 / 1 138 872** `adres` rows — exactly the secondary-addresses without a directly-matching verblijfsobject (`vbo` count = 731 571 = the OK count).

## Diagnostic evidence

`ogrinfo -al -so` on both gpkg files shows all 21 layers match in name / geometry-type / feature-count, and all extents match **except** `adres`:

| | extent |
|---|---|
| 19.5 reference | `(114249.97, 430007.16) - (171423.38, 479558.84)` |
| 20.0.0.c       | `(-0.001, -0.001) - (171423.38, 479558.84)` |

Drilling into the generated `adres` layer with osgeo.ogr:

```
total: 1 138 872
bad (at ~origin): 407 301    (35.76 %)
ok:               731 571    (64.24 %)
```

**Perfect correlation** — every bad row has `woonplaatsNaam = NULL`, every ok row has a value (Utrecht / Amersfoort / Nieuwegein / Veenendaal / Zeist …).

Sample bad rows (FID 1–10, all real BAG-registered addresses on Atalanta street, Zeewolde — postcode 3892EJ/ED/EE; should be at RD ≈ (190 k, 482 k)):

```
FID=1  (-0.001, -0.001)  nummeraanduiding_id=0050200000357225  Atalanta 118  3892EJ
FID=2  (-0.001, -0.001)  nummeraanduiding_id=0050200000357227  Atalanta 119  3892ED
FID=3  (-0.001, -0.001)  nummeraanduiding_id=0050200000357229  Atalanta 120  3892EJ
…
```

The `-0.001` value strongly suggests a default-constructed point that survived the `point_xy(0d, 0d)[rdc_meter]` rounding through the `rdc_mm` fixed-point grid.

## Hypothesis

The `polygon_i4HV` → `geos_buffer_multi_polygon` migration is correct for the primary-vbo path (geometry resolves cleanly there: 731 571 OK rows = vbo count). But the secondary-address fallback chain — `nummeraanduiding` → (no vbo) → inherit from ligplaats / standplaats / nevenadres — relied on the boost::polygon path's ipoint-typed multipolygon flowing through the join. The new GEOS-based dpoint path doesn't propagate via the same fallback, and the join produces a default-zero geometry instead of either inheriting or erroring.

This is also adjacent to t301's `Cannot find Item Values` on `BAG_MakeSnapshot/.../functioneel_pand/NeighbourCount`, which exhibits the same lookup-failure-without-error pattern.

## Repro

1. Build cmake-Release 20.0.0 (or the equivalent msbuild flavor) with the v19→v20 cfg migrations applied.
2. Run regression: `python full.py -version local` (or `local-msbuild-release`).
3. Inspect `C:\LocalData\Regression\BAG20\snapshot_Utrecht_20210701.gpkg` :
   ```python
   from osgeo import ogr
   ds = ogr.Open(path); lyr = ds.GetLayerByName("adres")
   bad = sum(1 for f in lyr if abs(f.GetGeometryRef().GetX()) < 100 and abs(f.GetGeometryRef().GetY()) < 100)
   # ~407,301 bad of 1,138,872 expected
   ```

## Affected configurations / code

- Test: `t060_Storage_BAGSnapshot_Utrecht_GeoPackage_Compare` in `tst/batch/full.py`.
- Cfg with the polygon_i4HV call site (now migrated): `prj_snapshots/BAG20/cfg/BAG20_MakeSnaphot/MakeSnapshot{,_gpkg}/afleidingen.dms` line 126:
  ```dms
  attribute<geometries/rdc_mm> i4HV_mm (domain, polygon)
      := geos_buffer_multi_polygon(domain/geometry_mm, inflate_size, 16b);
  ```
- Migration script that performed the rename (one-liner, no other changes): `tst migration commit 4aa30ab — v19→v20 migration: operator renames + multi-build-target runner`.

## Suggested fixes (in increasing scope)

1. Make the fallback chain in BAG20 cfg explicit: use `coalesce` (or DMS-equivalent) so `adres` rows without a `vbo` join take geometry from `nummeraanduiding` → `ligplaats`/`standplaats` directly, not from a Minkowski-buffered side-product.
2. Review `geos_buffer_multi_polygon`'s contract for "no-op / pass-through" cases — `polygon_i4HV(0d)` ≡ identity, so the migrated `geos_buffer_multi_polygon(g, 0, 16b)` should equally be identity. If GEOS empties the geometry on zero-buffer or NULL input, it should propagate NULL rather than a degenerate origin point.
3. Add a check at the FSS/GPKG writer that errors on geometries whose extent is more than N standard deviations away from the parent layer's bbox (would have caught this regression in CI immediately).
