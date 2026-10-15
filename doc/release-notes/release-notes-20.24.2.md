**Pre-release.** GeoDms 20.24.2 brings everything since **20.20.0**, the latest full release, in all four flavours. It includes the pre-release 20.23.1; what came after it is marked *(new in 20.24)*. The details are in the issues linked below and on the wiki page of each operator; the [full list up to 20.23.1](https://github.com/ObjectVision/GeoDMS/blob/v20.24.2/doc/release-notes/release-notes-20.23.0.md) also gives what was measured.

| Flavour | Asset |
|---|---|
| `.m`: Windows, MSBuild | `GeoDms20.24.2.m-Setup-x64.exe` |
| `.c`: Windows, CMake | `GeoDms20.24.2.c-Setup-x64.exe` |
| `.g`: Windows, GLOBIO compatibility | `GeoDms20.24.2.g-Setup-x64.exe` |
| `.l`: Linux, Ubuntu 24.04 | `GeoDms20.24.2.l-linux-x64.deb`, `.tar.gz` (+ `.sha256`, `.sha256.p7s`) |

## Before you upgrade

- **Memory and run times are back at 20.19.x.** The deferral of commits and IntegrityChecks that 20.20.0 introduced is withdrawn; plan on the 20.19.x figures, not on those of 20.20.0 (#1259).
- **Some results change; the old ones were wrong.** Among them `diversity`, `rounded_convert`, `corr`, `modus` per partition (#1285), the interaction results of `impedance_matrix`, the split polygon operators on a tiled domain (#1283), equally near candidates in `connect` and the other search operators (#1290), `box_connectivity` for boxes that only touch (#1291), the non-zero classifications, Jenks-Fisher breaks for values far from zero, and 64-bit integer columns of a `.dbf`. *(new in 20.24)* A float literal is rounded correctly, so `0.7` in a rule equals `float64('0.7')`, and `0.1 + 0.2 == 0.3` is false, as in IEEE arithmetic; `uint2(3.9)` is 3, where it was 0; and a conversion to string no longer depends on the thousand-separator option.
- **Some configurations that ran now stop with an error**, with a message that names the cause: a missing `.mmd` store (#1272); an upper-case literal suffix such as `60D` (#1262); a region id outside its unit, or a node, zone or region unit whose range does not start at 0, in the allocation and network operators; a simplify tolerance that is an attribute, negative or null; a rotated georeference in a tif or world file; counts and sums that overflow their type. *(new in 20.24)* `combine_data` checks each relation against the factor at its position, so relations given in another order than the factors of the combine are refused (#1293, #1310); a literal that its type cannot hold, such as `300b` or `7u2`, is an error, as is a data-block point outside the range of its point type.
- **Rewrite an `.mmd` store that 20.20.0 wrote below a `select_with_org_rel` or `select_with_attr_by_*` holder** (#1288).

## New

- **`pareto_optimal` and `pareto_optimal_eps`**, and a relative epsilon for the pareto option of `impedance_matrix` (#1281, #1282, #1286, #1287).
- *(new in 20.24)* **Time-dependent link impedances** in `impedance_matrix`, `timedependent(...)` and `timedependent_alt(...)` for the second criterion of pareto (#1304).
- *(new in 20.24)* **Combined relations decoded per element:** `combine_rel` and `first_rel` .. `sixteenth_rel`; `combine_data` is an operator again, and a combine on either side may stand for its factors (#1293, #1310).
- *(new in 20.24)* **Convex parts and grouped connectivity:** `dms_split_convex_polygon`, `dms_split_convex_polygon_keilsnoeyink`, `dms_polygon_connectivity` with groups and `dms_convex_polygon_connectivity` (#1300, #1305); `dms_buffer_multi_polygon` and Minkowski sums in one sweep per element (#1301).
- **Pie Chart** (#1273), a legend row for a layer drawn without a classification (#1274), an `.mmd` dictionary that carries how its items are shown (#1275), and in Python `update_metainfo()` and a safe `fail_reason()` (#1279).
- *(new in 20.24)* `GeoDmsRun` writes its log while the main thread computes, where long tasks used to keep it silent until they ended (#1303); `tif` reads 24-bit RGB, 4-bit and 8-bit files into `uint32`, `uint8` and `bool`.

## Faster

- The `dms_*` polygon operators, several times (#1283), and *(new in 20.24)* their per-element forms in parallel blocks (#1205, #1306), the buffer and Minkowski operators about a quarter (#1302), and the second phase of a partitioned dissolve in parallel; the spatial index of the search operators (#1289); reading a `.dbf`; and many polygon, grid and network operators that now run in parallel.

## Fixed

- **Storages:** a `for_each` over a `gdal.vect` table that needs a second column of it (#1284); the `odbc` and `bmp` storages; `gdal.vect` on mixed geometry layers; writes that failed without an error. *(new in 20.24)* An `.mmd` array gets its file only when its write has finished, and a store lock keeps out a second writer (#1298); stored arrays and sequences hold exactly their bytes, where they kept unused space (#1280, #1308); the Linux build reads stores written on Windows (#1297).
- **Memory:** *(new in 20.24)* `id()` and the factor relations of a combine no longer hold a record per tile, which took gigabytes on very large domains (#1309); `@statistics` reports the sizes of tiled strings correctly (#1299).
- **Robustness:** the findings of a code audit, among them reads beyond the end of an array in Release builds and a spatial index that split a node without end (#1289); *(new in 20.24)* code that wrote into or read from a tile without holding it: the colour writers of the GUI, the GDAL field writers, `potential`, text parsing, and the numeric array calls of the C interface (#1311).
- **GUI and Python:** pasting into a grid layer, a WMS server that stops answering, table actions on rows still being computed, wide dashed pens; crashes in the Python binding (#1279).
- **Setups:** the uninstaller removes the install folder also when a post-install test fails (#1296), and a setup run keeps itself at Normal priority, which a console in Visual Studio did not.
