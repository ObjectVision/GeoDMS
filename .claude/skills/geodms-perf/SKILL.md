---
name: geodms-perf
description: Investigating GeoDMS performance, on a single operator or on the full.py regression round. How to compare rounds per experiment (wall time and peak commit), how to rerun one full.py experiment or chain on any engine build without full.py, why a slower round is first checked by an interleaved A/B against a kept build before anything is bisected, keeping engine copies in C:\LocalData\GeoDMS_engine, measuring an operator change on a probe, which machine a figure comes from (OVSRV10 and OVSRV05 differ and are never compared with each other), and where findings are recorded. Use when a regression round looks slower, when asked to measure or bisect a performance change, or before writing a timing into a commit, issue or release note.
---

# Investigating GeoDMS performance

Policy for builds and test runs is in geodms-build (ask first, check that the tree is quiet); this
skill is the measuring on top of it. The scripts are in `scripts/` beside this file; run them with the
user's Python 3.13 (`C:\Users\MaartenHilferink\AppData\Local\Programs\Python\Python313\python.exe`),
since they unpickle full.py's experiment files.

## Every figure carries its machine

The regression rounds run on more than one machine, and the machines do not give comparable times:

| Machine | Memory | Role |
|---|---|---|
| OVSRV10 | 127 GB, 32 logical processors | development and coordinator machine; builds run here, other sessions share it |
| OVSRV05 | 64 GB, Ryzen 9 5900X, 24 logical processors | dedicated, idle performance machine |

A model that commits more than the RAM (t641 commits up to 190 GB) pages on both, but differently, and
an effect seen on one has been absent on the other (`doc/performance-test.md`, 20.22.0 reading: the
t641.1 loss of OVSRV05 did not show on OVSRV10). So:

- Write the machine with every figure: in a table heading, a commit body, an issue, a release note
  ("on OVSRV10"). `hostname` tells which one you are on.
- Compare times only within one machine. A ratio across machines says nothing about the engine.
- A result folder of full.py does not record its machine in its name for the local rounds
  (`20_22_1_m`); the report and `doc/performance-test.md` do. Check before comparing two folders.

## Is it the engine? Interleaved A/B first

A slower full.py round is a hypothesis, not a regression. On 2026-09-28 a "t810 regression" was
bisected for hours and turned out to be machine drift (load from other sessions in the morning); on
2026-10-01 t020 took 6.4 min in a round against 3.1 min for 20.22.0, and an A/B of the same two builds
gave 219 to 226 s against 220 to 228 s. The round had run next to a disk scan of the agent's own. The
same 20.22.0 binary took 28.7 min for t641_1 in its round of 27-09 and 34.7 min in an A/B of 01-10: on
OVSRV10 the spread between days is about 20 % for a model that commits more than the RAM, more than any
engine change of that release (`doc/performance-test.md`, Results (OVSRV10), 20.22.1).

So before any bisect:

1. Keep the builds to compare as copies: `robocopy bin\Release\x64 C:\LocalData\GeoDMS_engine\<name> /E /XF *.pdb *.lib *.exp`.
   Name them after the commit (`head_d46a6e199`, `bisect_1ec17232f`). A copy outside `bin\` also keeps a
   long run from holding the DLLs that the next build wants to link.
2. Run the experiment on both, interleaved (A, B, A, B), with nothing else heavy on the machine:
   `run_exp.py` below. One pair is enough to see a factor; three are needed to quote a range.
3. Only when B is slower than A in every pair, bisect between them, again with kept copies, and refute
   by reading the diff before trusting a candidate that merely fits.

## Comparing rounds: durations.py

```
python scripts\durations.py Regression/20_20_0_m Regression/20_22_0_m Regression/20_22_1_m
```

Wall time per experiment log (first to last timestamp) and the `Highest CommitCharge` of the process,
side by side, with totals. A folder that was moved aside (`_aside_<label>_<date>`) is passed by its name
under `C:\LocalData\GeoDMS-Test`. Read the outliers against `doc/performance-test.md` first: some are
known, such as t405 at 5 to 6 min under the 20.20.0 deferral of commits (which cost 70 to 80 GB and was
removed in 20.22.0, #1259) against 13 to 16 min since, or a 0.2 min t720 in 20.22.0, which was a run
that failed early (#1285), not a fast one.

## Rerunning one experiment: run_exp.py

```
python scripts\run_exp.py <label> <tag> <engine dir> <experiment> [<experiment> ...]
python scripts\run_exp.py 20_22_1_m t2000_A C:/LocalData/GeoDMS_engine/bisect_1ec17232f t2000_hestia_hWP_asl_statistics
python scripts\run_exp.py 20_22_1_m t641_B C:/LocalData/GeoDMS_engine/head_d46a6e199 t641_1_RSopen_MakeBaseData t641_2_RSopen_Allocatie "t641_2_RSopen_Allocatie@/t641_2_RSopen_Indicator_results/result_json"
```

It reads the experiment from `<label>`'s `.bin` files, so the command line and environment are exactly
what full.py used, and redirects the log, the local data (`C:\LocalData\runs\fp_<tag>`) and the results
(`PERF_OUT`, default `scratch\perf_runs\<tag>`) per tag, so runs on different builds do not share data.
It prints `rc` and seconds per experiment and the metrics of every `*.result.json`, which is how a
changed result is told from a slower one. Traps it handles, each of which cost a failed run:

- The configurations read their results folder from `<tmpFileDir>\results_folder.txt`, which full.py
  writes before a round, without a newline; without it the run stops at once with a StrStorageManager error.
- full.py's command is `"<dir>"/GeoDmsRun.exe ...`; through `cmd /c` that does not start. The script
  quotes the whole path and starts the process without a shell.
- Indicator steps (`t641_1_2_...`, `t641_2_RSopen_indicator`, `t405_*_2_...`) have no `.bin`; write them as
  `<experiment>@<items>`, the experiment whose environment they share with their own items.
- A chain must run in order on one tag: t641_2 reads the base data t641_1 wrote under the same local data folder.
- Delete `C:\LocalData\runs\fp_<tag>` afterwards; a t641 chain leaves tens of GB.

Durations from the same scheduled round are comparable; durations of `run_exp.py` against a full.py
round are not quite (full.py samples the process with its profiler and runs experiments back to back).

## Per item: itemtimes.py

```
python scripts\itemtimes.py <log A> <log B> [n]
```

Lists the `} Updating::[[item]] (x secs)` lines of two logs by difference. Only items named on the
command line are logged that way, so to split an experiment, run it again with `run_exp.py` and the
sub-items as `@` items (`t020_polygons@/Ops/ManySmall/geos/ok /Ops/ManySmall/bg/ok ...`).

## A full.py round that measures the build you think it does

- full.py caches results per version label: a local build under an existing label reuses the cached
  `.bin` files ("reused and not recalculated"). Move `Regression\<label>` and `C:\LocalData\runs\<label>`
  aside (`_aside_<label>_<date>`, never delete what another session may want to compare) and check that
  the new log has 0 such lines.
- Start it from an interactive scheduled task, so that it survives the agent app updating itself.
- Note in the result what else ran on the machine; a build, a battery or a disk scan of your own during
  the round invalidates its times, not its results.

## Measuring an operator change

For a change to one operator (the step 5 groups of `doc/code-audit-2026-09-27.md` are the worked
examples): a probe in `scratch\` at a size where the item takes well over 0.5 s, the inputs computed
beforehand in a warm-up item that has an IntegrityCheck, `@statistics` on the checks so the values can be
diffed exactly, three runs per build, interleaved against a kept copy of the parent build. Quote ranges,
not means, with the machine; state that the results are equal; record a change that measured no gain or
a loss in the audit finding instead of committing it.

## Where findings go

- `doc/performance-test.md`: the rounds, per machine under `## Results (<machine>), <version>`, and what
  explained an outlier. This is the memory of what is known to be slow and why.
- The commit body and the release note: the measurement of one change, with machine and range.
- The audit document: a change that was measured and not done.
- Keep this skill for the method; when a recipe or trap changes, update it in the same commit.
