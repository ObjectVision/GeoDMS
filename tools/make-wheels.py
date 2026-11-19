"""Build the geodms wheels for PyPI from a build output folder (#1355).

One wheel per CPython minor in python/PythonVersions.txt. Each holds the ABI-tagged binding
geodms.cpXY-win_amd64.pyd in a package geodms, the engine DLLs it loads (found and renamed by
delvewheel, so that they cannot clash with a DLL of the same name that another package already
loaded), and the resource files the engine looks for next to Rtc.dll (GetExeDir).

delvewheel puts the DLLs in <distribution>.libs (geodms.libs), so GetExeDir points there, and the
resources go there as well. Each wheel also carries the licence text of every vcpkg port that one
of its DLLs comes from, in its .dist-info/licenses/third-party folder, with a table of the bundled
DLLs.

With --flavour g it makes the wheel of the GLOBIO flavour instead: the distribution geodms-globio,
whose package is also called geodms, from bin_GLOBIO/Release/x64, for the CPython minor of
python/PythonVersionsGlobio.txt. That build takes GDAL, PROJ, GEOS and the libraries they use from
the GLOBIO conda environment (vcpkg-globio/environment.yml), so the wheel carries copies of those,
renamed like the others, and their licence texts come from that environment (--conda, default
GLOBIO_ENV_ROOT), as in the .g setup (tools/setup_licences.py).

On Linux, with --flavour l (the default there), it makes the Linux wheels of the distribution
geodms from the CMake output folder build/linux-x64-release/bin: per CPython minor the binding
geodms.cpython-3XY-x86_64-linux-gnu.so, and in geodms.libs the libDm*.so it loads, stripped and with
an rpath of $ORIGIN, beside the same resources. auditwheel then adds the system libraries those need
that a manylinux platform does not promise (unixODBC), renamed, and gives the wheel its manylinux
tag. The engine libraries keep their names: renaming them would have patchelf copy their large
dynamic symbol tables into the file once more. The licence texts are those of the vcpkg ports,
which the static triplet x64-linux compiles into the libraries, and those of the Debian packages
the added system libraries come from, as in the .l setup.

Run it with a Python that has the packages of python/wheel-tools.txt installed:

    python tools/make-wheels.py [--flavour m|g|l] [--bin bin/Release/x64] [--out distr]
                                [--python 3.12;3.13;3.14] [--version 20.26.0]
                                [--vcpkg vcpkg_installed] [--conda C:\\dev\\globio4env]

--bin may also be the folder of an installed setup, to make wheels of a published release; give
its --version then, since the default is the version of the source tree. The licence texts and
the port versions in the table come from --vcpkg (and --conda), so those folders must hold the
ports and packages that the release was built with.
"""
import argparse
import glob
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

# Per flavour: the distribution name on PyPI, the platform it is made on (sys.platform), the build
# output, the vcpkg tree, the list of CPython minors, the text of the PyPI page and its one-line
# summary, and whether the spatial libraries come from the GLOBIO conda environment. The package
# that all install is called geodms; m and l are the Windows and Linux wheels of one distribution.
SUMMARY = "GeoDMS, an open-source platform for spatial models over large grid, vector and attribute data"
FLAVOURS = {
    "m": {
        "name": "geodms",
        "platform": "win32",
        "bin": os.path.join("bin", "Release", "x64"),
        "vcpkg": "vcpkg_installed",
        "pythons": os.path.join("python", "PythonVersions.txt"),
        "description": os.path.join("python", "pypi-description.md"),
        "summary": SUMMARY,
        "conda": False,
    },
    "g": {
        "name": "geodms-globio",
        "platform": "win32",
        "bin": os.path.join("bin_GLOBIO", "Release", "x64"),
        "vcpkg": "vcpkg_installed_GLOBIO",
        "pythons": os.path.join("python", "PythonVersionsGlobio.txt"),
        "description": os.path.join("python", "pypi-description-globio.md"),
        "summary": "GeoDMS built against the GDAL, PROJ and GEOS of the GLOBIO 4 conda environment",
        "conda": True,
    },
    "l": {
        "name": "geodms",
        "platform": "linux",
        "bin": os.path.join("build", "linux-x64-release", "bin"),
        "vcpkg": os.path.join("build", "linux-x64-release", "vcpkg_installed"),
        "pythons": os.path.join("python", "PythonVersions.txt"),
        "description": os.path.join("python", "pypi-description.md"),
        "summary": SUMMARY,
        "conda": False,
    },
}
OS_CLASSIFIER = {"win32": "Microsoft :: Windows", "linux": "POSIX :: Linux"}

# The largest wheel this script lets through: 100 MiB, the upload limit per file that PyPI sets for
# a project outside an organisation. A larger wheel stops the script, because the upload of the
# release could fail on it. The projects of an organisation, as geodms is, get twice that by default
# (Project.upload_limit_size in warehouse), and PyPI can grant a project more. Raise this value
# once pypi.org shows a higher limit for every project that the wheels of a flavour go to.
PYPI_FILE_LIMIT_BYTES = 100 * 1024 * 1024

# The processor of the Linux build (preset linux-x64-release), as the module names and tags spell it.
LINUX_ARCH = "x86_64"

# The conda packages whose versions the PyPI page of the GLOBIO flavour names, by the token in
# python/pypi-description-globio.md that stands for each.
CONDA_VERSION_TOKENS = {"@GDAL_VERSION@": "libgdal", "@PROJ_VERSION@": "proj", "@GEOS_VERSION@": "geos",
                        "@PYTHON_VERSION@": "python"}


def dist_file_name(name):
    """The distribution name as wheel file names and the .dist-info and .libs folders spell it."""
    return re.sub(r"[-_.]+", "_", name).lower()


def read_version():
    text = open(os.path.join(REPO, "rtc", "dll", "src", "RtcVersionNumbers.h"), encoding="utf-8").read()
    parts = [re.search(rf"#define DMS_VERSION_{p}\s+(\d+)", text).group(1) for p in ("MAJOR", "MINOR", "PATCH")]
    return ".".join(parts)


def read_python_versions(flavour):
    return open(os.path.join(REPO, flavour["pythons"]), encoding="utf-8").read().strip().split(";")


def run(args, env=None):
    print(">", " ".join(args), flush=True)
    subprocess.run(args, check=True, env=env)


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


def version_key(version):
    return tuple(int(p) for p in version.split("."))


def requires_python(flavour, minors):
    # PyPI keeps one Requires-Python per release, taken from the first file uploaded, and serves
    # it for every file of that release; so all wheels of a version carry the same lower bound,
    # and the cpXY tag of each wheel is what ties it to its own Python. The GLOBIO flavour has a
    # project of its own and one minor, that of the GLOBIO environment, so it names that minor.
    if flavour["conda"] and len(minors) == 1:
        return f"=={minors[0]}.*"
    lowest = min(minors, key=version_key)
    return f">={lowest}"


def description_text(flavour, version, conda):
    description = open(os.path.join(REPO, flavour["description"]), encoding="utf-8").read()
    if conda is not None:
        for token, package in CONDA_VERSION_TOKENS.items():
            if package not in conda.packages:
                sys.exit(f"make-wheels: the conda environment {conda.root} has no package {package}")
            description = description.replace(token, conda.packages[package][0])
    unreplaced = re.findall(r"@[A-Z_]+@", description)
    if unreplaced:
        sys.exit(f"make-wheels: {flavour['description']} has tokens that nothing replaces: {', '.join(unreplaced)}")
    if (flavour["platform"] == "win32" and not flavour["conda"]
            and version_key(version) < version_key(FIRST_VERSION_WITH_DELAY_LOAD_HOOK)):
        description += REFUSED_CASE
    return description


def metadata(flavour, version, python_requirement, conda, licence_files=("license.txt",)):
    description = description_text(flavour, version, conda)
    return (
        "Metadata-Version: 2.4\n"
        f"Name: {flavour['name']}\n"
        f"Version: {version}\n"
        f"Summary: {flavour['summary']}\n"
        "License-Expression: GPL-3.0-or-later\n"
        + "".join(f"License-File: {f}\n" for f in licence_files)
        + f"Requires-Python: {python_requirement}\n"
        "Project-URL: Homepage, https://www.geodms.nl\n"
        "Project-URL: Documentation, https://github.com/ObjectVision/GeoDMS/wiki/Python-bindings\n"
        "Project-URL: Source, https://github.com/ObjectVision/GeoDMS\n"
        "Project-URL: Issues, https://github.com/ObjectVision/GeoDMS/issues\n"
        f"Classifier: Operating System :: {OS_CLASSIFIER[flavour['platform']]}\n"
        "Classifier: Programming Language :: C++\n"
        "Classifier: Programming Language :: Python :: 3\n"
        "Classifier: Topic :: Scientific/Engineering :: GIS\n"
        "Description-Content-Type: text/markdown\n"
        "\n"
        f"{description}"
    )


# The names of the GeoDMS DLLs (under the GPL of license.txt), the pattern of the Visual C++ runtime
# and the reader of the vcpkg tree are those of tools/setup_licences.py, which writes the licence
# texts of the setups (#1371), so that a wheel and a setup map a DLL to its source in the same way.
# A bundled DLL that is no GeoDMS DLL, not installed by a vcpkg port and not the Visual C++ runtime
# stops the script, so that no library goes out without its licence text. Importing it writes no
# tools/__pycache__ into the working copy.
sys.dont_write_bytecode = True
from setup_licences import (GEODMS_DLLS, GEODMS_OTHER, MSVC_RUNTIME, SHARED_LIBRARY,  # noqa: E402
                            CondaEnv, DebianSystem, Problems, VcpkgTree, sha256, vcpkg_ports, write_table)

DELVEWHEEL_SUFFIX = re.compile(r"-[0-9a-f]{32}(?=\.dll$)", re.IGNORECASE)  # the hash delvewheel adds
THIRD_PARTY = "third-party"  # the folder in .dist-info/licenses with the texts of the vcpkg ports


def add_licences(libs_dir, licences_dir, vcpkg_installed, bin_dir, conda=None):
    """Copy the licence texts of the vcpkg ports and conda packages in the wheel into
    licences_dir/third-party, write a table of them there, and return the new files as paths
    relative to licences_dir, for License-File. The ports are those that a DLL in libs_dir comes
    from, and those without a DLL of their own, whose code is compiled into the DLLs and the
    binding that use them. A DLL whose name both a vcpkg port and a conda package install is
    assigned by the copy in bin_dir, the folder the wheel was made from."""
    dlls, ports = vcpkg_ports(vcpkg_installed)
    libs_name = os.path.basename(libs_dir)
    os.makedirs(os.path.join(licences_dir, THIRD_PARTY))
    copied = {}
    conda_folders, conda_texts = set(), []
    problems = Problems()

    def copy_conda_text(package):
        # the licence of a conda package can be several files (GDAL's covers its parts), so each
        # package gets a folder, and every file in it becomes a License-File
        rel = f"{THIRD_PARTY}/conda/{package}"
        if rel not in conda_folders:
            conda_folders.add(rel)
            src = conda.licence_source(package, problems)
            if src is None:
                return rel
            dst = os.path.join(licences_dir, THIRD_PARTY, "conda", package)
            if isinstance(src, list):
                os.makedirs(dst)
                for f in src:
                    shutil.copyfile(f, os.path.join(dst, os.path.basename(f)))
            else:
                shutil.copytree(src, dst)
            for dirpath, _, files in os.walk(dst):
                for f in files:
                    conda_texts.append(os.path.relpath(os.path.join(dirpath, f), licences_dir).replace(os.sep, "/"))
        return rel

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
        lower = name.lower()
        in_vcpkg = lower in dlls
        in_conda = conda is not None and lower in conda.dlls
        if in_vcpkg and in_conda:
            in_conda = sha256(conda.dlls[lower][1]) == sha256(os.path.join(bin_dir, name))
            in_vcpkg = not in_conda
        if lower in GEODMS_DLLS:
            dll_rows.append((name, "GeoDMS", "", "license.txt"))
        elif MSVC_RUNTIME.fullmatch(name):
            dll_rows.append((name, "Visual C++ runtime", "", "redistributable under the Visual Studio licence terms"))
        elif in_conda:
            package = conda.dlls[lower][0]
            dll_rows.append((name, f"conda {package}", conda.packages[package][0], copy_conda_text(package) + "/"))
        elif in_vcpkg:
            port, version, triplet = dlls[lower]
            dll_rows.append((name, port, version, copy_text(port, port, triplet)))
        else:
            unknown.append(name)
    if unknown:
        sources = f"a vcpkg port in {vcpkg_installed}" + (f" or a conda package in {conda.root}" if conda else "")
        sys.exit(f"make-wheels: no licence known for {', '.join(unknown)}: not a GeoDMS DLL, "
                 f"not installed by {sources} and not the Visual C++ runtime")
    problems.stop_if_any("copying the licence texts of the conda packages")

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
        if conda is None:
            out.write(f"The DLLs in {libs_name}, with the vcpkg port each was built from and the file in this\n"
                      "licenses folder that holds its licence text. delvewheel added a hash to each name.\n\n")
        else:
            out.write(f"The DLLs in {libs_name}, with the vcpkg port or the package of the GLOBIO conda environment\n"
                      "each comes from, and the file or folder in this licenses folder that holds its licence\n"
                      "text. delvewheel added a hash to each name.\n\n")
        write_table(out, (f"DLL in {libs_name}", "comes from" if conda else "built from", "version", "licence"), dll_rows)
        out.write("\nThe vcpkg ports without a DLL of their own (header-only or static libraries), whose code\n"
                  "is compiled into the DLLs above or into the binding where these use it. Not every one of\n"
                  "them need be in this wheel. The Boost libraries share one licence text.\n\n")
        write_table(out, ("port", "version", "licence"), static_rows)
    return [f"{THIRD_PARTY}/README.txt"] + sorted(copied.values()) + sorted(conda_texts)


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


def build_one(flavour, bin_dir, out_dir, work_dir, vcpkg_installed, conda, version, minor, python_requirement):
    cp = "cp" + minor.replace(".", "")
    tag = f"{cp}-{cp}-win_amd64"
    pyd = f"geodms.{cp}-win_amd64.pyd"
    if not os.path.isfile(os.path.join(bin_dir, pyd)):
        raise FileNotFoundError(f"{pyd} is not in {bin_dir}")
    dist = dist_file_name(flavour["name"])  # geodms or geodms_globio; the package is geodms in both

    # 1. the wheel as it would be without its DLLs: the package and its metadata
    stage = os.path.join(work_dir, cp, "stage", f"{dist}-{version}")
    pkg = os.path.join(stage, "geodms")
    info = os.path.join(stage, f"{dist}-{version}.dist-info")
    os.makedirs(pkg)
    os.makedirs(os.path.join(info, "licenses"))
    shutil.copy2(os.path.join(bin_dir, pyd), os.path.join(pkg, pyd))
    preload_needed = version_key(version) < version_key(FIRST_VERSION_WITH_DELAY_LOAD_HOOK)
    with open(os.path.join(pkg, "__init__.py"), "w", encoding="utf-8", newline="\n") as f:
        f.write(INIT_PY_WITH_PRELOAD if preload_needed else INIT_PY)
    with open(os.path.join(info, "METADATA"), "w", encoding="utf-8", newline="\n") as f:
        f.write(metadata(flavour, version, python_requirement, conda))
    with open(os.path.join(info, "WHEEL"), "w", encoding="utf-8", newline="\n") as f:
        f.write(wheel_file(tag))
    shutil.copy2(os.path.join(REPO, "license.txt"), os.path.join(info, "licenses", "license.txt"))
    bare_dir = os.path.join(work_dir, cp, "bare")
    os.makedirs(bare_dir)  # wheel pack does not create its output folder
    run([sys.executable, "-m", "wheel", "pack", stage, "-d", bare_dir])
    bare = os.path.join(bare_dir, f"{dist}-{version}-{tag}.whl")

    # 2. delvewheel finds the DLLs the binding needs in the output folder, delay-loaded ones
    #    included, copies them into <dist>.libs with a hash in their name, and patches the
    #    import tables and __init__.py to match
    repaired_dir = os.path.join(work_dir, cp, "repaired")
    run([sys.executable, "-m", "delvewheel", "repair", "--add-path", bin_dir, "-w", repaired_dir, bare])
    repaired = os.path.join(repaired_dir, os.path.basename(bare))

    # 3. the resources go next to the renamed Rtc.dll, where GetExeDir looks for them
    unpack_dir = os.path.join(work_dir, cp, "unpacked")
    run([sys.executable, "-m", "wheel", "unpack", repaired, "-d", unpack_dir])
    root = os.path.join(unpack_dir, f"{dist}-{version}")
    libs_dir = os.path.join(root, f"{dist}.libs")
    if not os.path.isdir(libs_dir):
        sys.exit(f"make-wheels: delvewheel wrote no {dist}.libs folder in {repaired}")
    copy_resources(bin_dir, libs_dir)

    #    and the licence texts of the bundled libraries go into the metadata (wheel pack below
    #    writes a new RECORD)
    info_dir = os.path.join(root, f"{dist}-{version}.dist-info")
    licence_files = ["license.txt"] + add_licences(libs_dir, os.path.join(info_dir, "licenses"), vcpkg_installed,
                                                   bin_dir, conda)
    with open(os.path.join(info_dir, "METADATA"), "w", encoding="utf-8", newline="\n") as f:
        f.write(metadata(flavour, version, python_requirement, conda, licence_files))

    # 4. for an engine without the delay-load hook, the list of delay-loaded DLLs that __init__.py preloads
    if preload_needed:
        preload = delay_only_dlls(libs_dir, os.path.join(root, "geodms", pyd))
        print("delay-loaded only, preloaded by __init__.py:", ", ".join(preload) or "none")
        with open(os.path.join(libs_dir, PRELOAD_LIST), "w", encoding="utf-8", newline="\n") as f:
            f.write("\n".join(preload) + "\n")
    os.makedirs(out_dir, exist_ok=True)
    run([sys.executable, "-m", "wheel", "pack", root, "-d", out_dir])
    return os.path.join(out_dir, os.path.basename(bare))


# ---------------------------------------------------------------------------------------------
# Linux (--flavour l)
# ---------------------------------------------------------------------------------------------

# The name auditwheel gives a system library that it adds to a wheel: eight hex digits before .so
AUDITWHEEL_SUFFIX = re.compile(r"(.+?)-[0-9a-f]{8}(\.so(\.\d+)*)")


def tool_path():
    """PATH with the folder of the tool venv first, where pip put patchelf and auditwheel."""
    return os.pathsep.join([os.path.dirname(sys.executable), os.environ.get("PATH", "")])


def tool(name):
    """A program of the tool venv (patchelf), else one on PATH (strip, from binutils)."""
    found = shutil.which(name, path=tool_path())
    if not found:
        sys.exit(f"make-wheels: {name} not found; install python/wheel-tools.txt, and binutils for strip")
    return found


def elf_query(option, path):
    return subprocess.run([tool("patchelf"), option, path], check=True, capture_output=True, text=True).stdout.split()


def engine_libraries(bin_dir, module):
    """The libraries in bin_dir that the module loads, directly or through each other: the libDm*.so
    of the engine it uses, so not libDmShv.so, the map and table views that only the GUI loads."""
    found, todo = [], [module]
    while todo:
        for name in elf_query("--print-needed", todo.pop()):
            path = os.path.join(bin_dir, name)
            if name not in found and os.path.isfile(path):
                found.append(name)
                todo.append(path)
    return sorted(found)


def copy_elf(src, dst, rpath):
    """Copy a library or module without the symbols that only a debugger reads, a quarter of the
    libDm*.so, and give it the rpath when it has one: a library without an rpath (libDmRtc.so) needs
    nothing beside it, and adding an entry would have patchelf copy its dynamic symbol table."""
    run([tool("strip"), "--strip-unneeded", "-o", dst, src])
    if elf_query("--print-rpath", dst):
        run([tool("patchelf"), "--set-rpath", rpath, dst])


def add_licences_linux(libs_dir, licences_dir, vcpkg_installed):
    """Copy the licence texts of the libraries in a Linux wheel into licences_dir/third-party: those
    of the Debian packages that the system libraries added by auditwheel come from, and those of
    the vcpkg ports, which the static triplet x64-linux compiles into the libDm*.so. Write a table
    of them there, and return the new files as paths relative to licences_dir, for License-File."""
    libs_name = os.path.basename(libs_dir)
    third = os.path.join(licences_dir, THIRD_PARTY)
    os.makedirs(third)
    debian, problems = DebianSystem(), Problems()
    texts = {}  # the licence path in third-party, and the file it is copied from
    lib_rows = []
    for f in sorted(os.listdir(libs_dir)):
        if not SHARED_LIBRARY.fullmatch(f):
            continue
        if GEODMS_OTHER.fullmatch(f):
            lib_rows.append((f, "GeoDMS", "", "license.txt"))
            continue
        m = AUDITWHEEL_SUFFIX.fullmatch(f)
        candidates = debian.candidate(m.group(1) + m.group(2)) if m else []
        if not candidates:
            problems.append(f"no licence known for {f}: not a GeoDMS library, and not the copy of a library "
                            "of a Debian package of this machine")
            continue
        c = candidates[0]
        texts[c.licence] = c.copy_from
        lib_rows.append((f, c.source, c.version, f"{THIRD_PARTY}/{c.licence}".replace(os.sep, "/")))
    port_rows = []
    for key, version, licence, copy_from in VcpkgTree(vcpkg_installed).static_ports():
        texts[licence] = copy_from
        port_rows.append((key, version, f"{THIRD_PARTY}/{licence}".replace(os.sep, "/")))
    for licence, src in sorted(texts.items()):
        if not os.path.isfile(src):
            problems.append(f"the licence text {src} does not exist")
            continue
        dst = os.path.join(third, licence)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copyfile(src, dst)
    problems.stop_if_any("collecting the licence texts of the Linux wheel")

    with open(os.path.join(third, "README.txt"), "w", encoding="utf-8", newline="\n") as out:
        out.write(f"The shared libraries in {libs_name}, with where each comes from and the file in this\n"
                  "licenses folder that holds its licence text. auditwheel added a hash to the names of\n"
                  "the system libraries it bundled.\n\n")
        write_table(out, (f"library in {libs_name}", "comes from", "version", "licence"), lib_rows)
        out.write("\nThe vcpkg ports, whose code the static triplet x64-linux compiles into the GeoDMS\n"
                  "libraries above and into the binding where these use it. Not every one of them need\n"
                  "be in this wheel. The Boost libraries share one licence text.\n\n")
        write_table(out, ("port", "version", "licence"), port_rows)
    return [f"{THIRD_PARTY}/README.txt"] + sorted(f"{THIRD_PARTY}/{licence}".replace(os.sep, "/") for licence in texts)


def build_one_linux(flavour, bin_dir, out_dir, work_dir, vcpkg_installed, version, minor, python_requirement):
    abi = minor.replace(".", "")
    cp = "cp" + abi
    module = f"geodms.cpython-{abi}-{LINUX_ARCH}-linux-gnu.so"
    if not os.path.isfile(os.path.join(bin_dir, module)):
        raise FileNotFoundError(f"{module} is not in {bin_dir}")
    dist = dist_file_name(flavour["name"])
    libs_name = f"{dist}.libs"  # the folder where auditwheel puts the libraries it adds

    # 1. the package with the module, and in geodms.libs the engine libraries it loads, with the
    #    resources beside libDmRtc.so, where GetExeDir looks for them (it finds that library through
    #    dladdr); the libraries are found through their rpath, so auditwheel leaves them as they are
    stage = os.path.join(work_dir, cp, "stage", f"{dist}-{version}")
    pkg = os.path.join(stage, "geodms")
    libs_dir = os.path.join(stage, libs_name)
    info = os.path.join(stage, f"{dist}-{version}.dist-info")
    for folder in (pkg, libs_dir, os.path.join(info, "licenses")):
        os.makedirs(folder)
    copy_elf(os.path.join(bin_dir, module), os.path.join(pkg, module), f"$ORIGIN/../{libs_name}")
    libraries = engine_libraries(bin_dir, os.path.join(bin_dir, module))
    print("engine libraries:", ", ".join(libraries))
    for lib in libraries:
        copy_elf(os.path.join(bin_dir, lib), os.path.join(libs_dir, lib), "$ORIGIN")
    copy_resources(bin_dir, libs_dir)
    with open(os.path.join(pkg, "__init__.py"), "w", encoding="utf-8", newline="\n") as f:
        f.write(INIT_PY)
    with open(os.path.join(info, "METADATA"), "w", encoding="utf-8", newline="\n") as f:
        f.write(metadata(flavour, version, python_requirement, None))
    with open(os.path.join(info, "WHEEL"), "w", encoding="utf-8", newline="\n") as f:
        f.write(wheel_file(f"{cp}-{cp}-linux_{LINUX_ARCH}"))
    shutil.copy2(os.path.join(REPO, "license.txt"), os.path.join(info, "licenses", "license.txt"))
    bare_dir = os.path.join(work_dir, cp, "bare")
    os.makedirs(bare_dir)
    run([sys.executable, "-m", "wheel", "pack", stage, "-d", bare_dir])
    bare = os.path.join(bare_dir, f"{dist}-{version}-{cp}-{cp}-linux_{LINUX_ARCH}.whl")

    # 2. auditwheel adds the system libraries that a manylinux platform does not promise, renamed,
    #    and tags the wheel with the oldest manylinux platform whose glibc and libstdc++ have every
    #    symbol version that the files of the wheel ask for
    repaired_dir = os.path.join(work_dir, cp, "repaired")
    env = dict(os.environ, PATH=tool_path())  # auditwheel runs patchelf from PATH
    run([sys.executable, "-m", "auditwheel", "repair", "-w", repaired_dir, bare], env=env)
    repaired = glob.glob(os.path.join(repaired_dir, "*.whl"))
    if len(repaired) != 1:
        sys.exit(f"make-wheels: auditwheel wrote {len(repaired)} wheels in {repaired_dir}, expected one")
    repaired = repaired[0]

    # 3. the licence texts of what the wheel contains go into the metadata (wheel pack below writes
    #    a new RECORD, and names the wheel after the tag that auditwheel wrote in WHEEL)
    unpack_dir = os.path.join(work_dir, cp, "unpacked")
    run([sys.executable, "-m", "wheel", "unpack", repaired, "-d", unpack_dir])
    root = os.path.join(unpack_dir, f"{dist}-{version}")
    info_dir = os.path.join(root, f"{dist}-{version}.dist-info")
    licence_files = ["license.txt"] + add_licences_linux(os.path.join(root, libs_name), os.path.join(info_dir, "licenses"),
                                                         vcpkg_installed)
    with open(os.path.join(info_dir, "METADATA"), "w", encoding="utf-8", newline="\n") as f:
        f.write(metadata(flavour, version, python_requirement, None, licence_files))
    os.makedirs(out_dir, exist_ok=True)
    run([sys.executable, "-m", "wheel", "pack", root, "-d", out_dir])
    return os.path.join(out_dir, os.path.basename(repaired))


def check_size(wheel):
    size = os.path.getsize(wheel)
    if size > PYPI_FILE_LIMIT_BYTES:
        sys.exit(f"make-wheels: {wheel} has {size} bytes, more than the {PYPI_FILE_LIMIT_BYTES} that PyPI accepts "
                 "per file (PYPI_FILE_LIMIT_BYTES in this script)")
    print(f"{os.path.basename(wheel)}: {size} bytes")
    return wheel


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    on_linux = sys.platform.startswith("linux")
    parser.add_argument("--flavour", choices=sorted(FLAVOURS), default="l" if on_linux else "m",
                        help="m: the Windows wheels of geodms (the default on Windows); g: geodms-globio, of the "
                             "GLOBIO build; l: the Linux wheels of geodms (the default on Linux)")
    parser.add_argument("--bin", help="build output folder (default: bin/Release/x64, bin_GLOBIO/Release/x64 for g, "
                                      "build/linux-x64-release/bin for l)")
    parser.add_argument("--out", default=os.path.join(REPO, "distr"), help="folder for the finished wheels")
    parser.add_argument("--python", help="';'-separated CPython minors (default: those of the flavour's list in python/)")
    parser.add_argument("--version", default=read_version(), help="package version (default: RtcVersionNumbers.h)")
    parser.add_argument("--work", default="", help="work folder (default: a temporary folder, removed afterwards)")
    parser.add_argument("--vcpkg", help="vcpkg_installed folder with the licence texts of the bundled libraries "
                                        "(default: vcpkg_installed, vcpkg_installed_GLOBIO for g, "
                                        "build/linux-x64-release/vcpkg_installed for l)")
    parser.add_argument("--conda", help="g: the GLOBIO conda environment the build took its spatial libraries "
                                        "from (default: GLOBIO_ENV_ROOT)")
    args = parser.parse_args()
    flavour = FLAVOURS[args.flavour]
    if not sys.platform.startswith(flavour["platform"]):
        sys.exit(f"make-wheels: the flavour {args.flavour} is made on {flavour['platform']}, not on {sys.platform}")
    bin_dir = os.path.abspath(args.bin or os.path.join(REPO, flavour["bin"]))
    vcpkg_installed = os.path.abspath(args.vcpkg or os.path.join(REPO, flavour["vcpkg"]))
    minors = (args.python or ";".join(read_python_versions(flavour))).split(";")
    conda = None
    if flavour["conda"]:
        # The GLOBIO build looks up GDAL in its own folder with the delay-load hook of 20.29.0, and
        # the preload that older wheels of geodms needed instead was never written for its DLLs.
        if version_key(args.version) < version_key(FIRST_VERSION_WITH_DELAY_LOAD_HOOK):
            sys.exit(f"make-wheels: the GLOBIO flavour needs a build of {FIRST_VERSION_WITH_DELAY_LOAD_HOOK} or later")
        conda_root = args.conda or os.environ.get("GLOBIO_ENV_ROOT")
        if not conda_root or not os.path.isdir(os.path.join(conda_root, "conda-meta")):
            sys.exit("make-wheels: the GLOBIO flavour needs the conda environment of its build, "
                     "through --conda or GLOBIO_ENV_ROOT")
        conda = CondaEnv(os.path.abspath(conda_root))

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
        out_dir = os.path.abspath(args.out)
        for minor in minors:
            if flavour["platform"] == "linux":
                wheel = build_one_linux(flavour, bin_dir, out_dir, work_dir, vcpkg_installed, args.version, minor,
                                        requires_python(flavour, minors))
            else:
                wheel = build_one(flavour, bin_dir, out_dir, work_dir, vcpkg_installed, conda, args.version, minor,
                                  requires_python(flavour, minors))
            print("wrote", check_size(wheel))
    finally:
        if not args.work:
            shutil.rmtree(work_dir, ignore_errors=True)


if __name__ == "__main__":
    main()
