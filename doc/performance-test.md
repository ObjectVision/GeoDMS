# Performance tests of the regression rounds, 20.22.x

*Status (2026-10-07): a measurement log of the full.py regression rounds from 20.22.0 on, per machine,
followed by the known causes of differences between versions. The rounds of 20.20.0 and 20.21.x, which
measured the deferral of commits and IntegrityChecks of #1259 that was removed again (a7127224f,
92eaa7150), moved on 2026-10-06 to
[`doc/archive/performance-test-20.20-20.21.md`](archive/performance-test-20.20-20.21.md); where a
section below says "above" of one of them (round 3 of "20.21.1 without deferral", the OVSRV05 section
of 20.20.0, the `/SH` commit e11c1ac of GeoDMS-Test), that file is meant. The latest round, 20.23.0 at
d4f23aa43 on OVSRV05, gives 27 of 27 `ok` and the times of 20.22.1. Section 8.1.x of
`doc/development/schedule-with-lookahead.md` is now in `doc/archive/schedule-with-lookahead-log.md`.*

## Results (OVSRV10), 20.22.0

Run on 2026-09-27 on OVSRV10, 127 GB, 32 logical processors, `MemoryFlushThreshold` 90 (the ledger
lines of the 20.20.0.m column on this machine read `budget 117094 MB`). Tree `C:\dev\GeoDMS_2026` on
`main` at `1ec17232f` (version 20.22.0), so without deferral as in round 3 (archived), plus `ccda904fa`
and `53c0931b4` (the low-RAM brake keeps one context activated or running), which had been verified
in Debug only. GeoDMS-Test at `e452efc`, which does not have the `/SH` commit `e11c1ac` of the
archived 20.21.1 section (it is not on origin), so t1742 still depends on the account.

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
  OVSRV05 (64 GB, archived section) t641.1 lost its 20.20.0 speed; with 127 GB it does not. Why the
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
"20.21.1 without deferral", now in the archive; 20.19.3.m ran under another account, whose threshold is presumably the
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

## Results (OVSRV05), 20.22.1 at `1315b0357`: the quadtree changes of #1289

Run on 2026-10-02 from 18:33 to 21:52 on OVSRV05, threshold 60 as above. Tree `C:\dev\GeoDMS` on `main` at
`1315b0357`: the round above (`fd30ef780`) plus the GEO-A54 follow-up `20134d443` (64-byte nodes) and the
three #1289 commits, `108459dd2` (a node no longer splits into a copy of itself when `Center` cannot halve
its box), `271922804` (a node splits from 16 objects instead of 4, `SpatialIndex::MinObjectsToSplit`) and
`1315b0357` (a Debug-only check); the only code they change is `geo/dll/src/geom/SpatialIndex.h`.
GeoDMS-Test at `1b8aa71`, as above. Built with the VS18 msbuild, 18:29:22 to 18:31:41, exit 0, which
recompiled the 13 files of `Geo.dll` that include the header (`Geo.dll` 18:31:08, the other DLLs of the
morning's rebuild of `main`); `testcases\run_testcases.bat` 443 cases, 0 bad. Kept as
`C:\LocalData\GeoDMS_engine\head_1315b0357`. The `20_22_1_m` column of the round above went to
`_Archive\20_22_1_m_fd30ef780_20261001`, its report with it, and its intermediates to
`C:\LocalData\runs\_aside_20_22_1_m_fd30ef780_20261001`; full.py ran 32 experiments with no reuse.

Running on the machine: two VS18 `devenv` windows, open since 12:53 and 14:58, mostly idle; the Claude
desktop app's main process at a full core from about 18:50 to about 21:30 (295 to 301 s of CPU per
5 minutes, 4 % of the 24 logical processors), the Chrome Remote Desktop host at 10 to 20 % of a core.
The #1289 session had agreed to start nothing until the round had finished.

### Nine of 27 report rows are red, all from the split threshold

The report gives 18 of 27 `ok`. Every red row is a changed result, not a crash or a slower run: exit 0
everywhere except t060, whose file comparison failed (99). The same experiments were rerun with
`run_exp.py` on the kept builds, and on `C:\LocalData\GeoDMS_engine\spi_knob`, a build of the #1289
session after `108459dd2` that reads the split threshold from `GEODMS_SPI_MINOBJECTS`, run with 4 and with
16 one after the other:

| test | `fd30ef780` | `node64_geoa54` (`20134d443`) | `spi_knob`, 4 | `spi_knob`, 16 | `1315b0357` |
|---|---|---|---|---|---|
| t010 | OK | | OK | `point_in_ranked_polygon` fails | `point_in_ranked_polygon` fails |
| t100 connect points | 6 639 850 | 6 639 850 | 6 639 850 | 6 639 852 | 6 639 852 |
| t102 OD cells | 7 812 324 | 7 812 324 | 7 812 324 | 7 812 314 | 7 812 314 |
| t101 cells that differ from the reference | 0 | | 0 | 8180 | 8180 |
| t301 panden that differ from the reference | 0 | | 0 | 16 048 | 16 048 |
| t060 gpkg against the reference | same | | same | differs | differs |
| t910 Transport Accessibility difference | −1 051 036 480 | | −1 051 036 480 | −1 050 769 728 | −1 050 769 728 |
| t2000 hWP_asl StartJaar, R5_2025 | 50 003, 93 069 | | 50 003, 93 069 | 49 997, 92 671 | 49 997, 92 671 |
| t641.2 2050 Woningen, Banen | 9 590 219, 9 893 365 | | | | 9 590 281, 9 893 214 |

t010 fails `point_in_ranked_polygon/test_attr` and `point_in_ranked_polygon/rank_constant/test_attr`, in
all four tilings. t641.2 was not rerun (the chain takes about 100 minutes per setting); its change is of the
same kind. On the one binary, 4 gives the results of every earlier round and 16 the results of this one,
so the threshold is the whole change; the GEO-A54 follow-up and `108459dd2` change no result.

Why a threshold changes a result: these operators let the order in which the quadtree hands out its
candidates decide between candidates that qualify equally. `point_in_polygon` (`OperPolygon.cpp`, the
kernel at line 1388) returns the first polygon found that contains the point, so a point inside two
polygons, such as a BAG verblijfsobject on the shared wall of two panden (t301 and t060 through
`MakeSnapshot.dms`), gets whichever the index visits first. `point_in_ranked_polygon` keeps the first of
equal rank (`thisRank > foundRank`), which in `rank_constant` is every polygon. `connect` takes the first of
equally near arcs. With 16 objects per node instead of 4 the tree has other nodes and other leaf lists, and
the first candidate is another one. The results of the earlier rounds were no more defined than these;
they were the visiting order of a quadtree with 4 objects per node, which the references recorded.

### Wall time, the span between the first and the last timestamp of the GeoDMS log

| test | `1315b0357` (02-10) | `fd30ef780` (01-10) | 20.22.0.m | 20.21.1.m | 20.20.0.m | 20.19.3.m | against 20.22.0.m |
|---|---|---|---|---|---|---|---|
| t020 | 0:04:12 | 0:05:03 | 0:04:02 | 0:03:56 | 0:03:47 | 0:03:44 | 1.04 |
| t060 | 0:02:07 | 0:01:47 | 0:02:22 | 0:02:11 | 0:01:46 | 0:01:59 | 0.89 |
| t101 | 0:03:34 | 0:03:50 | 0:03:31 | 0:03:16 | 0:03:16 | 0:03:08 | 1.01 |
| t200 | 0:00:38 | 0:00:40 | 0:00:38 | 0:00:37 | 0:00:32 | 0:00:32 | 1.00 |
| t300 | 0:01:58 | 0:01:09 | 0:01:55 | 0:01:56 | 0:00:43 | 0:01:19 | 1.03 |
| t301 | 0:02:43 | 0:02:48 | 0:02:38 | 0:02:31 | 0:02:26 | 0:02:31 | 1.03 |
| t405.1 | 0:04:27 | 0:04:44 | 0:04:31 | 0:04:29 | 0:01:57 | 0:11:37 | 0.99 |
| t405.2 | 0:18:46 | 0:20:47 | 0:19:07 | 0:18:22 | 0:15:21 | 0:18:24 | 0.98 |
| t405.3 | 0:18:42 | 0:20:36 | 0:18:39 | 0:18:26 | 0:15:09 | 0:18:09 | 1.00 |
| t410 | 0:04:03 | 0:05:34 | 0:04:56 | 0:04:49 | 0:04:38 | 0:04:43 | 0.82 |
| t611 | 0:00:31 | 0:00:34 | 0:00:31 | 0:00:31 | 0:00:31 | 0:00:30 | 1.00 |
| t641.1 | 0:52:49 | 0:53:08 | 0:51:32 | 0:51:50 | 0:24:11 | 0:50:55 | 1.02 |
| t641.2 | 0:46:06 | 0:44:25 | 0:43:25 | 0:44:06 | 0:58:50 | 0:46:27 | 1.06 |
| t710 | 0:01:11 | 0:01:08 | 0:01:02 | 0:01:01 | 0:01:03 | 0:01:01 | 1.15 |
| t720 | 0:09:30 | 0:10:12 | failed, #1285 | 0:09:12 | 0:09:18 | 0:09:16 | - |
| t810 | 0:05:02 | 0:05:14 | 0:05:02 | 0:05:12 | 0:04:49 | 0:04:57 | 1.00 |
| t910 | 0:00:50 | 0:00:53 | 0:00:50 | 0:00:48 | 0:00:44 | 0:00:46 | 1.00 |
| t2000 | 0:17:58 | 0:19:36 | 0:16:52 | 0:17:01 | 0:17:11 | 0:16:25 | 1.07 |

Summed over the 32 logs: 196.8 min, against 203.7 for `fd30ef780`, 183.6 for 20.22.0.m (of which t720,
failing early, 0.4), 191.8 for 20.21.1.m, 167.9 for 20.20.0.m and 198.0 for 20.19.3.m. The timings of the
threshold pairs, 4 against 16 on `spi_knob` in one sitting: t910 55 and 52 s, t301 170 and 171 s, t060 104 s
and 100 s, t101 194 and 196 s, t2000 1069 and 1073 s.

Memory against `fd30ef780`, Highest CommitCharge / PeakLiveLarge in MB: t2000 80 796 / 77 250 against
63 591 / 64 231 (20.22.0.m 66 137 / 69 628), which is not the threshold's: the pair gives 67 770 / 63 563
at 4 and 62 282 / 60 570 at 16, and the A/B above 62 871 to 67 277 MB live for either build; t405.2 and t405.3 48 922 and 47 771 / 31 383 against
30 830 and 30 854 / 31 383, a commit peak that moved between 29.5 and 46.9 GB on one build in the A/B
above; t720 21 261 / 19 754 against 17 902 / 18 758; t710 5874 / 3508 against 6792 / 4183; t300 and
t405.1 1.1 GB more and 1.8 GB less commit at the same PeakLiveLarge; every other test within
300 MB on both figures.

### Reading

- Not releasable as it stands: `271922804` changes the results of nine release tests, t010's operator
  test among them, and model outputs with them (t2000's heat pumps, t641.2's allocation, t910's
  accessibility). None of them is wrong in a way the code would notice; all of them depend on how the
  quadtree is split, and `point_in_polygon`, `point_in_ranked_polygon` and `connect` resolve ties by its
  visiting order. Two ways out, both outside this measurement: break those ties by a property of the
  candidates (the lowest polygon or arc index, say), which makes the results independent of the tree but
  changes the references once more; or keep the threshold at 4, which restores every earlier result and
  gives up the #1289 speedup. *(Added 2026-10-06: the first way out was taken, keeping the threshold at
  16: tie rules for `point_in_ranked_polygon`, `connect`, `canyon` and `box_connectivity` (#1290:
  f6d34a69a, 9d23831a8, 1a340596e, b854591e6), with #1291 (11ff62902) for the boxes of
  `box_connectivity` that share only an edge or a corner, while `point_in_polygon` still returns the
  first polygon found; the round at 15651313f, recorded in the next section (6bdbc0911), gives 27 of
  27 `ok` after GeoDMS-Test moved the references.)*
- The speedup is real: t410 4:03 against 4:56 for 20.22.0.m and 5:34 for the round above, as the #1289
  session measured (`connect_ne` 98 s against about 150 s). Nothing measured
  is slower for the threshold: in the pairs on one binary t101, t301, t910, t060 and t2000 take the same
  time at 4 and at 16 (within 4 s), so the expected cost on an unfiltered nearest-arc search does not
  show in these tests.
- The rest of the round is at the 20.22.0.m times, within the drift between sittings that the section
  above measured (t641.2 +6 %, t710 +9 s); t405.2 and t405.3 are back to 18:46 and 18:42, the round above
  having been the slow sitting.

## Results (OVSRV05), 20.22.1 at `15651313f`: ties by the lowest index (#1290)

Run from 2026-10-03 23:42 to 2026-10-04 03:02 on OVSRV05, threshold 60 as above, 3:16:53, full.py exit 0. Tree
`C:\dev\GeoDMS` on `main` at `15651313f`: the round above (`1315b0357`) with 17 commits of `origin/main`
merged in, among them the #1290 tie rules, `f6d34a69a` (`point_in_ranked_polygon` takes the highest rank and,
of equal ranks, the lowest index), `9d23831a8` (`connect`, `connect_neighbour` and `connect_info` take, of
equally near arcs, the lowest index, and search a box that still holds an arc at exactly the best distance),
`1a340596e` (`canyon`), `b854591e6` and `11ff62902` (`box_connectivity`); further `35f7fd470` (the readiness
assertions of `OperationContext::Join`, Debug only), `2d3f05496` (`parse_xml`), `182bc1b80` (#1288, MMD),
`e04ad20f0` (#1287, `pareto_optimal`) and `1155a678e` (ODBC). GeoDMS-Test at `1b8aa71`. Built with the VS18
msbuild, 23:36:13 to 23:41:47, exit 0, which relinked Rtc, Stg, Stx, Clc, Geo, Shv and both executables;
`testcases\run_testcases.bat` 455 cases, 0 bad (the 443 above and the 12 cases of the merge). Kept as
`C:\LocalData\GeoDMS_engine\head_15651313f`. The column of the round above went to
`_Archive\20_22_1_m_1315b0357_20261002`, its intermediates to `C:\LocalData\runs\_aside_20_22_1_m_1315b0357_20261002`;
full.py ran 32 experiments with no reuse.

Running on the machine, from five-minute samples: the Chrome Remote Desktop host 1 707 s of CPU over the
round (about 15 % of a core), Explorer 145 s, the Claude desktop app 137 s, two idle VS18 windows.

### Wall time, the span between the first and the last timestamp of the GeoDMS log

| test | `15651313f` (03-10) | `1315b0357` (02-10) | `fd30ef780` (01-10) | 20.22.0.m | against `1315b0357` |
|---|---|---|---|---|---|
| t020 | 0:04:03 | 0:04:12 | 0:05:03 | 0:04:02 | 0.96 |
| t060 | 0:02:03 | 0:02:07 | 0:01:47 | 0:02:22 | 0.97 |
| t101 | 0:03:29 | 0:03:34 | 0:03:50 | 0:03:31 | 0.98 |
| t200 | 0:00:38 | 0:00:38 | 0:00:40 | 0:00:38 | 1.00 |
| t300 | 0:01:56 | 0:01:58 | 0:01:09 | 0:01:55 | 0.98 |
| t301 | 0:02:36 | 0:02:43 | 0:02:48 | 0:02:38 | 0.96 |
| t405.1 | 0:04:20 | 0:04:27 | 0:04:44 | 0:04:31 | 0.97 |
| t405.2 | 0:18:25 | 0:18:46 | 0:20:47 | 0:19:07 | 0.98 |
| t405.3 | 0:18:37 | 0:18:42 | 0:20:36 | 0:18:39 | 1.00 |
| t410 | 0:03:59 | 0:04:03 | 0:05:34 | 0:04:56 | 0.98 |
| t611 | 0:00:31 | 0:00:31 | 0:00:34 | 0:00:31 | 1.00 |
| t641.1 | 0:52:16 | 0:52:49 | 0:53:08 | 0:51:32 | 0.99 |
| t641.2 | 0:46:26 | 0:46:06 | 0:44:25 | 0:43:25 | 1.01 |
| t710 | 0:01:06 | 0:01:11 | 0:01:08 | 0:01:02 | 0.93 |
| t720 | 0:09:37 | 0:09:30 | 0:10:12 | failed, #1285 | 1.01 |
| t810 | 0:05:09 | 0:05:02 | 0:05:14 | 0:05:02 | 1.02 |
| t910 | 0:00:51 | 0:00:50 | 0:00:53 | 0:00:50 | 1.02 |
| t2000 | 0:19:36 | 0:17:58 | 0:19:36 | 0:16:52 | 1.09 |

Summed over the 32 logs: 197.2 min, against 196.8 for `1315b0357`, 203.7 for `fd30ef780` and 183.6 for
20.22.0.m (of which t720, failing early, 0.4). t100 and t102, the connect-only tests, take 4 and 6 s as before.

Memory against `1315b0357`, Highest CommitCharge / PeakLiveLarge in MB: t2000 66 765 / 77 313 against
80 796 / 77 250; t405.2 and t405.3 29 728 and 28 606 / 31 383 against 48 922 and 47 771 / 31 383, the commit
peak that moved between 29.5 and 48.9 GB on one build before; t641.1 145 029 / 136 920 against 140 145 /
136 920; t641.2 176 362 / 168 907 against 170 038 / 168 907; t720 25 210 / 20 361 against 21 261 / 19 754;
t910 4 461 / 4 786 against 3 202 / 4 785; every other test within 1 GB on both figures.

### Five red rows follow the new rule, four still follow the tree

The report of the round gave 18 of 27 `ok`, the same nine red rows as the round above. To tell a result
of the new rule from one of the tree, `15651313f` was built a second time with `MinObjectsToSplit = 4` in
`geo/dll/src/geom/SpatialIndex.h` (16:24 to 16:30 on 04-10, kept as
`C:\LocalData\GeoDMS_engine\knob4_15651313f`; the source restored and rebuilt after), and the nine
experiments rerun on it with `run_exp.py` from 16:33 to 18:54, the t641 chain included. Its timings are not
comparable: probes of this section ran beside it.

| test | `fd30ef780` (4) | `1315b0357` (16) | `15651313f` at 16, the round | `15651313f` at 4 |
|---|---|---|---|---|
| t010 `point_in_ranked_polygon` of vbo 1 | 3 | 14 | 1 | 1 |
| t100 connect points | 6 639 850 | 6 639 852 | 6 639 869 | 6 639 869 |
| t102 OD cells | 7 812 324 | 7 812 314 | 7 812 248 | 7 812 248 |
| t101 cells that differ from the 2018 reference | 0 | 8 180 | 8 180 | 8 180 |
| t910 Transport Accessibility difference | −1 051 036 480 | −1 050 769 728 | −1 051 017 024 | −1 051 017 024 |
| t301 panden that differ from the reference | 0 | 16 048 | 16 048 | 0 |
| t060 gpkg against the reference | same | differs | differs | same |
| t2000 hWP_asl StartJaar, R5_2025 | 50 003, 93 069 | 49 997, 92 671 | 49 997, 92 671 | 50 003, 93 069 |
| t641.2 2050 Woningen, Banen | 9 590 219, 9 893 365 | 9 590 281, 9 893 214 | 9 590 281, 9 893 214 | 9 590 219, 9 893 365 |

The first five rows give one value at 4 and at 16: `connect` and `point_in_ranked_polygon` no longer depend
on the tree. Where `connect` changed, a dump of `connect_info` per point (a probe that includes `stam.dms` of
the regression and writes `arc_rel`, `dist`, `InArc`, `InSegm` and `SegmID` per location) on `fd30ef780`,
`1315b0357` and `15651313f` shows what changed:

- PC6, 458 108 points, `fd30ef780` to `15651313f`: 707 points on another arc at the same distance, every one
  of them on the arc with the lower index; 16 on an arc 0.1 to 28 mm nearer, which the old search box,
  `Inflate` by the distance in float32 at RD coordinates where a float32 step is 1.5 to 3 cm, had left out;
  none on a farther arc. `connect` writes 2 points for a cut inside a segment and 1 for a cut at a vertex, none
  at an arc's end: ten of the 16 went from an arc's end to inside a segment and one from a vertex, +21 points,
  and the ties net −2, which is the +19 of t100. `fd30ef780` to `1315b0357`, by contrast: 237 ties moved, 117
  to a lower index and 120 to a higher one, 3 points to a nearer arc and 1 to a farther one.
- PC4, 4 066 points: 14 ties, all to the lower index. Only one of them splits an arc: point 3496 lies 3.98 m
  from arcs 296241 and 264333, inside a segment of each, and now splits 264333, as the visiting order at 16
  already did. That one other network is the 8 180 cells of t101; the other 13 cut at the same point on
  either arc, an end both arcs share, and `1315b0357` gives the matrix of `15651313f` cell for cell.
- t010: vbo 1 lies in pands 1 to 14, identical polygons with one rank, 0.00178042368. 4 gave 3, 16 gave 14,
  the rule gives 1, in all four tilings.

The last four rows give the reference at 4 and the values of the round at 16. All four go through
`point_in_polygon`, which returns the first polygon found that contains the point and stays so by design (the
comment at the kernel in `OperPolygon.cpp`; the 20.22.1 release notes: which of several polygons it gives is
unspecified). In t060 the difference is `vbo/pand_id_geom` of 3 331 of the 731 571 verblijfsobjecten, which
lie in more than one pand, and the per-pand counts and `woningtype` that follow from it; the gpkg of 4 is
equivalent to the reference by the harness's own comparison. t301, t2000 and t641.2 were classified by the
threshold only, not traced to an item.

GeoDMS-Test `f7891d5` (not pushed) moves the five: a 20.22.1 epoch in `references.json` for t100, t102 and
t910; for t101 a store `TestReferenceFiles\t101\PC4_impedance_v20221.fss` recorded from `15651313f`, chosen by
`GeoDMSVersion()` (it exists on OVSRV05 only; every machine that runs the round needs a copy); for t010 the
expectation `[0,1,18,15]` from 20.22.1 on, and a `rank_constant` that tests something: its `test_attr` read
`all(test)`, which resolved to the parent's `test`. Both branches were run, the new on `15651313f` at 4 and 16,
the old on 20.21.0.m, 20.8.0.m, 18.1.2 and 17.4.6. t010 and t101 were then rerun inside the round
(`full.py -resume -tests`, 18:55 to 19:00 on 04-10, 0:00:08 and 0:03:29 as in the round), which regenerated
the report: 23 of 27 `ok`, red t060, t301, t2000 and t641.2.

GeoDMS-Test `f6fe770` (not pushed) gives those four a 20.22.1 reference as well, by decision, and says at
each that it records the order of the tree: an epoch in `references.json` for t2000 and t641.2;
`TestReferenceFiles\t301\type_woonpand_rel_ok_v20221.fss` for t301 (0 panden differ on `15651313f` against it,
and 0 on 20.21.0.m against the old store); `TestReferenceFiles\t060\snapshot_Utrecht_20210701_v20221.gpkg`,
the gpkg of the round, chosen by `full.py` for 20.22.1 and later. The two stores exist on OVSRV05 only, like
the t101 store. t060 and t301 were rerun inside the round (00:11 to 00:18 on 05-10), which regenerated the
report: 27 of 27 `ok`; the older columns keep their verdicts.

### Reading

- No performance effect: 197.2 min against 196.8, every test within the drift between sittings. t2000's
  +1:38 is inside the 16:52 to 19:36 that this machine has measured for it (`fd30ef780` 19:36 as well, the
  A/B above 17:55 to 19:16 for one binary); an interleaved A/B of `head_1315b0357` and `head_15651313f` would
  settle it. t410 keeps the #1289 gain, 3:59, and the per-arc handle and the wider search box of `connect`
  do not show in t100, t101, t102 or t410.
- `connect` was not only order-dependent but also missed the nearest arc: 16 of the 458 108 PC6 points, by up
  to 28 mm, at any threshold. Its results now follow from the arcs alone.
- The four rows that stayed red are `point_in_polygon`'s free choice among overlapping polygons, which the
  references recorded as the visiting order of a tree that split at 4. Their 20.22.1 references record that
  of a tree that splits at 16: the release changes these model outputs, by up to 0.66 % (t2000's hWP_asl
  R2_2022), and the next change to the index may move them again without any change to a model. Such a
  difference in t060, t301, t2000 or t641.2 is first a question of the visiting order.

## Results (OVSRV05), 20.23.0

Run from 2026-10-07 01:27 to 04:43 on OVSRV05, threshold 60 as above (set by
`scratch\launch_perf_20230.cmd` and put back to 95 after it), full.py exit 0, nothing on stderr; report
header 3:13:24, 27 of 27 `ok`. Tree `C:\dev\GeoDMS` on `main` at `d4f23aa43` (the version bump to 20.23.0),
47 commits after `15651313f`: the dead-code deletions of B7 in every module, the #1282 epsilon-dominance of
`pareto_optimal_eps` and `pareto(imp2_epsilon)`, the #1284 ceiling of `JoinOperationThatMissesWantedMember`
and the join of a `storage_read_table` member to its table's read, GEO-A29 (`impedance_matrix` with
`precalculated_NrDstZones`), the checked raster sizes and the writers of shp, dbf and tif that report a
failed write, and the GUI changes of SHV-A07 to A13. GeoDMS-Test at `f6fe770`, which carries the 20.22.1
references of t010, t060, t100, t101, t102, t301, t910, t2000 and t641.2. The build was made before the
round (`Rtc.dll` 01:02, `GeoDmsRun.exe` 01:06, `GeoDmsGuiQt.exe` 01:15; the version string 20.23.0);
`testcases\run_testcases.bat` 471 cases, 0 bad. Kept as `C:\LocalData\GeoDMS_engine\head_d4f23aa43`. No
`20_23_0_m` folder existed beforehand; full.py ran 32 experiments with no reuse.

Running on the machine, from five-minute samples: the Chrome Remote Desktop host 2 215 s of CPU over the
round (about 19 % of a core), the Claude desktop app 661 s, its webview 76 s, an idle VS18 window 18 s;
together under 1 % of the 24 logical processors. Free physical memory fell to 1.3 GB during t641.1.

### Wall time, the span between the first and the last timestamp of the GeoDMS log

| test | 20.23.0.m (07-10) | `15651313f` (03-10) | `1315b0357` (02-10) | 20.22.0.m | against `15651313f` |
|---|---|---|---|---|---|
| t020 | 0:04:04 | 0:04:03 | 0:04:12 | 0:04:02 | 1.00 |
| t060 | 0:01:52 | 0:02:03 | 0:02:07 | 0:02:22 | 0.91 |
| t101 | 0:03:21 | 0:03:29 | 0:03:34 | 0:03:31 | 0.96 |
| t200 | 0:00:38 | 0:00:38 | 0:00:38 | 0:00:38 | 1.00 |
| t300 | 0:01:57 | 0:01:56 | 0:01:58 | 0:01:55 | 1.01 |
| t301 | 0:02:51 | 0:02:36 | 0:02:43 | 0:02:38 | 1.10 |
| t405.1 | 0:04:26 | 0:04:20 | 0:04:27 | 0:04:31 | 1.02 |
| t405.2 | 0:18:45 | 0:18:25 | 0:18:46 | 0:19:07 | 1.02 |
| t405.3 | 0:18:18 | 0:18:37 | 0:18:42 | 0:18:39 | 0.98 |
| t410 | 0:03:53 | 0:03:59 | 0:04:03 | 0:04:56 | 0.97 |
| t611 | 0:00:31 | 0:00:31 | 0:00:31 | 0:00:31 | 1.00 |
| t641.1 | 0:51:01 | 0:52:16 | 0:52:49 | 0:51:32 | 0.98 |
| t641.2 | 0:44:53 | 0:46:26 | 0:46:06 | 0:43:25 | 0.97 |
| t710 | 0:01:06 | 0:01:06 | 0:01:11 | 0:01:02 | 1.00 |
| t720 | 0:09:51 | 0:09:37 | 0:09:30 | failed, #1285 | 1.02 |
| t810 | 0:05:06 | 0:05:09 | 0:05:02 | 0:05:02 | 0.99 |
| t910 | 0:00:50 | 0:00:51 | 0:00:50 | 0:00:50 | 0.98 |
| t2000 | 0:18:44 | 0:19:36 | 0:17:58 | 0:16:52 | 0.96 |

Summed over the 32 logs: 193.7 min, against 197.2 for `15651313f`, 196.8 for `1315b0357` and 183.6 for
20.22.0.m, which with a t720 of 9.8 min instead of its early failure comes to 193.0. The `15651313f` column
gives the times of its round; the logs of t010, t060, t101 and t301 in `20_22_1_m` are now those of the
reruns of 04-10 and 05-10 (t060 2:06, t301 3:01), which bring `durations.py` to 197.7 for that folder.

Memory against `15651313f`, Highest CommitCharge / PeakLiveLarge in MB: t2000 63 200 / 64 459 against
66 765 / 77 313 (`fd30ef780` 63 591 / 64 231, the A/B of 02-10 62 871 to 67 277 live for one build);
t641.1 141 334 / 136 920 against 145 029 / 136 920, t641.2 169 508 / 168 907 against 176 362 / 168 907;
t720 20 656 / 20 049 against 25 210 / 20 361; t910 3 313 / 4 784 against 4 461 / 4 786; t060 8 372 /
7 823 against 7 023 / 7 826 and t020 2 953 / 2 609 against 1 975 / 2 609, more commit at the same live
peak; t301 20 738 / 3 413 against 19 515 / 3 207 for `1315b0357`; every other test within 1 GB on both
figures. `Calling EmptyWorkingSet`: t641.1 262 against 282, t641.2 483 against 448, t2000 55 against 66,
t405.2 4 against 9. Report cells, peak physical / peak committed in GB: t641.1 54.90 / 157.55, t641.2
54.25 / 191.38, t2000 42.94 / 82.37, t405.2 30.37 / 52.17.

### Reading

- Correct: 27 of 27 `ok` against the references of GeoDMS-Test `f6fe770`, so none of the 47 commits changes
  a result that the round checks, the tree-order rows t060, t301, t2000 and t641.2 included.
- No performance effect: 193.7 min against 197.2, and every test within the drift between sittings that
  this machine has shown (about 10 %). The largest steps are t060 −11 s, whose 1:52 lies inside the 1:46 to
  2:22 of the rounds since 20.19.3, and t301 +15 s, whose 2:51 is 3 s above their 2:26 to 2:48; only an
  interleaved A/B of `head_15651313f` and `head_d4f23aa43` would tell that from the sitting. t410 keeps the
  #1289 gain, 3:53. Against 20.22.0.m the round is level once t720 is counted.
- Memory equal or lower; t2000's live peak is back at the 64 GB of `fd30ef780` after 77 GB in the two
  rounds between, a figure that moves between sittings of one build.

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
the scheduling log (`doc/archive/schedule-with-lookahead-log.md`). That run used t641 with the gate enforcing 100 GB on the document's 128 GB
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
