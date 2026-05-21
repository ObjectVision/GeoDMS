# Recursion-elimination refactor plan & state

Status snapshot — 2026-05-21 — branch `refactor_linux_gui`.

This document captures the state of an in-progress refactor wave aimed at
removing unbounded C-stack recursion from GeoDMS so the binary eventually
runs at MSVC's default 1 MB stack (currently 64 MB reserved). The longer-
term goal is **memory-aware scheduling that adapts as execution-time
information becomes available** — the iterative DAG drivers are the
infrastructure that lets the OC scheduler see the full task DAG and
per-task resource estimates before committing tasks to run.

Resume on a different machine (OVSRV10): pull this branch and read this
file. The next steps and outstanding design problems are at the bottom.

---

## Commits landed on `refactor_linux_gui`

The 17 commits below are all clean local builds; many were validated by
`TestDebugUnit` (the user runs that — Claude's harness can't drive the
batch file through the PowerShell→cmd boundary, see
`memory/feedback_test_validation.md` for details).

| # | Commit | Subject |
|---|---|---|
| 1 | `e0675e05` | OperationContext_scheduleThis: recursion → worklist (R1) |
| 2 | `b0c497ca` | prioritize_impl: recursion → worklist |
| 3 | `2b6524aa` | AbstrCalculator::SubstituteArgs: tail-recursion → loop (C1a) |
| 4 | `b55d2bb5` | XmlParser::ReadEncl: recursion → explicit stack (D1) |
| 5 | `57d30e5e` | Actor::SuspendibleUpdate: post-order driver (A1) |
| 6 | `4a7e4f72` | Actor::UpdateMetaInfo: post-order driver (A2) |
| 7 | `1350254c` | TreeItem_VisitConstVisibleSubTree: explicit stack (F2) |
| 8 | `dfd595bf` | FuncDC::MakeResult: arg-DAG drain (R2 Phase 1) |
| 9 | `806f7dec` | FuncDC::CallCalcResultImpl: arg-DAG drain (R2 Phase 2) |
| 10 | `aa305410` | OperationContext::EstimateRamUsage hook (R2 Phase 5) |
| 11 | `69e3b139` | TreeItem: remove 320 KB stack batons (reverted in 13) |
| 12 | `7d7a0b3b` | Remove 64 MB stack-reserve override (reverted in 13) |
| 13 | `d635a50f` | Restore 64 MB stack reserve + TreeItem batons; iterated-calc depths matter |
| 14 | `5c61e71f` | Linux: -Wl,-z,stack-size=67108864 for GeoDmsRun and GeoDmsGuiQt |
| 15 | `127d0415` | Assoc::ApplyOnce / ApplyMany: recursion → result-stitching worklist (H3) |
| 16 | `b07b954a` | AssocList_RepApplyTopEnvList: tail-recursion → loop (partial H1) |
| 17 | `43c4bbd4` | ApplyTopEnv: iterative outer rewrite-chain loop (full H1) |

### Validation status

- Commits 1-10 + 13 + 14 + 15-17: clean `TestDebugUnit` (only the two
  pre-existing failures `DPGeneral_explicit_supplier_error` and
  `DPGeneral_missing_file_error` — line-ending CRLF↔LF mismatch + the
  `@projdir@` placeholder substitution. Unrelated to this work.)
- 11 (`69e3b139` baton removal) and 12 (`7d7a0b3b` /STACK removal):
  superseded by 13 (`d635a50f`) which restores both. The supplier-DAG
  drivers in 5/6 are bounded but other recursion paths (see "Open
  problems" below) still grow with iterated-calc depth, so the 64 MB
  reserve and the std::async batons remain in place.

Note: commits 15-18 were rebased onto upstream after the initial push
(remote had three commits ahead at fetch time). The hashes above
reflect post-rebase IDs; the commit subjects and ordering are
unchanged. Future rebases / merges may shift these hashes again.

### Build context

- `DmsDef.props` global setting: `<StackReserveSize>67108864</StackReserveSize>` is back.
- `DmsRun.vcxproj` and `DmsPython.vcxproj` keep their explicit per-project
  StackReserveSize overrides REMOVED (they inherit from `DmsDef.props` now).
- `run/exe/CMakeLists.txt` and `qtgui/exe/CMakeLists.txt` restored the
  Windows `target_link_options(..., /STACK:67108864,8192)` and added a
  Linux parity `-Wl,-z,stack-size=67108864` for `UNIX AND NOT APPLE`.

---

## The post-order driver pattern (A1/A2 template)

Used in commits 5, 6, 8, 9. Shape:

1. `thread_local bool s_inXxxDrain = false;` — reentry latch
2. Outermost call (latch false): pre-walk the relevant DAG via an explicit
   stack (`std::vector<frame_t>` with `(actor, children, nextChild)`)
   using a `visited` set; collect into a post-order `std::vector`.
3. Set the latch, register `make_scoped_exit` to clear it.
4. Drain: call the virtual method on each item in post-order.
5. Fall through to run self's body.
6. Nested calls (latch true) skip the drain-build and run the body
   directly; they hit the body's early-exit guard (`progress >= MetaInfo`,
   `m_Data` populated, OC.status != none, etc.) thanks to post-order.

Already-iterative templates in the codebase to reuse:
- `ListObj::~ListObj` at `sym/dll/src/LispRef.cpp:767-792` (zombie_destroyer_stack)
- `RewriteExprList` at `tic/dll/src/ExprRewrite.cpp:80-118`

## The iterative-outer-loop pattern (H1 template)

Used in commit 17. Shape:

1. Add `lookup()` and `store()` methods to the cache (`set/Cache.h` —
   `UnorderedMapCache`) that decouple lookup from compute under the cache's
   `recursive_mutex`.
2. Rewrite the cache-entry function (`ApplyTopEnv`) as an iterative loop:
   - `lookup` → on hit, set current to cached and break.
   - Inline the rule-matching logic (originally in the cache's m_Func).
   - On match, call the recursive helper (`AssocList_RepApplyTopEnv`) to
     produce the substituted result; loop with that as the new current.
3. After loop: cache write-back. Original arg AND all intermediate exprs
   in the chain map to the fixed-point result.
4. The inner recursive helper stays recursive but is bounded by the rule
   template tree depth (typically 3-5).

This pattern can be applied to other cache+functor combinations.

---

## Open problems

### Open problem 1: SubstituteExpr_impl ↔ slSupplierExprImpl chain — the actual iterated-calc depth source

The user noted that iterated-calc configs stack thousands of Lisp levels in
substitution. My initial framing assumed this lived in the `ApplyTopEnv`
rewrite path, which H1 (commit 17) addresses. **It does not.**

The actual depth chain, discovered late in the session:

```
SubstituteExpr_impl(expr)                       [AbstrCalculator.cpp:1265]
  → slSupplierExpr(symbol)                       [AbstrCalculator.cpp:856]
    → slSupplierExprImpl(supplier)               [AbstrCalculator.cpp:895]
      → supplier->GetCheckedKeyExpr()            [TreeItem.cpp:2456]
        → TreeItem_GetCheckedDC_impl
          → self->UpdateDC()                     [TreeItem.cpp:2402]
            → GetOrgDC()
              → GetCurrMetaInfo(...)             [TreeItem.cpp:2260]
                → calc->GetMetaInfo()            [AbstrCalculator.cpp:1481]
                  → SubstituteExpr(...)          [AbstrCalculator.cpp:1437]
                    → SubstituteExpr_impl(...)   ← RECURSES HERE
```

`AbstrCalculator::GetMetaInfo()` has a one-shot guard
(`m_HasSubstituted`), so each calculator substitutes exactly once. But on
the first traversal of an iterated-calc chain (step1000 → step999 → … →
step1), each level triggers the next via `slSupplierExprImpl`. The
recursion depth equals the supplier-chain depth.

**Why H1 doesn't fix this**: H1 iterativizes the `ApplyTopEnv` rewrite
chain, which only fires for `RewriteExprTop` calls at the leaves of the
substitution. The recursive descent through suppliers happens BEFORE any
`RewriteExprTop` runs.

**The fix** (C1b, not yet started): a fused iterative driver covering
`SubstituteExpr_impl` + `SubstituteArgs` + `slSupplierExprImpl` together.
Two reasonable approaches:

1. **Topological pre-walk**: before processing the root, walk the supplier
   DAG bottom-up (suppliers first) and substitute each calculator's
   expression in topological order. Substantially mirrors A1/A2.

2. **Worklist driver** (similar shape to H1): a frame stack with kinds
   like `kSubstituteExpr`, `kSubstituteArgs`, `kSlSupplierExpr`,
   `kStitchOpCall`, etc., processed iteratively with results threaded
   through slots.

Approach 1 is simpler and likely correct (since DAG topology is
well-defined). Approach 2 is more invasive but generalizes better.

### Open problem 2: AssocList_RepApplyTopEnv recursive template walk

H1's iterative outer loop bounds the chain of `ApplyTopEnv` invocations.
Each iteration calls `AssocList_RepApplyTopEnv` once on the matched rule's
template. That function still recurses through the template tree.

For typical rule templates (depth 3-5) this is fine. If any config has
unusually deep templates, depth scales with that.

Not load-bearing for iterated-calc. Defer unless profiler shows real
overflows.

### Open problem 3: OnEnd cascade

`OperationContext::OnEnd → onEnd → separateResources → disconnect_waiters
→ disconnect_supplier` enqueues `OnEnd` lambdas into a `garbage_can`. When
the garbage_can is destroyed (outside `cs_ThreadMessing`), the lambdas
fire → another OnEnd → another disconnect_waiters → …

Depth = waiter chain length. Per-task-completion path; fires on every
query that produces results. Not a known crash source but unbounded in
theory.

**Fix** (OC #3, task #7 in the in-session task list): per-thread
propagation queue at the outermost OnEnd. First entry installs the queue;
inner OnEnd lambdas append `(waiter, status)` instead of self-firing. The
outermost OnEnd drains.

### Open problem 4: Phase 4 — inline runDirect Join recursion

`Schedule(runDirect=true)` (`OperationContext.cpp:1288-1318`) forces
non-parallel operators onto the meta-thread's C stack via
`JoinSupplOrSuspendTrigger → supplier.Join → recurse`. Phases 1-3 don't
fix this chain.

**Fix**: replace inline `Join` recursion with a *pump-the-scheduler* loop
on the meta-thread. `JoinSupplOrSuspendTrigger` becomes "set fence, pump
runnable tasks until target task is `done` or `suspended`".

High risk; touches the most load-bearing scheduling logic. Existing
`OperationContext::Join` at `:2254` already implements cooperative
work-stealing — Phase 4 generalizes that to all runDirect paths.

### Open problem 5: Spirit V1 grammars (D2/D3)

`stx/dll/src/ConfigParse.cpp` (`item` rule recursion via container body)
and `stx/dll/src/ExprParse.h` (`expression` rule recursion via parens at
line 220) are Boost Spirit V1 recursive-descent grammars. Each recursion
level burns ~10 frames (one per grammar production in the chain).

The user noted nested item definitions in configs can compound to
hundreds of source-level depth → thousands of grammar frames. With 1 MB
stack this overflows.

**Fix (pragmatic, D2/D3)**: add a thread_local depth counter incremented
in actions on `LBRACE`/`LPAREN`, decremented on `RBRACE`/`RPAREN`. Throw
on exceeding ~256.

Caveat: Spirit V1's backtracking can leave the counter incremented if a
production matches the opening token but later fails. In the GeoDMS
grammars the openings are inside `assert_d` guards that throw rather than
backtrack, so this should be safe in practice — verify per callsite.

**Fix (proper)**: migrate from Spirit V1 to Spirit X3 or hand-written
recursive descent with a heap-allocated parse stack. Multi-week.

### Open problem 6: H2 — Match → loop

`sym/dll/src/Lispeval.cpp:199-216` (`Match`) is mutual-recursive on Lisp
pattern depth. Used from `ApplyTopEnvFunc::operator()` at line 524 (still
present although `operator()` is no longer called from cache.apply after
commit 17 — the inlined rule-matching in `ApplyTopEnv` calls `Match`
directly).

Config-time only; pattern depths are small. Low priority.

### Open problem 7: sym H-track other items

Tasks #12 (full H1 — done), #13 (H2 Match), #14 (H3 — done), #15 (I1
Parser GetExpr/GetExprList) are the remaining sym/ items. #13 and #15
are config-load only.

### Open problem 8: real Operator::EstimateRamUsage estimators

Commit 10 added the `EstimateRamUsage()` hook returning 0 (= unknown,
preserves pre-Phase-5 behavior). To deliver actual adaptive scheduling,
plug in real estimators for FuncDC subtypes — typically
`domain_cardinality × value_type_size` for data-producing operators.

Sources:
- `dynamic_cast<const AbstrDataItem*>(m_Result.get_ptr())`
- `adi->GetAbstrDomainUnit()->GetCount()`
- `adi->GetAbstrValuesUnit()->GetValueType()->GetSize()`

Skip / return 0 when the result is a unit, container, or undefined-
cardinality domain (passors). The activation loop at
`OperationContext.cpp:1017-1031` already consumes the estimate via
`SufficientFreeSpace(estimate)`.

---

## Recommended ordering for resumption on OVSRV10

1. **Run the full Release test suite** to validate commits 1-10, 13-17
   on representative iterated-calc configs at the current (64 MB)
   stack reserve. This confirms no regressions in what's landed.

2. **C1b — fused SubstituteExpr_impl driver** (Open problem 1). This is
   the load-bearing fix for iterated-calc depth. Approach 1 (topological
   pre-walk of supplier DAG, substitute calculators bottom-up) is
   recommended — it mirrors A1/A2 and is the actual structural fix.

3. **D2/D3 — Spirit grammar depth caps** (Open problem 5). Pragmatic
   defense-in-depth; ~30 lines.

4. **Validate again** with /STACK removed once C1b + D2/D3 land.

5. **Phase 4 — pump-the-scheduler** if Phase 5 (real EstimateRamUsage
   estimators) needs the inline path to be iterative. Otherwise defer.

6. **Plug in real EstimateRamUsage estimators** per FuncDC subtype to
   activate the memory-aware scheduling pathway.

---

## Tooling notes

- The user runs `TestDebugUnit.bat` themselves; Claude's PowerShell→cmd
  invocations fail because `cd ..\tst\batch` + `Call unit.bat` doesn't
  carry through the shell boundary. Validation on OVSRV10 happens via
  `TestReleaseUnit`.
- Build verifies via `msbuild` on the relevant `*.vcxproj` (e.g.,
  `tic/dll/DmTic.vcxproj`, `rtc/dll/DmRtc.vcxproj`, `sym/dll/DmSym.vcxproj`).
- The OperationContext cache (`g_applyTopEnvCache`) is now
  `UnorderedMapCache<ApplyTopEnvFunc>` but `apply()` has no callers
  post-commit-17 — the iterative driver uses `lookup()` and `store()`
  directly. `ApplyTopEnvFunc::operator()` is dead code; can be cleaned
  up in a follow-up.
