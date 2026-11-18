**Pre-release.** GeoDms 20.29.0 contains all changes since 20.26.0, the latest full release, including those of the pre-releases 20.27.0 and 20.28.0. The main points are faster operations on detailed geometry, less memory for operators that cannot know the size of their result in advance, the new operators `fold` and `fold_c`, and Python wheels that need no setup. Please read the last section before upgrading: some changes can affect an existing configuration. The linked issues and wiki pages give the details.

| Flavour | Asset |
|---|---|
| `.m`: Windows, MSBuild | `GeoDms20.29.0.m-Setup-x64.exe` |
| `.c`: Windows, CMake | `GeoDms20.29.0.c-Setup-x64.exe` |
| `.g`: Windows, GLOBIO compatibility | `GeoDms20.29.0.g-Setup-x64.exe` |
| `.l`: Linux, Ubuntu 24.04 | `GeoDms20.29.0.l-linux-x64.deb`, `.tar.gz` (+ `.sha256`, `.sha256.p7s`) |
| Python wheels for 64-bit Windows | `geodms-20.29.0-cp312-cp312-win_amd64.whl`, `-cp313-cp313-`, `-cp314-cp314-` |

## Speed and memory

- `point_in_polygon` reads only the polygon edges near each point (#1320), and `connect` and `connect_info` skip the parts of long arcs that are far from a point (#1334), so neither walks every vertex of a detailed polygon or arc any more.
- `dms_buffer_multi_polygon` simplifies each ring before buffering, as `geos_buffer_multi_polygon` does: on detailed outlines, a buffer that took minutes now takes milliseconds (#1352, [dms_buffer_multi_polygon](https://github.com/ObjectVision/GeoDMS/wiki/dms_buffer_multi_polygon)).
- `impedance_matrix` with a sparse result runs its search once instead of twice (#1327, [Impedance options](https://github.com/ObjectVision/GeoDMS/wiki/Impedance-options)).
- `join_near_values`, `connect_matrix` and `dist_matrix` write their rows straight into the tiles of the result (#1327).
- `modus`, `weeded_counts`, the [classify functions](https://github.com/ObjectVision/GeoDMS/wiki/Classify-functions) and `unique` need about half the memory for their intermediate values, and `unique` of strings and arcs no longer copies its argument (#1327).
- `pareto_optimal` and `pareto_optimal_eps` need at most 16 bytes per row while sorting, so large calls that ran out of memory now fit (#1327).
- GeoParquet that GDAL did not write opens without a scan of all its geometries (#1328).

## New operators: fold and fold_c

- `fold(domain, step, initial state)` applies a function once for each element of a unit, each time to the state the previous step returned. It also works inside a function body, where `for_each` is not allowed (#1347, [Fold](https://github.com/ObjectVision/GeoDMS/wiki/Fold)).
- `fold_c(step, condition, maxN, initial state)` repeats a step while a condition on the state holds, and reports an error if it still holds after maxN steps (#1347, #1364, [Fold_c](https://github.com/ObjectVision/GeoDMS/wiki/Fold_c)).
- The step and the condition may be functions with variants; the variant is chosen once, from the initial state (#1367).
- `instantiate fold(...)` and `instantiate fold_c(...)` make each intermediate state an item: `trace/step0`, `trace/step1`, ... and `trace/result` (#1347).

## Functions

- A function result with sub-items can be passed straight to another function, as in `SumV(DropMax(x, Vals))` (#1347).
- In a function body, an item can refer to sub-items of other body items, including those of a copy made with `instantiate G(args)`, and a nested function can use the parameters of the function around it (#1365).
- A function defined in a template instance finds its own unit parameter, and can be passed as an argument whose signature uses a unit outside the template (#1341).
- When `for_each`, `loop` or `iterate` is refused in a function body, the message now says what to do instead (#1366, [Function-definition](https://github.com/ObjectVision/GeoDMS/wiki/Function-definition)).

## Reading and writing data

- GeoTIFFs with ZSTD compression can be read (#1332).
- `gdal.grid` no longer adds a sub-item GridData that fails (#1354).
- `gdalwrite.vect` no longer fails when a GDAL option parameter lies inside the storage (#1333).
- The export dialog offers GeoParquet, and one file per table for formats that hold a single table (#1330).
- A native CSV export of a table with a failing attribute no longer closes the GUI (#1340).
- A function signature that names a unit by its path keeps that spelling in the XML notation and in `@dumpconfig` output (#1351, [XML configuration syntax](https://github.com/ObjectVision/GeoDMS/wiki/XML-configuration-syntax)).
- A partial MMD write keeps the sub-items of a container that gets them from its calculation rule (#1350, [MMD](https://github.com/ObjectVision/GeoDMS/wiki/MMD)).

## Polygons, charts and views

- `^` accepts `fpoint` and `dpoint` polygons, and `bitxor` accepts `spoint` and `ipoint` polygons (#1345).
- The table-view aggregation `union_polygon` works again; it called an operator that does not exist (#1346).
- A chart line takes the colour of its series, so several series in one chart can be told apart (ObjectVision/NetworkModel_PBL#129).

## Python

- The `geodms` module comes as a wheel for Python 3.12, 3.13 and 3.14 on 64-bit Windows, with the engine inside, so it needs no setup (#1355, [Python bindings](https://github.com/ObjectVision/GeoDMS/wiki/Python-bindings)). For a pre-release the wheels are only on this page; a full release also publishes them on PyPI.
- The binding of a setup also works when its folder is only on `sys.path` (#1355).
- An item keeps its configuration loaded as long as Python holds it, which fixes a crash on the first read; `Config` has `close()` and works in a `with` block (#1370).

## Stability, examples and setups

- An occasional hang of GeoDmsRun 20.27.0 is addressed: a parked supplier is now also started on the meta thread (#1329, still open because the hang was never reproduced).
- `examples/grid_to_polygon.dms` runs on Linux, and an interrupted download of the CBS geopackage is removed, so the next run downloads it again (#1359, #1360).
- `examples/convex_hull.dms` uses `fold_c` (#1347).
- The setups install `license.txt` (GPL version 3) and the licence texts of all bundled libraries (#1371).

## Changes that can affect an existing configuration

- MMD data files have the extension `.mmddata` (#1349). Stores written by 20.28.0 or earlier are still read, with a deprecation warning, and converted when they are written again. A store written by 20.29.0 cannot be read by 20.28.0 or earlier, and GeoDms 21 will not read data files without the extension ([MMD](https://github.com/ObjectVision/GeoDMS/wiki/MMD)).
- A store whose dictionary a partial write of an earlier version emptied is not repaired by another partial write: write it once in full (#1350).
- `lookup(rlookup(a, a), c)` is no longer simplified to `c` (#1342), and `uint32(x / x)` no longer to `1` (#1343). Both rewrites gave wrong results for duplicates, zeros or nulls.
- An attribute with SyncMode None that reads a georeferenced raster with `gdal.grid`, `tif` or `bmp` is now placed through the georeference of the file, where it read every cell as null, and reports the same errors as the other modes (#1362, [SyncMode](https://github.com/ObjectVision/GeoDMS/wiki/SyncMode)).
- Reading a raster warns when only the domain or only the raster has a georeference (#1362, [Projection](https://github.com/ObjectVision/GeoDMS/wiki/Projection)).
- The `bp_` polygon set operations return polygons instead of arcs (#1348).
- `dms_buffer_multi_polygon` can give a slightly different result where an outline has dents or bumps shallower than a hundredth of the buffer distance (#1352).
- `fold` and `fold_c` are new operator names: rename any function or template of your own with these names (#1347).
- `exec_ec` sets the folder for the child process only. A folder that does not exist is now an error on Windows too, and on Linux the command line must start with the program name (#1361, [Exec_ec](https://github.com/ObjectVision/GeoDMS/wiki/Exec_ec)).
