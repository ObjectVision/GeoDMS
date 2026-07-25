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

1. **K11a-1** `ParamType` builds the container DefType (a) — moderate; reuses `PositionType`. Unblocks (b).
2. **K11a-2** `ResolveName` member-access typing (b) — moderate; new resolve code threaded through `InferExpr`.
3. **K11a-3** instantiation-point contract check (c) — moderate; reducer-side, independent of 1–2.
4. **K11b** operator `ArgContainer` linking — substantial; gated on K11a.

**Risks.** (R-a) member DefTypes must use *per-instantiation* identity nodes so two applications of `connectedness` don't force their `nodeset`s equal (the same instance-key discipline as WP4.1 T3, `TypeUnifier` keyed `(owner, instance, token)`). (R-b) composite-by-example exemplars can carry *data* (fn_test.dms `network_links` has `[0,1,2]`) — the checker must read only the declared kind/type, never the values. (R-c) soundness of the def-time "member not found": only report when `membersComplete` (as the result-side already gates, `:2881-2884`). (R-d) instantiation check must not double-report what the body already catches — prefer the boundary message and suppress the transitive one, or accept both (boundary first).

---

## 6. Test plan

Mirror the empirical probes as regression cases (positive + `_neg`, computed via `/checks`):
- `fn_test_network` — valid `connectedness(network_links)` (positive; the fn_test.dms case, isolated).
- `fn_test_network_neg1` — `F1`/`F2` are `float32`: **def-time** rejection (not a body `pcount` error).
- `fn_test_network_neg2` — `F2` missing: **def-time** "member not found" / instantiation "member missing".
- `fn_test_network_neg3` — `nodeset` a float unit / `F1`,`F2` to *different* node units: shared-domain violation.
- Confirm the full battery stays green and the messages name the **parameter/instantiation**, not the copied body item.
