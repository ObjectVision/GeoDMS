# Copyright (C) 2026 Object Vision B.V.
# License: GNU GPL 3

"""Exercise the Linux module next to another GDAL in the same process (#1381, #1383).

    python LinuxGdalCompatibility.py <order> <output_dir>
    python LinuxGdalCompatibility.py <order> --installed

order is one of
    gdal-preloaded             Python is started with the GDAL of osgeo in LD_PRELOAD, and that GDAL is
                               initialised with GDALAllRegister before anything is imported, as in a
                               program that links GDAL itself, such as QGIS; osgeo then uses that GDAL
    osgeo-global-first         osgeo first, loaded with RTLD_GLOBAL, so its GDAL is in the global
                               symbol scope when geodms is imported
    geodms-first-osgeo-global  geodms first, then osgeo with RTLD_GLOBAL
    osgeo-first                osgeo first, loaded the usual way (RTLD_LOCAL)
    geodms-first               geodms first, then osgeo the usual way

With an output folder (build/linux-x64-release/bin or an installed .l setup), the module is imported
from that folder; with --installed, the geodms wheel installed in this Python is tested. osgeo is the
one this Python finds, such as Ubuntu's python3-gdal in a venv made with --system-site-packages.

The engine links GDAL and PROJ into its own libraries and keeps their symbols to itself, so in every
order the process holds two GDALs that must not touch each other. The engine converts a point from
EPSG:28992 to EPSG:4326 and writes it with a few values to a dbf through its GDAL; osgeo reads the dbf
back and converts the same point. The engine's point must agree with the one osgeo computes in a
process without geodms, within the tolerance below, since both run their own PROJ with their own
proj.db; osgeo's point must equal it exactly, since the engine must leave osgeo's GDAL and PROJ as
they were. osgeo then creates a raster and reports a missing file as an exception. Before #1381 the
orders with GDAL in the global scope crashed at the write. Exit code 0 when every check passes.
"""

import argparse
import ctypes
import inspect
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile


ORDERS = ("gdal-preloaded", "osgeo-global-first", "geodms-first-osgeo-global", "osgeo-first", "geodms-first")

# A point in EPSG:28992 (Amersfoort / RD New), easting and northing in metres.
RD_X, RD_Y = 155000.0, 463000.0
# The engine and osgeo run different builds of PROJ, each with its own proj.db, and may choose a
# different transformation for this datum shift; 1e-4 degrees is about 11 m in latitude and 7 m in
# longitude here, ten times what such a choice can make.
DEGREE_TOLERANCE = 1e-4

PRELOAD_MARK = "GEODMS_TEST_GDAL_PRELOADED"

parser = argparse.ArgumentParser()
parser.add_argument("order", choices=ORDERS)
parser.add_argument("output_dir", type=pathlib.Path, nargs="?")
parser.add_argument("--installed", action="store_true", help="test the installed geodms wheel")
args = parser.parse_args()
if args.installed == (args.output_dir is not None):
    parser.error("give either an output folder or --installed")


def run_python(code):
    """Runs code in a fresh interpreter of this Python, without geodms and without a preloaded GDAL."""
    env = {k: v for k, v in os.environ.items() if k not in ("LD_PRELOAD", PRELOAD_MARK)}
    return subprocess.run([sys.executable, "-I", "-c", code], env=env, capture_output=True, text=True,
                          check=True).stdout.strip()


def mapped_libgdal():
    """The path of the libgdal that this process has mapped, or None."""
    with open("/proc/self/maps") as maps:
        for line in maps:
            path = line.split()[-1]
            if "/libgdal.so" in path:
                return path
    return None


if args.order == "gdal-preloaded" and os.environ.get(PRELOAD_MARK) != "1":
    libgdal = run_python("from osgeo import gdal\n"
                         "print(next(l.split()[-1] for l in open('/proc/self/maps') if '/libgdal.so' in l))")
    env = dict(os.environ, LD_PRELOAD=libgdal, **{PRELOAD_MARK: "1"})
    sys.stdout.flush()
    os.execve(sys.executable, sys.orig_argv, env)  # this script again, with the same interpreter options

if not args.installed:
    sys.path.insert(0, str(args.output_dir.resolve()))

failures = []


def check(ok, what):
    print(("  OK: " if ok else "  FAIL: ") + what, flush=True)
    if not ok:
        failures.append(what)


def osgeo_transform(osr, x, y):
    """Converts (x, y) from EPSG:28992 to (lon, lat) in EPSG:4326 with the given osgeo.osr."""
    src = osr.SpatialReference()
    src.ImportFromEPSG(28992)
    dst = osr.SpatialReference()
    dst.ImportFromEPSG(4326)
    for s in (src, dst):
        s.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    lon, lat, _ = osr.CoordinateTransformation(src, dst).TransformPoint(x, y)
    return lon, lat


# osgeo alone, in a process of its own; print() writes a float so that float() reads it back exactly
reference = tuple(float(v) for v in run_python(
    "from osgeo import osr\nosr.UseExceptions()\n" + inspect.getsource(osgeo_transform)
    + f"print(*osgeo_transform(osr, {RD_X!r}, {RD_Y!r}))").split())


def import_osgeo(into_global_scope):
    flags = sys.getdlopenflags()
    if into_global_scope:
        sys.setdlopenflags(os.RTLD_NOW | os.RTLD_GLOBAL)
    try:
        from osgeo import gdal, ogr, osr
    finally:
        sys.setdlopenflags(flags)
    gdal.UseExceptions()
    osr.UseExceptions()
    return gdal, ogr, osr


if args.order == "gdal-preloaded":
    preloaded = mapped_libgdal()
    check(preloaded is not None, f"GDAL is loaded before anything is imported: {preloaded}")
    program_gdal = ctypes.CDLL(None)  # the global scope, where the preloaded GDAL is
    program_gdal.GDALAllRegister()
    program_gdal.GDALVersionInfo.restype = ctypes.c_char_p
    print("  the program's GDAL, initialised:", program_gdal.GDALVersionInfo(b"--version").decode(), flush=True)

if args.order.startswith("osgeo"):
    gdal, ogr, osr = import_osgeo(args.order == "osgeo-global-first")
    import geodms
else:
    import geodms
    gdal, ogr, osr = import_osgeo(args.order == "geodms-first-osgeo-global")

DMS = """container linux_gdal_compatibility
{
	unit<dpoint> rd  : SpatialReference = "EPSG:28992";
	unit<dpoint> wgs : SpatialReference = "EPSG:4326";
	unit<uint32> obj : nrofrows = 3
	{
		attribute<float64> v : [3.0, 6.0, 9.0];
		attribute<rd>      p_rd  := point_xy(const(@X@, obj), const(@Y@, obj), rd);
		attribute<wgs>     p_wgs := convert(p_rd, wgs);
	}
	container export
	{
		unit<uint32> tab := obj
		,	StorageName = "@DBF@"
		,	StorageType = "gdalwrite.vect"
		{
			attribute<float64> v := obj/v;
			attribute<float64> r := pointrow(obj/p_wgs);
			attribute<float64> c := pointcol(obj/p_wgs);
		}
	}
}
"""

print(f"{args.order}, {'installed wheel' if args.installed else args.output_dir}; Python {sys.version.split()[0]}; "
      f"osgeo runs {gdal.VersionInfo('--version')} and PROJ {osr.GetPROJVersionMajor()}.{osr.GetPROJVersionMinor()}",
      flush=True)
print(f"  osgeo alone, in another process: ({RD_X}, {RD_Y}) in EPSG:28992 is lon {reference[0]!r}, lat {reference[1]!r}",
      flush=True)
if args.installed:
    check(hasattr(geodms, "__path__"), "geodms is an installed package, not the extension in a build folder")

work = pathlib.Path(tempfile.mkdtemp(prefix="geodms-linux-gdal-test-"))
try:
    dbf = work / "out.dbf"
    cfg = work / "linux_gdal_compatibility.dms"
    cfg.write_text(DMS.replace("@DBF@", dbf.as_posix()).replace("@X@", repr(RD_X)).replace("@Y@", repr(RD_Y)),
                   encoding="utf-8")

    print(" ", geodms.version(), flush=True)
    engine = geodms.Engine()
    with engine.load_config(str(cfg)) as config:
        config.const_root().find("/export/tab/v").asDataItem().update()
    check(dbf.is_file() and dbf.stat().st_size > 0, f"gdalwrite.vect wrote {dbf.name}")

    dataset = ogr.Open(str(dbf))
    rows = [(f.GetField("v"), f.GetField("r"), f.GetField("c")) for f in dataset.GetLayer(0)]
    dataset = None
    values = [r[0] for r in rows]
    check(values == [3.0, 6.0, 9.0], f"osgeo reads back {values}")

    # Which of the two point components holds the latitude depends on the axis order of EPSG:4326
    # in the engine (wiki: XY-order); in the Netherlands the latitude is the larger one.
    engine_points = {(min(r[1], r[2]), max(r[1], r[2])) for r in rows}
    check(len(engine_points) == 1, f"the engine converts the three equal points alike: {sorted(engine_points)}")
    lon, lat = next(iter(engine_points))
    d_lon, d_lat = lon - reference[0], lat - reference[1]
    check(abs(d_lon) < DEGREE_TOLERANCE and abs(d_lat) < DEGREE_TOLERANCE,
          f"the engine's PROJ converts it to lon {lon!r}, lat {lat!r}; difference with osgeo alone: "
          f"{d_lon:.2e} and {d_lat:.2e} degrees")

    here = osgeo_transform(osr, RD_X, RD_Y)
    check(here == reference, f"osgeo converts it here as osgeo alone does: lon {here[0]!r}, lat {here[1]!r}")

    raster = gdal.GetDriverByName("MEM").Create("", 3, 2, 1)
    band = raster.GetRasterBand(1)
    band.Fill(7)
    check(band.Checksum() != 0, "osgeo creates, fills and reads a MEM raster after the engine used GDAL")
    raster = None

    try:
        gdal.Open(str(work / "missing.tif"))
        raised = False
    except RuntimeError:
        raised = True
    check(raised, "osgeo reports a missing file as an exception, as its error handler says")
finally:
    shutil.rmtree(work, ignore_errors=True)

print("RESULT:", "PASS" if not failures else f"FAIL ({len(failures)})")
sys.exit(1 if failures else 0)
