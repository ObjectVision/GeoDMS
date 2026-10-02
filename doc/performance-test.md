# Performance test request: 20.21.1 (a deferred check is counted and charged) against 20.21.0.m and 20.20.0.m

> **A measurement log, not a description of the engine.** The deferral of commits and IntegrityChecks
> that this document measures was removed on 2026-09-21 (a7127224f, #1259), and the ExplicitSuppliers
> pre-start that replaced it for a few hours went again the same day (92eaa7150). Since GeoDMS 20.22.0
> the walk updates an item's suppliers one after another, each completely before the next, as before
> 20.20.0. `DeferScope`, `s_MaxDeferred`, the ledger of deferred work, the retry loop with its stall
> guard, `StartSupplierProduction` and `StartProductionForCommit` no longer exist; where the text below
> says "this tree" or "the build", it means the build of that day. What it still documents is why the
> deferral went: the measured rounds, including t405.1 at 1:59 under the commit deferral against 4:27
> without it, the one model the deferral did help.

For the Claude Code session on OVSRV05. Written on 2026-09-20 on a Linux session that has neither
the Windows toolchain nor the test data, from the OVSRV05 measurement below; the code is not compiled
there, so the build is the first thing this request asks for.

## What to measure

Commit `#1259 A deferred IntegrityCheck is counted and charged like a deferred commit` (this tree,
`rtc\dll\src\tic\TreeItemMetaInfo.cpp`, `rtc\dll\src\tic\OperationContext.cpp`) replaces the 20.21.0
switch-off of the check deferral (`deferIntegrityChecks = false`) by the bound the commit deferral
always had: a deferred check is noted in the ledger, counted against `s_MaxDeferred` (8, or twice the
workers) and charged with its producer's in-flight suppliers, and taken out when its verdict is taken.
With that, at most `s_MaxDeferred` checks and commits are in flight, the walk stops at the first one
beyond the budget and the retries drain them in order. The 20.20.0 deferral had none of that and ran
the lookahead until the process commit hit the flush threshold; the 20.21.0 switch-off gave the memory
back and lost the two models whose lookahead was breadth.

The question: does the bounded deferral keep the 20.21.0 memory figures and recover the 20.20.0 wall
times of t641.1 and t300, without the retry storm of t2000 and t810?

| test | 20.20.0.m | 20.21.0.m | expected on this build |
|---|---|---|---|
| t641.1 | 0:24:11, commit 340 GB | 0:55:51, commit 167 GB | wall towards 0:24, commit near 167 GB (the cap binds) |
| t641.2 | 0:58:50, commit 355 GB, 964 trims | 0:42:39, commit 183 GB, 81 trims | wall and commit at the 20.21.0 figures; registrations per pass in the tens, not 18k |
| t300 | 0:00:43 | 0:01:51 | wall towards 0:43 |
| t2000 | 0:17:11, 52k registrations per pass | 0:18:13, 38 retry lines on one item | registrations per pass in the tens; wall at or below 20.20.0 |
| t810 | 0:04:49, 73 retry lines, 2139 registrations | 0:04:45, none | memory at 20.21.0; retry lines, if any, with registrations in the tens |
| t060, t405.1, t405.2, t405.3 | | | unchanged against 20.21.0 |

## How to run

1. Pull the branch and check the commit is there: `git log -1 --oneline -- rtc/dll/src/tic/TreeItemMetaInfo.cpp`.
   The version is 20.21.1 (`rtc\dll\src\RtcVersionNumbers.h`), so the result folder and the report column
   are `20_21_1_m`, next to the `20_21_0_m` and `20_20_0_m` columns already on the machine.
2. Build Release x64 of `all22.sln` with the VS18 msbuild exactly as `AGENTS.md` and the `geodms-build`
   skill say (never a single project). The code was not compiled where it was written: read the build
   output for errors in the two files above before anything else. Prove the build ran: the mtime of
   `bin\Release\x64\Rtc.dll` and `GeoDmsRun.exe` must be after the launch.
3. `testcases\run_testcases.bat` and `testcases\run_xml_roundtrip.bat` on the Release build first; a
   deferral that is never released would show there as a hang or a stall report, at no cost.
4. In the GeoDMS-Test working copy, `batch\local_settings.json`: `ProfilerDir` must point at this tree's
   `profiler` folder. Nothing else of GeoDMS may be running, no build, no GUI.
5. Run, detached, from the GeoDMS-Test `batch` folder:

   ```
   powershell -ExecutionPolicy Bypass -File batch\run_detached.ps1 -Version local-msbuild-release -Tests t641,t2000,t810,t060,t300,t405
   ```

   Two to three hours. If time allows, a full round (no `-Tests`) gives the complete column.
6. When the round has ended, open the regenerated report in `C:\LocalData\GeoDMS_Test_Results\reports\`
   next to the `20_21_0_m` and `20_20_0_m` columns.

## What to compare, per test

From the report cell: duration, `fys` and `cmt`. From the GeoDMS log of each run
(`<results>\<column>\log\<test>.txt`), the same lines as the 20.21.0 request below:

- the end-of-run `[memory]` line: `Highest CommitCharge`, `PeakLiveLarge`, `PeakFreeStack`;
- the number of lines `Calling EmptyWorkingSet`;
- the number of lines `deferred commits: retry` and the `registrations in the pass` and `commits in
  flight` figures on the first of them: on this build the second is bounded by `s_MaxDeferred` (48 on
  OVSRV05), and a first figure in the thousands means a consumer fan-out, not the cap failing;
- every `ledger: room 0` line: the walk stopped at the budget, which the 20.20.0 build never logged for
  a check;
- any `deferred commits: the update of ... made no progress` line: the stall guard fired and the run
  finished inline from there, which is a regression in this build even when the wall time is fine.

Write the table with these figures for the three columns under a heading `## Results (OVSRV05), 20.21.1`
at the end of this file, with the date, the commit hashes of the tree and of GeoDMS-Test, and what was
running on the machine, and commit this file locally. Do not push.

---

# Performance test request: 20.20.0 against the build without IntegrityCheck deferral

For the Claude Code session on OVSRV05 (64 GB, idle, dedicated to performance testing).
Written on OVSRV10 on 2026-09-15; the analysis behind it is summarised at the end.

## What to measure

Commit `#1259 An IntegrityCheck is not deferred` (this tree, `rtc\dll\src\tic\TreeItemMetaInfo.cpp`,
`constexpr bool deferIntegrityChecks = false`) switches off one half of the 20.20.0 deferral:
a check whose data is still being produced is now waited for, as before 20.20.0, while the
deferral of stored-item commits (`TreeItemDataUsage.cpp`, what the BAG import gains from) stays.

The question: does this build return the 20.20.0 regressions seen in the OVSRV05 report to the
20.19.x level, without losing the 20.20.0 gains?

| test | 20.20.0 symptom on OVSRV10 | expected on this build |
|---|---|---|
| t641.2 | commit 188 -> 270 GB, PeakLiveLarge 170 -> 256 GB, ~18k check registrations per pass, wall +14 % against the same binary with the deferral off | commit and PeakLiveLarge back at the 20.19.x figures, no retry lines, wall at or below 20.20.0 |
| t2000 | 52k check registrations per pass, 8500 retries over the whole run | no retry lines; wall not worse |
| t810 | 2139 registrations per pass, 5700 retries, +6 % memory, +11 % wall | memory and wall back at 20.19.x |
| t060, t300, t405.1, t405.2, t405.3 | faster than 20.19.x with more memory (commit deferral, kept) | unchanged against 20.20.0 |

## How to run

1. Pull this tree and check the commit is there: `git log -1 --oneline -- rtc/dll/src/tic/TreeItemMetaInfo.cpp`.
2. Build Release x64 of `all22.sln` with the VS18 msbuild exactly as `AGENTS.md` and the
   `geodms-build` skill say (never a single project). Prove the build ran: the mtime of
   `bin\Release\x64\Rtc.dll` and `GeoDmsRun.exe` must be after the launch.
3. In the GeoDMS-Test working copy, check `batch\local_settings.json`: `ProfilerDir` must point at
   this tree's `profiler` folder, so that `-version local-msbuild-release` resolves to this
   tree's `bin\Release\x64` (its parent is taken as the repo root). Nothing else of GeoDMS may be
   running, no build, no GUI.
4. Run, detached, from the GeoDMS-Test `batch` folder:

   ```
   powershell -ExecutionPolicy Bypass -File batch\run_detached.ps1 -Version local-msbuild-release -Tests t641,t2000,t810,t060,t300,t405
   ```

   `t641` also runs t641.1, which t641.2 needs. Expect two to three hours. If time allows, a full
   round (no `-Tests`) gives the complete column instead.
5. When the round has ended, open the report it regenerated in `C:\LocalData\GeoDMS_Test_Results\reports\`
   (the column carries the tree's version, 20.21.0.m, plus a timestamp and hash) next to the
   `20.20.0.m` column and the last 20.19.x column that exists there.

## What to compare, per test

From the report cell: duration, `fys` (peak physical) and `cmt` (peak committed).

From the GeoDMS log of each run (`<results>\<column>\log\<test>.txt`), the same lines for the
20.20.0.m column:

- the end-of-run `[memory]` line: `Highest CommitCharge`, `PeakLiveLarge`, `PeakFreeStack`;
- the `vmcalls:` line: `drained ... over N sweeps`;
- the number of lines `Calling EmptyWorkingSet` (the working-set trim loop; on 64 GB t641.2 and
  t2000 exceed RAM in every version, so count them rather than expect zero);
- the number of lines `deferred commits: retry` and the `registrations in the pass` figure on the
  first of them. On this build the check registrations are gone; what remains are commit deferrals.

Write the table with these figures for both columns below under a heading `## Results (OVSRV05)`,
with the date, the commit hashes of the tree and of GeoDMS-Test, and what was running on the
machine, and commit this file locally. Do not push.

## Background

Measured on OVSRV10 (127 GB) with the installed 20.20.0.m, the same binary with `/SB1` (which sets
the deferral budget to 1 MB and thereby disables both kinds of deferral) and this build:

- The deferral defers a check whose checker data is still being produced, so that the supplier
  walk goes on and starts the producers of the next items. A deferred check is not noted in the
  ledger: not charged, not counted against `s_MaxDeferred`, not logged. Without `/SP` there are
  no estimates, so commit deferrals charge 0 MB as well. The only brake is the sampled process
  commit against a budget of `MemoryFlushThreshold` (80 or 90 %) times RAM.
- On t641.2 the first pass registers ~18k checks and starts their producers until the commit
  reaches the budget; the started work then grows the commit to 270-338 GB. `/SB1` gives
  PeakLiveLarge 170 033 MB against 170 025 MB for 20.19.2; this build gives 166 GB live and
  175 GB commit with zero retries. On t2000 (73 GB) memory does not change; the retry loop did.
- Once the physical load passes `MemoryFlushThreshold`, `MemGuard.cpp` calls `EmptyWorkingSet`
  once per second, which is self-sustaining while the modified page list keeps the load up and
  which also degrades the runs that follow. That is why wall times of the large models vary by
  tens of percent between runs on the same binary; the memory figures do not.

## Results (OVSRV05)

Run on 2026-09-16 between 02:44 and 05:19 on OVSRV05, 64 GB, AMD Ryzen 9 5900X, 24 logical
processors, pagefiles totalling 391 GB over three disks. Tree `C:\dev\GeoDMS` on `main` at
`e00135837`, the commit this request was written against, so `deferIntegrityChecks` is `false`
in the binaries; `bin\Release\x64\Rtc.dll` and `GeoDmsRun.exe` are of 02:34 and 02:42 that
morning. GeoDMS-Test at `86229d3`. Command exactly as above, `-Version local-msbuild-release
-Tests t641,t2000,t810,t060,t300,t405`, which ran 14 of the 32 experiments with `/S1 /S2 /S3`
and no `/SP`, the flags the 20.20.0.m column was measured with. All 14 ended `ok`. The result
folder is plain `20_21_0_m`, without the timestamp and hash suffix this request expected, and
the report it regenerated is `reports\20_21_0_m___16_0_5.html`.

Running on the machine throughout: one idle Visual Studio 2022, between 0.5 and 1.4 GB
resident, nothing else of GeoDMS. The 20.20.0.m column it is compared with was measured on
2026-09-11 on the same machine under another account.

The 20.19.3.m column is the last 20.19.x on this machine. It is absent from the report, which
shows full releases plus the version under test only, so its figures below come from its result
folder, and its wall times, like those of every column in the first table, are the span between
the first and the last timestamp of the GeoDMS log rather than the report cell.

Two caveats about the binaries. Provisioning vcpkg needed overlay ports for `gmp` and `libaec`,
whose baseline sources have gone off the net (commit `39750f9b`), which puts `libaec` at 1.1.7
instead of 1.1.6, one patch release of an entropy codec the engine reaches only through hdf5 and
netcdf-c. And `ShvDLL` and `DmsPython` do not build on this machine, which has neither Qt6 nor
the CPython 3.14 headers, so there is no `GeoDmsGuiQt.exe` here; no test in this request uses
either.

### Wall time

| test | 20.21.0.m (this build) | 20.20.0.m | 20.19.3.m | 20.17.0.m |
|---|---|---|---|---|
| t060 | 0:01:44 | 0:01:46 | 0:01:59 | 0:02:03 |
| t300 | 0:01:51 | 0:00:43 | 0:01:19 | 0:02:32 |
| t405.1 | 0:01:58 | 0:01:57 | 0:11:37 | 0:11:15 |
| t405.2 | 0:13:14 | 0:15:21 | 0:18:24 | 0:18:19 |
| t405.3 | 0:11:36 | 0:15:09 | 0:18:09 | 0:17:53 |
| t641.1 | 0:55:51 | 0:24:11 | 0:50:55 | 0:53:29 |
| t641.2 | 0:42:39 | 0:58:50 | 0:46:27 | 0:44:44 |
| t810 | 0:04:45 | 0:04:49 | 0:04:57 | 0:05:13 |
| t2000 | 0:18:13 | 0:17:11 | 0:16:25 | 0:16:15 |

### Report cells, peak physical and peak committed of the process

| test | 20.21.0.m fys / cmt | 20.20.0.m fys / cmt |
|---|---|---|
| t060 | 15.39 / 16.18 GB | 14.89 / 15.79 GB |
| t300 | 9.41 / 12.03 GB | 13.01 / 16.46 GB |
| t405.1 | 14.28 / 15.16 GB | 14.11 / 14.97 GB |
| t405.2 | 51.78 / 116.20 GB | 32.42 / 93.77 GB |
| t405.3 | 59.64 / 92.22 GB | 35.53 / 95.11 GB |
| t641.1 | 62.14 / 179.12 GB | 35.24 / 373.57 GB |
| t641.2 | 61.78 / 195.11 GB | 61.44 / 382.50 GB |
| t810 | 29.61 / 30.67 GB | 32.01 / 32.97 GB |
| t2000 | 61.53 / 91.77 GB | 48.80 / 81.68 GB |

`fys` and `cmt` are sampled from outside by the harness; the engine's own accounting is in the
next table and does not always move with them. Where this build shows a higher `fys` than
20.20.0.m on the same test, it is holding more pages resident because it is trimming far less
often, which the `EmptyWorkingSet` column shows.

### GeoDMS log figures

`EmptyWorkingSet` lines / `deferred commits: retry` lines / `registrations in the pass` on the
first of them / `Highest CommitCharge` / `PeakLiveLarge` / `PeakFreeStack`, the last three in MB.

| test | 20.21.0.m | 20.20.0.m | 20.19.3.m |
|---|---|---|---|
| t060 | 0 / 0 / - / 13733 / 14537 / 4069 | 0 / 0 / - / 13848 / 14371 / 3804 | 0 / 0 / - / 6141 / 7351 / 3501 |
| t300 | 0 / 2 / 1 / 8259 / 11256 / 3548 | 0 / 13 / 264 / 10476 / 14339 / 3348 | 0 / 0 / - / 5740 / 11252 / 3468 |
| t405.1 | 0 / 31 / 49 / 10533 / 12784 / 9965 | 0 / 33 / 49 / 10374 / 12548 / 9711 | 0 / 0 / - / 9241 / 9421 / 6509 |
| t405.2 | 93 / 72 / 132 / 77181 / 74010 / 65196 | 315 / 55 / 132 / 83008 / 73959 / 65308 | 1 / 0 / - / 31627 / 32129 / 25215 |
| t405.3 | 64 / 170 / 132 / 74179 / 71663 / 63198 | 299 / 52 / 132 / 79349 / 73959 / 65308 | 1 / 0 / - / 31623 / 32129 / 25215 |
| t641.1 | 25 / 0 / - / 167005 / 159063 / 88321 | 441 / 12 / 1 / 340360 / 346136 / 221172 | 88 / 0 / - / 153696 / 144606 / 85790 |
| t641.2 | 81 / 0 / - / 182694 / 170923 / 170253 | 964 / 0 / - / 354601 / 348896 / 348820 | 232 / 0 / - / 177021 / 171757 / 171338 |
| t810 | 0 / 0 / - / 28844 / 22500 / 15170 | 0 / 73 / 2139 / 31465 / 25292 / 15051 | 0 / 0 / - / 28272 / 22538 / 15281 |
| t2000 | 5 / 38 / 1 / 69769 / 66669 / 62272 | 30 / 20 / 52034 / 64044 / 63489 / 59348 | 7 / 0 / - / 60110 / 67484 / 61568 |

The `vmcalls` drain figures fall with the memory on every test that drains at all: t641.2 goes
from 1 882 021 calls draining 285 244 MB over 764 733 sweeps to 795 158 calls draining
126 603 MB over 95 343 sweeps, against 1 022 322 calls and 162 376 MB for 20.19.3.m; t641.1
from 640 396 to 548 599 calls; t405.2 from 165 077 to 63 704; t2000 from 669 770 to 407 457.

### Against what was asked

| test | expected of this build | measured |
|---|---|---|
| t641.2 | commit and PeakLiveLarge back at the 20.19.x figures, no retry lines, wall at or below 20.20.0 | met on all four. PeakLiveLarge 170 923 MB against 171 757 for 20.19.3.m, half a percent apart, and 348 896 for 20.20.0.m. CommitCharge 182 694 against 177 021 and 354 601. No retry line. Wall 0:42:39, faster than both 20.20.0.m and 20.19.3.m. |
| t2000 | no retry lines, wall not worse | partly. The 52 034 check registrations per pass are gone, the first retry line now reports 1. But 38 retry lines remain, all of them the one item `/t2000_hestia_test/result_json` waiting on a single commit in flight, where 20.19.3.m has none, and the wall is a minute above 20.20.0.m and nearly two above 20.19.3.m. |
| t810 | memory and wall back at 20.19.x | met. CommitCharge 28 844 against 28 272 for 20.19.3.m and 31 465 for 20.20.0.m; PeakLiveLarge 22 500 against 22 538 and 25 292. Retry lines 0, against 73 with 2139 registrations. Wall unchanged. |
| t060, t405.1, t405.2, t405.3 | unchanged against 20.20.0 | met. Their commit deferral gains stand: t405.1 keeps 0:01:58 against 0:11:37 for 20.19.3.m, and t405.2 and t405.3 are two and three and a half minutes faster than 20.20.0.m at a lower CommitCharge. The one figure that rises is the sampled `cmt` of t405.2, 116.20 against 93.77 GB, while the engine's own CommitCharge for it falls from 83 008 to 77 181 MB. |
| t300 | unchanged against 20.20.0 | not met. 0:01:51 against 0:00:43, and slower than 20.19.3.m's 0:01:19 as well, with its 264 registrations down to 1. |

The OVSRV10 measurement in the request is reproduced closely: it predicted 166 GB live and
175 GB commit with zero retries for t641.2 on this build, and OVSRV05 gives 167 GB live and
178 GB commit with zero retries.

### The two costs, neither of them in the table above

**t641.1 loses a two-fold speedup.** It ran in 0:24:11 on 20.20.0.m and takes 0:55:51 here,
which is the 20.19.3.m figure of 0:50:55 and the 20.17.0.m figure of 0:53:29 again. Its memory
falls just as far the other way: CommitCharge 167 005 MB against 340 360, and PeakLiveLarge
159 063 against 346 136, both close to 20.19.3.m. So on the pass that builds the base data, the
deferral of checks was buying half the wall time at twice the commit charge, and switching it
off returns both. The request's table did not carry a row for t641.1 because the OVSRV10 work
looked at t641.2; on this machine it is the largest single change in the set.

**t300 is slower than either reference.** 0:01:51 against 0:00:43 for 20.20.0.m and 0:01:19 for
20.19.3.m. Its CommitCharge, 8259 MB, sits between the two as expected, so it is not paying in
memory for the time. This is the one test where the build is behind both.

Read together: switching off the deferral of checks does what it was meant to do. It removes the
commit explosion of t641, the retry storm of t810 and the 52 000 check registrations a pass of
t2000, without touching the gains that
the deferral of stored-item commits brings, which are the whole of the t405 improvement. It costs
the speedup on the two tests whose work the check deferral was overlapping, t641.1 and t300. On
t2000 a retry loop remains that is not a check deferral at all, one item spinning on one commit in
flight, which is worth a look of its own.

Wall times of the large models vary by tens of percent between runs on one binary, as the
background above says, so the t641.1 and t641.2 minutes should be read as coarse; the memory
figures and the registration and retry counts should not.

## Results (OVSRV05), 20.21.1

Run on 2026-09-20 between 16:37 and 19:12 on OVSRV05, 64 GB, AMD Ryzen 9 5900X, 24 logical
processors, so `s_MaxDeferred` is 48. Tree `C:\dev\GeoDMS` on `main` at `c4987c5ef`, the three
commits of the 20.21.1 bundle fast-forwarded onto `b4e601059`. Built with the VS18 msbuild,
Release x64 of `all22.sln`, launched 16:17:49 and exit 0 at 16:28:50; `bin\Release\x64\Rtc.dll`
is of 16:18:31, `GeoDmsRun.exe` of 16:27:43 and `GeoDmsGuiQt.exe` of 16:28:35 (Qt and the three
CPython versions are on this machine since 16/09, so all eleven projects link now).
`TreeItemMetaInfo.cpp`, `OperationContext.cpp` and `TreeItemDataUsage.cpp` compiled without a
diagnostic. On that build `testcases\run_testcases.bat` gave 347 cases, 0 bad, and
`testcases\run_xml_roundtrip.bat` 194 configurations, 192 matching, the 2 listed known diffs,
0 bad; neither battery output holds a stall or assertion line. GeoDMS-Test at `86229d3`. Command
exactly as above, `-Version local-msbuild-release -Tests t641,t2000,t810,t060,t300,t405`, started
through a scheduled task so that the run is a child of neither the Claude app nor its shell; 14
experiments with `/S1 /S2 /S3` and no `/SP`, all ended `ok`. Result folder `20_21_1_m`, report
`reports\20_21_1_m___16_0_5.html`. (The first launch attempt at 16:32 died in `full.py` on a
missing `packaging` module: the 16/09 pip install of the harness modules lived in the tool
shell's overlay of the user profile; they were installed for the real machine at 16:36, the same
versions, and the run was started at 16:37:45.)

Running on the machine throughout: the VS18 `devenv` opened at 16:15, idle with four idle
MSBuild nodes of its own (30 to 170 MB resident, a few seconds of CPU per ten minutes), a Chrome
Remote Desktop host session at about a third of a core, the Claude desktop app at about a sixth
of a core, nothing else of GeoDMS. Free physical memory fell to 122 MB at 18:03 and 141 MB at
18:13, during t641.2.

About the 20.21.0.m column: the `20_21_0_m` result folder and the report of 16/09 hold the full
round of the installed GeoDms20.21.0.m, run on 2026-09-16 between 20:34 and 23:37, not the
14-experiment run of the local build that the section above reports on. The figures below are
from that full round; where the two runs differ, the section above says 0:55:51 for t641.1
against 0:50:38 here, 0:42:39 for t641.2 against 0:46:14, 0:18:13 for t2000 against 0:18:53, and
38 retry lines for t2000 against 51. The report regenerated for 20.21.1 shows releases and the
version under test only and carries no 20.21.0.m column, so its cells come from
`reports\20_21_0_m___16_0_5.html`.

### Report cells: duration, peak physical / peak committed (GB)

| test | 20.21.1.m (this build) | 20.21.0.m | 20.20.0.m |
|---|---|---|---|
| t060 | 0:01:55, 10.04 / 11.22 | 0:01:50, 10.35 / 11.54 | 0:01:45, 14.89 / 15.79 |
| t300 | 0:01:02, 13.00 / 16.45 | 0:01:53, 9.49 / 12.25 | 0:00:42, 13.01 / 16.46 |
| t405.1 | 0:02:48, 13.58 / 14.47 | 0:02:00, 14.27 / 15.06 | 0:01:57, 14.11 / 14.97 |
| t405.2 | 0:13:44, 54.22 / 111.81 | 0:13:08, 54.30 / 96.98 | 0:15:22, 32.42 / 93.77 |
| t405.3 | 0:13:04, 59.69 / 107.10 | 0:13:06, 58.80 / 100.90 | 0:15:10, 35.53 / 95.11 |
| t641.1 | 0:49:45, 59.15 / 188.22 | 0:50:45, 62.05 / 175.96 | 0:24:18, 35.24 / 373.57 |
| t641.2 | 0:47:12, 58.43 / 248.46 | 0:46:21, 61.25 / 193.98 | 0:59:01, 61.44 / 382.50 |
| t810 | 0:05:19, 32.21 / 33.18 | 0:04:53, 29.57 / 30.55 | 0:04:49, 32.01 / 32.97 |
| t2000 | 0:16:47, 61.39 / 92.50 | 0:18:58, 60.48 / 89.66 | 0:17:14, 48.80 / 81.68 |

### Wall time, the span between the first and the last timestamp of the GeoDMS log

| test | 20.21.1.m | 20.21.0.m | 20.20.0.m |
|---|---|---|---|
| t060 | 0:01:56 | 0:01:50 | 0:01:46 |
| t300 | 0:01:01 | 0:01:53 | 0:00:43 |
| t405.1 | 0:02:48 | 0:01:59 | 0:01:57 |
| t405.2 | 0:13:43 | 0:13:07 | 0:15:21 |
| t405.3 | 0:13:01 | 0:13:04 | 0:15:09 |
| t641.1 | 0:49:40 | 0:50:38 | 0:24:11 |
| t641.2 | 0:47:06 | 0:46:14 | 0:58:50 |
| t810 | 0:05:18 | 0:04:53 | 0:04:49 |
| t2000 | 0:16:42 | 0:18:53 | 0:17:11 |

### The `[memory]` line: Highest CommitCharge / PeakLiveLarge / PeakFreeStack (MB)

| test | 20.21.1.m | 20.21.0.m | 20.20.0.m |
|---|---|---|---|
| t060 | 9720 / 10160 / 3087 | 10708 / 10598 / 3578 | 13848 / 14371 / 3804 |
| t300 | 10925 / 14297 / 3107 | 7947 / 11252 / 3371 | 10476 / 14339 / 3348 |
| t405.1 | 11681 / 12521 / 10113 | 10329 / 12621 / 9791 | 10374 / 12548 / 9711 |
| t405.2 | 86939 / 73350 / 64823 | 79518 / 73397 / 65196 | 83008 / 73959 / 65308 |
| t405.3 | 82819 / 73624 / 64608 | 85031 / 74609 / 65276 | 79349 / 73959 / 65308 |
| t641.1 | 154430 / 144693 / 85792 | 150623 / 144651 / 85835 | 340360 / 346136 / 221172 |
| t641.2 | 236570 / 222630 / 222777 | 182332 / 170224 / 169774 | 354601 / 348896 / 348820 |
| t810 | 31203 / 25293 / 15052 | 28208 / 22509 / 15223 | 31465 / 25292 / 15051 |
| t2000 | 68398 / 69818 / 63599 | 70784 / 70143 / 63861 | 64044 / 63489 / 59348 |

### The deferral lines

`Calling EmptyWorkingSet` lines / `deferred commits: retry` lines / on the first of them,
`registrations in the pass` and `commits in flight` / `ledger: room 0` lines / `ledger: ...
deferred` notes / `made no progress` stall lines.

| test | 20.21.1.m | 20.21.0.m | 20.20.0.m |
|---|---|---|---|
| t060 | 0 / 1 / 1, 48 / 1 / 68 / 0 | 0 / 0 / - / 0 / 30 / 0 | 0 / 0 / - / 0 / 55 / 0 |
| t300 | 0 / 18 / 264, 10 / 0 / 26 / 0 | 0 / 2 / 1, 1 / 0 / 1 / 0 | 0 / 13 / 264, 0 / 0 / 1 / 0 |
| t405.1 | 0 / 45 / 49, 4 / 0 / 14 / 0 | 0 / 33 / 73, 6 / 0 / 13 / 0 | 0 / 33 / 49, 4 / 0 / 13 / 0 |
| t405.2 | 117 / 71 / 132, 36 / 19 / 37 / 1 | 102 / 77 / 132, 36 / 15 / 36 / 1 | 315 / 55 / 132, 36 / 7 / 36 / 1 |
| t405.3 | 66 / 71 / 132, 36 / 12 / 37 / 1 | 39 / 211 / 132, 36 / 5 / 36 / 0 | 299 / 52 / 132, 36 / 6 / 36 / 1 |
| t641.1 | 39 / 32 / 1, 48 / 2 / 72 / 1 | 48 / 0 / - / 1 / 0 / 0 | 441 / 12 / 1, 11 / 1 / 50 / 0 |
| t641.2 | 548 / 90 / 1, 48 / 1 / 49 / 1 | 87 / 0 / - / 1 / 0 / 0 | 964 / 0 / - / 1 / 0 / 0 |
| t810 | 0 / 80 / 2139, 1 / 0 / 18 / 0 | 0 / 0 / - / 0 / 2 / 0 | 0 / 73 / 2139, 0 / 0 / 2 / 0 |
| t2000 | 13 / 198 / 52034, 2 / 12 / 71 / 0 | 8 / 51 / 1, 1 / 2 / 1 / 0 | 30 / 20 / 52034, 0 / 3 / 0 / 0 |

The `commits in flight` figure never exceeds 48 in any log: the cap binds. It is reached on t060,
t641.1 and t641.2; t405.2 and t405.3 sit at 36, t300 at 10, t810 at 8, t2000 at 2.

The `vmcalls` drain figures: t641.2 1 320 944 calls draining 188 389 MB over 200 242 sweeps,
against 812 448 calls, 129 200 MB and 114 410 sweeps for 20.21.0.m and 1 882 021, 285 244 MB and
764 733 for 20.20.0.m; t641.1 409 534 calls and 53 321 MB against 503 124 and 50 467 MB, and
640 396 and 126 416 MB; t2000 471 752 calls and 44 545 MB against 337 485 and 62 108 MB, and
669 770 and 111 635 MB. t405.2 and t405.3 are within a few percent of 20.21.0.m.

### Against what was asked

| test | expected of this build | measured |
|---|---|---|
| t641.1 | wall towards 0:24, commit near 167 GB | not met on wall: 0:49:40 is the 20.21.0.m figure (0:50:38), not the 0:24:11 of 20.20.0.m. Memory at the 20.21.0.m level: CommitCharge 154 430 MB against 150 623, PeakLiveLarge 144 693 against 144 651. The stall guard fired at 17:16:27, five and a half minutes into the run, after 3239 retries with 43 commits deferred and no operation activated or running, and the walk waited inline for the remaining 44 minutes. |
| t641.2 | wall and commit at the 20.21.0 figures; registrations per pass in the tens, not 18k | wall met: 0:47:06 against 0:46:14. Commit not met: CommitCharge 236 570 MB against 182 332 for 20.21.0.m and 354 601 for 20.20.0.m, PeakLiveLarge 222 630 against 170 224 and 348 896, trims 548 against 87 and 964, sampled cmt 248 GB against 194 and 383. Every retry line reports 1 registration in the pass and 48 commits in flight. The stall guard fired at 18:11:14, ten and a half minutes in, after 9087 retries with 48 commits deferred; 310 of the 548 trims fell in those ten minutes. |
| t300 | wall towards 0:43 | halfway: 0:01:01 against 0:00:43 for 20.20.0.m and 0:01:53 for 20.21.0.m. 18 retry lines with 264 registrations on the first, as the 13 lines of 20.20.0.m, and 10 commits in flight; CommitCharge 10 925 MB is the 20.20.0.m figure (10 476) against 7947 for 20.21.0.m. |
| t2000 | registrations per pass in the tens; wall at or below 20.20.0 | wall met: 0:16:42, below both 0:17:11 and 0:18:53. Registrations not met: the first retry line reports 52 034 registrations in the pass, the 20.20.0.m fan-out, with 2 commits in flight, and there are 198 retry lines (the last is retry 19 800) against 20 for 20.20.0.m and 51 for 20.21.0.m. Memory at the 20.21.0.m level, 68 398 / 69 818 MB against 70 784 / 70 143. Twelve `room 0` lines. |
| t810 | memory at 20.21.0; retry lines, if any, with registrations in the tens | not met: CommitCharge 31 203 MB and PeakLiveLarge 25 293 are the 20.20.0.m figures (31 465 and 25 292) against 28 208 and 22 509 for 20.21.0.m; 80 retry lines with 2139 registrations on the first, the storm 20.20.0.m had in 73 lines, 1 to 8 commits in flight; wall 0:05:18 against 0:04:53 and 0:04:49. |
| t060, t405.1, t405.2, t405.3 | unchanged against 20.21.0 | t060 and t405.3 unchanged. t405.2 is 36 s slower with CommitCharge 86 939 MB against 79 518 and a sampled cmt of 111.81 GB against 96.98, with the stall line that all three columns have. t405.1 takes 0:02:48 against 0:01:59 and 0:01:57, with 45 retry lines against 33, at unchanged memory. |

### What the two t641 stall reports show

The guard lists the ledger entries at the moment it fires. In t641.1, 19 of the 20 listed wait
for a `DataItem<bool>` that is `calculating 0, ready 1, producer status -1`, a checker result
that is ready and whose producer is gone; the one other waits for an `AbstrDataItem` that is
`calculating 1, ready 0, producer status 1`. In t641.2 all 20 listed are ready checker results,
`and 28 more`. So what fills the cap is checks whose verdict has become available but was never
taken. The code releases a deferred check in one place only, `TreeItem_ValidateIntegrity` when
it takes the verdict, and the retry passes did not get there: every retry line of both runs
reports 1 registration in the pass, so each pass ended at its first deferral, with 43 to 48
entries left in flight. With the cap full `LedgerHasRoomForDeferral` is false and no further
lookahead is possible; the walk repeats the same pass until the guard fires (3239 retries in
314 s on t641.1, 9087 in 603 s on t641.2) and then waits inline for the rest of the run. That is
why t641.1 is the 20.21.0.m run again from minute five, and why t641.2 pays: its 48 deferred
producers ran to completion during the ten stalled minutes (the `room 0` line of 18:02:19 reads
commit 35 355 MB with 48 in flight; 310 trims followed before the stall) and the inline remainder
came on top, which puts its peak 52 GB above 20.21.0.m.

Where the fan-out is wide the same accounting shows without a stall: t2000 and t810 register
their checks by the thousands per pass again, 52 034 and 2139 as in 20.20.0.m, and retry more
often than either reference, and t300 and t810 return to the 20.20.0.m memory with part, or none,
of the 20.20.0.m speed.

Read together: against 20.21.0.m this build keeps the memory of t641.1 and t2000, loses it on
t641.2 (+54 GB commit) and t810 (+3 GB), recovers half of t300's wall time and nothing of
t641.1's, is two minutes faster on t2000 and fifty seconds slower on t405.1, and adds a failure
mode the 20.21.0.m build did not have: a ledger full of checks whose verdicts are ready, which
only the stall guard resolves. Wall times of the large models vary by tens of percent between
runs on one binary, as the background says; the memory figures, the registration and retry
counts and the stall lines do not.

## Results (OVSRV05), 20.21.1 without deferral

Three rounds followed the one above, every one started by full.py on this machine under the
mtahi account, and the code changed between them. All three columns are kept: the first two are
under `_Archive`, the third is the `20_21_1_m` column and `reports\20_21_1_m___16_0_5.html` now.

1. 2026-09-20, 19:47 to 20:22: the binaries of the section above with `/SB1`, the scheduler budget
   of 1 MB under which nothing is deferred, over t060, t300, t301, t405.1, t405.2 and t810, as a
   column `20_21_1_s` (a `msbuild-release-sb1` entry with suffix `s` in the gitignored
   `local_settings.json`, removed again afterwards). Threshold 95. Archived as
   `_Archive\20_21_1_s_deferral_off_SB1_20260920`.
2. 2026-09-21, 08:47 to 11:07: a7127224f, the deferral of commits and integrity checks removed and
   a start of the producers of an item's ExplicitSuppliers before their sequential update added,
   the full round, threshold 60. t641.1 ended in exit 1 after 39 minutes at 210 GB commit and t641.2
   had no base data. Archived as `_Archive\20_21_1_m_explicitsuppliers_prestart_thr60_20260921`.
3. 2026-09-21, 11:26 to 14:40: 92eaa7150, the deferral removed and nothing added, the full round,
   threshold 60, 32 experiments, all `ok`. Tree `C:\dev\GeoDMS` on `main` at `92eaa7150`,
   GeoDMS-Test at `e11c1ac` (which injects `/SH` on every flavour, see below). Built with the VS18
   msbuild, Release x64 of `all22.sln`, 11:12 to 11:22, exit 0; on that build testcases 347/0, the
   XML round trip 194 with the 2 known diffs, the tst unit suite with no FAILED line and the
   shipped-content release test passed. Running on the machine throughout: the VS18 `devenv`, idle
   at 0.8 to 1.1 GB with a few seconds of CPU per ten minutes, the Claude desktop app, a Chrome
   Remote Desktop host that was idle, nothing else of GeoDMS.

**The memory threshold.** `MemoryFlushThreshold` is a per-account registry value that sets the
deferral budget, the working-set trim loop and the drainage trigger. The 20.20.0.m column ran with
60 (its ledger lines read `budget 39269 MB`, 60 % of the 65 450 MB this machine has), the section
above and the 20.21.0.m column with 95 (the mtahi value), and the 20.19.x columns under the Cicada
account, whose value cannot be read from here (its hive is not loaded; its drain volumes sit
between the two, which fits the default of 80). Rounds 2 and 3 ran with 60, set for the run and
put back to 95 after it, so their trim counts compare with the 20.20.0.m column and not with the
section above.

**What full.py reported and what was done about it.** The 20.21.0.m column carried two verdicts
that were not timings: t010 `data error` and t1742 `output differs`. t010 was the literal `60D` in
the network tests of the Operator configuration, whose upper-case suffix #1262 stopped accepting;
GeoDMS-Test had already lower-cased it in 4eb065d and 679a571, and the working copy here was two
commits behind, so a fast-forward pull fixed it. t1742 differed only in thousand separators: on
Windows the harness leaves them to the running account's `ShowThousandSeparator` status flag,
which the account of the 20.20.0.m column has and mtahi does not; e11c1ac injects `/SH` for every
flavour, as it already did for `.l`, so the round no longer depends on the account. Both are `ok`
in round 3.

### Round 1: the same binaries with and without deferral, threshold 95

| test | with (20.21.1 of the section above, bounded checks) | without (`/SB1`) | 20.19.3.m |
|---|---|---|---|
| t060 | 0:01:56, 9720 / 10160 | 0:02:03, 7354 / 7607 | 0:01:59, 6141 / 7351 |
| t300 | 0:01:01, 10925 / 14297 | 0:01:01, 8091 / 11268 | 0:01:19, 5740 / 11252 |
| t301 | (not in that run; 20.21.0.m 0:02:59, 20237 / 4365) | 0:02:38, 18239 / 4365 | 0:02:31, 15447 / 4620 |
| t405.1 | 0:02:48, 11681 / 12521 | 0:04:27, 10339 / 9618 | 0:11:37, 9241 / 9421 |
| t405.2 | 0:13:43, 86939 / 73350, 117 trims, stall | 0:18:16, 31668 / 32129, no trims | 0:18:24, 31627 / 32129 |
| t810 | 0:05:18, 31203 / 25293 | 0:05:31, 27784 / 22593 | 0:04:57, 28272 / 22538 |

Wall time, then Highest CommitCharge / PeakLiveLarge in MB. Without deferral the memory of every
test is the 20.19.3 figure; the deferral's own gain is 2.5 minutes on t405.1 for 3 GB and 5
minutes on t405.2 for 41 GB of live memory beyond the RAM, and the rest of the 20.20.0 gain on
t405.1 (11:37 to 4:27) and t300 (1:19 to 1:01) is not the deferral's and stays without it.

### Round 2: the pre-start of the ExplicitSuppliers' producers

t641.1 failed the #1181 backstop of `TreeItem_ValidateIntegrity`, `Check Failed Error: iCheckerDC
&& iCheckerDC->GetInterestCount()`, on the stored tif attributes of the Zonneladder `Write`
container, which `for_each_neidvat` gives a self-referring IntegrityCheck: the pre-start had
produced them, with their folded checks, before the walk validated them. A probe of that shape
(`scratch\probe_prestart\probe.dms`, a driver whose ExplicitSupplier is a container of three such
attributes) writes the first tif and fails the other two with the same error on that build, and
writes all three on the build of round 3. Two more figures against round 3: t060 peaked at
23 941 MB commit and 23 903 MB live against 7221 / 7606, every target of the driver in production
at once, and t405.1 took 4:15 against 4:29, no gain, because its drivers name their own
ExplicitSuppliers and the pre-start left those to the walk. 92eaa7150 takes it out again; the
ExplicitSuppliers wiki page is as it was.

### Round 3: 20.21.1 without deferral, threshold 60, against the reference columns

Wall time, the span between the first and the last timestamp of the GeoDMS log.

| test | 20.21.1.m (round 3) | 20.21.0.m | 20.20.0.m | 20.19.3.m | 20.19.0.m |
|---|---|---|---|---|---|
| t060 | 0:02:11 | 0:01:50 | 0:01:46 | 0:01:59 | 0:01:56 |
| t300 | 0:01:56 | 0:01:53 | 0:00:43 | 0:01:19 | 0:01:15 |
| t301 | 0:02:31 | 0:02:59 | 0:02:26 | 0:02:31 | 0:02:20 |
| t405.1 | 0:04:29 | 0:01:59 | 0:01:57 | 0:11:37 | 0:11:05 |
| t405.2 | 0:18:22 | 0:13:07 | 0:15:21 | 0:18:24 | 0:18:03 |
| t405.3 | 0:18:26 | 0:13:04 | 0:15:09 | 0:18:09 | 0:17:59 |
| t641.1 | 0:51:50 | 0:50:38 | 0:24:11 | 0:50:55 | 0:58:00 |
| t641.2 | 0:44:06 | 0:46:14 | 0:58:50 | 0:46:27 | (config error) |
| t810 | 0:05:12 | 0:04:53 | 0:04:49 | 0:04:57 | 0:05:33 |
| t2000 | 0:17:01 | 0:18:53 | 0:17:11 | 0:16:25 | 0:18:26 |
| t020 | 0:03:56 | 0:03:47 | 0:03:47 | 0:03:44 | 0:03:49 |
| t101 | 0:03:16 | 0:03:10 | 0:03:16 | 0:03:08 | 0:03:16 |
| t200 | 0:00:37 | 0:00:33 | 0:00:32 | 0:00:32 | 0:00:33 |
| t410 | 0:04:49 | 0:04:49 | 0:04:38 | 0:04:43 | 0:04:39 |
| t710 | 0:01:01 | 0:01:04 | 0:01:03 | 0:01:01 | 0:01:03 |
| t720 | 0:09:12 | 0:09:15 | 0:09:18 | 0:09:16 | 0:09:19 |
| t910 | 0:00:48 | 0:00:46 | 0:00:44 | 0:00:46 | 0:00:48 |

The `[memory]` line, Highest CommitCharge / PeakLiveLarge in MB, and the `Calling EmptyWorkingSet`
count.

| test | 20.21.1.m (round 3) | 20.21.0.m | 20.20.0.m | 20.19.3.m |
|---|---|---|---|---|
| t060 | 7221 / 7606, 0 | 10708 / 10598, 0 | 13848 / 14371, 0 | 6141 / 7351, 0 |
| t300 | 6872 / 11306, 0 | 7947 / 11252, 0 | 10476 / 14339, 0 | 5740 / 11252, 0 |
| t301 | 17106 / 4131, 0 | 20237 / 4365, 0 | 17058 / 4369, 0 | 15447 / 4620, 0 |
| t405.1 | 10340 / 9618, 0 | 10329 / 12621, 0 | 10374 / 12548, 0 | 9241 / 9421, 0 |
| t405.2 | 31664 / 32129, 1 | 79518 / 73397, 102 | 83008 / 73959, 315 | 31627 / 32129, 1 |
| t405.3 | 31759 / 32129, 1 | 85031 / 74609, 39 | 79349 / 73959, 299 | 31623 / 32129, 1 |
| t641.1 | 146710 / 144651, 216 | 150623 / 144651, 48 | 340360 / 346136, 441 | 153696 / 144606, 88 |
| t641.2 | 171774 / 169560, 496 | 182332 / 170224, 87 | 354601 / 348896, 964 | 177021 / 171757, 232 |
| t810 | 28137 / 22552, 0 | 28208 / 22509, 0 | 31465 / 25292, 0 | 28272 / 22538, 0 |
| t2000 | 63193 / 62641, 43 | 70784 / 70143, 8 | 64044 / 63489, 30 | 60110 / 67484, 7 |
| t720 | 22497 / 20638, 0 | 20551 / 20988, 0 | 19339 / 20896, 0 | 21368 / 20919, 0 |

The report cells, peak physical / peak committed in GB, for the memory-heavy tests: t641.1 56.82 /
162.88 against 61.66 / 163.65 for 20.19.3.m and 35.24 / 373.57 for 20.20.0.m; t641.2 57.10 /
193.69 against 60.82 / 196.69 and 61.44 / 382.50; t405.2 33.05 / 52.96 against 45.12 / 52.36 and
32.42 / 93.77; t2000 50.44 / 81.01 against 52.85 / 87.09 and 48.80 / 81.68; t060 8.71 / 9.35
against 8.65 / 9.15 and 14.89 / 15.79. No retry, `room 0` or stall line exists any more.

### Reading

- Memory is the 20.19.x memory again on every test, within a gigabyte: t641.1 147 GB commit and
  145 GB live, t641.2 172 and 170, t405.2 and t405.3 32 GB live where 20.20.0.m and 20.21.0.m held
  73 to 74, t060 7 GB where they held 11 to 14, t810 28 GB. The trims of t641 and t2000 are the
  threshold's: 216, 496 and 43 at 60 against 88, 232 and 7 for 20.19.3.m at its own setting and
  441, 964 and 30 for 20.20.0.m at 60.
- The big models are at their 20.19.x wall times: t641.1 0:51:50 against 0:50:55, t641.2 0:44:06
  against 0:46:27, t2000 0:17:01 against 0:16:25, t405.2 and t405.3 within 20 seconds of 20.19.3.m.
  The one figure 20.20.0.m had that no build without deferral has is t641.1's 0:24:11, which cost
  346 GB of live memory on a 64 GB machine.
- Of the 20.20.0 gains, what was not the deferral's stays: t405.1 4:29 against 11:37, 2.6 times
  faster than 20.19.3.m at the 20.19.3.m memory, and 2.3 times slower than with the commit deferral.
  t405.2 and t405.3 give up the deferral's 3 to 5 minutes and keep 41 GB of live memory.
- Small residuals, all outside the 20.19.x band of the same test and present in round 2 as well:
  t020 0:03:56 against 3:44 to 3:49, t200 0:00:37 against 0:32 to 0:33, t060 0:02:11 against
  1:46 to 1:59, t810 0:05:12 against 4:49 to 4:57 (20.19.0.m: 5:33). t300 is 0:01:56, and that
  test has read 0:43, 1:01, 1:15, 1:19, 1:53 and 1:54 on the columns of this file, so its figure
  says nothing about the build. Wall times of the large models vary by tens of percent between
  runs on one binary, as the background says; the memory figures and the absence of the deferral
  lines do not.

## Reading the 20.20.0 t641 columns (OVSRV05): the deferral's first minute, and why the ledger could not take it back

Written on 2026-10-02 from the logs of the columns above, after the question of 2026-09-22 why
20.20.0.m halves t641.1 and lengthens t641.2 when both ran under the same deferral and the same
threshold, and why the ledger that was to bound the memory did not. OVSRV05 throughout: the
20.19.3.m column ran under the Cicada account (threshold not readable, probably the default 80),
the 20.20.0.m column under the account with threshold 60 (budget 39 269 MB of 65 450), and the
20.21.1.m column is round 3 above, no deferral, threshold 60. The code read is that of 20.20.0
(`06e1b2de2`): `LedgerHasRoomForDeferral`, `LedgerNoteDeferral` and `DeferScope` in
`OperationContext.cpp`, `TriggerOperator.cpp`, `TreeItemMetaInfo.cpp` and `TreeItemDataUsage.cpp`
of that commit, none of which exists in the tree any more.

### What each model is

t641.1 (`/WriteBaseData/Generate_Run1`) writes 82 stored tif targets that do not form a chain:
under 20.20.0 all of them were in production at once and written in one burst. Three quarters are
small, the last quarter are the large ones. t641.2 (`/t641_2_RSopen_Indicator_results/
RunAllocation_y2050`) is one chain: year 2030, then 2040 on the state that 2030 leaves, then 2050,
with 32 files written per year. Each run is a single `ItemUpdateImpl` (3 505 s for t641.2 under
20.20.0.m), so under 20.20.0 a single `DeferScope` spanned each run.

### When the walk wrote

t641.1, the 82 `Writing to` lines, in minutes from the walk's start, at 0, 25, 50, 75 and 100 % of
them, and the live peak:

| column | first | 25 % | 50 % | 75 % | last | PeakLiveLarge |
|---|---|---|---|---|---|---|
| 20.19.3.m | 3.2 | 4.2 | 4.8 | 17.4 | 50.9 | 144 606 MB |
| 20.21.1.m (round 3) | 4.8 | 5.8 | 6.5 | 19.1 | 51.8 | 144 651 MB |
| 20.20.0.m | 17.3 | 22.6 | 23.1 | 23.9 | 24.1 | 346 136 MB |

Without deferral the walk takes each target in turn: validate its check, which computes the item,
wait, write, next. Three quarters of the targets are written by minute 5; the last quarter are
produced one after another for 33 minutes, each chain with little parallelism of its own, and the
pool has only that chain to run. Under 20.20.0 nothing is written before minute 17 and everything
within the next seven: the run is the longest chain plus a write burst, with all 82 chains alive
at once, Highest allocated 219 717 MB against 84 609 for 20.19.3.m and 85 025 for round 3.

t641.2, the minutes in which each year's files were written, the end of the run, the live peak and
the `Calling EmptyWorkingSet` count:

| column | 2030 | 2040 | 2050 | end | PeakLiveLarge | trims |
|---|---|---|---|---|---|---|
| 20.19.3.m | 19 to 22 | 30 to 34 | 42 to 46 | 46 | 171 757 MB | 232 |
| 20.21.1.m (round 3) | 24 to 26 | 33 to 35 | 42 to 44 | 44 | 169 560 MB | 496 |
| 20.20.0.m | 41 to 42, 5 files | 49 to 50, 5 files | 56 to 59, 32 files | 59 | 348 896 MB | 964 |

The thread count is 61 in all three logs: the pool was not the difference. Year 2040 cannot start
before 2030 is done, so there is no breadth to find; what 20.20.0 found instead was memory.
Highest allocated 338 475 MB and Highest freed 197 172 MB against 163 298 and 93 849 for 20.19.3.m
and 156 739 and 69 568 for round 3; the allocator drained 285 244 MB in 1 882 021 calls against
162 376 MB in 1 022 322 calls. The trims per ten minutes of the walk say when the machine was
above its threshold: 20.19.3.m 129, 93, 5, 2, 3; round 3 (threshold 60) 260, 181, 32, 21, 2;
20.20.0.m 284, 323, 211, 102, 19, 25. Without deferral the load drops under the threshold once the
base data is in and the walk frees what it has consumed; under 20.20.0 it stayed above it for 40
minutes, and the first year's results, ready at minute 19 in the plain walk, were written at
minute 41.

### What the ledger saw, and when

The resource-aware admission gate (`AdmitOrRequeue`, `ResourceAwareScheduling`) was off in every
full.py column: it is off by default since the measurement in section 8.1.33 of
`doc/development/schedule-with-lookahead.md`, and the harness passes `/S1 /S2 /S3 /CP` (and `/SH`
since e11c1ac), never `/SQ`. With `/SQ` it would have admitted everything as well: it admits any
context without an estimate, and estimates exist only under `/SP`. So the one budget-aware
decision in these runs was the walk's own `LedgerHasRoomForDeferral`: fewer than 48 items in
flight, and the process commit plus the charge of the in-flight producers under the budget. The
charge comes from the same estimates, so every `ledger: ... deferred` line in every full.py log
reads `charge 0 MB` and `in flight 0 MB`, and the test was "PagefileUsage below 39 269 MB", sampled
at most five times a second. A t641 run lives at 145 to 170 GB when walked one item at a time, so
the gauge crossed the budget in the first minute and never came back:

| run under 20.20.0.m | walk starts | `room 0` | commit then | room again |
|---|---|---|---|---|
| t641.1 | 10:15:02 | 10:16:13 | 39 640 MB, 42 commits in flight | never |
| t641.2 | 10:39:32 | 10:40:44 | 39 563 MB, 0 commits in flight | never |

In t641.2 not one commit was deferred in the whole run: the one `ledger:` line is the `room 0`
line, there is no `deferred` note and no retry line. Everything it deferred was integrity checks,
which the 20.20.0 code registered in the `DeferScope` without a ledger note (the Background above;
58def697f added the note in 20.21.1), so the in-flight term was zero by construction and the
ledger learnt of the checks only through the commit gauge, after their producers had allocated.

### What the first minute did, and why refusing afterwards did not undo it

While room is true the first pass does not wait at a check whose item is calculating: it schedules
the check through `CalledCalcHandle`, which schedules the item's producer chain, keeps the handle
alive (`DeferScope_KeepAlive`) so that the scheduled work is not cancelled, starts the pool and
walks on to the next supplier. A pass that never waits runs at meta-thread speed. In the first 75
seconds of the t641.2 walk the 20.20.0.m log holds 250 lines with 235 `storage read` lines on 24
threads, against 71 lines with 36 reads on 21 threads for 20.19.3.m and 36 lines with 30 reads on
14 threads for round 3; the OVSRV10 count in the Background, about 18 000 registrations per pass,
is the same pass. The commit gauge lags that pass by construction: a scheduled producer allocates
when a worker picks it up, so the walk was a minute ahead of the figure it was steering by.

After `room 0` the ledger can only refuse the next deferral. It cannot cancel a scheduled producer
and it cannot drop a keep-alive. A keep-alive went when the walk re-entered that item's validation
(`DeferScope_Release` at the top of the check branch) or when the outermost scope ended, which in
t641.2 is the end of the run; and a consumer whose supplier was deferred stays below Committed and
keeps its supplier interest (`StopSupplInterest` runs at Committed) until a retry pass gets the
supplier through. Once room is 0 a pass stops at the next deferral it meets and the retry walks
again from the root, waiting inline at every deferred check in walk order, so the holders of the
items deep in the chain, the 2040 and 2050 iterations, are released when the walk gets there, near
the end. That is what turned 163 GB allocated and 94 GB freed into 338 and 197: the allocation
states, 7 to 8 GB per `StateNaAllocatie` container and 94 % of the run's production by the census
in section 8.1.31 of the lookahead document, were held instead of freed as the walk moved on. The
one memory-aware mechanism that was on, the low-RAM activation brake (`IsLowOnFreeRAM` in
`collectOperationContexts`), bounds how many contexts of a phase are activated above the
machine-load threshold to the number of threads waiting in a Join; it governs what starts, not
what is held, and every inline wait of the second pass is a Join. (Its 20.20.0 form could activate
nothing with nobody joining; ccda904fa, 53c0931b4 and 9e37f1363 changed that on 2026-09-26 and 27,
after the deferral was gone.)

### Why t641.1 got away with the same burst

Its 42 commits were deferred between 10:15:38 and 10:16:13, each `charge 0 MB`, over 8 to 55 418
operations, and the commit branch kept deferring while anything was in flight, room or not
(`LedgerHasRoomForDeferral() || LedgerDeferredCommits()`; 8 more deferrals at 10:37), so the 42
chains ran side by side until the writes at minutes 22 to 24 (the `released` lines: 37 at 10:37, 9
at 10:38, 4 at 10:39). Eighty-two targets of similar size that do not wait for each other are the
one shape in which an uncontrolled burst is the right schedule, and the paging it cost was
outweighed by 24 workers being busy. t641.2 paid the paging and got nothing, because its chain has
no breadth to fill. Getting the first without the second would need concurrency limited to
independent targets, a real memory bound, and the check validated before its item is produced,
which neither the deferral nor the pre-start of round 2 had.

### The bounded variant, read the same way

The 20.21.1 build of the first section counted and charged checks. On t641.2 that bounded a count,
not memory: 49 deferrals at `charge 0 MB` within 73 seconds of the walk (commit 35 355 MB against
the 62 177 MB budget of threshold 95), then 9 087 retries over nine minutes with 48 in flight and 1
registration per pass, because each pass stopped at the first re-registered actor before reaching
the 48 whose data was already ready ("What the two t641 stall reports show" above). The stall
guard switched deferral off at 18:11:14 and the remaining 36 minutes ran inline; the 236 GB commit
is what the first 73 seconds had started.

### Read with the lookahead document

This is the verdict of section 8.1.33 of `doc/development/schedule-with-lookahead.md` from the
other side. Enforce on t641_2 parked 124 184 of 173 178 operations and left the live peak at
171.9 GiB; 15 433 of 15 503 refusals came when the commit already exceeded the budget, 16 022 of
16 144 ended in a lift, and 91 % of the volume was tile work inside chains already admitted. The
unit the gate controls, an operation's start, is not the unit the memory is spent in, which is
retained state and held interest; the walk-level deferral inherited that blindness and added a
lookahead term of zero. Section 8.1.37 there records the same reading from the ledger's side.

## Results (OVSRV10), 20.22.0

Run on 2026-09-27 on OVSRV10, 127 GB, 32 logical processors, `MemoryFlushThreshold` 90 (the ledger
lines of the 20.20.0.m column on this machine read `budget 117094 MB`). Tree `C:\dev\GeoDMS_2026` on
`main` at `1ec17232f` (version 20.22.0), so without deferral as in round 3 above, plus `ccda904fa`
and `53c0931b4` (the low-RAM brake keeps one context activated or running), which had been verified
in Debug only. GeoDMS-Test at `e452efc`, which does not have the `/SH` commit `e11c1ac` of the
section above (it is not on origin), so t1742 still depends on the account.

Binaries: Release x64 of `all22.sln`, linked 03:45 to 03:56 by the Visual Studio 18 build of the
version bump; Debug x64 with the VS18 msbuild, 03:58 to 04:01; `linux-x64-release` with CMake in
WSL, `libDmRtc.so` 04:38 and `GeoDmsRun` 04:48. Uncommitted edits to `clc/dll/src/Modus.cpp`,
`clc/dll/include/ValuesTable.h` and nine `testcases/oper_modus_*` configs appeared in the tree
from 09:53, after both builds; the figures below are of `1ec17232f` as committed.

Before the rounds: `testcases\run_testcases.bat` on Release 377 cases, 0 bad;
`testcases\run_xml_roundtrip.bat` 212 configurations, 210 matching, the 2 listed known diffs, 0
bad. `batch\TestDebugUnit.bat` lists one line, `operator.dms results/unit_test_log FAILED` (exit
nonzero, no crash event, no log of the run); the same command run five times on its own afterwards
exited 0 each time, so the failure is intermittent or was caused by the load of the moment, and is
not explained.

Running on the machine: until 04:49 the NetworkModel_PBL recompute (a GeoDmsRun at 33 to 89 GB),
during the Debug build, the unit suite and the Linux build. It was stopped at 04:49 for the rounds
and restarted at 11:21. During both rounds: the idle VS18 `devenv` with its idle MSBuild nodes, the
Claude desktop app, nothing else of GeoDMS.

- `.m`: `full.py -version local-msbuild-release`, 04:49 to 06:36, 27 experiments, no reuse, all
  `ok` except t720_2BURP.
- `.l`: `full.py -version local-linux-release`, 06:40 to 11:20, 32 experiments, all `ok` except
  t720_2BURP. A first attempt at 06:36 ended every test `unavailable` within seconds:
  `profiler.py` runs a `.l` test through `<bin>/profiler/run_with_sampler.sh`, which only
  `nsi/CreateLinuxSetup.sh` stages, and the build removes `build/linux-x64-release/bin` first.
  The two sampler scripts were installed there by hand, the column deleted and the round rerun.

### A regression: t720_2BURP, #1285

On both flavours t720_2BURP ends after 13 to 17 seconds with exit 1:
`[[.../smoothing_loop/iter0/NextValue/MedianFiltering]] Range Error Error: Value 135546 not in
expected range from 0 till 135545`, in `modus`. It comes from `b932df281` (modus family: integral
values lie in the formal range): the argument is an `attribute<uint32>` whose `switch` carries the
range of the values unit of its first case, `FindContiguousCells`, and its sentinels
`#FindContiguousCells+1` and `+2` lie beyond it. A 20-line probe (in the issue) reproduces it; the
installed 20.21.0.m computes it. 20.20.0.m took 6:35 on the test and 20.19.2.m 5:36.

### Wall time and memory, `.m`

Wall time is the span between the first and the last timestamp of the GeoDMS log; then Highest
CommitCharge / PeakLiveLarge in MB. All three columns were measured on this machine; 20.20.0.m on
2026-09-11 with the deferral of both commits and checks.

| test | 20.22.0.m | 20.20.0.m | 20.19.2.m |
|---|---|---|---|
| t020 | 0:03:07, 2008 / 2614 | 0:09:45, 2016 / 2591 | 0:03:25, 1959 / 2592 |
| t060 | 0:01:43, 8417 / 7826 | 0:01:21, 13859 / 13317 | 0:01:40, 6332 / 7353 |
| t101 | 0:01:25, 1965 / 955 | 0:01:39, 1959 / 956 | 0:01:37, 676 / 967 |
| t300 | 0:00:49, 9203 / 11059 | 0:00:36, 9636 / 13708 | 0:01:07, 5740 / 11276 |
| t301 | 0:02:10, 23483 / 5273 | 0:03:31, 21584 / 4851 | 0:02:04, 20193 / 4224 |
| t405.1 | 0:03:35, 8537 / 9637 | 0:02:44, 10809 / 11426 | 0:09:47, 8315 / 9421 |
| t405.2 | 0:12:54, 32245 / 34939 | 0:05:49, 71705 / 72103 | 0:14:32, 34635 / 34938 |
| t405.3 | 0:12:40, 31392 / 34588 | 0:05:33, 82337 / 74075 | 0:13:23, 31681 / 34587 |
| t410 | 0:03:39, 1240 / 1444 | 0:05:45, 1480 / 1463 | 0:03:54, 1171 / 1463 |
| t641.1 | 0:28:42, 155147 / 137010 | 0:27:45, 356521 / 345458 | 0:31:29, 187226 / 144550 |
| t641.2 | 0:19:36, 189035 / 170287 | 0:20:38, 270446 / 255833 | 0:21:02, 188431 / 170025 |
| t720 | fails, see above | 0:06:35, 25851 / 21449 | 0:05:36, 24359 / 20767 |
| t810 | 0:02:57, 27959 / 23252 | 0:03:31, 32609 / 25294 | 0:03:10, 31033 / 22508 |
| t910 | 0:00:34, 2516 / 4888 | 0:00:38, 4850 / 4784 | 0:00:32, 3520 / 6465 |
| t2000 | 0:07:18, 72139 / 66983 | 0:09:07, 73764 / 68669 | 0:07:50, 72389 / 68367 |

`Calling EmptyWorkingSet` lines: t641.1 20 against 278 and 21, t641.2 4 against 234 and 7, t405.3
0 against 2 and 0, every other test 0. `deferred commits: retry` lines: none in any 20.22.0.m log,
against 1 to 176 per test in the 20.20.0.m column (t020 176, t720 114, t410 96, t2000 85, t405.2
84); no `made no progress` line. The `vmcalls` drain: t641.1 58 489 calls draining 2678 MB
against 144 943 and 16 049 MB for 20.20.0.m and 59 822 and 2118 MB for 20.19.2.m; t641.2 236 892
calls and 34 691 MB against 944 830 and 150 146 MB, and 293 812 and 48 273 MB.

### Wall time and memory, `.l`

| test | 20.22.0.l | 20.20.0.l |
|---|---|---|
| t020 | 0:03:24, 6298 / 2608 | 0:06:29, 6579 / 2586 |
| t060 | 0:02:44, 12480 / 7826 | 0:02:19, 14586 / 11284 |
| t300 | 0:01:19, 11721 / 11076 | 0:00:47, 9961 / 14397 |
| t405.1 | 0:06:22, 11030 / 9644 | 0:06:07, 11580 / 11730 |
| t405.2 | 0:21:03, 50560 / 34588 | 0:15:54, 90917 / 82687 |
| t405.3 | 0:20:57, 34500 / 34588 | 0:15:49, 100209 / 82687 |
| t410 | 0:03:31, 2450 / 1691 | 0:09:00, 2522 / 1690 |
| t641.1 | 1:04:53, 108419 / 136993 | 2:17:37, 110243 / 317077 |
| t641.2 | 1:57:34, 110713 / 169627 | 3:45:03, 109938 / 251750 |
| t810 | 0:04:54, 31799 / 23207 | 0:07:25, 36241 / 25290 |
| t2000 | 0:16:29, 93313 / 77165 | 0:25:05, 80456 / 65669 |

On `.l` the CommitCharge line is the engine's own figure under WSL and says less than
PeakLiveLarge. 20.20.0.l logged one `made no progress` stall on t641.1; 20.22.0.l none, and no
retry line on any test.

### Reading

- On this machine the build without deferral is the 20.19.x memory with the 20.20.0 wall time
  on t641: t641.1 0:28:42 at 155 GB commit and 137 GB live, against 0:27:45 at 357 and 345 GB for
  20.20.0.m and 0:31:29 at 187 and 145 GB for 20.19.2.m; t641.2 0:19:36 at 189 and 170 GB. On
  OVSRV05 (64 GB, section above) t641.1 lost its 20.20.0 speed; with 127 GB it does not. Why the
  two machines differ is not measured; one reading is that the deferral's gain on OVSRV05 came from
  overlap that 64 GB could only afford by paging, but no run here tests that.
- On `.l` the effect is larger: t641.1 and t641.2 take half the 20.20.0.l time, at 137 against
  317 GB and 170 against 252 GB of live memory.
- The one loss is t405.2 and t405.3, as on OVSRV05: 12:54 and 12:40 against 5:49 and 5:33 for
  20.20.0.m (and 21 against 16 minutes on `.l`), at less than half the memory, and slightly
  faster than 20.19.2.m. That is the commit deferral's gain returned.
- t020, t410, t810 and t2000 are faster than both 20.20.0.m and 20.19.2.m; t301 is faster than
  20.20.0.m and 6 seconds slower than 20.19.2.m. t060 is 22 seconds slower than 20.20.0.m and in
  line with 20.19.2.m; t300 is 13 seconds slower than 20.20.0.m and 18 faster than 20.19.2.m.
- These are single runs. The memory figures and the retry, stall and trim counts hold; wall time
  differences below a minute or two on the large models do not, until a second round or a paired
  `/SB1` run confirms them.

### How to measure on OVSRV10 from here

Compare only columns of this machine, and record the `MemoryFlushThreshold` of each. OVSRV10 has
the RAM to run t641 and t2000 almost without trimming, which makes it the machine to see memory
change on; OVSRV05 remains the one for behaviour under RAM pressure. Stop the NetworkModel_PBL
recompute for the duration (its chain step peaks at about 110 GB; it can be stopped between
steps), and before a `.l` round install the sampler scripts into the build's `bin/profiler`.

## Results (OVSRV10), 20.22.1

Run on 2026-10-01 on OVSRV10, 127 GB, 32 logical processors. Tree `C:\dev\GeoDMS_2026` on `main` at
`d46a6e199` (version 20.22.1, with CLC-A07 and GEO-A19 reverted), Release x64 of `all22.sln`, Geo.dll
linked 11:57. GeoDMS-Test at `7423f6a`.

- `.m`: `full.py -version local-msbuild-release`, 12:00 to 14:13, 28 experiments, no reuse, all `ok`.
  The 28-09 round of an earlier 20.22.1 build is kept aside as `C:\LocalData\GeoDMS-Test\_aside_20_22_1_m_20260928`.

Running on the machine: no other model (the NetworkModel_PBL chain ended on 30-09 at 23:20), no build.
A `du` over `C:\LocalData\runs` of this session's own ran from about 12:00 during the first experiments.
Before the round, between 08:48 and 11:00, the same session ran t2000 four times and the t641 chain once
on kept builds (bisecting the t2000 and t641 result changes), so the machine started the round after
hours of 75 to 190 GB processes.

### The round is slower; the engine is not

The round took 127.6 min against 104.0 min for 20.22.0.m on 27-09 (section above); per experiment:

| experiment | 20.20.0.m | 20.22.0.m (27-09) | 20.22.1.m (28-09) | 20.22.1.m (01-10) |
|---|---|---|---|---|
| t020_polygons | 9.8 | 3.1 | 4.7 | 6.4 |
| t405_2 zonder fence | 5.8 | 12.9 | 15.5 | 15.6 |
| t405_3 met fence | 5.5 | 12.7 | 16.4 | 14.9 |
| t641_1 MakeBaseData | 27.8 | 28.7 | 37.8 | 34.0 |
| t641_2 Allocatie | 20.6 | 19.6 | 25.1 | 21.7 |
| t2000 | 9.1 | 7.3 | 8.6 | 8.2 |
| t720_2BURP | 6.6 | 0.2 (failed, #1285) | 6.3 | 5.9 |

Minutes, wall time of the GeoDMS log. t405 at 5 to 6 min under 20.20.0 is the commit deferral, removed in
20.22.0 (#1259), and t720 in 20.22.0 failed after 13 to 17 s, so neither is a regression of 20.22.1.

An interleaved A/B the same afternoon, on kept copies of the two builds, `1ec17232f` (20.22.0) and
`d46a6e199`, one after the other with nothing else running, each experiment rerun with full.py's own
command and environment (`.claude/skills/geodms-perf`, `run_exp.py`):

| experiment | 20.22.0 | 20.22.1 |
|---|---|---|
| t020_polygons, two pairs | 219, 226 s | 228, 220 s |
| t405_1 prepare | 196 s | 207 s |
| t405_2 zonder fence | 848 s | 844 s |
| t641_1 MakeBaseData | 2082 s | 1998 s |
| t641_2 Allocatie | 1571 s | 1489 s |

The two builds are equal within the spread, and 20.22.1 is 4 to 5 % faster on t641. The same 20.22.0
binary took 28.7 min for t641_1 in its own round of 27-09 and 34.7 min in this A/B, and 12.9 against
14.1 min for t405_2: the round-to-round spread of this machine is about 20 % on the models that commit
more than the RAM, larger than anything the engine changed. The memory lines show the state differed:
in both rounds t641_1 drained 57 000 to 58 000 times with at most 6 GB uncommitted, in the A/B 760 000
times with 61 GB uncommitted, for either build. Which part of the machine's state causes the difference
(standby and modified lists after the morning's large runs, the pagefile, the 27-09 round starting right
after the NetworkModel_PBL chain was stopped at 04:49) was not measured.

### Reading

- No experiment of 20.22.1 is slower than 20.22.0 on the same machine in the same state. The round's
  extra 24 minutes are the machine, mostly t641 and t405, plus t020 during the disk scan.
- A round on OVSRV10 is a correctness check and a memory measurement; its wall times are comparable
  between versions only through an A/B in one sitting. Wall-time comparisons between rounds belong on
  OVSRV05, which is dedicated and idle, or need a reference column measured in the same round.
- Before a round on OVSRV10 that is meant for times: no large GeoDMS processes in the hours before (or a
  reboot), the NetworkModel_PBL chain stopped, no scans or copies of `C:\LocalData` of one's own.

## Results (OVSRV05), 20.22.1

Run from 2026-10-01 21:13 to 2026-10-02 00:39 on OVSRV05, 64 GB, AMD Ryzen 9 5900X, 24 logical
processors, `MemoryFlushThreshold` 60 (set by the launching script for the round and put back to 95
after it, as for the 20.22.0.m and 20.21.1.m columns below). Tree `C:\dev\GeoDMS` on `main` at
`fd30ef780` (version 20.22.1, with CLC-A07 and GEO-A19 reverted; `b8ef1e32a` plus a documentation
commit). GeoDMS-Test at `1b8aa71`, which contains the t010 reference of `7423f6a`.

Built with the VS18 msbuild, Release x64 of `all22.sln` with `-m -nr:false`, 21:00:29 to 21:12:02,
exit 0, no error and no C warning; `Rtc.dll` 21:01:17, `Geo.dll` 21:04:04, `Clc.dll` 21:10:53,
`GeoDmsRun.exe` 21:11:02, `GeoDmsGuiQt.exe` 21:11:58. On that build `testcases\run_testcases.bat` gave
442 cases, 0 bad. The binaries are kept as `C:\LocalData\GeoDMS_engine\head_fd30ef780`.
`full.py -version local-msbuild-release` (`/S1 /S2 /S3`, no `/SP`) was started from an interactive
scheduled task with `py -3.13` (`C:\Python313`, 3.13.15, the harness modules in the mtahi user site):
32 experiments, no "reused and not recalculated" line, all 27 report rows `ok` (report header
3:23:26). Neither `20_22_1_m` folder existed beforehand, under `C:\LocalData\GeoDMS_Test_Results` or
under `C:\LocalData\runs`, so nothing was moved aside. Report `reports\20_22_1_m___16_0_5.html`.

The 20.22.0.m column is a round that this file did not record until now: 2026-09-27, 03:53 to 06:59,
`1ec17232f` built on this machine with the VS18 msbuild from 03:41 to 03:52 (testcases 377/0, XML round
trip 212 with the 2 known diffs), threshold 60, all `ok` except t720_2BURP, which failed after 26 s
(#1285). Its binaries were still in `bin\Release\x64` and are kept as
`C:\LocalData\GeoDMS_engine\ovsrv05_1ec17232f`. The 20.21.1.m column is round 3 of the section
"20.21.1 without deferral" above; 20.19.3.m ran under another account, whose threshold is presumably the
default 80.

Running on the machine. Before the round: a reboot at 14:32; from 16:23 to 19:20 another session ran
full.py over the small tests (t020 to t910, no t405, t641 or t2000) on the installed 17.4.6, 18.1.2,
19.0.0 and 20.12.0; then the build and the battery above. During the round, sampled every 5 minutes
(`scratch\perf_ovsrv05\machine_watch.ps1`, which lists a process when it used 3 s or more since the
previous sample): two VS18 `devenv` windows opened at 20:55, idle (listed once, with 6 s); the Claude
desktop app, on average 21 % of one core; the Chrome Remote Desktop host, 20 % of one core; explorer
2 %; together 1.8 % of the 24 logical processors. The session
that ran the round only read logs and the git history meanwhile. Free physical memory fell to 1.9 GB
at 23:53, during t641.2. During the A/B runs below the other processes took 0.4 %.

### Wall time, the span between the first and the last timestamp of the GeoDMS log

| test | 20.22.1.m (01-10) | 20.22.0.m (27-09) | 20.21.1.m (21-09) | 20.19.3.m | 20.22.1 / 20.22.0 |
|---|---|---|---|---|---|
| t020 | 0:05:03 | 0:04:02 | 0:03:56 | 0:03:44 | 1.25 |
| t060 | 0:01:47 | 0:02:22 | 0:02:11 | 0:01:59 | 0.75 |
| t101 | 0:03:50 | 0:03:31 | 0:03:16 | 0:03:08 | 1.09 |
| t200 | 0:00:40 | 0:00:38 | 0:00:37 | 0:00:32 | 1.05 |
| t300 | 0:01:09 | 0:01:55 | 0:01:56 | 0:01:19 | 0.60 |
| t301 | 0:02:48 | 0:02:38 | 0:02:31 | 0:02:31 | 1.06 |
| t405.1 | 0:04:44 | 0:04:31 | 0:04:29 | 0:11:37 | 1.05 |
| t405.2 | 0:20:47 | 0:19:07 | 0:18:22 | 0:18:24 | 1.09 |
| t405.3 | 0:20:36 | 0:18:39 | 0:18:26 | 0:18:09 | 1.10 |
| t410 | 0:05:34 | 0:04:56 | 0:04:49 | 0:04:43 | 1.13 |
| t611 | 0:00:34 | 0:00:31 | 0:00:31 | 0:00:30 | 1.10 |
| t641.1 | 0:53:08 | 0:51:32 | 0:51:50 | 0:50:55 | 1.03 |
| t641.2 | 0:44:25 | 0:43:25 | 0:44:06 | 0:46:27 | 1.02 |
| t710 | 0:01:08 | 0:01:02 | 0:01:01 | 0:01:01 | 1.10 |
| t720 | 0:10:12 | failed, #1285 | 0:09:12 | 0:09:16 | - |
| t810 | 0:05:14 | 0:05:02 | 0:05:12 | 0:04:57 | 1.04 |
| t910 | 0:00:53 | 0:00:50 | 0:00:48 | 0:00:46 | 1.06 |
| t2000 | 0:19:36 | 0:16:52 | 0:17:01 | 0:16:25 | 1.16 |

Summed over all 32 logs (`durations.py`): 203.7 min against 183.6 for 20.22.0.m, 191.8 for 20.21.1.m
and 198.0 for 20.19.3.m. 9.8 of the 20 minutes against 20.22.0.m are t720, which 20.22.0 failed early;
the remaining 10 minutes, 5.6 %, are spread over nearly every test.

### Memory: Highest CommitCharge / PeakLiveLarge in MB, and the `Calling EmptyWorkingSet` count

| test | 20.22.1.m | 20.22.0.m | 20.21.1.m |
|---|---|---|---|
| t020 | 2532 / 2628, 0 | 2085 / 2614, 0 | 1662 / 2593, 0 |
| t200 | 2530 / 6701, 0 | 4018 / 7256, 0 | 3716 / 6960, 0 |
| t300 | 5594 / 11083, 0 | 6698 / 11099, 0 | 6872 / 11306, 0 |
| t301 | 19721 / 3431, 0 | 16935 / 4406, 0 | 17106 / 4131, 0 |
| t405.2 | 30830 / 31383, 5 | 43120 / 32130, 3 | 31664 / 32129, 1 |
| t405.3 | 30854 / 31383, 5 | 32248 / 32130, 3 | 31759 / 32129, 1 |
| t641.1 | 140627 / 136920, 220 | 140864 / 136993, 220 | 146710 / 144651, 216 |
| t641.2 | 175327 / 168907, 382 | 180723 / 169619, 471 | 171774 / 169560, 496 |
| t720 | 17902 / 18758, 0 | (failed after 26 s) | 22497 / 20638, 0 |
| t810 | 26840 / 20827, 0 | 28362 / 23232, 0 | 28137 / 22552, 0 |
| t2000 | 63591 / 64231, 41 | 66137 / 69628, 38 | 63193 / 62641, 43 |

Every other test within 400 MB of 20.22.0.m on both figures. Report cells, peak physical / peak committed in
GB: t641.1 56.47 / 157.39 against 58.43 / 158.40, t641.2 56.60 / 191.68 against 58.37 / 194.32, t2000
51.08 / 80.68 against 48.18 / 85.21, t405.2 30.25 / 52.08 against 30.09 / 53.32. No retry, `room 0` or
stall line in any log.

### Interleaved A/B: 20.22.0 (`ovsrv05_1ec17232f`) against 20.22.1 (`head_fd30ef780`)

On 2026-10-02 from 00:42, right after the round, each experiment rerun with full.py's own command and
environment, in the order A, B, A, B, threshold 60 (`run_exp.py` of `.claude/skills/geodms-perf`
through a copy with this machine's paths, `scratch\perf_ovsrv05\run_exp_ovsrv05.py`; the skill's
scripts name the OVSRV10 folders). Seconds per run; the result files of A and B are identical byte for
byte in every run.

| experiment | A1 | B1 | A2 | B2 |
|---|---|---|---|---|
| t020_polygons (00:42 to 01:37) | 293 | 296 | 291 | 288 |
| t101_network_od_pc4_dense | 215 | 226 | 213 | 219 |
| t410_NetworkModel_EU | 305 | 323 | 307 | 321 |
| t2000 (01:37 to 02:48) | 1156 | 1013 | 1075 | 1021 |
| t405_1 prepare (02:48 to 04:16) | 266 | 218 | 214 | 215 |
| t405_2 zonder fence | 1071 | 1075 | 1072 | 1103 |
| t101 again (04:18 to 04:30) | 193 | 187 | 183 | 182 |
| t410 again (04:31 to 04:50) | 271 | 289 | 273 | 289 |

t410 split at its first `impedance_matrix` progress line: the part before it, which is almost all
`connect_ne` (CreateInitialWorkingNetwork/LinkSet, 337 497 points on the roads, its "Connect discovery"
lines), and the part after it.

| t410 run | before / after the first `impedance_matrix` line, s | points discovered 79 s in |
|---|---|---|
| A/B 00:42, A1 / B1 / A2 / B2 | 166 / 139, 184 / 139, 169 / 138, 184 / 137 | 259 437 (A2), 244 693 (B2) |
| again 04:31, A1 / B1 / A2 / B2 | 138 / 132, 154 / 135, 137 / 136, 153 / 136 | 298 803, 267 604, 298 834, 269 489 |
| round, 20.22.0.m / 20.22.1.m | 153 / 143, 193 / 140 | |

t101 split the same way spends 6 to 7 s before `impedance_matrix` (a `connect` of 4066 points, 1 s on
its own) and the rest in one dense `impedance_matrix` of 16.5 million od pairs.

### t410 and GEO-A54: builds of `a847e23a3` and `e5e7c8d0b`

On 2026-10-02 the tree was checked out at `a847e23a3` (GEO-A41) and then at `e5e7c8d0b` (GEO-A54, which
changes `geo/dll/src/geom/SpatialIndex.h` only), each built with the VS18 msbuild (08:11 to 08:21, and
08:21 to 08:24, which recompiled 13 files of `Geo.dll`, `Connect.cpp` among them) and kept as
`C:\LocalData\GeoDMS_engine\bisect_a847e23a3` and `bisect_e5e7c8d0b`; then `main` again and rebuilt (08:24
to 08:34). t410 with full.py's command, threshold 60, nothing else running. Seconds: total, the part before
the first `impedance_matrix` line, and the points discovered 79 s in. The result file is the same in every
run.

| run, 08:34 to 09:04 | total | before | discovered |
|---|---|---|---|
| `a847e23a3`, three runs | 295, 289, 290 | 153, 154, 150 | 271 951, 270 740, 281 184 |
| `e5e7c8d0b`, interleaved | 302, 302, 300 | 166, 166, 164 | 257 031, 257 452, 259 346 |

The four builds in one sitting, in the order 20.22.0, `a847e23a3`, `e5e7c8d0b`, 20.22.1 and back:

| run, 09:05 to 09:44 | total | before | discovered |
|---|---|---|---|
| 20.22.0 (`1ec17232f`) | 289, 287 | 148, 150 | 276 761, 275 062 |
| `a847e23a3` | 289, 288 | 151, 153 | 275 076, 272 379 |
| `e5e7c8d0b` | 299, 305 | 162, 166 | 260 748, 257 158 |
| 20.22.1 (`fd30ef780`) | 304, 305 | 165, 162 | 258 079, 261 167 |

20.22.0 and the parent of GEO-A54 are equal, and so are GEO-A54 and 20.22.1; the step lies in that one
commit, in the `connect_ne`, and the `impedance_matrix` part after it is equal in all eight runs.

The GEO-A54 follow-up `20134d443` gives a node a pointer to that leaf instead of a copy of its extents and
narrows its two offsets and its count to `UInt32`, which brings a `dpoint` node back to 64 bytes. It was
built on `main` at `6b77b04cc` (11:36 to 11:38, the same 13 files) and kept as
`C:\LocalData\GeoDMS_engine\node64_geoa54`, then run interleaved with 20.22.1 and with the parent of GEO-A54:
same command and threshold, nothing else running (the machine watch saw the agent and the remote-desktop
host only, at most 14 s of CPU a minute). The result file is the same in all six runs.

| run, 11:40 to 12:10 | total | before | discovered |
|---|---|---|---|
| 20.22.1 (`fd30ef780`), 104-byte nodes | 307, 304 | 167, 167 | 258 699, 257 543 |
| `20134d443`, 64-byte nodes | 287, 293 | 150, 150 | 276 443, 279 174 |
| `a847e23a3`, 64-byte nodes, rescanning | 293, 297 | 152, 155 | 275 320, 270 666 |

The follow-up is 17 s ahead of 20.22.1 in the `connect_ne` in both pairs and level with the parent of
GEO-A54: the size of the node was the whole loss. The gain of GEO-A54 stays: a `join_near_values` of 10
points against 50 000 `dpoint` values of which 49 000 coincide (a probe in the session's scratch folder,
three interleaved runs per build) takes 1.19 s on `a847e23a3`, 0.001 to 0.002 s on 20.22.1 and 0.002 s on
the follow-up, with the same pairs.

### Reading

- Correct: all 27 `ok`, including t010, t641_2 and t2000, which were red on OVSRV10 before CLC-A07 and
  GEO-A19 were reverted, and t720_2BURP again, which 20.22.0 failed (#1285).
- The round's extra time is mostly the machine. In the A/B 20.22.1 is equal on t020 and t405 and faster
  on t2000 (1013 and 1021 s against 1156 and 1075), while the same 20.22.0 binary moved by more than the
  round's differences between sittings: t020 4:02 in its round against 4:53 and 4:51 in the A/B, t2000 16:52
  against 19:16 and 17:55, t101 213 to 215 s at 01:00 and 183 to 193 s at 04:20. A round on OVSRV05 is
  quieter than one on OVSRV10, but its wall times also move by about 10 % between sittings, so a
  difference below that is only an engine change when an A/B in one sitting shows it.
- t101 is not slower: B was slower in the first two pairs (+5 and +3 %) and not in the next two (−3
  and −0.5 %).
- t410 is slower in all four pairs, by 14 to 18 s (+5 to +7 %), and all of it lies in the `connect_ne`
  before the first `impedance_matrix` line: 153 to 154 s against 137 to 138 s in the second sitting,
  +11 to +12 %, with the discovery 10 % behind 79 s in; the `impedance_matrix` part is equal. That loop
  in `Connect.cpp` is unchanged since 20.22.0; what it calls per point is the spatial index query
  (`GetSqrProximityUpperBound` and the node iterator) and `ArcProjectionHandle`. Of the changes to those,
  GEO-A34 (`fccd29343`) leaves the arithmetic for float coordinates as it was, and t410 runs on `dpoint`;
  GEO-A41 (`a847e23a3`) does not apply to the arcs-first connect. GEO-A54 (`e5e7c8d0b`) adds the extents of
  a leaf and two flags to every node of the quadtree, which for `dpoint` grows a node from 64 bytes, one
  cache line, to 104, so every query walks larger nodes. The builds of `a847e23a3` and `e5e7c8d0b` below
  confirm it: GEO-A54 is the whole t410 loss, and nothing else between 20.22.0 and 20.22.1 changes t410.
  It costs the arcs-first connect of 337 497 points 11 to 14 s per adjacent pair, 7 to 9 %; the gain it
  was made for is on coincident objects (join_near_values over 49 000 coincident points, 2.1 s to
  0.008 s). The follow-up `20134d443` keeps that gain with a pointer instead of the copy; the node is
  64 bytes again and the `connect_ne` takes as long as before GEO-A54 (the third table above).
- t720 takes 10:12 against 9:12 for 20.21.1.m and 9:16 for 20.19.3.m. It has no A/B: 20.22.0 fails it,
  and a comparison needs a build of 20.21.1 (`92eaa7150`). t641.1 and t641.2 are 2 to 3 % slower in the
  round, within the spread between sittings; an A/B pair of them takes three and a half hours and was
  not run.
- Memory is equal or lower than 20.22.0.m: t2000 5.4 GB less live, t810 2.4 GB less, t641.2 382 trims
  against 471.

# Known causes of differences between versions

Changes found in the code history that explain why a figure differs between versions, recorded here so
that a round is not bisected for a cause that is already known.

## FixedAlloc's free stacks: which sizes they keep, and since when

Established on 2026-10-01 from the history of `rtc/dll/src/mem/FixedAlloc.cpp`; no new measurement.

`AllocateFromStock` serves a request from a free stack only when `SpecialSize` admits it. The request
then takes the object store of the next power of 2 (`BlockListIndex`), `VirtualAllocChunk::commit`
commits that whole store, and when the block is freed the store goes onto its class's free stack still
committed. A request that `SpecialSize` refuses goes to `std::allocator` (the CRT heap) at the size asked
for and goes back there when freed; FixedAlloc keeps none of it.

| versions | code | sizes served from a free stack |
|---|---|---|
| 8.035 to 8.6.0 | `SpecialSize`: `sz > 4 KB && sz <= 256 MB`, from before the start of the git history | every size above 4 KB up to 256 MB, rounded up to a power of 2 |
| 8.6.1 to 19.3.0 | `be8e162fc` (2022-12-05), `MG_CACHE_ALLOC_ONLY_SPECIALSIZE` | only the exact powers of 2 from 8 KB to 1 MB: the buffers of a full default tile, 2^16 elements of 1/8 to 16 bytes |
| 20.0.0 and later | `87bdc62d1` (2026-05-08, "drop power-of-2 guard from SpecialSize") | every size from 8 KB to 1 MB, rounded up to the next power of 2 |

`87bdc62d1` is already in `v20.0.0` (2026-05-11), on its first-parent line, even though the 20.0.4
release notes list it among the changes since 20.0.0c. 19.3.0 (2026-04-03) is the last release without
it. In June 2024 `SpecialSize` returned `true` for every size for two days (`eeb127377`, reverted by
`78503eedd`); both commits first appear in 15.4.0, so no release shipped that state.

**Since `87bdc62d1`, no size above 1 MB goes through a free stack.** `SpecialSize` admits
`(1 << log2_default_segment_size) / 8` to `(1 << log2_default_segment_size) * sizeof(Float64) * 2`, and
`log2_default_segment_size` is 16 (`RtcBase.h`), which gives 8 KB to 1 MB. The 256 MB of
`ALLOC_OBJSSIZE_MAX` only sets how many free-stack classes exist (`NR_FREE_STACK_ALLOCS`, 4 KB to
256 MB). The classes above 1 MB are never reached. Until 2026-10-01 two texts said otherwise, and
both are corrected now. One was the comment above `s_AllocSizeHistogram` in `FixedAlloc.cpp`, which
called 256 MB the cut-off above which requests bypass the free stacks. The other was §8.1.23 of
`doc/development/schedule-with-lookahead.md`, which said "Pool classes span 4 KB–256 MB". The comment
above `VirtualAllocChunk::recommit` and §8.1.20 already gave the range correctly.

**What 20.0.0 changed for committed memory.** Before 20.0.0, a request between 8 KB and 1 MB that was
not a power of 2 went to the CRT heap at its own size. Examples are the buffers of a domain's last,
shorter tile, the sequence and string payloads of a tile, and a growing vector. Since 20.0.0 such a
request takes a power-of-2 store:

1. While the block lives, up to twice its size is committed: a 520 KB request commits a 1 MB store.
   Until 20.22.0 the census (`PeakLiveLarge`) counted the size asked for, so the rounding showed up only
   in `Highest CommitCharge`. Since `52e6c7da4` (20.22.0), `my_vector`, `my_vec_t`, the sequence pools
   and `BitVector` take the whole store as their capacity, and the census counts the store.
2. After the block is freed, its store stays committed on the free stack for the rest of the run. No
   version decommits a free-stack store when it is freed: `release()` was a no-op through 20.9.0, and
   since 20.10.0 `DECOMMIT_MIN_SIZE` decommits only stores of 2 MB and up, a size no free-stack class
   reaches. Before 20.0.0 this retention applied only to the power-of-2 tile buffers.

So from 20.0.0 through 20.10.0 every non-power-of-2 size between 8 KB and 1 MB that a model churns
adds to a committed dead pool that is not returned during the run. Since 20.11.0, drainage gives part
of it back (`7938d2605`, on by default since `30df2d1a3`; §8.1.24 and §8.1.32): while the machine's RAM
use is above `MemoryFlushThreshold`, it decommits the cold half of each free stack. Below the threshold
the pool stays committed, as in 20.0.0. The `drained Nx = M[MB]` figure of the `vmcalls` line says how
much came back; for t641.1 in the 20.22.0.m round on OVSRV10 above that was 2678 MB.

**How large the effect is has not been measured.** No A/B of `87bdc62d1` against its parent exists. What
has been measured is the whole free-stack pool, power-of-2 and other sizes together, in §8.1.14 of
`schedule-with-lookahead.md`. That run used t641 with the gate enforcing 100 GB on the document's 128 GB
host (OVSRV10). Without decommit, `Highest CommitCharge` was 179 256 MB against a `PeakLiveLarge` of
144 449 MB on t641_1, and 197 179 against 175 973 MB on t641_2. A local build that decommitted every
freed store brought commit down to live (143 793 and 175 626 MB). So the pool held about 35 GB and
21 GB at the peak. §8.1.23 measured the committed dead pool at 53 to 112 GB over the course of t641.

To measure the share of `87bdc62d1`, use the current tree with `&& IsIntegralPowerOf2OrZero(sz)`
restored in `SpecialSize`, A/B against HEAD on t641_1 and t641_2 with `run_exp.py` on OVSRV10, and
compare `Highest CommitCharge`, `PeakLiveLarge` and the drained megabytes. Building `87bdc62d1` and its
parent instead would also measure the five months of other changes since.

**Reading a comparison of rounds:**

- A higher `Highest CommitCharge` from 20.0.0 on, against 19.x, with an unchanged `PeakLiveLarge` is this
  retention and rounding, not a leak.
- Between 20.10.x and earlier and 20.11.0 and later, drainage changes commit charge on any model that
  pushes RAM use past `MemoryFlushThreshold`. Compare at the same threshold and on the same machine.
- `PeakLiveLarge` from 20.22.0 on includes the rounding of the containers named above. The gap between
  `Highest CommitCharge` and `PeakLiveLarge` is therefore smaller from 20.22.0 on, partly through
  bookkeeping.
