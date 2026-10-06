# Archived documents

*Status (2026-10-06): created by the documentation sweep of that day (A11 of
`doc/continuations-2026-10-06.md`; `git log --grep=A11`).*

The documents in this folder are history: plans whose work has landed or was dropped, audits
whose findings are closed or carried over, logs of measurements of a state of the engine that no
longer exists, and notes that the skills under `.claude/skills/` or a newer document replaced. They
are kept as written, each with an `Archived 2026-10-06` line under its title that says why and
where its subject lives now; a few carry dated notes where the sweep found the text wrong. Read
the successor first; come here for the reasoning and the measurements behind it.

## What is here

| Document | Why it is here | Successor |
|---|---|---|
| [build-tips.md](build-tips.md) | Its CMake paths contradict the presets; the x64-hosted compiler is forced since 92c99e468 and `RLookup.cpp` was split in 0929ad45d, so its two workarounds are moot | `.claude/skills/geodms-build` |
| [code-fixes.md](code-fixes.md) | The pick-list of 2026-09-05: every item fixed, refuted or moved; the six still open became item 17 of the cleanup list | [../cleanup-list.md](../cleanup-list.md) |
| [compile-time-refactor-analysis-2026-07.md](compile-time-refactor-analysis-2026-07.md) | Done: precompiled headers, the clc splits, the rtc+sym+tic merge, the move to `std::format` | [../development/header-hygiene-2026-08.md](../development/header-hygiene-2026-08.md), [../development/tu-reorg-and-export-surface-2026-08.md](../development/tu-reorg-and-export-surface-2026-08.md) |
| [dev-environment-gotchas.md](dev-environment-gotchas.md) | The launchers moved to `batch\`, `run_unit_suite.bat` puts the tst folder on `PATH`, and the case renames landed (ffde99f53) | AGENTS.md; `.claude/skills/geodms-commit`, `geodms-debug` |
| [FutureTileFunctor notes.txt](FutureTileFunctor%20notes.txt) | 2023 notes; aggregations consume arrays of future tiles (`GetFutureTileArray`) | [../tile-data-retainment.md](../tile-data-retainment.md) |
| [include-tree.txt](include-tree.txt) | A 2023 include tree of headers that no longer exist | [../development/header-hygiene-2026-08.md](../development/header-hygiene-2026-08.md) |
| [k11-container-types-scope.md](k11-container-types-scope.md) | K11a-1 to K11a-4 and K11b landed on 2026-07-26 to 28; the leftovers are listed in the successor | [../development/typed-hof-remaining-work.md](../development/typed-hof-remaining-work.md) |
| [MT2-issues.txt](MT2-issues.txt) | Notes of 2022; items 1, 2 and 6 are done and item 5 went with the DataStoreManager (#1189) | [../deadlocks.md](../deadlocks.md), [../Interest.md](../Interest.md) |
| [performance-test-20.20-20.21.md](performance-test-20.20-20.21.md) | The first 722 lines of `doc/performance-test.md`: the rounds that measured the #1259 deferral of commits and IntegrityChecks, which was removed in a7127224f and 92eaa7150 | [../performance-test.md](../performance-test.md) |
| [ptr-safety-review-2026-07-02.md](ptr-safety-review-2026-07-02.md) | The review of dangling dereferences after the std-ptr migration, fixed on 2026-07-02 and 03 | [../development/ownership.md](../development/ownership.md) |
| [schedule-with-lookahead-log.md](schedule-with-lookahead-log.md) | Section 8.1, the dated running log of the scheduling plan, about 2,200 lines | [../development/schedule-with-lookahead.md](../development/schedule-with-lookahead.md) |
| [std-ptr-migration-plan.md](std-ptr-migration-plan.md) | The design of the move of the TreeItem family to `std::shared_ptr` (3f17a26e5 to ae5fecde5, wrappers removed in 70c3f9422) | [../development/ownership.md](../development/ownership.md) |
| [stdptr-migration-handoff.md](stdptr-migration-handoff.md) | The work log of that migration | [../development/ownership.md](../development/ownership.md) |
| [teardown-leak-and-ownership-cycles.md](teardown-leak-and-ownership-cycles.md) | The leak hunt that led to the migration; the tile-functor cycle it describes was cut again by TIC-A10 (0fc692c63) | [../development/ownership.md](../development/ownership.md) |
| [testing-strategy.md](testing-strategy.md) | Its thesis holds (only full.py trips thread, stack and meta-thread bugs); its logistics were stale | `.claude/skills/geodms-build` (tier 3), `geodms-debug`, `geodms-perf` |
| [TECH_DEBT_REVIEW.md](TECH_DEBT_REVIEW.md) | The review of 2026-06-06 on branch `refactor_linux_gui`; its open items moved to the cleanup list (items 6, 15, 18 and 19) | [../cleanup-list.md](../cleanup-list.md) |

## Moved, not archived

These live documents moved on the same day. Code comments and older documents may still name the
old path.

| Old path | New path |
|---|---|
| `RECURSION_REFACTOR_PLAN.md` | [doc/development/recursion-refactor-plan.md](../development/recursion-refactor-plan.md) |
| `Transformation_complexity_plan.md` | [doc/development/transformation-complexity-plan.md](../development/transformation-complexity-plan.md) |
| `SaveLoadDesktop_findings.md` | [doc/development/save-load-desktop-findings.md](../development/save-load-desktop-findings.md) |
| `PORTING_STATUS.md` | [doc/linux/porting-status.md](../linux/porting-status.md) |
| `doc/development/schedule-with-lookahead.md` §8.1.x | [doc/archive/schedule-with-lookahead-log.md](schedule-with-lookahead-log.md) §8.1.x |

## Old paths still cited in the sources

Left as they are on purpose: a comment edit in a header that sits in a precompiled header rebuilds
most of the tree, and a testcase edit makes the shipped copy of the battery stale until the next
build. Change them together with the next real edit of each file.

| Cited document | Where |
|---|---|
| `schedule-with-lookahead.md` §8.1.x (about 70 comments, most without the file name) | `rtc/dll/src/mem/FixedAlloc.cpp`, `FixedAlloc.h`, `MyContainers.h`, `rtc/dll/src/tic/stg/StorageReadOperators.cpp`, and the operator and allocator sources |
| `std-ptr-migration-plan.md` | `rtc/dll/src/tic/DataController.cpp`, `TreeItemDualRef.h` (successor: `doc/development/ownership.md` §4), `TreeItem.cpp` (§11, now in the archive) |
| `Transformation_complexity_plan.md` | `rtc/dll/src/geom/Transform.h`, `shv/dll/src/Controllers.h`, `shv/dll/src/ViewPort.cpp` (the STUB comments the plan's status line counts) |
| `doc/code-fixes.md` | `geo/dll/src/Dijkstra.cpp`, `rtc/dll/src/dbg/Diagnostics.h`, `stg/dll/src/shp/ShpImp.cpp`; the headers of `testcases/combine_uint8_empty.dms`, `diversity_circle.dms`, `dyna_point_dist.dms`, `dyna_point_zero_dist_neg.dms`, `indirect_cycle_neg.dms` and `testcases/data/make_fixtures.py` |
| `k11-container-types-scope.md` | the header of `testcases/fn_test_network.dms` |

`doc/code-audit-2026-09-27.md` and the release notes name documents at the paths they had when
they were written; they are records and stay so.
