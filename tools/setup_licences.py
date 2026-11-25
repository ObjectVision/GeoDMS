# Copyright (C) 2026 Object Vision B.V.
# License: GNU GPL 3

"""The GPL of GeoDMS and the licence texts of the libraries that a GeoDMS setup bundles (#1371).

A setup installs license.txt, the GPL of GeoDMS, in its installation folder, and beside it a folder
licenses with the licence texts of the libraries it bundles and a table, licenses/README.txt, of every
binary it installs: where that binary comes from and which file in that folder holds its licence text.
This script writes that folder before a setup is packaged (subcommand stage), and checks an installed
setup against the table afterwards (subcommand check). tools/make-wheels.py takes the names of the
GeoDMS DLLs, the pattern of the Visual C++ runtime, the reader of the vcpkg tree and the table writer
write_table from here, so the wheels and the setups map a DLL to its source in the same way.

There are four setup flavours: m, built with msbuild; c, built with CMake; g, the GLOBIO build, whose
spatial libraries come from a conda environment; and l, the Linux build. The scripts
batch/BuildSignAndCreateSetup.bat (m), BuildSignAndCreateSetupCmake.bat (c) and
BuildSignAndCreateSetupGlobio.bat (g) run stage right before makensis and check right after the
install; for l, nsi/CreateLinuxSetup.sh runs stage on its staged tree, and
batch/BuildSignAndCreateSetupLinux.bat runs check after dpkg -i.

The script and its messages write "licence", the British spelling used in this repository, but the
folder, the constants and the file license.txt say "license", so a search for "license" finds them.

    py -3 tools/setup_licences.py stage --flavour m|c|g [--out <folder>] [--define NAME=VALUE ...]
    python3 tools/setup_licences.py stage --flavour l --tree <staged installation folder>
                                    --deb-copyright <file> --install-prefix /opt/ObjectVision/GeoDms<ver>.l
    py -3 tools/setup_licences.py check --install <installation folder> [--table <README.txt>]
                                    [--deb-copyright /usr/share/doc/geodms/copyright]

stage, for the Windows flavours, takes the installed files from the File lines of the NSIS script of
the flavour (FLAVOURS below), as makensis would resolve them: the !define, !ifdef, !if and !include
lines, the wildcards with their Win32 meaning (*.* also matches a name without a dot), the /r and /x
options, and SetOutPath. It stops on any line it cannot read, so that a later edit of the NSIS script
cannot install a file that the table misses. The licenses folder goes where the line
"File /r <folder>\\licenses\\*" of the NSIS script takes it from, unless --out says otherwise.
For the Linux flavour, stage takes the files of the folder that nsi/CreateLinuxSetup.sh has staged.

Each binary (a PE or ELF file) is GeoDMS itself, the Visual C++ runtime, or a byte-identical copy of a
file of one of the sources of the flavour: the Qt installation (QT_ROOT, with the module of each file
from its SBOM), a vcpkg port, a package of the conda environment of the GLOBIO flavour, or, for Linux,
a Debian package of the build machine. Any other binary, or a copy that differs from its source, stops
the script: no library goes out without its licence text. Besides those, the table lists the vcpkg
ports without a shared library of their own, whose code is compiled into the binaries that use them.

Some files of GeoDMS itself contain material from other projects (#1389): GeoDmsGuiQt compiles two
fonts in through its Qt resource file, one of which is also installed as a font file, and Geo.dll
holds a polygon rasterizer adapted from GDAL. INSIDE_GEODMS lists them with their licence texts, which
stage copies into licenses/geodms and lists in a second table of README.txt, per installed file that
contains them. stage stops on a file in the Qt resource file, or a font among the installed files,
that is neither GeoDMS's own nor has a licence text here, as it stops on a binary.

check walks an installation folder and compares the binaries in it with the table, in both directions,
and checks that license.txt and every licence file that the table names are there, and that every
installed file that contains an item of INSIDE_GEODMS has a row with that item's licence text.
"""
import argparse
import collections
import fnmatch
import glob
import hashlib
import json
import os
import posixpath
import re
import shutil
import struct
import subprocess
import sys
import textwrap

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# ---------------------------------------------------------------------------------------------
# Parameters
# ---------------------------------------------------------------------------------------------

# Per setup flavour: the NSIS script that packages it (none for Linux), the vcpkg tree its binaries
# were built from, relative to the repository, and whether its spatial stack comes from the conda
# environment named by GLOBIO_ENV_ROOT (the GLOBIO flavour, vcpkg-globio/environment.yml).
FLAVOURS = {
    "m": {"nsi": "nsi/DmsSetupScriptX64.nsi", "vcpkg": "vcpkg_installed", "conda": False},
    "c": {"nsi": "nsi/DmsSetupScriptX64-cmake.nsi", "vcpkg": "vcpkg_installed", "conda": False},
    "g": {"nsi": "nsi/DmsSetupScriptX64-globio.nsi", "vcpkg": "vcpkg_installed_GLOBIO", "conda": True},
    "l": {"nsi": None, "vcpkg": "build/linux-x64-release/vcpkg_installed", "conda": False},
}

# The Qt installation of the Windows builds: QtInstall 6.11.1_msvc2022_64 in
# qtgui/exe/GeoDmsGuiQt.vcxproj and CMAKE_PREFIX_PATH in batch/BuildSignAndCreateSetupCmake.bat. Its
# sbom folder names the module of each file, and the Src folder beside it, the Sources component of
# the Qt installer, holds Src/<module>/LICENSES, which is copied whole for each module a setup uses.
# A shipped Qt file that differs from its copy here stops the script, so a Qt upgrade that forgets
# this line cannot ship the licence texts of the old version.
QT_ROOT = r"C:\Qt\6.11.1\msvc2022_64"

# The NSIS installation that the setup scripts run makensis from; COPYING is the licence of the
# uninstaller stub that WriteUninstaller writes into the installation folder.
NSIS_ROOT = r"C:\Program Files (x86)\NSIS"

# Licence texts kept in the repository for the conda packages of the GLOBIO environment that carry
# none of their own: <package>-<version>.txt, taken from the upstream source of that version. When
# environment.yml moves such a package to another version, the script stops until the text of the
# new version is checked and stored here as <package>-<new version>.txt.
CONDA_KEPT_TEXTS = os.path.join(REPO, "vcpkg-globio", "licenses")

# Where the Linux flavour finds the system copies of the libraries and Qt plugins it bundles
# (Ubuntu 24.04, the WSL distribution of batch/BuildSignAndCreateSetupLinux.bat).
LINUX_LIB_DIRS = ["/usr/lib/x86_64-linux-gnu", "/lib/x86_64-linux-gnu"]
LINUX_QT_PLUGIN_DIR = "/usr/lib/x86_64-linux-gnu/qt6/plugins"

# Material from other projects inside files of GeoDMS itself (#1389). Per item: the file in the
# repository; what it is and its licence, as README.txt and the Debian copyright file name them; the
# file in the repository with its licence text; the name of that text in the licenses folder; and the
# lower-case names of the installed files that contain the item, in every flavour. stage stops when a
# setup installs none of those files, because then a name here is stale; check requires the text
# wherever one of them is installed.
Inside = collections.namedtuple("Inside", "source what licence text name installed")
INSIDE_GEODMS = [
    Inside("qtgui/exe/res/fonts/dmstext.ttf",
           "the fonts Noto Sans Medium, Noto Sans Arabic Medium and Noto Sans Math, merged into dmstext.ttf",
           "SIL Open Font License 1.1", "qtgui/exe/res/fonts/dmstext-OFL.txt", "geodms/dmstext-OFL.txt",
           {"geodmsguiqt.exe", "geodmsguiqt", "dmstext.ttf"}),
    Inside("qtgui/exe/res/fonts/remixicon.ttf", "the icon font Remix Icon 3.5, remixicon.ttf",
           "Apache License 2.0", "qtgui/exe/res/fonts/remixicon-License.txt", "geodms/remixicon-License.txt",
           {"geodmsguiqt.exe", "geodmsguiqt"}),
    Inside("geo/dll/src/Poly2GridOper.cpp",
           "the polygon rasterizer of poly2grid and poly2allgrids, adapted from alg/llrasterize.cpp of GDAL",
           "MIT License", "geo/dll/src/Poly2GridOper-GDAL-MIT.txt", "geodms/Poly2GridOper-GDAL-MIT.txt",
           {"geo.dll", "dmgeo.dll", "libdmgeo.so"}),
]

# The fonts that a setup can install or GeoDmsGuiQt can compile in; each needs a licence text.
FONT_FILE = re.compile(r".*\.(ttf|otf|ttc|woff2?|pfa|pfb|fon)$", re.IGNORECASE)
# The fonts that need no licence text of their own because their name table names Object Vision as
# the copyright holder, by lower-case file name: DMS Font, res/fonts/dms.ttf, the symbol font of point
# layers (shv/dll/src/FontRole.h). A font with symbols from another font belongs in INSIDE_GEODMS.
# Object Vision drew the symbols of dms.ttf, the windmills at the codes 43, 82 and 83 included, which
# were added in 2022. Apart from simple shapes such as a triangle, none of them matches a drawing in the
# symbol fonts of Windows or in the open source fonts compared for #1389, Remix Icon among them. A
# symbol taken from another font makes dms.ttf an item of INSIDE_GEODMS, with that font's licence.
GEODMS_FONTS = {"dms.ttf"}
# The Qt resource file that GeoDmsGuiQt compiles in. Every file in it must be either an item of
# INSIDE_GEODMS or one of GEODMS_GUI_FILES.
GUI_QRC = "qtgui/exe/GeoDmsGuiQt.qrc"
# The files of GUI_QRC that are GeoDMS's own and need no licence text of their own, by their path in
# GUI_QRC: the icons of the toolbars, the detail pages, the event log and the tree view, and the
# program icon, which Object Vision made; the branch lines of the tree view, which
# tools/make_tree_line_images.py draws; and WorldHeatMap.jpg, the splash screen, which doc/licensing.md
# describes. A file from another project belongs in INSIDE_GEODMS, with its licence text.
GEODMS_GUI_FILES = {"res/images/" + f for f in """
    DP_back.bmp DP_configuration.bmp DP_explore.bmp DP_forward.bmp DP_history.bmp DP_MetaData.bmp
    DP_properties.bmp DP_properties_general.bmp DP_properties_properties.bmp DP_SourceData.png
    DP_ValueInfo.bmp
    EL_clear.bmp EL_scroll_down.bmp EL_selection.bmp EL_selection_text.bmp EL_selection_type.bmp
    TB_copy.bmp TB_group_by.bmp TB_pan_button.bmp TB_save.bmp TB_select_all.bmp TB_select_circle.bmp
    TB_select_district.bmp TB_select_none.bmp TB_select_object.bmp TB_select_poly.bmp TB_select_rect.bmp
    TB_show_all_features.bmp TB_show_selected_features.bmp TB_table_select_row.bmp
    TB_table_show_first_selected.bmp TB_toggle_layout_1.bmp TB_toggle_layout_2.bmp TB_toggle_layout_3.bmp
    TB_toggle_needle.bmp TB_toggle_palette.bmp TB_toggle_scalebar.bmp TB_vcopy.bmp TB_zoom_active_layer.bmp
    TB_zoom_all_layers.bmp TB_zoom_selected.bmp TB_zoomin_button.bmp TB_zoomout_button.bmp
    TV_storage_unstored.png arrow_down.png down_arrow.png down_arrow_hover.png right_arrow.png
    right_arrow_hover.png GeoDmsGuiQt.png
    TV_branch_end.png TV_branch_more.png TV_vline.png
    WorldHeatMap.jpg
""".split()}

GPL_FILE = "license.txt"     # the GPL of GeoDMS, in the repository root and the installation folder
LICENCES_DIR = "licenses"    # the folder beside it with the texts of the bundled libraries
README = "README.txt"        # the table in that folder
TITLE = "Licence texts of the libraries bundled with GeoDMS"  # its first line; marks a folder this script wrote
INSIDE_HEADER = "file of GeoDMS"  # the first column of its table of INSIDE_GEODMS
TEXT_WIDTH = 92              # line width of the prose in README.txt and the Debian copyright file

# ---------------------------------------------------------------------------------------------
# The mapping that tools/make-wheels.py shares
# ---------------------------------------------------------------------------------------------

# The DLLs that GeoDMS builds, under the GPL of license.txt: the msbuild names and the Dm-prefixed
# names of the CMake build.
GEODMS_DLLS = {"clc.dll", "geo.dll", "rtc.dll", "shv.dll", "stg.dll", "stx.dll",
               "dmclc.dll", "dmgeo.dll", "dmrtc.dll", "dmshv.dll", "dmstg.dll", "dmstx.dll"}
# The other binaries of GeoDMS: the programs, the Python binding and the Linux libraries.
GEODMS_OTHER = re.compile(r"geodmsrun(\.exe)?|geodmsguiqt(\.exe)?|geodms\.cp\d+-win_amd64\.pyd"
                          r"|geodms\.cpython-\d+-\w+-linux-gnu\.so"
                          r"|libdm(clc|geo|rtc|shv|stg|stx)\.so", re.IGNORECASE)
# The Visual C++ runtime, redistributable under the licence terms of Visual Studio. vccorlib140,
# vcomp140 and vcamp140 are part of the same redistributable and are shipped by the setups.
MSVC_RUNTIME = re.compile(r"(msvcp140|vcruntime140|concrt140|vccorlib140|vcomp140|vcamp140)(_\w+)?\.dll",
                          re.IGNORECASE)
SHARED_LIBRARY = re.compile(r".*(\.dll|\.so(\.\d+)*)$", re.IGNORECASE)


def vcpkg_listing(vcpkg_installed):
    """Every installed port as port -> (version, triplet), and every shared library that a port
    installed, by lower-case file name, as a list of (port, version, triplet, path relative to
    vcpkg_installed), in the order of the listings."""
    info = os.path.join(vcpkg_installed, "vcpkg", "info")
    libs, ports = {}, {}
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
                if SHARED_LIBRARY.fullmatch(line) and ("/bin/" in line or "/lib/" in line):
                    libs.setdefault(os.path.basename(line).lower(), []).append((port, version, triplet, line))
    return libs, ports


def vcpkg_ports(vcpkg_installed):
    """Two maps, as make-wheels.py uses them: the lower-case name of every DLL that vcpkg installed
    to (port, version, triplet), and every installed port to (version, triplet)."""
    libs, ports = vcpkg_listing(vcpkg_installed)
    dlls = {}
    for name, entries in libs.items():
        for port, version, triplet, line in entries:
            if "/bin/" in line and name.endswith(".dll"):
                dlls[name] = (port, version, triplet)
    return dlls, ports


def write_table(out, header, rows):
    widths = [max(len(r[i]) for r in rows + [header]) for i in range(len(header) - 1)]
    for r in [header] + rows:
        out.write("  ".join(r[i].ljust(w) for i, w in enumerate(widths)) + "  " + r[-1] + "\n")


# ---------------------------------------------------------------------------------------------
# Small helpers
# ---------------------------------------------------------------------------------------------

class Problems(list):
    """The errors of a run, all reported before the script stops."""

    def stop_if_any(self, what, remove=None):
        """Stop when there are errors, after removing the folder given as remove, so that a
        half-written licenses folder cannot be packaged."""
        if self:
            for p in self:
                print(f"setup_licences: ERROR: {p}", file=sys.stderr)
            if remove:
                shutil.rmtree(remove, ignore_errors=True)
            sys.exit(f"setup_licences: {what} failed with {len(self)} error(s)")


def read_version():
    text = open(os.path.join(REPO, "rtc", "dll", "src", "RtcVersionNumbers.h"), encoding="utf-8").read()
    parts = [re.search(rf"#define DMS_VERSION_{p}\s+(\d+)", text).group(1) for p in ("MAJOR", "MINOR", "PATCH")]
    return ".".join(parts)


_digests = {}


def sha256(path):
    path = os.path.realpath(path)
    if path not in _digests:
        h = hashlib.sha256()
        with open(path, "rb") as f:
            for block in iter(lambda: f.read(1 << 20), b""):
                h.update(block)
        _digests[path] = h.hexdigest()
    return _digests[path]


def is_binary(path):
    """A PE file (exe, dll, pyd) or an ELF file (executable or shared library)."""
    with open(path, "rb") as f:
        head = f.read(64)
        if head[:4] == b"\x7fELF":
            return True
        if head[:2] != b"MZ" or len(head) < 64:
            return False
        f.seek(struct.unpack_from("<I", head, 0x3C)[0])
        return f.read(4) == b"PE\0\0"


def walk_files(root, skip=None):
    """Paths relative to root of every file below it, except below the folder skip."""
    for d, dirs, files in os.walk(root):
        if skip:
            dirs[:] = [x for x in dirs if os.path.normcase(os.path.join(d, x)) != os.path.normcase(skip)]
        for f in files:
            yield os.path.relpath(os.path.join(d, f), root)


def wrap(text):
    return textwrap.fill(text, TEXT_WIDTH) + "\n"


def qrc_files():
    """The files that GeoDmsGuiQt compiles in through GUI_QRC, as their path in GUI_QRC."""
    with open(os.path.join(REPO, *GUI_QRC.split("/")), encoding="utf-8") as f:
        files = re.findall(r"<file\b[^>]*>\s*([^<]*?)\s*</file>", f.read())
    return [posixpath.normpath(p.replace("\\", "/")) for p in files]


# ---------------------------------------------------------------------------------------------
# Reading an NSIS script
# ---------------------------------------------------------------------------------------------

# Commands of the setup scripts that install no file. Any other command stops the script; add it
# here only when it does not put a file in the installation folder.
NSIS_HARMLESS = {"name", "outfile", "installdir", "requestexecutionlevel", "page", "createdirectory",
                 "setregview", "writeregstr", "writeregdword", "deleteregkey", "ifsilent", "messagebox",
                 "setshellvarcontext", "createshortcut", "delete", "rmdir"}


class NsisError(Exception):
    pass


class NsisLineError(NsisError):
    """An NsisError that already names the file and line."""


def nsis_tokens(line):
    """Split a logical NSIS line into tokens, without the comment; quotes are removed."""
    tokens, i, n = [], 0, len(line)
    while i < n:
        c = line[i]
        if c.isspace():
            i += 1
            continue
        if c in ";#":
            break
        if line.startswith("/*", i):
            raise NsisError("a /* comment */ block")
        if c in "\"'`":
            j = line.find(c, i + 1)
            if j < 0:
                raise NsisError("an unterminated quote")
            tokens.append(line[i + 1:j])
            i = j + 1
        else:
            j = i
            while j < n and not line[j].isspace():
                j += 1
            tokens.append(line[i:j])
            i = j
    return tokens


def win_match(name, pattern):
    """A name against an NSIS wildcard, as Win32 matches it: case-insensitive, and a pattern that
    ends in .* also matches the name without a dot (so *.* matches every name)."""
    n, p = name.lower(), pattern.lower().replace("[", "[[]")
    return fnmatch.fnmatchcase(n, p) or (p.endswith(".*") and fnmatch.fnmatchcase(n, p[:-2]))


class NsisScript:
    """What an NSIS script installs, read as makensis would read it. The attributes:
    files, the files it installs below $INSTDIR as (path relative to the installation folder, source
    path), with source None for the uninstaller that WriteUninstaller writes; licences_source, the
    folder that its line "File /r <folder>\\licenses\\*" reads; file_lines, the number of File lines
    it read; and empty_nonfatal, the File /nonfatal lines that matched nothing.
    A name given in defines, as makensis /D gives it, wins over a !define of the same name in the
    script. makensis itself refuses a !define of a name that is already defined; this exception
    lets a test point the script at another folder, such as an installed setup."""

    def __init__(self, path, defines):
        self.base = os.path.dirname(os.path.abspath(path))  # makensis runs in the folder of the script
        self.defines, self.given = dict(defines), set(defines)
        self.files, self.licences_source, self.file_lines, self.empty_nonfatal = [], None, 0, []
        self.outpath = ""            # relative to $INSTDIR; None when outside it
        self.section = None
        self.conditions = []         # per open !if: [parent active, a branch was taken, active]
        self.read(os.path.abspath(path))
        if self.conditions:
            raise NsisError(f"{path}: an !if without !endif")

    def active(self):
        return all(c[2] for c in self.conditions)

    def expand(self, text):
        for _ in range(20):
            new = re.sub(r"\$\{([^}]+)\}", lambda m: self.defines.get(m.group(1), m.group(0)), text)
            new = re.sub(r"\$%([^%]+)%", lambda m: os.environ.get(m.group(1), m.group(0)), new)
            if new == text:
                return text
            text = new
        raise NsisError(f"the defines in {text} do not resolve")

    def read(self, path):
        with open(path, encoding="utf-8", errors="replace") as f:
            raw = f.read().splitlines()
        logical, buf, start = [], "", 0
        for number, line in enumerate(raw, 1):
            if not buf:
                start = number
            if line.endswith("\\"):
                buf += line[:-1]
                continue
            logical.append((start, buf + line))
            buf = ""
        if buf:
            logical.append((start, buf))
        for number, line in logical:
            try:
                self.line(path, line)
            except NsisLineError:
                raise
            except NsisError as e:
                raise NsisLineError(f"{os.path.relpath(path, REPO)}:{number}: cannot read {line.strip()!r}: {e}")

    def line(self, path, line):
        tokens = nsis_tokens(line)
        if not tokens:
            return
        cmd = tokens[0].lower()
        if cmd.startswith("!"):
            return self.directive(path, cmd, tokens[1:])
        if not self.active():
            return
        args = [self.expand(t) for t in tokens[1:]]
        if len(tokens) == 1 and tokens[0].endswith(":"):
            return  # a label
        if cmd == "section":
            if self.section is not None:
                raise NsisError("a Section inside a Section")
            names = [a for a in args if not a.startswith("/")]
            self.section = names[0].lower().lstrip("-!") if names else ""
        elif cmd == "sectionend":
            self.section = None
        elif cmd == "setoutpath":
            self.outpath = self.install_relative(args[0] if args else "")
        elif cmd == "file":
            self.file(args)
        elif cmd == "writeuninstaller":
            target = self.install_relative(args[0] if args else "")
            if target is None:
                raise NsisError("an uninstaller outside $INSTDIR")
            self.files.append((target, None))
        elif cmd not in NSIS_HARMLESS:
            raise NsisError(f"the command {tokens[0]}, which this script does not know; if it installs no "
                            "file, add it to NSIS_HARMLESS in tools/setup_licences.py")

    def directive(self, path, cmd, args):
        if cmd in ("!ifdef", "!ifndef", "!if"):
            parent = self.active()
            if cmd == "!if":
                vals = [self.expand(a) for a in args]
                if len(vals) == 3 and vals[1] in ("==", "!="):
                    true = (vals[0] == vals[2]) == (vals[1] == "==")
                else:
                    raise NsisError("an !if other than <a> == <b> or <a> != <b>")
            else:
                if len(args) != 1:
                    raise NsisError(f"{cmd} with other than one name")
                true = (args[0] in self.defines) == (cmd == "!ifdef")
            self.conditions.append([parent, true, parent and true])
        elif cmd == "!else":
            if args or not self.conditions:
                raise NsisError("an !else with a condition, or without !if")
            c = self.conditions[-1]
            c[2] = c[0] and not c[1]
            c[1] = True
        elif cmd == "!endif":
            if not self.conditions:
                raise NsisError("an !endif without !if")
            self.conditions.pop()
        elif not self.active():
            return
        elif cmd == "!define":
            if not args or args[0].startswith("/"):
                raise NsisError("a !define with an option")
            if args[0] not in self.given:
                self.defines[args[0]] = self.expand(" ".join(args[1:]))
        elif cmd == "!undef":
            self.defines.pop(args[0], None)
        elif cmd == "!include":
            if len(args) != 1:
                raise NsisError("an !include with an option")
            self.read(os.path.join(os.path.dirname(path), self.expand(args[0])))
        elif cmd == "!echo":
            pass
        else:
            raise NsisError(f"the directive {cmd}")

    def install_relative(self, outpath):
        if re.search(r"\$\{|\$%", outpath):
            raise NsisError(f"the unresolved {outpath}")
        m = re.fullmatch(r"\$INSTDIR(?:[\\/](.*))?", outpath, re.IGNORECASE)
        if not m:
            return None  # elsewhere, such as $PROFILE
        return os.path.normpath(m.group(1)) if m.group(1) else ""

    def file(self, args):
        if self.section is None or self.section == "uninstall" or self.section.startswith("un."):
            raise NsisError("a File line outside an install section")
        self.file_lines += 1
        recursive = nonfatal = False
        excludes, specs, i = [], [], 0
        while i < len(args):
            a = args[i]
            low = a.lower()
            if specs or not a.startswith("/"):
                specs.append(a)
            elif low == "/r":
                recursive = True
            elif low == "/nonfatal":
                nonfatal = True
            elif low == "/a":
                pass
            elif low == "/x":
                i += 1
                if i == len(args):
                    raise NsisError("a /x without a pattern")
                excludes.append(args[i])
            else:
                raise NsisError(f"the option {a}")
            i += 1
        if not specs:
            raise NsisError("a File line without a file")
        for spec in specs:
            if re.search(r"\$\{|\$%|\$[A-Za-z]", spec):
                raise NsisError(f"the unresolved {spec}")
            path = os.path.normpath(os.path.join(self.base, spec.replace("\\", os.sep)))
            folder, pattern = os.path.split(path)
            if recursive and pattern == "*" and os.path.basename(folder).lower() == LICENCES_DIR:
                if self.outpath != LICENCES_DIR:
                    raise NsisError(f"the licence texts go to {self.outpath!r}, not to {LICENCES_DIR}")
                self.licences_source = folder  # written by this script; empty or absent while it reads
                continue
            found = self.matches(folder, pattern, recursive, excludes)
            if not found and not nonfatal:
                raise NsisError(f"{path} matches no file, so makensis would fail as well")
            if not found:
                self.empty_nonfatal.append(path)
            for rel, src in found:
                if self.outpath is None:
                    if is_binary(src):
                        raise NsisError(f"the binary {src} goes outside $INSTDIR")
                    continue
                self.files.append((os.path.normpath(os.path.join(self.outpath, rel)), src))

    def matches(self, folder, pattern, recursive, excludes):
        excluded = lambda name: any(win_match(name, x) for x in excludes)  # noqa: E731
        if not any(c in pattern for c in "*?"):
            path = os.path.join(folder, pattern)
            if os.path.isfile(path):
                return [(pattern, path)]
            if os.path.isdir(path):
                if not recursive:
                    raise NsisError(f"the folder {path} without /r")
                folder, pattern, prefix = path, "*", os.path.basename(path)
            else:
                return []
        else:
            prefix = ""
        if not os.path.isdir(folder):
            return []
        found = []
        for d, dirs, files in os.walk(folder):
            dirs[:] = sorted(x for x in dirs if recursive and not excluded(x))
            for f in sorted(files):
                if win_match(f, pattern) and not excluded(f):
                    src = os.path.join(d, f)
                    found.append((os.path.join(prefix, os.path.relpath(src, folder)), src))
        return found


# ---------------------------------------------------------------------------------------------
# The sources a binary can come from
# ---------------------------------------------------------------------------------------------

class Candidate:
    """A file of a source that a shipped binary may be a copy of, with what the table says of it."""

    def __init__(self, origin, source, version, licence, copy_from):
        self.origin = origin        # the file in the source
        self.source = source        # "comes from" in the table
        self.version = version
        self.licence = licence      # path relative to the licenses folder; one ending in a separator is a folder
        self.copy_from = copy_from  # the file or folder that becomes that licence path


class QtInstall:
    def __init__(self, root):
        self.root, self.files, self.version = root, {}, None
        self.src = os.path.join(os.path.dirname(root), "Src")
        for spdx in glob.glob(os.path.join(root, "sbom", "*.spdx")):
            m = re.fullmatch(r"(.+)-(\d+\.\d+\.\d+)\.spdx", os.path.basename(spdx))
            if not m:
                continue  # the *.source.spdx files describe the sources, not the installed files
            module, self.version = m.groups()
            with open(spdx, encoding="utf-8", errors="replace") as f:
                for line in f:
                    if line.startswith("FileName: ./"):
                        self.files[line[len("FileName: ./"):].strip().lower()] = module

    def find(self, rel):
        # The Qt module and file of an installed path: a file in the installation folder is looked
        # for in Qt's bin, a file in a subfolder in Qt's plugins and then from Qt's root (translations).
        parts = rel.replace("\\", "/").split("/")
        options = ["bin/" + parts[-1]] if len(parts) == 1 else ["plugins/" + "/".join(parts), "/".join(parts)]
        for o in options:
            module = self.files.get(o.lower())
            if module:
                return module, os.path.join(self.root, *o.split("/"))
        return None

    def candidate(self, rel):
        hit = self.find(rel)
        if not hit:
            return []
        module, origin = hit
        return [Candidate(origin, f"Qt {module}", self.version, os.path.join("qt", module) + os.sep,
                          os.path.join(self.src, module, "LICENSES"))]


class VcpkgTree:
    def __init__(self, root):
        self.root = root
        self.libs, self.ports = vcpkg_listing(root)

    def candidate(self, rel):
        out = []
        for port, version, triplet, line in self.libs.get(os.path.basename(rel).lower(), []):
            if "/debug/" in line:
                continue
            out.append(Candidate(os.path.join(self.root, *line.split("/")), f"vcpkg port {port}", version,
                                 os.path.join("vcpkg", port + ".txt"),
                                 os.path.join(self.root, triplet, "share", port, "copyright")))
        return out

    def static_ports(self):
        """The ports without a shared library of their own: header-only libraries such as Boost and
        pybind11, and static ones, or all ports where the triplet is static (Linux). Which of them the
        binaries contain is not known here, so all are listed, except vcpkg's own helper ports
        (vcpkg-*), which only run at build time, and ports that install nothing but build metadata
        under share/, such as libiconv on Linux, which leaves iconv to the C library. The Boost ports
        share one licence text."""
        with_lib = {e[0] for entries in self.libs.values() for e in entries}
        rows, seen = [], set()
        for port in sorted(self.ports):
            if port in with_lib or port.startswith("vcpkg-"):
                continue
            version, triplet = self.ports[port]
            listing = os.path.join(self.root, "vcpkg", "info", f"{port}_{version}_{triplet}.list")
            with open(listing, encoding="utf-8") as f:
                content = [line.strip() for line in f
                           if not line.strip().endswith("/") and not line.startswith(f"{triplet}/share/")]
            if not content:
                continue
            key = "boost" if port.startswith("boost-") else port
            if key in seen:
                continue
            seen.add(key)
            rows.append((key, version, os.path.join("vcpkg", key + ".txt"),
                         os.path.join(self.root, triplet, "share", port, "copyright")))
        return rows


LICENCE_FILE_NAME = re.compile(r"(licen[cs]e|copying|copyright|notice)(\.(txt|md|rst))?", re.IGNORECASE)


class CondaEnv:
    def __init__(self, root):
        self.root, self.dlls, self.packages = root, {}, {}
        for meta in glob.glob(os.path.join(root, "conda-meta", "*.json")):
            with open(meta, encoding="utf-8") as f:
                d = json.load(f)
            cache = d.get("extracted_package_dir") or (d.get("link") or {}).get("source")
            self.packages[d["name"]] = (d["version"], cache, d.get("files", []))
            for p in d.get("files", []):
                p = p.replace("\\", "/")
                if p.lower().startswith("library/bin/") and p.lower().endswith(".dll"):
                    self.dlls[os.path.basename(p).lower()] = (d["name"], os.path.join(root, *p.split("/")))

    def licence_source(self, name, problems):
        """The folder or file with the licence text of a package: info/licenses in the package cache,
        else the licence files the package installs (libgdal: Library/share/gdal/LICENSE.TXT), else the
        text kept in the repository for this version."""
        version, cache, files = self.packages[name]
        if cache and os.path.isdir(os.path.join(cache, "info", "licenses")) and os.listdir(os.path.join(cache, "info", "licenses")):
            return os.path.join(cache, "info", "licenses")
        installed = [os.path.join(self.root, *p.replace("\\", "/").split("/")) for p in files
                     if LICENCE_FILE_NAME.fullmatch(os.path.basename(p))]
        installed = [p for p in installed if os.path.isfile(p)]
        if installed:
            return installed
        kept = os.path.join(CONDA_KEPT_TEXTS, f"{name}-{version}.txt")
        if os.path.isfile(kept):
            return [kept]
        others = glob.glob(os.path.join(CONDA_KEPT_TEXTS, f"{name}-*.txt"))
        if others:
            problems.append(f"the conda package {name} {version} has no licence text of its own, and the text kept "
                            f"in {CONDA_KEPT_TEXTS} is for another version ({', '.join(map(os.path.basename, others))}); "
                            f"check the licence of {version} and keep it as {os.path.basename(kept)}")
        else:
            problems.append(f"the conda package {name} {version} has no licence text: no info/licenses in "
                            f"{cache}, no licence file among its files, and no {kept}")
        return None

    def candidate(self, rel):
        hit = self.dlls.get(os.path.basename(rel).lower())
        if not hit:
            return []
        name, origin = hit
        return [Candidate(origin, f"conda package {name}", self.packages[name][0],
                          os.path.join("conda", name) + os.sep, ("conda", name))]


class DebianSystem:
    """The Debian packages of the build machine that the Linux setup takes Qt and its libraries from."""

    def __init__(self):
        self.copyrights = {}  # licence path -> copyright file, to give one text per source package

    def candidate(self, rel):
        parts = rel.replace("\\", "/").split("/")
        options = ([os.path.join(d, parts[0]) for d in LINUX_LIB_DIRS] if len(parts) == 1
                   else [os.path.join(LINUX_QT_PLUGIN_DIR, *parts)])
        for o in options:
            if not os.path.isfile(o):
                continue
            real = os.path.realpath(o)
            r = subprocess.run(["dpkg", "-S", real], capture_output=True, text=True)
            owners = [m.group(1) for m in re.finditer(r"^([^\s:,]+)(?::[a-z0-9]+)?: /", r.stdout, re.MULTILINE)]
            if r.returncode != 0 or not owners:
                continue  # "diversion by" lines and unowned files
            package = owners[0]
            q = subprocess.run(["dpkg-query", "-W", "-f", "${source:Package}\t${Version}", package],
                               capture_output=True, text=True)
            source, version = q.stdout.split("\t") if q.returncode == 0 else (package, "-")
            copyright = os.path.join("/usr/share/doc", package, "copyright")
            licence = os.path.join("debian", source + ".txt")
            known = self.copyrights.get(licence)
            # One text per source package, unless two binary packages of the same source carry
            # different copyright files: then the second is named after its binary package.
            if known and os.path.isfile(copyright) and sha256(known) != sha256(copyright):
                licence = os.path.join("debian", package + ".txt")
            self.copyrights.setdefault(licence, copyright)
            return [Candidate(real, f"Debian package {package} (source {source})", version, licence, copyright)]
        return []


SOURCE_NAMES = {QtInstall: "the Qt installation", VcpkgTree: "a vcpkg port",
                CondaEnv: "a conda package", DebianSystem: "a Debian package"}  # for the error messages


# ---------------------------------------------------------------------------------------------
# stage
# ---------------------------------------------------------------------------------------------

def prepare_out(out, problems):
    """Empty the licenses folder before writing it, but only one that is empty or that this script
    wrote, recognised by the first line of its README.txt, so that a wrong --out cannot wipe a folder."""
    if os.path.isdir(out) and os.listdir(out):
        readme = os.path.join(out, README)
        first = open(readme, encoding="utf-8").readline().strip() if os.path.isfile(readme) else ""
        if first != TITLE:
            problems.append(f"{out} is not empty and was not written by this script; give another --out")
            return
        shutil.rmtree(out)
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, README), "w", encoding="utf-8", newline="\n") as f:
        f.write(TITLE + "\n\nThis folder is incomplete: tools/setup_licences.py did not finish writing it.\n")


def copy_licence(out, licence, copy_from, problems):
    dst = os.path.join(out, licence)
    if os.path.exists(dst.rstrip(os.sep)):
        return
    sources = copy_from if isinstance(copy_from, list) else [copy_from]
    for src in sources:
        if not os.path.exists(src):
            problems.append(f"the licence text {src} does not exist")
            return
    if licence.endswith(os.sep):
        os.makedirs(dst, exist_ok=True)
        for src in sources:
            if os.path.isdir(src):
                shutil.copytree(src, dst, dirs_exist_ok=True)
            else:
                shutil.copyfile(src, os.path.join(dst, os.path.basename(src)))
    else:
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copyfile(sources[0], dst)


def stage(args):
    problems = Problems()
    flavour = FLAVOURS[args.flavour]
    version = args.version or read_version()
    gpl_rel = os.path.join(os.pardir, GPL_FILE)
    if flavour["nsi"]:
        defines = dict(d.split("=", 1) for d in args.define)
        nsi_path = os.path.join(REPO, flavour["nsi"])
        try:
            nsi = NsisScript(nsi_path, defines)
        except NsisError as e:
            sys.exit(f"setup_licences: {e}")
        if not nsi.licences_source:
            problems.append(f"{flavour['nsi']} installs no {LICENCES_DIR} folder (File /r <folder>\\{LICENCES_DIR}\\*)")
        if not any(os.path.normcase(rel) == GPL_FILE for rel, _ in nsi.files):
            problems.append(f"{flavour['nsi']} installs no {GPL_FILE} in the installation folder")
        problems.stop_if_any("reading the NSIS script")
        out = os.path.abspath(args.out or nsi.licences_source)
        installed = nsi.files
        origin = f"{flavour['nsi']}: {nsi.file_lines} File lines"
    else:
        if not args.tree:
            sys.exit("setup_licences: the Linux flavour needs --tree, the folder that CreateLinuxSetup.sh staged")
        tree = os.path.abspath(args.tree)
        out = os.path.abspath(args.out or os.path.join(tree, LICENCES_DIR))
        installed = [(rel, os.path.join(tree, rel)) for rel in sorted(walk_files(tree, skip=out))]
        if not os.path.isfile(os.path.join(tree, GPL_FILE)):
            problems.append(f"{tree} holds no {GPL_FILE}")
        origin = f"the staged folder {tree}"

    sources = []
    if args.flavour != "l":
        qt = QtInstall(args.qt)
        if not qt.files:
            problems.append(f"no Qt SBOM in {args.qt}\\sbom: is QT_ROOT the Qt installation of this build?")
        sources.append(qt)
    vcpkg_root = os.path.abspath(args.vcpkg or os.path.join(REPO, flavour["vcpkg"]))
    vcpkg = VcpkgTree(vcpkg_root)
    sources.append(vcpkg)
    conda = None
    if flavour["conda"]:
        conda_root = args.conda or os.environ.get("GLOBIO_ENV_ROOT")
        if not conda_root:
            sys.exit("setup_licences: the GLOBIO flavour needs GLOBIO_ENV_ROOT or --conda")
        conda = CondaEnv(conda_root)
        sources.append(conda)
    if args.flavour == "l":
        sources.append(DebianSystem())
    prepare_out(out, problems)
    problems.stop_if_any("preparing the licenses folder")

    inside_sources = {i.source for i in INSIDE_GEODMS}
    qrc_dir = posixpath.dirname(GUI_QRC)
    for path in qrc_files():
        if path not in GEODMS_GUI_FILES and posixpath.join(qrc_dir, path) not in inside_sources:
            problems.append(f"{posixpath.join(qrc_dir, path)} is compiled into GeoDmsGuiQt through {GUI_QRC}, but no "
                            "licence text is known for it: add it to GEODMS_GUI_FILES in tools/setup_licences.py if it "
                            "is GeoDMS's own, or else to INSIDE_GEODMS, with its licence text")
    for path in sorted(GEODMS_GUI_FILES - set(qrc_files())):
        problems.append(f"GEODMS_GUI_FILES in tools/setup_licences.py names {path}, which {GUI_QRC} does not "
                        "compile in: remove it from the list")

    rows, inside_rows, licences, qt_data = [], [], {}, {}
    counts, contained = {}, set()
    for rel, src in installed:
        name = os.path.basename(rel)
        inside = [i for i in INSIDE_GEODMS if name.lower() in i.installed]
        inside_rows += [(rel, i.what, i.licence, os.path.normpath(i.name)) for i in inside]
        contained |= {i.source for i in inside}
        if src is None:  # WriteUninstaller
            row = (rel, "NSIS uninstaller", "-", "nsis.txt")
            licences["nsis.txt"] = os.path.join(NSIS_ROOT, "COPYING")
        elif not is_binary(src):
            hit = qt.find(rel) if args.flavour != "l" else None
            if hit:
                qt_data.setdefault(hit[0], []).append(rel)
                licences[os.path.join("qt", hit[0]) + os.sep] = os.path.join(qt.src, hit[0], "LICENSES")
            elif FONT_FILE.fullmatch(name) and not inside and name.lower() not in GEODMS_FONTS:
                problems.append(f"the font {rel} is installed, but no licence text is known for it: it is not a font "
                                "of GeoDMS (GEODMS_FONTS), not a file of Qt and not an item of INSIDE_GEODMS in "
                                "tools/setup_licences.py")
            continue
        else:
            if name.lower() in GEODMS_DLLS or GEODMS_OTHER.fullmatch(name):
                row = (rel, "GeoDMS", version, gpl_rel)
            elif MSVC_RUNTIME.fullmatch(name):
                row = (rel, "Visual C++ runtime", "-", "-")
            else:
                candidates = [c for s in sources for c in s.candidate(rel)]
                same = [c for c in candidates if os.path.isfile(c.origin) and sha256(c.origin) == sha256(src)]
                if not same:
                    if candidates:
                        problems.append(f"{rel} differs from every file of that name in the sources of this flavour, "
                                        "so it is not a copy of any of them: "
                                        + ", ".join(f"{c.source} {c.version} ({c.origin})" for c in candidates))
                    else:
                        names = [SOURCE_NAMES[type(s)] for s in sources]
                        problems.append(f"no licence known for {rel}: not GeoDMS, not the Visual C++ runtime and "
                                        "not a copy of a file of " + ", ".join(names[:-1]) + " or " + names[-1])
                    continue
                c = same[0]
                row = (rel, c.source, c.version, c.licence)
                if isinstance(c.copy_from, tuple) and c.copy_from[0] == "conda":
                    licences[c.licence] = conda.licence_source(c.copy_from[1], problems)
                else:
                    licences[c.licence] = c.copy_from
        rows.append(row)
        kind = row[1].split(" ")[0] if not row[1].startswith("Visual") else "Visual C++ runtime"
        counts[kind] = counts.get(kind, 0) + 1

    for item in INSIDE_GEODMS:
        if item.source not in contained:
            problems.append(f"INSIDE_GEODMS in tools/setup_licences.py names {item.source}, but this setup installs "
                            f"none of the files that contain it ({', '.join(sorted(item.installed))}): correct those "
                            "names, or remove the item if GeoDMS no longer contains it")
    static = vcpkg.static_ports()
    for key, port_version, licence, src in static:
        licences[licence] = src
    licences = {k: v for k, v in licences.items() if v is not None}
    for licence, src in sorted(licences.items()):
        copy_licence(out, licence, src, problems)
    for item in INSIDE_GEODMS:
        src = os.path.join(REPO, *item.text.split("/"))
        if not os.path.isfile(src):
            problems.append(f"the licence text {item.text} of {item.source} does not exist")
            continue
        with open(src, encoding="utf-8") as f:
            text = f.read()
        dst = os.path.join(out, os.path.normpath(item.name))
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        with open(dst, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
    problems.stop_if_any("staging the licence texts", remove=out)

    rows.sort(key=lambda r: r[0].lower())
    inside_rows.sort(key=lambda r: (r[0].lower(), r[3]))
    write_readme(out, args.flavour, version, rows, inside_rows, [(k, v, l) for k, v, l, _ in static], qt_data,
                 vcpkg_root, qt.version if args.flavour != "l" else None)
    if args.deb_copyright:
        write_deb_copyright(args.deb_copyright, args.install_prefix or "/opt/ObjectVision/GeoDms" + version + ".l",
                            inside_rows)
    n_texts = sum(len(fs) for _, _, fs in os.walk(out)) - 1
    summary = ", ".join(f"{n} {k}" for k, n in sorted(counts.items()))
    print(f"setup_licences: {args.flavour}: {origin}, {len(installed)} installed files, "
          f"{len(rows)} binaries ({summary}); {len({r[0] for r in inside_rows})} files of GeoDMS that contain "
          f"material from other projects; {len(static)} vcpkg ports without a shared library; "
          f"{n_texts} licence files in {out}")
    if args.flavour != "l" and flavour["nsi"] and nsi.empty_nonfatal:
        print("setup_licences: /nonfatal File lines that match nothing: " + ", ".join(nsi.empty_nonfatal))


def write_readme(out, flavour, version, rows, inside_rows, static, qt_data, vcpkg_root, qt_version):
    with open(os.path.join(out, README), "w", encoding="utf-8", newline="\n") as f:
        f.write(TITLE + "\n\n")
        f.write(wrap(f"GeoDMS {version}, setup flavour {flavour}. GeoDMS is free software: you can redistribute it "
                     "and/or modify it under the terms of the GNU General Public License as published by the Free "
                     "Software Foundation, either version 3 of the License, or (at your option) any later version. "
                     f"The text of that licence is {GPL_FILE} in the installation folder."))
        f.write("\n")
        f.write(wrap("The binaries that this installation holds, each with where it comes from and the file or "
                     "folder, relative to this folder, with its licence text:"))
        f.write("\n")
        write_table(f, ("installed file", "comes from", "version", "licence text"), rows)
        if any(r[1] == "Visual C++ runtime" for r in rows):
            f.write("\n")
            f.write(wrap("The Visual C++ runtime is redistributable under the licence terms of Microsoft Visual "
                         "Studio and has no text of its own here."))
        if any(r[1] == "NSIS uninstaller" for r in rows):
            f.write("\n")
            f.write(wrap("The uninstaller is written by NSIS, the Nullsoft Scriptable Install System, whose "
                         "licence is nsis.txt."))
        if qt_version:
            f.write("\n")
            f.write(wrap(f"The Qt files come from Qt {qt_version}; the folder qt holds the LICENSES folder of the "
                         "source of each Qt module they belong to."
                         + "".join(f" The module {m} also supplies {len(fs)} data files, such as {fs[0]}."
                                   for m, fs in sorted(qt_data.items()) if not any(r[1] == f'Qt {m}' for r in rows))))
        f.write("\n")
        f.write(wrap("Some files of GeoDMS itself contain material from other projects, under the licence of that "
                     "project. Those files, each with what it contains, that licence and the file, relative to this "
                     "folder, with its text:"))
        f.write("\n")
        write_table(f, (INSIDE_HEADER, "contains", "licence", "licence text"), inside_rows)
        f.write("\n")
        f.write(wrap(f"The vcpkg ports of {os.path.basename(vcpkg_root)} below have no shared library of their own: "
                     "they are header-only or static libraries, and their code is compiled into the binaries above "
                     "that use them. Some of them may not be used by this installation at all. The Boost libraries "
                     "share one licence text."))
        f.write("\n")
        write_table(f, ("vcpkg port", "version", "licence text"), static)


def write_deb_copyright(path, prefix, inside_rows):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(wrap("GeoDMS is copyright Object Vision B.V. It is free software: you can redistribute it and/or "
                     "modify it under the terms of the GNU General Public License as published by the Free Software "
                     "Foundation, either version 3 of the License, or (at your option) any later version."))
        f.write("\n")
        f.write(wrap("On Debian systems, the complete text of the GNU General Public License version 3 can be found "
                     f"in /usr/share/common-licenses/GPL-3. This package also installs it as {prefix}/{GPL_FILE}."))
        f.write("\n")
        f.write(wrap(f"The package bundles libraries that GeoDMS uses: Qt and the libraries Qt needs, copied from "
                     "the Debian packages of the build machine, and the vcpkg ports that are linked into the "
                     f"libDm*.so libraries. {prefix}/{LICENCES_DIR}/{README} lists every binary of the package, "
                     "where it comes from and which file in that folder holds its licence text."))
        f.write("\n")
        parts = []
        for item in INSIDE_GEODMS:
            files = [r[0] for r in inside_rows if r[3] == os.path.normpath(item.name)]
            parts.append(f"{' and '.join(files)} contain{'s' if len(files) == 1 else ''} {item.what}, under the "
                         f"{item.licence}, whose text is {prefix}/{LICENCES_DIR}/{item.name}.")
        f.write(wrap(f"Some files of GeoDMS itself, in {prefix}, contain material from other projects. "
                     + " ".join(parts)))


# ---------------------------------------------------------------------------------------------
# check
# ---------------------------------------------------------------------------------------------

def read_tables(readme):
    """The rows of the tables in README.txt, per header: columns are separated by two or more spaces."""
    tables, current = {}, None
    with open(readme, encoding="utf-8") as f:
        lines = [line.rstrip("\n") for line in f]
    if not lines or lines[0] != TITLE:
        return None
    for line in lines:
        cells = re.split(r"\s{2,}", line.strip())
        if current is None:
            if cells[:1] in (["installed file"], [INSIDE_HEADER], ["vcpkg port"]):
                current = tables.setdefault(cells[0], [])
        elif not line.strip():
            current = None
        else:
            current.append(cells)
    return tables


def check(args):
    problems = Problems()
    inst = os.path.abspath(args.install)
    readme = os.path.abspath(args.table or os.path.join(inst, LICENCES_DIR, README))
    if not os.path.isfile(readme):
        sys.exit(f"setup_licences: no {readme}")
    tables = read_tables(readme)
    if tables is None or "installed file" not in tables:
        sys.exit(f"setup_licences: {readme} is not a table written by this script")
    base = os.path.dirname(readme)
    norm = os.path.normcase
    listed = {}
    for cells in tables["installed file"]:
        if len(cells) != 4:
            problems.append(f"{readme}: a row without four columns: {cells}")
            continue
        listed[norm(cells[0])] = cells
    licences_folder = os.path.join(inst, LICENCES_DIR)
    present = {norm(rel): rel for rel in walk_files(inst, skip=licences_folder)
               if is_binary(os.path.join(inst, rel))}
    for key in sorted(set(present) - set(listed)):
        problems.append(f"{present[key]} is installed but not in the table")
    for key in sorted(set(listed) - set(present)):
        problems.append(f"{listed[key][0]} is in the table but not installed")
    texts = {cells[3] for cells in listed.values() if cells[3] != "-"}
    texts |= {cells[2] for cells in tables.get("vcpkg port", []) if len(cells) == 3}
    inside = set()
    for cells in tables.get(INSIDE_HEADER, []):
        if len(cells) != 4:
            problems.append(f"{readme}: a row without four columns: {cells}")
            continue
        if not os.path.isfile(os.path.join(inst, cells[0])):
            problems.append(f"{cells[0]} is in the table of files of GeoDMS but not installed")
        inside.add((norm(cells[0]), norm(cells[3])))
        texts.add(cells[3])
    for rel in walk_files(inst, skip=licences_folder):
        for item in INSIDE_GEODMS:
            if os.path.basename(rel).lower() in item.installed and (norm(rel), norm(os.path.normpath(item.name))) not in inside:
                problems.append(f"{rel} contains {item.what}, but the table does not name its licence text {item.name}")
    texts.discard(os.path.join(os.pardir, GPL_FILE))  # checked below
    for lic in sorted(texts):
        p = os.path.join(base, lic)
        if not (os.path.isfile(p) or (lic.endswith(os.sep) and os.path.isdir(p) and os.listdir(p))):
            problems.append(f"the licence text {lic} of the table is not in {base}")
    if not os.path.isfile(os.path.join(inst, GPL_FILE)):
        problems.append(f"{inst} holds no {GPL_FILE}")
    if args.deb_copyright and not os.path.isfile(args.deb_copyright):
        problems.append(f"no {args.deb_copyright}")
    problems.stop_if_any(f"checking {inst}")
    print(f"setup_licences: {inst}: {len(present)} installed binaries, all in the table and the table holds no "
          f"other; every installed file that contains material from other projects has its {len(inside)} rows; "
          f"the {len(texts)} licence texts the table names (a Qt folder counts once) are present, and "
          + (f"so are {GPL_FILE} and {args.deb_copyright}" if args.deb_copyright else f"so is {GPL_FILE}"))


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    sub = parser.add_subparsers(dest="command", required=True)
    s = sub.add_parser("stage", help="write the licenses folder of a setup")
    s.add_argument("--flavour", required=True, choices=sorted(FLAVOURS))
    s.add_argument("--out", help="the licenses folder to write (default: where the NSIS script reads it, "
                                 "or <tree>/licenses)")
    s.add_argument("--define", action="append", default=[], help="NAME=VALUE, as makensis /D")
    s.add_argument("--tree", help="Linux: the staged installation folder")
    s.add_argument("--version", help="the GeoDMS version (default: RtcVersionNumbers.h)")
    s.add_argument("--vcpkg", help="the vcpkg_installed folder (default: that of the flavour)")
    s.add_argument("--qt", default=QT_ROOT, help="the Qt installation (default: %(default)s)")
    s.add_argument("--conda", help="GLOBIO: the conda environment (default: GLOBIO_ENV_ROOT)")
    s.add_argument("--deb-copyright", help="Linux: write the Debian copyright file here")
    s.add_argument("--install-prefix", help="Linux: the installation folder that the copyright file names")
    c = sub.add_parser("check", help="compare an installed setup with its table")
    c.add_argument("--install", required=True, help="the installation folder")
    c.add_argument("--table", help="the README.txt to compare with (default: <install>/licenses/README.txt)")
    c.add_argument("--deb-copyright", help="Linux: the Debian copyright file that must be installed")
    args = parser.parse_args()
    if args.command == "stage":
        stage(args)
    else:
        check(args)


if __name__ == "__main__":
    main()
