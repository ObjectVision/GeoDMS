# IntegrityCheck folding and exact, memoized dedup (#1180, #1182)

Design note for the check-set memoization that replaces the depth-bounded redundant-guard
search. Companion to the #1180 commit series (`9b76395b` ancestor guarding, `8ef86c09` fold
into checked expressions, `e36611c4` fold every GetCheckedKeyExpr exit, `42441a58`
RewriteExpr hoist rules, `abaf2447` depth-bounded skip); this document records why #1182 is
solved at DataController granularity and how.

## Problem

Since #1180, every item under a checked ancestor carries `(IntegrityCheck <expr> <cond>)`
guards: TreeItem_CreateCheckedExpr folds the checks of the item and of its ancestors into the
checked key expression, and consumers reference items through GetCheckedKeyExpr, so anything
below a checked ancestor carries its guard. Unmitigated, one root check multiplied into 3,393
CheckOperator applications on t720 (2BURP): every item under the checked ancestor is wrapped,
AND every reference between two such items embeds another copy. The `abaf2447` mitigation —
`TreeItem_ExprEnforcesCheck`, a recursive containment search bounded at 4 argument levels —
got it to 452, but missed redundancy deeper than its bound and re-scanned substituted trees on
every fold.

Issue #1182 asks for exact memoization of "which checks does evaluating this LispRef imply",
so each check is included once per calculation DAG. The issue sketched

```cpp
using check_set = std::set<LispRef>;
std::map<LispPtr, check_set> s_MemoizisedCheckSetsMap;
```

which is inefficient and hazardous in three ways:

1. nearly every interned Lisp node reachable from a checked expression would get an entry;
2. many downstream nodes imply the same set, so the map would hold thousands of duplicate
   sets — at minimum it should hold `SharedThingPtr<check_set>` (ptr/SharedObj.h) with
   deduplication when merging argument sets;
3. weak `LispPtr` keys need eviction when a LispObj dies, and no such hook exists: the leaf
   dtors hand-remove themselves from their own interning caches only, and the `ListObj` dtor
   cascade (`s_LispObjStackActive`, sym/LispRef.cpp) bypasses descendant dtor bodies entirely.
   Every durable Lisp-keyed map in the codebase (`s_DcMap`, `g_applyTopEnvCache`,
   `FunctionChecker::m_ApplTypes` with its explicit ABA comment) uses strong `LispRef` keys
   instead — but a strong-keyed global map would retain every substituted expression for the
   whole session.

## Why DataController granularity

The proposal "memoize only expressions that pass slSupplierExprImpl, store the unioned set
when AbstrCalculator returns" has the right granularity but the wrong anchor:

- `registerSupplier` fires *before* the `subst_never`/passor early-out that embeds an
  *unchecked* `(sourceDescr …)` tree (AbstrCalculator.cpp, slSupplierExprImpl), so a union
  over `SubstitutionBuffer::m_SupplierSet` would claim checks the built expression does not
  contain — an overcount, i.e. a skipped wrap that nothing enforces: a silently missed check.
- `scope` and `arrow` handlers substitute through fresh SubstitutionBuffers that do not
  inherit `m_CollectSuppliers` — an undercount.
- `MetaInfo` is a 3-way variant; the `MetaFuncCurry` and `SharedTreeItem` arms carry no
  LispRef at GetMetaInfo() return, and ~20 direct `GetCheckedKeyExpr()` splices bypass
  slSupplierExprImpl altogether.

The DataController layer *is* the intended granularity, keyed by the LispRefs actually
embedded, with none of those hazards:

- every checked key expression a consumer can embed is DC-backed by construction
  (slSupplierExprImpl → GetCheckedKeyExpr → UpdateDC → GetOrCreateDataController), and
  `FuncDC`'s constructor eagerly creates a DC per argument sub-expression
  (tic/MoreDataControllers.cpp), so the DC graph mirrors the calculation DAG;
- a set stored on the DC lives exactly as long as its subject and dies with it — no new
  global registry, no LispObj dtor hooks, no teardown code (`~DataController` already erases
  itself from `s_DcMap`);
- DC creation is meta-thread-only (asserted in GetDataControllerImpl), so the memo member
  needs no lock;
- this is the set-valued refinement of the existing `AF_IntegrityChecked` bit, which already
  propagates DC-arg-wise (FuncDC::MakeResult) and supplier-wise (Actor::UpdateMetaInfo).

## Design

Types (tic/DataController.h):

```cpp
struct lisp_ref_less {          // transparent: probe with a LispPtr without creating a LispRef
    using is_transparent = void;
    template <typename L, typename R>
    bool operator()(const L& lhs, const R& rhs) const { return lhs.get() < rhs.get(); }
};
using check_set     = std::set<LispRef, lisp_ref_less>; // elements: interned cond exprs (arg 2 of integrity_check)
using check_set_ptr = SharedThingPtr<check_set>;
```

`DataController` carries `mutable check_set_ptr m_ImpliedChecks` (null = not yet derived; a
shared static empty instance is the derived-∅ sentinel — it holds no LispRefs, so its CRT-exit
destruction stays clear of the LispObj caches) plus the accessor `GetImpliedChecks()`;
implication tests go through the free functions `InsertCheckAtoms`/`AreCheckAtomsImplied`
(tic/DataController.h).

`DataController::GetImpliedChecks()` (tic/MoreDataControllers.cpp) folds lazily, with an
explicit stack (key expressions nest deeper than the C-stack allows; memoized DCs act as the
visited set, so each DC folds once per lifetime):

- leaf DCs (SymbDC incl. sourceDescr, StringDC, NumbDC, UI64DC, nullary applications) → shared ∅;
- FuncDC: union over the *contributing* args' sets, plus the own condition when the node is an
  integrity_check application (cond = `m_Key.Right().Right().Left()`). Identify that through
  `m_OperatorGroup->GetNameID()`, **not** `DataController::GetNameID()`: `FuncDC::GetNameID()` returns
  `GetLispRef()->Left()->GetNameID()`, which is the `Object` identity of the key's head node — its
  LispObj class, reported as `SymbObj` — and never equals an operator token. Getting this wrong
  yields an empty set for every node, so every guard is re-applied and the dedup silently
  degrades to no dedup at all, with no error anywhere;
- an arg contributes iff `FuncDC::MustCalcArg(og->GetArgPolicy(argNr, nullptr), true)` — the
  args the engine calculates before the operator runs (FuncDC::GetArgs applies exactly this
  predicate), which answers the issue's "short-circuited branches" caveat more strictly than
  the AF_IntegrityChecked OR. Groups with dynamic argument policies are treated as
  non-contributing (defensive: they are dont_cache_result and never become FuncDCs).
  Undercounting errs towards one superfluous wrapper — the safe direction;
- sharing: no contribution → shared ∅; exactly one distinct non-empty child set and nothing
  added → that child's `check_set_ptr` is passed through (chains of unary/converting
  operators share one set object); otherwise one allocated union.

### Conditions are compared per conjunct

A `check_set` holds the **conjuncts** of a condition, not the condition as a whole.
`InsertCheckAtoms` / `AreCheckAtomsImplied` (declared in tic/DataController.h) split a condition
on its conjunction spine, and every popped node is re-tested, so `and(and(a,b),c)`,
`and(a,and(b,c))` and any n-ary or item-substituted mix (`a && x` where `x := b && c`) all
normalise to the same atoms — `and` is associative and the normal form must not depend on
spelling.

Soundness runs one way only. Enforcing `and(a,b)` over every element enforces `a` and enforces
`b`, so **recording** may decompose. Enforcing `a` says nothing about `b`, so **testing** may not:
a candidate is skipped only when *every* one of its atoms is already enforced. `AreCheckAtomsImplied`
returns false for an empty atom list, so an unrecognisable condition keeps its guard.

The payoff is merging: two checks that share a conjunct now recognise each other, argument sets
union on atoms instead of on opaque whole conditions, and the share-the-child's-pointer fast path
in the fold fires more often. A nearer ancestor's `a && b` discharges an outer ancestor's `a`,
collapsing two guards into one.

The wrap-site guard (tic/TreeItem.cpp, TreeItem_CreateCheckedExpr): the caller passes the
DataController of the expression about to be guarded (UpdateDC has it in hand; the non-DC
funnel of GetCheckedKeyExpr passes null → implied ∅ → wrap all, conservative and cheap since
those literal/sourceDescr trees are leaves). A loop-local `enforced` set is seeded from the
memoized set and extended with the atoms of each guard this fold adds on top of the expression
(those have no DataController of their own yet), so an atom counts whichever source it came from;
a candidate is skipped iff all of its atoms are in it. Deciding at
CONSTRUCTION time is what makes skipping sound (see `abaf2447`: an item's own DataController
IS its guarded expression; stripping guards per consumer breaks cache-unit identity).

Failure semantics are unchanged from the bounded search, only exact: a skipped wrap means the
same cond is enforced by an embedded node; a violation fails that node and consumers fail
derivatively through arg-failure propagation. The validate loop of TreeItem::DoUpdate (verdict
propagation, `8ef86c09`) is untouched.

## Observability

Under MG_DEBUG, CheckOperator::CreateResult (clc/Checker.cpp) counts distinct integrity_check
DC instantiations and traces one `integrity_check dc #N: <cond>` line per instantiation
(ST_MinorTrace), so before/after totals are grep-able from a /L log.

## Measured

`scratch/icheck_count_probe.dms` (not part of the suite): 25 items under one checked container,
each referencing the previous, requested at the tail. The whole chain is enforced by **1**
integrity_check DataController — the wrap on the chain head — where before this change the
count equalled the number of items. The retired depth-4 containment search lost sight of the
guard once the chain outgrew its budget and re-wrapped roughly every fifth link.

`testcases/fn_test_icheck_dedup.dms` yields 4 check DCs: one for the guarded chain plus one per
distinct condition in its `/checks` container. Debug runs of both new cases exit without
assertions, exercising the `IsMetaThread` assert in `GetImpliedChecks` and the LispCaches
empty-at-teardown asserts.

## Verification

- testcases/fn_test_icheck_dedup.dms: a checked container with a 5-deep reference chain — one
  guard on the chain head; every later link's wrap is skipped, beyond the old 4-level bound;
  values computed through skipped wraps stay correct.
- testcases/fn_test_icheck_dedup_neg1.dms: the same chain under a violated check must fail the
  run even though only the chain head carries the wrap.
- testcases/run_testcases: no verdict changes in the pre-existing battery.
- t720 (tst/Projects/2BURP, `/t720_2BURP_indicator_results/result_json`): exit 0 with no
  errors; check-instantiation count at or below the 452 of the bounded search (3,393 unmitigated).
- MG_DEBUG run: exercises the IsMetaThread assert in GetImpliedChecks and the LispCaches
  empty-at-teardown asserts (would catch set-held LispRefs leaking past DC destruction).

## Non-goals / follow-ups

- Retiring the DoUpdate ancestor-validate loop (the issue's secondary outcome) stays out of
  scope: it is what propagates a wrapper's verdict to consumers already holding data;
  retirement is gated on persisting verdicts from calculation to validation.
- A canonicalizing set-interner for cross-DC dedup of equal multi-element sets: only if
  profiling shows multi-parent unions allocating measurably; single-lineage sharing covers
  the dominant shape.
- The unattributed t720 heap corruption noted in `abaf2447` (0xC0000374): unrelated to this
  change, watched during its verification runs.
- The RewriteExpr.lsp hoist rules (`42441a58`) relocate guards rather than duplicate conds;
  the memo sees post-rewrite keys (RewriteExprTop precedes DC creation), so no interaction.

## #1218: checks travel along ExplicitSuppliers

The collection side of the fold gained a second in-edge. The checks that apply to an item were
"own + ancestors", walked per fold; since #1218 they are the transitive closure of the item
under **GetTreeParent ∪ ExplicitSuppliers**: declaring a supplier means "evaluate me first",
and whatever guards the supplier — its own check, its ancestors', its suppliers', transitively —
guards the declaring item with it. Before, a supplier's check was only evaluated beside the
declaring item by the validate phase (out-of-band, verdict not travelling to consumers), and a
TreeView/detail-page visit ran it not at all.

Design (TreeItem.cpp, `TreeItem_GetCheckGuardians`):

- The closure is reduced to a per-item **guardian list** (items with `HasIntegrityChecker()`,
  in fold order: own check, then each ExplicitSupplier's closure in declaration order, then the
  parent's closure) and memoized in `ConfigProperties::mc_CheckGuardians` — meta-thread only,
  like `mc_DC`, reset with the other config-derived state in `DoInvalidate` and in
  `ResetSubTreeConfigData` (the list holds `SharedTreeItem` refs that can cross branches via
  the supplier edge; the teardown reset is what breaks those cycles before refcount collapse).
- An item that adds nothing **shares its parent's instance and is not memoized**: re-deriving
  it is the walk the pre-#1218 fold did anyway, so no ConfigProperties is allocated per
  descendant of a checked root. For a chain without supplier edges the list is exactly the old
  self-to-root walk in the same order — folded expressions, and thereby every existing
  DataController moniker, are byte-identical (pinned: `fn_test_icheck_dedup` still instantiates
  the 4 check DCs recorded above).
- The gate (`TreeItem_HasIntegrityCheckerInclAncestors`) stays a flag walk when no
  (config, non-template) SupplCache sits on the parent chain; only then is the closure consulted.
- Redundancy stays decided at the wrap site, per folded expression, against the DC-memoized
  implied-atom sets of #1182: an item that both references and declares the same supplier
  carries the guard once, through the reference (verified on the detail page: no CheckedKeyExpr
  wrap appears).
- Cycles along ExplicitSuppliers are broken with an in-progress set; nothing computed under a
  skipped back-edge is memoized (it would be incomplete). The DoesContain gate does not refuse
  a supplier's checker that references its own declaring consumer; that shape surfaces as a
  metainfo/DC circularity like any other cyclic configuration.
- Fence SupplCaches (`InitAt`, PhaseContainer mirrors on cache items) are engine bookkeeping,
  not a configured "evaluate me first" relation, and are excluded from the closure.

Semantics: the guard gates the **delivery** of the declaring item's result, not the start of
its calculation — cond and the item's own expression are sibling args of CheckOperator and may
still compute concurrently (measured: a 20M-element supplier check and the declaring item's
`add` run on different workers; the view waits for both). Strict check-before-calc ordering
would need the org expression's OperationContext to depend on the check future — a lookahead-
scheduling follow-up, not part of this change.

Verified: `fn_test_icheck_suppl` (two-hop supplier chain + supplier-under-checked-container;
Debug trace shows `IntegrityCheck(IntegrityCheck(add(2,3), eq(22,22)), eq(11,11))` — the
closure in fold order), `fn_test_icheck_suppl_neg1/2` (violated direct and transitive supplier
checks fail the run), battery 231/0, Debug runs assert-free.
