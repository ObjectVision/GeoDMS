# Performance tests of the regression rounds, 20.22.x

*Status (2026-11-01): a measurement log of the full.py regression rounds from 20.22.0 on, per machine,
followed by the known causes of differences between versions. The rounds of 20.20.0 and 20.21.x, which
measured the deferral of commits and IntegrityChecks of #1259 that was removed again (a7127224f,
92eaa7150), moved on 2026-10-06 to
[`doc/archive/performance-test-20.20-20.21.md`](archive/performance-test-20.20-20.21.md); where a
section below says "above" of one of them (round 3 of "20.21.1 without deferral", the OVSRV05 section
of 20.20.0, the `/SH` commit e11c1ac of GeoDMS-Test), that file is meant. On OVSRV05 the release v20.24.2
(0176bebdb) gives 27 of 27 `ok` in `.c` and `.m` and 19 of 21 in `.l`, with the results of 20.24.0; the latest
round, 20.25.0 at cd3e83ec7, gave 23 of 27: the loose quadtree of #1292 makes `point_in_polygon` take another
of overlapping polygons in t060, t301, t641.2 and t2000, whose references GeoDMS-Test 6387c67 moved (27 of 27
since), and makes t060 and t301 4 to 7 % faster; the final engine code of 20.25.0, c56fe51a2, gives 27 of 27 with
the same results and times, and so does the release v20.25.0 (fec6efe90) in `.m` and `.c` (27 of 27) and `.l` (19 of
21); the release v20.26.0 (f3c45bc70) gives the same verdicts and results, and an A/B shows no performance change
against 20.25.0; the pre-release v20.27.0 (02f9fc329) gives 27 of 27 in `.m` with the results of 20.26.0, and in an
A/B its one-pass sparse `impedance_matrix` (#1327) makes t910 14 % faster and leaves the other network experiments and
every peak commit level; the local build of 20.28.0 (db151676e) gives 27 of 27 with the same results, and in an
A/B the counting in blocks of #1327 (55b161945) makes t720 24 % faster and t405.2 5 %, while the peak commit of t301
and t720 rises by about 1 GB (the 2.8 GB of that A/B was the high end of the spread); the pre-release v20.28.0
(01feccd21) gives 21 of 21 in `.l` and 27 of 27 in `.c` and `.m` with the same results, and is as fast as
db151676e in an A/B. t405.2 and t405.3, stopped after six minutes in every `.l` round from 20.23.1 on, were the WSL VM
of the account that ran them, which had no `.wslconfig` and so half the RAM, not the engine: with 48 GB for that VM
(2026-10-22) 20.26.0.l passes both, 21 of 21. t300's two levels on this machine are the first read of its XML, not
the build (see the known causes at the end). Section 8.1.x of
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

## Results (OVSRV05), 20.24.1

Run on 2026-10-14 from 13:02 to 14:24 on OVSRV05, threshold 60 as above (set by
`scratch\launch_perf_20241.cmd` and put back to 95 after it), full.py exit 0, nothing on stderr; report
header 1:19:39, 23 of 27 `ok`. Tree `C:\dev\GeoDMS` on `main` at `4c06c9c97` (version 20.24.1, pulled at
12:25), GeoDMS-Test at `6c30635`. The binaries in `bin\Release\x64` were built after the pull, from 12:32
(`Rtc.dll`) to 12:42 (`GeoDmsGuiQt.exe`), not by this session: every DLL whose sources the pull changed is
younger than it, and `prelude.dms` and `RewriteExpr.lsp` match `res\`. `testcases\run_testcases.bat` 513
cases, 0 bad, highest peak commit 0.29 GB. Kept as `C:\LocalData\GeoDMS_engine\head_4c06c9c97`. No
`20_24_1_m` folder existed beforehand; full.py ran 32 experiments with no reuse.

Running on the machine, from five-minute samples: the Chrome Remote Desktop host at about a third of a core,
the Claude desktop app at a tenth, three idle VS18 windows; together 1.7 % of the 24 logical processors.

The columns compared: 20.24.0.m, the round of 2026-10-10 from 16:42 to 19:58 on the local build `dd343fa16`
(kept as `head_dd343fa16`); 20.23.1.m, the installed release, 2026-10-08 from 16:37 to 19:54; 20.23.0.m, the
section above. All three at threshold 60, each with 27 of 27 `ok` exit codes in its own round.

### Four tests stop in the meta phase: the relation check of `combine_data` (#1293)

t010, t641.1, t641.2 and t2000 end with exit 1 within 2 to 19 s. The first error in each log comes from the
check that `c247d85cc` (#1293) gave `combine_data` when it made it an operator again: with a combine record
on its first argument, it unifies the values unit of every relation with the factor at its position
(`clc/dll/src/OperUnit.cpp`, `CombineData_GetFactors` and the `UnifyValues` loop of `CreateResult`), and
it requires one relation per factor. The configurations break both rules:

| test | first error |
|---|---|
| t010 | `Relational/.../combine_data/CityForEachYear_rel`: factor 1 `NoordHollandBigCities` of `unit_CityForEachYear` against the values unit `/domains/RegionDomain` of the relation |
| t641.1, t641.2 | `.../Uitsmeer_key/pand_grid_5m_tabel/pandAdmin_keyset` (and `pandAlloc_keyset`): factor 2, the 25 m (100 m) grid domain with its projection, against the values unit of the relation |
| t2000 | `BouwSoortBouwJaarKlasseeigendom_rel := combine_data(BouwSoortBouwJaarKlasseEigendom, BouwSoortBouwJaarKlasse_rel, Eigendom_rel)`: X is `combine(Bouwsoort, BouwJaarWoning, Eigendom)`, "a combine of 3 factors, but 2 relations were given"; the first relation is itself a `combine_data` to `combine(Bouwsoort, BouwJaarWoning)`, so the prelude function computed the same index as three relations would |

The errors after the first are its consequences: `ToedelingsMatrix` is a copy of the unit whose keyset
failed, so its `Pand_rel` is an unknown identifier, and likewise `UniekIsolatieCode/values` and
`GeschikteOptie/InstallatiePerProduct/RVb` in Hestia. No other commit between 20.24.0 and 20.24.1 touches
`OperUnit.cpp`. Rerun with `run_exp.py`, t2000 computes on `head_dd343fa16` (exit 0, 1212 s) and stops after
2 s on `head_4c06c9c97` with the same error. t010 cannot be cleared that way: on `head_dd343fa16` it stops on
`-128c`, the literal regression STX-A25 that `36deb9395` fixed on 2026-10-13. A build of `382316377`, the
parent of `c247d85cc`, would complete the proof; none was made.

### Wall time of the tests that ran, and an A/B of the two that moved

| test | 20.24.1.m (14-10) | 20.24.0.m (10-10) | 20.23.1.m (08-10) | 20.23.0.m (07-10) | against 20.24.0.m |
|---|---|---|---|---|---|
| t020 | 0:04:00 | 0:04:25 | 0:04:17 | 0:04:04 | 0.91 |
| t060 | 0:01:52 | 0:02:02 | 0:01:53 | 0:01:52 | 0.92 |
| t101 | 0:03:32 | 0:03:56 | 0:03:56 | 0:03:21 | 0.90 |
| t200 | 0:00:37 | 0:00:39 | 0:00:39 | 0:00:38 | 0.95 |
| t300 | 0:01:55 | 0:01:54 | 0:01:58 | 0:01:57 | 1.01 |
| t301 | 0:02:49 | 0:02:46 | 0:02:44 | 0:02:51 | 1.02 |
| t405.1 | 0:04:19 | 0:04:27 | 0:04:24 | 0:04:26 | 0.97 |
| t405.2 | 0:18:36 | 0:18:29 | 0:18:53 | 0:18:45 | 1.01 |
| t405.3 | 0:18:19 | 0:18:25 | 0:18:39 | 0:18:18 | 0.99 |
| t410 | 0:03:59 | 0:04:05 | 0:03:55 | 0:03:53 | 0.98 |
| t611 | 0:00:33 | 0:00:34 | 0:00:32 | 0:00:31 | 0.97 |
| t710 | 0:01:04 | 0:01:04 | 0:01:03 | 0:01:06 | 1.00 |
| t720 | 0:09:51 | 0:09:35 | 0:09:13 | 0:09:51 | 1.03 |
| t810 | 0:05:48 | 0:05:18 | 0:05:00 | 0:05:06 | 1.09 |
| t910 | 0:00:55 | 0:00:49 | 0:00:49 | 0:00:50 | 1.12 |
| t641.1, t641.2, t2000 | stopped | 0:51:40, 0:43:51, 0:17:42 | 0:52:08, 0:44:43, 0:18:20 | 0:51:01, 0:44:53, 0:18:44 | |

t810 and t910 are above all three earlier rounds, so they were rerun interleaved on `head_dd343fa16` and
`head_4c06c9c97` the same afternoon (14:49 to 15:13), A, B, A, B, threshold 60:
t810 315 and 311 s on 20.24.0 against 310 and 311 s on 20.24.1, t910 50 and 46 s against 47 and 47 s, with the
same result files. Both moved with the sitting, not with the build. Memory of the tests that ran is within the
spread of the earlier rounds (t405.2's commit peak of 47.7 GB at a PeakLiveLarge of 31 377 MB is the figure that
moves between 29.5 and 48.9 GB on one build, see above).

### Reading

- Not releasable as it stands: #1293's `combine_data` rejects three configurations of the round, the Operator
  test, RSopen and Hestia, that 20.24.0 and every earlier release compute. Two kinds: a relation whose values
  unit differs from the factor in what `UnifyValues` compares (t010's region unit, t641's projected grid
  domains), and a relation that encodes two leading factors at once (t2000). Whether the check becomes a
  warning, accepts a relation to a combine of leading factors, or the configurations change, is a decision
  outside this measurement; the commit names neither the tst unit suite nor full.py among what it ran.
- No performance effect where the round could measure: every test that ran is within the drift between
  sittings of the earlier rounds, and the two above it are level with 20.24.0 in an A/B. t641 and t2000, the
  largest models, are not measured by this round.

## Results (OVSRV05), 20.24.2

Run from 2026-10-14 21:16 to 2026-10-15 00:30 on OVSRV05, threshold 60 as above (set by
`scratch\launch_perf_20242.cmd` and put back to 95 after it), full.py exit 0, nothing on stderr; report
header 3:10:49, 27 of 27 `ok`. Tree `C:\dev\GeoDMS` on `main` at `806e7dd61` (version 20.24.2: the bump
0860841e4, #1309 `e075b670c`, #1310 `12925994b` and the eight #1311 commits on top of 20.24.1), GeoDMS-Test at
`2cd9891`, which has the t010 change `8765a6e` (#1293) and t641's keysets over `AdminDomain/id_key` (#1310).
Built with the VS18 msbuild, Release x64 of `all22.sln`, 21:05:19 to 21:15:45, exit 0, no error and no C
warning, every engine DLL rebuilt (`Rtc.dll` 21:05:47 to `GeoDmsGuiQt.exe` 21:15:40);
`testcases\run_testcases.bat` 518 cases, 0 bad. Kept as `C:\LocalData\GeoDMS_engine\head_806e7dd61`. No
`20_24_2_m` folder existed beforehand; full.py ran 32 experiments with no reuse.

Running on the machine, from five-minute samples: the Chrome Remote Desktop host at about a sixth of a core,
the Claude desktop app, three idle VS18 windows; together 0.8 % of the 24 logical processors. Free physical
memory fell to 0.1 GB during t641.1.

### Correct again

27 of 27 `ok`, and every result file of the round is byte for byte the one of 20.24.0.m, except t010's
`operator_coverage.txt` and `operator_groups.txt`, which follow from its changed Relational test and the new
operators of #1293. The four tests of the section above compute again: t010 through GeoDMS-Test `8765a6e`,
which gives the relations in factor order; t2000 through `12925994b`, in which a relation to a combine of
leading factors stands for those factors; t641.1 and t641.2 through GeoDMS-Test `2cd9891`.

### Wall time and Highest CommitCharge

| test | 20.24.2.m (14-10) | 20.23.0.m (07-10) | 20.24.0.m (10-10) | 20.24.1.m (14-10) | against 20.23.0.m |
|---|---|---|---|---|---|
| t020 | 0:04:02, 2.4 GB | 0:04:04, 2.9 GB | 0:04:25, 2.4 GB | 0:04:00, 2.4 GB | 0.99 |
| t060 | 0:01:52, 8.2 GB | 0:01:52, 8.2 GB | 0:02:02, 6.9 GB | 0:01:52, 8.0 GB | 1.00 |
| t101 | 0:03:24, 1.6 GB | 0:03:21, 1.6 GB | 0:03:56, 1.6 GB | 0:03:32, 1.6 GB | 1.01 |
| t200 | 0:00:36, 2.8 GB | 0:00:38, 2.8 GB | 0:00:39, 2.8 GB | 0:00:37, 2.7 GB | 0.95 |
| t300 | 0:01:53, 9.6 GB | 0:01:57, 6.5 GB | 0:01:54, 9.6 GB | 0:01:55, 8.6 GB | 0.97 |
| t301 | 0:02:40, 15.9 GB | 0:02:51, 20.3 GB | 0:02:46, 19.9 GB | 0:02:49, 17.1 GB | 0.94 |
| t405.1 | 0:04:21, 8.2 GB | 0:04:26, 8.2 GB | 0:04:27, 8.2 GB | 0:04:19, 8.2 GB | 0.98 |
| t405.2 | 0:17:54, 27.9 GB | 0:18:45, 29.0 GB | 0:18:29, 29.0 GB | 0:18:36, 46.6 GB | 0.95 |
| t405.3 | 0:17:55, 28.4 GB | 0:18:18, 29.1 GB | 0:18:25, 29.0 GB | 0:18:19, 29.1 GB | 0.98 |
| t410 | 0:04:14, 1.1 GB | 0:03:53, 1.1 GB | 0:04:05, 1.2 GB | 0:03:59, 1.1 GB | 1.09 |
| t611 | 0:00:32, 4.0 GB | 0:00:31, 3.8 GB | 0:00:34, 3.8 GB | 0:00:33, 3.7 GB | 1.03 |
| t641.1 | 0:51:03, 137.9 GB | 0:51:01, 138.0 GB | 0:51:40, 138.0 GB | stopped | 1.00 |
| t641.2 | 0:44:42, 165.7 GB | 0:44:53, 165.5 GB | 0:43:51, 165.7 GB | stopped | 1.00 |
| t710 | 0:01:04, 6.7 GB | 0:01:06, 5.7 GB | 0:01:04, 7.4 GB | 0:01:04, 5.7 GB | 0.97 |
| t720 | 0:09:41, 20.4 GB | 0:09:51, 20.2 GB | 0:09:35, 20.6 GB | 0:09:51, 16.8 GB | 0.98 |
| t810 | 0:04:58, 26.6 GB | 0:05:06, 26.7 GB | 0:05:18, 25.8 GB | 0:05:48, 26.3 GB | 0.97 |
| t910 | 0:00:51, 4.3 GB | 0:00:50, 3.2 GB | 0:00:49, 3.1 GB | 0:00:55, 3.9 GB | 1.02 |
| t1640 | 6 s, 95 MB | 6 s, 94 MB | 6 s, 94 MB | 5 s, 95 MB | 1.00 |
| t1642 | 13 s, 87 MB | 12 s, 89 MB | 13 s, 87 MB | 12 s, 90 MB | 1.08 |
| t2000 | 0:17:53, 61.6 GB | 0:18:44, 61.7 GB | 0:17:42, 61.9 GB | stopped | 0.95 |

Summed over the rows: 190.3 min, against 192.9 for 20.23.0.m and 192.5 for 20.24.0.m.

t410 is the one row above the drift of 20.24.0 and 20.24.1, so it was rerun interleaved on `head_4c06c9c97`
(20.24.1) and `head_806e7dd61` the same night, 00:32 to 00:48, A, B, A, B, threshold 60: 236 and 245 s on
20.24.1 against 242 and 243 s on 20.24.2, the same result file every time. The two builds are level.

Memory: Highest CommitCharge equal or lower than 20.23.0.m on every test except t910 (4.3 GB against 3.2) and
t710 (6.7 against 5.7, 7.4 in 20.24.0.m). t910's figure moves between 3.7 and 4.4 GB on one build: in the A/B
of the section above it was 4.0 and 3.7 GB on 20.24.0 and 3.8 and 4.4 GB on 20.24.1. t405.2's 46.6 GB in
20.24.1.m is the commit peak that moves between sittings at the same PeakLiveLarge, see 20.22.1 above.

### Reading

- Correct: 27 of 27, with the results of 20.24.0, so neither the combine record of #1293, nor the generated
  tiles of #1309, nor the lock fixes of #1311 change a result that the round checks.
- No performance regression: every experiment is within the drift between sittings of the earlier rounds, the
  one above it, t410, is level with 20.24.1 in an A/B, and t2000, t405.2 and t405.3 are 4 to 5 % below
  20.23.0.m, which a single round does not attribute to a build. The experiments that #1309 named, t641.1,
  t641.2, t2000, t405.2, t405.3, t710, t720, t1640 and t1642, are each level with 20.23.0.m or faster, within
  a second for the value-info tests.

### The setup GeoDms20.24.2.m against the local build

The `.m` setup of 20.24.2 (`distr\GeoDms20.24.2.m-Setup-x64.exe` of 2026-10-14 23:21, signed by Object Vision
B.V., SHA256 `C8284A49...B0F4`) was installed silently on 2026-10-15 to
`C:\Program Files\ObjectVision\GeoDms20.24.2.m`, and full.py ran on it with `-version 20.24.2.m` from 03:29
to 06:38, threshold 60, exit 0, 32 experiments with no reuse; report header 3:06:40, 27 of 27 `ok`. The round
of the local build went to `_Archive\20_24_2_m_local_806e7dd61_20261014` beforehand (its intermediates to
`C:\LocalData\runs\_aside_20_24_2_m_local_806e7dd61_20261014`), so the `20_24_2_m` column is now the setup's.
`testcases\run_testcases.bat` with the installed `GeoDmsRun.exe`: 518 cases, 0 bad.

The setup was built in `C:\dev\GeoDMS_2026`, the local build in `C:\dev\GeoDMS` (82 occurrences in `Rtc.dll`).
Of the files the two have in common, 687 are identical; the 58 executables that differ, the six engine DLLs,
the Python modules, the two exe's and the vcpkg libraries alike, have `.text` sections of exactly the same size
(by `dumpbin /headers`), and only their `.rdata` grows, by the longer source path; the 136 text files that
differ do so in line endings only. The binaries show no code change against `806e7dd61`; a change that keeps
every code section at its size is not excluded by this.

Every `.json`, `.txt` and `.tif` result file of the setup's round is byte for byte the one of the local round;
the `.xml` results differ in `GeoDmsVersion`, `SessionStartTime` and `CurrentTime` only (the local build stamps
its version `[2026-09-14]`, the setup `[2026-10-14]`).

| test | setup (15-10) | local `806e7dd61` (14-10) | setup / local |
|---|---|---|---|
| t020 | 0:04:03, 1.9 GB | 0:04:02, 2.4 GB | 1.00 |
| t060 | 0:01:53, 7.3 GB | 0:01:52, 8.2 GB | 1.01 |
| t101 | 0:03:17, 1.6 GB | 0:03:24, 1.6 GB | 0.97 |
| t300 | 0:01:55, 9.6 GB | 0:01:53, 9.6 GB | 1.02 |
| t301 | 0:02:41, 18.4 GB | 0:02:40, 15.9 GB | 1.01 |
| t405.1 | 0:04:23, 10.0 GB | 0:04:21, 8.2 GB | 1.01 |
| t405.2 | 0:17:33, 28.9 GB | 0:17:54, 27.9 GB | 0.98 |
| t405.3 | 0:17:24, 29.0 GB | 0:17:55, 28.4 GB | 0.97 |
| t410 | 0:03:50, 1.3 GB | 0:04:14, 1.1 GB | 0.91 |
| t641.1 | 0:50:20, 137.7 GB | 0:51:03, 137.9 GB | 0.99 |
| t641.2 | 0:43:34, 172.7 GB | 0:44:42, 165.7 GB | 0.97 |
| t710 | 0:01:03, 7.4 GB | 0:01:04, 6.7 GB | 0.98 |
| t720 | 0:09:21, 23.6 GB | 0:09:41, 20.4 GB | 0.97 |
| t810 | 0:05:02, 25.7 GB | 0:04:58, 26.6 GB | 1.01 |
| t910 | 0:00:48, 2.6 GB | 0:00:51, 4.3 GB | 0.94 |
| t2000 | 0:17:14, 67.0 GB | 0:17:53, 61.6 GB | 0.96 |

Wall time and Highest CommitCharge as above; summed over the rows of the earlier table 186.3 min against 190.3.
t1640 and t1642 6 and 13 s in both. The commit peaks that rose, t641.2 by 7 GB, t2000 by 5.4, t720 by 3.2, come
at the same PeakLiveLarge (168 907 MB for t641.2 in both, 136 936 for t641.1, 9296 for t405.1; t2000 63 876
against 61 708 to 64 459 in the earlier rounds), and lie within what the process commit of those tests has
shown between sittings of one build (t641.2 169.5 to 180.7 GB since 20.22.0).

Reading: the setup computes what the local build computes, in the same time (every row within 9 %, the larger
ones within 4 %, none slower than the drift), with the same live memory. No A/B was needed.

## Results (OVSRV05), the release v20.24.2 (tag `0176bebdb`) in `.l`, `.c` and `.m`

The tag `v20.24.2` points at `0176bebdb`, two commits past the `806e7dd61` of the rounds above: `d41ca60ff`
(braces and default member initializers for the GCC 13 warnings) and `0176bebdb` (a `bit_reference` no longer
reads and writes the three bytes after the lone 1-byte Bool accumulator of `any` and `all`). The `.m` setup of
the release has other `.text` sizes than `806e7dd61` in five engine DLLs (`Clc`, `Geo`, `Rtc`, `Shv`, `Stg`), so
it is not the build tested above and got a round of its own. The `distr` folder had only `.c` and `.m` setups
of before the tag and no `.l`, so the assets of the GitHub pre-release were installed; their SHA256 match the
release's digests and both Windows setups carry a valid signature. `.l` went into mtahi's WSL Ubuntu 26.04 as
`/opt/ObjectVision/GeoDms20.24.2.l`, in place of 20.23.1.l; `.c` and `.m` into `C:\Program Files\ObjectVision`.
The `20_24_2_m` column of the setup from `distr` moved to `_Archive\20_24_2_m_setup_distr_20261015` (its
intermediates to `C:\LocalData\runs\_aside_20_24_2_m_setup_distr_20261015`) first.

`scratch\launch_perf_20242_release.cmd` ran the three rounds one after another, threshold 60, put back to 95
after them: `.l` 2026-10-15 17:36 to 18:38, `.c` 18:38 to 21:43, `.m` 21:43 to 2026-10-16 00:55. Each full.py
exit 0, nothing on stderr, no reuse. The 20.23.1 releases, the references of `.l` and `.c`, ran on 2026-10-08
in the order `.m` (16:37 to 19:54), `.c` (19:54 to 22:58), `.l` (23:00 to 23:58), also threshold 60.

| flavour | report header | verdict |
|---|---|---|
| `.l` | 0:55:12 | 19 of 21 `ok`: t405.2 and t405.3 stop with exit 15 after 6 minutes, as in 20.23.1.l; t641, t2000 and the GUI tests are not run on `.l` |
| `.c` | 3:02:31 | 27 of 27 `ok` |
| `.m` | 3:09:12 | 27 of 27 `ok` |

`.l` runs with full.py's defaults for Linux: WSL has 31 GB here, so `RunAllLinuxTests` stays off and t641.x
(which asks for a host of at least 192 GB), t2000 (96 GB) and the GUI tests are skipped; t405.2 and t405.3,
which commit about 29 GB on Windows, do not fit either.

Results: every `.json`, `.txt` and `.tif` result of the `.m` round is byte for byte the one of the local round
of `806e7dd61`, so the two commits do not change a checked result. `.c` gives the results of `.m`. `.l` does too,
except where 20.23.1.l already differed from `.m`: `t020_polygons.result.json` prints one metric with 17 instead
of 15 significant digits (466169.37699179107), and `t710_2UP_indicator_results_zonder_CalCache.result.json`. In
`.c` and `.l` t010's `operator_groups.txt` lists the operators of `.m` in another order.

| test | `.m` release | `.m` local `806e7dd61` | `.c` release | `.c` 20.23.1 | `.l` release | `.l` 20.23.1 |
|---|---|---|---|---|---|---|
| t020 | 0:03:46, 1.9 GB | 0:04:02, 2.4 GB | 0:03:44, 2.0 GB | 0:03:38, 1.9 GB | 0:03:30, 6.4 GB | 0:03:31, 6.6 GB |
| t060 | 0:01:50, 6.8 GB | 0:01:52, 8.2 GB | 0:01:51, 6.8 GB | 0:01:49, 6.8 GB | 0:02:56, 10.6 GB | 0:02:49, 9.6 GB |
| t101 | 0:03:14, 1.6 GB | 0:03:24, 1.6 GB | 0:03:10, 1.6 GB | 0:03:11, 1.6 GB | 0:03:38, 1.7 GB | 0:03:14, 1.7 GB |
| t200 | 0:00:36, 2.9 GB | 0:00:36, 2.8 GB | 0:00:30, 3.2 GB | 0:00:36, 2.7 GB | 0:01:00, 1.7 GB | 0:00:55, 2.1 GB |
| t300 | 0:01:03, 9.6 GB | 0:01:53, 9.6 GB | 0:00:55, 10.5 GB | 0:01:04, 7.6 GB | 0:02:12, 7.1 GB | 0:01:16, 8.7 GB |
| t301 | 0:02:32, 13.6 GB | 0:02:40, 15.9 GB | 0:02:35, 16.5 GB | 0:02:28, 13.0 GB | 0:01:37, 20.6 GB | 0:01:36, 19.5 GB |
| t405.1 | 0:04:22, 10.0 GB | 0:04:21, 8.2 GB | 0:03:23, 10.0 GB | 0:04:18, 10.0 GB | 0:04:42, 10.4 GB | 0:04:22, 10.6 GB |
| t405.2 | 0:17:37, 29.1 GB | 0:17:54, 27.9 GB | 0:17:32, 29.0 GB | 0:17:48, 29.2 GB | stopped | stopped |
| t405.3 | 0:17:48, 29.0 GB | 0:17:55, 28.4 GB | 0:17:33, 29.0 GB | 0:18:04, 29.0 GB | stopped | stopped |
| t410 | 0:04:04, 1.2 GB | 0:04:14, 1.1 GB | 0:03:47, 0.9 GB | 0:03:52, 1.1 GB | 0:04:15, 2.2 GB | 0:04:11, 2.3 GB |
| t611 | 0:00:31, 3.9 GB | 0:00:32, 4.0 GB | 0:00:30, 4.0 GB | 0:00:31, 3.9 GB | 0:00:32, 4.5 GB | 0:00:32, 3.5 GB |
| t641.1 | 0:49:31, 142.4 GB | 0:51:03, 137.9 GB | 0:49:41, 141.9 GB | 0:48:15, 138.7 GB | not run | not run |
| t641.2 | 0:45:46, 165.8 GB | 0:44:42, 165.7 GB | 0:42:57, 166.2 GB | 0:42:10, 173.5 GB | not run | not run |
| t710 | 0:01:05, 5.7 GB | 0:01:04, 6.7 GB | 0:01:04, 7.1 GB | 0:01:02, 6.5 GB | 0:01:06, 7.9 GB | 0:01:05, 8.2 GB |
| t720 | 0:09:41, 20.4 GB | 0:09:41, 20.4 GB | 0:09:20, 19.4 GB | 0:09:16, 20.9 GB | 0:09:25, 20.3 GB | 0:09:18, 26.7 GB |
| t810 | 0:05:33, 26.3 GB | 0:04:58, 26.6 GB | 0:05:03, 25.9 GB | 0:05:31, 25.9 GB | 0:06:48, 27.4 GB | 0:06:26, 26.9 GB |
| t910 | 0:00:50, 3.2 GB | 0:00:51, 4.3 GB | 0:00:48, 3.1 GB | 0:00:49, 2.7 GB | 0:00:58, 3.5 GB | 0:00:55, 3.1 GB |
| t2000 | 0:18:08, 67.3 GB | 0:17:53, 61.6 GB | 0:16:53, 60.8 GB | 0:17:52, 70.9 GB | not run | not run |

Wall time and Highest CommitCharge as above (for `.l` what the Linux build logs under that name). Summed over the rows that
ran: `.m` 188.8 min against 190.3 for the local round, `.c` 182.1 against 183.0 for 20.23.1.c. t1640 and t1642
5 and 12 s in `.m` and `.c`.

t300 is the one row far from its reference: 1:03 in `.m` and 0:55 in `.c` against 1:53 for the local round, but
2:12 in `.l` against 1:16. That is not the build. On 2026-10-16 from 01:11 to 01:17, interleaved, with t300's XML
read once beforehand, t300 took 58 and 59 s on `head_806e7dd61`, 59 and 58 s on the release `.m` and 58 and 58 s
on `head_cd3e83ec7` (20.25.0), each with 8 of 8 spot checks equal. It is the first read of its 5.7 GB of XML; see
"t300 and the first read of its XML on OVSRV05" under the known causes below.

### Reading

- Correct: `.m` and `.c` 27 of 27 with the results of the local round, `.l` 19 of 21 with the limits and the
  known differences of 20.23.1.l. The two commits after `806e7dd61` change no checked result.
- No performance regression in `.m` and `.c`: `.m` lies within the drift of the local round and the `distr`
  setup (188.8 min against 190.3 and 186.3 for the same code), `.c` is level with 20.23.1.c.
- `.l` is slower than 20.23.1.l: summed over the twelve rows that both ran to the end, t300 left out, 40.5 against
  38.9 min (+4 %), with t101 +24 s (12 %), t810 +22 s, t405.1 +20 s and t200 +5 s. 20.23.1.l was the third round
  of its sitting and 20.24.2.l the first, which accounts for t300 (+56 s) and perhaps for more of the text
  inputs; no A/B of the two `.l` builds was run.

## Results (OVSRV05), 20.25.0: the loose quadtree of #1292

Run from 2026-10-16 01:17 to 04:31 on OVSRV05, threshold 60 (set by `scratch\launch_perf_20250.cmd` and put
back to 95 after it), full.py exit 0, nothing on stderr; report header 3:12:21, 23 of 27 `ok`. Tree
`C:\dev\GeoDMS` on `main` at `cd3e83ec7`: the release v20.24.2 (`0176bebdb`) with the release notes
`fc6414fed`, the #1292 commit `7c4000cdc` (the spatial index keeps boxes, polygons and arcs in a loose
quadtree), its continuations entry `674a76876` and the version bump `c2b0790cb`, merged with `905312173` of this
file. In code it differs from the release only by `7c4000cdc` (`geo/dll/src/geom/SpatialIndex.h`,
`NeighbourIter.h`) and the version number. GeoDMS-Test at `2cd9891`, as above. Built with the VS18 msbuild,
00:59:02 to 01:09:58, exit 0, no error and no C warning, every engine DLL rebuilt (`Rtc.dll` 00:59:42 to
`GeoDmsGuiQt.exe` 01:09:53); `testcases\run_testcases.bat` 519 cases, 0 bad, `geo_spatial_index_loose` among
them. Kept as `C:\LocalData\GeoDMS_engine\head_cd3e83ec7`. No `20_25_0_m` folder existed; full.py ran 32
experiments with no reuse. The t300 A/B of the section above ran right before it and read t300's XML, so this
round's t300 is the fast one.

Running on the machine, from 39 five-minute samples: the Chrome Remote Desktop host 1 761 s of CPU (about 15 % of
a core), the Claude desktop app 576 s, nothing else above 70 s.

### Four red rows: `point_in_polygon` takes another of overlapping polygons

The four red rows are t060 (its file comparison, 99), t301, t641.2 and t2000; every other `.json`, `.txt` and
`.tif` result of the round is byte for byte the one of the release `.m`. They are the four rows that the 20.22.1 section "Five red
rows follow the new rule, four still follow the tree" above traced to the visiting order of the tree, and the
commit message of `7c4000cdc` named the risk: the order in which the index yields objects changes, and
`point_in_polygon` returns the first polygon found that contains the point.

| test | release 20.24.2.m | 20.25.0.m |
|---|---|---|
| t301 panden that differ from the reference | 0 | 43 495 of 10 644 899 |
| t060 vbo with another `pand_id_geom` | | 11 160 of 731 571; 5 863 panden with another `woningtype` |
| t2000 hWP_asl StartJaar, R5_2025 | 49 997, 92 671 | 49 998, 92 855 |
| t2000 eWP_asl R5_2025 | 1 442 096 | 1 438 874 |
| t2000 gebiedsoptie_nieuw R4_2024, R5_2025 | 4 111, 8 725 | 4 316, 8 930 |
| t641.2 2050 Woningen, Banen | 9 590 281, 9 893 214 | 9 590 344, 9 893 416 |

t060 was traced on the geopackages of the two rounds (`C:\LocalData\runs\<label>\regression\BAG20`), with a
point-in-polygon test of its own on their geometry (`scratch\perf_ovsrv05\pip_check.py`): of the 11 160
verblijfsobjecten whose pand changed, 11 135 lie in both the old and the new pand, and the other 25 lie 1.1 to
20.6 mm from the boundary of the pand that the test leaves out, within the float32 resolution of these RD
coordinates (steps of 1.6 cm in x and 3.1 cm in y). 195 verblijfsobjecten have no pand in either round. No point lost or
gained a pand; each took another of the panden it lies in. t301, t2000 and t641.2 were not traced to an item;
t301 reads the same kind of BAG panden. The largest relative changes are in t2000: gebiedsoptie_nieuw +5.0 % in
R4_2024, the heat-pump counts within 0.25 %.

### Wall time and Highest CommitCharge

| test | 20.25.0.m (16-10) | release 20.24.2.m (15-10) | local `806e7dd61` (14-10) | `distr` setup (15-10) | against the release |
|---|---|---|---|---|---|
| t020 | 0:04:07, 2.5 GB | 0:03:46, 1.9 GB | 0:04:02, 2.4 GB | 0:04:03, 1.9 GB | 1.09 |
| t060 | 0:01:57, 6.9 GB | 0:01:50, 6.8 GB | 0:01:52, 8.2 GB | 0:01:53, 7.3 GB | 1.06 |
| t101 | 0:03:29, 1.6 GB | 0:03:14, 1.6 GB | 0:03:24, 1.6 GB | 0:03:17, 1.6 GB | 1.08 |
| t200 | 0:00:38, 2.9 GB | 0:00:36, 2.9 GB | 0:00:36, 2.8 GB | 0:00:37, 2.7 GB | 1.06 |
| t300 | 0:00:59, 10.1 GB | 0:01:03, 9.6 GB | 0:01:53, 9.6 GB | 0:01:55, 9.6 GB | 0.94 |
| t301 | 0:02:36, 18.2 GB | 0:02:32, 13.6 GB | 0:02:40, 15.9 GB | 0:02:41, 18.4 GB | 1.03 |
| t405.1 | 0:04:34, 9.6 GB | 0:04:22, 10.0 GB | 0:04:21, 8.2 GB | 0:04:23, 10.0 GB | 1.05 |
| t405.2 | 0:18:05, 29.1 GB | 0:17:37, 29.1 GB | 0:17:54, 27.9 GB | 0:17:33, 28.9 GB | 1.03 |
| t405.3 | 0:18:32, 29.4 GB | 0:17:48, 29.0 GB | 0:17:55, 28.4 GB | 0:17:24, 29.0 GB | 1.04 |
| t410 | 0:04:05, 1.2 GB | 0:04:04, 1.2 GB | 0:04:14, 1.1 GB | 0:03:50, 1.3 GB | 1.00 |
| t611 | 0:00:32, 3.9 GB | 0:00:31, 3.9 GB | 0:00:32, 4.0 GB | 0:00:31, 4.0 GB | 1.03 |
| t641.1 | 0:51:23, 137.6 GB | 0:49:31, 142.4 GB | 0:51:03, 137.9 GB | 0:50:20, 137.7 GB | 1.04 |
| t641.2 | 0:45:20, 171.7 GB | 0:45:46, 165.8 GB | 0:44:42, 165.7 GB | 0:43:34, 172.7 GB | 0.99 |
| t710 | 0:01:06, 5.7 GB | 0:01:05, 5.7 GB | 0:01:04, 6.7 GB | 0:01:03, 7.4 GB | 1.02 |
| t720 | 0:09:39, 20.4 GB | 0:09:41, 20.4 GB | 0:09:41, 20.4 GB | 0:09:21, 23.6 GB | 1.00 |
| t810 | 0:05:35, 26.2 GB | 0:05:33, 26.3 GB | 0:04:58, 26.6 GB | 0:05:02, 25.7 GB | 1.01 |
| t910 | 0:00:52, 2.8 GB | 0:00:50, 3.2 GB | 0:00:51, 4.3 GB | 0:00:48, 2.6 GB | 1.04 |
| t2000 | 0:17:38, 61.9 GB | 0:18:08, 67.3 GB | 0:17:53, 61.6 GB | 0:17:14, 67.0 GB | 0.97 |

Summed over the rows: 191.9 min, against 188.8 for the release, 190.3 for the local round and 186.3 for the
`distr` setup, the last three of the same code. t1640 and t1642 4 and 12 s. Peak live memory unchanged:
`PeakLiveLarge` 136 936 MB for t641.1 and 168 907 MB for t641.2, as in the three rounds of 20.24.2; t2000 63 098
MB, inside their 61 708 to 65 153.

### Interleaved A/B: the release against 20.25.0

`scratch\perf_ovsrv05\ab_20250.cmd`, 2026-10-16 04:41 to 06:04, threshold 60: X = the installed release
20.24.2.m (kept as `C:\LocalData\GeoDMS_engine\release_0176bebdb_m`), Y = `head_cd3e83ec7`, in the order X, Y, X,
Y, each pass the seven experiments below. Their source files were read once beforehand
(`scratch\perf_ovsrv05\warm_inputs.py`, 13 912 files, 36.3 GiB, 109 s). Seconds per run:

| test | X1 | Y1 | X2 | Y2 | Y / X |
|---|---|---|---|---|---|
| t020 | 239 | 242 | 238 | 240 | 1.01 |
| t060 | 111 | 101 | 109 | 103 | 0.93 |
| t101 | 205 | 203 | 209 | 205 | 0.99 |
| t200 | 32 | 32 | 32 | 32 | 1.00 |
| t301 | 162 | 157 | 160 | 152 | 0.96 |
| t405.1 | 266 | 215 | 214 | 215 | 1.00, X1 left out |
| t410 | 243 | 242 | 243 | 243 | 1.00 |

Each run gave the results of its build's round, byte for byte. t405.1's 266 s in X1 is a first run: X2 takes
214 s on the same build.

### Reading

- Correct as far as the round can tell: the four red rows are `point_in_polygon`'s unspecified choice among
  overlapping polygons, for t060 checked point by point. Like 20.22.1, they got references that record the new
  order, by decision: GeoDMS-Test `6387c67` (not pushed) adds a 20.25.0 epoch in `references.json` for t2000 and
  t641.2, and `TestReferenceFiles\t301\type_woonpand_rel_ok_v20250.fss` (recorded from `cd3e83ec7`) and
  `TestReferenceFiles\t060\snapshot_Utrecht_20210701_v20250.gpkg` (the gpkg of the round), chosen from 20.25.0
  on; the two stores exist on OVSRV05 only. t301 gives 0 differing panden on `cd3e83ec7` against the new store
  and on the release 20.24.2.m against the `_v20221` one. t060 and t301 were then rerun inside the round
  (`full.py -resume -tests`, 14:55 to 15:01 on 16-10), which regenerated the report: 27 of 27 `ok`. The t060 and
  t301 logs of the column are now those of the rerun (1:47 and 2:43); the tables above give the round's own.
- #1292 makes the `point_in_polygon` experiments faster, t060 by 7 % and t301 by 4 %, and slows down nothing:
  `connect` (t101, t410), the grid and polygon experiments and t405.1 are level. The +5 to +9 % of t020, t060,
  t101, t200 and t405.1 between the rounds is the drift between sittings; in the A/B each is level or faster.
- The cases where `7c4000cdc` measured its large gains, `box_connectivity` of dense unit squares (30 to 40 s to
  2.5 s) and `point_in_ranked_polygon` (6.1 to 1.5 s), dominate no experiment of the round, so the round does not
  show them.

## Results (OVSRV05), 20.25.0 at `c56fe51a2`: the final engine code

Run from 2026-10-16 20:56 to 2026-10-17 00:11 on OVSRV05, threshold 60 (set by `scratch\launch_perf_20250b.cmd`
and put back to 95 after it), full.py exit 0, nothing on stderr; report header 3:11:52, 27 of 27 `ok`. Tree
`C:\dev\GeoDMS` on `main` at `c56fe51a2`: the round above (`cd3e83ec7`) with this file's `1f4455e02` and
`c953d8949` and the engine commits #1313 `b17d330f1` (a unit made by combine keeps its factors in interest while it
is in interest), #1314 `7b4e2b9dc` (`mid` and `centroid_or_mid` of a polygon with a null vertex), #1315 `8487e5f39`,
#1316 `374149dce`, #1317 `22f424664` and #1318 `13b875271` and `48f7e71ad` (MMD read holders, files of `const()`
and `id()` attributes, the dictionary across write sessions, units outside a store). GeoDMS-Test at `6387c67`, with
the 20.25.0 references of the section above. Built with the VS18 msbuild, 20:44:26 to 20:54:58, exit 0, no error
and no C warning, every engine DLL rebuilt (`Rtc.dll` 20:45:00 to `GeoDmsGuiQt.exe` 20:54:54);
`testcases\run_testcases.bat` 529 cases, 0 bad. Kept as `C:\LocalData\GeoDMS_engine\head_c56fe51a2`. The column of
the round above went to `_Archive\20_25_0_m_cd3e83ec7_20261016` with its report, its intermediates to
`C:\LocalData\runs\_aside_20_25_0_m_cd3e83ec7_20261016`; full.py ran 32 experiments with no reuse. t300's XML was
read once beforehand (`warm_inputs.py`), as the A/B before the round above had done: 500 files, 5.6 GiB in 57 s,
the first-read rate of the known cause below, a third time.

Running on the machine, from 39 five-minute samples: the Claude desktop app 128 s of CPU, its web view 73 s,
nothing else above 30 s.

### The same results

Every `.json`, `.txt` and `.tif` result of the round is byte for byte the one of the round above, t010, t641.1,
t641.2 and t2000 included; the t060 gpkg has the tables of the `_v20250` reference, column for column, and differs in
metadata bytes only. #1313 to #1318 change no result that the round checks.

### Wall time and Highest CommitCharge

| test | `c56fe51a2` (16-10) | `cd3e83ec7` (16-10) | release 20.24.2.m (15-10) | against `cd3e83ec7` |
|---|---|---|---|---|
| t020 | 0:04:00, 2.5 GB | 0:04:07, 2.5 GB | 0:03:46, 1.9 GB | 0.97 |
| t060 | 0:01:41, 7.4 GB | 0:01:57, 6.9 GB | 0:01:50, 6.8 GB | 0.86 |
| t101 | 0:03:11, 1.6 GB | 0:03:29, 1.6 GB | 0:03:14, 1.6 GB | 0.91 |
| t200 | 0:00:37, 2.7 GB | 0:00:38, 2.9 GB | 0:00:36, 2.9 GB | 0.97 |
| t300 | 0:00:58, 10.4 GB | 0:00:59, 10.1 GB | 0:01:03, 9.6 GB | 0.98 |
| t301 | 0:02:44, 17.8 GB | 0:02:36, 18.2 GB | 0:02:32, 13.6 GB | 1.05 |
| t405.1 | 0:04:26, 8.2 GB | 0:04:34, 9.6 GB | 0:04:22, 10.0 GB | 0.97 |
| t405.2 | 0:18:18, 36.2 GB | 0:18:05, 29.1 GB | 0:17:37, 29.1 GB | 1.01 |
| t405.3 | 0:18:17, 29.2 GB | 0:18:32, 29.4 GB | 0:17:48, 29.0 GB | 0.99 |
| t410 | 0:03:55, 1.3 GB | 0:04:05, 1.2 GB | 0:04:04, 1.2 GB | 0.96 |
| t611 | 0:00:31, 4.1 GB | 0:00:32, 3.9 GB | 0:00:31, 3.9 GB | 0.97 |
| t641.1 | 0:51:22, 139.4 GB | 0:51:23, 137.6 GB | 0:49:31, 142.4 GB | 1.00 |
| t641.2 | 0:46:06, 171.4 GB | 0:45:20, 171.7 GB | 0:45:46, 165.8 GB | 1.02 |
| t710 | 0:01:03, 6.2 GB | 0:01:06, 5.7 GB | 0:01:05, 5.7 GB | 0.95 |
| t720 | 0:09:17, 20.7 GB | 0:09:39, 20.4 GB | 0:09:41, 20.4 GB | 0.96 |
| t810 | 0:05:19, 25.9 GB | 0:05:35, 26.2 GB | 0:05:33, 26.3 GB | 0.95 |
| t910 | 0:00:49, 2.8 GB | 0:00:52, 2.8 GB | 0:00:50, 3.2 GB | 0.94 |
| t2000 | 0:18:06, 76.3 GB | 0:17:38, 61.9 GB | 0:18:08, 67.3 GB | 1.03 |

The t060 and t301 times of `cd3e83ec7` are those of its round, not of the rerun of the section above. Summed over the
rows: 191.5 min against 191.9. t1640 and t1642 4 and 12 s. `PeakLiveLarge` 136 936 MB for t641.1 and 168 907 MB for
t641.2, as in every round of 20.24.2 and 20.25.0. t2000 has 77 224 MB live against 63 098 in the round above.

### t2000's live peak: an interleaved A/B

#1313 keeps the factors of a combine in interest while the combine is, which could keep more data alive, and t2000
uses `combine_data`. `scratch\perf_ovsrv05\ab_20250c_t2000.cmd` ran t2000 on `head_cd3e83ec7` (X) and
`head_c56fe51a2` (Y), X, Y, X, Y, 2026-10-17 00:14 to 01:25, threshold 60, its source files read once beforehand:

| run | seconds | Highest CommitCharge | `PeakLiveLarge` |
|---|---|---|---|
| X1 | 1082 | 73 727 MB | 74 678 MB |
| Y1 | 1051 | 74 010 MB | 72 601 MB |
| X2 | 1064 | 62 759 MB | 70 972 MB |
| Y2 | 1068 | 78 336 MB | 74 271 MB |

Every run gave the result of the round. The two builds are level in time and in live memory: the build without
#1313 reaches the high level as well. t2000's live peak has two levels across the rounds of this machine, 61.7 to
65.2 GB (20.23.0, 20.24.0, the three rounds of 20.24.2, `cd3e83ec7`) and 76.7 to 77.3 GB (20.22.1 at `1315b0357` and
`15651313f`, 20.23.1.m, this round), with 69.6 GB for 20.22.0 and 71 to 75 GB in the runs above; a round's figure for
t2000 says which level that run reached, not which build ran.

### Reading

- Correct: 27 of 27, with the results of `cd3e83ec7`, so the 20.25.0 references of GeoDMS-Test `6387c67` hold for the
  final code.
- No performance change: every experiment level or faster than the round above, within its drift, and t2000 level
  with `cd3e83ec7` in an A/B, in time and in live memory. Not measured: the tst unit suite.

## Results (OVSRV05), the release v20.25.0 (tag `fec6efe90`) in `.l`, `.c` and `.m`

The GitHub pre-release `v20.25.0` (published 2026-10-20 13:42 UTC) is tagged at `fec6efe90`, whose engine code is
that of `c56fe51a2` above; the five commits after it are the release notes, plan documents and a testcase. The
`.m`, `.c` and `.l` assets were downloaded with `gh` into `C:\LocalData\GeoDMS_releases\v20.25.0`; their SHA256
match the release's digests and both Windows setups carry a valid signature of Object Vision B.V. `.m` and `.c` were
installed silently into `C:\Program Files\ObjectVision` (exit 0), and `testcases\run_testcases.bat` with each
installed `GeoDmsRun.exe` gives 529 cases, 0 bad; `.l` replaced 20.24.2.l in mtahi's WSL Ubuntu 26.04
(`dpkg -i`, its dependencies and `libodbc2` already present) and passed a smoke test. The 84 executables of the
`.m` setup have the `.text` sizes of `head_c56fe51a2`. The column of the local `c56fe51a2` round went to
`_Archive\20_25_0_m_local_c56fe51a2_20261016` with its report, its intermediates to
`C:\LocalData\runs\_aside_20_25_0_m_local_c56fe51a2_20261016`; no `20_25_0_c` or `20_25_0_l` existed.

`scratch\launch_perf_20250_release.cmd` first read t300's XML (500 files, 5.6 GiB in 59 s, the first-read rate of the
known cause below), then ran the three rounds one after another, threshold 60, put back to 95 after them: `.l`
2026-10-20 21:08 to 22:10, `.c` 22:10 to 2026-10-21 01:21, `.m` 01:21 to 04:29. Each full.py exit 0, nothing on
stderr, no reuse. Running on the machine: from 22:14 to 23:09, during `.c`'s t101 to t405.3, the Chrome Remote Desktop
host at 140 to 160 s of CPU per five minutes (a connected session) and the Claude desktop app at about 110 s,
Explorer and OneDrive at 22:49; otherwise the Claude app alone.

| flavour | report header | verdict |
|---|---|---|
| `.l` | 0:54:38 | 19 of 21 `ok`: t405.2 and t405.3 stop with exit 15 after 6 minutes, as in 20.24.2.l and 20.23.1.l |
| `.c` | 3:08:46 | 27 of 27 `ok` |
| `.m` | 3:05:09 | 27 of 27 `ok` |

Results: every `.json`, `.txt` and `.tif` result of `.m` is byte for byte the one of the local `c56fe51a2` round.
`.c` gives the results of `.m`, apart from the order of t010's `operator_groups.txt`. `.l` gives the results of
20.24.2.l except t301, which now has the 20.25.0 value of `.m`, and its t060 gpkg passes against the `_v20250`
reference: the loose quadtree visits the overlapping panden in the same order on Linux. Its known differences from
`.m` (t020's printed digits, t710, t010's order) are unchanged.

| test | `.m` release | `.m` local `c56fe51a2` | `.c` release | `.c` 20.24.2 | `.l` release | `.l` 20.24.2 |
|---|---|---|---|---|---|---|
| t020 | 0:03:52, 2.3 GB | 0:04:00, 2.5 GB | 0:03:53, 1.8 GB | 0:03:44, 2.0 GB | 0:03:57, 6.3 GB | 0:03:30, 6.4 GB |
| t060 | 0:01:44, 6.9 GB | 0:01:41, 7.4 GB | 0:01:48, 6.9 GB | 0:01:51, 6.8 GB | 0:02:59, 10.5 GB | 0:02:56, 10.6 GB |
| t101 | 0:03:12, 1.6 GB | 0:03:11, 1.6 GB | 0:03:48, 1.6 GB | 0:03:10, 1.6 GB | 0:03:42, 1.7 GB | 0:03:38, 1.7 GB |
| t200 | 0:00:37, 2.7 GB | 0:00:37, 2.7 GB | 0:00:39, 2.8 GB | 0:00:30, 3.2 GB | 0:00:58, 2.2 GB | 0:01:00, 1.7 GB |
| t300 | 0:01:04, 8.6 GB | 0:00:58, 10.4 GB | 0:01:03, 8.6 GB | 0:00:55, 10.5 GB | 0:01:17, 8.4 GB | 0:02:12, 7.1 GB |
| t301 | 0:02:40, 19.5 GB | 0:02:44, 17.8 GB | 0:03:04, 21.1 GB | 0:02:35, 16.5 GB | 0:01:36, 20.4 GB | 0:01:37, 20.6 GB |
| t405.1 | 0:04:26, 8.9 GB | 0:04:26, 8.2 GB | 0:03:33, 9.6 GB | 0:03:23, 10.0 GB | 0:04:35, 10.5 GB | 0:04:42, 10.4 GB |
| t405.2 | 0:17:38, 29.2 GB | 0:18:18, 36.2 GB | 0:19:20, 46.6 GB | 0:17:32, 29.0 GB | stopped | stopped |
| t405.3 | 0:17:28, 29.1 GB | 0:18:17, 29.2 GB | 0:19:10, 46.6 GB | 0:17:33, 29.0 GB | stopped | stopped |
| t410 | 0:03:50, 1.2 GB | 0:03:55, 1.3 GB | 0:03:51, 0.9 GB | 0:03:47, 0.9 GB | 0:04:14, 2.2 GB | 0:04:15, 2.2 GB |
| t611 | 0:00:30, 3.7 GB | 0:00:31, 4.1 GB | 0:00:31, 4.0 GB | 0:00:30, 4.0 GB | 0:00:33, 4.3 GB | 0:00:32, 4.5 GB |
| t641.1 | 0:49:13, 137.0 GB | 0:51:22, 139.4 GB | 0:50:38, 137.1 GB | 0:49:41, 141.9 GB | not run | not run |
| t641.2 | 0:44:13, 166.0 GB | 0:46:06, 171.4 GB | 0:43:00, 175.2 GB | 0:42:57, 166.2 GB | not run | not run |
| t710 | 0:01:02, 7.4 GB | 0:01:03, 6.2 GB | 0:01:01, 6.1 GB | 0:01:04, 7.1 GB | 0:01:10, 8.5 GB | 0:01:06, 7.9 GB |
| t720 | 0:09:22, 23.3 GB | 0:09:17, 20.7 GB | 0:09:20, 20.8 GB | 0:09:20, 19.4 GB | 0:09:35, 23.1 GB | 0:09:25, 20.3 GB |
| t810 | 0:05:29, 26.2 GB | 0:05:19, 25.9 GB | 0:05:23, 26.0 GB | 0:05:03, 25.9 GB | 0:06:28, 27.2 GB | 0:06:48, 27.4 GB |
| t910 | 0:00:49, 4.1 GB | 0:00:49, 2.8 GB | 0:00:48, 3.9 GB | 0:00:48, 3.1 GB | 0:00:58, 3.3 GB | 0:00:58, 3.5 GB |
| t2000 | 0:16:50, 64.5 GB | 0:18:06, 76.3 GB | 0:16:44, 62.8 GB | 0:16:53, 60.8 GB | not run | not run |

Summed over the rows that ran: `.m` 184.8 min against 191.5 for the local round and 188.8 for 20.24.2.m, `.c` 188.4
against 182.1 for 20.24.2.c, `.l` 54.4 against 54.8 for 20.24.2.l. `.l`'s t300 is 1:17 with its XML read beforehand,
against 2:12 for 20.24.2.l, the first round of its sitting, and 1:16 for 20.23.1.l, the third of its.

### Interleaved A/B of the rows that moved

`.c` was 9 to 30 % slower than 20.24.2.c on t101, t200, t301, t405.2 and t405.3, in the hour that it shared the
machine with the Remote Desktop session, and `.l` 13 % on t020. `scratch\perf_ovsrv05\ab_20250r.cmd` reran them
on 2026-10-21 from 04:30 to 06:40, X, Y, X, Y, threshold 60, the inputs read once beforehand (36.3 GiB):
X = the release 20.24.2 (`.c` kept as `C:\LocalData\GeoDMS_engine\release_0176bebdb_c`, `.l` unpacked from its
`.deb` beside 20.25.0.l in `/opt/ObjectVision`), Y = the release 20.25.0 (`release_fec6efe90_c`, the installed
20.25.0.l). t405.1 runs before t405.2 in each pass, which reads the intermediates it prepares. Seconds:

| flavour, test | X1 | Y1 | X2 | Y2 | Y / X |
|---|---|---|---|---|---|
| `.c` t101 | 195 | 191 | 190 | 190 | 0.99 |
| `.c` t200 | 31 | 31 | 31 | 30 | 0.98 |
| `.c` t301 | 148 | 148 | 146 | 152 | 1.02 |
| `.c` t405.1 | 204 | 207 | 203 | 208 | 1.02 |
| `.c` t405.2 | 1036 | 1082 | 1053 | 1079 | 1.03 |
| `.l` t020 | 211 | 204 | 206 | 203 | 0.98 |
| `.l` t710 | 68 | 67 | 66 | 67 | 1.00 |

Each run gave the results of its build's round. t405.2 is the one row where both 20.25.0 runs are slower than both
20.24.2 runs, by 26 to 46 s; two pairs do not settle 3 %, and the `.m` rounds show nothing of it (17:38 for the
release against 17:37 for 20.24.2.m).

### Reading

- Correct: `.m` and `.c` 27 of 27 and `.l` 19 of 21, with the results of the local `c56fe51a2` round and the
  20.25.0 references; on `.l` the order-dependent t301 and t060 give the values of `.m`.
- No performance regression: `.m` takes 184.8 min over the rows, the lowest of the 20.24.2 and 20.25.0 rounds. The rows of `.c` and
  `.l` that moved in their rounds are level with 20.24.2 in the A/B, except perhaps t405.2 on `.c` (+3 %, not
  settled). The setups compute what the local build computes, in the same time.

## Results (OVSRV05), the release v20.26.0 (tag `f3c45bc70`) in `.l`, `.c` and `.m`

The GitHub pre-release `v20.26.0` (published 2026-10-21 15:29 UTC) is tagged at `f3c45bc70`. Its engine differs
from 20.25.0 (`fec6efe90`) in `geo/dll/src/Dijkstra.{h,cpp}` (#1325, `3ddefdb69`: `impedance_matrix` with a LinkSet
per od row and pareto, through a pool of accepted labels), `rtc/dll/src/tic/Unit.cpp` (#1324, `88e796c31`:
`SetIrregularTileRange` reports the range it installed), the charts' X axis (`602c72590`) and the version number.
GeoDMS-Test at `5fb7780` (Jip Claassens' report changes `30962b2` and `5fb7780` on top of `6387c67`), pulled again
before the rounds. The `.m`, `.c` and `.l` assets were downloaded with `gh` into `C:\LocalData\GeoDMS_releases\v20.26.0`;
their SHA256 match the release's digests and both Windows setups carry a valid signature of Object Vision B.V.; `.m`
and `.c` were installed silently (exit 0), `.l` replaced 20.25.0.l in WSL and passed a smoke test.

`testcases\run_testcases.bat` with each installed `GeoDmsRun.exe` first gave 528 of 529: `stor_mmd_rule_orgrel_2_read`
failed its check `file_dom_rel_out == true`, which since `fec6efe90` expects `dom/rel_out` of the lookups store to be
kept as a rule, without a data file. The file it found was written on 2026-10-16 01:10, by a battery run of the
testcase before `fec6efe90`: the battery keeps `<OutDir>\LocalData` between runs, and the write half of the pair
does not remove a file that it no longer writes. With a fresh `-OutDir` both builds give 529 cases, 0 bad.

`scratch\launch_perf_20260_release.cmd` read t300's XML first (5.6 GiB in 58 s), then ran `.l` 2026-10-21 17:41 to
18:46, `.c` 18:46 to 22:06 and `.m` 22:06 to 2026-10-22 01:21, threshold 60, put back to 95. Each full.py exit 0,
nothing on stderr, no reuse. Running on the machine: a Remote Desktop session through `.c` and `.m` (the host
1 399 and 1 144 s of CPU, about a tenth of a core), the Claude desktop app at about a third of a core through `.l`.
The machine had restarted on 2026-10-21 at 11:46 for the October 2026 cumulative update of Windows 10 22H2
(KB5127308, KB5126657, KB5127689) and the .NET and Visual Studio updates of that morning.

| flavour | report header | verdict |
|---|---|---|
| `.l` | 0:57:35 | 19 of 21 `ok`: t405.2 and t405.3 stop with exit 15, as in every `.l` round of this account since 20.23.1; 21 of 21 in the rerun with 48 GB for WSL below |
| `.c` | 3:17:45 | 27 of 27 `ok` |
| `.m` | 3:12:32 | 27 of 27 `ok` |

Results: every `.json`, `.txt` and `.tif` result of each flavour is byte for byte the one of 20.25.0 in that
flavour, t101, t102, t405.x, t410 and t910, the network experiments that go through Dijkstra, included.

| test | `.m` 20.26.0 | `.m` 20.25.0 | `.c` 20.26.0 | `.c` 20.25.0 | `.l` 20.26.0 | `.l` 20.25.0 |
|---|---|---|---|---|---|---|
| t020 | 0:04:07 | 0:03:52 | 0:04:20 | 0:03:53 | 0:03:45 | 0:03:57 |
| t060 | 0:01:45 | 0:01:44 | 0:01:50 | 0:01:48 | 0:02:56 | 0:02:59 |
| t101 | 0:03:35 | 0:03:12 | 0:03:39 | 0:03:48 | 0:03:48 | 0:03:42 |
| t200 | 0:00:38 | 0:00:37 | 0:00:38 | 0:00:39 | 0:01:00 | 0:00:58 |
| t300 | 0:01:03 | 0:01:04 | 0:01:06 | 0:01:03 | 0:01:18 | 0:01:17 |
| t301 | 0:02:44 | 0:02:40 | 0:02:39 | 0:03:04 | 0:01:35 | 0:01:36 |
| t405.1 | 0:04:38 | 0:04:26 | 0:04:26 | 0:03:33 | 0:04:55 | 0:04:35 |
| t405.2 | 0:18:18 | 0:17:38 | 0:18:31 | 0:19:20 | stopped | stopped |
| t405.3 | 0:18:12 | 0:17:28 | 0:18:23 | 0:19:10 | stopped | stopped |
| t410 | 0:03:58 | 0:03:50 | 0:03:54 | 0:03:51 | 0:04:35 | 0:04:14 |
| t611 | 0:00:31 | 0:00:30 | 0:00:31 | 0:00:31 | 0:00:36 | 0:00:33 |
| t641.1 | 0:51:38 | 0:49:13 | 0:52:31 | 0:50:38 | not run | not run |
| t641.2 | 0:45:05 | 0:44:13 | 0:47:32 | 0:43:00 | not run | not run |
| t710 | 0:01:04 | 0:01:02 | 0:01:05 | 0:01:01 | 0:01:14 | 0:01:10 |
| t720 | 0:09:47 | 0:09:22 | 0:09:42 | 0:09:20 | 0:10:06 | 0:09:35 |
| t810 | 0:05:08 | 0:05:29 | 0:05:41 | 0:05:23 | 0:07:03 | 0:06:28 |
| t910 | 0:00:51 | 0:00:49 | 0:00:51 | 0:00:48 | 0:01:03 | 0:00:58 |
| t2000 | 0:18:16 | 0:16:50 | 0:19:15 | 0:16:44 | not run | not run |

Summed over the rows that ran: `.m` 192.1 min against 184.8, `.c` 197.3 against 188.4, `.l` 57.4 against 54.4, each
4 to 5 % above 20.25.0, with t101 on `.m` (+12 %), the experiment that the Dijkstra change touches most, and t405.1
on `.c` (+25 %, back at `.m`'s level) the largest rows. Peak memory as before; t2000's live peak 64 822 MB on `.m`
and 78 831 MB on `.c`, its two known levels.

### Interleaved A/B: the release 20.25.0 against the release 20.26.0

`scratch\perf_ovsrv05\ab_20260r.cmd`, 2026-10-22 01:23 to 04:21, X, Y, X, Y per flavour, threshold 60, the inputs read
once beforehand (50.1 GiB): X = 20.25.0 (`release_fec6efe90_m`, `release_fec6efe90_c`, 20.25.0.l unpacked from its
`.deb` beside 20.26.0.l), Y = 20.26.0 (`release_f3c45bc70_m`, `release_f3c45bc70_c`, the installed 20.26.0.l). Seconds:

| flavour, test | X1 | Y1 | X2 | Y2 | Y / X |
|---|---|---|---|---|---|
| `.m` t101 | 215 | 213 | 216 | 214 | 0.99 |
| `.m` t102 | 4 | 4 | 4 | 4 | 1.00 |
| `.m` t405.1 | 220 | 218 | 220 | 225 | 1.01 |
| `.m` t410 | 238 | 245 | 242 | 237 | 1.00 |
| `.m` t020 | 244 | 245 | 236 | 243 | 1.02 |
| `.c` t101 | 214 | 217 | 213 | 217 | 1.02 |
| `.c` t020 | 227 | 230 | 234 | 234 | 1.01 |
| `.c` t405.1 | 220 | 218 | 227 | 224 | 0.99 |
| `.l` t405.1 | 283 | 275 | 278 | 277 | 0.98 |
| `.l` t410 | 259 | 261 | 260 | 257 | 1.00 |
| `.l` t810 | 387 | 401 | 399 | 396 | 1.01 |
| `.l` t910 | 68 | 64 | 65 | 65 | 0.97 |
| `.l` t611 | 34 | 34 | 35 | 35 | 1.00 |

All 36 result files of the A/B are byte for byte those of their build's round. The two releases are level on every
row, t101 included. The sitting itself is slower than the ones before it: the
same binary, `release_fec6efe90_c`, took 195, 191, 190 and 190 s for t101 and 204 and 203 s for t405.1 in the A/B of
the section above (2026-10-21 04:33 to 06:22, no other load), and 214 and 213 s and 220 and 227 s here, 9 to 12 %
more. Between the two lie the Windows update with its restart and, during this A/B, a Remote Desktop session (its
host 867 s and `dllhost` 359 s of CPU). The session alone can account for it: the 20.25.0.c round took 3:48 for t101
in the hour that it shared with such a session, before the update, against 190 s the morning after without one.
Which of the two it is has not been separated; the session was still connected at the end of the A/B.

### t405.2 and t405.3 on `.l`, rerun with 48 GB for WSL

On 2026-10-22 the account mtahi, which runs the rounds here, got a `C:\Users\mtahi\.wslconfig`, written from a
scheduled task (`scratch\set_wslconfig_mtahi.cmd`), since a write into the profile from the agent's shell does not
reach the real one:

```
[wsl2]
memory=48GB
swap=128GB
swapFile=D:\\WSL\\swap_mtahi.vhdx
```

The swap file is the account's own: two VMs cannot share one, and the other account's VM uses `D:\WSL\swap.vhdx`.
After `wsl --shutdown` the VM reports 47 GB of memory and 128 GB of swap (`free -g`), against about 33.6 and 8 GB
before; see "t405.2 and t405.3 on `.l` on OVSRV05: the WSL VM of the account" under the known causes. GeoDMS-Test's
`batch\run_detached.ps1 -Version 20.26.0.l -Tests t405` then ran from `scratch\launch_t405_20260_l.cmd`, 2026-10-22
16:30 to 17:31, full.py exit 0, nothing on stderr, no other round on the machine. `-tests` reruns t405.1 too, which
prepares the data of the other two. The round's t405 files of 2026-10-21 are kept in
`_Archive\20_26_0_l_t405_wsl33GB_timeout_20261021`; the `20_26_0_l` column now holds the rerun.

Duration and peak rss / vms of the harness's sampler:

| test | rerun, 48 GB | round of 2026-10-21, 33.6 GB | 20.19.3.l, Cicada |
|---|---|---|---|
| t405.1 | 0:05:26, 12.1 / 15.6 GB | 0:04:55, 12.1 / 15.5 GB | 0:10:17, 12.0 / 15.6 GB |
| t405.2 | 0:26:11, 49.1 / 56.0 GB | stopped at 0:06:40, 32.8 / 41.6 GB | 0:24:28, 49.1 / 56.4 GB |
| t405.3 | 0:26:29, 49.1 / 55.8 GB | stopped at 0:06:47, 32.8 / 48.9 GB | 0:24:00, 49.1 / 56.6 GB |

Both ran to the end, their indicator steps too, and the `result.json` of t405.2 and of t405.3 is byte for byte the
one of 20.26.0.m. The report gives the `.l` column 21 `ok`, t641.x, t2000 and the GUI tests not run, header
1:37:20. The rerun is 7 to 10 % slower than 20.19.3.l, which ran in the Cicada account's distro before the Windows
update of 2026-10-21, after which this machine runs the same binary 9 to 12 % slower (above): no build comparison.

### t2000 on `.c`: 93 to 96 GB of commit is the level of the run, not of the flavour

The report gives t2000 on `.c` a `cmt` (the sampler's highest `vms`, on Windows the commit charge, in 10^9 bytes) of
96.0 GB in the round and 93.2 GB in a rerun of 2026-10-22 10:32 (GeoDMS-Test `run_detached.ps1 -Version 20.26.0.c
-Tests t2000`, outside the round's wrapper and so at threshold 95; the column's t2000 cell now holds it), against
82.7 GB on `.m` and 80.8 to 83.6 GB on both flavours of 20.24.2 and 20.25.0. Both `.c` runs are at the high level of
t2000's live peak (see "t2000's live peak: an interleaved A/B" under 20.25.0 at `c56fe51a2`): `PeakLiveLarge` 78 831
and 77 054 MB at 80 893 and 80 899 MB `Reserved in Blocks`, against 64 822 MB at 71 683 MB on `.m`. The two `.c` runs
agree although one ran at threshold 60 and the other at 95, and 20.26.0 changes no build file against 20.25.0.

`scratch\t2000_c\ab_t2000_c_vs_m.cmd` ran t2000 on the kept copies of the two release builds, `release_f3c45bc70_c`
(C) and `release_f3c45bc70_m` (M), interleaved, 2026-10-22 17:32 to 20:00, threshold 60, the inputs read once
beforehand, WSL shut down, beside the Claude desktop app and a Remote Desktop session (its host 50 to 90 s of CPU per
5 minutes). `scratch\t2000_c\run_exp_peak.py` is `run_exp.py` with two additions: it reads the process's own
`PeakPagefileUsage`, the exact peak of its commit charge, which the sampler and GeoDMS's status line see only when a
sample falls on it, and writes the commit charge per second; and it can add a switch, here `/SP` in CP and MP, which
logs a line per operation. Seconds, peak commit in 10^9 bytes, GeoDMS's own figures in MB, and the seconds in which
the commit charge stood above 80 GB:

| run | `/SP` | seconds | `PeakPagefileUsage` | Highest CommitCharge | `PeakLiveLarge` | Reserved in Blocks | s above 80 GB |
|---|---|---|---|---|---|---|---|
| C1 | | 1158 | 83.7 GB | 67 347 | 65 703 | 70 657 | 2 |
| M1 | | 1229 | 86.9 GB | 78 859 | 73 278 | 80 899 | 29 |
| C2 | | 1143 | 83.2 GB | 65 111 | 64 594 | 70 657 | 2 |
| M2 | | 1083 | 83.6 GB | 64 903 | 65 644 | 70 657 | 3 |
| CP | yes | 1075 | 94.5 GB | 72 099 | 78 071 | 80 899 | 44 |
| MP | yes | 1053 | 84.2 GB | 70 428 | 67 611 | 74 749 | 3 |
| C3 | | 1082 | 94.6 GB | 77 632 | 79 536 | 82 945 | 52 |
| M3 | | 1056 | 80.8 GB | 64 683 | 63 883 | 70 657 | 2 |

Every run gave the result of the round. Each build reaches both levels: `.c` was low in C1 and C2 and high in CP and
C3, `.m` high in M1 and low in M2, MP and M3. On the low level the commit charge passes 80 GB for 2 or 3 seconds and
peaks at 81 to 84 GB; on the high level it stays above 80 GB for 29 to 52 seconds and peaks at 87 to 95 GB. The
duration does not follow the level.

Where the peak is: in CP and MP it falls in R2_2022's
`BebouwingsComponenten/Woning/ActieveWoning/GeschikteOptieBerekening`, at the moment nine `sum`s with a result of
129 282 492 float64 values run together. They are materialized at once (`!regime=eager predicted=deferred`, 986 MB
each, 8.7 GiB together, about the distance between the two levels of `Reserved in Blocks`). In CP they ran from 404 to
422 s, 15 to 18 s each, and the commit charge stood above 80 GB from 404 to 448 s, with its peak of 94.5 GB at 413 s;
in MP they started at 430 s, eight of them took 7 to 9 s and the ninth 18 s, and the commit charge passed 80 GB for
3 s, at 434 s. The block's other 986 MB steps fell at other moments: its `merge` (arguments 5.9 GB) at 364 s in CP and
at 417 s in MP, and the `max_elem` that follows the sums (arguments 2.9 GB) at 423 to 435 s in CP and at 440 to 447 s
in MP; before the sums CP ran 22 `max_elem`s of 164 MB between 365 and 388 s, MP 7 between 389 and 409 s and a 986 MB
`lookup` of 21 s (384 to 405 s). A likely reading is
that in a high run the nine results come on top of more of the block's other 986 MB arrays than in a low run, but the
lines do not show which data is alive: most of them come from deferred operations, which report the construction of
their tile functors, and none of the 112 655 lines of CP carries `gross=` or `peak=`. The order is the order in which
the workers reach these steps, and runs already differ earlier: M1's commit charge lagged C1's and C2's by about 15 s
around 100 s.

### Reading

- Correct: `.m` and `.c` 27 of 27 and `.l` 19 of 21, each with the results of 20.25.0; #1325 and #1324 change no
  checked result, and the network experiments compute the same matrices. With 48 GB for WSL `.l` gives 21 of 21:
  its t405.2 and t405.3 were the account's VM, not a regression.
- No performance regression: every row that moved in the rounds is level with 20.25.0 in an A/B on the same
  machine in the same sitting. The 4 to 5 % of the rounds is the sitting: the same 20.25.0 binary is 9 to 12 %
  slower in it than the day before, under a Remote Desktop session and after the Windows update of 2026-10-21.
  Rounds from that day on compare with each other, and with earlier ones only through an A/B.
- No `.c` regression in t2000: its 96.0 and 93.2 GB of commit on `.c` against 82.7 GB on `.m` are the two levels of
  that experiment, which the same `.c` binary and the same `.m` binary each reach in an interleaved A/B (81 to 84 GB
  low, 87 to 95 GB high, results equal). The level follows the moment at which nine materialized 986 MB `sum`s of
  R2_2022's `GeschikteOptieBerekening` run, relative to the block's other large steps; a t2000 cell of the report
  says which level its run reached, and its memory verdict compares two draws.

## Results (OVSRV05), the pre-release v20.27.0 (tag `02f9fc329`) in `.m`: #1327

The GitHub pre-release `v20.27.0` (published 2026-10-28 09:04 UTC) is tagged at `02f9fc329` and has one asset, the
`.m` setup. Its engine differs from 20.26.0 (`f3c45bc70`) in #1327 (`02f9fc329`: a sparse `impedance_matrix` runs its
search once instead of a counting pass and a filling pass, and hands its rows over in the order of the origin zones
through `ordered_turn` in `rtc/dll/src/tic/TileChannel.h` into a chain of heap tiles, which the od attributes take
over with `DataArrayBase::AdoptTile`; `geo/dll/src/Dijkstra.cpp`, `rtc/dll/src/tic/{DataArray.{h,cpp},
TileArrayImpl.h,TileChannel.h}`, `stx/dll/src/DijkstraFlags.h`), in chart lines in the colour of their series
(`d158619a3`, ObjectVision/NetworkModel_PBL#129, `shv/dll/src/ChartLayer.{h,cpp}`) and in the version number
(`685085065`). The setup was downloaded with `gh` into `C:\LocalData\GeoDMS_releases\v20.27.0`; its SHA256 matches the
release's digest, it carries a valid signature of Object Vision B.V., and it was installed silently (exit 0) and kept
as `C:\LocalData\GeoDMS_engine\release_02f9fc329_m`. GeoDMS-Test at `5fb7780`, as for 20.26.0.

`scratch\launch_perf_20270_m.cmd` read t300's XML first (5.6 GiB in 59 s), then ran `.m` 2026-10-28 10:27 to 13:44,
threshold 60, put back to 95; full.py exit 0, nothing on stderr, no reuse. Running on the machine: a Chrome Remote
Desktop session (`remoting_host` 2 146 s of CPU over the round, about a fifth of a core) and the Claude desktop app.

| flavour | report header | verdict |
|---|---|---|
| `.m` | 3:14:24 | 27 of 27 `ok` |

Results: every `.json`, `.txt` and `.tif` result is byte for byte the one of 20.26.0.m. In the logs of 20.27.0 no
`impedance_matrix Counting` line is left; in 20.26.0.m t405.2 and t405.3 had 37 each, t910 6, t810 2, t102 1, t010
16, and t101 and t410 none.

Wall time and Highest CommitCharge, `.m`:

| test | 20.27.0 | 20.26.0 | 20.27.0 / 20.26.0 |
|---|---|---|---|
| t020 | 0:04:08, 1.8 GB | 0:04:07, 2.0 GB | 1.00 |
| t060 | 0:01:54, 6.9 GB | 0:01:45, 7.4 GB | 1.09 |
| t101 | 0:03:30, 1.6 GB | 0:03:35, 1.6 GB | 0.98 |
| t200 | 0:00:38, 3.4 GB | 0:00:38, 3.4 GB | 1.00 |
| t300 | 0:00:58, 10.3 GB | 0:01:03, 9.6 GB | 0.92 |
| t301 | 0:03:01, 12.9 GB | 0:02:44, 19.5 GB | 1.10 |
| t405.1 | 0:04:33, 8.9 GB | 0:04:38, 9.6 GB | 0.98 |
| t405.2 | 0:18:07, 28.0 GB | 0:18:18, 29.4 GB | 0.99 |
| t405.3 | 0:17:57, 29.0 GB | 0:18:12, 29.0 GB | 0.99 |
| t410 | 0:04:03, 1.2 GB | 0:03:58, 1.2 GB | 1.02 |
| t611 | 0:00:31, 4.1 GB | 0:00:31, 3.8 GB | 1.00 |
| t641.1 | 0:53:04, 138.4 GB | 0:51:38, 137.9 GB | 1.03 |
| t641.2 | 0:45:05, 171.8 GB | 0:45:05, 174.0 GB | 1.00 |
| t710 | 0:01:03, 7.4 GB | 0:01:04, 7.4 GB | 0.98 |
| t720 | 0:09:56, 19.8 GB | 0:09:47, 22.6 GB | 1.02 |
| t810 | 0:05:12, 25.8 GB | 0:05:08, 26.2 GB | 1.01 |
| t910 | 0:00:45, 4.4 GB | 0:00:51, 4.1 GB | 0.88 |
| t2000 | 0:18:44, 68.5 GB | 0:18:16, 63.4 GB | 1.03 |

Summed over these rows and t1630: 194.0 min against 192.1 (+1 %). t060 and t301, which do not call `impedance_matrix`,
are 9 and 10 % slower, t910 12 % faster.

### Interleaved A/B: the release 20.26.0 against the release 20.27.0

`scratch\perf_ovsrv05\ab_20270.cmd`, 2026-10-28 13:46 to 16:47, X, Y, X, Y, threshold 60, the inputs of the six
experiments read once beforehand (35.1 GiB, the list made from their logs by `scratch\perf_ovsrv05\log_inputs.py`):
X = 20.26.0 (`release_f3c45bc70_m`), Y = 20.27.0 (`release_02f9fc329_m`), through `scratch\t2000_c\run_exp_peak.py`,
which also reads each process's own `PeakPagefileUsage`. t405.1 prepares the data of t405.2 and t405.3 and makes no
sparse `impedance_matrix`; it is the control. The Remote Desktop session was still connected (`remoting_host` 838 s of
CPU). Seconds, and `PeakPagefileUsage` in 10^9 bytes:

| test | X1 | Y1 | X2 | Y2 | Y / X | peak commit X1, Y1, X2, Y2 |
|---|---|---|---|---|---|---|
| t910 | 49 | 42 | 47 | 41 | 0.86 | 6.38, 6.65, 7.36, 6.79 |
| t810 | 283 | 286 | 281 | 277 | 1.00 | 29.18, 29.12, 29.12, 29.08 |
| t102 | 5 | 5 | 5 | 5 | 1.00 | 2.47, 2.56, 2.55, 2.56 |
| t405.1 | 217 | 216 | 214 | 212 | 0.99 | 11.50, 11.50, 11.50, 11.50 |
| t405.2 | 1060 | 1090 | 1068 | 1062 | 1.01 | 52.20, 52.21, 52.21, 52.20 |
| t405.3 | 1080 | 1080 | 1074 | 1061 | 0.99 | 52.16, 52.22, 52.15, 52.21 |

All 24 result files of the A/B are byte for byte those of their build's round. t910 is 6.5 s faster, and the part of
its log between its first and its last `impedance_matrix` line, where its four sparse matrices are made, takes 19 and
17 s on X against 14 and 13 s on Y. t910's two `Allocation_Matrix` calls over 1 210 sources (616 296 and 303 997
od-pairs) were the largest counting passes of the round, and their filling pass ended 12 and 15 s after their counting
pass in 20.26.0.m. The other rows are level within 1.5 %. In t405.2 and t405.3 the 37 counting passes per run went
over networks that yield 1 to 860 od-pairs per call; the seconds that lie between such a counting and its filling line
in 20.26.0.m (44 and 50 s summed, 7 s at most) are those of other work running at the same time, not of the search.
t810's two calls with a counting pass make 49.5 and 49.6 million od-pairs, and their filling pass ended 2 s after their
counting pass.

Peak commit does not move: `PeakPagefileUsage` is level on every row, and GeoDMS's own Highest CommitCharge of
t405.2 (30 092 MB on X1, 29 786 on Y1) is the same. The 52.2 GB of t405.2 and t405.3 against GeoDMS's 30 GB is short:
the per-second commit of `run_exp_peak.py` stays above 35 GB for 34 and 41 seconds of the 1 060 and 1 090 s of X1 and
Y1, spread from 148 s to the end, on both builds.

### Reading

- Correct: 27 of 27 with the results of 20.26.0; the sparse matrices of one pass are those of two.
- #1327 makes t910 14 % faster (6.5 s of 48), the experiment whose sparse matrices are the largest relative to its
  run. Elsewhere the saving is below the noise of an A/B: t405.2 and t405.3 make many sparse matrices of a few
  od-pairs, t810 two large ones whose counting pass cost about 2 s each, t102 one of 7.8 million od-pairs in 5 s.
- Peak memory level: the shadow blocks that #1327 removes do not fall on the peak of any of these experiments.
- The round's 1 % over 20.26.0.m and its 9 and 10 % on t060 and t301 are the sitting, as in the rounds since
  2026-10-21: those rows do not reach `impedance_matrix`.

## Results (OVSRV05), 20.28.0 at `db151676e`: #1327 in join_near_values and in counting

A local msbuild Release x64 of `db151676e` (2026-10-29), kept as `C:\LocalData\GeoDMS_engine\head_db151676e`. Its engine
differs from the pre-release 20.27.0 (`02f9fc329`) in three commits and the version number:

- `2aca01715` (#1327): `join_near_values`, `connect_matrix` and `dist_matrix` write their rows in one pass, through
  `ordered_row_sink` in `rtc/dll/src/tic/TileChannel.h` and `clc/dll/include/IndexAssigner.h`.
- `55b161945` (#1327): the (value, count) pairs of `modus`, the classifications and `weeded_counts`, and the values that
  `unique` sorts, are kept in `block_vector` (`rtc/dll/src/mem/BlockVector.h`), in blocks of a tile's worth of elements
  and at most 1 MB, and a merge frees each block once it has read it. `unique` adopts whole blocks as tiles of its result.
- `795f75b37`: `Min`, `Max` and `MakeRange` are constexpr.

GeoDMS-Test at `5fb7780`, as for 20.26.0 and 20.27.0.

`scratch\launch_perf_20280.cmd` read t300's XML first (5.6 GiB in 46 s), then ran `.m` 2026-10-29 12:14 to 15:23,
threshold 60, put back to 95; full.py exit 0, nothing on stderr, no reuse. The Chrome Remote Desktop session was
connected (`remoting_host` 1 408 s of CPU over the round).

| flavour | report header | verdict |
|---|---|---|
| `.m` | 3:06:48 | 27 of 27 `ok` |

Results: every `.json`, `.txt` and `.tif` result is byte for byte the one of 20.27.0.m.

Wall time and Highest CommitCharge, `.m`:

| test | 20.28.0 | 20.27.0 | 20.28.0 / 20.27.0 |
|---|---|---|---|
| t020 | 0:04:10, 2.0 GB | 0:04:08, 1.8 GB | 1.01 |
| t060 | 0:01:54, 7.1 GB | 0:01:54, 6.9 GB | 1.00 |
| t101 | 0:03:32, 1.6 GB | 0:03:30, 1.6 GB | 1.01 |
| t200 | 0:00:38, 2.7 GB | 0:00:38, 3.4 GB | 1.00 |
| t300 | 0:00:57, 10.4 GB | 0:00:58, 10.3 GB | 0.98 |
| t301 | 0:02:35, 17.3 GB | 0:03:01, 12.9 GB | 0.86 |
| t405.1 | 0:04:29, 10.2 GB | 0:04:33, 8.9 GB | 0.99 |
| t405.2 | 0:17:22, 29.4 GB | 0:18:07, 28.0 GB | 0.96 |
| t405.3 | 0:17:34, 27.8 GB | 0:17:57, 29.0 GB | 0.98 |
| t410 | 0:04:14, 1.2 GB | 0:04:03, 1.2 GB | 1.05 |
| t611 | 0:00:35, 4.0 GB | 0:00:31, 4.1 GB | 1.13 |
| t641.1 | 0:50:57, 137.6 GB | 0:53:04, 138.4 GB | 0.96 |
| t641.2 | 0:43:35, 173.6 GB | 0:45:05, 171.8 GB | 0.97 |
| t710 | 0:01:05, 6.9 GB | 0:01:03, 7.4 GB | 1.03 |
| t720 | 0:07:25, 20.0 GB | 0:09:56, 19.8 GB | 0.75 |
| t810 | 0:05:16, 25.7 GB | 0:05:12, 25.8 GB | 1.01 |
| t910 | 0:00:44, 4.5 GB | 0:00:45, 4.4 GB | 0.98 |
| t2000 | 0:18:33, 77.3 GB | 0:18:44, 68.5 GB | 0.99 |

Summed over these rows and t1630: 186.4 min against 194.0 (-4 %). t720's 445 s is new: every kept round of this machine,
from 17.4.6 (2025-06) to 20.27.0, ran it in 552 to 612 s, except 20.22.0, where it stopped on #1285. t2000's 77.3 GB is
the higher of its two known levels (see "t2000's live peak" above).

### Interleaved A/B: the release 20.27.0 against 20.28.0

`scratch\perf_ovsrv05\ab_20280.cmd`, 2026-10-29 15:27 to 18:07, X, Y, X, Y, threshold 60, the inputs of the eight
experiments read once beforehand (33.3 GiB, list `ab_20280_inputs.txt` from their logs by `log_inputs.py`):
X = 20.27.0 (`release_02f9fc329_m`), Y = 20.28.0 (`head_db151676e`), through `scratch\t2000_c\run_exp_peak.py`. Chosen:
the rows that moved in the round (t720, t611, t410, t301) and those that call `unique`, `modus` or `join_near_values`
often (t710 2UP, t060 with the `join_near_values` of `Regression/cfg/stam/MakeSnapshot.dms:332`, t405.1 and t405.2 of
NetworkModel_PBL). `remoting_host` took 1 506 s of CPU. Seconds, and `PeakPagefileUsage` in 10^9 bytes:

| test | X1 | Y1 | X2 | Y2 | Y / X | peak commit X1, Y1, X2, Y2 |
|---|---|---|---|---|---|---|
| t720 | 589 | 438 | 575 | 444 | 0.76 | 23.99, 26.68, 25.76, 26.49 |
| t611 | 31 | 32 | 31 | 31 | 1.02 | 4.81, 4.78, 4.84, 5.02 |
| t710 | 64 | 65 | 65 | 64 | 1.00 | 8.05, 8.04, 8.04, 8.03 |
| t060 | 100 | 100 | 100 | 100 | 1.00 | 9.59, 9.59, 9.59, 9.59 |
| t301 | 158 | 155 | 159 | 155 | 0.98 | 19.51, 23.11, 20.38, 22.46 |
| t410 | 245 | 241 | 253 | 240 | 0.97 | 1.41, 1.49, 1.41, 1.50 |
| t405.1 | 218 | 217 | 220 | 217 | 0.99 | 11.50, 11.75, 11.50, 11.75 |
| t405.2 | 1072 | 1032 | 1099 | 1029 | 0.95 | 52.17, 52.38, 52.19, 52.43 |

All 20 `.json`, `.txt` and `.tif` result files of the A/B are byte for byte those of the round; the 16 `.result.xml`
files differ from it in their version and time fields only. Summed, Y takes 4 560 s against 4 979 on X (-8 %), of which
t720 282 s and t405.2 110 s.

t720 (2BURP) is 141 s faster per run, and the time falls in the stretch where its log is silent before the result is
written (518 s in the round of 20.27.0, 380 s in that of 20.28.0). The likely place is the smoothing loop of
`Preprocessing/DegreeOfUrbanisation.dms`: up to 20 iterations of `modus(PrevState/Urban_Cluster[Cdomain_x_pot1k_3x3/
third_rel], Cdomain_x_pot1k_3x3/first_rel)` over the 3x3 neighbourhood of every land cell. Its values (cluster numbers)
have a range far larger than the values per partition, so it counts in sets (`ModusPartBySet`,
`GetPartitionedWallCounts` in `clc/dll/include/ValuesTable.h`), whose partial counts are merged in the code that
`55b161945` changed. That one operator carries the gain is not measured.

Peak commit rises on t301 and t720:

- t301 by 2.8 GB (19.5 and 20.4 on X, 23.1 and 22.5 on Y). The per-second commit lies above 18 GB for 13 and 18 s on X
  and for 34 and 32 s on Y, from 40 to 90 s into the run, while the 84 rectangles of the BAG snapshot run
  `polygon_connectivity` and the `unique(neighbours/coded_pair)` of `Regression/cfg/stam/MakeSnapshot/afleidingen.dms:121`
  on its upoint pairs, many at a time. GeoDMS's own Highest CommitCharge, which samples, says the opposite (16.7 and 17.1
  GB on X, 14.5 and 15.5 on Y).
- t720 by 1.7 GB, at the start (the sample at 9 s: 21.2 and 22.8 GB on X, 26.2 and 25.7 on Y), while the source grids are
  read; afterwards its commit is level (median 12.8 to 13.3 GB).
- t405.1 and t405.2 by 0.25 GB, t410 by 0.08; t060, t710 and t611 level.

A possible cause, not measured: the blocks of `block_vector` are at most 1 MB and so come from the free stacks of the
allocation stock, which keep a freed block committed (see "FixedAlloc's free stacks" below), where the containers of
20.27.0 above 1 MB went to the CRT heap and back when freed. With many `unique` calls at once, the free stacks could keep
more committed than the containers did.

### Reading

- Correct: 27 of 27 with the results of 20.27.0, and the A/B results are those of the round.
- 20.28.0 makes t720 24 % faster (141 of 582 s), most likely through the counting in blocks of `55b161945`, and
  t405.2 5 % and t410 3 %; which operators carry those two is not measured. The other rows are level within 2 %. The
  one-pass `join_near_values` of `2aca01715` leaves t060 at 100 s.
- t611's 13 % in the round is the sitting: 31 s on both builds in the A/B.
- Peak commit: t301 2.8 GB and t720 1.7 GB higher in the process counter, both in a phase where many counting or `unique`
  calls run side by side; the memory halving of a single large `modus` or `unique` in the commit message does not show
  in these experiments.
  The A/Bs of the release 20.28.0 below put the rise at about 1 GB on average, with 2.8 GB the high end of the
  spread between runs.

## Results (OVSRV05), the pre-release v20.28.0 (tag `01feccd21`) in `.l`, `.c` and `.m`: #1327 in `unique` on strings and in `pareto_optimal`

The GitHub pre-release `v20.28.0` (published 2026-10-31 14:32 UTC) is tagged at `01feccd21`. Its engine differs from the
local build of `db151676e` measured above in three commits:

- `9fb4d4d8f` (#1327): `unique` of strings and other sequences sorts references to the elements per part of a tile and
  merges them in `block_vector`s, as `unique` on values does since `55b161945`, instead of one index over all elements.
- `01feccd21` (#1327): `pareto_optimal` and `pareto_optimal_eps` read their criteria from their tiles, with pairs of two
  `UInt32` where they fit. No configuration of GeoDMS-Test calls either, so full.py does not measure this one.
- `1a5f06822`: `omni::swap` removed in favour of `std::swap`.

The setups were built on OVSRV10 by the release scripts; `db151676e` was a local msbuild on OVSRV05. GeoDMS-Test at
`5fb7780`, unchanged upstream. The `.m`, `.c` and `.l` assets were downloaded with `gh` into
`C:\LocalData\GeoDMS_releases\v20.28.0`; their SHA256 match the release's digests and both Windows setups carry a valid
signature of Object Vision B.V.; `.m` and `.c` were installed silently (exit 0) and kept as
`C:\LocalData\GeoDMS_engine\release_01feccd21_{m,c}`; `.l` replaced 20.26.0.l in WSL, which was unpacked again beside it
from its `.deb`. Each passed a smoke test on `testcases\fn_test_pareto_optimal.dms`. The column of `db151676e` moved to
`_Archive\20_28_0_m_local_db151676e_20261029` (with its report and `runs` folder) before the `.m` round.

`scratch\launch_perf_20280_release.cmd` read t300's XML first (5.6 GiB in 46 s), then ran `.l` 2026-10-31 15:38 to 17:08,
`.c` 17:08 to 20:11 and `.m` 20:11 to 23:14, threshold 60, put back to 95. Each full.py exit 0, nothing on stderr, no
reuse. No Remote Desktop session was connected this time (it was through both earlier `.m` rounds of 20.27.0 and
20.28.0); the Claude desktop app took about a fifth of a core through `.l` and `.c` (1 796 and 2 292 s of CPU) and next to
nothing through `.m` (130 s).

| flavour | report header | verdict |
|---|---|---|
| `.l` | 1:22:44 | 21 of 21 `ok` |
| `.c` | 3:00:59 | 27 of 27 `ok` |
| `.m` | 3:00:21 | 27 of 27 `ok` |

Results: every `.json`, `.txt` and `.tif` result of `.m` is byte for byte the one of 20.27.0.m and of `db151676e`, and
those of `.c` and `.l` the ones of 20.26.0 in that flavour (20.27.0 had a `.m` asset only).

Wall time per experiment, `.m`, from the report:

| test | release 20.28.0 | `db151676e` | 20.27.0 | release / `db151676e` | release / 20.27.0 |
|---|---|---|---|---|---|
| t020 | 0:03:49 | 0:04:10 | 0:04:08 | 0.92 | 0.92 |
| t060 | 0:01:50 | 0:01:54 | 0:01:54 | 0.96 | 0.96 |
| t101 | 0:03:19 | 0:03:32 | 0:03:30 | 0.94 | 0.95 |
| t200 | 0:00:35 | 0:00:38 | 0:00:38 | 0.92 | 0.92 |
| t300 | 0:01:39 | 0:00:57 | 0:00:58 | 1.74 | 1.71 |
| t301 | 0:02:37 | 0:02:35 | 0:03:01 | 1.01 | 0.87 |
| t405.1 | 0:04:29 | 0:04:29 | 0:04:33 | 1.00 | 0.99 |
| t405.2 | 0:16:49 | 0:17:22 | 0:18:07 | 0.97 | 0.93 |
| t405.3 | 0:16:32 | 0:17:34 | 0:17:57 | 0.94 | 0.92 |
| t410 | 0:03:51 | 0:04:14 | 0:04:03 | 0.91 | 0.95 |
| t611 | 0:00:30 | 0:00:35 | 0:00:31 | 0.86 | 0.97 |
| t641.1 | 0:49:18 | 0:50:57 | 0:53:04 | 0.97 | 0.93 |
| t641.2 | 0:43:10 | 0:43:35 | 0:45:05 | 0.99 | 0.96 |
| t710 | 0:01:02 | 0:01:05 | 0:01:03 | 0.95 | 0.98 |
| t720 | 0:07:02 | 0:07:25 | 0:09:56 | 0.95 | 0.71 |
| t810 | 0:04:53 | 0:05:16 | 0:05:12 | 0.93 | 0.94 |
| t910 | 0:00:42 | 0:00:44 | 0:00:45 | 0.95 | 0.93 |
| t2000 | 0:17:13 | 0:18:33 | 0:18:44 | 0.93 | 0.92 |

Over all experiment logs (`durations.py`): 180.7 min against 187.2 for `db151676e` and 194.7 for 20.27.0. Against
`db151676e` nearly every row is 3 to 9 % faster, also those that none of the three commits touches (t020, t101, t200,
t810): the sitting, which the A/B below confirms. t300's 1:39 is its first read of the XML (18 s per file set against
11 s in the `.c` round of that afternoon), see "t300 and the first read of its XML on OVSRV05" below.

`.c` and `.l` against 20.26.0 (`durations.py`, minutes; no A/B of these, so they include 20.27.0, 20.28.0 and the
sitting):

| test | `.c` 20.28.0 | `.c` 20.26.0 | `.l` 20.28.0 | `.l` 20.26.0 |
|---|---|---|---|---|
| t301 | 3.4 | 2.6 | 1.6 | 1.6 |
| t405.1 | 4.4 | 4.4 | 4.3 | 5.4 |
| t405.2 | 17.0 | 18.5 | 23.0 | 26.1 |
| t405.3 | 17.4 | 18.4 | 22.3 | 26.4 |
| t641.1 | 49.3 | 52.5 | not run | not run |
| t641.2 | 42.5 | 47.5 | not run | not run |
| t720 | 7.1 | 9.7 | 6.6 | 10.1 |
| t810 | 5.4 | 5.7 | 6.2 | 7.0 |
| t910 | 0.7 | 0.8 | 0.8 | 1.1 |
| t2000 | 16.6 | 18.8 | not run | not run |
| total | 181.4 | 197.6 | 82.8 | 97.4 |

t720's gain of `55b161945` shows in every flavour (`.l` 6.6 against 10.1 min). t301 on `.c` is the one row above its
predecessor: its five `.c` rounds since 20.23.1 gave 2.5, 2.6, 3.1, 2.6 and 3.4 min, and the extra time lies in the waits
between the insert and extract phases of `polygon_connectivity` in a few rectangles (54 and 43 s, against a longest of
46 s in the round of 20.26.0.c); see the `.c` A/B below.

### Interleaved A/B: 20.27.0, `db151676e` and the release 20.28.0, `.m`

In the same wrapper, 2026-10-31 23:15 to 2026-11-01 03:02, X Y Z Z Y X, threshold 60, the inputs read once beforehand
(list `ab_20280_inputs.txt`), the experiments of the A/B of 2026-10-29 through `scratch\t2000_c\run_exp_peak.py`:
X = 20.27.0 (`release_02f9fc329_m`), Y = `db151676e` (`head_db151676e`), Z = the release 20.28.0 (`release_01feccd21_m`).
Nothing else ran (the Claude desktop app 168 s of CPU). Seconds:

| test | X1 | Y1 | Z1 | Z2 | Y2 | X2 | Y / X | Z / Y |
|---|---|---|---|---|---|---|---|---|
| t720 | 565 | 423 | 423 | 425 | 425 | 551 | 0.76 | 1.00 |
| t611 | 30 | 30 | 31 | 30 | 30 | 30 | 1.00 | 1.02 |
| t710 | 61 | 61 | 61 | 61 | 61 | 61 | 1.00 | 1.00 |
| t060 | 97 | 96 | 96 | 94 | 95 | 94 | 1.00 | 0.99 |
| t301 | 151 | 146 | 158 | 151 | 152 | 149 | 0.99 | 1.04 |
| t410 | 227 | 229 | 234 | 229 | 226 | 228 | 1.00 | 1.02 |
| t405.1 | 212 | 209 | 209 | 209 | 210 | 212 | 0.99 | 1.00 |
| t405.2 | 1041 | 1004 | 998 | 998 | 1004 | 1048 | 0.96 | 0.99 |

Summed: X 4 757 s, Y 4 401, Z 4 407. All 30 `.json`, `.txt` and `.tif` result files of the six runs are byte for byte
those of the `.m` round. `PeakPagefileUsage`, in 10^9 bytes:

| test | X1, X2 | Y1, Y2 | Z1, Z2 |
|---|---|---|---|
| t720 | 24.09, 25.06 | 24.21, 26.59 | 24.73, 24.42 |
| t611 | 4.77, 4.95 | 4.94, 4.89 | 4.98, 4.85 |
| t710 | 8.03, 8.06 | 8.05, 8.05 | 8.02, 8.03 |
| t060 | 9.59, 9.58 | 9.58, 9.58 | 9.59, 9.58 |
| t301 | 22.01, 20.69 | 20.34, 21.68 | 21.90, 21.79 |
| t410 | 1.41, 1.41 | 1.47, 1.47 | 1.42, 1.51 |
| t405.1 | 11.50, 11.50 | 11.75, 11.75 | 11.75, 11.75 |
| t405.2 | 52.19, 52.19 | 52.45, 52.47 | 52.44, 52.44 |

- The release and `db151676e` are level: Z / Y lies between 0.99 and 1.04 on every experiment, and where it moves (t301
  151 to 158 against 146 to 152, t410) the two Z runs and the two Y runs overlap or come within 2 %. `unique` on strings
  and the build machine change none of these experiments measurably.
- 20.27.0 to `db151676e` repeats the A/B of 2026-10-29: t720 0.76 (558 to 424 s), t405.2 0.96. t410's 0.97 of then is
  1.00 now.
- Peak commit: the A/B of 2026-10-29 gave `db151676e` +2.8 GB on t301 and +1.7 GB on t720. In this sitting the
  difference is smaller and mixed: t301 X 22.01 and 20.69, Y 20.34 and 21.68 (the per-second commit above 18 GB for 31
  and 23 s on X, 37 and 24 s on Y, 25 and 25 s on Z), t720 X 24.09 and 25.06, Y 24.21 and 26.59. The release lies with
  `db151676e`. What repeats exactly in every run of both sittings is +0.25 GB on t405.1 and t405.2 and +0.06 GB on
  t410, in Y and Z alike; for t301 and t720 see the `.c` A/B below.

### Interleaved A/B: t301 on `.c`

`scratch\perf_ovsrv05\ab_20280c_t301.cmd`, 2026-11-01 03:03 to 03:25, X Y Y X X Y Y X, threshold 60, right after the
`.m` A/B (inputs warm): X = the release 20.26.0.c (`release_f3c45bc70_c`; 20.27.0 had no `.c`, and its change does not
reach t301), Y = the release 20.28.0.c (`release_01feccd21_c`). All eight result sets are byte for byte those of the
`.c` round.

| run | X1 | Y1 | Y2 | X2 | X3 | Y3 | Y4 | X4 |
|---|---|---|---|---|---|---|---|---|
| seconds | 157 | 160 | 153 | 159 | 148 | 160 | 172 | 175 |
| `PeakPagefileUsage` | 20.10 | 22.52 | 22.08 | 22.02 | 22.98 | 24.40 | 25.93 | 24.81 |

Time is level (X 639 s in four runs, Y 645, 1.01), and every run took 148 to 175 s: the 3.4 min of the `.c` round is
the sitting. The peak commit rises through the sitting for both builds (X 20.1 to 24.8 GB, Y 22.5 to 25.9), and Y is
above its neighbouring X in all four pairs (+2.42, +0.06, +1.42, +1.12 GB). Taking the neighbouring runs of the three
A/Bs together, a build with `55b161945` is above one without in 11 of 12 pairs on t301 and t720 (−1.67 to +3.60 GB),
by 1.25 GB on average on t301 and 1.27 GB on t720, against a spread of 2 to 5 GB between runs of one build. So a rise of
about 1 GB on these two experiments is likely; the 2.8 GB of 2026-10-29 was the high end of that spread. Its cause is
still not measured.

### Reading

- Correct: 21 of 21 in `.l`, 27 of 27 in `.c` and `.m`, each with the results of the previous version in that flavour.
- The release 20.28.0 is as fast as the local build of `db151676e` on every experiment of the A/B; the 3 to 9 % that the
  `.m` round shows on most rows is the sitting. The commits since, `unique` on strings per tile (`9fb4d4d8f`) and
  `pareto_optimal` from its tiles (`01feccd21`), have no measurable effect on full.py: no experiment calls
  `pareto_optimal`, and the A/B shows no difference that `unique` on strings could account for (which experiments call
  it on strings, and for how long, is not established).
- Against 20.27.0, 20.28.0 stays 24 % faster on t720 and 4 % on t405.2 (A/B), and the `.l` and `.c` rounds show the t720
  gain too.
- t301 on `.c` is level with 20.26.0.c in an A/B (1.01); its 3.4 min in the round is the sitting.
- Peak commit: the rise of t301 and t720 since `55b161945` is about 1 GB on average (11 of 12 neighbouring pairs, -1.7 to
  +3.6 GB), not the 2.8 and 1.7 GB of the A/B of 2026-10-29, which were the high end of a spread of 2 to 5 GB between
  runs of one build. t405.1 and t405.2 are 0.25 GB higher in every run.

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

## t300 and the first read of its XML on OVSRV05

Established on 2026-10-16 from the rounds of 2026-10-07 to 2026-10-16 and a read test; not a change in the code.

t300 reads the 500 BAG XML files of `D:\SourceData\t300_XML`, 5.7 GB on an NVMe SSD, through the `strfiles`
storage: five sets of 100 files, each file by one `fread` of its whole size (`StrStorageManager::ReadDataItem`),
and `parse_xml` of one set overlapping the read of the next. Its wall time has two levels on this machine:

| round | t300 | its place |
|---|---|---|
| 20.23.0.m (07-10), 20.24.0.m (10-10), 20.24.1.m (14-10), 20.24.2 local (14-10), `distr` setup (15-10) | 1:53 to 1:57 | the only round of the sitting |
| 20.23.1.m (08-10) | 1:58 | first of three |
| 20.23.1.c, 20.23.1.l (08-10) | 1:04 (`.c`), 1:16 (`.l`) | second and third |
| release 20.24.2.l (15-10) | 2:12 (`.l`) | first of three |
| release 20.24.2.c and `.m` (15-10) | 0:55, 1:03 | second and third |
| 20.25.0.m (16-10) | 0:59 | after the A/B below, which read the same files |

In a slow run the read is the bottleneck: the 100 files of a set (about 1.1 GB) take 21 s, and the parse is
hidden behind them. In a fast run the read takes about 1 s and the parse about 10 s per set. The builds do not
matter: in an interleaved A/B with the files read beforehand (2026-10-16, 01:11 to 01:17), `806e7dd61`, the
release 20.24.2.m and `cd3e83ec7` each took 58 to 59 s.

A read test on 2026-10-16 at 04:35, after the 20.25.0 round, with `scratch\perf_ovsrv05\scan_read_test.py` (one
read per file, as `ReadDataItem` does):

| read | rate |
|---|---|
| the first 50 XML files of t300 (571 MiB), first read | 106 MiB/s |
| the same files, second read | 2 273 MiB/s |
| a copy of those 50 files made with `cp` | 108 MB/s to write it |
| that copy, first read | 3 055 MiB/s |
| `t060_BAG20\snapshot_Utrecht_20210701_voor14.15.1.gpkg` (621 MiB, a reference no round reads), first read | 1 071 MiB/s |

The disk delivers ten times what a first read of these XML files gets, and a binary file of the same size reads
at disk speed the first time. What is slow is the first opening of a text file, and writing one: the pattern of
an on-access scan, which Windows Defender's real-time protection on this machine performs (`Get-MpComputerStatus`:
`RealTimeProtectionEnabled`, `OnAccessProtectionEnabled`); its exclusions cannot be read without
administrator rights. At about 100 MB/s, 5.7 GB costs about 57 s, which is the difference between the two levels.
When the scanner forgets a file again is not established: the first round of a sitting has always been slow, and
a round that followed another within a few hours fast, but the read test found the files forgotten three hours
after the 20.25.0 round had read them.

**Reading a comparison of rounds:** a t300 of about 1:55 against about 1:00 says which round of its sitting it
was, not which build ran. To compare builds, read the inputs once before the round or the A/B
(`scratch\perf_ovsrv05\warm_inputs.py`, which reads every file it is given), or compare rounds that held the
same place. The `copy /b D:\SourceData\t300_XML\*.xml NUL` before the A/B above took 0.9 s, so the files were
still known then and it proves nothing as a warm-up. Other experiments that read text files
(t405.1 reads 82 CSV and 68 shapefile reads) may carry the same first-read cost; that has not been measured.

## t405.2 and t405.3 on `.l` on OVSRV05: the WSL VM of the account

Established on 2026-10-22 from the `.l` columns of this machine and a rerun of 20.26.0.l; not a change in the code.

WSL settings and distros belong to a Windows account: each account that runs a `.l` round has its own VM, sized by
its own `%USERPROFILE%\.wslconfig`. Without that file the VM gets half the host's RAM, here about 33.6 GB inside
the VM, and a swap of a quarter of that, about 8 GB on C:. On Linux t405.2 and t405.3 reach about 49 GB of rss,
and in such a VM they are stopped when they reach its ceiling: exit 15 after about six minutes, shown as a
`timeout` pill. Duration and peak rss / vms of the harness's sampler, both tests together, from the `.l` columns
kept in the results tree:

| rounds | account (load lines of the logs) | duration | rss | vms |
|---|---|---|---|---|
| 20.3.0, 20.7.0, 20.8.0 (2026-06-22 to 07-12) | Cicada | 0:24:16 to 0:25:54 | 49.1 to 49.4 GB | 65.3 to 67.0 GB |
| 20.12.0 to 20.19.3 (2026-08-09 to 09-05) | Cicada | 0:23:42 to 0:24:46 | 48.9 to 49.2 GB | 56.4 to 56.8 GB |
| 20.20.0 (2026-09-11) | Jip Claassens | 0:30:55 and 0:37:25 | 49.5 GB | 116.0 and 121.5 GB |
| 20.23.1, 20.24.2, 20.25.0, 20.26.0 (2026-10-08 to 10-21) | mtahi, no `.wslconfig` | stopped at 0:05:53 to 0:06:47 | 32.4 to 32.9 GB | 41.5 to 48.9 GB |
| 20.26.0 rerun (2026-10-22) | mtahi, 48 GB and 128 GB swap | 0:26:11 and 0:26:29 | 49.1 GB | 55.8 and 56.0 GB |

The other two accounts' VMs held those 49 GB without stopping them, so they had more than the default; their
`.wslconfig` files cannot be read from this account. On Windows the report gives t405.2 and t405.3 51 to 53 GB of
peak commit in every version from 16.0.5 to 20.26.0, `.m` and `.c` alike, at 27 to 48 GB physical, except 20.20.0
(92 to 98 GB, high in `.l` too): the engine asks no more of these tests than it did.

**Reading a `.l` column:** a `timeout` on t405.2 or t405.3 after about six minutes says which VM ran it, not which
build. Before a `.l` round, check `wsl -d Ubuntu -- free -g` in the account that runs it (47 GB and 128 GB of swap
for mtahi since 2026-10-22). After a Windows restart run `wsl --shutdown` and boot the VM once, so that its swap on
D: is active again (the note in GeoDMS-Test's `batch\run_detached.ps1`). Two VMs of 48 GB together exceed the
host's 64 GB: never run `.l` rounds under two accounts at once, and shut down a VM that is not in use. t641.x and
t2000 stay skipped on `.l` regardless: full.py decides that on the host's RAM (`_HEAVY_L_MIN_HOST_GB`), not the VM's.
