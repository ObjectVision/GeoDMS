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
