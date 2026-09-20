# Performance test request: 20.21.1 (a deferred check is counted and charged) against 20.21.0.m and 20.20.0.m

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
