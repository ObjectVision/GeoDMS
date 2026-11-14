*Draft, started 2026-11-11 and kept up to date until the setups are built. Whether 20.29.0 is a pre-release or a full release is still to be decided.* GeoDms 20.29.0 holds the changes since 20.28.0. The details are in the issues linked below.

| Flavour | Asset |
|---|---|
| `.m`: Windows, MSBuild | `GeoDms20.29.0.m-Setup-x64.exe` |
| `.c`: Windows, CMake | `GeoDms20.29.0.c-Setup-x64.exe` |
| `.g`: Windows, GLOBIO compatibility | `GeoDms20.29.0.g-Setup-x64.exe` |
| `.l`: Linux, Ubuntu 24.04 | `GeoDms20.29.0.l-linux-x64.deb`, `.tar.gz` (+ `.sha256`, `.sha256.p7s`) |

## Changes that can affect an existing configuration

- The data files of an MMD store have the extension `.mmddata`: the attribute `dom/w` is the file `dom\w.mmddata`, a string's values `dom\w.mmddata.seq` (#1349). An item that changes between attribute and container no longer makes the next write fail or stall. A store written by 20.28.0 or earlier, with data files without extension, is still read, with a deprecation warning, and is converted attribute by attribute when it is written again; GeoDms 21 will not read data files without extension. A store that 20.29.0 has written cannot be read by 20.28.0 or earlier. See [MMD](https://github.com/ObjectVision/GeoDMS/wiki/MMD).
- `lookup(rlookup(a, a), c)` is no longer simplified to `c`, which was wrong where `a` has duplicate or null values (#1342), and `uint32(x / x)` is no longer simplified to `1`, which was wrong where `x` is 0 or null and gave a parameter where `x` is an attribute (#1343). Configurations that relied on these rewrites now get the result that the expression defines.
- The `bp_` polygon set operations give polygons, not arcs, so a result declared as polygon no longer warns (#1348).
- `fold` and `fold_c` are new operators (#1347): a configuration that defines its own function or template with one of these names must rename it.

## Speed

- `point_in_polygon` reads only the edges near the row of each point (#1320), where it walked every vertex of each candidate polygon: before this change, 10 million points over 400 polygons of 10 001 vertices each took 10 s, against 0.27 s with 5 vertices each.
- `connect` and `connect_info` pass over the parts of a long arc that are far from a point, and take the candidate arcs nearest first (#1334), where they walked every segment: before this change, 1 million points against 400 rings of 10 001 vertices took 17 s, against 0.06 s with 5 vertices.
- GeoParquet that GDAL did not write is opened without a scan of all its geometries (#1328).

## Reading and writing data

- GeoTIFFs with ZSTD compression can be read: libtiff is built with that codec (#1332).
- An attribute read with `gdal.grid` no longer gets a sub-item GridData that fails with "CreateHeapTileArray cannot create a DataArray for an unspecified domain"; the attribute is the grid data itself, as with `tif` (#1354).
- `gdalwrite.vect` no longer fails when a GDAL option parameter lies inside the storage; such items are not written to it (#1333).
- The export dialog offers GeoParquet for an item with geometry, and one file per table where a format holds one table (#1330).
- A native CSV export of a table with a failing attribute no longer ends the GUI (#1340).

## Operators and views

- `^` takes `fpoint` and `dpoint` polygons, and `bitxor` takes `spoint` and `ipoint` polygons (#1345).
- The table-view aggregation `union_polygon` uses `dms_union_polygon`; it called an operator that the engine does not have (#1346).
- The palette editor says Logarithmic Intervals.
- `fold(domain, step, initial state)` applies a function once for each element of a unit, each time to the state that the previous application returned, also inside a function body, where `for_each` is refused (#1347). See [Fold](https://github.com/ObjectVision/GeoDMS/wiki/Fold).
- `fold_c(step, condition, maxN, initial state)` applies a step while a condition on the state holds, and reports an error when the condition still holds after maxN steps (#1347). See [Fold_c](https://github.com/ObjectVision/GeoDMS/wiki/Fold_c).
- `container trace := instantiate fold(...)`, or `instantiate fold_c(...)`, makes the state after each step an item, `trace/step0`, `trace/step1`, ..., and `trace/result`; in the body of a function the result can refer to them, also when the function is applied as a value (#1347).
- A function result with declared sub-items can be passed straight on as an argument of another function, as in `SumV(DropMax(x, Vals))`; this failed with "member access through parameter ... requires the corresponding argument to be a direct item reference" (#1347).
- A function defined inside a template instance finds its own unit parameter instead of a unit of the same name in the container around it, which failed with "the body pins unit variable ... to a specific unit" or with "relative pathname ascended above root". Such a function also passes as the argument of a parameter whose type is a function signature over a unit outside the template, which failed with "the domain of parameter ... does not match the domain relationship required by signature" (#1341).

## Stability

- `OperationContext::Join` activates a parked supplier on the meta thread too, which addresses the occasional hang of GeoDmsRun 20.27.0 (#1329, still open: the hang was not reproduced).

## Examples and library

- `examples/grid_to_polygon.dms` runs on Linux: two of its includes named a library file in the wrong case, and `library/basedata_nl/RegioIndelingen.dms` downloads the CBS geopackage with `curl` instead of through cmd.exe (#1359).
- `examples/convex_hull.dms` repeats its step with `fold_c` in a function `ConvexHull` until the step drops no more points, instead of with `for_each_ne` and an indirect expression a fixed number of times (#1347).
