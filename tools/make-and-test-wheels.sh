#!/usr/bin/env bash
# Builds and tests the Linux wheels of the geodms module (#1355), the bash twin of
# tools/make-and-test-wheels.ps1; batch/BuildSignAndCreateSetupLinux.bat runs it in WSL on
# build/linux-x64-release/bin before CreateLinuxSetup.sh.
#
# usage: tools/make-and-test-wheels.sh <output folder> <dist folder> <version>
#
# 1. Makes a venv with the packages of python/wheel-tools.txt, kept in
#    ~/.local/share/ObjectVision/GeoDMS/wheel-tools and made again when that file changes. Making it
#    downloads the packages from PyPI; a venv that is up to date needs no network.
# 2. First removes the Linux wheels of the same version that an earlier run left in the dist folder,
#    then has tools/make-wheels.py --flavour l write one wheel per Python version listed in
#    python/PythonVersions.txt from the output folder into the dist folder.
# 3. twine check --strict on those wheels: the metadata checks that PyPI applies to an upload.
# 4. Each wheel is installed in a fresh venv of its own CPython (python3.XY on PATH; on Ubuntu 24.04
#    3.13 and 3.14 come from the deadsnakes PPA), made on the Linux file system, and
#    python/tst/UnitTests.py --installed must pass there with GEODMS_PYDIR and LD_LIBRARY_PATH unset,
#    so that only the wheel can supply the engine.
# 5. The wheel for the Python of the system (/usr/bin/python3) is installed once more, in a venv
#    made with --system-site-packages so that it sees the system's osgeo (on Ubuntu python3-gdal,
#    with Ubuntu's GDAL), and python/tst/LinuxGdalCompatibility.py --installed must pass there in
#    its four import orders, the one that loads osgeo with RTLD_GLOBAL included (#1381). Without an
#    osgeo the step is skipped, with a line that says so.
#
# Stops with exit code 1 at the first failure.
set -euo pipefail

if [ $# -ne 3 ]; then
    echo "usage: $0 <output folder> <dist folder> <version>" >&2
    exit 1
fi
OUTPUT_DIR=$(realpath "$1")
mkdir -p "$2"
DIST=$(realpath "$2")
VERSION=$3

# The CPython that runs auditwheel, wheel and twine; the versions in python/wheel-tools.txt were
# tested under it.
TOOL_PYTHON_VERSION=3.13

REPO=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
TOOLS_FILE=$REPO/python/wheel-tools.txt
MAKE_WHEELS=$REPO/tools/make-wheels.py
TEST_SCRIPT=$REPO/python/tst/UnitTests.py
GDAL_TEST=$REPO/python/tst/LinuxGdalCompatibility.py
SYSTEM_PYTHON=/usr/bin/python3
TOOL_VENV=$HOME/.local/share/ObjectVision/GeoDMS/wheel-tools
TEST_ROOT=$(mktemp -d -t geodms-wheel-test-XXXXXX)
STARTED=$SECONDS
trap 'rm -rf "$TEST_ROOT"' EXIT
trap 'echo "*** WHEELS FAILED: line $LINENO: $BASH_COMMAND (exit $?) ***"; exit 1' ERR

IFS=';' read -r -a MINORS <<< "$(tr -d ' \r\n' < "$REPO/python/PythonVersions.txt")"

# 1. the tool venv
if [ ! -x "$TOOL_VENV/bin/python" ] || ! cmp -s "$TOOLS_FILE" "$TOOL_VENV/wheel-tools.txt"; then
    rm -rf "$TOOL_VENV"
    "python$TOOL_PYTHON_VERSION" -m venv "$TOOL_VENV"
    "$TOOL_VENV/bin/python" -m pip install --quiet -r "$TOOLS_FILE"
    cp "$TOOLS_FILE" "$TOOL_VENV/wheel-tools.txt"
fi
TOOL_PYTHON=$TOOL_VENV/bin/python

# 2. the wheels
rm -f "$DIST/geodms-$VERSION"-cp*-manylinux_*.whl "$DIST/geodms-$VERSION"-cp*-linux_*.whl
"$TOOL_PYTHON" "$MAKE_WHEELS" --flavour l --bin "$OUTPUT_DIR" --out "$DIST" --version "$VERSION"
WHEELS=()
for minor in "${MINORS[@]}"; do
    abi=cp${minor/./}
    found=("$DIST/geodms-$VERSION-$abi-$abi"-manylinux_*.whl)
    if [ ${#found[@]} -ne 1 ] || [ ! -f "${found[0]}" ]; then
        echo "*** WHEELS FAILED: make-wheels.py wrote no single manylinux wheel for $abi in $DIST ***"
        exit 1
    fi
    WHEELS+=("${found[0]}")
done

# 3. the metadata
"$TOOL_PYTHON" -m twine check --strict "${WHEELS[@]}"

# 4. each wheel in a fresh venv, with nothing else that could supply the engine
unset GEODMS_PYDIR LD_LIBRARY_PATH
for i in "${!MINORS[@]}"; do
    minor=${MINORS[$i]}
    venv=$TEST_ROOT/venv${minor/./}
    "python$minor" -m venv "$venv"
    "$venv/bin/python" -m pip install --quiet --no-deps --no-index "${WHEELS[$i]}"
    (cd "$TEST_ROOT" && "$venv/bin/python" -I -u "$TEST_SCRIPT" --installed)
done

# 5. next to the GDAL of osgeo (#1381)
system_minor=$("$SYSTEM_PYTHON" -c 'import sys; print("%d.%d" % sys.version_info[:2])' 2>/dev/null || true)
osgeo_tested=0
if [ -n "$system_minor" ] && "$SYSTEM_PYTHON" -c 'import osgeo' 2>/dev/null; then
    for i in "${!MINORS[@]}"; do
        [ "${MINORS[$i]}" = "$system_minor" ] || continue
        venv=$TEST_ROOT/venv_osgeo
        "$SYSTEM_PYTHON" -m venv --system-site-packages "$venv"
        "$venv/bin/python" -m pip install --quiet --no-deps --no-index "${WHEELS[$i]}"
        for order in osgeo-global-first geodms-first-osgeo-global osgeo-first geodms-first; do
            (cd "$TEST_ROOT" && "$venv/bin/python" -I -u "$GDAL_TEST" "$order" --installed)
        done
        osgeo_tested=1
    done
fi
if [ $osgeo_tested -eq 0 ]; then
    echo "*** NOT TESTED next to another GDAL (#1381): $SYSTEM_PYTHON has no osgeo, or is not one of the Python versions of the wheels; on Ubuntu: sudo apt install python3-gdal ***"
fi

echo "=== wheels of $VERSION built and tested in $((SECONDS - STARTED)) s: ${WHEELS[*]} ==="
