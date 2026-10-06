# Performance tests of the #1259 deferral, 20.20.0 to 20.21.1

*Archived 2026-10-06: the deferral of commits and IntegrityChecks that these rounds measure was removed
again on 2026-09-21 (a7127224f, 92eaa7150), so the part of `doc/performance-test.md` that held them is a
log of a removed feature. Successor: `doc/performance-test.md`, which keeps the rounds from 20.22.0 on
and the known causes of differences between versions.*

This is lines 1 to 722 of `doc/performance-test.md` as of 2026-10-06 (at 0563aa5a0), moved verbatim:
the two test requests, the OVSRV05 results of 20.20.0, 20.21.1 and 20.21.1 without deferral, and the
reading of the 20.20.0 t641 columns. Where it cites section 8.1.x of
`doc/development/schedule-with-lookahead.md`, that section is now in
`doc/archive/schedule-with-lookahead-log.md`.

---

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
