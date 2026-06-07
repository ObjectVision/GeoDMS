# Technical-Debt Review — GeoDMS

Branch reviewed: `refactor_linux_gui`. Date: 2026-06-06.
Sources: the code tree, `RECURSION_REFACTOR_PLAN.md`, `PORTING_STATUS.md`, the
May-2026 security audit, and a full-tree survey.

GeoDMS is a mature, high-performance spatial calculation engine. The debt here is
the normal accumulation of a long-lived, performance-critical C++ system written
before modern C++ and cross-platform expectations existed — not neglect. Items are
ranked by *leverage* (cost of leaving it × how much it blocks current goals), not
just size.

A caveat on provenance: some specific `file:line` citations come from audit notes
9–15 days old and from a survey agent that reads excerpts — treat exact line
numbers as leads to verify, not gospel. The structural conclusions were
cross-checked against the live tree.

---

## 1. Unbounded C-stack recursion — the load-bearing debt

The most consequential item: it blocks the headline goal (run on Linux at default
stack) and is a latent crash class in production today.

- Real iterated-calc configs stack **thousands** of Lisp/substitution frames. The
  engine compensates with a **64 MB reserved stack** (`DmsDef.props`
  `<StackReserveSize>67108864</StackReserveSize>`, mirrored as `/STACK` on Windows
  and `-Wl,-z,stack-size` on Linux). That reserve is a workaround papering over
  recursive descent in:
  - `AbstrCalculator::SubstituteExpr_impl` ↔ `slSupplierExprImpl` (the actual depth
    source — Open problem #1, fix **C1b** not yet started),
  - Boost Spirit V1 grammars (`ExprParse.h`, `ConfigParse.cpp`) — ~10 frames per
    nested production,
  - the `OnEnd` cascade and `runDirect` Join recursion in the scheduler.
- **Progress is real**: 17 commits converted many hot paths to explicit-stack /
  worklist drivers (A1/A2 post-order pattern, H1/H3). But the plan itself is candid
  that the reserve can't be removed until C1b + Spirit caps land and are validated.

**Severity: High.** A documented plan and reusable pattern already exist. The risk
is that this work is parked mid-stream — half-converted recursion plus a giant
stack reserve is a state that *looks* safe and silently isn't on deep configs or
smaller default stacks.

## 2. Platform-portability debt — scattered `#ifdef`, no clean OS seam

The reason `refactor_linux_gui` is hard is that Win32 leaks past the GUI layer.

- The visualization module (`shv/dll/src`) binds directly to GDI/Win32 —
  `DcHandle.h` is entirely `#ifdef _WIN32` over `HDC`/`HWND`/`SelectObject`/
  `DeleteObject`; `Win32ViewHost.*`, `dataview.h`. A `ViewHost` abstraction exists
  but isn't fully implemented for Qt.
- Windows-specific surfaces appear in non-GUI code too: registry access
  (`Environment.cpp`, `ConfigFileName.cpp`), ANSI path APIs (the issue-1101
  `_access` bug), `FILE_SHARE_DELETE` file maps.
- `PORTING_STATUS.md` exists precisely because this is tracked-but-incomplete.

**Severity: High** for the branch goal. The debt isn't the `#ifdef`s themselves —
it's the *absence of a single platform-abstraction seam*, so each port touches
dozens of files.

## 3. Dual build system (MSBuild **and** CMake) maintained in parallel

- A large MSBuild footprint (`all22.sln` + many `.vcxproj`, plus `DmsDef.props` /
  `DmsRelease.props` / `Directory.Build.props`) runs alongside **26**
  `CMakeLists.txt` + `CMakePresets.json`.
- Settings are duplicated and **drift** (the refactor plan repeatedly notes having
  to change stack size in *both* `DmsDef.props` and CMake; toolset pinned `v145` in
  props vs. CMake config). The recent commit "Pin GeoDmsGuiQt PlatformToolset=v145
  explicitly" is itself a drift-patch.
- vcpkg is pinned by baseline commit (boost 1.91 / gdal 3.12 / cgal 6.1), with
  in-repo submodule + overlay triplets.

**Severity: Medium-High.** Two build graphs mean every structural change is done
twice or one rots. Until Linux ships, both are needed — but a plan to make one
authoritative (CMake, generating or replacing MSBuild) would remove a recurring
tax.

## 4. Dependency debt — Boost Spirit V1

- Config and expression parsing use **Boost Spirit V1 ("classic")** —
  `BOOST_SPIRIT_USE_OLD_NAMESPACE`, `classic_symbols.hpp`, `SpiritTools.h`. Spirit
  V1 has been deprecated upstream for over a decade.
- It's also entangled with debt #1: its backtracking-with-actions semantics make
  even a simple depth cap (D2/D3) non-trivial, and a proper fix is a multi-week
  migration to X3 or hand-written recursive descent.

**Severity: Medium** (low churn, but a maintenance dead-end and a recursion-depth
contributor).

## 5. Hand-rolled core idioms predating modern C++

The codebase compiles as C++23 but carries a large pre-std substrate:

- Custom ownership/string/container types: `SharedPtr`/`WeakPtr` with bespoke
  `newly_obj` / `existing_obj` / `no_zombies` semantics, `SharedStr`, `TokenID`,
  custom `Cache`/`Assoc` containers, hand-rolled RTTI (`Class`/`ValueClassID` over
  string tokens).
- Macro-heavy debug layer: `dms_assert` compiles to `CC_ASSUME()` (a **no-op
  optimizer hint**) in Release. Several security-audit findings (#14 XML token
  buffer, others) depend on asserts that don't exist in shipping builds — so
  "asserted" invariants are unchecked in production.

**Severity: Medium.** Much of this is legitimately load-bearing for performance and
can't be naively swapped for `std::`. The real debt is that the custom semantics
raise the onboarding bar and the no-op-assert pattern hides input-validation gaps.

## 6. Testing & CI debt

- There **are** 5 test dirs (`rtc/tst`, `stg/tst`, `sym/tst`, `tic/tst`,
  `python/tst`) — but they're driven by `.bat`/`.sh` runners (`TestDebugUnit.bat`,
  `TestReleaseUnit.bat`, `RunGUITests.bat`) executed manually.
- **No CI**: the only `.github/workflow` is `jekyll-gh-pages.yml` (docs). There is
  no automated build/test on push, no coverage, and the validation loop in the
  refactor plan is "the user runs the batch file on a specific machine (OVSRV10)."
- Two pre-existing unit failures are knowingly carried (`DPGeneral_*` CRLF /
  `@projdir@` issues) — they desensitize the suite: a red baseline trains people to
  ignore red.

**Severity: Medium-High.** For a refactor wave touching the scheduler and parser,
the absence of automated regression gating is the single biggest multiplier on
every other risk here.

## 7. Security hardening backlog

From the May-2026 audit: ~24 findings, several P0/P1 fixed (GDAL VRT-Python/PAM
disabled, DLL search hardening, include-depth cap, auto-load path checks), but open
items remain — WMS TLS `verify_none` (MITM), GDAL driver allowlist gap,
integer-overflow / decompression-bomb in raster tile math, unrandomized temp-file
names. Many trace back to debt #5 (release-stripped asserts) and #4 (parser
robustness).

**Severity: Medium**, trending down — this one has active remediation momentum.

## 8. Repo hygiene

Working tree and root are cluttered with ~30 `msbuild_*.log` / `testdebugunit_*.log`
artifacts, `debug.log`, `linux_errors.txt`, `UpgradeLog.htm`, and build output dirs
at root. Low-stakes, but it obscures signal and risks committing transient state.

---

## Bottom line / suggested order

| # | Debt | Severity | Blocks |
|---|------|----------|--------|
| 1 | Unbounded recursion + 64 MB stack workaround | High | Linux, prod stability |
| 2 | Win32 leakage / no OS seam | High | Linux GUI |
| 6 | No CI, manual tests, red baseline | High | *everything else* |
| 3 | Dual MSBuild/CMake drift | Med-High | maintenance |
| 4 | Boost Spirit V1 | Medium | #1, future maint. |
| 5 | Pre-std idioms + no-op release asserts | Medium | onboarding, #7 |
| 7 | Security backlog | Medium↓ | (in progress) |
| 8 | Root log clutter | Low | hygiene |

The highest-leverage move isn't a feature — it's **#6**: stand up CI that builds
both configs and runs the existing 5 test dirs green (fix or quarantine the two
known failures first). Every other item here — the recursion conversions, the Linux
port, the Spirit migration — is being done *without an automated safety net*, which
is what makes each of them slow and risky. After that, the plan's own ordering
(land **C1b**, then Spirit depth caps, then drop the stack reserve) is the right
sequence for #1.
