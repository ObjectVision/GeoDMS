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
   STILL DEFERRED: the composite-type-by-example form (`nw: network_links`, whose exemplar
   members must be persisted — today `ConfigProd.cpp:888` clones only the class).
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
4. **K11a-3** instantiation-point contract check (c) — moderate; reducer-side; also reports a **missing** declared member (def-time, `membersComplete`).
5. **K11b** operator `ArgContainer` linking — substantial; gated on K11a.

**Risks.** (R-a) member DefTypes must use *per-instantiation* identity nodes so two applications of `connectedness` don't force their `nodeset`s equal (the same instance-key discipline as WP4.1 T3, `TypeUnifier` keyed `(owner, instance, token)`). (R-b) composite-by-example exemplars can carry *data* (fn_test.dms `network_links` has `[0,1,2]`) — the checker must read only the declared kind/type, never the values. (R-c) soundness of the def-time "member not found": only report when `membersComplete` (as the result-side already gates, `:2881-2884`). (R-d) instantiation check must not double-report what the body already catches — prefer the boundary message and suppress the transitive one, or accept both (boundary first).

---

## 6. Test plan

Mirror the empirical probes as regression cases (positive + `_neg`, computed via `/checks`):
- `fn_test_network` — valid `connectedness(network_links)` (positive; the fn_test.dms case, isolated).
- `fn_test_network_neg1` — `F1`/`F2` are `float32`: **def-time** rejection (not a body `pcount` error).
- `fn_test_network_neg2` — `F2` missing: **def-time** "member not found" / instantiation "member missing".
- `fn_test_network_neg3` — `nodeset` a float unit / `F1`,`F2` to *different* node units: shared-domain violation.
- Confirm the full battery stays green and the messages name the **parameter/instantiation**, not the copied body item.
