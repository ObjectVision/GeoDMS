#!/usr/bin/env bash
# Run the testcases battery and the unit test suite against the Linux CMake release build, as
# batch\TestReleaseUnit.bat does on Windows: first testcases/run_testcases.sh, then the unit suite of
# the tst repository, which also runs when the battery fails. Exit code 1 when either fails.
# Must be run from WSL2 or a native Linux environment.
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TST_BATCH="$(dirname "$SCRIPT_DIR")/tst/batch"
CONFIG=linux-x64-release

if [[ ! -d "$TST_BATCH" ]]; then
    echo "ERROR: tst/batch not found at $TST_BATCH"
    echo "       Clone the tst repository as a sibling of this repository."
    exit 1
fi

export GEODMS_ROOT="$SCRIPT_DIR"

echo "=== testcases battery on build/$CONFIG/bin ==="
bash "$SCRIPT_DIR/testcases/run_testcases.sh" "$SCRIPT_DIR/build/$CONFIG/bin/GeoDmsRun" --out-dir "$SCRIPT_DIR/testcases/_out_$CONFIG"
battery=$?

# Keep the GUI unit tests ON SCREEN (the WSLg display) but render via software GL.
# WSLg's GPU path (Mesa ZINK/d3d12) flakily blocks during EGL init -- the GeoDmsGuiQt
# test then hangs and gets SIGKILLed (exit 137), failing the whole suite even though
# every other test passes. Forcing llvmpipe avoids that GPU init while still drawing
# to the on-screen window. The GUI tests score computed text (value-info/
# classification), not pixels, so software vs GPU rendering give identical output.
# (Deliberately NOT QT_QPA_PLATFORM=offscreen -- that would render headless.)
export LIBGL_ALWAYS_SOFTWARE=1

bash "$TST_BATCH/unit_linux.sh" "$CONFIG"
unit=$?

if (( battery != 0 || unit != 0 )); then
    echo "*** FAILED: testcases battery exit $battery, unit suite exit $unit ***"
    exit 1
fi
