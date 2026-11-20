"""Exercise the GDAL import orders of the Linux module next to the GDAL of osgeo (#1381).

    python LinuxGdalCompatibility.py <order> <output_dir>
    python LinuxGdalCompatibility.py <order> --installed

order is one of
    osgeo-global-first         osgeo first, loaded with RTLD_GLOBAL, so its GDAL is in the global
                               symbol scope when geodms is imported, as in a program that links GDAL
    geodms-first-osgeo-global  geodms first, then osgeo with RTLD_GLOBAL
    osgeo-first                osgeo first, loaded the usual way (RTLD_LOCAL)
    geodms-first               geodms first, then osgeo the usual way

With an output folder (build/linux-x64-release/bin or an installed .l setup), the module is imported
from that folder; with --installed, the geodms wheel installed in this Python is tested. osgeo is the
one this Python finds, such as Ubuntu's python3-gdal in a venv made with --system-site-packages.

The engine links GDAL into its own libraries and keeps their symbols to itself, so in every order it
must call its own GDAL and osgeo its own: the engine writes a dbf through GDAL, osgeo reads it back,
and osgeo creates a raster afterwards. Before #1381 the first order crashed at the write. Exit code
0 when every check passes.
"""

import argparse
import os
import pathlib
import shutil
import sys
import tempfile


ORDERS = ("osgeo-global-first", "geodms-first-osgeo-global", "osgeo-first", "geodms-first")

parser = argparse.ArgumentParser()
parser.add_argument("order", choices=ORDERS)
parser.add_argument("output_dir", type=pathlib.Path, nargs="?")
parser.add_argument("--installed", action="store_true", help="test the installed geodms wheel")
args = parser.parse_args()
if args.installed == (args.output_dir is not None):
    parser.error("give either an output folder or --installed")

if not args.installed:
    sys.path.insert(0, str(args.output_dir.resolve()))


def import_osgeo(into_global_scope):
    flags = sys.getdlopenflags()
    if into_global_scope:
        sys.setdlopenflags(os.RTLD_NOW | os.RTLD_GLOBAL)
    try:
        from osgeo import gdal, ogr
    finally:
        sys.setdlopenflags(flags)
    gdal.UseExceptions()
    return gdal, ogr


if args.order.startswith("osgeo"):
    gdal, ogr = import_osgeo(args.order == "osgeo-global-first")
    import geodms
else:
    import geodms
    gdal, ogr = import_osgeo(args.order == "geodms-first-osgeo-global")

DMS = """container linux_gdal_compatibility
{
	unit<uint32> obj : nrofrows = 3
	{
		attribute<float64> v : [3.0, 6.0, 9.0];
	}
	container export
	{
		unit<uint32> tab := obj
		,	StorageName = "@DBF@"
		,	StorageType = "gdalwrite.vect"
		{
			attribute<float64> v := obj/v;
		}
	}
}
"""

failures = []


def check(ok, what):
    print(("  OK: " if ok else "  FAIL: ") + what, flush=True)
    if not ok:
        failures.append(what)


print(f"{args.order}, {'installed wheel' if args.installed else args.output_dir}; Python {sys.version.split()[0]}; "
      f"osgeo runs GDAL {gdal.VersionInfo('--version')}", flush=True)
if args.installed:
    check(hasattr(geodms, "__path__"), "geodms is an installed package, not the extension in a build folder")

work = pathlib.Path(tempfile.mkdtemp(prefix="geodms-linux-gdal-test-"))
try:
    dbf = work / "out.dbf"
    cfg = work / "linux_gdal_compatibility.dms"
    cfg.write_text(DMS.replace("@DBF@", dbf.as_posix()), encoding="utf-8")

    print(geodms.version(), flush=True)
    engine = geodms.Engine()
    with engine.load_config(str(cfg)) as config:
        config.const_root().find("/export/tab/v").asDataItem().update()
    check(dbf.is_file() and dbf.stat().st_size > 0, f"gdalwrite.vect wrote {dbf.name}")

    dataset = ogr.Open(str(dbf))
    values = [feature.GetField("v") for feature in dataset.GetLayer(0)]
    dataset = None
    check(values == [3.0, 6.0, 9.0], f"osgeo reads back {values}")

    raster = gdal.GetDriverByName("MEM").Create("", 3, 2, 1)
    band = raster.GetRasterBand(1)
    band.Fill(7)
    check(band.Checksum() != 0, "osgeo creates, fills and reads a MEM raster after the engine used GDAL")
    raster = None
finally:
    shutil.rmtree(work, ignore_errors=True)

print("RESULT:", "PASS" if not failures else f"FAIL ({len(failures)})")
sys.exit(1 if failures else 0)
