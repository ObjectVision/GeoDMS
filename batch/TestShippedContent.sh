#!/usr/bin/env bash
# =====================================================================
# Release test for the .dms content the Linux setup ships, download included: the bash twin of
# TestShippedContent.bat (#1360), run inside WSL by batch/BuildSignAndCreateSetupLinux.bat on
# build/linux-x64-release/bin before nsi/CreateLinuxSetup.sh packages it.
#
# Two steps, as in the .bat:
#   1. batch/TestShippedDms.sh, the offline part (GeoDMS-Test #24);
#   2. the shipped examples/grid_to_polygon.dms over the real CBS buurt map, which
#      library/basedata_nl/RegioIndelingen.dms downloads with curl.
#
# Step 2 runs from a copy of the output folder in a temporary folder on the Linux file system,
# as step 3 of TestShippedDms.sh does when the output folder ignores case: from a Windows drive,
# WSL finds a file named in the wrong case anyway (#1359). Its SourceDataDir is an empty folder
# beside that copy, given to GeoDmsRun as GEODMS_directories_SourceDataDir, so the download
# always runs and no existing SourceData is read or touched. The temporary folder is removed at
# the end, also when the test fails.
#
# Step 2 needs the network and MIN_MEM_GB of memory in WSL. Without either it fails, and so does
# the setup that calls it: there is no skip. With too little memory it fails before it starts;
# without a network curl cannot download the geopackage, and the summary says so.
#
# Usage:  bash batch/TestShippedContent.sh [build/linux-x64-release/bin] [cbs-year] [temp-parent]
#   temp-parent is the folder in which the temporary folder is made, $TMPDIR or /tmp when not
#   given; it must be on the Linux file system. The copy of the output folder takes about
#   0.5 GB there and the geopackage about 170 MB while the test runs.
# Exit code: 0 when both steps pass, 1 when one fails, 2 when step 2 cannot start.
# =====================================================================
set -u

# The memory that WSL must have for step 2, in GB, compared with MemTotal in /proc/meminfo.
# GeoDmsRun peaked at 12.1 GB resident on 2026-11-17 (CBS Gebiedsindelingen 2025, Release build
# in WSL), so 14 GB leaves about 2 GB for the rest of the WSL machine. WSL gets half of the
# memory of Windows unless memory= in %UserProfile%\.wslconfig says otherwise, and MemTotal is
# a little less than that (110.0 GB with memory=112GB), which is why the minimum is not 16 GB.
MIN_MEM_GB="${GEODMS_SHIPPED_CONTENT_MIN_MEM_GB:-14}"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${1:-$ROOT/build/linux-x64-release/bin}"
case "$BIN" in /*) ;; *) BIN="$ROOT/$BIN" ;; esac
BIN="${BIN%/}"
CBSYEAR="${2:-2025}"   # must track the year examples/grid_to_polygon.dms asks for (/CBS/Y<year>/Buurt)
PARENT="${3:-${TMPDIR:-/tmp}}"

if [[ ! -x "$BIN/GeoDmsRun" ]]; then
    echo "*** GeoDmsRun not found or not executable in $BIN; build first, or pass the bin folder ***"
    exit 2
fi
if [[ ! -f "$BIN/examples/grid_to_polygon.dms" ]]; then
    echo "*** $BIN/examples/grid_to_polygon.dms is missing: the DeployResources step did not run ***"
    exit 2
fi
if [[ ! -d "$PARENT" ]]; then
    echo "*** the folder $PARENT for the temporary folder does not exist ***"
    exit 2
fi

echo
echo "=== Shipped content release test (Linux) ==="
echo "bin                        : $BIN"
echo "temporary folder made under: $PARENT"
echo

# --- 1/2: the offline part ---------------------------------------------------
echo "--- 1/2: shipped .dms content, offline (TestShippedDms.sh) ---"
TC_RC=0
bash "$ROOT/batch/TestShippedDms.sh" "$BIN" || TC_RC=1

# --- 2/2: grid_to_polygon over a geopackage downloaded into an empty SourceDataDir
echo
echo "--- 2/2: examples/grid_to_polygon.dms over the real CBS buurt map ---"
LOG="$ROOT/scratch/grid_to_polygon_release_linux.log"
mkdir -p "$ROOT/scratch"
rm -f "$LOG"

WORK=""
cleanup() { if [[ -n "$WORK" && -d "$WORK" ]]; then rm -rf "$WORK"; fi; }
trap cleanup EXIT

GPKG_INFO="was not downloaded, because the step did not start"
step2() {
    local mem_kb need_kb run sdd gpkg rc
    mem_kb=$(awk '/^MemTotal:/ { print $2 }' /proc/meminfo)
    need_kb=$((MIN_MEM_GB * 1024 * 1024))
    if (( mem_kb < need_kb )); then
        echo "*** WSL has $(awk -v k="$mem_kb" 'BEGIN { printf "%.1f", k / 1048576 }') GB of memory, and this step needs $MIN_MEM_GB GB:"
        echo "    give WSL more with memory= in %UserProfile%\\.wslconfig, then run wsl --shutdown and the setup again ***"
        return 2
    fi

    WORK="$(mktemp -d "$PARENT/geodms_shipped_content_XXXXXX")" || { echo "*** could not make a temporary folder in $PARENT ***"; return 2; }
    run="$WORK/bin"
    sdd="$WORK/SourceData"
    mkdir "$run" "$sdd"
    echo "copying $BIN to $run"
    if ! cp -a "$BIN/." "$run/"; then
        echo "*** could not copy $BIN to $run ***"
        return 2
    fi
    if [[ -e "$run/GEODMSRUN" ]]; then
        echo "*** $PARENT ignores case; pass a temp-parent on the Linux file system ***"
        return 2
    fi
    # The RUNPATH of a build-tree binary names the build folder, so the copy would load the
    # libraries of $BIN, and %exeDir% follows libDmRtc.so back there (see TestShippedDms.sh).
    export LD_LIBRARY_PATH="$run${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    if ! ldd "$run/GeoDmsRun" | grep -q "=> $run/libDmRtc"; then
        echo "*** the copy in $run does not load its own libDmRtc.so; see ldd $run/GeoDmsRun ***"
        return 2
    fi

    gpkg="$sdd/RegionalUnits/cbs/gebiedsindelingen$CBSYEAR.gpkg"
    echo "SourceDataDir : $sdd (new and empty, removed after the run)"
    echo "CBS geopackage: $gpkg"
    echo "log           : $LOG"

    # The example is the top-level container of its own configuration and carries its own
    # /checks, as in TestShippedContent.bat. GNU time, when there, reports the peak memory.
    if [[ -x /usr/bin/time ]]; then
        GEODMS_directories_SourceDataDir="$sdd" /usr/bin/time -v -o "$WORK/time.txt" \
            "$run/GeoDmsRun" "/L$LOG" "$run/examples/grid_to_polygon.dms" /checks
        rc=$?
        local rss_kb wall sig
        rss_kb=$(awk -F': ' '/Maximum resident set size/ { print $2 }' "$WORK/time.txt")
        wall=$(awk -F': ' '/Elapsed \(wall clock\) time/ { print $2 }' "$WORK/time.txt")
        [[ -n "$rss_kb" ]] && echo "GeoDmsRun: exit $rc after $wall (wall clock), peak resident memory $(awk -v k="$rss_kb" 'BEGIN { printf "%.1f", k / 1048576 }') GB"
        sig=$(grep -o 'terminated by signal [0-9]*' "$WORK/time.txt")
        [[ -n "$sig" ]] && echo "*** GeoDmsRun was $sig; signal 9 is most often the out-of-memory killer of WSL (see dmesg) ***"
    else
        GEODMS_directories_SourceDataDir="$sdd" "$run/GeoDmsRun" "/L$LOG" "$run/examples/grid_to_polygon.dms" /checks
        rc=$?
        echo "GeoDmsRun: exit $rc"
    fi

    if [[ -f "$gpkg" ]]; then
        GPKG_INFO="was downloaded: $(stat -c %s "$gpkg") bytes"
    else
        GPKG_INFO="was not downloaded (see the message of curl in $LOG; has WSL no network?)"
    fi
    [[ $rc -eq 0 ]] && return 0 || return 1
}

step2
G2P_RC=$?
cleanup
if [[ -n "$WORK" ]]; then
    if [[ -e "$WORK" ]]; then WORK_INFO="NOT removed; delete it by hand"; else WORK_INFO="removed"; fi
fi

echo
echo "=== Shipped content release test (Linux): results for $BIN ==="
if [[ $TC_RC -eq 0 ]]; then echo "shipped .dms content (offline part) PASSED"; else echo "*** SHIPPED .DMS CONTENT (offline part) FAILED; see the TestShippedDms.sh results above ***"; fi
case $G2P_RC in
    0) echo "grid_to_polygon PASSED" ;;
    2) echo "*** grid_to_polygon COULD NOT START; see the message above ***" ;;
    *) echo "*** grid_to_polygon FAILED; see $LOG"
       echo "    the measured counts and areas are on the 'grid_to_polygon:' line in that log ***" ;;
esac
[[ -f "$LOG" ]] && grep -m1 -o 'grid_to_polygon:.*' "$LOG"
echo "the CBS geopackage $GPKG_INFO"
[[ -n "$WORK" ]] && echo "the temporary folder $WORK is $WORK_INFO"

[[ $TC_RC -ne 0 || $G2P_RC -eq 1 ]] && exit 1
[[ $G2P_RC -ne 0 ]] && exit 2
exit 0
