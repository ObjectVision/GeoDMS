#!/usr/bin/env bash
# =====================================================================
# Offline test of the .dms content the Linux setup ships, before it is packaged
# (GeoDMS-Test #24): the bash twin of TestShippedDms.bat, run inside WSL by
# batch/BuildSignAndCreateSetupLinux.bat right before nsi/CreateLinuxSetup.sh stages
# build/linux-x64-release/bin. Runs against that output folder, i.e. against the
# copies tools/DeployResources.cmake put there and the .deb and tarball carry.
#
# The same three steps as the .bat:
#   1. the shipped copy of the battery is current (nothing in testcases/ is newer);
#   2. every shipped examples/*.dms and library/**/*.dms is reached from a shipped
#      shipped_*.dms case by following #include lines (a %exeDir% path against the
#      output folder, a bare name against <dir>/<stem>/ then <dir>/), case-insensitively,
#      since the includes spell some names in lower case and the shipped files do not;
#   3. the battery's shipped_*.dms cases, run directly: the battery has no Linux runner.
#      Positives must exit 0; there are no _neg cases among them.
#
# Usage:  bash batch/TestShippedDms.sh [build/linux-x64-release/bin]
# Exit code: 0 when every step passes.
# =====================================================================
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${1:-$ROOT/build/linux-x64-release/bin}"
case "$BIN" in /*) ;; *) BIN="$ROOT/$BIN" ;; esac
BIN="${BIN%/}"
EXE="$BIN/GeoDmsRun"
COPY="$BIN/examples/testcases"
SRC="$ROOT/testcases"

if [[ ! -x "$EXE" ]]; then
    echo "*** GeoDmsRun not found or not executable in $BIN - build first, or pass the bin folder ***"
    exit 2
fi
if [[ ! -d "$COPY" || ! -d "$BIN/library" ]]; then
    echo "*** $COPY or $BIN/library is missing - the DeployResources step did not run ***"
    exit 2
fi

LOGDIR="$ROOT/scratch/shipped_dms"
mkdir -p "$LOGDIR"

echo
echo "=== Shipped .dms content, offline (GeoDMS-Test #24) ==="
echo "bin : $BIN"
echo "logs: $LOGDIR"
echo

# resolve a path case-insensitively, component by component; prints the real path or nothing
resolve_ci() {
    local base="$1" rel="$2" cur="$1" part hit
    rel="${rel//\\//}"
    IFS='/' read -r -a parts <<< "$rel"
    for part in "${parts[@]}"; do
        [[ -z "$part" ]] && continue
        hit="$(find "$cur" -mindepth 1 -maxdepth 1 -iname "$part" 2>/dev/null | head -n 1)"
        [[ -z "$hit" ]] && return 1
        cur="$hit"
    done
    printf '%s\n' "$cur"
}

# --- 1/3: the shipped copy of the battery is current -------------------------
echo "--- 1/3: the shipped copy of the battery is current ---"
COV_RC=0
stale=0
# Whole seconds, not `-nt`: CMake's file(COPY) preserves the source's mtime to the second
# only on Linux (fn_test.dms 21:09:31.365 becomes 21:09:31.000 in the copy), and bash 5
# compares nanoseconds, so `-nt` called every file of a freshly deployed copy stale (the
# 20.21.0.l build stopped on it). The Windows copies keep the full 100 ns stamp.
for src in "$SRC"/*.dms "$SRC"/*.txt "$SRC"/*.bat "$SRC"/*.ps1; do
    [[ -f "$src" ]] || continue
    dst="$COPY/$(basename "$src")"
    if [[ ! -f "$dst" ]]; then echo "    $(basename "$src") (missing)"; stale=1
    elif (( $(stat -c %Y "$src") > $(stat -c %Y "$dst") )); then echo "    $(basename "$src") (source is newer)"; stale=1
    fi
done
if [[ $stale -eq 1 ]]; then
    echo "*** the shipped copy of the battery in $COPY is not the one in $SRC - rebuild so DeployResources mirrors it ***"
    COV_RC=1
else
    echo "shipped battery copy is current: $(ls "$SRC"/*.dms | wc -l) cases"
fi

# --- 2/3: every shipped .dms is reached from a battery case -------------------
echo
echo "--- 2/3: every shipped .dms is reached from a shipped_*.dms case ---"
declare -A reached
queue=()
for c in "$COPY"/shipped_*.dms; do [[ -f "$c" ]] && queue+=("$c"); done
if [[ ${#queue[@]} -eq 0 ]]; then
    echo "*** no shipped_*.dms case in $COPY - the battery that covers the shipped content is not there ***"
    COV_RC=1
fi
while [[ ${#queue[@]} -gt 0 ]]; do
    f="${queue[0]}"; queue=("${queue[@]:1}")
    key="$(printf '%s' "$f" | tr '[:upper:]' '[:lower:]')"
    [[ -n "${reached[$key]:-}" ]] && continue
    reached[$key]=1
    while IFS= read -r inc; do
        [[ -z "$inc" ]] && continue
        inc="${inc//\\//}"
        if [[ "$inc" == %exeDir%/* ]]; then
            p="$(resolve_ci "$BIN" "${inc#%exeDir%/}")"
        elif [[ "$inc" == %* ]]; then
            continue
        else
            dir="$(dirname "$f")"; stem="$(basename "$f" .dms)"
            p="$(resolve_ci "$dir" "$stem/$inc")"
            [[ -z "$p" ]] && p="$(resolve_ci "$dir" "$inc")"
        fi
        if [[ -n "$p" && -f "$p" ]]; then
            queue+=("$p")
        else
            echo "*** $f includes $inc, which does not exist under $BIN ***"
            COV_RC=1
        fi
    done < <(grep -o -E '#include[[:space:]]*[<"][^>"]+[>"]' "$f" | sed -E 's/#include[[:space:]]*[<"]//; s/[>"]$//; s/[[:space:]]+$//')
done
total=0; missing=0
while IFS= read -r s; do
    total=$((total + 1))
    key="$(printf '%s' "$s" | tr '[:upper:]' '[:lower:]')"
    if [[ -z "${reached[$key]:-}" ]]; then
        echo "*** shipped but reached by no battery case: ${s#"$BIN"/} - add a testcases/shipped_*.dms that includes it ***"
        missing=$((missing + 1)); COV_RC=1
    fi
done < <( (find "$BIN/examples" -maxdepth 1 -name '*.dms'; find "$BIN/library" -name '*.dms') | sort )
echo "shipped .dms files outside the battery: $total, reached from the shipped_*.dms cases: $((total - missing))"

# --- 3/3: the shipped_*.dms cases, from the shipped copy ----------------------
echo
echo "--- 3/3: the shipped_*.dms cases of the battery, from $COPY ---"
BAT_RC=0
n=0
for cfg in "$COPY"/shipped_*.dms; do
    [[ -f "$cfg" ]] || continue
    n=$((n + 1))
    stem="$(basename "$cfg" .dms)"
    log="$LOGDIR/${stem}_linux.log"
    rm -f "$log"
    "$EXE" "/L$log" /S1 /S2 /S3 "$cfg" /checks > "$LOGDIR/${stem}_linux.out" 2>&1
    rc=$?
    if [[ $rc -eq 0 ]]; then echo "  $stem  ok"; else echo "  $stem  exit $rc  UNEXPECTED - see $log"; BAT_RC=1; fi
done
[[ $n -eq 0 ]] && BAT_RC=1

echo
echo "=== Shipped .dms content, offline: results for $BIN ==="
ANY=0
if [[ $COV_RC -eq 0 ]]; then echo "battery copy current and every shipped .dms reached: PASSED"; else echo "*** BATTERY COPY STALE OR A SHIPPED .DMS UNREACHED - see the lines above ***"; ANY=1; fi
if [[ $BAT_RC -eq 0 ]]; then echo "shipped_*.dms cases PASSED ($n)"; else echo "*** SHIPPED_*.DMS CASES FAILED - see the lines above ***"; ANY=1; fi
exit $ANY
