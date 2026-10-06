# Open GitHub issues, classified

*Status (2026-10-06): regenerated from https://github.com/ObjectVision/GeoDMS/issues on 2026-10-06,
after #1284 was closed that afternoon: **9 open issues**, one of which (#1283) is fixed and only waits
to be closed. Until 2026-10-06 this document was the snapshot of 2026-08-29, which listed 9 issues as
open; 7 of them have been closed since (#587, #990, #1165, #1191, #1198, #1214, #1221; #990, #1198 and
#1221 were in fact closed on 2026-08-27 and 2026-08-28, before that snapshot), and the open issues filed
after it were missing. The closing debriefs that snapshot carried for issues closed up to 2026-08-29
are in its git history; since then each debrief is on the issue itself. Which continuation to pick up
next, issues included, is ranked in `doc/continuations-2026-10-06.md`.*

Classes (a class without an open issue is left out):

- **A. Fixed, still open on GitHub**: only the closing is left.
- **B. Implementable after a design choice**: clear scope; one or two decisions to settle first.
- **C. Performance and refactoring**: the fix lives in an internal mechanism, not in a local patch.
- **D. Needs design**: new semantics.
- **G. Outside the engine**: hosting and website.

## A. Fixed, still open on GitHub

| Issue | Area | What is known | Related | Next step |
|---|---|---|---|---|
| [#1283](https://github.com/ObjectVision/GeoDMS/issues/1283) `geos_split_polygon` on a tiled domain gives a wrong `polygon_rel` | geo: polygon split | An unpartitioned element was placed by its index within its tile, so element k of every tile was unioned into slot k, in all five families. Fixed in 170f96e45 (2026-09-24) with `testcases/oper_split_polygon_tiled.dms`; the `dms_` family then got faster in a0800530c, 26ce1fbd1 and 2ccb28116, and its single-ring shortcut was narrowed in e73a7d403. In the release notes of 20.22.1. Phase B of the `dms_` dissolve still runs serially. | `doc/development/dms-dissolve-single-noding.md`; continuations A8 | Close on GitHub. Phase B in parallel per slot is continuations A8. |

## B. Implementable after a design choice

| Issue | Area | What is known | Related | Next step |
|---|---|---|---|---|
| [#1293](https://github.com/ObjectVision/GeoDMS/issues/1293) The relation to a factor of a combined domain, without `first_rel` over the whole of X | clc: `combine` | `combine_data(X, a_rel, b_rel)` has no inverse per element. The lazy sub-items `first_rel` to `sixteenth_rel` of `combine` (`clc/dll/src/OperUnit.cpp:136`) are attributes over the whole of X, and `combine_unit_uint8_16_32_64` does not make them, so models decode with `x / #B` and `x % #B` by hand (eleven sites in NetworkModel_PBL). Proposed: `first_rel(x_rel)` and on, computed per element of `x_rel` in the factor order of `UnitCombine_impl` (`OperUnit.cpp:94`), converted through the range of the factor, null stays null, nothing allocated over X. | continuations section 2, "lower in the queue" | Settle the name (`first_rel(x_rel)`, a positional form such as `combine_rel(x_rel, n)`, or both) and what a value outside X gives (null or an error); then a testcase per `combine` variant, as the issue lists. |
| [#1276](https://github.com/ObjectVision/GeoDMS/issues/1276) Store the units in an MMD file and read them back from it | stg: MMD | A store's dictionary checks the value types of the units it refers to, so a reader has to configure those units under the same paths. The maintainer's position (2026-09-15 and 2026-09-16): which units travel with a store is the modeller's choice, and they can go into the dictionary `.dms` that the `.MMD` write produces. #1275 (ed477c49d) made the dictionary carry Descr, Label, DialogType, DialogData and cdf. | wiki `MMD`; #1154, #1275 | Decide whether a store may declare (some of) its units itself, and how a reader's own units then relate to them. |

## C. Performance and refactoring

| Issue | Area | What is known | Related | Next step |
|---|---|---|---|---|
| [#1292](https://github.com/ObjectVision/GeoDMS/issues/1292) Every query of the spatial index tests the boxes that cross a centre line of a node above it | geo: spatial index | `GetQuadrantOffset` (`geo/dll/src/geom/SpatialIndex.h:94`) keeps a box that crosses or touches the centre of a node in that node, and a query reads the leaves of every node it passes. `box_connectivity` of 2 million squares takes 29 to 40 s when rows and columns of squares lie on those centre lines, against 4.5 s when they fall between them, and it runs on one thread. Not a regression: 20.20.0 measures the same. | `doc/performance-test.md` (the #1289 round); #1289 (108459dd2, 271922804, 1315b0357), #1290 (f6d34a69a, 9d23831a8 and five more), #1291 (11ff62902, 8d02be9cb); continuations B2 | Step 1: `box_connectivity` in parallel, with the per-block results concatenated in index order. Step 2: fewer boxes high in the tree (lists of straddlers per node, or a loose quadtree); since #1290 the results no longer depend on the visiting order. |
| [#1280](https://github.com/ObjectVision/GeoDMS/issues/1280) An MMD store keeps the abandoned chunks of a grown sequence pool, so a string or geometry attribute costs up to twice its content; and [#1294](https://github.com/ObjectVision/GeoDMS/issues/1294) the `.seq` file of a text column differs per run and keeps the old bytes of moved tile chunks | rtc: mapped files; stg: MMD | One root. A chunk that cannot grow in place is moved by `MappedFileHandle::allocChunk` (`rtc/dll/src/ser/FileMapHandle.cpp:320`) and `mempage_table::ReallocChunk` (`:689`) to a free chunk or to the end of the file; the old one goes to the free list with its bytes, and the largest abandoned chunk usually fits nowhere. The pool grows by doubling up to a million elements and then by a million (`rtc/dll/src/mem/MappedSequenceProvider.h:63`); the shrink in `UnLock` (`:106`) is commented out. #1280: +5.5 GB on about 30 GB of stores in NetworkModel_PBL. #1294: the tiles are written in parallel, so offsets, length and stale bytes differ per run (25.9 to 31.4 MB for one content), and two stores cannot be compared byte for byte. The Linux path (`allocAtEnd` only) was not measured. | continuations B3 | Decide between writing the tile chunks in order and without gaps when the store is closed, and only zeroing freed chunks; check what that means for views that are still mapped. The open questions are on both issues. |
| [#1205](https://github.com/ObjectVision/GeoDMS/issues/1205) Polygon overlay: divide the work by complexity instead of by element count | geo: polygon overlay | Reported on 20.12.0: an overlay whose first argument was a few hundred dissolved features ran on one thread for more than 10 hours, against 5.5 minutes with the undissolved features as first argument. GEO-A37 (f0efd51aa, 2026-09-30) runs the elements of a tile in blocks of 256 in parallel and converts each polygon of the second argument once per block, so a first argument of one tile no longer runs on a single thread; a few hundred huge polygons still fill only one or two blocks. On 2026-09-10 the maintainer asked for feedback before going further. | GEO-A37 in `doc/code-audit-2026-09-27.md`; continuations B9 | Ask the reporter to measure on 20.22.1; then blocks weighted by vertex count. |

## D. Needs design

| Issue | Area | What is known | Related | Next step |
|---|---|---|---|---|
| [#724](https://github.com/ObjectVision/GeoDMS/issues/724) Circular units (wrap-around for a grid or coordinate domain, or for time units) | calculations | The comments list what existing operators already allow with `%`, and which operators would need changes for cylindrical or spherical topology (potential, griddist, districting, connect, area). Marked not planned soon on 2026-06-17. | | None planned. |

## G. Outside the engine

| Issue | Area | What is known | Related | Next step |
|---|---|---|---|---|
| [#1277](https://github.com/ObjectVision/GeoDMS/issues/1277) HTTP 403 on www.geodms.nl | documentation: hosting | The hosting party filters known crawlers and generic HTTP clients on the whole shared server, and is working on exceptions per domain (comment of 2026-10-01). Nothing in the site or its converter causes it. | #1250, #1278 (both closed) | Wait for the exceptions per domain, then check the user agents listed on the issue. |

## Closed since the snapshot of 2026-08-29

One line per issue closed on or after 2026-08-29, plus the three that the snapshot still listed as open
although they had been closed before it. "Commits" are the commits whose subject names the issue; the
debrief is on the issue.

| Issue | Closed | Commits | Subject |
|---|---|---|---|
| #587 | 2026-09-07 | 6100a6469 and 12 more | storage-read functions in a calculation rule (`doc/development/storage-read-operators.md`) |
| #990 | 2026-08-28 | e600a440a, add27a69e | `union_data` with mismatching domains of equal size warns |
| #1145 | 2026-08-29 | c75b5db7d, d84988a1c | a unit with a foreign-domain attribute exports as a table again |
| #1165 | 2026-08-29 | f5002118f, d5dc30520, 50226d4f6 | the untagged brace form of a point range is deprecated |
| #1191 | 2026-08-29 | 9dc39cb72, ceb2a7762 | closing the GUI during a calculation |
| #1198 | 2026-08-28 | 34d64f9eb (a measurement) | admission under `ResourceAwareScheduling=enforce` |
| #1214 | 2026-09-06 | 0eac27f43, 747360369, fd3dd0b8f, f136489d7 | fault-tolerant sweep: the `dms_` overlay operators |
| #1221 | 2026-08-27 | f73feb72a | a chart on an attribute of a derived unit hung |
| #1224 | 2026-08-29 | f05d69fb4 | ExplicitSuppliers referring to a SubItem_PropValues attribute |
| #1225 | 2026-08-29 | 5b0d24b28 | teardown re-resolved an expired weak domain unit |
| #1226 | 2026-08-29 | d4ca07da5 | the export dialog hung on an alias unit |
| #1227 | 2026-09-09 | 208ab52fd and 5 more | the TokenStr self-deadlock class |
| #1228 | 2026-09-02 | dcefc78dc, c3b3022a9, 28a09cfe4 | `connect` and `connect_info` from arcs to points |
| #1229 | 2026-08-31 | c5a61e38f | ChangeInterest recursion in 20.17.0 |
| #1230 | 2026-08-31 | none (closed by decision) | t641_2 after the #1212 ring-role fix: the 20.19.1 values are the correct ones |
| #1231 | 2026-09-03 | 53789b4df | the Globio unit-test launchers |
| #1232 | 2026-09-03 | 662a470d1 | `gdalwrite.vect` and a geometry column not named `geometry` |
| #1233 | 2026-09-01 | d220ac609 and 15 more | the potential deadlocks of the static lock analysis (`doc/deadlocks.md`) |
| #1234 | 2026-09-02 | c8d5e6af3, 99b5f29cf | `GDALAllRegister` serialized |
| #1235 | 2026-09-02 | 7e380fa67 | the active TreeView item is visible again |
| #1236 | 2026-09-02 | 4cf213a33 | `connect_matrix` and `dist_matrix`: all matches within a cutoff |
| #1237 | 2026-09-03 | 685ee657f | crash of the one-argument `area` |
| #1238 | 2026-09-03 | 3c3da40af | map view caption without a SpatialReference |
| #1239 | 2026-09-03 | fab17e6e2, a0b485d23, 9d49b4ed5 | a GUI test script byte outside ASCII |
| #1240 | 2026-09-04 | 0047498f8, 8ece48c02 | `iif` on polygons |
| #1241 | 2026-09-04 | f27400a01 | a minidump from the exception filter |
| #1242 | 2026-09-04 | ad91a5ee7 | GUI crash when hovering a grid layer outside its tiles |
| #1243 | 2026-09-04 | cbc40d6fd, b31bdc317 | the composition deprecation message |
| #1244 | 2026-09-10 | bdbc4c8e9, e329888f6 | an attribute takes the domain of its nearest ancestor unit |
| #1245, #1246 | 2026-09-04 | 2171cff07 | MMD: a DisableStorage case parameter; a zero-element attribute |
| #1247 | 2026-09-08 | a2f51b697, 4e2cf8c3b | MMD: an alias of an item in the same store |
| #1248 | 2026-09-08 | 233305f1d, 4fe9dfa61 | the class-break writer as an operator |
| #1249 | 2026-09-08 | 4d422d9d9 | three `SharedActor` casts that skipped every TreeItem |
| #1250 | 2026-09-28 | none here (site converter) | links on geodms.nl |
| #1251 | 2026-09-06 | 55ecf72d3 | `@dumpconfig`: a CR added per line break |
| #1252 | 2026-09-06 | 09410a732, 07eaa3b61 | `@dumpconfig`: a signature-typed parameter |
| #1253 | 2026-09-06 | df0af919c | `@dumpconfig`: included items defined twice |
| #1254 | 2026-09-06 | e30ba676d | an included XML fragment |
| #1255 | 2026-09-10 | 50fb2b124 | view preparation on a detached thread |
| #1256 | 2026-09-06 | 9a6f8deb9 | `@dumpconfig`: a range with digit grouping |
| #1257 | 2026-09-10 | none of its own | a Job Object memory limit; what remained was filed as #1269 and #1270 |
| #1258 | 2026-09-06 | 99b5f29cf | NetCDF bands read concurrently |
| #1259 | 2026-09-09 | 63997d3a4 and 24 more | a result of interest retained its source closure; the deferral added under it was removed on 2026-09-21 (a7127224f, 92eaa7150) |
| #1260 | 2026-09-06 | dd1e31f74 | the character after an XML entity |
| #1261 | 2026-09-10 | 42b8da98e, a531e9727, ef09afac8 | the XML configuration syntax |
| #1262 | 2026-09-09 | 95472567d, b4e601059 | the case mix-up warning on lookup paths |
| #1263 | 2026-09-10 | 3b790f896 | centred chart labels |
| #1264 | 2026-09-09 | 1b53fefc8 | MMD: a store-local rule instead of the data |
| #1265 | 2026-09-09 | 85f0dda64 | re-entry of a failing Debug assertion |
| #1266 | 2026-09-08 | 220ec739c, 1946a5cdd, 686bdf1e6 | MMD: the unit-Range guard |
| #1267 | 2026-09-09 | 163a4a109 | MMD: a non-default tiling |
| #1268 | 2026-09-10 | 54e772a82, 9c4902ffd | Debug `@dumpconfig` and the lock ceiling |
| #1269 | 2026-09-10 | 6ff0d75b8 | an unchecked `VirtualAlloc(MEM_COMMIT)` |
| #1270 | 2026-09-10 | f0cff0d81 | exit code of a failed `@statistics` |
| #1271 | 2026-09-10 | 406c6b8e9, 100d09798 | fixtures of the installed battery |
| #1272 | 2026-09-12 | 0c9fc4be8 | an error when an MMD store is missing |
| #1273 | 2026-09-14 | af8c286fb, 42fb75d7a | a pie chart |
| #1274 | 2026-09-15 | 11d36e39c | the Layer Control of a layer without classification |
| #1275 | 2026-09-15 | ed477c49d | the MMD dictionary keeps DialogType and DialogData |
| #1278 | 2026-10-01 | none here (deploy workflow) | the nightly geodms.nl deploy |
| #1279 | 2026-09-22 | f8a16e77f | `geodms.pyd`: `fail_reason()` and `update_metainfo()` |
| #1281 | 2026-09-23 | 9347cd552 | `pareto_optimal` |
| #1282 | 2026-09-23 | c2f64650d and 4 more | `pareto_optimal_eps` and epsilon dominance |
| #1284 | 2026-10-06 | 9a1182a1e, 135f1fc20 | a second column of a `storage_read_table` under `for_each` |
| #1285 | 2026-09-27 | fafac0553, 227feef27, 336027f68, f18286c42 | Range Error in `modus` |
| #1286 | 2026-09-29 | c9f1ed4c8, 426a6ad98 | `pareto_optimal_eps` in parallel |
| #1287 | 2026-10-01 | e04ad20f0 | bool criteria in `pareto_optimal` |
| #1288 | 2026-10-02 | 182bc1b80 | MMD: a rule that names `org_rel` |
| #1289 | 2026-10-02 | 108459dd2, 271922804, 1315b0357, 8c6d66a6a | a spatial index node split without end |
| #1290 | 2026-10-03 | f6d34a69a, 9d23831a8 and 5 more | ties taken from the visiting order of the spatial index |
| #1291 | 2026-10-03 | 11ff62902, 8d02be9cb | `box_connectivity` of boxes that share only an edge or a corner |
