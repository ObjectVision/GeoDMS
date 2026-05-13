# #1104 Profiler bokeh series flat-lines for .l (Linux/WSL) — design

Issue: https://github.com/ObjectVision/GeoDMS/issues/1104

## Problem

`profiler/profiler.py` samples `psutil.Process(parent_pid).children(recursive=True)`.
For `.l` flavor the parent is `wsl.exe`, a Windows-side shim with no Windows
children — the actual `GeoDmsRun` lives inside the WSL VM, invisible to psutil
from Windows. All Bokeh series (cpu_percent, vms, num_threads, total_read_bytes,
total_write_bytes, cpu_curr_time) flatline at 0 even when the test allocated GBs.

## Chosen approach: mitigation option 3 (sampler inside WSL / Linux)

Preferred over option 1 (per-sample `wsl bash -c …` shell-out) because:

- 50–300 ms wsl-shim round-trip per sample skews CPU% delta calc by 10–30 %.
- Doesn't generalise to native-Linux GeoDMS installs (no `wsl` command).
- Option 3 uses `psutil`'s Linux backend → row-for-row identical column set to
  the Windows path, no downstream remap.

## Three refinements agreed in conversation

### 1. Sampling rate stays at 1 Hz

Long regressions (t720, t641 multi-hour). No sub-second sampling.

### 2. PID handoff via launcher wrapper, not `pgrep`

Avoid name-based discovery entirely. Reason: user often has standby
`GeoDmsGuiQt.exe` (and could have stray `GeoDmsRun`) running concurrently;
matching by name is unsafe.

```bash
# /opt/ObjectVision/GeoDms<ver>/profiler/run_with_sampler.sh
#!/bin/bash
CSV="$1"; shift
"$@" &
TARGET_PID=$!
python3 /opt/.../profiler/linux_sampler.py \
    --pid "$TARGET_PID" --out "$CSV" --sampling-rate 1.0 &
SAMPLER_PID=$!
wait "$TARGET_PID"; EXIT=$?
wait "$SAMPLER_PID" 2>/dev/null
exit "$EXIT"
```

Sampler then walks `psutil.Process(exact_pid).children(recursive=True)` —
same descendant-tree-by-PID guarantee as the Windows side already has.

Windows side note: existing code is **already** PID-based (children of the
cmd.exe wrapper this profiler spawned). Concurrent unrelated `GeoDmsGuiQt.exe`
processes are not descendants and are correctly ignored today.

Dead-code cleanup in this commit: delete `getProcessIdIfActive(names, …)` —
unused, name-based, would tempt future regressions.

### 3. Symmetric "sampler failed" annotation, both platforms

Today the Windows `getPerformance` loop silently swallows exceptions →
flat-zero render indistinguishable from "test did nothing". Same failure
mode on Linux when sampler can't start.

Plan:
- `exp.result["profiler_status"]` ∈ {"ok", "failed", "unavailable"}
- `exp.result["profiler_status_detail"]` — one-line cause string, e.g.
  `"WSL sampler exited rc=1: psutil import failed"`
  `"Windows psutil raised AccessDenied on child PID 12345"`
- Heuristic: failed if sample count < N for an experiment that ran ≥ M seconds.
- `VisualizeExperiments`: when `!= "ok"`, overlay a Bokeh `Label` with the
  detail and skip the line render. Status code + GeoDmsRun /L log overlay
  (Highest allocated/CommitCharge) untouched, so test-pass signal preserved.

## Implementation checklist (one commit when .l run finishes)

1. **NEW** `profiler/linux_sampler.py`
   - argparse: `--pid`, `--out`, `--sampling-rate` (default 1.0)
   - loop: `psutil.Process(pid).children(recursive=True)`, sum same fields
     as Windows path: cpu_percent, memory_percent, rss, vms, num_threads,
     total_read_bytes, total_write_bytes, net_connections, processes
   - CSV columns identical to today's `profile_log` keys so downstream merge
     is a one-liner
   - exits on target PID disappearance

2. **NEW** `profiler/run_with_sampler.sh` (shown above; chmod +x)

3. **EDIT** `profiler/profiler.py`
   - In `getPerformance`: detect `cmd_parts[0] in ("wsl", "wsl.exe")`. For
     that branch prepend `run_with_sampler.sh <csv> --` before the
     `GeoDmsRun` invocation. Skip the Windows sample loop. After
     `parent_process.wait()`, read the CSV into `profile_log`.
   - Wrap both Windows and Linux paths with try/except that populates
     `profiler_status` + `profiler_status_detail`.
   - Delete `getProcessIdIfActive` (dead, name-based).
   - **Tree-kill on timeout, not single-process kill** — see next section.

## Bundled: tree-kill on 7200 s timeout (orphan wsl.exe fix)

Tabled together with #1104 because both edits live in `profiler/profiler.py`
and the changes touch the same `getPerformance` function — one iteration.

### Problem (observed 2026-05-12)

`t641_3_RSopen_Allocatie` ran 23:55:25 → ~01:55:30, just touching the default
`timeout=7200` (2 h). The timeout branch executes:

```python
if (time_measurement_start - experiment_start_time).total_seconds() > timeout:
    parent_process.kill()
```

`psutil.Process.kill()` on Windows is `TerminateProcess(handle, 1)` — a
**single-process** kill of the cmd.exe wrapper only. The wsl.exe child and
its peer wsl.exe (the WSL distro launcher pair) survive as orphans. The
console window persists because conhost keeps it alive as long as any
attached client (an orphan wsl.exe) still holds the console handle. Inside
the VM, GeoDmsRun runs to natural completion with no Windows-side parent.

Net symptoms: ghost "t641_3_RSopen_Allocatie" console window, two stale
wsl.exe processes that block `wsl --shutdown` from being a clean cleanup
(it would also kill the next test).

### Fix

In `getPerformance`, replace the single-process kill with a tree kill that
also signals wsl.exe to terminate its distro side cleanly:

```python
if (time_measurement_start - experiment_start_time).total_seconds() > timeout:
    # Tree-kill: cmd.exe's children include wsl.exe shims that won't die
    # on their own when the parent is TerminateProcess'd. Walk descendants
    # first so we don't lose visibility into them when the parent dies.
    try:
        descendants = parent_process.children(recursive=True)
    except psutil.NoSuchProcess:
        descendants = []
    for p in descendants:
        try:
            p.kill()
        except psutil.NoSuchProcess:
            pass
    try:
        parent_process.kill()
    except psutil.NoSuchProcess:
        pass
    # Best-effort: also ask wsl.exe to forcibly stop the inner Linux
    # process so the Linux side doesn't keep allocating after we've
    # given up on it. Only matters on Windows / Qt builds where cmd_parts
    # starts with "wsl".
    if cmd_parts and cmd_parts[0] in ("wsl", "wsl.exe"):
        subprocess.run(["taskkill", "/T", "/F", "/PID",
                        str(parent_process.pid)],
                       capture_output=True, check=False)
    break  # exit the sampling loop; parent_process.wait() will return next
```

### Notes / edge cases

- `taskkill /T /F /PID <cmd.pid>` is redundant with the manual walk above
  but is a belt-and-braces cleanup in case the manual walk misses a grand-
  child that was reparented mid-walk. Cheap to keep.
- The "kill the Linux GeoDmsRun" part is best-effort. WSL2's process model
  doesn't always forward SIGKILL across the VM bus when wsl.exe is killed;
  the safest thing is `wsl --shutdown` but that kills *all* WSL — too
  aggressive for our sequential-test driver. Accept that the inner Linux
  process may run a bit longer after timeout; the orphan-shim part is
  what actually matters for cleanup.
- The break statement is important: without it the sampling loop continues
  spinning on a now-dead parent_process and the eventual `parent_process.wait()`
  is the right termination point.

### Test plan addition

- Force a short timeout (e.g. `timeout=10` kwarg) on a long test and confirm:
  (a) cmd.exe, wsl.exe shims all gone after the kill
  (b) no lingering console window with the test's title
  (c) full.py advances to the next test as before

4. **EDIT** `profiler/profiler.py::VisualizeExperiments`
   - Read `profiler_status`; overlay `Label` with detail and skip the
     line render when not "ok".

5. **.deb packaging** (nsi/CreateLinuxSetup.sh and/or cpack config)
   - Add `python3`, `python3-psutil` to `Depends`.
   - Install `linux_sampler.py` + `run_with_sampler.sh` into
     `/opt/ObjectVision/GeoDms<ver>/profiler/`.

6. **Future native-Linux readiness** (no code now)
   - One-line branch: `if sys.platform == "linux": prefix = []` to run the
     sampler locally without `wsl --`. Defer until the use case is live.

## Test plan

- `python full.py -version 20.0.0.l -tests t405_2` → confirm Bokeh series
  show real CPU / memory / IO matching GeoDmsRun /L "Highest allocated" line.
- `python full.py -version 20.0.0.c -tests t010` → confirm Windows path
  unchanged (regression-equivalent output).
- Force a failure path (rename `linux_sampler.py` temporarily) → confirm the
  HTML renders the "WSL sampler exited rc=…" overlay, status code still OK.
- Concurrent stray `GeoDmsRun` outside the profiler → confirm sampler ignores
  it (PID-based, not name-based).

## Cross-refs

- Conversation context lives in OVSRV10 session as of 2026-05-11.
- Related state: `project_local_regression_run_state.md` (.l run in progress
  at time of design discussion).
