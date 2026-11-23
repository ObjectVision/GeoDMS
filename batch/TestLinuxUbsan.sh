#!/usr/bin/env bash
# =====================================================================
# The Linux tests on the build with gcc's checks for undefined behaviour (#1385), which
#     cmake --preset linux-x64-ubsan && cmake --build --preset linux-x64-ubsan
# makes in build/linux-x64-ubsan/bin. The checks are compiled into the engine's own code and report
# while it runs a member function called on a null pointer, a reference bound to null, signed integer
# overflow, a shift out of range, a misaligned pointer and the other checks of -fsanitize=undefined,
# each with its source line and stack. The vptr check is off; the top CMakeLists.txt says why.
#
# The tests, each of which also runs when one before it failed:
#   unit     TestLinuxReleaseUnit.sh linux-x64-ubsan: the testcases battery (testcases/run_testcases.sh)
#            and the unit suite of the tst repository;
#   shipped  batch/TestShippedContent.sh: the shipped .dms content, and examples/grid_to_polygon.dms
#            over the CBS buurt map, which it downloads, so it needs the network;
#   python   python/tst/UnitTests.py with the module of the output folder, and
#            python/tst/LinuxGdalCompatibility.py in each of its five orders, which needs the osgeo
#            of Ubuntu's python3-gdal.
#
# Every process writes its reports to a file of its own, <report folder>/<test>/ubsan.<pid>, through
# log_path in UBSAN_OPTIONS (a case of the battery adds its name: ubsan.<case>.<pid>), and goes on after
# a report, as gcc does by default, so that one round lists every report. A report does not change the
# exit code of the process that makes it. tools/ubsan_reports.py then lists each place in the code that
# was reported once, in <report folder>/summary.txt.
#
# Usage: bash batch/TestLinuxUbsan.sh [report folder]
#   report folder: ~/geodms_ubsan when not given, on the Linux file system; it is emptied first. Not under /tmp,
#   which WSL empties when it stops, as it does soon after the last process in it has ended.
# Exit code: 0 when every test passes and nothing is reported, 1 when a test fails, 2 when every test
#   passes but something is reported, 3 when the build is missing or has no undefined-behaviour checks.
# =====================================================================
set -u

PRESET=linux-x64-ubsan
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$ROOT/build/$PRESET/bin"
REPORTS="${1:-$HOME/geodms_ubsan}"

if [[ ! -x "$BIN/GeoDmsRun" ]]; then
    echo "*** $BIN/GeoDmsRun not found: build it with cmake --preset $PRESET && cmake --build --preset $PRESET ***"
    exit 3
fi
if ! readelf -d "$BIN/libDmRtc.so" | grep -q 'libubsan'; then
    echo "*** $BIN/libDmRtc.so does not link libubsan: that is not a build of the preset $PRESET ***"
    exit 3
fi
rm -rf "$REPORTS" && mkdir -p "$REPORTS" || exit 3
REPORTS="$(cd "$REPORTS" && pwd)"

# print_stacktrace: the stack of each report; report_error_type: the SUMMARY line names the check.
ubsan_options() { echo "print_stacktrace=1:report_error_type=1:log_path=$REPORTS/$1/ubsan"; }

declare -A RESULT
ORDER=()
run_test() {
    local name=$1; shift
    mkdir -p "$REPORTS/${name%% *}"
    echo
    echo "=== $name ==="
    UBSAN_OPTIONS="$(ubsan_options "${name%% *}")" "$@"
    RESULT["$name"]=$?
    ORDER+=("$name")
}

echo "=== Linux tests with gcc's undefined-behaviour checks ==="
echo "bin    : $BIN"
echo "reports: $REPORTS"

run_test unit bash "$ROOT/TestLinuxReleaseUnit.sh" "$PRESET"
run_test shipped bash "$ROOT/batch/TestShippedContent.sh" "$BIN"
run_test "python UnitTests.py" env GEODMS_PYDIR="$BIN" python3 "$ROOT/python/tst/UnitTests.py"
for order in gdal-preloaded osgeo-global-first geodms-first-osgeo-global osgeo-first geodms-first; do
    run_test "python LinuxGdalCompatibility.py $order" python3 "$ROOT/python/tst/LinuxGdalCompatibility.py" "$order" "$BIN"
done

python3 "$ROOT/tools/ubsan_reports.py" "$REPORTS" --root "$ROOT" > "$REPORTS/summary.txt"
reported=$?

echo
echo "=== Summary ==="
failed=0
for name in "${ORDER[@]}"; do
    if (( RESULT["$name"] == 0 )); then echo "  pass  $name"; else echo "  FAIL  $name (exit ${RESULT[$name]})"; failed=1; fi
done
echo
cat "$REPORTS/summary.txt"
echo
if (( failed )); then echo "*** a test failed ***"; exit 1; fi
if (( reported == 2 )); then echo "*** every test passed, and gcc's checks reported undefined behaviour: $REPORTS/summary.txt ***"; exit 2; fi
if (( reported != 0 )); then echo "*** tools/ubsan_reports.py failed (exit $reported) ***"; exit 1; fi
echo "Every test passed, and gcc's checks reported nothing."
exit 0
