# Code cleanup and simplification backlog

*Status (2026-10-06): this is the one live backlog of engine cleanup (code audit PLN-A12). On 2026-10-06
it took over the six items that `doc/code-fixes.md` still had open (item 17) and the open items of
`TECH_DEBT_REVIEW.md`: recursion (added to item 15), an automated gate (item 18) and the security
hardening of the May 2026 audit (item 19); both documents are now in `doc/archive/`. Done: 4, and 11 on
code that turned out to be unreachable. Partly done: 1, 3, 5, 8, 12. Open: 2, 6, 7, 9, 10, 13, 15 to 19.
Gated on version 21: 14. The open findings of `doc/code-audit-2026-09-27.md` stay in that document and the
open GitHub issues in `doc/issues.md`; which item to take next across all of them is ranked in
`doc/continuations-2026-10-06.md`. Until 2026-10-06 the table below called item 1 done, while on Linux an
override is still kept in the session cache only, and item 5's note named `RTC_GetRegDWord` as the only
function still defined twice.*

This list comes from a repository-wide static review of commit `77000da1c` on
2026-09-13. It is ordered roughly by urgency and expected leverage. The first
group contains concrete defects or stale lifecycle behavior; the remaining
items are refactors that should be delivered in small, independently testable
changes.

Status as of 2026-09-13, after a first pass that took only the changes whose
sole observable effect is the defect they remove; each item carries its own
note below.

| Item | Status (updated 2026-10-06) |
|---|---|
| 4 | done |
| 11 | done, but the unit it cleaned is unreachable (code audit INF-A13) |
| 1 | partly done: right on Windows; on Linux an override is still lost at exit |
| 3, 5, 8, 12 | partly done: the defect or dead code is gone, the refactor around it is open |
| 2 | open: the `Engine`'s once-only construction is deliberate; making it re-constructible needs the Python harness |
| 6, 7, 9, 10, 13, 15, 16 | open |
| 17, 18, 19 | open: carried over on 2026-10-06 from `doc/archive/code-fixes.md`, `doc/archive/TECH_DEBT_REVIEW.md` and the May 2026 security audit |
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

   *Only partly done (2026-10-06)*: on Linux the apply path still keeps an
   override that is not session-only in the session-local cache alone
   (`SetSessionLocalOverride` in the `#else` branch at `DmsOptions.cpp:915-918`),
   so it is lost at exit. The POSIX half of `Environment.cpp` has a persistent
   store, `~/.config/geodms/geodms.ini`, which this path does not use (code
   audit, section 4.9).

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

   *Correction (2026-10-06)*: not only `RTC_GetRegDWord` (`:753`, `:2845`):
   `RTC_ParseRegStatusFlag` is defined twice as well (`Environment.cpp:695` and
   `:2792`), and so is the session-override family (`:381` and `:2546`). Code
   audit PLN-A13 counts 64 functions defined in both halves, so a new `/S` flag
   has to be added twice or one platform silently drops it.

6. **Finish the `ViewHost` platform boundary.**

   `shv/dll/src/ViewHost.h` says that `DataView` should not depend directly on
   Win32, but `shv/dll/src/DataView.cpp` still contains many Win32 conditional
   blocks and nullable-host/`HWND` fallback pairs. Make the host mandatory for
   platform operations and isolate native message dispatch and GDI ownership in
   a dedicated adapter. Then either remove `Win32ViewHost`, whose implementation
   says it is not a shipping target, or make it the formal Windows adapter.
   Cover mouse capture, focus, timers, painting, caret behavior, clipboard, and
   teardown on Windows and Linux.

   *Note (2026-10-06)*: Qt is the only host. `Win32ViewHost` is compiled but
   never constructed (only its own constructor names it), so removing it is
   the cheap half (dead code: continuations B7; the boundary: code audit
   SHV-A20).

   *Done (2026-10-06)*: `Win32ViewHost` is deleted (c0737bfe8). The boundary of
   SHV-A20 is open.

7. **Reduce `MainWindow::TheOne()` coupling incrementally.**

   The GUI currently has about 161 calls to the singleton (recounted on
   2026-10-06: 164 in `qtgui/exe/src`, that is 170 occurrences of `TheOne()`
   less the declaration, the definition and four in comments). Start with
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
   commit of `x` instead of a message. Both are still there on 2026-10-06
   (`run/exe/src/MainRun.cpp:388-393`).

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

    *Correction (2026-10-06)*: done, but on dead code. The only caller of
    `RunDllProc`, in `clc/dll/src/OperExec.cpp`, sits behind the commented-out
    `//#define OPER_EXECDLL` (:26), so the whole unit is unreachable (code audit
    INF-A13; continuations B7 deletes it).

    *Deleted (2026-10-06)*: `dllimp/RunDllProc`, `DllHandle` included, and the
    `exec_dll` block of `OperExec.cpp` (92e3a541e).

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
    - `GetGeosNonDPointDeprecationFlag()` in `geo/dll/src/BoostPolygon.cpp`
      (:106), which throws when the major version is above 20. The constructor
      of the `geos_` overlay operators calls it for every non-DPoint
      instance (:326), and `geos_polygon_connectivity` is still instantiated
      for all point types, so at the bump those instances would throw while
      `Geo.dll` initialises (code audit GEO-A46: instantiate DPoint only).
    - The twelve kernel-suffix families of `bp_` polygon operators
      (`_i4HV` to `_dXD`, simple and split; 48 names per code audit PLN-A14),
      deprecated since #917 in favour of `bp_minkowski_sum` and
      `bp_minkowski_difference` (`BoostPolygon.cpp`, `BpPolyOperatorGroupss`
      with `isDeprecatedKernelSuffix`).

    Do not perform this removal while the current major version is still 20.

## Longer-term structural work

15. **Complete iterative expression substitution before reducing stack sizes.**

    `rtc/dll/src/tic/AbstrCalculator.cpp` still has mutually recursive supplier
    and substitution traversal. `doc/development/recursion-refactor-plan.md`
    (C1b, open problem 1) identifies the unfinished fused iterative driver.
    Complete and stress-test that work before reducing the 64 MB executable
    stack reserve in `DmsDef.props` and the CMake executable definitions.

    *Also carries `TECH_DEBT_REVIEW.md` #1 (2026-10-06)*: the protection is
    still the 64 MB reserve (`DmsDef.props`, `StackReserveSize` 67108864) with
    the 320 KB `std::async` hand-offs. Seven of the seventeen commits the plan
    listed as landed were reverted on 2026-05-22, C1b never started, and its
    target grew: `SubstituteExpr_impl` has 13 call sites now, against 4 when
    the plan was written. The Boost Spirit V1 grammars of `stx/dll/src` have no
    depth cap either (D2/D3 of the plan, item #21 of the security audit under
    item 19). Order, measuring first, as in continuations C1: a deep
    iterated-calc testcase with a counter of the hand-offs, the D2/D3 caps,
    one function-application dispatch in `SubstituteExpr(_impl)` instead of
    three, then C1b. Code audit PLN-A02, the one High finding of that audit
    still open.

16. **Ratchet compiler warnings by module.**

    The non-MSVC build currently suppresses several high-volume warning classes
    globally in `CMakeLists.txt:93-120`. Clean one module at a time, remove the
    corresponding local suppressions, and enable warnings-as-errors only for
    modules that have reached a clean baseline. Avoid a repository-wide warning
    rewrite that obscures behavioral changes.

## Carried over on 2026-10-06

17. **The six items `doc/code-fixes.md` still had open** (now
    `doc/archive/code-fixes.md`; the IDs are that document's).

    - **RTC-70 §9**: `Actor::DecInterestCount` (`rtc/dll/src/act/Actor.cpp:1270-1280`)
      still returns early when the interest count is already 0; since
      2026-09-05 it says so once per process, as a warning. Its cause, an
      intrusive interest holder that outlives the count reset of a
      std-managed TreeItem, goes only with the split of §9 of
      `doc/archive/std-ptr-migration-plan.md`. Effort L.
    - **RTC-12 / RTC-C14 / TIC-10**: `OperationContext::separateResources`
      (`rtc/dll/src/tic/OperationContext.cpp:2097`) calls `MemoryLedger_Retain`
      and `MemoryLedger_Release` outside any `try`, and is reached from the
      `noexcept` `OperationContext::onEnd` (:2028), so a throw there ends the
      process. Catch on that path, or make those calls `noexcept` (code audit,
      section 4.3; continuations A7).
    - **STG-14 residue**: `ReadTiles` keeps the `SizeT` that `TifImp::ReadTile`
      returns in an `Int32 read_result` (`stg/dll/src/GridStorageManager.h:185`,
      :193). Use `SizeT`, or an explicit checked conversion. It goes with the
      `UInt32 tile_wh` of the same function (item 19, #6; continuations A5).
    - **The `dyna_point` `carry`** (C2): in `geo/dll/src/OperPolygon.cpp`
      `carry` is not reset when a new polyline starts, so without `withEnds` the
      sampling runs on across polylines; the comment at :1272-1274 says so, and
      both passes agree. Resetting it changes results: a semantics decision,
      with a wiki note if it changes.
    - **INF-A05** (code audit): in XML element text an unknown or numeric
      entity reference (`&#169;`, `&#10;`, `&nbsp;`) decodes to an embedded NUL,
      which truncates every `c_str()` consumer (`XmlParser::TransformChar` and
      `SymbolGetChar`, `rtc/dll/src/xml/XmlParser.cpp:130-155`, :544-551).
      Decode numeric references with the decoder of `utl/Encodes.cpp`
      (`html::ParseNumericRef`, `append_utf8`) and report an unknown name.
    - **The last "activated" assert**: `Actor::SetProgress`
      (`rtc/dll/src/act/Actor.cpp:318`) keeps a commented-out
      `dms_assert(ps >= m_State.GetProgress())`, "activated at 21-08-2012, see
      if it holds". The four other sites of that family are gone. Make it a
      check or delete the line.

18. **An automated gate** (`TECH_DEBT_REVIEW.md` #6).

    `.github/workflows` still holds only `jekyll-gh-pages.yml`: nothing builds
    or tests on a push. The testcases battery, the XML round trip and the
    source checks (`batch/run_source_checks.bat`) run only from the launchers
    and setup scripts on the developers' machines. A self-hosted Windows
    runner, or a scheduled local run of those three, would gate every commit;
    it has to respect the pinned 14.50 toolset, the in-repository vcpkg and
    the machine rules of AGENTS.md (continuations C6). The other half of #6, a
    red baseline of unit failures carried knowingly, is no longer accepted:
    `batch/run_unit_suite.bat` fails on any `FAILED` or `not OK` line of the
    aggregate.

19. **Security hardening: the open items of the May 2026 audit**
    (`TECH_DEBT_REVIEW.md` #7).

    That audit (2026-05-11, on branch `refactor_linux_gui`) was kept in a
    private note, not in the repository, so its open items are listed here
    under its own numbers, with the code they concern as it stands on
    2026-10-06. Done since: #2 and #4 (GDAL VRT Python and PAM side files off)
    and #16 (DLL search path) in 2dfd392d8; #5 (the `LastConfigFile`
    auto-load) in 3e9a983d2 and 3bb5f00d8; #8 (tile bounds checked before
    `RasterIO`); #11 and #12 (include path compare and include depth cap) in
    0c60df1d8 and 2cb37b32c; #13 (XML element nesting) in b55d2bb50; #14 (the
    XML entity buffer) in 3d0db896e; and most of #23 (WMS loaders at teardown,
    a stalled tile abandoned after 20 s) in 9ebc86f31.

    - **#1 and #10, WMS TLS**: `shv/dll/src/WmsLayer.cpp` sets
      `ssl::verify_none` (:133, :180, :532, :536) and builds its contexts with
      `context::sslv23` (:158, :530), so the server certificate is not checked
      and a man in the middle can feed tiles to the GDAL decoders. Verify
      against the system root store, with `tls_client` and TLS 1.2 at least
      (continuations B10).
    - **#3 and #15, GDAL driver choice**: for a file extension it does not
      know, `stg/dll/src/gdal/gdal_base.cpp` registers every driver GDAL was
      built with (`GDALRegisterAllDriversOnce`, :1808); and a storage name may
      start with `/vsicurl/`, `/vsis3/` or `/vsizip/`, for which there is no
      policy. An allowlist of drivers, and a decision on the virtual file
      systems (continuations B10).
    - **#7, WMTS tile size**: `ReadBand` (`stg/dll/src/gdal/gdal_grid.cpp:800`)
      sizes its buffer to the width and height the tile file declares, with no
      cap.
    - **#6, remainder**: `UInt32 tile_wh = tw_aligned*tileSize.Y()` in
      `stg/dll/src/GridStorageManager.h` (:153 for reading, :535 for writing)
      can wrap; compute in `SizeT` and check.
    - **#9**: the band indices of a band specification are parsed with
      `std::stoi` (`gdal_grid.cpp:232`, :237, and a subdataset dimension at
      :634) without a range check, and an empty or overlong number throws a
      standard exception instead of a located error.
    - **#17 to #19, installer**: the Qt plugin folders (`imageformats`,
      `platforms`) and `uninstaller.exe` are written under `$INSTDIR`
      (`nsi/DmsSetupScript.nsh`, `nsi/DmsSetupScriptX64-cmake.nsi`), which is
      safe only while the ACL of the installation folder stays admin-only, and
      nothing verifies the signatures of the installed DLLs.
    - **#20**: `CreateFileHandleForRwView` opens read-write file maps with
      `FILE_SHARE_DELETE` (`rtc/dll/src/ser/FileMapHandle.cpp:138`), so another
      process can delete or rename a store while it is mapped.
    - **#21**: the expression and configuration parsers have no depth cap;
      see item 15 (D2/D3).
    - **#22**: the TokenID table grows with every name a configuration or a
      data source introduces, and nothing ever removes an entry
      (`rtc/dll/src/set/IndexedStrings.cpp`).
    - **#24**: a temporary file is created under the name its caller supplies
      (`CreateFileHandleForRwView`, `FileMapHandle.cpp:124`), not under a
      random one.

## Suggested delivery order

*2026-10-06: superseded by the ranking in `doc/continuations-2026-10-06.md`,
which orders these items together with the open findings of the code audit
and the open issues. The order below is the one of 2026-09-13.*

1. Ship items 1-4 as small correctness and dead-code changes (done, except the
   Python `Engine` lifecycle and the dialog descriptor table).
2. Refactor the settings/platform layer and command-line parser (items 5 and 8).
3. Improve GUI boundaries incrementally (items 6 and 7).
4. Refactor the GDAL and Python type-driven code only after strengthening their
   focused tests (items 9 and 10).
5. Take the remaining focused and release-gated work independently so each
   change stays reviewable and reversible.
