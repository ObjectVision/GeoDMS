# K11 — Container / Record Types in the Definition-Time Type Language (scope)

Scoping for the remaining-work item **"K11 — container-shaped types in `DefType`"** (`typed-hof-remaining-work.md` §2, `[substantial]`, deferred). Motivating case: a user composite type used as a formal parameter whose *structure* is checked — e.g. a network relating edges twice to nodes, both unsigned-integer units:

```
network_links: unit<uint32> { nodeset: unit<uint32>; F1, F2: attribute<nodeset>; }
function connectedness( nw: network_links )
-> link_counts: attribute<uint32> (nw/nodeset) := pcount(nw/F1) + pcount(nw/F2);
```

Goal: the shape (`F1`,`F2` present, both uint relations to a shared uint node unit) is validated **at the definition** (under ∀) *and* reported cleanly **at each instantiation point** — not merely surfaced transitively as a downstream `pcount`/`Unknown identifier` error inside the instantiated body.

---

## 1. Current state (verified)

The definition-time type language already has most of the machinery — it is populated for composite **results**, not for **parameters/arguments**:

- **The container kind exists.** `DefType` (`AbstrCalculator.cpp:2838`) carries `enum Kind { …, Container }` (`:2840`) **and** a member map `std::shared_ptr<const std::map<SharedStr, DefType, MemberPathLess>> members` + `bool membersComplete` (`:2885-2886`) — the "K11 member map", keyed by sub-item path, case-insensitive.
- **Composite results are typed.** Operator applications with a composite result (for_each `Kind::Container`; cacheable ops `unique`/`Values`/`connect_info`/… via the described `resultMembers`) fill that member map — `InferOperatorApplication` (`:4587`) + `resultMembers` consumption (`:4184-4209`). This is why `ci/dist` in `connect_info(...)` types as float32 and drove `fn_test_ci_neg3`.
- **Parameters are NOT typed.** `ParamType` (`:3406`) returns `{}` (Unknown) for structured / composite-by-example parameters — literally `return {}; // container / typed-by-example parameters: deferred` (`:3441`).
- **Member access through a parameter is deferred.** `ResolveName` (`:3072`) returns code `2` / `ExtRefKind::ParamMember` for a `param/member` path — `// parameter member access is not verified/typed at definition time` (`:3098-3099`). So `nw/F1` is `Unknown`; the body's use of it is checked only per-application at reduction.
- **The op-sig argument half is diagnostics-only.** `SignatureRecorder::ArgContainer` (`OperSignature.cpp:116`) records `PosKind::Container` with a `sharedMemberDomain` var but never links it (op-sig doc §7 K11).

Empirical confirmation (`scratch/network_check.dms`): a valid network computes; a network with `float32` `F1`/`F2` fails with `pcount: cannot find operator for DataItem<float32>` **inside** `/badf/link_counts`; a network missing `F2` fails with `Unknown identifier 'nw/F2'` inside `/badm/link_counts`. Both are caught, but transitively (body-op dependent) and attributed to the body item, not to the parameter contract.

---

## 2. Two sub-packages

K11 splits along where the member set comes from:

### K11a — user composite **parameters** (statically declared members) · *[moderate] · recommended first*
The member block is written at the definition (`unit<uint32> { nodeset; F1; F2 }`), so the members are **known statically** — no meta-time enumeration needed. Highest value (directly answers the motivating case), lowest risk. Delivers def-time body checking *and* a clean instantiation-point contract check.

### K11b — operator **`ArgContainer`** (runtime-container members) · *[substantial] · gated, later*
`for_each`, `discrete_alloc` name-array members, Tier-2 `SelectMeta` composite results take a container **argument** whose members are enumerable only at meta time. Shares the `DefType` container machinery from K11a but must link `sharedMemberDomain` and enumerate the argument's members during reduction. This is the cross-cutting gate the remaining-work summary calls out (unblocks for_each container expansion, discrete_alloc name-array obligations, Tier-2 SelectMeta typing).

---

## 3. K11a design

**(a) Build a container `DefType` for a composite parameter.** In `ParamType` (`:3441`), replace the blanket `{}` for a structured unit / composite-by-example parameter with a `DefType` whose `kind` is the parameter's own kind (`UnitVal` for `network_links = unit<uint32>{…}`; `Container` for a bare `container P {…}`) and whose `members` map is built by walking the parameter item's declared sub-items with the existing `PositionType`/`DeclaredItemType` typing:
- `nodeset` → `UnitVal` node (uint), a per-instantiation identity node (like a unit parameter, `:3426-3432`);
- `F1`, `F2` → `Data` over `nodeset`'s domain node, value class uint (via `PositionType`, `:3435`).
Set `membersComplete = true` (the block is a closed declaration). The member DefTypes reference the parameter's own rigid `nodeset` node, so `F1`/`F2` share a domain by construction — the "two relations to the *same* node unit" invariant falls out of node identity.

**(b) Type member access.** In `ResolveName` (`:3098`), when the matched parameter has a `membersComplete` container type, resolve the `/member` path against `members`: hit → return the member's `DefType` (new resolve code / out-param), so `pcount(nw/F1)` type-checks under ∀ at definition; miss → def-time "member not found" (sound: `membersComplete`). Falls back to today's code-`2` deferral when the parameter type is Unknown (non-composite).

**(c) Instantiation-point contract check** (reducer side, separate from the ∀ checker). At argument binding for a composite parameter (the `ParamMember`/structured-param path near `:2463`), validate the actual argument against the declared members: each declared member present, kind-compatible, value-class-compatible, and the shared-domain members co-domained. Emit `"'{fn}': argument '{arg}' does not match parameter '{p}' ({member} must be attribute<uint32>(nodeset) / {member} missing)"` at the instantiation point. Sound because the declaration is closed.

**Consequences.** With (a)+(b), the *definition* is checked independent of any call: `pcount` requiring a uint partitioning now sees `F1:uint` at def time, and a body that used `nw/F1` in a float context would be a definition error under ∀. With (c), a wrong argument is named at the call, not deep in the copied body.

**Files:** `AbstrCalculator.cpp` `ParamType` (:3406-3442), `ResolveName` (:3072-3104), the member-access binding path (~:2463-2470), `DefType` unchanged (member map already present). No grammar change (the syntax already parses and binds).

---

## 4. K11b design (sketch)

Link `SignatureRecorder::ArgContainer`'s `sharedMemberDomain` into the same unifier the positional args use, and — at the meta application — enumerate the container argument's actual members to (i) bind the shared domain and (ii) type-check declared member patterns. Enumeration must run in the reducer (members exist only after the argument item resolves), feeding results back as a `Kind::Container` `DefType`. Sequenced **after** K11a proves the parameter-side container-DefType plumbing, and after the impedance/for_each tranche (already done) — matching the remaining-work note "impedance tranche → K11 → for_each container expansion".

---

## 5. Sequencing, effort, risk

1. **K11a-1 — LANDED (commit `08ac5c8f`).** `ParamType` (IsUnit branch) builds `DefType.members` via `BuildParamMembers` for a structured unit parameter; the minimal member-access consumption shipped with it (`ResolveName` code 2 outputs the member path; `InferExpr` `case 2` → `InferParamMember`), so structured-parameter member usage is now type-checked at the definition (float-where-uint-required is a def error; `fn_test_structmember` + `_neg1`, battery 142). v1: direct members, value-class only.
2. **K11a-1b — identity plumbing LANDED; def-time observability GATED (documented).** A
   member ATTRIBUTE whose values token names a sibling member UNIT now carries that unit's
   IDENTITY node (`vuNode = UNode(m_FuncItem,0,vt,vc)` in `BuildParamMembers`), keyed by
   token so `F1`,`F2` both `attribute<nodeset>` share ONE node — exactly the batch-U K2
   pattern the signature-param path already uses. **Verified flowing**: an instrumented
   trace showed `pcount(nw/F1)` result domain = the member unit's *rigid* node
   (`argVuNodeSet=1`, result `dNode` rigid), and two members over *different* node units
   yield two *distinct rigid* result domains. **BUT** the natural network idiom
   `pcount(F1)+pcount(F2)` does not surface the cross-member conflict under ∀: the combining
   `add`/`+` group has multi-class/undescribed overloads, so the walker takes the
   `theRecord<0 → return {}` deferral (`InferOperatorApplication`) and the `ns1≠ns2` conflict
   is reported at the *instantiation* (concrete-argument reduction), not at the definition.
   So K11a-1b's identity payoff through a COMBINING operator is gated on a DESCRIBED
   combiner (the `add`-deferral gate — same class of work as WP4.1 select-family sigs /
   arithmetic-op description). Regression coverage: `fn_test_structmember2` (well-formed
   two-relation network computes), `fn_test_structmember_neg2` (malformed
   different-node-unit network rejected — at instantiation, per the gate above).
   *(CORRECTED 2026-07-27: the earlier claim that structured-param functions "only parse
   with `-> parameter<…>` results" was WRONG — `-> attribute<uint32> (Network)` and even
   the member-path result domain `-> attribute<uint32> (nw/nodeset)` parse, reduce, and
   compute; pinned by `fn_test_membergen` and `fn_test_network`.)*
   *(The composite-type-by-example deferral below was RESOLVED by the by-example tranche —
   see the K11a-3.2 entry.)*
3. **K11a-3.1 — generic member types LANDED (2026-07-27).** `BuildParamMembers` resolves
   member tokens through the SAME ladder as positional declarations (`PositionType`),
   innermost first — values: ValueClass name → sibling member unit (K11a-1b identity) →
   the function's generic variables (`attribute<V> w` carries the rigid V `vNode`; a
   domain-sorted variable also carries unit identity, the K2 bridge) → a telescope unit
   parameter (class + per-instantiation identity) → a definition-scope unit; domain: the
   parameter itself (default) → sibling member unit → generic domain variable → telescope
   unit parameter → scope unit (Void broadcasts / Concrete) → otherwise **Dom::Unknown
   (defer)**. This FIXED a live K11a-1 false rejection: an explicit member domain token
   (`cost (E2)`) was silently ignored and the member mistyped over the parameter unit, so
   a correct body item `mid (E2) := nw/cost` errored with a rigid-rigid `'nw'≠'E2'`
   conflict — an explicit token now resolves or defers, never falls back to the parameter
   unit. Coverage: `fn_test_structmember3` (generic values member checked under ∀ +
   telescope-domain member + generic-domain member; includes the once-falsely-rejected
   shape), `_neg1` (rigid V≠W via a member designated into a differently-generic body
   item — operator-free, no add-gate), `_neg2` (member over E2 designated into a body
   item declared over nw — rigid domain conflict), `fn_test_network` (the §6 motivating
   case with a member-path result domain). NOTE: `fn_test_membergen` needed a body fix —
   with `attribute<V>` members REALLY carrying V, a float64-pinned body must CONVERT
   (`float64(Network/flow)`); the bare division demanded `div(V,V)->float64` for every V
   and is now rejected exactly like its positional analog always was (and an
   `instantiate`d body must stay V-token-free: copied bodies do not substitute type
   variables).
   **Adversarial review round (3 lenses, 9 agents, all findings reproduced against the
   built binaries) confirmed 4 distinct defects, all fixed before landing:** (1) a member
   declared WITHOUT a domain carries the implicit `'.'` entity token, never an empty one
   — the first-cut default test missed it and the common no-domain member spelling
   silently DEFERRED its domain (losing the K11a-1 paramDomNode guarantee; wrong programs
   accepted) — `'.'` now selects the default (`fn_test_structmember3_neg3`); (2+3) member-
   unit nodes were keyed by BARE token, collapsing same-named member units of different
   structured parameters (and a member unit shadowing a same-named telescope parameter)
   into one rigid node — now keyed by the PARAMETER-QUALIFIED token `p/member`, which also
   upgrades the diagnostics ("unit variables 'nw2/ns' and 'nw1/ns' … independent";
   `fn_test_structmember3_neg4`); (4) the values ladder tested ValueClass names BEFORE the
   generic variables, inverting PositionType's precedence for a type variable named like a
   value class (member typed concrete, body rigid → false rejection) — the variable rungs
   now precede the class-name rung, sibling members innermost; (5, perf) `ParamType` is
   now memoized per index (`m_ParamTypes`) — every `nw/member` reference used to rebuild
   the whole member map including per-member `FindItem` scope walks.
   Still out of scope here: member UNITS with generic classes (`unit<V>`
   member: class stays unresolved/deferred), deep member paths (nested sub-containers),
   `container`-kind parameters; body-item declarations naming a member path as their
   domain (`attribute<uint32> x (nw/nodeset)`) defer (the member item is in-template for
   `ResolveUnitInScope`) — wiring those to the qualified member nodes is a natural
   K11a-3 companion.
4. **K11a-3.2 — composite-by-example member persistence LANDED (2026-07-27).** A
   `nw: network_links` parameter (UNIT exemplar) retains the exemplar through the
   function-spec side-assoc (`FunctionSpecData::paramTypeExemplars`, recorded in
   `ConfigProd::DoColonItemHeading` when the pending type exemplar is a unit and the
   declaration is a top-level parameter, flushed at `OnFunctionDeclEnd` — the exact
   `paramSigs` pattern). `ParamTypeImpl` then builds the member map from the EXEMPLAR's
   declared sub-items (`BuildParamMembers(p, dNode, exemplar)`): the K11a-3.1 ladder
   applies unchanged, node qualification stays on the PARAMETER's name (two by-example
   parameters of one exemplar are distinct rigid units), the member source's own name
   also selects the domain default, and only declared kind/type is read — a
   data-carrying exemplar (fn_test.dms `network_links : [0,1,2]`) is fine (risk R-b).
   An unresolved/absent exemplar or a weak-ptr expiry defers exactly as before.
   **Adversarial review round (2 lenses, 4 agents, both findings reproduced with
   control pairs) caught one root defect, fixed before landing:** exemplar member
   tokens initially resolved through the FUNCTION's ladder, so a same-named telescope
   parameter or a same-named unit in the function's container CAPTURED them — false
   definition-time rejections of correct programs (and `dt == p->GetID()` wrongly let
   the caller-chosen parameter name select the exemplar's domain default). In
   by-example mode the non-sibling rungs are now ONLY the ValueClass vocabulary and
   `ResolveUnitInScope` anchored on the EXEMPLAR (its own lexical world); the
   function's generic variables, telescope parameters, and parameter name are never
   consulted; unresolvable tokens defer.
   Coverage: `fn_test_byexample` (member-path result domain + member sum through the
   exemplar), `fn_test_byexample_neg1` (float `F1` in the exemplar → DEF-TIME `pcount`
   rejection — previously only surfaced per instantiation), `fn_test_byexample2`
   (the shadowing scenarios: same-named telescope parameter + same-named unit in the
   function's container must not capture), and `fn_test.dms`'s original
   `connectedness(nw: network_links)` now def-time-typed.
5. **K11a-3 — instantiation-point contract check LANDED (2026-07-28).** Two parts:
   **(def-time)** `InferParamMember` now reports a DIRECT-member miss under a complete
   declared interface as a definition error — "parameter 'nw' declares no member 'F3'"
   (the block is the §4.6 declared contract, regardless of what extra members an
   argument provides); DEEP paths defer (the argument may carry structure below a
   declared member). **(boundary)** `CheckStructuredParamContract` (sibling of
   `CheckFunctionSignature`) runs at argument binding in
   `FunctionApplication::ReduceValue` for a structured / by-example unit parameter
   whose argument is a PLAIN ITEM REFERENCE (`m_ArgItems` — the same config reference
   the body's member access binds to; expression arguments defer): each declared
   member must be present ("member 'F2' is missing"), kind-compatible, class/
   composition-compatible, satisfy generic constraints incl. cross-member consistency
   of a shared type variable (explicit blocks only), relate to the argument's own
   sibling member unit ("the values of 'F1' must be 'nodeset'"), and default-domain
   members must be attributes of the argument unit itself — every violation reported
   AT THE APPLICATION with parameter+member attribution. Unresolvable/null units
   defer, never misreport. Implementation trap (cost one round): the check must run
   against the argument's CONFIG item, NOT `DataController::MakeResult()` — the cache
   result unit carries no config members, so everything reported "missing".
   **Adversarial review round (2 lenses, 9 agents, all 7 findings reproduced with
   control pairs) — all fixed before landing:** (1+6) `ResolveName`'s return code 2
   is OVERLOADED (prelude refs / closure captures / def-scope externals also return
   2 without touching `paramIdx`/`genSubPath`) — the dispatcher now consults
   `ExtRefKind` and only a genuine `ParamMember` reaches the member map (the stale
   defaults falsely hit parameter 0 with an EMPTY path, rejecting any structured-
   first-param function referencing a bare external); (2) a VOID actual-member
   domain (a `parameter<>` member) broadcasts — `UM_AllowVoidRight` covers only a
   void RIGHT operand, so the check now skips void-left explicitly; (3) the contract
   is gated to FUNCTION items (`!isPlainTemplate`) — a classic template's
   rule-bearing unit-parameter locals are not a member contract and `apply` on such
   templates must keep working; (4+7) declared CONTAINER members now enter the
   member map as deferred (Unknown) entries — dropping them while
   `membersComplete=true` made a direct `nw/meta` reference a false "declares no
   member" definition error; (5) the boundary check's values ladder now mirrors
   `BuildParamMembers` (sibling → generic variable, testing `IsOwnDeclaredVar` OR
   `IsGenericVarOf` → ValueClass name).
   Coverage: `fn_test_network_neg2` (missing member), `fn_test_network_neg3` (wrong
   sibling relation + wrong member class), `fn_test_memnf_neg` (def-time undeclared
   member), `fn_test_structcontract` (the four fixed false-rejection scenarios as
   positives: external beside a structured first param, void member, apply-template
   with helper local, container member access). v1 deferrals: members over telescope
   parameters / generic domain variables (checked transitively by the body),
   expression arguments, the `instantiate` path (does not pass through
   `ReduceValue`'s binding loop), deep member paths.
6. **K11a-4 — container-kind parameters LANDED (2026-07-28).** `container cfg { … }`
   (explicit block) and `cfg: Settings` (by-example CONTAINER exemplar — ConfigProd
   now retains container exemplars for parameters, gated on `inParamList`) type as
   `DefType Kind::Container` with a member map. `BuildParamMembers` is now a
   RECURSIVE per-block walk producing a FLAT map keyed by full relative paths
   (`meta`, `meta/factor`), so DEEP member access (`cfg/nested/offset`) types at the
   definition; declared container members insert a deferred entry AND recurse;
   nested blocks pass no enclosing-unit node (default-domain members defer there);
   qualified rigid-node tokens carry the full path (`p/meta/subunit`); nested UNIT
   members' sub-items stay deferred. `CheckStructuredParamContract` recurses into
   declared container members (presence + the same per-member checks; the
   default-domain membership claim applies only at a unit parameter's top block; the
   root guard is kind-aware). Member-less plain parameters (`item x`) still defer.
   **Adversarial review round (2 lenses, 8 agents, findings reproduced) — 5 fixed
   in the follow-up commit:** (1) plain TEMPLATES and type-ALIAS exemplars
   (`IsTemplate`, not `IsFunctionItem`) were treated as required members and
   recursed into, enforcing a template's INTERNALS on the argument — both walks now
   skip `IsTemplate() || IsFunctionItem()` items (the same exemption the K11a-3
   plain-template gate established); (2) `membersComplete` was claimed from an
   exemplar whose member set is OPEN (a storage manager generates layer sub-items at
   `UpdateMetaInfo`, a calculation rule contributes composite members) — that made a
   body reference to a generated member a false def-time error with a
   *timing-dependent* verdict; `ExemplarMemberSetIsClosed` now gates it (an
   explicitly written block is always closed: it declares an interface, not an item);
   (3) a by-example exemplar's INCIDENTAL sub-containers were hard-required on every
   argument — by-example never requires container members; (4) an undeclared DEEP
   member under a COMPLETE nested block escaped the closed-interface error (deep
   paths deferred wholesale) — a deep miss now reports when its parent path is a
   declared block the walk actually entered; (5) nested violations named only the
   leaf — messages now carry the member PATH (`nested/offset`). Implementation trap
   worth remembering: `SharedStr` has **no** `(begin, end)` ctor, so a two-pointer
   call silently binds to `SharedStr(zStr, debugSrcName)` and yields the WHOLE
   string — use `CharPtrRange`.
   Coverage: `fn_test_containerparam` (explicit + nested deep member + by-example),
   `_neg1` (missing NESTED member + wrong class at the boundary), `_neg2` (def-time
   undeclared direct member), `_neg3` (undeclared DEEP member under a complete
   nested block), `fn_test_containerparam2` (the fixed false-rejections as
   positives: template/alias in the block, incidental exemplar sub-container, OPEN
   exemplar whose rule contributes members). Reduction needed NO changes
   (container params + deep member access already reduced correctly — verified by
   probe before the tranche).
7. **K11b** operator `ArgContainer` linking — substantial; gated on K11a.

**Risks.** (R-a) member DefTypes must use *per-instantiation* identity nodes so two applications of `connectedness` don't force their `nodeset`s equal (the same instance-key discipline as WP4.1 T3, `TypeUnifier` keyed `(owner, instance, token)`). (R-b) composite-by-example exemplars can carry *data* (fn_test.dms `network_links` has `[0,1,2]`) — the checker must read only the declared kind/type, never the values. (R-c) soundness of the def-time "member not found": only report when `membersComplete` (as the result-side already gates, `:2881-2884`). (R-d) instantiation check must not double-report what the body already catches — prefer the boundary message and suppress the transitive one, or accept both (boundary first).

---

## 6. Test plan

Mirror the empirical probes as regression cases (positive + `_neg`, computed via `/checks`):
- `fn_test_network` — valid `connectedness(network_links)` (positive; the fn_test.dms case, isolated).
- `fn_test_network_neg1` — `F1`/`F2` are `float32`: **def-time** rejection (not a body `pcount` error).
- `fn_test_network_neg2` — `F2` missing: **def-time** "member not found" / instantiation "member missing".
- `fn_test_network_neg3` — `nodeset` a float unit / `F1`,`F2` to *different* node units: shared-domain violation.
- Confirm the full battery stays green and the messages name the **parameter/instantiation**, not the copied body item.
