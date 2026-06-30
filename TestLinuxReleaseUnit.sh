#!/usr/bin/env bash
# Run the unit test suite against the Linux CMake release build.
# Must be run from WSL2 or a native Linux environment.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TST_BATCH="$(dirname "$SCRIPT_DIR")/tst/batch"

if [[ ! -d "$TST_BATCH" ]]; then
    echo "ERROR: tst/batch not found at $TST_BATCH"
    echo "       Clone the tst repository as a sibling of this repository."
    exit 1
fi

export GEODMS_ROOT="$SCRIPT_DIR"

# Render the GUI unit tests (GeoDmsGuiQt) headless. WSL/CI hosts expose a WSLg
# display (WAYLAND_DISPLAY) but no usable GPU, so Qt's default EGL/Mesa path
# blocks indefinitely (libEGL / "MESA ZINK: failed to choose pdev" / "failed to
# create dri2 screen"). Forcing the offscreen platform makes gui_instance.sh run
# the test without a GPU/display instead of hanging. Only affects Qt apps;
# GeoDmsRun ignores it. (#1134)
export QT_QPA_PLATFORM=offscreen

bash "$TST_BATCH/unit_linux.sh" linux-x64-release
