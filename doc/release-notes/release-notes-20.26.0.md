**Pre-release.** GeoDms 20.26.0 holds the changes since **20.25.0**, in all four flavours. For what came before, see the notes of [20.25.0](https://github.com/ObjectVision/GeoDMS/releases/tag/v20.25.0) and [20.24.2](https://github.com/ObjectVision/GeoDMS/releases/tag/v20.24.2). The details are in the issues linked below.

| Flavour | Asset |
|---|---|
| `.m`: Windows, MSBuild | `GeoDms20.26.0.m-Setup-x64.exe` |
| `.c`: Windows, CMake | `GeoDms20.26.0.c-Setup-x64.exe` |
| `.g`: Windows, GLOBIO compatibility | `GeoDms20.26.0.g-Setup-x64.exe` |
| `.l`: Linux, Ubuntu 24.04 | `GeoDms20.26.0.l-linux-x64.deb`, `.tar.gz` (+ `.sha256`, `.sha256.p7s`) |

## New

- **`impedance_matrix` and `impedance_matrix_od64` give the `LinkSet` of every route of a Pareto front** (#1325): `od:...,LinkSet` with the `pareto` option, which stopped with an error before, gives each row the links of its own route, from the destination back to the origin, also with `timedependent_alt`. The rows are the same as without it. `alternative(link_attr)` is still refused with `pareto`. Wiki: Impedance options, impedance_matrix.
- **A domain names the X axis of its charts**: a Scatter, Line or Bar Chart of an attribute opens with the attribute of the same domain whose `DialogType` is `"XAxis"` on the horizontal axis, instead of the row number; **Use as X axis ...** changes it as before. Wiki: Main menu, Domain unit example.

## Fixed

- **A unit that already had a range and then got an irregular tiling**, by the range operator with a tiled base unit or by `TiledUnit`, reported the change with a pointer it had just given away, which could crash (#1324).
