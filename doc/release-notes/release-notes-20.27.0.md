**Pre-release.** GeoDms 20.27.0 holds the changes since **20.26.0**, the latest full release, in the `.m` flavour only. The details are in the issues linked below.

| Flavour | Asset |
|---|---|
| `.m`: Windows, MSBuild | `GeoDms20.27.0.m-Setup-x64.exe` |
| `.c`: Windows, CMake | not in this pre-release |
| `.g`: Windows, GLOBIO compatibility | not in this pre-release |
| `.l`: Linux, Ubuntu 24.04 | not in this pre-release |

## Networks

- **`impedance_matrix` makes a sparse result in one pass** (#1327): the search from each origin zone runs once, where it ran twice before, first to count the rows of each origin zone and then to write them. The rows and their order are the same as in 20.26.0. `precalculated_NrDstZones` no longer saves time, but still fixes the number of rows per origin zone ([Impedance options](https://github.com/ObjectVision/GeoDMS/wiki/Impedance-options)).

## Charts and views

- **A chart line takes the colour of its series** (ObjectVision/NetworkModel_PBL#129): a Line or Points + Line layer whose points take their colour from a classification draws each segment in the colour of the point it starts from, so several series in one chart can be told apart by their lines. A series without a classification keeps the red line.
