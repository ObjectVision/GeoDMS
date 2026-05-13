#!/bin/bash
# run_with_sampler.sh -- launch a target process and a Linux-side performance
# sampler against it as siblings, then wait on the target. Used by profiler.py
# when the test command runs under WSL (or future native Linux) so the Bokeh
# series shows real CPU / memory / IO instead of flatlines (issue #1104).
#
# Usage:
#   run_with_sampler.sh <csv-path> <sampling-rate> -- <target> [args...]
#
# The first three positional args are consumed by this script; everything
# after `--` is exec'd as the target command. The target's PID is handed
# to the sampler via --pid so there is no name-based pgrep guessing —
# concurrent GeoDmsRun instances or unrelated long-running processes
# cannot be sampled by accident.
#
# Exit code: the target's exit code.

set -u

if [ "$#" -lt 4 ]; then
    echo "usage: $0 <csv-path> <sampling-rate> -- <target> [args...]" >&2
    exit 2
fi

CSV="$1"
RATE="$2"
SEP="$3"
shift 3

if [ "$SEP" != "--" ]; then
    echo "$0: expected '--' separator before target command, got '$SEP'" >&2
    exit 2
fi

# Locate the sampler next to this script. Symlinks resolved so the
# .deb-installed /opt/ObjectVision/GeoDms<ver>/profiler/ layout works.
SCRIPT_DIR="$(cd -- "$(dirname -- "$(readlink -f "$0")")" && pwd)"
SAMPLER="$SCRIPT_DIR/linux_sampler.py"

if [ ! -f "$SAMPLER" ]; then
    echo "$0: linux_sampler.py not found at $SAMPLER" >&2
    # Run the target anyway -- the regression should still produce a result;
    # only the Bokeh series will be empty.
    exec "$@"
fi

# Spawn the target as a backgrounded job so we know its PID for the sampler.
"$@" &
TARGET_PID=$!

# Spawn the sampler as a sibling. Its lifetime is bounded by the target's
# existence -- linux_sampler.py exits when target.is_running() flips False.
python3 "$SAMPLER" --pid "$TARGET_PID" --out "$CSV" --sampling-rate "$RATE" &
SAMPLER_PID=$!

# Wait on the target; capture its exit code.
wait "$TARGET_PID"
TARGET_RC=$?

# Give the sampler a moment to notice and write its last row, then collect it.
# kill -0 is a probe (no signal sent); the wait that follows reaps the proc.
if kill -0 "$SAMPLER_PID" 2>/dev/null; then
    # Sampler should self-terminate now that target.is_running() is False;
    # nudge it with TERM if it doesn't notice within 3 s.
    for _ in 1 2 3; do
        sleep 1
        if ! kill -0 "$SAMPLER_PID" 2>/dev/null; then
            break
        fi
    done
    if kill -0 "$SAMPLER_PID" 2>/dev/null; then
        kill -TERM "$SAMPLER_PID" 2>/dev/null || true
    fi
fi
wait "$SAMPLER_PID" 2>/dev/null || true

exit "$TARGET_RC"
