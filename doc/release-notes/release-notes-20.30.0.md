**Pre-release.** GeoDms 20.30.0 contains all changes since 20.26.0, the latest full release, including those of the pre-releases 20.27.0, 20.28.0 and 20.29.0. What is new since 20.29.0 is marked *(new in 20.30)*: Python wheels for Linux and for the GLOBIO 4 environment, a configuration dump that writes a `using` as it was configured, and fixes in how GeoDmsGuiQt reads its command line and reports a configuration that fails to load. The main points since 20.26.0 are faster operations on detailed geometry, less memory for operators that cannot know the size of their result in advance, the new operators `fold` and `fold_c`, and Python wheels with every release. Please read the last section before upgrading: some changes can affect an existing configuration. The linked issues and wiki pages give the details.

| Flavour | Asset |
|---|---|
| `.m`: Windows, MSBuild | `GeoDms20.30.0.m-Setup-x64.exe` |
| `.c`: Windows, CMake | `GeoDms20.30.0.c-Setup-x64.exe` |
| `.g`: Windows, GLOBIO compatibility | `GeoDms20.30.0.g-Setup-x64.exe` |
| `.l`: Linux, Ubuntu 24.04 | `GeoDms20.30.0.l-linux-x64.deb`, `.tar.gz` (+ `.sha256`, `.sha256.p7s`) |
| Python 3.12, 3.13 and 3.14 wheels, 64-bit Windows | `geodms-20.30.0-cp312-cp312-win_amd64.whl`, `-cp313-cp313-`, `-cp314-cp314-` |
| Python 3.12, 3.13 and 3.14 wheels, 64-bit Linux *(new in 20.30)* | `geodms-20.30.0-cp312-cp312-manylinux_2_39_x86_64.whl`, `-cp313-cp313-`, `-cp314-cp314-` |
| Python 3.9 wheel for the GLOBIO 4 environment, 64-bit Windows *(new in 20.30)* | `geodms_globio-20.30.0-cp39-cp39-win_amd64.whl` |

## Speed and memory

- `point_in_polygon` reads only the polygon edges near the y-coordinate of each point (#1320), and `connect` and `connect_info` skip the parts of long arcs that are far from a point (#1334). Both help most on polygons and arcs with many vertices.
- `dms_buffer_multi_polygon` simplifies each ring before buffering, as `geos_buffer_multi_polygon` does. On detailed outlines, a buffer that took minutes now takes milliseconds (#1352, [dms_buffer_multi_polygon](https://github.com/ObjectVision/GeoDMS/wiki/dms_buffer_multi_polygon)).
- `impedance_matrix` with a sparse result, `join_near_values`, `connect_matrix` and `dist_matrix` write their result once, straight into its tiles, instead of counting first or collecting and copying the rows (#1327). `impedance_matrix` therefore runs its search once instead of twice; `precalculated_NrDstZones` no longer saves time, but still sets the number of rows per origin zone ([Impedance options](https://github.com/ObjectVision/GeoDMS/wiki/Impedance-options)).
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
- An item of an MMD store that changes from attribute to container under the same name, or back, no longer makes the next write fail or stall (#1349).
- A function signature that names a unit by its path keeps that spelling in the XML notation and in `@dumpconfig` output (#1351, [XML configuration syntax](https://github.com/ObjectVision/GeoDMS/wiki/XML-configuration-syntax)).
- *(new in 20.30)* `@dumpconfig` writes a `using` as it was configured: `using = "geometries"` stays `using = "geometries"`, where it used to come out as `"../geometries"`, or as `".../geometries"` one level deeper. `PropValue(x, 'Using')` still gives the resolved path (#1377).
- *(new in 20.30)* When fewer bytes are read or written than asked, in a `str` or `strfiles` storage, a world file or the header of a dbf, the error names the file, how many bytes were asked for and how many went through, and the reason the system gives, where it used to be a GeoDMS internal error. A failure when a written file is closed is reported too, instead of leaving a shortened file without an error, and "Failure during Writing" now gives the reason (#1393).
- *(new in 20.30)* A line of a message in the log file, the console and the event log of the GUI is cut after 1000 bytes, where it was cut after 256, so that a long item path or file name stays whole, and the cut no longer falls inside a character that takes more than one byte in UTF-8 (#1393, #1376).
- *(new in 20.30)* A calculated item whose file exists and cannot be written, such as a file with the read-only attribute, fails with a message that says so and names the file. It used to fail with "Item has both a Calculation Rule and a read-only storage spec", which now only an item under `StorageReadOnly = "True"` or with a storage type that can only read gets (#1401, [StorageReadOnly](https://github.com/ObjectVision/GeoDMS/wiki/StorageReadOnly)).

## Polygons, charts and views

- `^` accepts `fpoint` and `dpoint` polygons, and `bitxor` accepts `spoint` and `ipoint` polygons (#1345).
- The table-view aggregation `union_polygon` works again; it called an operator that does not exist (#1346).
- A chart line takes the colour of its series, so several series in one chart can be told apart (ObjectVision/NetworkModel_PBL#129).

## GUI command line and test scripts

- *(new in 20.30)* A configuration given by a relative path is now looked for relative to the folder GeoDmsGuiQt was started in. Before, the GUI first made the folder of the configuration the current folder and then looked for the relative path from there, so a name such as `cfg/model.dms` failed to load; on Linux a bare file name failed too (#1379, [Command-line options](https://github.com/ObjectVision/GeoDMS/wiki/Command-line-options)).
- *(new in 20.30)* When a configuration cannot be loaded, the reason is also written to the EventLog and, with `/L`, to the session log, not only shown in the error dialog (#1379).
- *(new in 20.30)* When the GUI cannot make the folder of the configuration the current folder, it reports the folder and the reason. On Windows it used to go on loading in the folder that was current, where a relative StorageName could be resolved against the wrong folder (#1379).
- *(new in 20.30)* On Linux, a `/L` or `/T` in the place of the configuration file name is rejected, as on Windows, with a message that shows the order the options must have. Before, it was taken as the name of the configuration file, and the load failed with only "chdir failed" (#1379, [Command-line options](https://github.com/ObjectVision/GeoDMS/wiki/Command-line-options)).
- *(new in 20.30)* A GUI test script (`/T`) can pass a command to the active view, for instance to choose an item of the pop-up menu of a map (`SEND 3` and `SEND 4`). While another program has the desktop focus, such a command now goes to the view that was active or opened last. On Linux it used to get lost then, and the commands after it went to another view. The same change is made on Windows, where other programs can send GeoDmsGuiQt these commands as well. Each error of a test script is now also written to standard error, so a run without `/L` shows which step failed (#1378, [Gui-scripting](https://github.com/ObjectVision/GeoDMS/wiki/Gui-scripting)).

## Python

- Each release has wheels of the `geodms` module for Python 3.12, 3.13 and 3.14, with the engine inside, so it needs no setup: `pip install` the wheel file for your Python version (#1355, [Python bindings](https://github.com/ObjectVision/GeoDMS/wiki/Python-bindings)). The wheels of a pre-release are only on its release page; those of a full release are also on PyPI, where 20.26.0 has the first ones, for Windows. *(new in 20.30)* There are now also wheels for 64-bit Linux with glibc 2.39 or newer, such as Ubuntu 24.04.
- A Windows wheel can be imported when a `gdal.dll` from another folder, such as that of a GeoDMS setup, is already loaded; in 20.26.0 `import geodms` then stopped with an error (#1355).
- *(new in 20.30)* On Linux `osgeo` and `geodms` can be imported in one process in either order, also when the GDAL of `osgeo` is in the global symbol scope, as when it is loaded with `RTLD_GLOBAL`: the engine hides the symbols of the GDAL, PROJ, GEOS and other libraries linked into it, so that it calls its own copies. A program that links GDAL itself and runs Python inside, as QGIS does, puts its GDAL in that scope too; a small test program that does so passes the same test, but QGIS itself was not tried (#1381).
- *(new in 20.30)* The GLOBIO flavour has a wheel of its own, `geodms-globio`, for Python 3.9 in the conda environment of the GLOBIO 4 model. It is imported as `geodms` too and carries its own copies of GDAL, PROJ and GEOS, so a script can import `osgeo` and `geodms` in either order. Do not install `geodms` and `geodms-globio` in one environment (#1355).
- *(new in 20.30)* The Linux setup also installs the module, for Python 3.12, 3.13 and 3.14. Adding its installation folder, `/opt/ObjectVision/GeoDms20.30.0.l`, to `sys.path` is enough to import it (#1355).
- The Python module of a GeoDMS setup can now be imported when its folder is only on `sys.path`; before, the folder also had to be on PATH (#1355).
- An item keeps its configuration loaded as long as Python holds it, which fixes a crash on the first read. `Config` has `close()` and works in a `with` block (#1370).

## Stability, examples and setups

- GeoDmsRun 20.27.0 could occasionally hang. A likely cause is fixed, but the hang was never reproduced, so #1329 stays open: please report it if you still see it.
- `examples/grid_to_polygon.dms` runs on Linux, and an interrupted download of the CBS geopackage is removed, so the next run downloads it again (#1359, #1360).
- `examples/convex_hull.dms` uses `fold_c` (#1347).
- The setups install `license.txt` (GPL version 3) and the licence texts of all bundled libraries (#1371).
- *(new in 20.30)* The setups also install, in `licenses/geodms`, the licence texts of the Noto fonts and the Remix Icon font that GeoDmsGuiQt contains, and the MIT notice of the code from GDAL in Geo.dll, the polygon rasterizer of `poly2grid`; the Python wheels carry that notice too (#1389).
- *(new in 20.30)* The branch lines of the tree view are drawn by GeoDMS itself and look as before; they were images from Qt's documentation, whose licence does not combine with the GPL. Four unused icons from a Qt example are no longer compiled into GeoDmsGuiQt (#1389).
- *(new in 20.30)* On Linux the engine libraries export only the functions and types that are marked for export, as on Windows, instead of all their functions. They are smaller (`libDmClc.so` 166 MB instead of 237 MB), and GeoDmsRun starts about two and a half to three times sooner: in WSL, a run of a configuration with one item takes 0.05 s instead of 0.12 to 0.15 s (#1383).
- *(new in 20.30)* On Linux, an error that the engine catches inside a call from another of its calls now reaches the log that `/L` names, as on Windows; it used to go to standard error only. A failed conversion names the type as `unsigned char` where Linux gave gcc's internal name `h`. A failed system call is reported under the category `System` on both platforms, where it said `WindowsSystem`, as in `System Error: CreateFileHandleForRwView(<file>)`; a file that cannot be opened is reported as `System Error: FileHandle::OpenForRead(<file>)`, where Windows said `WindowsSystem Error: FileMapHandle(<file>).OpenForRead` (#1384).

## Changes that can affect an existing configuration

- MMD data files have the extension `.mmddata` (#1349). Stores written by 20.28.0 or earlier are still read, with a deprecation warning, and converted when they are written again. A store written by 20.29.0 or later cannot be read by 20.28.0 or earlier, and GeoDms 21 will not read data files without the extension ([MMD](https://github.com/ObjectVision/GeoDMS/wiki/MMD)).
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
- *(new in 20.30)* The GeoDMS symbol font, `dms.ttf`, no longer has symbols at the character codes 123, 163, 164 and 165: a speaking head (twice), an open book and a plate with knife and fork. They were drawings from Microsoft's Webdings and Wingdings, which GeoDMS has no licence to distribute. A point layer that uses one of these codes as SymbolIndex no longer shows that symbol (#1389, [SymbolFont](https://github.com/ObjectVision/GeoDMS/wiki/SymbolFont)).
