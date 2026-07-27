# Typed-HOF GeoDMS — Definitive Remaining Work

De-duplicated across the three source dimensions (design docs, code markers, documented limits). "Done" work (the operator-signature interface / all shipped batches, typed `map`, partial application, anonymous functions, def-time typed walker, composite-result sweep, function-decl serializer) is excluded; the P4 "WP4.1 remainder not implemented" note in the design-doc intro is **stale** and dropped.

Effort tags: **[substantial]** / **[moderate]** / **[niche]**. Items are sorted heaviest-first within each group. "Sources" cite `typed-hof-language-design.md` (= design doc), `operator-signature-interface.md` (= op-sig doc), `function_serializer.md`, and code paths.

---

## 1. Language features

- **WP4.2 — opt-in `applyF` boundary DataController** **[substantial]** · *not started*
  Non-inline function applications keyed as `(applyF "/path/F" argKey…)` via a new `token::applyF` + `FuncApplDC`, gated by `inline="false"`. The attribution-anchor / large-body-memoization escape hatch; inline beta-reduction is the only mode today. Blocked design pieces: R5 teardown of chained DCs (needs a prototype), R6/R7 attribution+memo, R11 key-head versioning.
  *Sources: design §10 P1/P4 WP4.2 (2038-2069, 2226-2234), §8.3; risks R5-R7, R11.*

- **WP3.2 — full runtime lambdas** **[substantial]** · *deferred (optional)*
  First-class `(lambda (formals) body)` LispRef terms with fresh-ChroID alpha-renaming (Prolog `Renum`/`Solve`) + §5.6 erasure post-condition scan. Named functions + partial application + parse-time lambda-lifting already "cover the practical space"; only needed if a true runtime binder is later wanted.
  *Sources: design §10 P3 WP3.2 (2132-2149); risk R10.*

- **Sub-expression container literals in non-argument position (§5.9)** **[moderate]** · *deferred*
  E.g. `2.0 * X{…}` — reducer throws "the '{}' construct is not yet supported inside inlined function bodies". Literals are argument-position-only today; §5.11 brace-disambiguation charts the path via explicit parens `2.0 * (X { m: e; })`.
  *Sources: design §5.9 (835), §5.11 (1323-1324); `AbstrCalculator.cpp:2239,4691`.*

- **Function-application into a bare item (WP4.1 tail)** **[moderate]** · *deferred*
  A direct function-application calc rule on a bare `name := expr` item hits the §5.9 container-holder guard and still requires a typed holder (operator + data rules on bare items already work). The remaining "fn-app-into-bare-item marker."
  *Sources: design §5.12 (1221-1224), WP4.1 "Still open" (1304-1306).*

- **Closure capture of enclosing-function LOCALS (§5.10)** **[moderate]** · *partial*
  A nested body may reference the enclosing application's **parameters** only; referencing an enclosing function's **local** item errors "reference to (part of) a template or function." Marked "lift on demand."
  *Sources: design §5.10 v1 limitation (971-973).*

- **WP3.3 — remaining `map`/combinator surface** **[moderate]** · *partial*
  Remaining siblings of the shipped typed `map(F,src)`: `filter` + `fold` container combinators (no `filter`/`fold` in `AbstrCalculator.cpp` yet — note `filter` has a meta-vs-data phase problem: deciding which children to keep needs computed data at instantiation time); and `for_each` deprecation / a template→function lint path. **DONE: `map` over a partial-application F** (`map(Scale(k, _), src)`, commit `7551dad2`).
  **`fold` motivation (Maarten):** the current idiom for sub-item aggregation is `AsItemList(...)` in an *indirect expression* over a member set that is only known per instantiation; a typed `fold` would be **checkable at the function definition** — the reduction type unifies without the concrete sub-item set — which the indirect-expression form cannot be. Future work; not built on spec (no confirmed reduce-over-variable-children need yet).
  *Sources: design §10 P3 WP3.3 (2166-2168), §9 (1934), §5.5.*

- **Member access through a function-valued parameter (§5.10)** **[moderate]** · *documented limitation*
  `p/member` where `p` is bound to a function is rejected with a dedicated error.
  *Sources: `AbstrCalculator.cpp:2470`; design §5.10 (1123-1127).*

- **§5.13 meta-reference (`item`) parameter follow-ups** **[niche]** · *partial*
  `item` in result position is rejected (parameter-only); member access **through** a meta-ref parameter beyond what container parameters support is deferred.
  *Sources: design §5.13 (1702-1704).*

- **§5.7 v2 variant-set residuals** **[niche]** · *partial*
  Variant-set calls are always inline (no container/`instantiate` holder); variant `using` imports are per-variant.
  *Sources: design §5.7 v2 (697-698).*

- **Nested `apply T(…)` in sub-expressions (§5.9)** **[niche]** · *deferred*
  Template value form is root-only (disallowed nested for soundness — templates can capture call-site names); "convert to a function for inline composition."
  *Sources: design §5.9 decision 2 (907-912).*

- **Top-level lone-call sugar `{ X(args) }` (§5.9)** **[niche]** · *deferred (redundant)*
  Equivalent to the working `instantiate X(args)`.
  *Sources: design §5.9 (834, 869).*

---

## 2. Operator signatures

- **K11 — container-shaped types in `DefType`** **[substantial]** · *K11a-1 landed (`08ac5c8f`); K11a-1b identity plumbing landed (member attr carries sibling member-unit `vuNode`; verified flowing through `pcount`; combining-operator observability still GATED on described `add`/`+`); K11a-3.1 generic member types landed 2026-07-27 (member values/domain tokens resolve through the positional ladder — generic vars incl. K2, telescope unit params, scope units; explicit domain tokens no longer ignored, fixing a K11a-1 FALSE definition-time rejection of `mid (E2) := nw/cost`; member-path result domains `-> attribute<…> (nw/nodeset)` confirmed working and pinned by `fn_test_network` — the earlier "only `-> parameter<…>` results" claim was wrong; adversarial review round fixed the `'.'`-implicit-domain default regression, parameter-QUALIFIED member-unit node keys `p/member` incl. better diagnostics, values-ladder precedence vs ValueClass names, and memoized `ParamType`); remaining: K11a-3 instantiation-point contract check, by-example member persistence, container-kind params, deep member paths, generic-class member units, K11b* · **scoped: `k11-container-types-scope.md`**
  `ArgContainer` records a domain var but does not **link** it (v1 diagnostics-only); there is no container/record kind in the def-time type language. The gate for the two items below plus discrete_alloc name-array obligations. (A narrow `DefType::Kind::Container` landed for for_each/composite typing, but general operator `ArgContainer` linking + container-argument member enumeration is deferred.) **Scope splits it into K11a — user composite *parameters* with statically-declared members (moderate, recommended first; the `network_links` case) — and K11b — operator `ArgContainer` runtime-member linking (substantial, gated).**
  *Sources: op-sig §7 K11 (571-587), §15 Q1 (1623), §12.7-12.8 (1351-1490); `k11-container-types-scope.md`.*

- **Application-time `FindOper` adoption + §18 ∀-selector** **[substantial]** · *not started*
  Replace first-match-wins `IsDerivedFrom` selection with the richer signature records at application time; the §18 exact ∀-selector via `FindOper` enumeration + a `noexcept` `TryFindOper` twin (absent in source). Declared an alternative-not-taken vs. the shipped batch-A member-class-tuple elimination.
  *Sources: op-sig §1 (34-37), §15 Q2 (1625-1627), §18 (1744-1819).*

- **for_each / meta-group full typed-container expansion** **[substantial]** · *partial*
  for_each shipped member-set + arity checking, but producing a fully typed result **container** needs the K11 gate. `loop` and other meta groups default `DescribeMetaSignature=false` and defer wholesale. Sequencing: impedance tranche (done) → K11 → for_each container expansion.
  *Sources: op-sig §12.7 (1347-1417).*

- **Tier-2 meta `SelectMetaOperator` composite-result typing** **[substantial]** · *specified, scoped out*
  `select_with_attr_by_cond`, `select_with_org_rel_with_attr_by_cond`, `select_with_attr_by_org_rel` (+uintN), `collect_attr_by_{cond,org_rel}`. Specified but not built: meta heads can't inline-reduce (diagnostics-only payoff), the member set is a usually-formal container arg, and it needs a new container-directed `MetaMemberLayout` variant. Current sound behavior pinned by `fn_test_selmeta{,_neg}`.
  *Sources: op-sig §12.9 Tier 2 (1527-1560).*

- **§9 `SigUnitChecker` drift-verifier** **[moderate]** · *both defenses landed + armed; two v1 coverage gaps remain*
  **DONE (defense #2, MG_DEBUG, commit `91412119`):** merge-time structural KIND audit in
  `AbstrOperGroup::GetSignatures` — a described `Attr` position registered as a unit (or `Unit`
  registered as data) asserts (contradiction-only, so polymorphic args pass).
  **DONE (defense #1, MG_DEBUG, commit `48967a7f`; ARMED as asserts in a follow-up):**
  `SigUnitChecker_VerifyApplication` (`OperSignature.cpp`) replays each resolved member's record against the
  ACTUAL units after a successful `CreateResultCaller` in `FuncDC_CreateResult` — unit IDENTITY per `sig_var`
  (K2 bridge: values-in-domain-role vars only) via `UnifyDomain`, value CLASS vs `memberClasses`,
  `SameValueClass`/`CompatibleValues`, and metric product/quotient/dimensionless (log-only). Those three
  class/identity checks now emit a detailed `ST_Error` line and ASSERT (headless: log + exit(3)); metric stays
  log-only (§9 deferred). Reports go via `reportF_without_cancellation_check` (plain `reportF`→`ASyncContinueCheck`
  throws on a pending GUI cancel and would fail the just-created result); the call site also wraps the checker in
  `catch(...)` so no report path can reach FuncDC's result-failing catch. Because the checker runs only AFTER
  CreateResult succeeded, a fired assert = the description genuinely OVER-claims (the S2 drift). Verified:
  137/137 Debug battery clean with asserts armed (zero ASSERT verdicts); a deliberate-drift canary confirmed the
  armed report+assert fires (exit 3, `ST_Error` detail) on a real application; a 22-agent adversarial audit
  predicted zero false-fires and zero real drift across 18 described families (and caught the cancellation-throw
  bug). The primary S2 (description↔CreateResult drift) defense.
  **Remaining (v1 gaps, follow-ups — silence, not false-fire):** (a) `rec.resultMembers` (§12.7/§12.8 composite
  sub-items — unique `Values`, union `UnionData`, `connect_info`/`discrete_alloc`/`dijkstra` member sets) are not
  replayed (member-path resolution post-`CreateResult` risks meta-info recursion); (b) `addValRep` is first-wins,
  so a `RepeatArgs`/variadic tail's value class is checked at its first position only. NOTE: a described family
  NOT exercised by the 137-config battery (e.g. some geo/aggregation members, and `rlookup` which the audit
  flagged as not actually analyzed) could surface its FIRST assert in a tst-Debug or GUI-Debug run — that is the
  intended drift signal, to be fixed at the description.
  *Sources: op-sig §9 (720-746), §6.4; risk S2.*

- **Speculation / trial harness in `TypeUnifier`** **[moderate]** · *not started*
  Copy-trial-adopt for multi-candidate overload selection; today ambiguity defers instead of committing the unique surviving candidate's implications. Boundary already shaped as Begin/Commit/AbortTrial.
  *Sources: op-sig §6.2 (484-505), §12.1 pt 5 (901-903); risk S4.*

- **`unit_creator_spec` + `uc_*` registration factories** **[moderate]** · *not started*
  Pair a `UnitDerivationKind` with a `UnitCreatorPtr` so K9/K10 values-unit derivation is consumable at definition time. "Deliberately deferred"; only useful once metric checking un-defers.
  *Sources: op-sig §5.4 (307-342), §12.1 pt 4 (897-900).*

- **discrete_alloc name-directed result members** **[moderate]** · *deferred*
  `shadow_prices/<name>`, `total_allocated/<name>` (+ conditional `bid_price`) types derive from partitionings/suitabilities, not the closed `typeNames` array, so they defer rather than error; `landuse`/`bid_price` are typed structurally (shipped).
  *Sources: op-sig §12.7-12.8 (1372-1490).*

- **§12.7 v1 narrowing — function-CALL head inside a closed K13 spec** **[moderate]** · *documented limitation*
  Such a head defers (building its key would re-enter `ReduceValue`). "Lift with the sentinel + errorHolder later."
  *Sources: op-sig §12.7 (1223-1224); `AbstrCalculator.cpp:3225`.*

- **K6 `generative`/`genOrigin` tightening on `UnitNode`** **[niche]** · *deferred*
  Refuse bind-to-concrete / link-to-rigid for fresh existential result units with a dedicated message. Batch D shipped K6 with plain flexible nodes; optional later hardening ("after batch D soaks, or never?").
  *Sources: op-sig §6.1 (441-443), §8 (597), §15 Q3 (1628).*

---

## 3. Prelude / RewriteExpr.lsp retirement

- **Category E — `select` / `collect_by_cond` argument-injection rules** **[substantial]** · *partial*
  Retire the per-flavor injection rules via overload resolution on the select Σ-result type — an operator-signature matter, gated on the WP4.1 machinery reaching the select family (not covered by shipped batches).
  *Sources: design §8.2 cat E (1539), §8.4 (1622-1624); `RewriteExpr.lsp:32-45`.*

- **Category D — compiled-in typed simplifier pass to fix the type-UNSAFE eliminations** **[substantial]** · *not built (latent type-safety gap)*
  The 2026-07-18 ruling legitimately keeps the simplification rewrites in the `.lsp` (see Accepted Limitations), **but** the goal of fixing the unsafe ones — `mean/sum/modus _X (id _E)` assume domain compatibility without checking — via an engine-owned typed pass firing only after the typing precondition (e.g. `UnifyDomain`) verifies is unmet. A genuine remaining gap, not a stylistic deferral.
  *Sources: design §8.2 cat D (1538), §8.4 (1617-1621); `RewriteExpr.lsp:101-119,170-172,195,244-255`.*

- **Category C — real variadic typed `switch` operator + pattern/destructuring parameters** **[substantial]** · *not built*
  Retire `switch`/`case` unrolling by building a typed `switch` operator (all case values unify; conditions bool over one domain) — which itself needs the distinct **pattern-parameter** feature that destructures `case(c,v)` wrappers.
  *Sources: design §8.2 cat C (1537), §5.14 (1756-1757), §10 P2 (2102); `RewriteExpr.lsp:91-97`.*

- **Relocate `claim_divF32` / `claim_corrF32` into the RuimteScanner config** **[moderate]** · *deferred*
  Their resolvent bakes in the absolute path `/Classifications/OperatorType`, which a prelude function cannot reference under strict scope — correct home is a user `function` in the model config. Needs coordination with that model's owners.
  *Sources: design §8.2 cat A (1535), §8.4 (1628-1631); `RewriteExpr.lsp:208-213`.*

---

## 4. Type system

- **§4.9 — CRS as a first-class `crs(σ)` refinement** **[substantial]** · *not started*
  A `CrsOperation` beside `MetricOperation`, a structured `(BaseUnit (SRef "EPSG:…"))` key head replacing the 0xFF-multiplexed metric string, background-layer `DialogData` leaving type identity, a coordinate base carrying both crs+metric. Would make CRS mismatches type errors while un-erroring background-only mismatches. No IMPLEMENTED marker; R15 treats it as a future transition.
  *Sources: design §4.9 (404-439); risk R15 (2323).*

- **Declared metric constraints on function parameters (un-defer K10)** **[substantial]** · *not started*
  Move metric checking from instantiation-time to definition-time. "Vacuous not intractable" — the metric algebra is a decidable free abelian group and the op-sig vocabulary (`MetricProduct/Quotient/Power`) already carries the relations; blocked only because a formal `unit<float64>` has no declarable metric syntax yet (hof P4/v2).
  *Sources: design §11 (2288-2298); op-sig §7 K10 (570), §15 Q4 (1629-1634), §20.1.*

- **Storage-bearing items inside function/template bodies** **[substantial]** · *deferred*
  `HasStorageManager` asserts `!InTemplate`; file-state-vs-DC-keys + write-lock design "needs its own design later — templates remain the vehicle for parameterized imports meanwhile."
  *Sources: design §11 (2302-2304); risk R1; `TreeItem.cpp:1848`.*

- **Definition-time type inference stays Unknown for several categories** **[substantial]** · *partial*
  Still infers Unknown (per-application only) for: **unsignatured** built-in operators, externals, variant selections, partial applications, member/container accesses, and closure-captured names. Narrower than the doc's original framing — described operator families now type through `InferOperatorApplication` (result + members).
  *Sources: design §5.10 (1123-1127); `AbstrCalculator.cpp:4094`.*

- **WP4.4 — surfacing/printing of metric constraints** **[moderate]** · *partial*
  Printable signatures should include the metric; WP4.1 TypeSpecs should carry it as `metric(μ)` terms. The metric-via-alias idiom already works with no code change — only the surfacing remains.
  *Sources: design WP4.4 (2250-2259).*

- **Type-variable clauses on signature aliases + `...rest` in signature types** **[moderate]** · *partial*
  Only function **declarations** accept `<V: constraint>`; a `nuf = function<V,D>(…)` alias with a type-var clause is unavailable, so shape checks against a signature alias stay kind-level. `...rest` inside a function-signature type is unsupported.
  *Sources: design P2 (2104); `ConfigProd.cpp:1127,1191`.*

- **`map(F,src)` container result falls back to a plain TreeItem** **[moderate]** · *partial*
  Commit `ad9fdba0` made mapped **data-item and unit** children first-class (materializing the derived kind+type), but a **container** result still becomes a plain TreeItem follower. WP3.3 doc text (2151-2168) still describes the old all-plain behavior.
  *Sources: commit `ad9fdba0`; design WP3.3 (2151-2168, stale).*

- **Bare `name := expr` first-class typing deferred** **[moderate]** · *deferred*
  The item is a plain `SignatureType::TreeItem` whose kind/type only follow from its calculation at DC time; for an underdetermined expr the inferred type is TOP, so `IsDataItem`/`IsUnit` cannot classify it statically. The mirror of the map-children fix (which followed the referred object; the bare item does not).
  *Sources: design §5.12 (1214-1234); `doc/Interest.md:92`; contrast `ad9fdba0`.*

- **WP4.3 — `range = [lo, hi]` sugar** **[niche]** · *not started*
  Generates the IntegrityCheck string for a refinement alias. The refined-alias core (IntegrityCheck via alias, with rebinding) is implemented; only this generating sugar remains ("can follow").
  *Sources: design WP4.3 (2248).*

---

## 5. Tooling / GUI / serializer

- **GUI "expand steps" inspection of inline (non-materialized) applications** **[moderate]** · *not started*
  A detail action that re-runs the reduction in instantiating mode on demand, plus showing the function body source with arguments substituted. Value-Info already walks the reduced DC graph per operator node.
  *Sources: design §5.9 (948-952).*

- **Serializer faithfulness — bare `f: function` param + `-> unit<V>` result render as `container`** **[niche]** · *documented limitation*
  Stored as plain TreeItems; binding-compatible but not fully faithful. Faithful rendering needs the signature/result-unit exemplar persisted the way signature-typed params already are. (Same family as the memory-index "fn-application-into-bare-item marker.")
  *Sources: `function_serializer.md:141-143`.*

- **Serializer faithfulness — `container` param member block dropped** **[niche]** · *documented limitation*
  Renders as bare `container P`; member declarations are dropped (unit params DO render their member block). Recomputes on reload; only the documentary shape is lost.
  *Sources: `function_serializer.md:144-147`.*

- **Serializer faithfulness — renamed designated result label lost** **[niche]** · *documented limitation*
  `-> x: T := y` loses the label `x` (semantically equivalent).
  *Sources: `function_serializer.md:148`.*

- **Code hygiene — belt-and-suspenders `SetIsInstantiated()`** **[niche]** · *cleanup marker*
  `holder->SetIsInstantiated()` after an assert that should already guarantee it — flagged "REMOVE if the above assert is PROVEN." Trivial, not a feature gap.
  *Sources: `AbstrCalculator.cpp:5168` (ApplyAsMetaFunction).*

---

## 6. Accepted limitations (by-design / permanent boundaries — not to-dos)

Listed so the "remaining" set above isn't mistaken for exhaustive; none of these are planned work.

- **Recursion is rejected** by the cycle guard; only bounded combinators / iteration are available (variadic folds with strictly-decreasing arity ARE allowed). *Design §11; `AbstrCalculator.cpp:1961`.*
- **`RewriteExpr.lsp` end-state is NOT an empty file (2026-07-18 ruling).** Residual normalizers legitimately stay: `Value→convert`, `min_elem`/`max_elem(_fast)` unary collapses, `pow _X 2..6` integer-literal fast paths, NlLater `BaseUnit`/`convert` fixups, `MakeDefined` idempotence, and the boolean/constant simplification algebra. *Design §8.4 (1617-1639), §5.15 (1810-1812).*
- **Pattern-destructuring definitional rules stay permanently** — `order` / `isOverlapping` / `median`-on-`interval` destructure syntactic nodes a function application would hide. *Design §8.4 (1570-1575, 1613-1621); `RewriteExpr.lsp:129-154`.*
- **`rjoin` self-join collapse stays permanently** — `lookup(rlookup a a) c → c` fires only post-substitution; a prelude `rjoin` would break key identity (documented negative finding). *Design §8.4 (1644-1655).*
- **Serializer: domain/values case may normalize** when a reference resolves — round-trip-safe (DMS identifiers are case-insensitive), documented rather than fixed. *`function_serializer.md:149-150`.*
- **Serializer: a config that explicitly `#include`s the auto-imported prelude cannot round-trip** — include-erasure re-emits `function sqr…` at root, colliding with the auto-import ("SubItem sqr already defined"). One documented skip (`fn_test_prelude`); the rendering itself is correct. *`function_serializer.md:151-157`.*
- **§5.11 anonymous function-literal v1 limits** — literals carry no `using` clause and no named result; `_lambda_<n>` items report line-1 source location (GUI go-to-source lands at file top); the spliced name shows in calculation rules. *Design §5.11 tier B (1405-1412).*

---

**Substantial items worth sequencing first (cross-cutting gates):** K11 container types (§2) unblocks for_each container expansion, discrete_alloc name-array members, and the Tier-2 SelectMeta typing; declared parameter-metric syntax (§4) unblocks K10 metric un-deferral and `unit_creator_spec`; the WP4.1 select-family signatures unblock Category-E `.lsp` retirement (§3).