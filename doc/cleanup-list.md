# Code cleanup and simplification backlog

This list comes from a repository-wide static review of commit `77000da1c` on
2026-09-13. It is ordered roughly by urgency and expected leverage. The first
group contains concrete defects or stale lifecycle behavior; the remaining
items are refactors that should be delivered in small, independently testable
changes.

Status as of 2026-09-13, after a first pass that took only the changes whose
sole observable effect is the defect they remove; each item carries its own
note below.

| Item | Status |
|---|---|
| 1, 4, 11 | done |
| 3, 5, 8, 12 | partly done: the defect or dead code is gone, the refactor around it is open |
| 2 | open: the `Engine`'s once-only construction is deliberate; making it re-constructible needs the Python harness |
| 6, 7, 9, 10, 13, 15, 16 | open |
| 14 | gated on the major version bump to 21 |

## Fix first

1. **Preserve Linux configuration overrides.**

   In `qtgui/exe/src/DmsOptions.cpp:905-915`, the non-Windows branch stores a
   non-session-only value as a session-local override and then immediately
   clears that same override. Keep the Linux fallback value and restrict the
   clearing step to the Windows persistent-storage path. Add an apply/read-back
   regression test.

   *Done 2026-09-13*: the clearing step is under `#ifdef _WIN32`. No read-back
   test was added; the dialog has no test harness yet.

2. **Make the Python `Engine` lifecycle RAII-safe.**

   `python/dll/src/Bindings.cpp:333-361` publishes
   `s_currSingleEngine` and registers a message callback before initialization
   has completed, while the destructor performs no cleanup. Publish the global
   pointer only after successful initialization, unregister the callback and
   clear the pointer during teardown, and define what happens if a `Config` is
   still alive. Test construction, destruction, and reconstruction, including
   an initialization failure.

3. **Make the options dialogs descriptor-driven.**

   Use one descriptor table to bind each option to its widget, persistence key,
   effective value, and apply/restore operation. This should eliminate the
   copy-paste mismatches in `qtgui/exe/src/DmsOptions.cpp`, including:

   - `changeNotCalculatedColor`, `changeScheduledColor`,
     `changeDataReadyColor`, and `changeDataStandbyColor` selecting the wrong
     color-option descriptions at lines 172-189.
   - `DmsLocalMachineOptionsWindow::cancel` completing with
     `QDialog::Accepted` instead of `QDialog::Rejected` at lines 530-534.
   - Repeated connect, save, restore, signal-blocking, and platform-precedence
     code.

   *Partly done 2026-09-13*: the two defects are fixed (each colour button now
   names its own option, `cancel` completes with `Rejected`). The descriptor
   table is still to do.

4. **Delete or rewrite `DmsYield`.**

   The Windows implementation in `rtc/dll/src/utl/Environment.cpp:188-201`
   calculates both timestamps from `currTime`, so its elapsed time is always
   zero. There are currently no call sites. Prefer deleting the declaration and
   both implementations; if it is retained, implement it once with
   `std::chrono::steady_clock`. Remove the unused `LocalAllocatedPtr` at lines
   136-141 at the same time.

   *Done 2026-09-13*: both deleted.

## High-leverage refactors

5. **Split `Environment.cpp` into common and platform-specific units.**

   Move shared status-flag parsing, caches, settings logic, and descriptor data
   to an `EnvironmentCommon.cpp`, leaving thin Windows and POSIX implementations
   for persistence, files, processes, and OS queries. Replace the two parallel
   `RegDWordAttr` arrays with one `std::array` keyed by `RegDWordEnum`, with a
   compile-time size check. `rtc/dll/src/utl/Registry.h:141-145` currently warns
   that the enum and both arrays must be maintained in lockstep.

   *Partly done 2026-09-13*: one `s_RegDWordAttrs` table in the common head of
   `Environment.cpp`, a `static_assert` against `RegDWordEnum::count`, and one
   `RTC_SetCachedDWord` in the common tail; only `RTC_GetRegDWord`, whose first
   read differs per platform, is still defined twice. The file split is still
   to do.

6. **Finish the `ViewHost` platform boundary.**

   `shv/dll/src/ViewHost.h` says that `DataView` should not depend directly on
   Win32, but `shv/dll/src/DataView.cpp` still contains many Win32 conditional
   blocks and nullable-host/`HWND` fallback pairs. Make the host mandatory for
   platform operations and isolate native message dispatch and GDI ownership in
   a dedicated adapter. Then either remove `Win32ViewHost`, whose implementation
   says it is not a shipping target, or make it the formal Windows adapter.
   Cover mouse capture, focus, timers, painting, caret behavior, clipboard, and
   teardown on Windows and Linux.

7. **Reduce `MainWindow::TheOne()` coupling incrementally.**

   The GUI currently has about 161 calls to the singleton. Start with
   `qtgui/exe/src/DmsTreeView.cpp:827-855`, where the context menu reaches into
   many public `MainWindow` actions. Inject the required action set or expose
   signals, then apply the same approach to the event log and view area. This
   will make child widgets independently testable and reduce lifecycle checks.

8. **Create one command-line parser shared by the GUI and runner.**

   Replace the independent parsing in `qtgui/exe/src/main_qt.cpp:74-134` and
   `run/exe/src/MainRun.cpp:194-338` with a parser that returns structured
   settings and supports executable-specific options through descriptors. It
   should reject missing arguments, unknown options, and unknown `@commands`.
   Remove the unused `itemCmd::file` enumerator and either implement or remove
   the accepted-but-placeholder `@histogram` and `@list` commands. Add
   parser-only tests for ordering, missing values, and invalid commands.

   *Partly done 2026-09-13*: `itemCmd::file` removed. The placeholders stay
   until the parser rejects unknown `@commands`: today an unmatched `@word` is
   silently skipped, so dropping them would turn `@histogram x` into a plain
   commit of `x` instead of a message.

9. **Collapse the GDAL type-conversion and geometry matrices.**

   `stg/dll/src/gdal/gdal_vect.cpp:2072-2171` contains a nested source-type by
   destination-type switch with many repeated tile operations and `goto ready`
   exits. Dispatch the source representation once, then use a target-type
   visitor for allocation and conversion. Similarly, merge the parallel XY and
   Z/M geometry traversal functions behind a coordinate-extractor policy. Lock
   down conversion, null, overflow, dimensionality, and geometry-shape behavior
   with table-driven tests before refactoring.

10. **Share Python binding implementations.**

    Const and mutable tree, unit, and data wrappers repeat most queries and
    pybind registrations in `python/dll/src/Bindings.cpp:398-823`. Introduce
    common wrapper traits and binding helpers, with mutable-only operations
    layered on top. Extract the duplicated integer/float bulk-read, scalar-read,
    and bulk-write loops into typed helpers.

11. **Use one non-copyable dynamic-library handle.**

    The Windows and POSIX `DllHandle` implementations in
    `rtc/dll/src/dllimp/RunDllProc.cpp:30-149` duplicate nearly all ownership and
    symbol-cache logic. Keep one movable, explicitly non-copyable RAII class and
    provide small platform primitives for open, symbol lookup, and close. This
    replaces runtime copy checks with compile-time ownership enforcement.

    *Done 2026-09-13*: one `DllHandle` with the copy operations deleted, over
    `dll_open`, `dll_sym` and `dll_close`. It is not movable; the only instance
    lives in the `std::map` cache and is constructed in place.

## Focused cleanup tasks

12. **Remove ambiguous algorithm state.**

    - In `clc/dll/src/OperMisc.cpp:446-478`, decide whether loop early
      termination is supported. The current `checkStopValue && false` branch can
      never stop but still resolves and validates `stopValue`. Either implement
      and test the feature or remove the dead branch and unused validation.
    - In `geo/dll/src/GridDist.cpp:145-173`, replace the three parallel border
      case vectors and separate count with a single
      `std::vector<BorderCase>`. This makes the index, distance, and edge
      invariant structural rather than conventional.

    *Partly done 2026-09-13*: the dead branch and its validation are removed;
    the loop instantiates every iteration. Note that the wiki's `Loop` page
    describes a `stop` condition the engine never honoured (the code looked
    for `stopValue`, behind `&& false`); implementing or dropping it is a
    separate decision. The `BorderCase` vector is left as is: the struct pads
    each case from 16.5 to 24 bytes, so it is a measured trade, not a pure
    cleanup.

13. **Centralize test-battery orchestration.**

    Extract the repeated unit-suite, testcase, XML-roundtrip, shipped-content,
    and result-reporting blocks from `batch/TestDebugUnit.bat`,
    `batch/TestReleaseUnit.bat`, `batch/TestGlobioDebugUnit.bat`, and
    `batch/TestGlobioReleaseUnit.bat` into one parameterized helper. Keep the
    existing launchers as thin interactive entry points and preserve the
    repository rule that agents do not launch them headlessly. Update the
    corresponding repository skill whenever the launcher contract changes.

14. **Prepare the version-21 compatibility cleanup.**

    When the major version is changed from 20 to 21, remove the compile-time
    tripwires and their obsolete compatibility paths together:

    - `PartNr` in `geo/dll/src/ConnectedParts.cpp`.
    - The obsolete `subset` form in `clc/dll/src/Subset.cpp`.
    - The `dijkstra_*` stubs in `geo/dll/src/Dijkstra.cpp`.
    - The `claim_*` stubs in `geo/dll/src/DiscrAlloc.cpp`.

    Do not perform this removal while the current major version is still 20.

## Longer-term structural work

15. **Complete iterative expression substitution before reducing stack sizes.**

    `rtc/dll/src/tic/AbstrCalculator.cpp` still has mutually recursive supplier
    and substitution traversal. `RECURSION_REFACTOR_PLAN.md:148-154` identifies
    the unfinished fused iterative driver. Complete and stress-test that work
    before reducing the 64 MB executable stack reserve in `DmsDef.props` and
    the CMake executable definitions.

16. **Ratchet compiler warnings by module.**

    The non-MSVC build currently suppresses several high-volume warning classes
    globally in `CMakeLists.txt:93-120`. Clean one module at a time, remove the
    corresponding local suppressions, and enable warnings-as-errors only for
    modules that have reached a clean baseline. Avoid a repository-wide warning
    rewrite that obscures behavioral changes.

## Suggested delivery order

1. Ship items 1-4 as small correctness and dead-code changes (done, except the
   Python `Engine` lifecycle and the dialog descriptor table).
2. Refactor the settings/platform layer and command-line parser (items 5 and 8).
3. Improve GUI boundaries incrementally (items 6 and 7).
4. Refactor the GDAL and Python type-driven code only after strengthening their
   focused tests (items 9 and 10).
5. Take the remaining focused and release-gated work independently so each
   change stays reviewable and reversible.
