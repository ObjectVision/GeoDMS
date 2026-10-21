**Pre-release.** GeoDms 20.26.0 holds the changes since **20.25.0**, in all four flavours. For what came before, see the notes of [20.25.0](https://github.com/ObjectVision/GeoDMS/releases/tag/v20.25.0) and [20.24.2](https://github.com/ObjectVision/GeoDMS/releases/tag/v20.24.2). The details are in the issues linked below.

| Flavour | Asset |
|---|---|
| `.m`: Windows, MSBuild | `GeoDms20.26.0.m-Setup-x64.exe` |
| `.c`: Windows, CMake | `GeoDms20.26.0.c-Setup-x64.exe` |
| `.g`: Windows, GLOBIO compatibility | `GeoDms20.26.0.g-Setup-x64.exe` |
| `.l`: Linux, Ubuntu 24.04 | `GeoDms20.26.0.l-linux-x64.deb`, `.tar.gz` (+ `.sha256`, `.sha256.p7s`) |

## Fixed

- **A unit that already had a range and then got an irregular tiling**, by the range operator with a tiled base unit or by `TiledUnit`, reported the change with a pointer it had just given away, which could crash (#1324).
