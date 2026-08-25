"""Exercise both GDAL import orders for the GeoDMS G flavour."""

import argparse
import ctypes
import os
import pathlib
import sys


parser = argparse.ArgumentParser()
parser.add_argument("order", choices=("osgeo-first", "geodms-first"))
parser.add_argument("output_dir", type=pathlib.Path)
args = parser.parse_args()

output_dir = args.output_dir.resolve()
sys.path.insert(0, str(output_dir))
os.add_dll_directory(str(output_dir))

if args.order == "osgeo-first":
    from osgeo import gdal
    import geodms
else:
    import geodms
    from osgeo import gdal

assert geodms.__file__.endswith("geodms.cp39-win_amd64.pyd"), geodms.__file__
assert gdal.VersionInfo("VERSION_NUM") == "3010400", gdal.VersionInfo("--version")

dataset = gdal.GetDriverByName("MEM").Create("", 3, 2, 1)
assert dataset is not None
band = dataset.GetRasterBand(1)
band.Fill(7)
assert band.Checksum() != 0
dataset = None

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
kernel32.GetModuleHandleW.argtypes = (ctypes.c_wchar_p,)
kernel32.GetModuleHandleW.restype = ctypes.c_void_p
kernel32.GetModuleFileNameW.argtypes = (ctypes.c_void_p, ctypes.c_wchar_p, ctypes.c_uint32)
kernel32.GetModuleFileNameW.restype = ctypes.c_uint32
handle = kernel32.GetModuleHandleW("gdal301.dll")
assert handle
buffer = ctypes.create_unicode_buffer(32768)
assert kernel32.GetModuleFileNameW(handle, buffer, len(buffer))
print(f"{args.order}: GeoDMS {geodms.__file__}; GDAL {buffer.value}")
