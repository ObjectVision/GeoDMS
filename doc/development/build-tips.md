# Build tips

## Sequencing — never run two full GeoDMS builds in parallel

Whether MSBuild Release, MSBuild Debug, CMake `windows-x64-{debug,release}`,
or CMake `linux-x64-{debug,release}` (under WSL), **finish one before
starting the next**. Two simultaneous full-solution compiles risk OOM and
will trash each other's intermediate files.

Compiling `DmClc` and `DmGeo` is memory-heavy (template-heavy translation
units; `RLookup.cpp` in particular can hit MSVC `C1060` "compiler is out
of heap space" on the 32-bit hosted toolset even with plenty of free
RAM — pass `/p:PreferredToolArchitecture=x64` to force the 64-bit hosted
`cl.exe`).

Two simultaneous cmake builds against the *same* tree also fight over
intermediate `.obj` files in `build/.../*.dir/Release/`, producing
`Permission denied` errors that can leave the tree in an
unrecoverable state — kill all stragglers before retrying.

**Exception:** lighter incremental rebuilds (e.g. just `Shv.dll` after
edits in `shv/dll/src/`) are fine in parallel — but be aware of PDB-file
collisions when two MSBuild invocations target the same
`obj/Debug/x64/Shv/vc145.pdb`.

## Windows CMake build paths

CMake and vcpkg for Windows builds come from the Visual Studio 18 (2026)
Community installation:

- CMake:
  `C:/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe`
- vcpkg toolchain:
  `C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake`

vcpkg packages are pre-installed under `vcpkg_installed/` at the repo root
(triplet `x64-windows-v145`). Point CMake at this dir via
`-DVCPKG_INSTALLED_DIR=C:/dev/GeoDMS_2026/vcpkg_installed`.

Qt6 is at `C:/Qt/6.9.0/msvc2022_64/` — pass as `CMAKE_PREFIX_PATH`.

The CMake presets in [`CMakePresets.json`](../../CMakePresets.json) use
`$env{VCPKG_ROOT}` which is **not** set in the environment by default;
the toolchain and installed-dir overrides above must be passed explicitly.

### Typical configure invocation (bash from repo root)

```sh
CMAKE='/c/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
TOOLCHAIN='C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake'
"$CMAKE" --preset windows-x64-release \
  -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
  -DVCPKG_INSTALLED_DIR="C:/dev/GeoDMS_2026/vcpkg_installed" \
  -DCMAKE_PREFIX_PATH="C:/Qt/6.9.0/msvc2022_64"
```

### Build invocation

```sh
"$CMAKE" --build --preset windows-x64-release
```
