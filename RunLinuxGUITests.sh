#!/usr/bin/env bash
# Linux equivalent of RunGUITests.bat — runs DPGeneral GUI tests via WSLg
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TST_DIR="/mnt/c/dev/tst"
BUILD_CONFIG="${1:-linux-x64-debug}"
BUILD_DIR="$SCRIPT_DIR/build/$BUILD_CONFIG/bin"

LOCAL_DATA_DIR="${GEODMS_LOCAL_DATA:-$HOME/geodms_localdata}"
RESULT_DIR="$LOCAL_DATA_DIR/GeoDMSTestResults"
RESULT_FOLDER="$RESULT_DIR/Unit/GUI"
RESULT_SUMMARY="$RESULT_DIR/unit/gui_test_run.txt"

mkdir -p "$RESULT_FOLDER" "$RESULT_DIR/unit/gui"

export ResultFolder="$RESULT_FOLDER"
export GEODMS_directories_LocalDataDir="$LOCAL_DATA_DIR"

GEODMS_GUI="$BUILD_DIR/GeoDmsGuiQt"
GEODMS_RUN="$BUILD_DIR/GeoDmsRun"

PASS_COUNT=0
FAIL_COUNT=0

run_gui_test() {
    local test_name="$1"
    local dmsscript="$2"
    local cfg="$3"
    local tmp_file="$RESULT_FOLDER/${test_name}.tmp"
    local log_file="$RESULT_DIR/unit/gui/${test_name}_error.txt"
    local compare_cfg="$4"
    local compare_item="$5"

    echo "======================================="
    echo "Running $test_name (GUI)"
    echo "======================================="
    rm -f "$tmp_file"

    "$GEODMS_GUI" "/T$dmsscript" /S1 /S2 /S3 "$cfg" test_log
    echo "GUI exit: $?"

    echo "======================================="
    echo "Running $test_name (compare)"
    echo "======================================="
    "$GEODMS_RUN" /S1 /S2 /S3 "$compare_cfg" "$compare_item"
    local rc=$?
    if [[ $rc -eq 0 ]]; then
        echo "PASS: $test_name"
        echo "PASS: $test_name" >> "$RESULT_SUMMARY"
        PASS_COUNT=$((PASS_COUNT + 1))
        [[ -f "$log_file" ]] && cat "$log_file"
    else
        echo "FAIL: $test_name (exit $rc)"
        echo "FAIL: $test_name" >> "$RESULT_SUMMARY"
        FAIL_COUNT=$((FAIL_COUNT + 1))
        [[ -f "$log_file" ]] && cat "$log_file"
    fi
    echo
}

rm -f "$RESULT_SUMMARY"
echo "GUI Test Run for $BUILD_CONFIG" > "$RESULT_SUMMARY"

run_gui_test \
    "DPGeneral_missing_file_error" \
    "$TST_DIR/dmsscript/DPGeneral_missing_file_error.dmsscript" \
    "$TST_DIR/Unit/GUI/cfg/DPGeneral_missing_file_error.dms" \
    "$TST_DIR/Unit/GUI/cfg/DPGeneral_missing_file_error.dms" \
    "test_log"

run_gui_test \
    "DPGeneral_explicit_supplier_error" \
    "$TST_DIR/dmsscript/DPGeneral_explicit_supplier_error.dmsscript" \
    "$TST_DIR/Unit/GUI/cfg/DPGeneral_explicit_supplier_error.dms" \
    "$TST_DIR/Unit/GUI/cfg/DPGeneral_explicit_supplier_error.dms" \
    "test_log"

echo "=== Done ==="
cat "$RESULT_SUMMARY"
[[ $FAIL_COUNT -eq 0 ]]
