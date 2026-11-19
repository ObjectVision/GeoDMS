"""Exercise both GDAL import orders for the GeoDMS G flavour.

    python GlobioCompatibility.py <osgeo-first|geodms-first> <output_dir>
    python GlobioCompatibility.py <osgeo-first|geodms-first> --installed

With an output folder (bin_GLOBIO or an installed .g setup), the binding is imported from that
folder, and the engine must share one gdal301.dll with osgeo: the copy next to the binding is
byte-for-byte that of the GLOBIO environment, and the binding refuses any other.

With --installed, the geodms-globio wheel installed in this Python is tested (#1355), and nothing
is added to sys.path or the DLL search path. The wheel carries its own copy of gdal301.dll under a
name with a hash, so osgeo's gdal301.dll and the wheel's copy must both be loaded, report the same
GDAL version, and work in the same process.

In both cases the engine converts a point through PROJ and writes a dbf through GDAL, and osgeo
creates a raster afterwards, so that both GDALs have been used in this process. Exit code 0 when
every check passes.
"""

import argparse
import ctypes
import ctypes.wintypes as wt
import os
import pathlib
import re
import shutil
import sys
import tempfile


parser = argparse.ArgumentParser()
parser.add_argument("order", choices=("osgeo-first", "geodms-first"))
parser.add_argument("output_dir", type=pathlib.Path, nargs="?")
parser.add_argument("--installed", action="store_true", help="test the installed geodms-globio wheel")
args = parser.parse_args()
if args.installed == (args.output_dir is not None):
    parser.error("give either an output folder or --installed")

if not args.installed:
    output_dir = args.output_dir.resolve()
    sys.path.insert(0, str(output_dir))
    os.add_dll_directory(str(output_dir))

if args.order == "osgeo-first":
    from osgeo import gdal
    import geodms
else:
    import geodms
    from osgeo import gdal

# The GDAL release of the GLOBIO environment (vcpkg-globio/environment.yml), as GDALVersionInfo
# gives it.
GLOBIO_GDAL_VERSION_NUM = "3010400"
GDAL_DLL = re.compile(r"gdal301(-[0-9a-f]{32})?\.dll", re.IGNORECASE)  # with the hash delvewheel adds

# Where the RD point (155000, 463000) lies in WGS84, as the regular wheels with PROJ 9 give it. The
# tolerance, a tenth of a millidegree or about 10 m, leaves room for the PROJ release of the GLOBIO
# environment and still tells a working conversion from a failed one.
EXPECTED_LAT, EXPECTED_LON, TOLERANCE_DEG = 52.155172, 5.387204, 1e-4

DMS = """container globio_compatibility
{
	unit<dpoint> rdc   : SpatialReference = "EPSG:28992";
	unit<dpoint> wgs84 : SpatialReference = "EPSG:4326";
	parameter<rdc>     p     := point_xy(155000.0, 463000.0, rdc);
	parameter<wgs84>   q     := convert(p, wgs84);
	parameter<float64> q_row := pointrow(q);
	parameter<float64> q_col := pointcol(q);

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
    print(("  OK: " if ok else "  FAIL: ") + what)
    if not ok:
        failures.append(what)


def loaded_modules():
    """Full paths of every DLL loaded in this process."""
    psapi = ctypes.WinDLL("psapi")
    kernel32 = ctypes.WinDLL("kernel32")
    kernel32.GetCurrentProcess.restype = wt.HANDLE
    psapi.EnumProcessModulesEx.argtypes = [wt.HANDLE, ctypes.POINTER(wt.HMODULE), wt.DWORD,
                                           ctypes.POINTER(wt.DWORD), wt.DWORD]
    psapi.GetModuleFileNameExW.argtypes = [wt.HANDLE, wt.HMODULE, wt.LPWSTR, wt.DWORD]
    process = kernel32.GetCurrentProcess()
    needed = wt.DWORD()
    handles = (wt.HMODULE * 4096)()
    list_modules_all = 3
    psapi.EnumProcessModulesEx(process, handles, ctypes.sizeof(handles), ctypes.byref(needed), list_modules_all)
    names = []
    buffer = ctypes.create_unicode_buffer(32768)
    for i in range(needed.value // ctypes.sizeof(wt.HMODULE)):
        if psapi.GetModuleFileNameExW(process, handles[i], buffer, len(buffer)):
            names.append(buffer.value)
    return names


def gdal_version_num(dll_path):
    """GDALVersionInfo("VERSION_NUM") of a loaded GDAL DLL; loading it by its path again only
    raises its reference count."""
    lib = ctypes.CDLL(dll_path)
    lib.GDALVersionInfo.restype = ctypes.c_char_p
    lib.GDALVersionInfo.argtypes = (ctypes.c_char_p,)
    return lib.GDALVersionInfo(b"VERSION_NUM").decode()


print(f"{args.order}, {'installed wheel' if args.installed else output_dir}; Python {sys.version.split()[0]}")
extension = sys.modules.get("geodms.geodms", geodms)  # the package loads the extension as geodms.geodms
check(os.path.basename(extension.__file__) == "geodms.cp39-win_amd64.pyd", f"extension {extension.__file__}")
if args.installed:
    check(hasattr(geodms, "__path__"), "geodms is an installed package, not the extension in a build folder")
check(gdal.VersionInfo("VERSION_NUM") == GLOBIO_GDAL_VERSION_NUM, f"osgeo runs GDAL {gdal.VersionInfo('--version')}")

work = pathlib.Path(tempfile.mkdtemp(prefix="geodms-globio-test-"))
try:
    dbf = work / "out.dbf"
    cfg = work / "globio_compatibility.dms"
    cfg.write_text(DMS.replace("@DBF@", dbf.as_posix()), encoding="utf-8")

    print(geodms.version())
    engine = geodms.Engine()
    config = engine.load_config(str(cfg))
    root = config.const_root()
    row = root.find("/q_row").asDataItem().get_value_as_float(0)
    col = root.find("/q_col").asDataItem().get_value_as_float(0)
    print(f"  RD (155000, 463000) in WGS84: {row:.6f}, {col:.6f}")
    lat, lon = max(row, col), min(row, col)
    check(abs(lat - EXPECTED_LAT) < TOLERANCE_DEG and abs(lon - EXPECTED_LON) < TOLERANCE_DEG,
          "PROJ converts the RD point")
    root.find("/export/tab/v").asDataItem().update()
    check(dbf.is_file() and dbf.stat().st_size > 0, f"gdalwrite.vect wrote {dbf.name}")

    dataset = gdal.GetDriverByName("MEM").Create("", 3, 2, 1)
    check(dataset is not None, "osgeo creates a MEM raster after the engine used GDAL")
    band = dataset.GetRasterBand(1)
    band.Fill(7)
    check(band.Checksum() != 0, "osgeo fills and reads that raster")
    dataset = None
finally:
    shutil.rmtree(work, ignore_errors=True)

gdal_dlls = [m for m in loaded_modules() if GDAL_DLL.fullmatch(os.path.basename(m))]
for m in gdal_dlls:
    print(f"  loaded: {m}, GDAL {gdal_version_num(m)}")
plain = [m for m in gdal_dlls if os.path.basename(m).lower() == "gdal301.dll"]
renamed = [m for m in gdal_dlls if m not in plain]
check(len(plain) == 1, "one gdal301.dll is loaded, the one osgeo uses")
if args.installed:
    check(len(renamed) == 1, "the wheel's renamed copy of gdal301.dll is loaded as well")
    check(all(gdal_version_num(m) == GLOBIO_GDAL_VERSION_NUM for m in gdal_dlls), "both report the GLOBIO GDAL version")
else:
    check(not renamed, "no other copy of GDAL is loaded: the engine uses osgeo's")

print("RESULT:", "PASS" if not failures else f"FAIL ({len(failures)})")
sys.exit(1 if failures else 0)
