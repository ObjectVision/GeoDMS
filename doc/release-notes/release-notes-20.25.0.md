**Pre-release.** GeoDms 20.25.0 holds the changes since **20.24.2**, which is about to become the latest full release, in all four flavours. For what came between 20.20.0 and 20.24.2, see the [notes of 20.24.2](https://github.com/ObjectVision/GeoDMS/releases/tag/v20.24.2). The details are in the issues linked below.

| Flavour | Asset |
|---|---|
| `.m`: Windows, MSBuild | `GeoDms20.25.0.m-Setup-x64.exe` |
| `.c`: Windows, CMake | `GeoDms20.25.0.c-Setup-x64.exe` |
| `.g`: Windows, GLOBIO compatibility | `GeoDms20.25.0.g-Setup-x64.exe` |
| `.l`: Linux, Ubuntu 24.04 | `GeoDms20.25.0.l-linux-x64.deb`, `.tar.gz` (+ `.sha256`, `.sha256.p7s`) |

## Before you upgrade

- **`point_in_polygon`** can give another of several overlapping polygons than 20.24.2 did; which one is not specified. Use `point_in_ranked_polygon` where the choice matters (#1292).
- **`mid` and `centroid_or_mid`** give null for a polygon with a null vertex, where they gave its first vertex (#1314).
- **MMD:** rewrite a store that an earlier version wrote in parts over several runs, or that holds an attribute computed by `const()` or `id()` (#1316, #1318). A rule that names a unit outside the store is now kept as a rule, so the reader has to declare that unit (#1318). A data file of an attribute that is now kept as a rule stays in an existing store folder (#1323).

## Faster

- **The spatial index of the polygon, box and arc operators is a loose quadtree** (#1292): `box_connectivity` of 2 million squares takes about 2.5 s instead of 5 to 40 s, `point_in_ranked_polygon` a quarter of its time.

## Fixed

- **MMD stores:** a store written in parts keeps the dictionary entries of earlier runs (#1316); an attribute computed by `const()` or `id()` gets its file or is kept as a rule (#1318); a store-local rule that another item computed first keeps its rule (#1317); a tiled read holder gives `id()`, `const()` and `range()` its tiling, where they failed or crashed (#1315).
- **`mid` and `centroid_or_mid`** no longer crash on a polygon whose vertices are all null (#1314).
