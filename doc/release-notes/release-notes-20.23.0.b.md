**Pre-release.** GeoDms 20.23.0.b brings everything since **20.20.0**, the latest full release, and replaces the pre-release v20.23.0: the same two setups, with shorter notes. The builds 20.21.0 to 20.22.1 were interim builds and never published; where a wiki page says *Since GeoDMS 20.21.0*, *20.22.0* or *20.22.1*, this is the first release that has it. The details are in the issues and on the wiki pages linked below; the [full list of changes](https://github.com/ObjectVision/GeoDMS/blob/v20.23.0.b/doc/release-notes/release-notes-20.23.0.md) also gives what was measured.

| Flavour | Asset |
|---|---|
| `.m`: Windows, MSBuild | `GeoDms20.23.0.m-Setup-x64.exe` |
| `.c`: Windows, CMake | `GeoDms20.23.0.c-Setup-x64.exe` |
| `.g`: Windows, GLOBIO compatibility | not in this pre-release; follows in 20.23.1 |
| `.l`: Linux, Ubuntu 24.04 | not in this pre-release; follows in 20.23.1 |

## Before you upgrade

- **Memory and run times are back at 20.19.x.** The deferral of commits and IntegrityChecks that 20.20.0 introduced is withdrawn, and an item's [ExplicitSuppliers](https://github.com/ObjectVision/GeoDMS/wiki/ExplicitSuppliers) are updated one after another again. Plan on the 20.19.x figures, not on those of 20.20.0 (#1259).
- **Some results change; the old ones were wrong.** Among them `diversity`, `rounded_convert`, `corr`, `modus` per partition (#1285), the interaction results of `impedance_matrix`, the split polygon operators on a tiled domain (#1283), equally near candidates in `connect` and the other search operators (#1290), `box_connectivity` for boxes that only touch (#1291), the [non-zero classifications](https://github.com/ObjectVision/GeoDMS/wiki/Classify-functions), [Jenks-Fisher](https://github.com/ObjectVision/GeoDMS/wiki/ClassifyJenksFisher) breaks for values far from zero, and 64-bit integer columns of a [.dbf](https://github.com/ObjectVision/GeoDMS/wiki/Dbf). The wiki page of each operator says what changed.
- **Some configurations that ran now stop with an error**, with a message that names the cause: a missing `.mmd` store (#1272); an upper-case [literal](https://github.com/ObjectVision/GeoDMS/wiki/Literal) suffix such as `60D` (#1262); a region id outside its unit, or a node, zone or region unit whose range does not start at 0, in the [allocation](https://github.com/ObjectVision/GeoDMS/wiki/Function-discrete-alloc) and [network](https://github.com/ObjectVision/GeoDMS/wiki/Network-functions) operators; a simplify tolerance that is an attribute, negative or null; a rotated georeference in a [tif](https://github.com/ObjectVision/GeoDMS/wiki/GeoTiff) or world file; and counts and sums that overflow their type, which used to wrap.
- **Rewrite an `.mmd` store that 20.20.0 wrote below a `select_with_org_rel` or `select_with_attr_by_*` holder** (#1288, [MMD](https://github.com/ObjectVision/GeoDMS/wiki/MMD)).

## New

- **`pareto_optimal` and `pareto_optimal_eps`** select the non-dominated rows per group, and the pareto option of `impedance_matrix` takes a relative epsilon (#1281, #1282, #1286, #1287; [pareto_optimal](https://github.com/ObjectVision/GeoDMS/wiki/pareto_optimal), [pareto_optimal_eps](https://github.com/ObjectVision/GeoDMS/wiki/pareto_optimal_eps), [impedance options](https://github.com/ObjectVision/GeoDMS/wiki/Impedance-options)).
- **Pie Chart** in the View menu (#1273, [Main menu](https://github.com/ObjectVision/GeoDMS/wiki/Main-menu)), and a legend row for a layer drawn without a classification (#1274, [legend](https://github.com/ObjectVision/GeoDMS/wiki/Map-view-Legend)).
- **An `.mmd` dictionary carries** `Descr`, `Label`, `DialogType`, `DialogData` and `cdf` (#1275, [MMD](https://github.com/ObjectVision/GeoDMS/wiki/MMD)).
- **Python:** `update_metainfo()`, and a safe `fail_reason()` (#1279, [Python bindings](https://github.com/ObjectVision/GeoDMS/wiki/Python-bindings)).
- **`asExprList_with_null` and `asItemList_with_null`** write nulls; the plain forms leave them out ([asExprList](https://github.com/ObjectVision/GeoDMS/wiki/AsExprList)).
- **`examples\convex_hull.dms`**, the convex hull script of the wiki.

## Faster

- The `dms_*` polygon operators, several times (#1283); the spatial index of the search operators (#1289); reading a `.dbf`, about 15 times; and the overlay, split, buffer and connectivity operators of the polygon families, `griddist`, `merge`, `convert`, `cumulate`, `regex_search` and the interaction results of `impedance_matrix`, which run in parallel or do less per element.

## Fixed

- **Storages:** a `for_each` over a `gdal.vect` table that needs a second column of it, broken since 20.20.0 (#1284); the `odbc` storage, which failed every read since 20.20.0; the `bmp` storage; [gdal.vect](https://github.com/ObjectVision/GeoDMS/wiki/Gdal.vect) on mixed geometry layers, and its memory leaks; NULL fields in a `.dbf`; writing a MultiPoint attribute; shp, dbf and tif writes that failed without an error.
- **Operators:** the findings of a code audit, each on its operator's wiki page: wrong results, reads beyond the end of an array in Release builds, `diversity` running forever on a large radius, and a spatial index that split a node without end until memory ran out (#1289).
- **GUI:** pasting cells into a grid layer, a WMS server that stops answering, table actions on rows that are still being computed, the *Calculation times* window after a reload, and wide dashed pens, drawn solid since 20.0.0.
- **Python:** crashes in `fail_reason()`, `asDataItem()`, `asUnitItem()` and `set_values_from_*_list` (#1279).
