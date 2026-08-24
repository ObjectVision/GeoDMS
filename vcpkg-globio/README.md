# GLOBIO flavour dependencies

The G flavour deliberately has its own vcpkg manifest and installed tree.
The repository-root `vcpkg.json` remains the dependency graph for the regular
`.m`, `.c`, and `.l` builds.

This manifest contains only GeoDMS dependencies that are not part of GLOBIO's
spatial ABI. GDAL, PROJ, GEOS, TIFF, NetCDF, SQLite, and their runtime closure
come from the exact conda environment locked in `environment.yml`. Mixing those
packages with the current root manifest would reintroduce the same-name DLL
collision that the G flavour exists to avoid.

Keep common vcpkg dependencies identical in both manifests. Run
`tools/verify-vcpkg-manifests.ps1` after changing either manifest; both G build
scripts run this check automatically. Only the three root spatial entries
(`gdal`, `geos`, and `sqlite3`) are omitted from the G manifest.

Build with MSBuild using `/p:GeoDmsGlobio=true`. This selects:

- `vcpkg-globio/vcpkg.json` and `vcpkg_installed_GLOBIO/`;
- `bin_GLOBIO/<Debug|Release>/x64` and `obj_GLOBIO/`;
- `python/PythonVersionsGlobio.txt` (CPython 3.9 only); and
- the conda prefix named by `GLOBIO_ENV_ROOT`.

For a new development environment, create the exact prefix and then verify it:

```powershell
micromamba create --prefix C:\dev\globio4env --file vcpkg-globio\environment.yml
setx GLOBIO_ENV_ROOT C:\dev\globio4env
tools\verify-globio-environment.ps1 -GlobioRoot C:\dev\globio4env
```

Run `batch/BuildSignAndCreateSetupGlobio.bat` for the signed `.g` setup.
