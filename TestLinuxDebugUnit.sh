#!/usr/bin/env bash
# Run the testcases battery and the unit test suite against the Linux CMake debug build, as
# batch\TestDebugUnit.bat does on Windows. The steps are those of TestLinuxReleaseUnit.sh, which
# this script runs with the preset linux-x64-debug, so that the debug and the release build are
# tested in the same way. Exit code 1 when the battery or the unit suite fails.
# Must be run from WSL2 or a native Linux environment.
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec bash "$SCRIPT_DIR/TestLinuxReleaseUnit.sh" linux-x64-debug
