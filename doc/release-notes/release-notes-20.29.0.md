**Pre-release.** GeoDms 20.29.0 contains all changes since 20.26.0, the latest full release, including those of the pre-releases 20.27.0 and 20.28.0. The main points are faster operations on detailed geometry, less memory for operators that cannot know the size of their result in advance, the new operators `fold` and `fold_c`, and Python wheels that need no setup. Please read the last section before upgrading: some changes can affect an existing configuration. The linked issues and wiki pages give the details.

| Flavour | Asset |
|---|---|
| `.m`: Windows, MSBuild | `GeoDms20.29.0.m-Setup-x64.exe` |
| `.c`: Windows, CMake | `GeoDms20.29.0.c-Setup-x64.exe` |
| `.g`: Windows, GLOBIO compatibility | `GeoDms20.29.0.g-Setup-x64.exe` |
| `.l`: Linux, Ubuntu 24.04 | `GeoDms20.29.0.l-linux-x64.deb`, `.tar.gz` (+ `.sha256`, `.sha256.p7s`) |
| Python wheels for 64-bit Windows | `geodms-20.29.0-cp312-cp312-win_amd64.whl`, `-cp313-cp313-`, `-cp314-cp314-` |

## Speed and memory

- `point_in_polygon` reads only the polygon edges near the row of each point (#1320), and `connect` and `connect_info` skip the parts of long arcs that are far from a point (#1334). Both help most on polygons and arcs with many vertices.
- `dms_buffer_multi_polygon` simplifies each ring before buffering, as `geos_buffer_multi_polygon` does. On detailed outlines, a buffer that took minutes now takes milliseconds (#1352, [dms_buffer_multi_polygon](https://github.com/ObjectVision/GeoDMS/wiki/dms_buffer_multi_polygon)).
- `impedance_matrix` with a sparse result, `join_near_values`, `connect_matrix` and `dist_matrix` write their result once, straight into its tiles, instead of counting first or collecting and copying the rows (#1327). `impedance_matrix` therefore runs its search once instead of twice; `precalculated_NrDstZones` no longer saves time, but still fixes the number of rows per origin zone ([Impedance options](https://github.com/ObjectVision/GeoDMS/wiki/Impedance-options)).
- With many distinct values, `modus` and `unique` need about half the memory for their intermediate values; `weeded_counts` and the [classify functions](https://github.com/ObjectVision/GeoDMS/wiki/Classify-functions) use the same approach. `unique` of strings and arcs no longer copies its argument (#1327).
- `pareto_optimal` and `pareto_optimal_eps` need at most 16 bytes per row while sorting, so large calls that ran out of memory now fit (#1327).
- GeoParquet in the layout that geopandas writes opens without a scan of all its geometries (#1328).

## New operators: fold and fold_c

- `fold(domain, step, initial state)` applies a function once for each element of a unit, each time to the state that the previous step returned. It also works inside a function body, where `for_each` is not allowed (#1347, [Fold](https://github.com/ObjectVision/GeoDMS/wiki/Fold)).
- `fold_c(step, condition, maxN, initial state)` repeats a step while a condition on the state holds, and reports an error if it still holds after maxN steps. Because the number of steps depends on the data, a `fold_c` item already calculates its steps when GeoDMS determines its metadata, and calculates them only once (#1347, #1364, [Fold_c](https://github.com/ObjectVision/GeoDMS/wiki/Fold_c)).
- The step and the condition may be a [generic function](https://github.com/ObjectVision/GeoDMS/wiki/Generic-function) with several variants; the variant is chosen once, for the initial state (#1367).
- `container trace := instantiate fold(...)`, or `instantiate fold_c(...)`, makes each intermediate state an item of `trace`: `trace/step0`, `trace/step1`, ... and `trace/result` (#1347).

## Functions

- A function that returns a container with sub-items can be passed straight to another function, as in `f(g(x))` (#1347).
- In a function body, an item can refer to sub-items of other body items, including those of a copy made with `instantiate G(args)`, and a nested function can use the parameters of the function around it. A misspelt sub-item now names the body item and the function (#1365).
- A function defined in a template instance no longer fails when it has a unit parameter with the same name as a unit around it, or when it is passed as an argument whose signature uses a unit outside the template (#1341).
- When `for_each`, `loop` or `iterate` is refused in a function body, the message now says what to do instead (#1366, [Function-definition](https://github.com/ObjectVision/GeoDMS/wiki/Function-definition)).

## Reading and writing data

- GeoTIFFs with ZSTD compression can be read (#1332).
- An attribute read with `gdal.grid` no longer gets a sub-item GridData that fails (#1354).
- `gdalwrite.vect` no longer fails when a GDAL option parameter is defined inside the container it writes (#1333).
- The export dialog offers GeoParquet for an item with geometry, and one file per table for formats that hold a single table (#1330).
- A native CSV export of a table with a failing attribute no longer crashes the GUI (#1340).
- An item that changes between attribute and container no longer makes the next MMD write fail or stall (#1349).
- A function signature that names a unit by its path keeps that spelling in the XML notation and in `@dumpconfig` output (#1351, [XML configuration syntax](https://github.com/ObjectVision/GeoDMS/wiki/XML-configuration-syntax)).

## Polygons, charts and views

- `^` accepts `fpoint` and `dpoint` polygons, and `bitxor` accepts `spoint` and `ipoint` polygons (#1345).
- The table-view aggregation `union_polygon` works again; it called an operator that does not exist (#1346).
- A chart line takes the colour of its series, so several series in one chart can be told apart (ObjectVision/NetworkModel_PBL#129).

## Python

- The `geodms` module comes as a wheel for Python 3.12, 3.13 and 3.14 on 64-bit Windows, with the engine inside, so it needs no setup: `pip install` the wheel file for your Python version (#1355, [Python bindings](https://github.com/ObjectVision/GeoDMS/wiki/Python-bindings)). For a pre-release the wheels are only on this page; a full release also publishes them on PyPI.
- The Python module of a GeoDMS setup can now be imported when its folder is only on `sys.path`; before, the folder also had to be on PATH (#1355).
- An item keeps its configuration loaded as long as Python holds it, which fixes a crash on the first read. `Config` has `close()` and works in a `with` block (#1370).

## Stability, examples and setups

- GeoDmsRun 20.27.0 could occasionally hang. A likely cause is fixed, but the hang was never reproduced, so #1329 stays open: please report it if you still see it.
- `examples/grid_to_polygon.dms` runs on Linux, and an interrupted download of the CBS geopackage is removed, so the next run downloads it again (#1359, #1360).
- `examples/convex_hull.dms` uses `fold_c` (#1347).
- The setups install `license.txt` (GPL version 3) and the licence texts of all bundled libraries (#1371).

## Changes that can affect an existing configuration

- MMD data files have the extension `.mmddata` (#1349). Stores written by 20.28.0 or earlier are still read, with a deprecation warning, and converted when they are written again. A store written by 20.29.0 cannot be read by 20.28.0 or earlier, and GeoDms 21 will not read data files without the extension ([MMD](https://github.com/ObjectVision/GeoDMS/wiki/MMD)).
- A partial MMD write keeps the sub-items of a container that gets them from its calculation rule, such as a template instance (#1350). Before, the store described such a container as empty, so a reader no longer found its sub-items. A store damaged this way by an earlier version is not repaired by another partial write: write the whole store once.
- GeoDMS no longer rewrites some expressions into a shorter form that was wrong for some values, so these now give correct, and possibly different, results (#1342, #1343): `lookup(rlookup(a, a), c)` and `rjoin(a, a, c)` are no longer replaced by `c`, `uint32(x / x)` no longer by `1`, and `sum(x, id(E))` no longer by `x`, so a group whose only value is null now sums to 0 instead of null. `a * (1.0 / b)` with an integer or `float32` b now fails, as `(1.0 / b) * a` already did.
- `connect`, `connect_info` and `dist_info`, with their `_eq` and `_ne` forms, can pick another segment when several are equally near, so SegmID, CutPoint, InArc and InSegm can differ for such ties; that choice is unspecified ([Deterministic and unspecified results](https://github.com/ObjectVision/GeoDMS/wiki/Deterministic-and-unspecified-results), #1334).
- An attribute with SyncMode None that reads a georeferenced raster with `gdal.grid`, `tif` or `bmp` used to read every cell as null. It is now positioned using the georeference of the file, so its values change, and it reports the same errors as the other modes (#1362, [SyncMode](https://github.com/ObjectVision/GeoDMS/wiki/SyncMode)).
- Reading a raster now warns when only the domain or only the raster has a georeference (#1362, [Projection](https://github.com/ObjectVision/GeoDMS/wiki/Projection)).
- `bp_intersect`, `bp_union`, `bp_xor` and `bp_difference`, and `*`, `+`, `-`, `^`, `bitand`, `bitor` and `bitxor` on `spoint` and `ipoint` polygons, return polygons instead of arcs, so a result declared as polygon no longer warns (#1348).
- `dms_buffer_multi_polygon` can give a slightly different result where an outline has dents or bumps shallower than a hundredth of the buffer distance (#1352).
- For GeoParquet in the layout that geopandas writes, a declared geometry is no longer checked against the file, and the sub-items `geometry_z` and `geometry_m` are no longer added (#1328).
- `fold` and `fold_c` are new operator names: rename any function or template of your own with these names (#1347).
- `exec_ec` now sets the folder for the child process only; before, the whole GeoDMS process moved to that folder while the child ran. A folder that does not exist is now an error on Windows too. On Linux the command line must start with the program name, a program without a path is looked for on the PATH, and only programs named `cmd`, `sh`, `bash` or `dash` run through a shell (#1361, [Exec_ec](https://github.com/ObjectVision/GeoDMS/wiki/Exec_ec)).
