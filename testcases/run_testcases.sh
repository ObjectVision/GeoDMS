#!/usr/bin/env bash
# Run the testcases battery (testcases/*.dms) through GeoDmsRun on Linux and classify each case as
# pass/fail: the bash twin of run_testcases.ps1, by the same rules (#1384). That file explains them;
# in short:
#
#   Positives (no '_neg' part in the name) must exit 0.
#   Negatives ('_neg' as a part of the name, followed by digits, '_' or the end, or the
#   'fn_test_defcheck' definition-check case) must exit nonzero, and every fragment of their lines
#   '// EXPECT-FAILURE: <fragment>' must occur in a message of their output, or they fail as
#   WRONG-FAILURE. expect_failure_missing.txt lists the negatives that have no such line.
#   Exit code 3 (a Debug assertion) is always a failure.
#   The item of a case comes from fnrun_itemmap.txt, /checks when it names none.
#
# What differs from run_testcases.ps1:
#   - A case with a line '// WINDOWS-ONLY: <reason>' in its .dms is not run, and is reported as
#     skipped(windows-only), which does not fail the run. run_testcases.ps1 runs it as any other case.
#   - The memory limit applies to the resident memory of the process (VmHWM in /proc/<pid>/status),
#     because Linux has no limit on the commit of one process. The script reads it every POLL_SEC
#     while the case runs and kills the process when it reaches the limit, as LIMIT(memory); the
#     peak it reports is the highest value it read.
#   - The cases run sorted by name byte by byte (the C locale), where PowerShell sorts by culture:
#     fn_test_opsigA.dms comes before fn_test_opsigA_neg1.dms here and after it there. The pairs of
#     cases that must run in order differ in a digit, as stor_shp_ringclose_1_write.dms and
#     stor_shp_ringclose_2_read.dms do, and run in the same order in both.
#   - The output folder is _out_linux by default, so that it does not mix with a Windows run of the
#     same tree.
#   - When UBSAN_OPTIONS names a log_path, as batch/TestLinuxUbsan.sh sets it for a build with gcc's
#     undefined-behaviour checks (#1385), each case writes its reports to <log_path>.<case>.<pid>.
#
# From a folder on a Windows drive, such as /mnt/c in WSL, a case finds a file that it names in the
# wrong case, as on Windows, and a Linux file system would not (#1359); batch/TestShippedDms.sh
# checks that for the shipped cases.
#
# Usage: bash run_testcases.sh [path/to/GeoDmsRun] [--out-dir <folder>] [--max-mem-gb <GB>] [--timeout-sec <s>]
# The default GeoDmsRun is ../build/linux-x64-release/bin/GeoDmsRun beside this folder in the
# repository, else ../../GeoDmsRun, where a setup installs it (this battery ships as
# examples/testcases). The limits are 8 GB and 300 s by default; 0 switches one off.
# Exit code: 0 if every case matched its expected outcome, 1 otherwise, 2 when it cannot start.
set -u
if [[ -z "${EPOCHREALTIME:-}" ]]; then echo "ERROR: run_testcases.sh needs bash 5 or later"; exit 2; fi

# How often, in seconds, the memory and the wall clock of a running case are read.
POLL_SEC=0.05

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXE=""
OUT_DIR=""
MAX_MEM_GB=8
TIMEOUT_SEC=300
while [[ $# -gt 0 ]]; do
    case "$1" in
        --out-dir)     OUT_DIR="${2:?--out-dir needs a folder}"; shift 2 ;;
        --max-mem-gb)  MAX_MEM_GB="${2:?--max-mem-gb needs a number}"; shift 2 ;;
        --timeout-sec) TIMEOUT_SEC="${2:?--timeout-sec needs a number}"; shift 2 ;;
        -*)            echo "ERROR: unknown option $1"; exit 2 ;;
        *)             EXE="$1"; shift ;;
    esac
done

if [[ -z "$EXE" ]]; then
    EXE="$HERE/../build/linux-x64-release/bin/GeoDmsRun"
    [[ -x "$EXE" ]] || EXE="$HERE/../../GeoDmsRun"
fi
if [[ ! -x "$EXE" || -d "$EXE" ]]; then
    echo "ERROR: GeoDmsRun not found at $EXE."
    echo "Build linux-x64-release first, or pass the path of GeoDmsRun as the first argument."
    exit 2
fi
EXE="$(cd "$(dirname "$EXE")" && pwd)/$(basename "$EXE")"

[[ -n "$OUT_DIR" ]] || OUT_DIR="$HERE/_out_linux"
if ! mkdir -p "$OUT_DIR" 2>/dev/null || [[ ! -w "$OUT_DIR" ]]; then
    # installed under /opt: the suite folder is not writable, so the output goes to the temporary folder
    OUT_DIR="${TMPDIR:-/tmp}/GeoDmsTestcases_out"
    mkdir -p "$OUT_DIR" || exit 2
    echo "NOTE: the output goes to $OUT_DIR (the suite folder is not writable)"
fi
OUT_DIR="$(cd "$OUT_DIR" && pwd)"

# GeoDMS takes the environment variable GEODMS_directories_LocalDataDir before the LocalDataDir setting.
mkdir -p "$OUT_DIR/LocalData"
export GEODMS_directories_LocalDataDir="$OUT_DIR/LocalData"

MAX_MEM_KB=$(awk -v g="$MAX_MEM_GB" 'BEGIN { printf "%d", g * 1048576 }')
TIMEOUT_US=$(awk -v s="$TIMEOUT_SEC" 'BEGIN { printf "%d", s * 1000000 }')

# Hash keys in lower case, since a PowerShell hashtable compares its keys without regard to case.
declare -A MAP
if [[ -f "$HERE/fnrun_itemmap.txt" ]]; then
    while IFS= read -r line || [[ -n "$line" ]]; do
        read -ra p <<< "${line%$'\r'}"
        if (( ${#p[@]} >= 2 )); then MAP["${p[0],,}"]="${p[*]:1}"; fi
    done < "$HERE/fnrun_itemmap.txt"
fi

# The negative cases without an EXPECT-FAILURE line, with the reason.
UNCHECKED_NAME=expect_failure_missing.txt
declare -A UNCHECKED UNCHECKED_AS_LISTED
if [[ -f "$HERE/$UNCHECKED_NAME" ]]; then
    while IFS= read -r line || [[ -n "$line" ]]; do
        read -r name reason <<< "${line%$'\r'}"
        [[ -z "${name:-}" || "$name" == \#* ]] && continue
        UNCHECKED["${name,,}"]="${reason:-(no reason given)}"
        UNCHECKED_AS_LISTED["${name,,}"]="$name"
    done < "$HERE/$UNCHECKED_NAME"
fi

# The fragments of the lines '// <TAG>: <fragment>' of a .dms, one per line, trimmed, without the CR of
# a Windows line end and the byte order mark that a few cases start with.
tag_lines() {
    sed -e '1s/^\xEF\xBB\xBF//' -e 's/\r$//' "$2" \
        | sed -n -E "s#^[[:space:]]*//[[:space:]]*$1:(.*)\$#\\1#p" \
        | sed -E 's/^[[:space:]]+//; s/[[:space:]]+$//'
}

# Reads a case's output like run_testcases.ps1: collects the lines of every message, its first line
# (one that starts with 'Failure:', 'Last ErrMsg:' or '[E]') and the lines that continue it, up to a
# line that starts with '[', with 'Context:' or with 'Update [['. In the output of a Debug build, where
# the CRT copy of each line comes first, the console's copy behind it is read (from_debug, the twin of
# ConvertFrom-DebugLine). Prints a line 'MISSING<tab><fragment>' for each fragment of the file frags
# that no such line contains, and then 'FIRST<tab><the first message line>'.
read -r -d '' JUDGE_AWK <<'AWK'
function from_debug(line,   rest, len, k) {
    if (match(line, /\[[^]]\]\[[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9] [0-9][0-9]:[0-9][0-9]:[0-9][0-9]\]\[[0-9]+\]\[/) && RSTART > 1)
        return substr(line, RSTART)
    if (match(line, /^(\[[a-z]+\])?[A-Za-z]+: /)) {
        rest = substr(line, RLENGTH + 1)
        len = length(rest)
        if (len >= 3 && (len - 3) % 2 == 0) {
            k = (len - 3) / 2
            if (substr(rest, k + 1, 3) == "   " && substr(rest, 1, k) == substr(rest, k + 4))
                return "   " substr(rest, k + 4)
        }
    }
    return line
}
BEGIN {
    n = 0
    while ((getline f < frags) > 0) frag[++n] = f
    close(frags)
    inmsg = 0; nl = 0
}
{
    sub(/\r$/, "")
    l = from_debug($0)
    if (l ~ /^(Failure:|Last ErrMsg:|\[E\])/) { msg[++nl] = l; inmsg = 1; next }
    if (inmsg && l ~ /^(\[|[[:space:]]*Context:|Update \[\[)/) inmsg = 0
    if (inmsg) msg[++nl] = l
}
END {
    for (i = 1; i <= n; i++) {
        found = 0
        for (j = 1; j <= nl; j++) if (index(msg[j], frag[i])) { found = 1; break }
        if (!found) print "MISSING\t" frag[i]
    }
    print "FIRST\t" (nl ? msg[1] : "(none)")
}
AWK

# Runs a GeoDmsRun under the limits, with all of its output to the file $1, in the current folder, in a
# session of its own, so that a limit kills whatever it started as well. Sets RG_CODE, RG_LIMIT (empty,
# memory or time), RG_NOTE, RG_PEAK_KB and RG_US (its wall-clock time in microseconds).
run_guarded() {
    local out=$1; shift
    local t0=${EPOCHREALTIME//[.,]/} now key val _ state
    RG_LIMIT=""; RG_NOTE=""; RG_PEAK_KB=0
    setsid "$@" > "$out" 2>&1 < /dev/null &
    local pid=$!
    while :; do
        state=""
        while read -r key val _; do
            case "$key" in
                State:) state=$val ;;
                VmHWM:) (( val > RG_PEAK_KB )) && RG_PEAK_KB=$val ;;
            esac
        done 2>/dev/null < "/proc/$pid/status"
        [[ -z "$state" || "$state" == Z ]] && break
        now=${EPOCHREALTIME//[.,]/}
        if (( MAX_MEM_KB > 0 && RG_PEAK_KB >= MAX_MEM_KB )); then RG_LIMIT=memory; break; fi
        if (( TIMEOUT_US > 0 && now - t0 >= TIMEOUT_US )); then RG_LIMIT=time; break; fi
        sleep "$POLL_SEC"
    done
    [[ -n "$RG_LIMIT" ]] && { kill -KILL -- "-$pid" 2>/dev/null || kill -KILL "$pid" 2>/dev/null; }
    wait "$pid"
    RG_CODE=$?
    now=${EPOCHREALTIME//[.,]/}
    RG_US=$(( now - t0 ))
    if [[ -n "$RG_LIMIT" ]]; then
        if [[ "$RG_LIMIT" == memory ]]; then
            RG_NOTE=$(awk -v m="$MAX_MEM_GB" -v p="$RG_PEAK_KB" 'BEGIN { printf "memory limit of %s GB per process reached (peak resident %.2f GB), process killed", m, p / 1048576 }')
        else
            RG_NOTE="wall-clock limit of $TIMEOUT_SEC s reached, process killed"
        fi
        RG_NOTE+=$(awk -v c="$RG_CODE" -v us="$RG_US" 'BEGIN { printf "; exit %d after %.1f s", c, us / 1000000 }')
        printf '\n*** testcases guard: %s\n' "$RG_NOTE" >> "$out"
    fi
}

R_CONFIG=(); R_VERDICT=(); R_EXIT=(); R_ITEM=(); R_DETAIL=()
add_result() { R_CONFIG+=("$1"); R_VERDICT+=("$2"); R_EXIT+=("$3"); R_ITEM+=("$4"); R_DETAIL+=("$5"); }

declare -A NEG_SEEN
PEAK_KB=0; PEAK_CASE=""; LONGEST_US=0; LONGEST_CASE=""
UBSAN_OPTIONS_OF_CALLER="${UBSAN_OPTIONS:-}"
UBSAN_LOG_PATH=$(sed -n 's/^\(.*:\)\{0,1\}log_path=\([^:]*\).*$/\2/p' <<< "$UBSAN_OPTIONS_OF_CALLER")
FRAGS="$OUT_DIR/.expect_fragments"

mapfile -t NAMES < <(cd "$HERE" && for f in *.dms; do [[ -f "$f" ]] && printf '%s\n' "$f"; done | LC_ALL=C sort)
for name in "${NAMES[@]}"; do
    stem=${name%.dms}
    cfg="$HERE/$name"
    if [[ -n "${MAP[${name,,}]+set}" ]]; then read -ra item_args <<< "${MAP[${name,,}]}"; else item_args=(/checks); fi
    item="${item_args[*]}"
    lstem=${stem,,}
    is_neg=0
    if [[ "$lstem" =~ _neg[0-9]*(_|$) || "$lstem" == fn_test_defcheck ]]; then is_neg=1; NEG_SEEN["$lstem"]=1; fi

    windows_only=$(tag_lines WINDOWS-ONLY "$cfg" | head -n 1)
    if [[ -n "$windows_only" ]]; then
        add_result "$stem" "skipped(windows-only)" "" "$item" "$windows_only"
        continue
    fi

    mapfile -t expects < <(tag_lines EXPECT-FAILURE "$cfg")
    out="$OUT_DIR/$stem.out"
    # On a build with gcc's undefined-behaviour checks, batch/TestLinuxUbsan.sh names a log_path in
    # UBSAN_OPTIONS; each case adds its name to it, so that its reports say which case made them.
    if [[ -n "$UBSAN_LOG_PATH" ]]; then export UBSAN_OPTIONS="$UBSAN_OPTIONS_OF_CALLER:log_path=$UBSAN_LOG_PATH.$stem"; fi
    run_guarded "$out" "$EXE" "/L$OUT_DIR/$stem.log" "$cfg" "${item_args[@]}"
    code=$RG_CODE
    if (( RG_PEAK_KB > PEAK_KB )); then PEAK_KB=$RG_PEAK_KB; PEAK_CASE=$stem; fi
    if (( RG_US > LONGEST_US )); then LONGEST_US=$RG_US; LONGEST_CASE=$stem; fi
    [[ -n "$RG_LIMIT" ]] && echo "LIMIT: $stem: $RG_NOTE" # straight away, so that a run which seemed to stall says why

    has_empty=0
    for f in "${expects[@]}"; do [[ -z "$f" ]] && has_empty=1; done
    detail=""
    if [[ -n "$RG_LIMIT" ]];                     then verdict="LIMIT($RG_LIMIT)"
    elif (( code == 3 ));                        then verdict=ASSERT
    elif (( is_neg != (code != 0) ));            then verdict=UNEXPECTED
    elif (( ! is_neg )); then
        if (( ${#expects[@]} )); then verdict=BAD-EXPECT; detail="a positive case with an EXPECT-FAILURE line"
        else verdict=ok; fi
    elif (( has_empty ));                        then verdict=BAD-EXPECT; detail="an EXPECT-FAILURE line without a fragment"
    elif [[ -n "${UNCHECKED[$lstem]+set}" ]]; then
        if (( ${#expects[@]} )); then verdict=LISTED-HAS-EXPECT; detail="listed in $UNCHECKED_NAME"
        else verdict="ok(neg,unchecked)"; fi
    elif (( ! ${#expects[@]} ));                 then verdict=NO-EXPECT
    else
        printf '%s\n' "${expects[@]}" > "$FRAGS"
        judged=$(awk -v frags="$FRAGS" "$JUDGE_AWK" "$out")
        missing=$(printf '%s\n' "$judged" | sed -n 's/^MISSING\t//p' | head -n 1)
        if [[ -n "$missing" ]]; then
            verdict=WRONG-FAILURE
            first=$(printf '%s\n' "$judged" | sed -n 's/^FIRST\t//p')
            (( ${#first} > 200 )) && first="${first:0:200}..."
            detail="not found: '$missing'; first error line: $first"
        else
            verdict="ok(neg)"
        fi
    fi
    add_result "$stem" "$verdict" "$code" "$item" "$detail"
done
rm -f "$FRAGS"

for lname in $(printf '%s\n' "${!UNCHECKED[@]}" | LC_ALL=C sort); do
    [[ -n "${NEG_SEEN[$lname]+set}" ]] && continue
    add_result "${UNCHECKED_AS_LISTED[$lname]}" LISTED-UNKNOWN "" "" "listed in $UNCHECKED_NAME, but no negative case has this name"
done

# Prints the results whose index is given, as a table.
print_table() {
    local w=6 i
    for i in "$@"; do (( ${#R_CONFIG[i]} > w )) && w=${#R_CONFIG[i]}; done
    printf "%-${w}s %-21s %4s %s\n" config verdict exit item
    printf "%-${w}s %-21s %4s %s\n" ------ ------- ---- ----
    for i in "$@"; do printf "%-${w}s %-21s %4s %s\n" "${R_CONFIG[i]}" "${R_VERDICT[i]}" "${R_EXIT[i]}" "${R_ITEM[i]}"; done
    echo
}

ALL=(); BAD=(); NEGS=(); SKIPPED=()
for i in "${!R_CONFIG[@]}"; do
    ALL+=("$i")
    case "${R_VERDICT[i]}" in
        ok|"ok(neg)"|"ok(neg,unchecked)") ;;
        "skipped(windows-only)") SKIPPED+=("$i") ;;
        *) BAD+=("$i") ;;
    esac
    [[ -n "${NEG_SEEN[${R_CONFIG[i],,}]+set}" ]] && NEGS+=("$i")
done
print_table "${ALL[@]}"

n_neg_ok=0; n_neg_unchecked=0
for i in "${NEGS[@]}"; do
    [[ "${R_VERDICT[i]}" == "ok(neg)" ]] && n_neg_ok=$((n_neg_ok + 1))
    [[ "${R_VERDICT[i]}" == "ok(neg,unchecked)" ]] && n_neg_unchecked=$((n_neg_unchecked + 1))
done
mem_text=$( (( MAX_MEM_KB > 0 )) && echo "$MAX_MEM_GB GB" || echo none)
time_text=$( (( TIMEOUT_US > 0 )) && echo "$TIMEOUT_SEC s" || echo none)
awk -v mem="$mem_text" -v tim="$time_text" -v p="$PEAK_KB" -v pc="$PEAK_CASE" -v us="$LONGEST_US" -v lc="$LONGEST_CASE" 'BEGIN {
    printf "GUARD per GeoDmsRun: resident memory limit %s, wall-clock limit %s; highest peak resident %.2f GB (%s), longest run %.1f s (%s)\n", mem, tim, p / 1048576, pc, us / 1000000, lc }'
echo "NEGATIVES=${#NEGS[@]}: $n_neg_ok failed with their EXPECT-FAILURE message, $n_neg_unchecked listed in $UNCHECKED_NAME and judged on the exit code only"
echo "SKIPPED=${#SKIPPED[@]}: cases with a WINDOWS-ONLY line"
echo "TOTAL=${#ALL[@]} BAD=${#BAD[@]}"
if (( ${#BAD[@]} )); then
    echo "FAILED CASES:"
    print_table "${BAD[@]}"
    seen=" "
    for i in "${BAD[@]}"; do
        [[ -n "${R_DETAIL[i]}" ]] && echo "${R_VERDICT[i]} ${R_CONFIG[i]}: ${R_DETAIL[i]}"
        seen+="${R_VERDICT[i]} "
    done
    [[ "$seen" == *" WRONG-FAILURE "* ]]     && echo "A WRONG-FAILURE failed, but not with the message that its EXPECT-FAILURE lines expect: compare them with <config>.out in $OUT_DIR."
    [[ "$seen" == *" NO-EXPECT "* ]]         && echo "A NO-EXPECT is a negative case without an EXPECT-FAILURE line: add one with a fragment of the message it is written to fail with."
    [[ "$seen" == *" LISTED-HAS-EXPECT "* ]] && echo "A LISTED-HAS-EXPECT has an EXPECT-FAILURE line and is still listed in $UNCHECKED_NAME: remove its line there."
    [[ "$seen" == *" LISTED-UNKNOWN "* ]]    && echo "A LISTED-UNKNOWN is a name in $UNCHECKED_NAME that is no negative case (any more): remove its line there."
    exit 1
fi
echo "ALL TESTCASES PASSED"
exit 0
