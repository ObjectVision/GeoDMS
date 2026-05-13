#!/usr/bin/env python3
"""Linux-side performance sampler for GeoDMS regression tests.

Companion to profiler.py: when the regression runs under WSL (or on a
native Linux host), the Windows-side psutil sees only the wsl.exe shim,
not the GeoDmsRun process living inside the VM. The Bokeh series for the
.l flavor therefore flatlines at zero (issue #1104).

This script runs *inside* the same OS as the target GeoDmsRun and samples
the process tree via psutil's Linux backend. The CSV it writes uses the
exact same column set as profiler.getPerformance's profile_log dict, so
the Windows side just reads the CSV and drops the rows in place.

Usage:
    python3 linux_sampler.py --pid <int> --out <csv-path>
                             [--sampling-rate <float>]
                             [--max-runtime <int>]

Behaviour:
  * Walks the target's descendant tree (psutil.Process(pid).children(
    recursive=True)) and sums per-sample stats across all descendants
    plus the target itself.
  * Writes one CSV row per sample.
  * Exits cleanly (rc=0) when the target PID disappears.
  * Exits with rc=2 if psutil is missing (caller should treat that as
    "sampler unavailable" and overlay a placeholder series).
  * Exits with rc=3 on argparse failure or unreachable target PID.

Why not just emit one big JSON at the end? CSV streams: a row is flushed
on each sample, so even if the sampler is force-killed mid-run we still
have a usable partial series. JSON would need a full re-read to recover.
"""

import argparse
import csv
import os
import signal
import sys
import time
from datetime import datetime

try:
    import psutil
except ImportError:
    sys.stderr.write(
        "linux_sampler: psutil is required (apt install python3-psutil "
        "or pip install psutil)\n"
    )
    sys.exit(2)


# Keep in lockstep with profiler.getPerformance's profile_log dict.
CSV_FIELDS = [
    "time",            # ISO-8601 wall clock at sample
    "dtime",           # seconds since experiment start
    "cpu_percent",     # 0..(100*ncpu) normalised to 0..100 (sum/ncpu)
    "cpu_curr_time",   # cumulative cores-seconds, running sum of cpu_p/100
    "memory_percent",  # process.memory_percent() summed across descendants
    "rss",             # GB
    "vms",             # GB
    "num_threads",
    "total_read_bytes",   # GB cumulative
    "total_write_bytes",  # GB cumulative
    "net_connections",
    "processes",       # count: target + descendants alive at this sample
]


def _read_one(proc: "psutil.Process"):
    """Return per-process stats or None if the process is gone. Matches
    profiler.getProcessCurrentStatistics but stays in Linux-only territory."""
    try:
        with proc.oneshot():
            cpu_p_raw = proc.cpu_percent()
            mem_p = proc.memory_percent()
            mi = proc.memory_info()
            rss = mi.rss * 1e-9
            vms = mi.vms * 1e-9
            nt = proc.num_threads()
            io = proc.io_counters()
            rb = io.read_bytes * 1e-9
            wb = io.write_bytes * 1e-9
            # net_connections requires root or CAP_NET_ADMIN on Linux for
            # non-owned processes; for our case (sampler runs as the same
            # user that launched GeoDmsRun) it works. Fall back to 0 if
            # the kernel rejects the request. psutil 6.0+ renamed
            # `connections` to `net_connections`; Ubuntu 24.04 ships 5.9.x
            # which only has the old name -- try the new name first, then
            # fall back so we work across both.
            _net_fn = getattr(proc, "net_connections", None) or getattr(proc, "connections", None)
            try:
                nc = len(_net_fn(kind="all")) if _net_fn else 0
            except (psutil.AccessDenied, OSError):
                nc = 0
            return (cpu_p_raw, mem_p, rss, vms, nt, rb, wb, nc)
    except (psutil.NoSuchProcess, psutil.ZombieProcess, psutil.AccessDenied):
        return None


def _gather(target: "psutil.Process"):
    """Sum across target + all live descendants. Returns the tuple to
    append to the CSV, or None if the target itself is gone."""
    if not target.is_running():
        return None

    try:
        children = target.children(recursive=True)
    except (psutil.NoSuchProcess, psutil.ZombieProcess):
        return None

    procs = [target] + children

    # Dry-run cpu_percent on every proc once before the real read: psutil
    # returns 0.0 on the first call and a real delta on subsequent calls,
    # so a single sample with no prior reading is always reported as 0%.
    # The Windows path does the same dance (profiler.py:241-249).
    for p in procs:
        try:
            p.cpu_percent()
        except (psutil.NoSuchProcess, psutil.ZombieProcess):
            pass

    return procs


def _sample(procs):
    """Second pass after the warm-up: read real stats and aggregate."""
    cpu_p_raw = 0.0
    mem_p = 0.0
    rss = 0.0
    vms = 0.0
    nt = 0
    rb = 0.0
    wb = 0.0
    nc = 0
    alive = 0
    for p in procs:
        r = _read_one(p)
        if r is None:
            continue
        cpu_p_raw += r[0]
        mem_p += r[1]
        rss += r[2]
        vms += r[3]
        nt += r[4]
        rb += r[5]
        wb += r[6]
        nc += r[7]
        alive += 1
    return (cpu_p_raw, mem_p, rss, vms, nt, rb, wb, nc, alive)


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--pid", type=int, required=True,
                    help="Target process PID (the GeoDmsRun process).")
    ap.add_argument("--out", required=True,
                    help="Output CSV path.")
    ap.add_argument("--sampling-rate", type=float, default=1.0,
                    help="Seconds between samples. Default 1.0.")
    ap.add_argument("--max-runtime", type=int, default=0,
                    help="Hard upper bound on runtime in seconds; 0 = none. "
                         "Belt-and-braces in case target.is_running() somehow "
                         "stays True forever.")
    args = ap.parse_args(argv)

    try:
        target = psutil.Process(args.pid)
    except psutil.NoSuchProcess:
        sys.stderr.write(
            f"linux_sampler: pid {args.pid} not found at startup\n"
        )
        return 3

    ncpu = psutil.cpu_count() or 1
    start = datetime.now()
    cpu_curr_time = 0.0

    # SIGTERM / SIGINT just break the loop cleanly so the CSV is flushed.
    interrupted = {"flag": False}

    def _stop(signum, frame):
        interrupted["flag"] = True

    signal.signal(signal.SIGTERM, _stop)
    signal.signal(signal.SIGINT, _stop)

    out_dir = os.path.dirname(args.out)
    if out_dir and not os.path.isdir(out_dir):
        os.makedirs(out_dir, exist_ok=True)

    with open(args.out, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(CSV_FIELDS)

        while True:
            if interrupted["flag"]:
                break

            t0 = time.monotonic()
            now = datetime.now()
            dt = (now - start).total_seconds()

            if args.max_runtime and dt > args.max_runtime:
                break

            procs = _gather(target)
            if procs is None:
                break

            # The first sample's cpu_percent is always 0 (psutil's required
            # warm-up). Sleep one sample interval so the real reading lands
            # in the second iteration.
            time.sleep(min(0.1, args.sampling_rate))
            cpu_p_raw, mem_p, rss, vms, nt, rb, wb, nc, alive = _sample(procs)

            cpu_p = cpu_p_raw / ncpu
            cpu_curr_time += cpu_p / 100.0

            writer.writerow([
                now.isoformat(),
                f"{dt:.3f}",
                f"{cpu_p:.4f}",
                f"{cpu_curr_time:.4f}",
                f"{mem_p:.4f}",
                f"{rss:.6f}",
                f"{vms:.6f}",
                nt,
                f"{rb:.6f}",
                f"{wb:.6f}",
                nc,
                alive,
            ])
            f.flush()

            elapsed = time.monotonic() - t0
            remaining = args.sampling_rate - elapsed
            if remaining > 0:
                time.sleep(remaining)

    return 0


if __name__ == "__main__":
    sys.exit(main())
