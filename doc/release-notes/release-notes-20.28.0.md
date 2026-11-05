**Pre-release.** GeoDms 20.28.0 holds the changes since **20.26.0**, the latest full release, in all four flavours. It includes the pre-release 20.27.0, which had the `.m` flavour only; what came after it is marked *(new in 20.28)*. Most of it is #1327: operators that do not know the number of their result rows beforehand now write their result once, into its tiles, instead of counting first or collecting the rows and copying them. Their results are the same as in 20.26.0. The details are in the issues linked below.

| Flavour | Asset |
|---|---|
| `.m`: Windows, MSBuild | `GeoDms20.28.0.m-Setup-x64.exe` |
| `.c`: Windows, CMake | `GeoDms20.28.0.c-Setup-x64.exe` |
| `.g`: Windows, GLOBIO compatibility | `GeoDms20.28.0.g-Setup-x64.exe` |
| `.l`: Linux, Ubuntu 24.04 | `GeoDms20.28.0.l-linux-x64.deb`, `.tar.gz` (+ `.sha256`, `.sha256.p7s`) |

## Memory and speed

- **`impedance_matrix` makes a sparse result in one pass** (#1327): the search from each origin zone runs once, where it ran twice before, first to count the rows of each origin zone and then to write them. `precalculated_NrDstZones` no longer saves time, but still fixes the number of rows per origin zone ([Impedance options](https://github.com/ObjectVision/GeoDMS/wiki/Impedance-options)).
- *(new in 20.28)* **`join_near_values`, `connect_matrix` and `dist_matrix` write their rows in one pass** into the tiles of the result (#1327), where they kept the rows of every input tile until all were done and then copied them into one untiled result.
- *(new in 20.28)* **`modus`, `weeded_counts`, the [classify functions](https://github.com/ObjectVision/GeoDMS/wiki/Classify-functions) and `unique` keep their intermediate values in blocks** that are freed as they are read (#1327). On 2^25 distinct values, `modus` takes 562 instead of 1083 MiB above the baseline and `unique` 271 instead of 551 MiB. `unique` of strings and arcs no longer copies its argument: on 2^22 strings it takes 3.8 instead of 6.1 s.
- *(new in 20.28)* **`pareto_optimal` and `pareto_optimal_eps` read their criteria from their tiles** (#1327) and take at most 16 bytes per row while they sort, whatever the number of criteria, where four criteria took 64. On 2^25 rows the peak goes from 2448 to 912 MB, so a large call that ran out of memory now fits.

## Charts and views

- **A chart line takes the colour of its series** (ObjectVision/NetworkModel_PBL#129): a Line or Points + Line layer whose points take their colour from a classification draws each segment in the colour of the point it starts from, so several series in one chart can be told apart by their lines. A series without a classification keeps the red line.
