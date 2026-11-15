"""Build the geodms wheels for PyPI from an msbuild output folder (#1355).

One wheel per CPython minor in python/PythonVersions.txt. Each holds the ABI-tagged binding
geodms.cpXY-win_amd64.pyd in a package geodms, the engine DLLs it loads (found and renamed by
delvewheel, so that they cannot clash with a DLL of the same name that another package already
loaded), and the resource files the engine looks for next to Rtc.dll (GetExeDir).

delvewheel puts the DLLs in geodms.libs, so GetExeDir points there, and the resources go there
as well. Each wheel also carries the licence text of every vcpkg port that one of its DLLs comes
from, in its .dist-info/licenses/third-party folder, with a table of the bundled DLLs.

Run it with a Python that has delvewheel and wheel installed:

    python tools/make-wheels.py [--bin bin/Release/x64] [--out distr] [--python 3.12;3.13;3.14]
                                [--version 20.26.0] [--vcpkg vcpkg_installed]

--bin may also be the folder of an installed setup, to make wheels of a published release; give
its --version then, since the default is the version of the source tree. The licence texts and
the port versions in the table come from --vcpkg, so that folder must hold the ports that the
release was built with.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# What the installer places next to the engine (nsi/DmsSetupScript.nsh) and the engine reads
# through GetExeDir: the prelude and the rewrite rules, the GDAL and PROJ data, the library that
# configurations include as %exeDir%/library, and the shipped examples. Not the fonts in misc/,
# which only the GUI registers, and not examples/testcases.
RESOURCE_FILES = ["prelude.dms", "RewriteExpr.lsp"]
RESOURCE_TREES = ["gdaldata", "proj4data", "library"]
RESOURCE_TOP_LEVEL_ONLY = ["examples"]

SUMMARY = "GeoDMS, an open-source platform for spatial models over large grid, vector and attribute data"


def read_version():
    text = open(os.path.join(REPO, "rtc", "dll", "src", "RtcVersionNumbers.h"), encoding="utf-8").read()
    parts = [re.search(rf"#define DMS_VERSION_{p}\s+(\d+)", text).group(1) for p in ("MAJOR", "MINOR", "PATCH")]
    return ".".join(parts)


def read_python_versions():
    return open(os.path.join(REPO, "python", "PythonVersions.txt"), encoding="utf-8").read().strip().split(";")


def run(args):
    print(">", " ".join(args), flush=True)
    subprocess.run(args, check=True)


PRELOAD_LIST = "delay-preload.txt"
WORK_MARKER = ".made-by-make-wheels"

# From this release on, Clc, Geo and Stg have a delay-load hook (rtc/dll/src/utl/DelayLoadFromOwnFolder.h)
# that loads GDAL from their own folder, geodms.libs in a wheel, and the binding's GDAL check leaves a
# gdal.dll that another package loaded alone in a wheel. The wheels of an older release, made from its
# installed setup, need the preload in INIT_PY_WITH_PRELOAD instead, and their PyPI page says what
# that check refuses (REFUSED_CASE).
FIRST_VERSION_WITH_DELAY_LOAD_HOOK = "20.29.0"

REFUSED_CASE = (
    "\nIn this release one case is refused. When a DLL called `gdal.dll` from another folder, such as "
    "that of a GeoDMS setup, is already loaded, `import geodms` stops with an error. A script that "
    "imports geodms before it loads the other GDAL does not have this problem.\n"
)

INIT_PY = '''"""GeoDMS engine for Python; see https://github.com/ObjectVision/GeoDMS/wiki/Python-bindings."""

from .geodms import *  # noqa: F401,F403
'''

INIT_PY_WITH_PRELOAD = f'''"""GeoDMS engine for Python; see https://github.com/ObjectVision/GeoDMS/wiki/Python-bindings."""


def _preload_delay_loaded():
    # The engine delay-loads GDAL. delvewheel renamed the bundled DLLs and made geodms.libs
    # searchable with os.add_dll_directory, which the delay-load helper of Windows does not use,
    # so the first GDAL call would not find the renamed DLL. A DLL that is already loaded is found
    # by its name, so the delay-loaded ones listed in {PRELOAD_LIST} are loaded here by full path.
    import ctypes
    import os
    libs = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "geodms.libs")
    listing = os.path.join(libs, "{PRELOAD_LIST}")
    if not os.path.isfile(listing):
        return
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.LoadLibraryExW.restype = ctypes.c_void_p
    kernel32.LoadLibraryExW.argtypes = (ctypes.c_wchar_p, ctypes.c_void_p, ctypes.c_uint32)
    search_dll_load_dir_and_default_dirs = 0x00000100 | 0x00001000
    with open(listing, encoding="utf-8") as f:
        names = f.read().split()
    for name in names:
        if not kernel32.LoadLibraryExW(os.path.join(libs, name), None, search_dll_load_dir_and_default_dirs):
            raise ImportError(f"geodms: cannot load {{name}} from {{libs}} (WinError {{ctypes.get_last_error()}})")


_preload_delay_loaded()
del _preload_delay_loaded

from .geodms import *  # noqa: F401,F403,E402
'''


def delay_only_dlls(libs_dir, pyd_path):
    """Names of the DLLs in libs_dir that some module imports only through a delay-load entry."""
    import pefile  # installed with delvewheel
    eager, delay = set(), set()
    modules = [pyd_path] + [os.path.join(libs_dir, f) for f in os.listdir(libs_dir) if f.lower().endswith(".dll")]
    for path in modules:
        pe = pefile.PE(path, fast_load=True)
        pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_IMPORT"],
                                               pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT"]])
        for entry in getattr(pe, "DIRECTORY_ENTRY_IMPORT", []):
            eager.add(entry.dll.decode().lower())
        for entry in getattr(pe, "DIRECTORY_ENTRY_DELAY_IMPORT", []):
            delay.add(entry.dll.decode().lower())
        pe.close()
    present = {f.lower(): f for f in os.listdir(libs_dir)}
    return sorted(present[n] for n in delay - eager if n in present)


DESCRIPTION = os.path.join(REPO, "python", "pypi-description.md")  # the text of the PyPI page


def version_key(version):
    return tuple(int(p) for p in version.split("."))


def requires_python(minors):
    # PyPI keeps one Requires-Python per release, taken from the first file uploaded, and serves
    # it for every file of that release; so all wheels of a version carry the same lower bound,
    # and the cpXY tag of each wheel is what ties it to its own Python.
    lowest = min(minors, key=version_key)
    return f">={lowest}"


def metadata(version, python_requirement, licence_files=("license.txt",)):
    description = open(DESCRIPTION, encoding="utf-8").read()
    if version_key(version) < version_key(FIRST_VERSION_WITH_DELAY_LOAD_HOOK):
        description += REFUSED_CASE
    return (
        "Metadata-Version: 2.4\n"
        "Name: geodms\n"
        f"Version: {version}\n"
        f"Summary: {SUMMARY}\n"
        "License-Expression: GPL-3.0-or-later\n"
        + "".join(f"License-File: {f}\n" for f in licence_files)
        + f"Requires-Python: {python_requirement}\n"
        "Project-URL: Homepage, https://www.geodms.nl\n"
        "Project-URL: Documentation, https://github.com/ObjectVision/GeoDMS/wiki/Python-bindings\n"
        "Project-URL: Source, https://github.com/ObjectVision/GeoDMS\n"
        "Project-URL: Issues, https://github.com/ObjectVision/GeoDMS/issues\n"
        "Classifier: Operating System :: Microsoft :: Windows\n"
        "Classifier: Programming Language :: C++\n"
        "Classifier: Programming Language :: Python :: 3\n"
        "Classifier: Topic :: Scientific/Engineering :: GIS\n"
        "Description-Content-Type: text/markdown\n"
        "\n"
        f"{description}"
    )


# The DLLs that all22.sln builds, under the GPL of license.txt. A bundled DLL that is none of these,
# not installed by a vcpkg port and not the Visual C++ runtime stops the script, so that no library
# goes out without its licence text.
GEODMS_DLLS = {"clc.dll", "geo.dll", "rtc.dll", "shv.dll", "stg.dll", "stx.dll"}
MSVC_RUNTIME = re.compile(r"(msvcp140|vcruntime140|concrt140)(_\w+)?\.dll", re.IGNORECASE)
DELVEWHEEL_SUFFIX = re.compile(r"-[0-9a-f]{32}(?=\.dll$)", re.IGNORECASE)  # the hash delvewheel adds
THIRD_PARTY = "third-party"  # the folder in .dist-info/licenses with the texts of the vcpkg ports


def vcpkg_ports(vcpkg_installed):
    """Two maps: the lower-case name of every DLL that vcpkg installed to (port, version, triplet),
    and every installed port to (version, triplet)."""
    info = os.path.join(vcpkg_installed, "vcpkg", "info")
    dlls, ports = {}, {}
    for listing in os.listdir(info):
        if not listing.endswith(".list"):
            continue
        # <port>_<version>_<triplet>.list; port names and triplets have no underscore
        port, rest = listing[: -len(".list")].split("_", 1)
        version, triplet = rest.rsplit("_", 1)
        ports[port] = (version, triplet)
        with open(os.path.join(info, listing), encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if "/bin/" in line and line.lower().endswith(".dll"):
                    dlls[os.path.basename(line).lower()] = (port, version, triplet)
    return dlls, ports


def write_table(out, header, rows):
    widths = [max(len(r[i]) for r in rows + [header]) for i in range(len(header) - 1)]
    for r in [header] + rows:
        out.write("  ".join(r[i].ljust(w) for i, w in enumerate(widths)) + "  " + r[-1] + "\n")


def add_licences(libs_dir, licences_dir, vcpkg_installed):
    """Copy the licence texts of the vcpkg ports in the wheel into licences_dir/third-party, write
    a table of them there, and return the new files as paths relative to licences_dir, for
    License-File. The ports are those that a DLL in libs_dir comes from, and those without a DLL
    of their own, whose code is compiled into the DLLs and the binding that use them."""
    dlls, ports = vcpkg_ports(vcpkg_installed)
    os.makedirs(os.path.join(licences_dir, THIRD_PARTY))
    copied = {}

    def copy_text(port, key, triplet):
        if key not in copied:
            src = os.path.join(vcpkg_installed, triplet, "share", port, "copyright")
            if not os.path.isfile(src):
                sys.exit(f"make-wheels: the vcpkg port {port} has no licence text {src}")
            shutil.copyfile(src, os.path.join(licences_dir, THIRD_PARTY, f"{key}.txt"))
            copied[key] = f"{THIRD_PARTY}/{key}.txt"
        return copied[key]

    dll_rows, unknown = [], []
    for f in sorted(os.listdir(libs_dir), key=str.lower):
        if not f.lower().endswith(".dll"):
            continue
        name = DELVEWHEEL_SUFFIX.sub("", f)
        if name.lower() in GEODMS_DLLS:
            dll_rows.append((name, "GeoDMS", "", "license.txt"))
        elif MSVC_RUNTIME.fullmatch(name):
            dll_rows.append((name, "Visual C++ runtime", "", "redistributable under the Visual Studio licence terms"))
        elif name.lower() in dlls:
            port, version, triplet = dlls[name.lower()]
            dll_rows.append((name, port, version, copy_text(port, port, triplet)))
        else:
            unknown.append(name)
    if unknown:
        sys.exit(f"make-wheels: no licence known for {', '.join(unknown)}: not a GeoDMS DLL, "
                 f"not installed by a vcpkg port in {vcpkg_installed} and not the Visual C++ runtime")

    # Ports without a DLL: header-only libraries such as Boost and pybind11, and static ones such
    # as thrift. Which of them the files of this wheel contain is not known here, so all are
    # listed, except vcpkg's own helper ports (vcpkg-*), which only run at build time. The Boost
    # libraries are separate ports with the same licence text, and are listed once.
    dll_ports = {port for port, _, _ in dlls.values()}
    static_rows = []
    for port in sorted(ports):
        if port in dll_ports or port.startswith("vcpkg-"):
            continue
        version, triplet = ports[port]
        key = "boost" if port.startswith("boost-") else port
        if key == "boost" and key in copied:
            continue
        static_rows.append((key, version, copy_text(port, key, triplet)))

    with open(os.path.join(licences_dir, THIRD_PARTY, "README.txt"), "w", encoding="utf-8", newline="\n") as out:
        out.write("The DLLs in geodms.libs, with the vcpkg port each was built from and the file in this\n"
                  "licenses folder that holds its licence text. delvewheel added a hash to each name.\n\n")
        write_table(out, ("DLL in geodms.libs", "built from", "version", "licence"), dll_rows)
        out.write("\nThe vcpkg ports without a DLL of their own (header-only or static libraries), whose code\n"
                  "is compiled into the DLLs above or into the binding where these use it. Not every one of\n"
                  "them need be in this wheel. The Boost libraries share one licence text.\n\n")
        write_table(out, ("port", "version", "licence"), static_rows)
    return [f"{THIRD_PARTY}/README.txt"] + sorted(copied.values())


def wheel_file(tag):
    return (
        "Wheel-Version: 1.0\n"
        "Generator: GeoDMS tools/make-wheels.py\n"
        "Root-Is-Purelib: false\n"
        f"Tag: {tag}\n"
    )


def copy_resources(bin_dir, libs_dir):
    for name in RESOURCE_FILES:
        shutil.copy2(os.path.join(bin_dir, name), os.path.join(libs_dir, name))
    for name in RESOURCE_TREES:
        shutil.copytree(os.path.join(bin_dir, name), os.path.join(libs_dir, name))
    for name in RESOURCE_TOP_LEVEL_ONLY:
        src = os.path.join(bin_dir, name)
        os.makedirs(os.path.join(libs_dir, name))
        for f in os.listdir(src):
            if os.path.isfile(os.path.join(src, f)):
                shutil.copy2(os.path.join(src, f), os.path.join(libs_dir, name, f))


def build_one(bin_dir, out_dir, work_dir, vcpkg_installed, version, minor, python_requirement):
    cp = "cp" + minor.replace(".", "")
    tag = f"{cp}-{cp}-win_amd64"
    pyd = f"geodms.{cp}-win_amd64.pyd"
    if not os.path.isfile(os.path.join(bin_dir, pyd)):
        raise FileNotFoundError(f"{pyd} is not in {bin_dir}")

    # 1. the wheel as it would be without its DLLs: the package and its metadata
    stage = os.path.join(work_dir, cp, "stage", f"geodms-{version}")
    pkg = os.path.join(stage, "geodms")
    info = os.path.join(stage, f"geodms-{version}.dist-info")
    os.makedirs(pkg)
    os.makedirs(os.path.join(info, "licenses"))
    shutil.copy2(os.path.join(bin_dir, pyd), os.path.join(pkg, pyd))
    preload_needed = version_key(version) < version_key(FIRST_VERSION_WITH_DELAY_LOAD_HOOK)
    with open(os.path.join(pkg, "__init__.py"), "w", encoding="utf-8", newline="\n") as f:
        f.write(INIT_PY_WITH_PRELOAD if preload_needed else INIT_PY)
    with open(os.path.join(info, "METADATA"), "w", encoding="utf-8", newline="\n") as f:
        f.write(metadata(version, python_requirement))
    with open(os.path.join(info, "WHEEL"), "w", encoding="utf-8", newline="\n") as f:
        f.write(wheel_file(tag))
    shutil.copy2(os.path.join(REPO, "license.txt"), os.path.join(info, "licenses", "license.txt"))
    bare_dir = os.path.join(work_dir, cp, "bare")
    os.makedirs(bare_dir)  # wheel pack does not create its output folder
    run([sys.executable, "-m", "wheel", "pack", stage, "-d", bare_dir])
    bare = os.path.join(bare_dir, f"geodms-{version}-{tag}.whl")

    # 2. delvewheel finds the DLLs the binding needs in the output folder, delay-loaded ones
    #    included, copies them into geodms.libs with a hash in their name, and patches the
    #    import tables and __init__.py to match
    repaired_dir = os.path.join(work_dir, cp, "repaired")
    run([sys.executable, "-m", "delvewheel", "repair", "--add-path", bin_dir, "-w", repaired_dir, bare])
    repaired = os.path.join(repaired_dir, os.path.basename(bare))

    # 3. the resources go next to the renamed Rtc.dll, where GetExeDir looks for them
    unpack_dir = os.path.join(work_dir, cp, "unpacked")
    run([sys.executable, "-m", "wheel", "unpack", repaired, "-d", unpack_dir])
    root = os.path.join(unpack_dir, f"geodms-{version}")
    libs_dir = os.path.join(root, "geodms.libs")
    copy_resources(bin_dir, libs_dir)

    #    and the licence texts of the bundled libraries go into the metadata (wheel pack below
    #    writes a new RECORD)
    info_dir = os.path.join(root, f"geodms-{version}.dist-info")
    licence_files = ["license.txt"] + add_licences(libs_dir, os.path.join(info_dir, "licenses"), vcpkg_installed)
    with open(os.path.join(info_dir, "METADATA"), "w", encoding="utf-8", newline="\n") as f:
        f.write(metadata(version, python_requirement, licence_files))

    # 4. for an engine without the delay-load hook, the list of delay-loaded DLLs that __init__.py preloads
    if preload_needed:
        preload = delay_only_dlls(libs_dir, os.path.join(root, "geodms", pyd))
        print("delay-loaded only, preloaded by __init__.py:", ", ".join(preload) or "none")
        with open(os.path.join(libs_dir, PRELOAD_LIST), "w", encoding="utf-8", newline="\n") as f:
            f.write("\n".join(preload) + "\n")
    os.makedirs(out_dir, exist_ok=True)
    run([sys.executable, "-m", "wheel", "pack", root, "-d", out_dir])
    return os.path.join(out_dir, os.path.basename(bare))


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--bin", default=os.path.join(REPO, "bin", "Release", "x64"), help="msbuild output folder")
    parser.add_argument("--out", default=os.path.join(REPO, "distr"), help="folder for the finished wheels")
    parser.add_argument("--python", default=";".join(read_python_versions()), help="';'-separated CPython minors")
    parser.add_argument("--version", default=read_version(), help="package version (default: RtcVersionNumbers.h)")
    parser.add_argument("--work", default="", help="work folder (default: a temporary folder, removed afterwards)")
    parser.add_argument("--vcpkg", default=os.path.join(REPO, "vcpkg_installed"),
                        help="vcpkg_installed folder with the licence texts of the bundled libraries")
    args = parser.parse_args()

    work_dir = args.work or tempfile.mkdtemp(prefix="geodms-wheels-")
    if args.work:
        # a --work folder is emptied before use, but only an empty one or one this script made,
        # recognised by its marker file, so that a wrong path cannot wipe someone's folder
        if os.path.isdir(work_dir) and os.listdir(work_dir):
            if not os.path.isfile(os.path.join(work_dir, WORK_MARKER)):
                sys.exit(f"make-wheels: {work_dir} is not empty and was not made by this script; give another --work")
            shutil.rmtree(work_dir)
        os.makedirs(work_dir, exist_ok=True)
        open(os.path.join(work_dir, WORK_MARKER), "w").close()
    try:
        minors = args.python.split(";")
        for minor in minors:
            print("wrote", build_one(os.path.abspath(args.bin), os.path.abspath(args.out), work_dir,
                                     os.path.abspath(args.vcpkg), args.version, minor, requires_python(minors)))
    finally:
        if not args.work:
            shutil.rmtree(work_dir, ignore_errors=True)


if __name__ == "__main__":
    main()
