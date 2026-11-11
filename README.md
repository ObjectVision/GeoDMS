# GeoDMS

GeoDMS (Geographic Data & Model Software) is an open-source platform for spatial models that calculate with large grid, vector and attribute data: discrete allocation of land use under capacity restrictions, accessibility with impedance matrices and network assignment, and scenarios compared side by side. A model is a set of plain-text configuration files that declare every map, table and indicator as an expression over source data. A multi-threaded C++ engine calculates what is requested, checks units and domains, and shows how any result was derived, so a model is scripted, reproducible and auditable, at the scale of the Netherlands at 10 m or Europe at 100 m on a single workstation.

## When to use GeoDMS, and when another tool fits better

GeoDMS is a modelling platform, not a general-purpose GIS. It is less suitable for quickly making a simple thematic map (use QGIS or ArcGIS for that) and for managing or versioning data (use a database or a version control system).

| Tool | Fits better for | GeoDMS fits better for |
|---|---|---|
| Desktop GIS: QGIS, GRASS GIS | a quick map, editing geometry, exploring a dataset, a one-off analysis with an algorithm from the toolbox | a model that is rerun for many scenarios and years, kept as text in git, with every intermediate result traceable to its source |
| Map-algebra frameworks: PCRaster, LUE | time-stepped raster models of environmental processes, such as hydrology or erosion, written in Python; LUE also runs such models on a cluster | models that combine grids with polygons, points, networks and attribute tables, with discrete allocation and network operators built in |
| Python raster libraries: xarray-spatial | a few raster operations, such as slope, proximity or zonal statistics, inside a Python, xarray or dask workflow | a whole model of many steps: only requested results are calculated, intermediate results can be kept on disk between runs, and each one opens as a map with one click |
| Dataframe and SQL engines: GeoPandas, DuckDB Spatial, Apache Sedona | preparing, cleaning and querying vector data, and ad hoc spatial joins in Python or SQL; Sedona for data spread over a Spark cluster | a calculation chain over many datasets at national scale on one machine, with domains and units checked by the engine before anything is calculated |

[Why GeoDMS](https://github.com/ObjectVision/GeoDMS/wiki/Why-GeoDMS) makes the case in more detail, with a translation table for pandas users.

## Who uses it

GeoDMS is the engine behind models that national and European policy leans on: the RuimteScanner and Vesta at PBL, LUISA and CRISP at the Joint Research Centre, the spatial projections in the United Nations' World Urbanization Prospects 2025, and the WoonZorgWijzer used by Dutch provinces and municipalities. Who commissioned what is on [objectvision.nl/projects](https://www.objectvision.nl/projects/), and the reports and dissertations that came out of it are on [objectvision.nl/publications](https://www.objectvision.nl/publications/).

## Quick start

### Windows

Download `GeoDms<version>.m-Setup-x64.exe` from the [release page](https://github.com/ObjectVision/GeoDMS/releases) and run it; the section [Verifying downloads](#verifying-downloads) below says how to check its signature. The setup installs the GUI, `GeoDmsRun.exe` and three example configurations into `C:\Program Files\ObjectVision\GeoDms<version>.m`.

Open the example that turns the CBS neighbourhood map of the Netherlands into a grid and back into simplified polygons:

```powershell
$geodms = "C:\Program Files\ObjectVision\GeoDms20.28.0.m"   # the folder the setup installed into
& "$geodms\GeoDmsGuiQt.exe" "$geodms\examples\grid_to_polygon.dms"
```

In the TreeView, double-click `resulting_polygons`. GeoDMS reads the neighbourhood polygons, rasterises them at 25 m, derives simplified polygons from the grid and shows them in a map view; any item it calculated on the way can be opened the same way. The first time, the example downloads the CBS geopackage (about 160 MB) from PDOK into the folder `RegionalUnits\cbs` of the SourceDataDir, which is `C:\SourceData` unless another folder is configured.

The same calculation without the GUI:

```powershell
& "$geodms\GeoDmsRun.exe" "$geodms\examples\grid_to_polygon.dms" /checks
```

GeoDmsRun calculates the container `checks`, whose integrity checks compare the simplified polygons with the source map, and prints a line that starts with `grid_to_polygon: 14729 buurten, 14729 with a polygon`. Exit code 0 means that every check held. Once the geopackage is there, the run takes less than a minute and up to 10 GB of memory.

### Linux

Install the `.l` package from the release page as described on [GeoDMS on Linux](https://github.com/ObjectVision/GeoDMS/wiki/GeoDMS-on-Linux), and start with the example that needs no download:

```bash
/opt/ObjectVision/GeoDms20.28.0.l/GeoDmsRun /opt/ObjectVision/GeoDms20.28.0.l/examples/function.dms /checks
```

It declares a typed function, applies it to a road and a rail table, and checks the results; exit code 0 means that every check held. `/opt/ObjectVision/GeoDms20.28.0.l/geodms <config>.dms` opens a configuration in the GUI.

## Python

The `.m` and `.c` Windows setups contain the `geodms` module, which runs the engine inside a 64-bit CPython 3.12, 3.13 or 3.14 process. It is not on PyPI; Python finds it in the installation folder:

```python
import os, sys
geodms_dir = r"C:\Program Files\ObjectVision\GeoDms20.28.0.m"
sys.path.append(geodms_dir)
os.add_dll_directory(geodms_dir)

import geodms
engine = geodms.Engine()
config = engine.load_config(geodms_dir + r"\examples\function.dms")
congestion = config.const_root().find("/congestion")
print(congestion.asDataItem().get_values_as_float_list())   # [0.5, 1.0, 0.5, 1.0]
```

The module uses the GDAL that comes with GeoDMS, so a process that also loads another GDAL build, such as that of QGIS, OSGeo4W or rasterio, is not safe; keep those in a separate process. The Linux package does not contain the module. [Python bindings](https://github.com/ObjectVision/GeoDMS/wiki/Python-bindings) describes the whole API.

Many projects do not need the module and exchange data files instead. A configuration reads and writes [parquet](https://github.com/ObjectVision/GeoDMS/wiki/Parquet), which pandas, pyarrow and GeoPandas read and write as well. Python can then run GeoDmsRun as a separate process and read its results, or the model can start a Python script itself through [exec_ec](https://github.com/ObjectVision/GeoDMS/wiki/Exec_ec) and read its output back. [GeoDMS through Python](https://github.com/ObjectVision/GeoDMS/wiki/GeoDMS-through-Python) compares these two routes with the module.

## Documentation

- The [GeoDMS Academy](https://github.com/ObjectVision/GeoDMS_Academy/wiki) teaches the language step by step, from installation to land-use allocation and network calculations.
- The [wiki](https://github.com/ObjectVision/GeoDMS/wiki), also published as [geodms.nl](https://geodms.nl), is the reference for the language, the operators, the GUI and GeoDmsRun, with worked [configuration examples](https://github.com/ObjectVision/GeoDMS/wiki/Configuration-examples).
- The methods and the models built with GeoDMS, such as land-use allocation, accessibility, the network models for the Netherlands and Europe, RSopen, CRISP and the BAG tools, are documented in the wikis of their own repositories, listed under [Project wikis](https://github.com/ObjectVision/GeoDMS/wiki#project-wikis) on the wiki home.

## GeoDMS as a tool for AI agents

GeoDMS is scriptable end to end: `GeoDmsRun.exe` installs unattended, calculates any item of a configuration headlessly, prints statistics, explains individual values, dumps configuration trees, and reports through exit codes, a tool contract that coding agents can drive directly. Ready-made agent skills (Claude Code / Codex `SKILL.md` format) for running GeoDMS, polygon simplification, discrete land-use allocation and travel-time calculation are catalogued at [GeoDMS AI skills](https://github.com/ObjectVision/GeoDMS/wiki/GeoDMS-AI-skills); the command-line reference is [User Guide GeoDMS Run](https://github.com/ObjectVision/GeoDMS/wiki/User-Guide-GeoDMS-Run).

## Verifying downloads

All GeoDMS installers and packages are signed with the **Object Vision B.V.**
GlobalSign EV Code Signing certificate.  Only run or install a package after
confirming it carries a valid signature from Object Vision B.V.

### Windows

The `.exe` installer is Authenticode-signed.  Windows will show a publisher
dialog during installation; confirm the publisher reads **Object Vision B.V.**
before proceeding.

To verify before running:

```powershell
# PowerShell: should print "Valid" and show O=Object Vision B.V.
Get-AuthenticodeSignature "GeoDms<version>.m-Setup-x64.exe" | Format-List
```

Or right-click the file → Properties → Digital Signatures → select the
signature → Details → confirm the signer is **Object Vision B.V.**

### Linux

Each Linux release includes a SHA-256 checksum file and a CMS/PKCS#7 detached
signature (`.p7s`) signed with the Object Vision B.V. GlobalSign EV Code Signing
certificate.  Download both alongside the tarball and verify before extracting:

```bash
openssl cms -verify -binary -purpose any \
  -in      GeoDms<ver>-linux-x64.tar.gz.sha256.p7s -inform DER \
  -content GeoDms<ver>-linux-x64.tar.gz.sha256 \
  -CAfile  <(curl -fsSL http://secure.globalsign.com/cacert/codesigningrootr45.crt \
             | openssl x509 -inform DER)

sha256sum -c GeoDms<ver>-linux-x64.tar.gz.sha256
```

For a full explanation of the certificate chain, how to obtain the signing root
from alternative sources, and how to inspect the signer identity, see
[`nsi/VERIFY-LINUX.md`](nsi/VERIFY-LINUX.md).

## Compilation

Build instructions for GeoDMS can be found at our [wiki compilation page](https://github.com/ObjectVision/GeoDMS/wiki/Compiling-the-GeoDMS) or our [website](https://geodms.nl/docs/compiling-the-geodms.html).
For a technical overview of the GeoDMS software, see [readme-dms.MD](readme-dms.MD) in the root folder of the source code.

## How to cite

The file [CITATION.cff](CITATION.cff) holds the citation metadata of GeoDMS; on GitHub, the link _Cite this repository_ in the About panel of the repository page gives it as APA and BibTeX. Add the GeoDMS version you used to the citation, as it appears in the name of the installation folder, since results can differ between versions.

## Terms of use

GeoDMS is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version. The licence text is in [license.txt](license.txt) and on [gnu.org](https://www.gnu.org/licenses/gpl-3.0.en.html); [license.md](license.md) also gives the licence of the wiki and the GeoDMS Academy.

## Contact

Feel free to contact us for support on land use modelling, transport modelling or geographic scientific computing.

Object Vision b.v.
Email:    [info(at)objectvision.nl](mailto:info@objectvision.nl)
Phone:    +31-20-598.9083
Location: 1081 HV  AMSTERDAM - the Netherlands - De Boelelaan 1085 room R223
Website:  [https://objectvision.nl](https://objectvision.nl)
