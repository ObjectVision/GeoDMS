# Operator unit-constraint signatures via a virtual interface

**Status: ALL BATCHES SHIPPED (0+A 2026-07-19, U/B/C/D 2026-07-19, E/F 2026-07-20) —
the infrastructure, the walker's record applier, and the full family coverage
(attribute/casted/min_elem, relational, aggregations, fresh-unit, composites) are
implemented; the interim `OperSigKind` registry is retired; the printer is wired into
`FindOper`'s failure message and the `XML_ReportOperGroup` doc surface. See §12.1–§12.6
for the as-shipped decisions per batch, which refine §5.5/§6 (member-class TUPLES, soft
support sets, no trial harness in v1). The §8/S11 `UM_AllowRightExpansion` prerequisite
shipped 2026-07-17. Companion to `typed-hof-language-design.md` (WP4.1 "operator
signature reification"), which this document supersedes for everything beyond its
interim batch 1. Read §1.1 and Part II (§16–§20) first — the 2026-07-16 staging ruling
governs how §2–§15 are read.**

## 1. Purpose

The definition-time typed body walker (`FunctionChecker` + `TypeUnifier`,
`rtc/dll/src/tic/AbstrCalculator.cpp`) needs to know what constraints an operator
imposes on its arguments, and how the operator's result type derives from them. Today it
learns this from a hand-written stopgap: an `OperSigKind` enum with four shapes and a
static map of ~22 operator names (`AbstrCalculator.cpp:3009-3051`).

That stopgap models the arithmetic core and nothing else. It cannot express what
composite operators (`discrete_alloc`, `impedance_matrix`) and — more importantly — the
*entire relational family* (`lookup`, `rlookup`, `index`, `invert`, aggregations)
require, which is precisely the information needed for expression type derivation and
operator selection.

This document designs the replacement: **operators present their unit constraints
through a virtual interface function**, in a form that a unifier can consume, a printer
can render, and a debug build can verify against reality.

Deliberate non-goals: no change to `CreateResult` semantics, to LispRef keys, to
DataController behavior, or to application-time `FindOper` selection. This is a
*checking and description* layer. It is designed so that application-time selection
could later adopt it, but that adoption is out of scope.

### 1.1 The staging contract (governing ruling, 2026-07-16)

Everything below serves one architectural decision that must be read first, because it
sets the boundary between what the description layer does at definition time and what it
leaves to instantiation:

> **Definition time is symbolic.** A generic formal's units are *placeholders* — the
> existing rigid/skolem unifier variables, **never materialized as `AbstrUnit`s**. At
> definition time the walker does the free-fragment structural checks (unit *identity*
> relations: "the K2 thing for `sum`, `rlookup`") and nothing else. It does **not** run
> `CreateResultCaller`, the operator's own `CreateResult` unit machinery, `UnifyValues`,
> or metric derivation. Composite operators (`discrete_alloc`, `impedance_matrix`) are
> **opaque** — their result is ⊤ ("something"), and a declared result type is trusted.
>
> **Instantiation is concrete.** All placeholders are gone; every argument has an actual
> unit, metric and unifiability, and `FuncDC_CreateResult` → `CreateResultCaller`
> (`MoreDataControllers.cpp:627-653`) runs the full concrete semantics exactly as today.
> This is where declared-type contracts are discharged and where the composites are
> actually checked.

Two consequences fixed on 2026-07-16: **concrete stays concrete** — placeholders arise
*only* where the formal's type is generic; a signature naming a real config unit keeps
that unit and the walker's own symbolic comparison on it stays (so the exclusion list
means precisely "the operator's `CreateResult` is not invoked at definition time", not
"the walker ignores units"). And an **underdetermined bare §5.12 intermediate warns and
defers, never errors** (the S1 rule: never error where deferring succeeded).

This ruling is why the naive alternative — fabricating placeholder units and probing the
operator with them — is not merely unnecessary but unsound; see §19. The staging model is
developed in §16, the declared-type inference boundary in §17, the class-selection oracle
that *does* survive the exclusion list in §18, probing's proper home in §19, and the
type-theoretic account in §20. The sections §2–§15 describe the description layer's
mechanism; read them through the lens of §16.

## 2. What exists today

### 2.1 The class layer — real, but only half a signature

`Operator` stores a per-position class array and a result class, exposed as:

- `ClassCPtr GetArgClass(arg_index i)` and `ClassCPtr GetResultClass()` —
  `rtc/dll/src/tic/Operator.h:90-91`, backed by `m_ArgClassesBegin/End` + `m_ResultClass`
  (`:104-106`); `NrSpecifiedArgs()`/`NrOptionalArgs()` (`:88-89`).

A `ClassCPtr` is a `Class*` (a `DataItemClass`/`UnitClass`): it identifies the **value
type** and, for data items, the **value composition**. It carries no domain unit, no
values-unit identity, and no metric. This layer exists to drive overload selection:
`AbstrOperGroup::FindOper(nrArgs, argTypes)` walks the group's member list and matches
positions with `IsDerivedFrom`, first-match-wins over that list (`OperGroups.cpp:355-431`).
The list is in **reverse registration order** — `Register` *prepends* (`OperGroups.cpp:213-217`)
— and registration order is static-init order across TUs, so "first match" is
link-order-dependent; a def-time reuse of `FindOper` inherits this identically (which is
exactly what makes it exact, §18). `FuncDC::GetOperator` feeds it the argument DCs' result *classes*
(`MoreDataControllers.cpp:494-522`) — classes only; no units are available at that point.

### 2.2 The unit layer — real, but imperative and unreadable

Everything about *units* lives inside each operator's `CreateResult`, as straight-line
code:

- **Domain relationships** are `UnifyDomain` calls with role strings. Binary attribute
  operators pick the non-void operand's domain and unify:
  `e1Void ? e2 : e1`, then `e1->UnifyDomain(e2, "e1", "e2", UM_Throw)`
  (`clc/dll/include/OperAttrBin.h:52-57`).
- **Values-unit derivation** is a `UnitCreatorPtr` — a small named vocabulary
  (`rtc/dll/src/tic/UnitCreators.h`): `compatible_simple_values_unit_creator` (`:125`,
  args must share a values unit), `mul2_unit_creator` → `operated_unit_creator(&cog_mul,…)`
  (`:72`, metric product), `div_unit_creator` (`:87`), `compare_unit_creator` (`:135`,
  check + Bool), `boolean_unit_creator` (`:130`), `count_unit_creator` (`:147-158`, a
  fresh unit sized from the argument's domain), `arg1_values_unit` (`:107`),
  `domain_unit_creator` (`:141`), `cast_unit_creator` (`:165`),
  `default_unit_creator_and_check_input` (`:50`, dimensionless input), `inv`/`square`/`mulx`.

Neither is reachable from a signature accessor. The *only* signature description that
exists is `GenerateArgClsDescription` (`OperGroups.cpp:328-344`), which prints
`"argN of type <ClassName>"` — classes again.

### 2.3 The stopgap this replaces

`FindOperatorSignature`/`InferOperator` (`AbstrCalculator.cpp:3009-3093`):
`OperSigKind{SameUnit, Compare, Logical, FloatFunc}`, `struct OperSig{kind, minArgs,
maxArgs}`, a `std::map<TokenID, OperSig>` of add/sub/mul/neg/min_elem/max_elem/
MakeDefined, eq/ne/lt/le/gt/ge, and/or/not, sqrt/exp/log/sin/cos/tan.

`InferOperator` builds **one** shared value node and **one** shared domain node per
application, unifies every argument position against it, and overrides the result to
`bool` for `Compare` (`:3069-3092`). Arity outside `[minArgs,maxArgs]` defers (`:3060`).
Operators with no entry: the walker still walks the arguments, then returns
`DefType{}` = `Unknown` (`:3196-3200`) — a *deferral*.

Its expressive ceiling is exactly "all positions share one unit". That is why the
relational family is absent: `lookup` needs *three* distinct units in a specific
relationship, not one.

## 3. The census: what operators actually constrain

Taxonomy derived from a sweep of `clc/`, `geo/`, `rtc/dll/src/tic/`. Notation `V[D]` =
attribute with values unit `V` over domain unit `D`.

| # | Constraint kind | Representative evidence |
|---|---|---|
| K1 | same-domain-of-args (usually with void broadcast) | `OperAttrBin.h:52-57`; `OperAccUni.h:150`; `Index.cpp:285-287` (subindex, 3 args); `Subset.cpp:344`; `OperAccBin.h:162-163` |
| K2 | **values-of-A == domain-of-B** (the join-key identity) | `lookupImpl.h:74` `arg1Values->UnifyDomain(arg2Domain,"v1","e2",UM_Throw\|UM_AllowDefaultLeft)`; `DiscrAlloc.cpp:1609` (`ggTypes2part.values == partNames.domain`); `Dijkstra.cpp:1394-1395` (node-rel values == Node unit, twice); `Index.cpp:287` (subindex arg1.values == E) |
| K3 | result-domain = domain-of-A | `lookupImpl.h:79`; `Subset.cpp:583`; `Connect.cpp:674-679` |
| K4 | result-values = domain-of-A (relation-forming) | `RLookupImpl.h:62` (result `E[D]`, E = arg2's domain); `Index.cpp:61` (index → `E[E]`); `Invert.cpp:71` (`B[A]` → `A[B]`); `Connect.cpp:311`; `OperDistrict.cpp:66` |
| K5 | result-domain = values-of-A (partition drives result domain) | `OperAccUni.h:152-157` (`p2 = arg2.values`; result `UnitCreator[p2]`); `OperAccBin.h:165-176`; `PCount.cpp:88-96` |
| K6 | fresh existential result unit (count set at calc) | `Unique.cpp:345` `CreateResultUnit` + `Values` member `:350`; `Subset.cpp:134` + `org_rel` `:144`; `Union.cpp:89`, `:216`; `Connect.cpp:970`; `OperDistrict.cpp:61`; `Dijkstra.cpp:1547` |
| K7 | compatible-values across args (ValueType+metric+projection) | `RLookupImpl.h:61`; `Union.cpp:86`; `Union.cpp:253` (union_data, skipping arg 0) |
| K8 | result-values = values-of-A (borrow) | `lookupImpl.h:79`; `Subset.cpp:348`; `AggrFuncNum.h:315,337` (`arg1_values_unit`, min/max) |
| K9 | result-values via UnitCreator (count/cast/default/product) | all aggregations; all attribute operators; `PCount.cpp:88-96` |
| K10 | metric product / quotient / power (Kennedy algebra) | `UnitCreators.h:72,87,93,96`; `OperUnit.h:95-220`; `Metric.cpp:243-311` |
| K11 | container-of-attrs-over-shared-domain | `DiscrAlloc.cpp:1729` (each suitability member's domain == allocUnit) + `:1742` (all share one price values unit); `Subset.cpp:295,504` (select_with_attr / collect_attr metas) |
| K12 | per-member cross-container link, keyed by member name | `DiscrAlloc.cpp:1660,1667` (min/maxClaims[type].domain == that type's partitioning unit; member found by NameID, `GetClaimAttr:1498`) |
| K13 | value-read-at-meta-time (a value decides arity/roles) | `Dijkstra.cpp:1324-1331` (spec string → `ParseDijkstraString` → `CalcNrArgs`); `DiscrAlloc.cpp:3582,3605` (name arrays, `calc_always`); `oper_policy::dynamic_argument_policies` (`OperPolicy.h:34`) |
| K14 | fixed-value-type check (not a unification) | `Dijkstra.cpp:1422-1543` (`dynamic_cast<Unit<MassType/ImpType>>`); `DiscrAlloc.cpp:1508` (claims must be UInt32) |
| K15 | void-domain result (aggregate to scalar) | `OperAccUni.h:51-55`; `OperAccBin.h:53-58`; `DiscrAlloc.cpp:3368-3384` |
| K16 | shared coordinate values unit for geometry pairs | `Connect.cpp:300`, `:646`, `:955` (`UnifyValues` on point/poly coords) |

Two observations drive the whole design:

1. **The relational family is all about K2/K4/K5** — units appearing in a *values* role
   in one position and a *domain* role in another. `lookup(rel, values)`: `rel`'s values
   unit *is* `values`' domain. That is the single most common non-trivial constraint in
   real configs, and it is exactly what `OperSigKind` cannot say.
2. **Composites are mostly deferrable.** `discrete_alloc` and `impedance_matrix` do have
   expressible constraints (K1/K2 among their scalar/attribute arguments), but their
   defining complexity — containers with name-keyed members (K11/K12), spec strings that
   decide the argument layout (K13) — is not statically checkable at all. The design must
   let them describe what they can and *declare the rest as deferred*, with prose good
   enough to print. Their real near-term win is documentation and diagnostics, not
   unification.

## 4. The verified blocker: values-side unit identity

`DefType` (`AbstrCalculator.cpp:2482-2504`) models a term's value position as
`const ValueClass* vc` XOR `SizeT vNode` (a `ValueNode` index) — **a class, never a
unit**. Unit identity exists only on the domain side: `Dom::Concrete` holds an
`AbstrUnit*`, `Dom::Node` indexes a `DomainNode`, and `DomainNode` is the only node type
carrying `const AbstrUnit* bound` (`:1464-1472`).

The two pools are disjoint (`m_ValueNodes` / `m_DomainNodes`, `FindV` / `FindD`,
`LinkValue` / `LinkDomain`, `:1473-1479`) and `UnifyData` never bridges them: it unifies
value positions against value positions and domain positions against domain positions
(`:2810-2837`).

**Therefore K2 is currently inexpressible**: no channel exists for "this value position
and that domain position denote the same unit". `DomainNode` is structurally exactly
what such a constraint needs (union-find + `AbstrUnit*` identity + `UnifyDomain`
comparison + rigid semantics). The split is the blocker, and §8 removes it.

## 5. The design

### 5.1 The virtual — the member describes, the group aggregates

```cpp
// rtc/dll/src/tic/Operator.h
class Operator {
    ...
    // Describe this operator's unit constraints declaratively.
    // Returns false (the default) when undescribed: every consumer then defers.
    TIC_CALL virtual bool DescribeSignature(AbstrSignatureBuilder& sb) const;
};
```

The virtual sits on `Operator` (the member), not on `AbstrOperGroup`:

- Family knowledge lives in member state (`m_ArgClasses`, the unit-creator, the value
  composition). A group-level description would have to reach into members anyway.
- Groups are keyed by name only (~2310 of them) and are frequently *heterogeneous*:
  `sum` has a unary total member (`AbstrOperAccTotUni`, K15) and a binary partitioned
  member (`AbstrOperAccPartUni`, K5) with different arities and shapes; `cog_mul` holds
  unit×unit, attribute×attribute, and geo polygon members registered from other DLLs. A
  group presents a *set* of signatures, computed from its members (§5.5).
- Undescribed operators need zero code: the default `return false` reproduces today's
  graceful degradation exactly.

**Family base classes override once** for all instantiations — `AbstrBinaryAttrOper`
(`OperAttrBin.h:26`), `AbstrLookupOperator` (`lookupImpl.h:41`), `AbstrOperAccPartUni`
(`OperAccUni.h:~120`), etc. The description then sits in the same class as the
`CreateResult` it mirrors, so the two are read and reviewed together — the primary
defense against drift (§9).

### 5.2 Record-then-interpret

`DescribeSignature` is invoked **once per member**, into a *recording* builder that
produces a plain-data `SignatureRecord`. Every consumer interprets records; nobody
re-invokes the virtual per application.

Why records rather than a direct-to-unifier builder:

- Merging across members (§5.5) means **comparing** two descriptions structurally. You
  can compare values; you cannot compare callback streams.
- The walker checks each function once but re-walks many applications; re-running
  virtuals per application is waste and invites re-entrancy on the meta thread.
- One recorded form makes it impossible for the printer and the checker to disagree
  about what an operator claims.
- It forbids, by construction, a `DescribeSignature` that peeks at per-application
  state. Descriptions are static facts about a member.

The abstract builder interface still earns its place: family code depends only on the
narrow vocabulary, not on the record layout, and external tools (docs extractor, tests)
can implement it directly.

### 5.3 The builder vocabulary

```cpp
// rtc/dll/src/tic/OperSignature.h   (new; TIC_CALL)

using sig_var = UInt32;
constexpr sig_var no_sig_var = sig_var(-1);

enum class SigArgTraits : UInt8 { none = 0, no_void_broadcast = 1, categorical = 2 };

struct AbstrSignatureBuilder
{
    // --- unit variables: ONE sort, usable in BOTH domain and values roles (the K2 fix)
    // pass the label plainly; NewVar interns it 'sig_'-prefixed (#1161)
    virtual sig_var UnitVar(CharPtr role) = 0;             // "D", "E2", "V"
    virtual sig_var VoidDomain() = 0;                      // K15, parameters
    virtual sig_var DefaultUnit(const ValueClass* vc) = 0; // boolean/default_unit_creator
    virtual sig_var GeneratedUnit(CharPtr role) = 0;       // K6 existential

    // --- value-class constraints on a unit variable
    virtual void MemberValueClass(sig_var u, const ValueClass* vc) = 0; // this member's own class; merged by union
    virtual void FixedValueClass (sig_var u, const ValueClass* vc) = 0; // K14; identical across members
    virtual void ConstrainValueClass(sig_var u, TokenID genericConstraint) = 0; // "floats"; intersected

    // --- argument positions (0-based, aligned with GetArgClass)
    virtual void ArgName(arg_index i, CharPtr name) = 0;                 // diagnostics
    virtual void ArgAttr(arg_index i, sig_var values, sig_var domain,
                         ValueComposition vc, SigArgTraits = SigArgTraits::none) = 0;
    virtual void ArgUnit(arg_index i, sig_var u) = 0;
    virtual void ArgMetaValue(arg_index i, const ValueClass* vc, CharPtr meaning) = 0; // K13
    virtual void ArgContainer(arg_index i, CharPtr memberPattern,
                              sig_var sharedMemberDomain = no_sig_var) = 0;            // K11
    virtual void ArgDeferred(arg_index i, CharPtr note) = 0;
    virtual void RepeatArgs(arg_index fromPos, sig_var values, sig_var domain,
                            ValueComposition vc) = 0;                                   // variadic tails

    // --- relations beyond shared variables
    virtual void SameValueClass  (sig_var a, sig_var b) = 0;
    virtual void CompatibleValues(sig_var a, sig_var b) = 0;   // K7/K16; def-time = class equality
    virtual void MetricProduct (sig_var r, sig_var a, sig_var b) = 0;  // K10: declared, def-time-deferred
    virtual void MetricQuotient(sig_var r, sig_var num, sig_var den) = 0;
    virtual void MetricPower   (sig_var r, sig_var base, int exponent) = 0;
    virtual void Dimensionless (sig_var u) = 0;
    virtual void CastOf(sig_var r, sig_var src, const ValueClass* toCls) = 0;
    virtual void DeferredRelation(CharPtr note) = 0;           // K12: real, unexpressible, printable

    // --- result derivation, in the same variables
    virtual void ResultAttr(sig_var values, sig_var domain, ValueComposition vc) = 0;
    virtual void ResultUnit(sig_var u) = 0;
    virtual void ResultContainer(CharPtr memberPattern, sig_var sharedMemberDomain = no_sig_var) = 0;
    virtual void ResultDeferred(CharPtr note) = 0;

    // --- whole-signature deferral: shape depends on a meta-read value (K13)
    virtual void DynamicShape(CharPtr why) = 0;
protected:
    ~AbstrSignatureBuilder() = default;
};
```

**Arity is not restated.** `NrSpecifiedArgs`/`NrOptionalArgs`/`AllowExtraArgs` are read
off the `Operator`/group by the recorder, so a description cannot contradict the
registration.

**One unit sort.** `UnitVar` returns a variable that may appear in a values role
(`ArgAttr(i, u, …)`) *and* a domain role (`ArgAttr(j, …, u)`). That single decision is
what lets `lookup` be written down at all.

### 5.4 `unit_creator_spec`: pairing the derivation with the pointer

The values-unit derivation (K9/K10) is known today only as a `UnitCreatorPtr`. A
pointer-keyed lookup table is **not viable**: the creators are `inline` functions in a
header, so every DLL gets its own copy and pointers taken in clc/geo would not match a
table built in tic.

Instead the declaration and the pointer are bundled at the registration site:

```cpp
// rtc/dll/src/tic/UnitCreators.h (addition)
enum class UnitDerivationKind : UInt8 {
    Opaque,                          // unmigrated -> result values deferred
    ArgValues, ArgDomain,            // arg1_values_unit, domain_unit_creator
    Default, DefaultDimensionless,   // default_unit_creator<T>[_and_check_input]
    Boolean, CompareBool,            // boolean/compare
    CompatibleValues,                // compatible_[simple_]values_unit_creator
    Mul2, MulX, Div, Inv, Square,    // metric algebra
    Count, UniqueCount, Cast,
};
struct unit_creator_spec {
    UnitCreatorPtr     fn = nullptr;
    UnitDerivationKind kind = UnitDerivationKind::Opaque;
    arg_index          argIndex = 0;
    const ValueClass*  cls = nullptr;
    unit_creator_spec(UnitCreatorPtr f) : fn(f) {}  // implicit: legacy sites compile -> Opaque
    // named factories keep both halves in sync: uc_mul2(), uc_div(), uc_default<T>(),
    // uc_compatible_values(), uc_arg_values(0), uc_count(), uc_cast<Field>(), ...
};
```

Family bases store `unit_creator_spec` instead of `UnitCreatorPtr` and call through
`.fn`; the implicit conversion keeps every current registration compiling as `Opaque`
(= defer, safe). `DescribeSignature` maps `kind` mechanically onto builder calls. This is
what makes migrating hundreds of registrations tractable: convert the family base once,
then upgrade registration sites from `mul2_unit_creator` to `uc_mul2()` at leisure.

### 5.5 ∀-families: per-member description + congruence-gated merge

> **Demoted by §18.** The exact ∀-selector is *enumeration of the closed value-class
> universe against the real `FindOper`* (§18) — that is what carries soundness. The
> per-member merge described here survives only to derive the printable feasible sets
> for diagnostics and the signature browser; it is no longer soundness-critical. Read
> this section as "how the printer learns a family's accepted classes", not "how the
> walker decides an application".

An instantiated `BinaryAttrOper<Float32,Float32,Float32>` does not know its family ranges
over `numerics` — that fact lives in the registration typelist
(`BinaryInstantiation<TL,…>`). Conversely, a hand-written family-level constraint set can
silently disagree with what was actually instantiated, which would make a merge-based
selector **unsound** — the reason enumeration (§18), not the merge, is the selector.

**Decision:** each member describes *structure* with shared unit variables and binds its
*own concrete classes* via `MemberValueClass`. Generic tic code then:

1. records every member of the group,
2. computes a **structural fingerprint** per record — everything except the
   `MemberValueClass` payloads,
3. **merges** equal-fingerprint records: per variable, the feasible set = **union of the
   members' concrete classes**,
4. caches the result on the group.

Because each member binds a variable consistently at every occurrence, cross-position
co-variance is captured exactly per member; the merged per-variable set is a sound
envelope of the true relation. Over-approximation costs precision, never soundness (e.g.
an aggregation's `V → R` accumulator map becomes two independently-unioned variables).

The feasible sets are then **derived, never hand-written**: `sqrt`'s set is exactly the
instantiated floats; `add`'s automatically includes `string` because a concat member is
registered — no one maintains a comment saying "the constraint stays any" (as
`AbstrCalculator.cpp:3025` does today).

Members that are not congruent with the rest keep their own records; the group is marked
**mixed** and handled by §6.2's candidate loop.

```cpp
// rtc/dll/src/tic/OperGroups.h
struct AbstrOperGroup {
    TIC_CALL const OperGroupSignatures* GetSignatures() const;  // nullptr if no member describes
private:
    mutable std::unique_ptr<OperGroupSignatures> m_Signatures;  // lazy, once-guarded
    // Register(member) bumps a generation counter -> summaries rebuild (late-loaded DLLs)
};
```

Records hold only `ValueClass*` (static lifetime) and strings — no `AbstrUnit*` — so the
cache has no keep-alive concerns.

## 6. The consumers

### 6.1 The walker (unifier interpreter)

`InferOperator(const OperSig&, …)` is replaced by a record applier. For one application:

| description element | unifier action |
|---|---|
| `UnitVar(role)` | one fresh node per var per application: key `(owner=nullptr, inst, roleTok)` |
| `MemberValueClass` (merged set) | seed the class node's `feasible` from the union, with a rendered constraint source ("supported by operator 'sqrt': float32, float64") |
| `ConstrainValueClass(tok)` | the existing fallback-constraint path (as `t_gcFloatsTok` does today, `:3076`) |
| `FixedValueClass(vc)` / `DefaultUnit(vc)` | `BindValue(classNode, vc, src)` |
| `VoidDomain` | `DefType::Dom::Void` (broadcast semantics preserved by `UnifyData:2824`) |
| `GeneratedUnit` | fresh **flexible** node (see below) |
| `ArgAttr(i, v, d, vc)` | build a positional `DefType` and `UnifyData(argTerm[i], posT, …)` |
| `SameValueClass` / `CompatibleValues` | `LinkValue` of the two class nodes |
| `Metric*` / `Dimensionless` / `CastOf` / `DeferredRelation` | **no unifier action** (printer + verifier only) |
| `ArgContainer` / `ArgMetaValue` / `ArgDeferred` / `DynamicShape` | position (or whole application) yields `Unknown`; arguments are still walked |
| `ResultAttr` / `ResultUnit` | result `DefType` in the same nodes |

**Owner = `nullptr`, fresh `m_NextInstance++` per application, node names = role tokens.**

**Role labels are interned `sig_`-prefixed (#1161).** `SignatureRecorder::NewVar` adds the prefix, so
describe sites keep writing plain `"D"` / `"Imp"` while the registry holds `sig_D` / `sig_Imp`. The labels share
the case-folded token registry with configuration identifiers, and a bare `"D"` folds onto the `d`
numeric-literal suffix (`ExprProd.cpp`) and `"Imp"` onto a modeller's `imp` -- each raising a depreciated
case-mixup warning about a name the modeller never wrote. `sig_*` stays a legal item name but was measured
against the regression corpus + battery and occurs there NOWHERE, where a trailing
`_` still collided (`U_`, `E_`, `A_`, `D_`) and so did a leading one (`_P`, `_B`, `_A`). Synthetic skeleton
roles are spelled out (`sig_arg0`, `sig_res`) so they cannot fold onto a one-letter label.
Rendered signatures and type-error text therefore read `attribute<sig_V>(sig_D)`.
Operators have no `TreeItem` identity and inventing one buys nothing: the instance
already isolates applications, and `DeclaredConstraintOf(nullptr, …)` correctly returns
none (`:1427-1428`), so constraints flow exclusively from the description. Role tokens
instead of today's `headID`-as-name (`:3075`) upgrade every existing message for free
("type variable 'V'" rather than "type variable 'mul'"). Attribution belongs in the
constraint *source strings*, which the interpreter composes.

**Rigid safety needs no new mechanism.** Signature nodes are always fresh and non-rigid,
so every interaction with the checked function's skolems flows through the existing
`LinkValue`/`LinkDomain` rigid rules (`:1552-1653`): rigid never bound concrete,
rigid-rigid never merged, flexible-into-rigid subject to the ∀ feasibility-subset check.

**K6 fresh units = fresh *flexible* nodes — not rigid, not concrete-unknown.**

- *Rigid would be unsound.* Two textually identical `unique(x)` applications reduce to
  the same DC key and therefore the *same* cache unit. Two rigid nodes would trip the
  rigid-rigid error (`:1638`) on a config that is perfectly legal — the prohibited
  failure mode.
- *Flexible is sound.* Inside a body, `Dom::Concrete` units come only from the definition
  scope (`ResolveUnitInScope`, `:2552-2562`, which excludes body-local units), so a
  concrete unit can never *be* the fresh cache unit; binding fresh→A then conflicting
  with B errors only on configs that also fail at reduction. A single conflicting use is
  merely missed (deferred) — conservative, allowed.
- *Rejected:* `Dom::Unknown`, which would lose same-application aliasing entirely for no
  safety gain.
- *Later tightening (optional, after batch D soaks):* a `generative` flag refusing
  bind-to-concrete and link-to-rigid with dedicated messages ("the result of `unique(…)`
  is a new unit; it cannot be the caller-supplied domain 'D'").

**Prerequisite for K6 soundness:** memoize application results per hash-consed `LispPtr`
within a checker run. Two occurrences of `unique(a)` in one body denote one DataController;
without memoization they would mint two generative nodes and a later equality requirement
would falsely error. With interned LispRefs this is exact and cheap, and it de-duplicates
diagnostics for repeated subexpressions as a bonus.

**Unknown arguments: partial unification stays on.** `UnifyData` already returns early on
`Unknown` (`:2805`). Every judgment the *known* arguments support is asserted; Unknown
positions contribute nothing but suppress nothing. This is strictly more checking than
deferring the whole application, and can never over-constrain, because any constraint
asserted from a known argument is one reduction will assert too. (The one consumer that
must *not* use Unknown positions is candidate elimination, where missing information must
widen the candidate set, not narrow it.)

### 6.2 Overload selection

Per application, against the group's signatures:

1. **Arity filter** — respecting `NrOptionalArgs` and `allow_extra_args`/`RepeatArgs`.
   An arity mismatch **always defers, never errors**: `FindOper` has two widening escape
   hatches the walker must not second-guess (optional-arg dropping, and the trailing
   `calc_as_result` skip for non-caching groups, `OperGroups.cpp:376-384`).
2. **Cheap class filter** — no unifier mutation: where an argument has a *concrete* class
   (or a bound node), intersect with the candidate variable's feasible set; empty ⇒ out.
   Unknown eliminates nothing.
3. **Verdict:**
   - exactly one candidate ⇒ apply it (§6.1);
   - several ⇒ **defer** (v1; committing the intersection of their implications is a
     later refinement);
   - zero **and** every arity-surviving member was described ⇒ **definition-time error**,
     listing candidates via the printer (sound: sound elimination plus failed unification
     for every member implies reduction fails too);
   - any surviving member undescribed ⇒ **defer** — it might be the one reduction picks.
   - `dynamic_result_class` members need no special case: the *description* carries the
     result scheme, which is strictly more informative than `m_ResultClass`; if
     undescribed, it defers like anything else.
   - `IsTemplateCall()` groups (user functions/templates) are unreachable here — the
     walker branches to the function path first (`:3133`).

**Speculation — `TypeUnifier` has no rollback today. Decision: copy-trial-adopt.**
Snapshot `{m_ValueNodes, m_DomainNodes, m_ValueVarIndex, m_DomainVarIndex}` plus
`m_NextInstance`; run a candidate inside a try/catch (unification failures already throw);
on the unique success, adopt the successful copy. Nodes are append-only and `DefType`s
hold stable indices, so the adopted copy descends from the committed state plus exactly
one candidate's effects; failed trials are discarded wholesale. Definition-time only,
memoized once per function, single-digit trial counts.

- *Rejected (for now): a trail/undo log.* Cheaper at runtime, but it taxes every future
  `TypeUnifier` mutation with trail discipline — on a struct that is about to undergo the
  UnitNode surgery. Keep it as the profiling-driven optimization; shape the boundary as
  `BeginTrial()/CommitTrial()/AbortTrial()` so the two are interchangeable.
- *Rejected: two-phase dry-run.* A candidate's own description links nodes mid-trial
  (position 2 must see position 1's merge), so a side-effect-free checker needs an
  overlay union-find — the trail with extra steps and double evaluation.

Implementation note: trial errors are thrown `ExprParser` exceptions; the harness must
catch them **only** inside the candidate loop, record them as candidate-failure
diagnostics, and never let one escape as a real definition error — nor catch anything
else. Materialize every name into `SharedStr` before the trial loop (token-registry lock
discipline; the pattern at `:3063`).

### 6.3 Diagnostics and the printer

All constraint flow reuses the existing `ConstraintRec`/source-string machinery
(`:1453`, `:1518-1530`), which already renders `"{m_Phase}'{item}': …"`. The interpreter
composes sources from three layers — application ("operator 'sum'"), position ("argument
2 of operator 'sum'"), and the description's role text ("the partitioning attribute"):

> in the definition of 'F': the domain of argument 1 of 'sum' (the values to aggregate)
> must equal the domain of argument 2 (the partitioning attribute)

Two small extensions: a `ValueVar` overload seeded from an explicit set plus rendered text
(so `CheckFeasible:1524-1529` says "one of: float32, float64 (supported by operator
'sqrt')" instead of naming a constraint token); and an optional role suffix in
`boundSource` so "inconsistent instantiation of domain variable 'D'" names the operator
role that pinned it.

**Relationship to the runtime `UnifyError` lexicon** (`AbstrUnit.cpp:236-262`, where
`Relabel("e2")` → "Domain of second argument"): the runtime path is untouched — it fires
with concrete units and metrics in hand, and its message content is unreachable at
definition time. Def-time messages deliberately adopt the *same words* so both phases read
as one voice, but generate them from described roles rather than the `"e1"/"v2"` literals
scattered through `CreateResult` preambles. **Rejected:** deriving those preamble role
strings from descriptions ("single source of truth") — it would turn a checking-layer
change into a behavior-adjacent edit across ~200 operators; the debug verifier (§9) keeps
the two honest instead.

**The printer** is the second interpreter over the same records:

```
mul(a: attribute<V>(D); b: attribute<V>(D)) -> attribute<V*V>(D)
    where V in {uint8, ..., float64}
lookup(org_rel: attribute<E2>(D); values: attribute<V>(E2)) -> attribute<V>(D)
sum(values: attribute<V>(D); partitioning: attribute<P>(D)) -> attribute<R>(P)
unique(values: attribute<V>(D)) -> unit<crd(D)> U [new] { Values: attribute<V>(U) }
impedance_matrix(spec: string [meta: directs the remaining arguments]; ...) [shape depends on spec]
```

Uses: def-time error text; enriching `FindOper`'s failure message with described signatures
(**shipped batch E**, `OperGroups.cpp` — the `IsMetaThread()`-guarded loop over
`GetSignatures()->records`; see §12.5); and the durable `XML_ReportOperGroup` `<Signature>`
elements (**shipped batch F**, §12.6) — the first place `discrete_alloc`'s contract is stated
anywhere findable. Since batch F the printer also renders the deferred-result prose
(`-> ... [<note>]`) and the `DeferredRelation` notes (`[deferred: …]`) — which therefore are
USER-VISIBLE text and must stay symbolic (no source line numbers). Having two consumers is
itself a check on the vocabulary: anything the printer cannot render means the record is too
operational.

### 6.4 The debug verifier

See §9.

## 7. Coverage over K1–K16

| Kind | How expressed | Def-time effect |
|---|---|---|
| K1 | shared domain `sig_var`; void broadcast default-on (`no_void_broadcast` opts out) | full |
| K2 | the same `sig_var` in A's values role and B's domain role | full — **needs §8** |
| K3 | `ResultAttr(_, D, _)` | full |
| K4 | `ResultAttr(E, _, _)` with an argument's domain var | full — **needs §8** |
| K5 | `ResultAttr(_, P, _)` with an argument's values var | full — **needs §8** |
| K6 | `GeneratedUnit` → fresh flexible node | identity yes; count/range stay calc-time |
| K7 | `CompatibleValues(a,b)` | class equality; metric/projection deferred |
| K8 | shared var | full |
| K9 | `unit_creator_spec` kind → builder calls | per kind; `Opaque` ⇒ result values deferred |
| K10 | `MetricProduct/Quotient/Power`, `Dimensionless` | **declared-deferred, but *vacuous* not intractable** (see §20: the metric algebra is a decidable free abelian group; it defers only because a formal has no declarable metric today — hof §11, P4/v2); printer + verifier consume |
| K11 | `ArgContainer(i, prose, sharedDomain)` — **v1 diagnostics-only**, the domain var is recorded but not linked | deferred |
| K12 | `DeferredRelation(note)` | deferred; printed |
| K13 | `ArgMetaValue` (per arg) / `DynamicShape` (whole signature) | application defers; args still walked |
| K14 | `FixedValueClass` | class bound |
| K15 | `ResultAttr(V, VoidDomain(), vc)` | full |
| K16 | shared values var + `CompatibleValues` | class equality; rest deferred/verifiable |

**"Deferred" operationally** = the affected position/result yields `DefType::Kind::Unknown`;
`UnifyData`'s existing Unknown early-out (`:2805`) does the rest, and the argument
expressions are still walked for their own internal errors. A `DynamicShape` record
behaves exactly like an absent signature, except the printer can still render it.

**K11 recommendation — defer in v1.** Full member-pattern unification needs a container
kind in `DefType` (which does not exist) and container members are only enumerable at meta
time. That is a separate work package; the v1 vocabulary records the pattern so the
printer can state the contract, and the recommendation is to revisit only if real configs
show the checking would pay.

## 8. Prerequisite: the UnitNode generalization

**Status: SHIPPED 2026-07-19 (batch U), dark for operators.** As-shipped deviations from
the sketch below: (1) the companion class node is created **eagerly at unit-node
creation** under the SAME `(owner, instance, name)` key as `ValueVar` — not lazily — so
every existing class-side path (`ValNode`, signature bindings) converges on the same
node by construction; a `declaredCls` argument (`unit<uint32> U`: the identity is rigid
per-instantiation but the class is pinned by the declaration) makes the companion
**bound non-rigid** instead of rigid. (2) The `generative`/`genOrigin` K6 fields are
deferred to batch D. (3) The concrete-concrete values compare is single-direction under
`s_CheckerUM` (total and symmetric per S11) rather than the pre-S11 two-direction retry.
(4) Beyond the "dark" floor, the tranche activates the K2 bridge for **function
signatures**: a values-position token naming a unit parameter or domain-sorted generic
resolves to the same unit node its domain role uses, values tokens resolving to concrete
scope units carry the unit (`DefType::vUnit`), and `UnifyData`'s values-identity block
enforces the declared relationships (`fn_test_unitnode{,_neg1,_neg2}.dms` — the join-key
contract `unit<uint32> E; attribute<E> rel (D); attribute<V> vals (E)` is now checked at
definition/first-reference). Operator records still make no values-identity claims —
that is batch B. Unit-side diagnostics now say "unit variable" rather than "domain
variable" (role-neutral wording; S10 deliberate).

**Adversarial-review corrections (2026-07-19, pre-landing).** (a) The sketch below says
concrete-concrete values conflicts "error only if permissive `UnifyDomain` fails in both
directions" and that such configs are "already broken at reduction" — **both wrong for
values units**: reduction checks values units by `UnifyValues` (class + metric,
`UM_AllowDefaultLeft`, `AbstrDataItem.cpp:592`), under which two key-distinct metric-less
units of one class UNIFY; `UnifyDomain`-on-values runs only for categorical items. A
key-identity error would therefore reject configs that reduce fine (S1). As shipped, the
**concrete-vs-concrete arm defers**; values-unit identity is enforced only through a
declared unit-variable contract (the `vuNode` arms) — a surface that did not resolve at
all before this tranche, so no legacy config can be affected. (b) A unit parameter's
node can be created FIRST by a type-application/sig-binding path that does not know the
declared class; the companion then froze rigid/unconstrained and the verdict depended on
the body's reference order. Fixed centrally: `UNode` scans the owner's parameters for a
unit item of that name, and `UnitVar` reconciles a later-supplied declared class onto an
unbound non-rigid companion instead of dropping it. (c) The tok2owner values-role branch
now carries `vuNode` for domain-sorted targets like its siblings (the sig-typed-parameter
path no longer severs the K2 identity). (d) A pre-existing TokenStr-over-`FindItem`
self-deadlock hazard in `LinkSignatureBinding`'s concrete-domain fallback (confirmed
reachable via a path-formed domain reference on a bound function) is fixed by
materializing the `SharedStr` first.

**Change: `DomainNode` → `UnitNode` (one pool), layered over the existing class sort.**

```cpp
struct UnitNode {                    // was DomainNode
    SizeT parent; TokenID name; bool rigid = false;
    SharedTreeItem keepAlive; const AbstrUnit* bound = nullptr; SharedStr boundSource;
    SizeT classNode = NO_TYPE_VAR;   // NEW: lazily created ValueNode = class-of(this unit)
    bool  generative = false;        // K6
    LispPtr genOrigin;               // application identity for generative merges
};
```

- `ClassNodeOf(unitIdx)` lazily creates the companion `ValueNode` (rigid iff the unit node
  is rigid, seeded from the declared constraint via the existing `DeclaredConstraintOf`
  path). **All** class reasoning — feasible sets, rigid ∀ checks, diagnostics — continues
  to run on the unchanged `ValueNode` machinery.
- **Invariant:** `BindUnit` = old `BindDomain` + `BindValue(ClassNodeOf(i),
  u->GetValueType())`; `LinkUnit` = old `LinkDomain` + `LinkValue` of the class nodes when
  either exists. Confining the invariant to these two functions means no caller ordering
  can break it.

**`DefType`** gains a values-identity slot parallel to the domain triple:

```cpp
const ValueClass* vc = nullptr;   // unchanged meaning
SizeT vNode  = NO_TYPE_VAR;       // unchanged: class variable
SizeT vuNode = NO_TYPE_VAR;       // NEW: identity of the values unit, when known
// (+ vUnit/vKeep for a concrete values unit, mirroring domUnit/domKeep)
SizeT dNode = NO_TYPE_VAR;        // now indexes UnitNodes (rename only)
```

**`PositionType`** (`:2730-2753`) gains one branch: a values-position token naming a unit
parameter or domain-sorted generic of `fnDef` yields `vuNode = UnitNode(fnDef, instance,
tok)` — **the same node its domain role uses at `:2784`**. That single line is the bridge
the whole K2 story rests on. Class-sorted vars keep producing `vNode` as today.

**`UnifyData`** gains a values-identity block mirroring the domain block, with one crucial
difference: **concrete-concrete conflicts error only if permissive `UnifyDomain` fails in
both directions** — exactly the shape already used for domains at `:2846-2848`. Rationale:
runtime values-side relations run under operator-specific modes (lookup's
`UM_AllowDefaultLeft` borrowing, `lookupImpl.h:74`) and **metrics are deferred at
definition time** (§20: vacuous, not intractable), so def-time identity comparison must be
key-identity/default-borrowing tolerant and must never call `UnifyValues`-with-metric.
When in doubt the block returns (defers).

**Touch points** (the full list): `TypeUnifier` node/type/index renames and
`DomainVar/BindDomain/LinkDomain` → unit-role-neutral names; `DefType`; `PositionType`;
`UnifyData`; and the existing `DomNode` users — `ParamType` (`:2682`),
`LinkSignatureBinding`'s `targetD` (`:2902`), `InferOperator`'s `posT.dNode` (`:3078`),
the `DomNode()` helper (`:2548`) — all pure renames, behavior-identical while no values
identity is present.

**Back-compat risk and its mitigation.** The new error surface is values-identity conflicts
that used to be invisible. Each requires two *concrete* units (reachable only from scope
units, never body locals) failing permissive `UnifyDomain` in both directions — configs
already broken at reduction. Regardless: **land the UnitNode tranche dark** (interpreter
ignores `valuesIdentity`; no operator sets it), run the full battery, and only then let
the relational batch turn it on. Two-step landing isolates any surprise to one of the two
diffs.

**Hard prerequisite before this tranche — resolved 2026-07-17 with `UM_AllowRightExpansion`.**
`AbstrUnit::UnifyDomain` is *directional*: it interns the **left** operand's
DataController (`GetOrCreateDataController`) but by default only **looks up** the right's
(`GetExistingDataController`), so `a.UnifyDomain(b)` can fail where `b.UnifyDomain(a)`
succeeds, purely by DC-interning order. Archaeology showed the asymmetry is
**intentional**: commit `766848c9` (2023-08, issue #361) demoted the right side from
`GetOrCreate` to `GetExisting` because operator arg-shape guards re-run at `CalcResult`
time on **worker threads**, and DC *creation* is meta-thread-only (the same commit relaxed
`GetDataControllerImpl`'s gate to `MG_CHECK(IsMainThread() || !mayCreate)`); its TODO —
"prevent UnifyXXX() to be called from WorkerThreads" — is still open.

The resolution keeps that fix intact and makes the capability an explicit caller
contract: a new `UnifyMode` flag **`UM_AllowRightExpansion = 64`** (`AbstrUnit.h`) lets a
caller that *guarantees* meta-thread execution allow interning of the right operand's DC
too, making the comparison **total and symmetric** for that caller.
(`IsMetaThread()` inside the primitive was considered and rejected: a worker task can
incidentally execute on the meta thread, which would make the verdict
scheduling-dependent — the flag is a caller contract, not a runtime probe.) The
release-active `MG_CHECK` inside `GetOrCreateDataController` enforces the contract; a
debug assert at the flag's use site documents it. The def-time checker — always
meta-thread — passes the flag at its three unit-identity sites (`BindDomain`,
`LinkDomain`, `UnifyData`'s concrete-concrete case, via `TypeUnifier::s_CheckerUM`),
replacing the interim two-direction `DomainsUnify` retry helper with plain
single-direction calls. Verified by the full `scratch/fn_test*` battery (70/70), tst
Operator `/Rescale` `/Arithmetics`, and `examples/function.dms`. This must be in place
before the UnitNode pool widens the comparison surface to values-side identity; the
values-identity block (above) should use the same flag. See risk S11.

## 9. `CreateResult` stays authoritative; drift defenses

`CreateResult`/`CreateResultCaller` remain unchanged and authoritative at
application/calc time. Every `UnifyDomain`/`UnifyValues`/`UnitCreatorPtr` call keeps
running exactly as today. The described signature is a parallel declarative layer,
consumed at definition time and by printers.

The inherent risk is **drift**: a description claiming a relation the operator does not
enforce, or missing one it does. Defenses, in order of strength:

1. **Debug cross-check (`SigUnitChecker`).** Under `MG_DEBUG`, after a successful
   `CreateResultCaller` in `FuncDC` — where operator, args, and result are all in hand —
   replay the member's record against the *actual* units: assign each `sig_var` the actual
   units of the positions referencing it; check identity within each var's equivalence
   class via `UnifyDomain(um=0)`; check classes directly; check `CompatibleValues` via
   `UnifyValues`; recompute declared metric products/quotients and compare `GetCurrMetric`
   (log, do not assert — metrics are the deferred part); skip generative/deferred. Drift
   surfaces on the first debug run of any test touching the family.
2. **Merge-time structural audit.** When folding members (§5.5), cross-check each member's
   described positions against its registered `m_ArgClasses`/`GetResultClass()` (arity,
   per-position values type and composition). Catches gross drift at first use, in debug,
   without executing anything.
3. **Per-family test scripts** (pattern: `scratch/fn_test_opsig*.dms`) asserting negatives
   fire and generic positives still check.
4. **Soundness bias.** Because deferral is always available and merged sets come from real
   members, the failure mode of forgotten maintenance is *silence* (a missed diagnostic),
   not a wrong rejection — except for a wrong hand-written *structure*, which is what (1)
   and (3) target.

## 10. File and module layout

| Artifact | Location | Notes |
|---|---|---|
| `AbstrSignatureBuilder`, `sig_var`, `SignatureRecord`, `OperGroupSignatures`, recorder, merge, printer, `ValueClassSet` hoist | **new** `rtc/dll/src/tic/OperSignature.h` + `.cpp` | TIC_CALL; visible to clc/geo (they already include tic headers) and to `AbstrCalculator.cpp`; dependency direction (clc/geo → tic) preserved |
| `Operator::DescribeSignature` | `rtc/dll/src/tic/Operator.h` (+ default impl in a tic .cpp) | forward-declare the builder; no include cycle; new vtable slot ⇒ all-DLL rebuild (the standard recipe anyway) |
| `AbstrOperGroup::GetSignatures` + cache + `Register` generation counter | `rtc/dll/src/tic/OperGroups.h/.cpp` | |
| `unit_creator_spec` + `uc_*` factories | `rtc/dll/src/tic/UnitCreators.h` | |
| Record applier, candidate loop, trial harness, UnitNode surgery | `rtc/dll/src/tic/AbstrCalculator.cpp` (anon namespace — unchanged home) | **`TypeUnifier` is NOT extracted in v1**: the shared boundary is the pure-data `SignatureRecord`. Extraction to `tic/TypeUnifier.h` is a v2 refactor, justified only when try-unify selection or application-time `FindOper` needs the unifier from another TU |
| Family overrides | `clc/dll/include/OperAttrUni.h`, `OperAttrBin.h`, `OperAttrTer.h`, `OperAccUni.h`, `clc/dll/src/lookupImpl.h`, `RLookupImpl.h`, geo files | no new exports from clc/geo |

## 11. Worked examples

**`AbstrBinaryAttrOper` (K1, K9, K10) — written once for `mul`/`add`/`sub`/… members:**

```cpp
bool AbstrBinaryAttrOper::DescribeSignature(AbstrSignatureBuilder& sb) const override
{
    sig_var D  = sb.UnitVar("D");
    sig_var V1 = sb.UnitVar("V1"), V2 = sb.UnitVar("V2");
    sb.MemberValueClass(V1, GetArgClass(0)->GetValuesType());   // member-concrete
    sb.MemberValueClass(V2, GetArgClass(1)->GetValuesType());
    sb.ArgAttr(0, V1, D, m_ValueComposition);                   // void broadcast default-on
    sb.ArgAttr(1, V2, D, m_ValueComposition);
    sig_var R = DescribeUnitCreator(sb, m_UnitCreatorSpec, {V1, V2});
      // helper: Mul2 -> { r = UnitVar("V1*V2"); MetricProduct(r,V1,V2) }
      //         CompatibleValues -> { r = UnitVar("V"); CompatibleValues(r,V1); CompatibleValues(r,V2) }
      //         Boolean -> DefaultUnit(bool) (+ CompatibleValues(V1,V2) for CompareBool)
      //         Opaque -> no_sig_var
    if (R == no_sig_var) R = sb.UnitVar("R");
    sb.MemberValueClass(R, GetResultClass()->GetValuesType());
    sb.ResultAttr(R, D, m_ValueComposition);
    return true;
}
```

Mirrors `OperAttrBin.h:52-70` (`UnifyDomain` on the non-void domains; `m_UnitCreatorPtr`
for the values unit). Merged and printed for `mul`:
`mul(a: attribute<V>(D); b: attribute<V>(D)) -> attribute<V*V>(D) where V in {uint8…float64}`.

**`AbstrLookupOperator` (K2, K3, K8)** — mirrors `lookupImpl.h:74,79`:

```cpp
sig_var D = sb.UnitVar("D"), E2 = sb.UnitVar("E2"), V = sb.UnitVar("V");
sb.MemberValueClass(E2, GetArgClass(0)->GetValuesType());
sb.MemberValueClass(V,  GetArgClass(1)->GetValuesType());
sb.ArgName(0, "org_rel"); sb.ArgAttr(0, /*values*/ E2, /*domain*/ D,  ValueComposition::Single);
sb.ArgName(1, "values");  sb.ArgAttr(1, /*values*/ V,  /*domain*/ E2, m_VC);  // K2: E2 in BOTH roles
sb.ResultAttr(V, D, m_VC);                                                    // K3 + K8
```

`lookup(org_rel: attribute<E2>(D); values: attribute<V>(E2)) -> attribute<V>(D)`.

**`AbstrOperAccPartUni` (K1, K5, K9)** — mirrors `OperAccUni.h:150-157` (`e2->UnifyDomain(e1)`;
result domain `p2 = arg2.values`):
`sum(values: attribute<V>(D); partitioning: attribute<P>(D)) -> attribute<R>(P)` — `P`
appears in arg 2's *values* role and the result's *domain* role (needs §8); `R` from the
`unit_creator_spec`.

**`AbstrIndexedSearchOperator` = rlookup (K4, K7)** — mirrors `RLookupImpl.h:61-62`:
`rlookup(a: attribute<V>(D); b: attribute<V'>(E)) -> attribute<E>(D)` with
`CompatibleValues(V, V')`; the result's *values* is arg 2's domain var (needs §8).

**`unique` (K6)** — mirrors `Unique.cpp:345-352`:
`unique(values: attribute<V>(D)) -> unit<crd(D)> U [new] { Values: attribute<V>(U) }`;
as shipped (batch D) records `GeneratedUnit("U")` and — refining the earlier "ResultDeferred"
sketch — `ResultUnit(U)`, so the application denotes a *typed unit* (the expression `unique(a)`
IS the fresh unit, which the walker can bind to a declared `unit<…>`): the walker's `posType`
Unit branch gives it a fresh **flexible** node with no extra applier code. The `Values` sub-item
(`attribute<V>(U)`) is a container member, accessed as `unique(a)/Values` — a subitem reference
the walker already declines inside bodies — so it stays deferred (K11). `U`'s value class is
`crd(D)`: bound only for the typed `unique_uintN` groups (`m_ResDomainClass` fixed),
member-unconstrained for the dynamic-result-class `unique`.

**`discrete_alloc` (partial)** — mirrors `DiscrAlloc.cpp:1514` ff.: `typeNames:
ArgMetaValue(string, "names the allocation types")`, `allocUnit: ArgUnit(A)`,
`atomicRegionMap: ArgAttr(6, AR, A)` (K1 link to `A`, `:1766`), `ggTypes2partitionings:
ArgAttr(3, PartSet, T)` with `partitioningNames: ArgAttr(4, _, PartSet)` (K2, `:1609`);
`suitabilities: ArgContainer(2, "per-type: attribute<S>(allocUnit), all sharing one price
unit", A)` (`:1729,1742`); claims: `ArgContainer` + `DeferredRelation("claims[t].domain ==
the partitioning named by ggTypes2partitionings[t]")` (K12, `:1660,1667`). Net def-time
value: the expressible cell-domain consistency; everything else prints.

**`impedance_matrix` (K13 floor)** — mirrors `Dijkstra.cpp:1324-1331`: describe the four
registered prefix classes, `ArgMetaValue(0, string, "directs the remaining arguments")`,
`DynamicShape("the argument layout is computed from the spec string")`; result defers. This
batch exists to prove the vocabulary's floor: an operator that can only say "arg 1 is the
spec; everything else is per-application" must still print sensibly and constrain nothing.

## 12. Migration batches

Global gate for **every** batch: the full `scratch/fn_test*` battery (~71 configs;
positives green, negatives failing *for the same reason*) plus the `tst` suite. Hard rule:
**a described operator must never error where the old defer succeeded**, unless the config
is genuinely ill-typed — and every new error class must come with a negative test *and* a
demonstration that the same config already fails at reduction on `main`.

| Batch | Content | New vocabulary | Risk / benefit |
|---|---|---|---|
| **0** | infra: `OperSignature.h/.cpp`, vtable slot, recorder, merge + fingerprint + group cache, printer, record applier, candidate loop, `unit_creator_spec`, LispPtr memoization | all but identities/fresh | low / enabling. Zero behavior change (no group described ⇒ `GetSignatures()==nullptr`) |
| **A** | arithmetic/compare/logical attr families (`AbstrUnaryAttrOperator`, `AbstrBinaryAttrOper`, `AbstrTernaryAttrOper`, `CastedUnaryAttrOper`) — **retires the `OperSigKind` registry** | merged sets, Compare result override, void broadcast | low-med / **high**. Gate: `fn_test_opsig{,_neg1,_neg2}` verdicts ⊇ old; verify `sqrt`'s derived set == floats and `add`'s ⊇ {numerics, string}. **Sentinel test: `cog_mul` is mixed (unit + attr + geo polygon members) — it must demote to Tier 2 and defer with undescribed members** |
| **U** | the UnitNode tranche (§8), landed **dark** | — | med / enabling. `fn_test_unitnode.dms` exercising unit-parameters in values positions through *function* signatures (no operators needed) |
| **B** | relational: `lookup`, `rlookup`, `index`, `invert`, `collect_by_cond` | `valuesIdentity`, cross-role units, result values identity | med / **very high** — the headline payoff: join-key errors caught at definition. Specific risk: **borrowing** (lookup runs `UM_AllowDefaultLeft`), so ship an explicit regression config with a default-metric values unit against a named domain |
| **C** | aggregations (`AbstrOperAccTotUni`, `AbstrOperAccPartUni` + Num/Str variants) | K5, K15, accumulator sets | med / high. Negative test: declaring the result over the *data* domain instead of the partition set — a classic user error, now caught at definition |

### 12.1 Batches 0 + A as shipped (2026-07-19) — deviations and refinements

Implemented: `rtc/dll/src/tic/OperSignature.h/.cpp` (builder interface, `SignatureRecord`,
`SignatureRecorder`, shape-equality merge, `OperGroupSignatures` cache on
`AbstrOperGroup::GetSignatures()` with `Register` invalidation, printer
`RenderMergedSignature`), the `Operator::DescribeSignature` vtable slot (default
`return false`), family descriptions on `AbstrUnaryAttrOperator`, `AbstrBinaryAttrOper`,
`AbstrTernaryAttrOper`, `AbstrCastedUnaryAttrOperator` (convert/value) **plus**
`ArgMinMaxOper` (the `min_elem`/`max_elem`/`argmin*` allow-extra-args family, via
`RepeatArgs` — added to batch A to keep the retired registry's `min_elem` coverage), and
the walker's `InferOperatorApplication`/`ApplyOperRecord` replacing
`FindOperatorSignature`/`InferOperator`. Decisions that refine the design above:

1. **Member class TUPLES supersede the §5.5 union-only merge.** A merged record keeps
   each congruent member's per-variable class vector. Cross-position co-variance is then
   *derived*: variable pairs on which **all** tuples agree are **linked hard** (exactly
   the old shared-node semantics — `mul(x:V, y:W)` with independent rigids still errors
   rigid-rigid), and variables all tuples pin to one class are **bound** — but never onto
   a rigid ∀-variable. After argument unification the tuple set is narrowed by the bound
   classes (empty ⇒ definition-time error: reduction's `FindOper` is bound to fail) and
   agreement is re-derived on the remainder. This removes the need for any per-kind
   emission at the class level: the family overrides publish only structure (shared
   domain, positions) plus `MemberValueClass` per position — nothing hand-written.
2. **Derived support sets are SOFT (new rule, S1-driven).** A `ConstraintRec` gained a
   `soft` flag: support sets error when a **concrete** class binds outside them, but they
   neither narrow a rigid variable's `feasible` set nor fire the rigid ∀-subset check.
   Driving counter-example: the prelude's `<T: any>` null-aware predicates apply `eq`
   (registered over `fields`) and `lt` (over `scalars`) to variables whose constraint
   'any' spans **all** value classes — a hard derived set would reject the prelude
   itself. The old batch-1 registry avoided this only by *under-claiming* (Compare
   carried no constraint). Consequence: the batch-1 `sqrt: floats` ∀-error is retired —
   it was also factually **wrong**: sqrt is registered over `num_objects`, not floats
   (the hand-written table over-claimed; the derived set cannot). `fn_test_opsig_neg1`
   is rewritten to a concrete violation (`sin` on a string attribute).
3. **Candidate selection = registered-class elimination, described or not.** Witnesses
   per argument: a concrete class, a node's binding, or the node's (hard) feasible set —
   each an over-approximation of what any successful reduction can present, so
   elimination is sound. §18.4's `DefType::vcomp` (`ValueComposition`) shipped with this
   batch: a Single-composition argument eliminates sequence-registered members (iif over
   `value_elements` would otherwise always stay mixed). The **no-overload error** fires
   only when the elimination rested exclusively on concrete witnesses; feasible-driven
   elimination defers (the soft-support principle again). Multi-record survivors,
   undescribed survivors, arity mismatches, and non-caching groups all defer.
4. **`unit_creator_spec` did NOT ship** — deliberately deferred to batch U. At the class
   level the registrations + tuples carry everything batch A needs, and unit-level
   claims (Mul2 vs CompatibleValues vs Default) only become consumable when the UnitNode
   pool exists. The family overrides therefore use distinct per-position variables and
   never claim unit identity (mul's result var is not its argument var).
5. **No trial harness in v1** (§6.2's copy-trial-adopt): ambiguity defers instead of
   speculating. **No LispPtr memoization yet**: it exists for K6 generative nodes, and
   no shipped description emits `GeneratedUnit` — lands with batch D.

Net coverage: every group with members from the four families + `ArgMinMaxOper` is now
described — arithmetic, compare, logical, trig/float functions, rounding, predicates
(IsNull/IsDefined/isZero/...), string ops, `iif`, `mod`, `pow`, `dist`, bit-ops,
`convert`/`value`, `min_elem`/`argmin` families — and typed at definition wherever the
surviving membership is unambiguous, with `and`/`or`/`not` no longer falsely pinned to
bool (they are registered over all-ints) and div/iif checked for the first time.

### 12.2 Batch B as shipped (2026-07-19) — the relational family

Implemented: `DescribeSignature` on `AbstrLookupOperator` (lookup + collect_by_org_rel:
the K2 join-key — one variable `E2` in org_rel's VALUES role and values' DOMAIN role;
result `V[D]` with the class borrow through shared `V`), `AbstrIndexedSearchOperator`
(rlookup + kin: K4 — the result's VALUES unit IS arg2's domain, one variable `E` in both
roles; `E` carries no member class, its class flows through the unit node's companion),
`AbstrInvertOperator` (double cross-role: `B[A] → A[B]`), and `AbstrIndexOperator`
(conservative: the result-values claim uses a SEPARATE variable because index's result is
not flagged categorical — see the S1 rule below). `AbstrDirectIndexOperator` stays
undescribed (its values class is chosen conditionally at runtime). `collect_by_cond`
defers to batch D (K6 fresh subset domain).

**The identity rule.** The walker claims values-unit identity (`DefType::vuNode` = the
same unit node the domain role uses) for a record variable used in BOTH a values role
and a domain role — and only for those. Values-only variables stay class-level, because
their runtime discharge is `UnifyValues` (class + metric) or a plain borrow, where a
key-identity claim would over-reject (the batch-U S1 rule). Reduction-honesty per claim:
lookup's `E2` is UnifyDomain-enforced in `CreateResult` (with `UM_AllowDefaultLeft`
borrowing — harmless here because concrete DEFAULT units are not denotable in walker
terms: `vUnit` only ever comes from named scope units, so borrowing configs defer; the
doc-mandated regression config is `fn_test_opsigB`'s `pick`); rlookup's and invert's
result claims discharge against declared caller items via `CheckResultItem`'s
**categorical** UnifyDomain branch (both flag `TSF_Categorical`); index does not flag it,
hence its separate result variable.

**Adversarial-review correction (pre-landing, 3 independent confirmations): the
field-class vocabulary.** Sequence/polygon members register COMPOSED classes
(`float32seq`, `dpolygon`), but walker terms carry the FIELD class with the composition
separate (§18.2). Member elimination already bridges (witness synthesis via
`GetValueType(comp)`), so the sequence record was correctly SELECTED — and then the raw
composed classes in its tuples falsely rejected every concrete sequence/polygon argument
at the class bind and the tuple narrowing (an S1 violation on routine geometry lookups,
also latently present in batch A's sequence-registered `iif` members). Fixed centrally:
`SignatureRecorder::MemberValueClass` normalizes composed classes to their field class
(`ValueClass::GetFieldClass`, accessor added); the position's `ValueComposition` keeps
the composed-ness, so sequence records remain shape-distinct. Regression: the meta-only
`seqOf` case in `fn_test_opsigB.dms`.

Tests: `fn_test_opsigB{,_neg1,_neg2}.dms` — neg1 is the headline K2 catch
("inconsistent instantiation of unit variable 'E2': the unit bound argument 1 of
operator 'lookup' differs from the unit bound argument 2"), neg2 the K4 result-identity
catch against the declared result. (Debug note: neg2 exits 3 in Debug via the
pre-existing reduce-side-error teardown artifact — TreeItem leak + `NumbObjCache`
assert — identical to `fn_test_unify_neg2` since the bare-id fix; Release shows the
clean intended message.)

### 12.3 Batch C as shipped (2026-07-19) — the aggregations

Implemented: `DescribeSignature` on `AbstrOperAccTotUni` (total: one data argument,
VOID-domain result — K15; the result class is the member's accumulator class, which may
widen, carried by the tuples), `AbstrOperAccPartUni` (partitioned: both arguments share
one domain — K1, mirrored from `e2->UnifyDomain(e1)` — and the result ranges over the
PARTITIONING argument's VALUES unit — K5: one variable `P` in arg2's values role and the
result's domain role), `AbstrOperAccTotBin` (**deliberately separate domain variables**:
its `CreateResult` does not unify the argument domains, so no shared-domain claim would
be honest), `AbstrOperAccPartBin` (shared `D` + `P` as PartUni), and the
`PartCountOperator` template (pcount: `P[D] → R[P]`, count class fixed for the typed
`pcount_uintN` groups and unconstrained for the dynamic form). Covers
sum/mean/min/max/sd/var/count/first/last/modus/… and the typed `sum_*`/`pcount_*`
variants through the same bases.

**The classic catch is live** (`fn_test_opsigC_neg1`): declaring a partitioned sum's
result over the DATA domain instead of the partition set errors at the definition's
first reference — *"inconsistent instantiation of unit variable 'P': the unit bound
argument 2 of operator 'sum' differs from the unit bound the declared type of
'result'"* — exactly the §12 batch-table promise. Reduction-honest: the declared result
DOMAIN discharges via `CheckResultItem`'s domain `UnifyDomain`.

**Wildcard argument classes (two rounds).** The partitioned members register their
partitioning argument with the WILDCARD `AbstrDataItem` class (any partition class
serves) — a describe that `dynamic_cast`s and bails made every partitioned member
silently undescribed (caught by the batch's own negative test failing on the wrong
side). The adversarial review then found the same pattern once more: `modus_weighted`
registers its WEIGHT vector as the wildcard too. Rule now applied uniformly: a wildcard
argument class leaves its variable **member-unconstrained** (no `MemberValueClass`,
composition Single) instead of suppressing the description — matching §18.3's wildcard
observation. Only the RESULT class remains a hard requirement for describing.

### 12.4 Batch D as shipped (2026-07-19) — the fresh-unit family

Implemented: `DescribeSignature` on `AbstrUniqueOperator` (`Unique.cpp`), `SubsetOperator`
(`Subset.cpp`: `select`/`select_with_org_rel`/the obsolete `subset`),
`AbstrCollectByCondOperator` (`Subset.cpp`: `collect_by_cond`, deferred from batch B),
`AbstrUnionOperator` and `UnionUnitOperator` (`Union.cpp`) — plus the **LispPtr application-result
memoization** in the walker (`FunctionChecker::m_ApplTypes`, `AbstrCalculator.cpp`).

1. **K6 needed NO new applier code.** A `GeneratedUnit("U")` var appears only in a `ResultUnit(U)`
   result; the walker's existing `posType` Unit branch already produces a `DefType::Kind::UnitVal`
   whose identity is `DN(U)` — a fresh node created by `UnitVar(nullptr, inst, …)`, i.e.
   **flexible** (never rigid: signature nodes are always non-rigid, §6.1). Since a generated var
   is referenced by no argument position, that node is genuinely fresh, and the pre-unification
   `LinkValue` loop and post-narrowing propagation only touch *value* (class) nodes, so the unit
   identity stays isolated. The doc's §6.1 K6 story therefore realizes as "give the result var a
   `GeneratedUnit` and let the existing Unit-result path mint the fresh flexible node" — no
   `generative`/`genOrigin` fields were needed for this batch (§8 deferred them; they remain the
   optional later tightening, open question §15.3).

2. **LispPtr memoization** (`m_ApplTypes`, keyed by `(refScope, expr.get())`). LispRefs are
   interned, so two textually identical applications share one `LispObj`; the key makes both
   occurrences denote **one** result node. This is the K6 soundness prerequisite (two `unique(a)`
   reduce to one DataController, so their fresh units must be one node), and it de-duplicates
   diagnostics for every repeated subexpression. It is sound in both directions: an error throws
   (never cached); a cache hit skips re-walking the *identical* arguments, whose own checks already
   ran on the first occurrence; and sharing one result node across two occurrences only *adds*
   constraints that reduction (which passes the single DC's result to both contexts) must satisfy
   anyway — so it can catch a real error earlier but never invent a false one (S1). `FunctionChecker`
   is one-per-`CheckFunctionDefinition`, so the cached node indices never outlive their unifier.

3. **The headline catch is `collect_by_cond`** (`fn_test_opsigD_neg1`): its `CreateResult` runs
   `condA->GetAbstrDomainUnit()->UnifyDomain(dataA->GetAbstrDomainUnit(), UM_Throw)` (K1), so the
   description threads the condition and data domains through ONE variable `D`. Declaring the
   condition over `D1` and the data over `D2` — independent domain generics — errors at the
   definition's first reference: *"the body requires unit variables 'D2' and 'D1' to be equal
   (operator 'collect_by_cond'), but they are independent in the definition"*. The result ranges
   over the passed subset unit `S` (arg0's identity) and borrows the data's value class `V`
   (values-only ⇒ class-level, per the batch-B identity rule — honest, since the cache result's
   values unit is later checked by `UnifyValues`).

4. **`union` claims only its fresh result unit — the cross-argument class check is DEFERRED**
   (adversarial-review correction, 2026-07-19). Every argument must share one value class
   *end-to-end* (`const_array_cast<V>` in `UnionCopy` at **calc** time), and the first draft
   threaded all arguments through one values variable `V` (arg0 + a `RepeatArgs` tail) to catch a
   mismatch at definition. The review confirmed this is an **S1 false-error under the metainfo
   honesty bar** (the batch-U default-unit mirror): `AbstrUnionOperator::CreateResult`'s only
   metainfo cross-arg check —
   `currArg_ValuesUnit->UnifyValues(arg1_ValuesUnit, UM_Throw | UM_AllowDefault)` — is **skipped
   entirely** when the running reference unit is a DEFAULT unit (`if
   (arg1_ValuesUnit->IsDefaultUnit()) arg1_ValuesUnit = currArg_ValuesUnit;` adopts the next arg's
   unit of any class without a check), and a bare `attribute<V>` argument carries the default
   values unit — so `union(default-float32, float64)`'s metainfo succeeds while the hard `V`-share
   would reject the definition. (One verifier held the config is "genuinely ill-typed" since no
   *computable* mismatched-class union exists — the cast crashes at calc — but a metadata-only use
   succeeds at metainfo, so the conservative, batch-U-consistent choice is to defer.) As shipped,
   `union` shares no values variable across arguments: it records arg0's own class (printer +
   member selection) and the fresh **`Unit<UInt32>`** result (`class uint32`, produced
   unconditionally). The result-class claim IS honest — `unit<uint16> u := union(…)` still errors,
   since union's metainfo result is `Unit<UInt32>` and the declared `uint16` domain conflicts.

5. **Result-class binding is guarded** exactly as batch C: `MemberValueClass(U, …)` is emitted only
   when the group fixes a concrete result unit class — `dynamic_cast<const UnitClass*>(GetResultClass())`
   / `m_ResDomainClass` non-null (the typed `select_uintN`/`unique_uintN`/`union_unit_uintN`
   groups) — and left member-unconstrained for the dynamic-result-class `select`/`unique`. `select`'s
   Bool condition and `union_unit`'s unit arguments are described; the wildcard `AbstrUnit` positions
   (`collect_by_cond` arg0, `union_unit` arg0) use `ArgUnit` and take no class constraint (the batch-C
   wildcard rule).

Tests: `fn_test_opsigD{,_neg1,_neg2}.dms` — `neg1` is the `collect_by_cond` K1 domain-mismatch
catch, `neg2` a non-Bool `select` condition (the described `SubsetOperator` member is eliminated at
definition; honest at metainfo too — `FindOper(cog_select, [uint32])` finds no member and throws).
Validation: 92/92 battery, tst Operator `/Rescale` + `/Arithmetics` (prelude functions exercise the
walker + memoization), `examples/function.dms`, both flavors rebuilt, Debug sweep clean (the
negatives error cleanly at definition — exit 1, not the reduce-side teardown artifact). The tst
Operator config has **no** `function` items, so its reduction (incl. the pre-existing `/Relational`
`union_data`/`combine_data` failures) is provably untouched — the describes are consumed only
during function-body checking.

**Adversarial review (2026-07-19, workflow `wf_17dbf3b1`, 3 dimensions + verify).** One S1 finding
CONFIRMED and fixed pre-landing: union's cross-argument class-equality (point 4 above). Two findings
REFUTED on verification: (a) a claim that the LispPtr memo keys dangle — refuted because
`RewriteExpr`→`ApplyTopEnv` unconditionally interns every walked tree into the process-global,
never-evicted `g_applyTopEnvCache` (strong `LispRef`s), which transitively pins the whole subtree
for the run, so the `const LispObj*` keys stay live and stable (and cross-body-item identical
subexpressions correctly share one node — the memo is *positively* sound for K6); (b) a claim that
the memo hides errors — refuted because a memo hit can only *suppress* checking (errors throw and
are never cached), which moves the checker toward fewer errors, the opposite of an S1 violation, and
a suppressed diagnostic still fails at reduction (a sound deferral).

| **D** ✅ | fresh-unit family: `unique`, `select`/`subset`, `union` + `collect_by_cond` (deferred from B) | K6 generative nodes + LispPtr memoization | SHIPPED 2026-07-19 — §12.4 |
| **E** ✅ | `discrete_alloc` (opaque) + `connect` family + **the printer wired into `FindOper`'s failure** | `ArgContainer`, `ArgMetaValue`, `DeferredRelation`, `DynamicShape`, `ResultDeferred` | SHIPPED 2026-07-20 — §12.5 |
| **F** ✅ | `impedance_matrix` / dijkstra (the K13 floor) + `connect_info`/`dist_info` + the durable `XML_ReportOperGroup` signature surface + printer completion | `DynamicShape`, `ArgMetaValue`, `DefaultUnit` result | SHIPPED 2026-07-20 — §12.6. **The batch sequence is COMPLETE.** |

### 12.6 Batch F as shipped (2026-07-20) — the K13 floor, the last family, and the doc surface

The final batch, docs-only by the §16 ruling. Four deliverables:

1. **The impedance/dijkstra family is the §11 floor, verbatim** —
   `DijkstraMatrOperator<T>::DescribeSignature` (all 6 groups: `impedance_table`/`impedance_matrix`/
   `impedance_matrix_od64` + the 3 obsolete `dijkstra_*`; T ∈ Float64/Float32/UInt32/UInt64). It
   **constrains nothing**: `ArgMetaValue` (the spec string, K13), `ArgDeferred` for the three prefix
   attributes (their Links/Nodes relations fire in `CreateResult`'s preamble but stay prose —
   `DynamicShape` forces the whole application to defer anyway, so a hard claim would buy no
   checking), `DeferredRelation` + `DynamicShape` + `ResultDeferred`. The rendered record is exactly
   the §11 promise: *`impedance_matrix(String [meta: the specification: directs the remaining
   arguments]; … ) -> … [a new OD-pairs unit …] [shape: the argument layout is computed from the
   specification string (K13)] [deferred: linkImpedance, fromNode_rel and toNode_rel share one Links
   domain; …]`* — an operator that can only say "arg 1 is the spec" prints sensibly and constrains
   nothing.

2. **`connect_info`/`dist_info`** (`ConnectInfoOperator`, arities 2–6, `_eq`/`_ne`, ±maxdist/mindist)
   — deferred from batch E, mirroring the shipped FastConnect shape: arcs/keys/distances as deferred
   prose, points with fresh single-use vars (no cross-argument claim). The one faithful upgrade is
   **`dist_info`'s result**: `CreateResult` unconditionally builds
   `CreateCacheDataItem(pointEntity, default-dist-unit)`, so `ResultAttr(DefaultUnit(SqrtDistType),
   Dp)` states the **K3 domain identity** (result domain == points domain) and the metric-less float
   class — both discharged by `CheckResultItem` at reduction. The K3 catch is live
   (`fn_test_opsigF_neg2`): declaring the result over the ARC domain errors at the definition's
   first reference. `connect_info`'s container result is `ResultContainer` prose (walker: Unknown).

3. **The durable doc surface**: `XML_ReportOperGroup` (`ReportFunctions.cpp`) emits a `<Signature>`
   element per merged record — the same printer, `IsMetaThread()`-guarded like the FindOper site;
   `<`/`>` in rendered signatures are XML-escaped by `OutStream_XmlBase::WriteValue`. The review
   **confirmed a pre-existing out-of-bounds bug** on this very surface, fixed in this batch:
   `XML_ReportAllOperGroups` used `GetNrOperators()` (the total MEMBER count) as the loop bound for
   `GetOperatorGroup(i)` (a GROUP index), overrunning the group registry — debug assert / release
   out-of-bounds read on every whole-report generation. The bound is now
   `GetNrOperatorGroups()`.

4. **Printer completion** (review-driven): `RenderMergedSignature` never rendered `DeferredRelation`
   notes or `ResultDeferred` prose — the very content batches E/F exist to publish. It now renders
   `-> ... [<result note>]` for a deferred result and a trailing `[deferred: <note>; <note>]` block.
   Consequence: persistent note strings are USER-VISIBLE — they must never embed source line
   numbers (the review confirmed the first draft's `:698`-style citations were invalidated by the
   very insertion that added them; all notes are now symbolic).

Tests: `fn_test_opsigF.dms` (a valid `dist_info` in a function body — accepted and reduced, the K3
claim live), `fn_test_opsigF_neg1` (the enriched `impedance_matrix` failure renders the full floor
record incl. `[shape:]` and `[deferred:]`), `fn_test_opsigF_neg2` (the K3 wrong-result-domain
def-time catch). Validation: 97/97 battery, tst `/Arithmetics` + `/Rescale` + `/MetaInfo`,
`examples/function.dms`, both flavors, Debug sweep clean; `/Network` fails only on its pre-existing
`pow`-metric items (zero connect/dijkstra/describe errors). Adversarial review (workflow
`wf_3d462fe1`, 3 dimensions + verify): 2 CONFIRMED findings — the stale note citations and the
pre-existing `XML_ReportAllOperGroups` overrun — both fixed pre-landing; the refuted-as-non-defect
observations (printer gap, missing guard) were addressed as quality items anyway.

### 12.5 Batch E as shipped (2026-07-20) — the composites + the printer's first consumer

Batch E is diagnostics/docs-only by the §16 ruling: the composite operators are OPAQUE at
definition (result ⊤). Its two deliverables:

1. **The printer is wired into `FindOper`'s failure message** (`OperGroups.cpp`, §6.3) — the first
   consumer of `RenderMergedSignature`, and the batch's observable payoff. When `FindOper` throws
   *"Cannot find operator for these arguments"* (a reduction-time, meta-thread-only event — an
   operator is resolved from arg CLASSES once and cached in `FuncDC`, never re-looked-up on worker
   threads), the message now appends the group's **declared unit-constraint signatures**
   (`GetSignatures()->records` rendered by the printer), for EVERY described group (batches A–E).
   Undescribed groups yield `sigs==nullptr` ⇒ empty ⇒ byte-identical message (zero behaviour
   change). Guarded by `IsMetaThread()` — belt-and-suspenders documenting the meta-only contract;
   `GetSignatures()`'s lazy build is race-free because its only writer, `Register()`, completes at
   static-init. Verified live: `connect(float32, float32)` now prints
   `connect(point1: attribute<Vc>(D1); point2: attribute<Vc>(D2))` and the arc→network variants
   `… -> unit<connected_network>`.

2. **`discrete_alloc`** (`HitchcockTransportationOperator::DescribeSignature`, all three
   np/sp/multi families via `GetNrArguments()`): **fully opaque, S1-vacuous**. The entire
   obligation set is computed from the meta-read type-name array (K13), so the whole application
   defers (`DynamicShape`) and the result is ⊤ (`ResultDeferred`). It makes ZERO cross-argument
   claims: `ArgMetaValue` (typeNames), single-use `ArgUnit` (allocUnit / atomicRegionUnit — bind
   their argument, never link), `ArgContainer`/`ArgDeferred` (Unknown positions), `DeferredRelation`
   prose. Purely printer content.

3. **The `connect` family** (`AbstrConnectNeighbourPointOperator`, `AbstrConnectPointOperator`,
   `FastConnectOperator`) — described with only the **verified-safe** claims. Every shared-variable
   claim was checked against `CreateResult` to be a plain `UnifyValues`/`UnifyDomain(UM_Throw)` with
   **no default-unit escape** (the union S1 mirror) and no void escape: the neighbour domain share
   (`:133`), the point-point coordinate class share (`:300`, class-level — reduction also checks the
   metric, so def-time is strictly more permissive), the capacitated weight domain/value shares
   (`:303`/`:304`/`:306`). The mis-registered `eq`/`ne` `ConnectPointOperator` members (whose
   `CreateResult` asserts `size==2||4` and never implements the compare-key contract) are **guarded
   out** (`describable = (n==2 && !isCapacitated) || (n==4 && isCapacitated)`). `FastConnect`
   (arc→network) states its **fresh `Unit<UInt32>` result** as `ResultUnit(GeneratedUnit)` — the
   proven-safe batch-D K6 pattern — with the geometry, K16 coordinate share, join keys and
   void-broadcasting distances all recorded as `ArgDeferred`/`DeferredRelation` prose. `spatialIndex`
   is left to its batch-A `AbstrTernaryAttrOperator` describe; `connect_info`/`dist_info`
   (`ConnectInfoOperator`) are deferred to batch F.

Tests: `fn_test_opsigE.dms` (a valid `connect_neighbour` in a function body — the walker accepts it,
proving no false def-time rejection, and it reduces) and `fn_test_opsigE_neg1.dms` (the enrichment:
`connect(float32,…)` fails `FindOper` and the message lists the declared signatures). Validation:
94/94 battery, tst Operator `/Arithmetics` + `/Rescale`, `examples/function.dms`, both flavors
rebuilt, Debug sweep clean. tst `/Network`'s pre-existing `pow`-metric failure in
`dijkstra_all_interaction` is unrelated (no connect/`FindOper` error). **Adversarial review
(workflow `wf_9619da3d`, 3 dimensions): ZERO findings** — the conservative dial (verified
plain-`UM_Throw` claims, opaque-favouring vocabulary, no cross-arg claim over any
conditional/default-escaping path) held.

### 12.7 Definition-time K13 spec processing (ruled 2026-07-20) — **impedance AND for_each tranches SHIPPED 2026-07-20**

**As shipped** (`Operator::DescribeSpecSignature` + the `FunctionChecker` closed-spec machinery in
`AbstrCalculator.cpp` + `DijkstraMatrOperator<T>::DescribeSpecSignature` in `Dijkstra.cpp`):

- **The pipeline**: when a `DynamicShape` record's single string-valued `ArgMetaValue` position
  holds a spec CLOSED over the formals, the walker evaluates it (literal fast path off the parse
  tree; otherwise `TryBuildClosedKeyExpr` reduces the closed sub-expression to its hash-consed DC
  key — externals via `GetCheckedKeyExpr`, body locals recursively, operator nodes via
  `RewriteExprTop` — and `EvalClosedSpec` runs `GetOrCreateDataController` → `CalcCertainResult` →
  `GetTheValue<SharedStr>` under a `FencedInterestRetainContext`, the dynamic-argument-policies
  idiom). The surviving members' `DescribeSpecSignature` records (no `DynamicShape`; positions in
  the extraction order, count == `CalcNrArgs(df)`) merge and apply. **Storage-backed specs are
  read at definition scan** per the ruling — verified: a storage-backed spec's arity violation
  errors at definition with the file's content quoted. Every failure at every stage defers.
- **The two live catches**: the ruled honest ARITY error (*"number of given arguments to operator
  'impedance_table' doesn't match the specification '…': 6 arguments given (including the
  specification), but 5 expected"*) and the per-spec hard unit shares (the Links domain over
  imp/F1/F2 + the per-flag tail relations — *"the body requires unit variables 'L2' and 'L1' to
  be equal (operator 'impedance_table')"*). Non-OD results claim `ResultAttr(Imp, dstZones)`; OD
  results a `GeneratedUnit` with the `UInt64_Od`-aware class (and, since 2026-07-21, its typed
  OD member zoo — see §12.8).
- **Adversarial-review corrections (fixed pre-landing, workflow `wf_2db0d75a`)**: (a) a per-spec
  record memo keyed `(group, spec)` replayed one application's SURVIVOR tuples against another's
  argument classes (two impedance calls with different Imp classes in one body → false
  no-overload; the exact repro is now the `dist2` regression in `fn_test_opsigK13`) — the memo is
  DROPPED, records derive per application (the LispPtr memo dedups call sites); (b) a trailing
  `...rest` symbol counts as ONE syntactic term but splices at reduction — rest-having functions
  defer the whole spec path; (c) reduction resolves the closure ENVIRONMENT before the definition
  scope, so a def-scope item shadowed by an enclosing function's member must classify as a
  capture (open), not an evaluable external; (d) the application/closed-key memos now hold
  **strong `LispRef` keys** — the map entries pin the interned nodes, closing the raw-pointer
  ABA hazard two review rounds had disputed. Additionally `CheckFunctionDefinition` gained a
  re-entrancy sentinel (closed-spec evaluation can `UpdateMetaInfo` items whose expressions apply
  the function being checked).
- **v1 narrowings inside the granted scope**: function-call heads inside a spec expression defer
  (building their key would re-enter `ReduceValue`; lift with the sentinel + errorHolder later).
  ~~A cleanly-evaluated-but-invalid spec defers~~ — **UPGRADED 2026-07-20**: a throw from
  `DescribeSpecSignature`/`DescribeMetaSignature` on a cleanly evaluated closed spec now
  PROPAGATES as the definition-time error (the interface contract requires such throws to be
  the member's own spec validation — `ParseDijkstraString`/`CheckFlags`/`ScanFirstArg`, the very
  predicates `CreateResult` applies first, so the report is honest). Verified:
  `fn_test_opsigK13_neg3` ("parse dijkstra options Error: syntax error at …" at definition) and
  `fn_test_fe_neg7` ("argument specification, unexpected token(s): 'qq' in nqq" at definition).

Tests: `fn_test_opsigK13{,_stor,_neg1,_neg2}.dms` (positives: literal + closed-external +
storage-backed + open-defer + the memo regression; negatives: the arity and Links-domain catches).
Validation: 101/101 battery, tst `/Arithmetics` + `/Rescale` + `/MetaInfo` (+ `/Network`
unchanged: pre-existing `pow`-metric only), `examples/function.dms`, Release + Debug, Debug sweep
clean (the emission's `assert(i == CalcNrArgs(df))` holds). The `for_each` tranche is now also
shipped (below), as is the invalid-spec honest error (see the v1-narrowings note above).

#### The original ruling and plan

> **Ruling (Maarten, 2026-07-20): when a K13 meta-directing argument is available at function
> definition, process it — not only literals: every spec that does not depend on the function's
> arguments.**

Today a K13 application (`impedance_matrix` & co., `discrete_alloc`'s name arrays) defers wholly
(`DynamicShape` ⇒ ⊤), because the argument layout is a function of the spec's *value* and
definition time is symbolic (§16, §20.1 fragment 3). But that deferral is only *forced* when the
spec is unknowable at definition. The planned refinement:

- **Trigger** — the described member carries `DynamicShape` and the `ArgMetaValue` position's
  argument expression is **closed with respect to the function's formals**: it references no
  parameter, no `...rest` slice, no captured closure value, and no enclosing function's parameter
  — transitively. Literals qualify; so do definition-scope constants and parameters
  (`parameter<string> spec: [...]` at config scope) and pure expressions over them. Anything
  touching a formal ⇒ defer exactly as today.
- **Closedness is decided syntactically, without evaluating anything** — a memoized reachability
  predicate over the walker's existing reference classification (`ResolveName`: formal / body
  local / closure env / external). The scan terminates at externals *by construction*: the
  formals are lexically invisible outside the function, so a definition-scope item's own
  expression cannot reference them; transitivity is needed only through body locals and the
  closure environment. Equivalent characterization: an expression is closed **iff β-substitution
  of the arguments is the identity on it** — which is also exactly the condition under which its
  DataController key is one and the same across all applications, so a single definition-time
  evaluation is coherent. Note the value-question is strictly stronger than the meta-question
  (`parameter<string> s := formalSpec` has closed *meta* — void × string — but an open *value*);
  it is the value-question this WP tests.
- **Closed means EVALUATE — storage-backed specs included (ruled 2026-07-20).** A closed spec is
  read at definition-scan time whatever backs it: a literal comes straight from the parse tree;
  every other closed form — definition-scope constants, pure expressions over them, **and
  storage-backed items** (`parameter<string> spec: storagename = "x.txt"`) — is evaluated through
  the standard meta-time calculation, the same one `oper_arg_policy::calc_always` performs for
  that argument at reduction. *Scanning `f`'s definition IS the time to read `x.txt` when `x.txt`
  decides which sub-items `impedance_matrix` yields in `f`* — the I/O is the point, not a side
  effect to avoid. There is no closed-but-effectful defer tier. Two consequences: (a) the
  definition check acquires a **data dependency** on the closed spec's sources, with the same
  per-config-load consistency reduction already has — it is the *same hash-consed DC* the
  eventual reductions use, so the value is read once and the definition-time evaluation warms the
  very cache entry reduction needs (no double read); (b) a spec source changing between sessions
  changes the definition verdict exactly as it changes every reduction — no new invalidation
  machinery.
- **Mechanism** — evaluate the closed spec sub-expression at meta time (the same evaluation
  `oper_arg_policy::calc_always` already performs for that argument in `CreateResultCaller` — the
  argument is *ground*, so this is legitimate probing per §19.3's groundness law, unlike the
  fabricated-placeholder probing §19.2 forbids). For the impedance family: feed it to
  `ParseDijkstraString` → `CalcNrArgs` and re-derive a **concrete per-spec record**: the arity
  check (honest — `CreateResult` throws unconditionally on `args.size() != CalcNrArgs(df)`), plus
  the per-flag tail roles from the argument-extraction order (node-rels ranging over the one Node
  set — `UnifyDomain` in the preamble, K2-style; impedances sharing the Imp values unit —
  `UnifyValues`, so **class-level only** per the identity rule; zone-rels and their domains).
  Cache the derived record per `(member, spec value)`; the LispPtr application memo already
  de-duplicates per call site. For `discrete_alloc`, a closed typeNames array names the member
  obligations — but container members have no `DefType` kind (§7 K11 defers in v1), so the
  realistic v1 scope is the **impedance/dijkstra family**; `discrete_alloc` joins if/when K11
  container checking lands.
- **S1 guards** — never evaluate anything depending on a formal. An **evaluation failure**
  (including a transient storage failure reading a closed spec's source) ⇒ **defer**, never a
  definition-time error from the evaluation itself: the instantiation retries the same DC and
  reports the failure properly if it persists. A **cleanly evaluated but invalid** spec may be
  reported at definition (reduction hits the same `CheckFlags`/parse throw, so it is honest —
  defer remains acceptable). The arity verdict for a known spec is exempt from the §6.2 "arity
  always defers" rule *only* because `CalcNrArgs` is the very predicate reduction applies — the
  general rule stands for every other operator.
- **Why this is not a ruling change** — §16 excludes running the operator's `CreateResult` on
  *placeholders*; here the spec argument is concrete and the evaluation is the meta stage's own.
  K13 stays "genuinely staged" in general; this tranche merely notices when the staging boundary
  for one argument has, for a particular body, already been crossed at definition.

#### The for_each tranche — **SHIPPED 2026-07-20**

**As shipped** (`MetaMemberLayout` + `Operator::DescribeMetaSignature` in tic, `FillForEachLayout`
+ the two overrides in `clc/ForEach.cpp`, and the `FunctionChecker` container machinery in
`AbstrCalculator.cpp`):

- **K11 lands**: `DefType` gains `Kind::Container` with a shared path-keyed member map
  (`members` + `membersComplete`) — the pseudo-expanded member set of a container-GENERATING
  meta application. `InferOperatorApplication` branches to `TryMetaContainerProcessing`
  **inside** the (previously wholesale) `!MustCacheResult` deferral: the group's members
  describe their argument LAYOUT (`namesPos`, `domain/values/unit` positions with optional
  per-member name-array companions, `vcomp`, member kind Data/Unit/TemplateCopy/Untyped,
  `nrArgs`); a closed name array is EVALUATED at definition scan (`EvalClosedStrArray` — the
  `EvalClosedSpec` idiom over any-domain string arrays, **storage-backed sources included** per
  the ruling); undefined/empty rows skip exactly as `ForEach_CreateResult`; duplicates defer.
- **Member types**: Data members take their domain/values from the layout's unit positions — a
  *formal* unit parameter contributes its unifier node in both class and identity roles (the K2
  bridge, verified by the `fe_neg3`/`fe_neg4` pair: a member typed by `unit<float64> V` unifies
  with a declared `attribute<V>` result, and a `float32`-valued member against a declared
  `string` result errors at definition); a value-class name (`float64` default unit) pins the
  class only; a closed def-scope external unit contributes concrete identity; the
  `(container, name-array)` pair mode resolves units per member inside a closed external
  container. Anything unresolvable defers that MEMBER's type, never the member set.
- **Member references**: `ResolveName`'s slash-descend over body locals is now segment-wise;
  a miss below a generating item (meta-head rule, `RuleMayGenerateSubItems`; the nearest
  generating item on the walked path wins) returns code 3, and `InferGeneratedMember` types the
  remaining path against the container's member map: exact hit ⇒ the member's type; either-way
  path-prefix relations ⇒ defer (intermediate generated containers above members, and
  sub-structure BELOW template-copy/rule-bearing members — a review fix); a complete-set miss
  ⇒ an honest definition-time error listing the generated members (capped at 10; empty sets get
  their own wording). Sound because the inline reduction rejects every meta-rule member access
  with certainty ("meta function call is not supported inside function bodies" /
  `ResolveBodySymbol`'s `FindSubItem` throw); the copy-instantiating form is not checker-covered.
- **Arity**: `for_each_ind`'s spec-derived width IS `CreateResult`'s own
  `CalcNrArgs(fs)+1` predicate — its violation errors at definition with CreateResult's message
  shape (the ruled exemption, mirroring the impedance tranche); layout-static suffix groups
  defer arity mismatches (§6.2), and counts outside the group's accepted range defer before
  that (a same-named function may serve the call).
- **What defers wholly**: `...rest`-having functions, heterogeneous/undescribed groups, open
  or unevaluable names/specs, duplicate names — byte-identical to the pre-tranche deferral.
  `loop` and other meta groups don't describe (default `DescribeMetaSignature` = false) and
  keep deferring.
- **discrete_alloc scoping (2026-07-20 — the join is NOT free; needs a ruling)**: reading
  `CreateResultCaller`/`CreateResultingItems`: the result container holds FIXED members
  (`landuse` = `attribute<AT>(allocUnit)` whose VALUES unit is the typeNames array's DOMAIN;
  `status`/`statusFlag` parameters; `bid_price` = `attribute<S>(allocUnit)` with the
  suitabilities' shared price unit) plus NAME-DIRECTED members `shadow_prices/<typeName>` and
  `total_allocated/<typeName>` (partitioningUnit × priceUnit / land_unit_id). Joining needs:
  (1) an ARRAY-spec path on the CACHEABLE side (`TrySpecProcessing` is scalar-string-gated;
  the evaluation itself is ready — `EvalClosedStrArray`); (2) a typed result-CONTAINER
  vocabulary for spec-describes (SignatureRecord's `ResultContainer` is prose-only) or a
  second producer of the K11 `Container` DefType on the normal path; (3) most importantly, a
  REDUCTION-side decision: member references into cacheable operator results (`a/landuse` on
  a body local) throw at inline reduction today (`ResolveBodySymbol`'s `FindSubItem`), so
  checker-side result typing would be diagnostics-only — unless reduction learns to emit
  `slSubItemCall` sub-item keys for them (the cache machinery exists: `GetLispRefForTreeItem`
  uses exactly that form), which is a language-semantics extension to rule on first. The
  INPUT obligations (suitabilities/claims members per type name) are checkable without (3)
  but only bind when the container argument is a closed external — rarely the case in
  function bodies (containers are usually formals). **STATUS (2026-07-21): all three gaps
  closed for the flagship. (3) shipped as §12.8; (2)'s vocabulary is `ResultContainerMember`;
  and (1) shipped — `landuse = attribute<AT>(allocUnit)` (AT = typeNames' domain) is typed
  STRUCTURALLY (no array evaluation needed — the "join" is the type derivation, symbolic in
  formals), plus `bid_price`, incomplete set (§12.8). The name-directed
  `shadow_prices/<name>`/`total_allocated/<name>` members stay deferred: their types come from
  the partitionings/suitabilities, not the type-names, so a closed typeNames array does not
  determine them.**

Tests: `fn_test_fe_pos` (declared-sub-container slash paths + config-scope for_each unchanged)
and `fn_test_fe_{neg1..neg6,stor_neg}`: closed-set miss listing `a, b`; the ind spec-arity
error; the CLEAN scan with a K2-bridged member (fails only late); the member-type conflict at
definition; the open-names S1 defer; the below-member defer; and the storage proof — the
missing-member message quotes `alpha` read from `fe_names.txt` at definition scan. Validation:
109/109 battery (Release), tst Operator `/Arithmetics` + `/Rescale` + `/MetaInfo`,
`examples/function.dms`, Debug sweep with all `fe` tests assertion-free. (The Debug sweep now
distinguishes assertion exits and exposed a PRE-EXISTING `NumbObjCache.empty()` teardown leak
on two old unifier-error negatives — `fn_test_unify_neg2`, `fn_test_opsigB_neg2` — verified
present on a clean HEAD baseline build and filed separately; unrelated to this tranche.)
**Adversarial review
(workflow `wf_f404ef1a-219`, 4 dimensions): zero confirmed findings; the three reviewer
observations that survived manual adjudication (below-member paths mis-reported, garbled
empty-set message, deepest-item-only code-3 attribution) were all fixed pre-landing** (the
verify stage was partially cut short by session limits; adjudication was redone by hand).

#### The original for_each extension ruling

**The same ruling extends to the meta-scripting family (ruled 2026-07-20): `for_each_*`,
`for_each_ind`, `loop`, …** — the §19.2 value-reading operators. Their K13 profile differs from
the impedance family in an instructive way: the *argument layout* is already **static** (encoded
in the suffix-generated group name — `for_each_nedv`'s parallel arrays are fixed at
registration), so what a closed spec unlocks is not arity but the **generated member set and the
per-member types**: with the name array closed over the formals, the checker can pseudo-expand
the resulting container — one member per name — and with closed `d`/`v`/`u` arrays type each
member from the named domain/values units (unit-name strings resolving against the definition
scope, or against a *formal* unit parameter, in which case the member's type rides that formal's
unifier node — the K2 machinery, per instantiation). `for_each_ind`'s indirection strings
qualify under the same closedness test. Two extra gates beyond the impedance tranche, in
dependency order: (1) **container-shaped types in `DefType`** (the §7 K11 gap — the same gate as
`discrete_alloc`'s name arrays), since the payoff *is* a typed container; (2) a walker path for
**meta groups**: all `for_each_*` groups are `dont_cache_result`, which
`InferOperatorApplication` today defers wholesale before signatures are even consulted — the
closed-spec path must branch before that gate. The evaluation ruling applies identically
(ruled 2026-07-20): a `for_each` application whose meta-directing arguments do not depend on
`f`'s formals is processed at definition scan — **including storage-backed name/expr/unit-name
arrays**, read through the same meta-time calculation reduction uses; there is no
closed-but-effectful defer tier here either. S1 guards identical: formal-dependence, evaluation
failure (defer — the instantiation retries), or an unresolvable unit name ⇒ defer as today.
Sequencing recommendation: the impedance tranche first (no new `DefType` machinery), then K11
containers, then this — at which point `discrete_alloc`'s obligations come along for free.

**Superseded 2026-07-28.** Both "extra gates" were in fact already cleared: the impedance
tranche built the container `DefType` itself, and `TryMetaContainerProcessing` branches ahead
of the `dont_cache_result` deferral. The typed `for_each_*` result container has therefore been
live since that tranche — member DOMAIN, member VALUES, the complete member SET and the ∀/K2
case (the domain argument a formal unit parameter, the member's type riding its unifier node)
are all checked at the definition; see the `fn_test_fe_neg*` battery, `_neg8` for the ∀ domain
half. What remains is out of scope by design: `MemberKind::Untyped` variants (`for_each_ne`,
whose member types could only come from evaluating the expression STRING) and `TemplateCopy`
members. `discrete_alloc`'s own container obligations landed separately with K11b, which found
its member set is NAME-DIRECTED rather than universal.

### 12.8 Composite-result member references — the slSubItemCall tranche (ruled + SHIPPED 2026-07-20/21)

**The ruling** (Maarten): `container a := discrete_alloc(…); … a/landuse …` in a function body
should type as `attribute<AT>(allocUnit)` with AT the typeNames array's domain — *symbolically*,
even when typeNames (or anything it depends on, transitively) rides a formal — and where typing
must defer, the definition-time placeholder must be broad enough never to false-error; the check
then happens at instantiation. That presupposes the reference is *legal* at instantiation, which
ruled the reduction extension in.

**As shipped:**

- **Reduction** (`FunctionApplication::ResolveBodySymbol`): a body slash-path descends the
  DECLARED structure segment-wise; a miss below an item WITH a calculation rule reduces to
  `slSubItemCall(ReduceBodyItem(item), rest)` — the cache layer's canonical
  `(SubItem base 'path')` form, whose `SubItemOperator` performs the per-application
  "instantiation-time typing": a missing member fails that DC's own `MakeResult` honestly. The
  deepest rule-bearing item on the walked path wins; rule-less misses keep the old `FindSubItem`
  throw; meta rules keep their own rejection (`ReduceBodyItem` throws it); and a base whose rule
  reduces to a CONFIG-item reference (a `sourceDescr` key: bare import/def-scope/param aliases)
  keeps the pre-tranche throw — `SubItemOperator` requires a cache base (review finding,
  live-repro'd Debug assert). `unique(x)/values`-style references thereby became legal in inline
  bodies, operator-genericly.
- **Checker flip (the S1 atom)**: the definition-time code-3 computed-member path now fires for
  ANY rule-bearing local (`RuleMayComputeSubItems`), because the old definition-time
  "Cannot find" throw became false the moment reduction accepts; `InferGeneratedMember` consults
  the member map on ANY DefType kind (a `unique` result is a UnitVal *carrying* members). Member
  maps are keyed case-insensitively with FIXED ASCII folding (the engine's item lookup accepts
  either case; locale-dependent folding was a review finding).
- **Vocabulary**: `ResultContainerMember(path, values, domain, vc)` + `ResultMembersComplete()`
  on `AbstrSignatureBuilder`/`SignatureRecord` (printer renders `{ path: attribute<V>(U); … }`).
  `ApplyOperRecord` builds the member map through the SAME `VN`/`DN` nodes the positions bound:
  member values stay class-level unless the var is also in a domain role (the batch-B K2 rule);
  member domains claim identity — `unique`'s `Values` rides the *same existential node* as the
  result unit, so `attribute<V> y (u) := u/Values`-style bindings unify. `vc` must be `Unknown`
  unless member-fixed (a wrong composition claim falsely eliminates downstream overloads). A
  complete-EMPTY set attaches too (review finding: it was silently dropped, making the plain
  `select_*` promise inert). Completeness licenses definition-time missing-member errors and is
  claimed ONLY where `CreateResult` provably creates nothing else.
- **Described members**: `unique` (`Values`: V class-linked + U domain, arg composition,
  complete — the flagship: `u/Values` types with V riding the formal's rigid node, verified by
  the roundtrip positive and the "inconsistent instantiation of type variable 'V': Float64 …
  vs String (the declared type of 'result')" negative); `subset`/`select_*` family (`org_rel` /
  `nr_OrgEntity` per `OrgRelCreationMode`: values = D — identity for free, D is in both roles —
  domain U, complete; the plain `select_*` groups are complete-EMPTY, so `select(...)/org_rel`
  errors at definition); `union` (`UnionData`: domain-only — the default-unit adoption makes
  values unclaimable — complete); `connect` (`geometry` Sequence + `arc_rel`, domain-only,
  complete). **dijkstra's OD sub-item zoo (SHIPPED 2026-07-21)**: `DijkstraMatrOperator<T>::
  DescribeSpecSignature` emits the 17 OD members per `Prod*` flag (`impedance`, `LinkSet`,
  `alt_imp`, `LinkAttr`, `D_i`/`M_ix`/`C_j`/`M_xj`, the `OrgZone_*` aggregates, `Link_flow`,
  and the four `*_rel`), each through the vars the arguments bound — members over the fresh
  `OD_Pairs` unit `R` (impedance/LinkSet/`*_rel`) share it; the zone aggregates ride `OZ`/`DZ`;
  the `*_rel` members claim values IDENTITY to the start/end/zone units (K2), impedance stays
  class-level `Imp`, mass/param values unclaimed. `impedance_matrix` is CACHEABLE, so
  `impedance_matrix(spec,…)/impedance` both types AND inline-reduces (slSubItemCall) in a body —
  the set is COMPLETE (df fixes exactly which exist). Non-OD keeps `ResultAttr(Imp, dstZones)`
  plus a non-complete `TraceBack` member. **`connect_info` members (SHIPPED 2026-07-21)**:
  `dist_info` (`OnlyDistResult`) was already a single `dist` attribute; `connect_info`'s
  container now emits its 7 members — `dist` (metric-less default dist class), `arc_rel` +
  `ArcID` (the deprecated alias, created at meta time — included so the COMPLETE set stays
  sound), `CutPoint` (points' coordinate class via `Vpt`), `InArc`/`InSegm` (Bool), `SegmID`
  (UInt32) — ALL keyed by the points' domain `Dp` (K3). `connect_info` is cacheable ⇒
  `connect_info(…)/dist` types AND inline-reduces. **`discrete_alloc` landuse (SHIPPED
  2026-07-21 — the array-spec join)**: `DescribeSignature` describes typeNames as an
  attribute (values = the names, domain = the allocation-types unit `AT`) so `AT` gets a var,
  and emits `landuse : attribute<AT>(allocUnit)` + `bid_price : attribute<S>(allocUnit)`,
  dropping the old `DynamicShape`/`ResultDeferred`. `landuse`'s type is STRUCTURAL — `AT` = the
  typeNames' domain, `allocUnit` = arg 1 — so `a/landuse` types even when typeNames/allocUnit
  are FORMALS (the ruling), with no closed-array evaluation needed. The set is INCOMPLETE (the
  name-directed `shadow_prices/<name>`/`total_allocated/<name>` and the conditional `bid_price`
  mean unknown members DEFER, never error — the ruled broad placeholder). The name-directed
  members' types come from the partitionings/suitabilities, not the type-names, so they stay
  deferred. `discrete_alloc` is cacheable ⇒ `a/landuse` inline-reduces too.

Adversarial review (workflow `wf_d8532e78-bd5`, 4 dimensions, 34 agents, all completed): 3
distinct confirmed defects, all fixed pre-landing — the non-cache-base crash (live-repro'd),
the dropped complete-EMPTY sets, locale-dependent case folding. Non-actionable observations:
`SubItemOperator`'s "Cannot find 'x' from ''" renders the cache root's empty name (pre-existing
message, honest but terse); deep multi-segment rests ride one `slSubItemCall` (GetCurrItem
handles paths). Tests `fn_test_subref{,2,_neg1..5}`; validation: 118/118 battery Release +
Debug (assertion-free), tst Operator `/Arithmetics` + `/Rescale` + `/MetaInfo`,
`examples/function.dms`.

### 12.9 The select family — complete composite-result type rules (2026-07-21)

The `select`/`subset` family spans two tiers, split by `oper_policy`. The **cacheable** tier
inline-reduces in function bodies and is fully typed by §12.8; the **meta** tier
(`dont_cache_result`) cannot inline-reduce in bodies at all (`SubstituteBodyExpr` rejects every
non-cacheable head with *"meta function call is not supported inside function bodies"*), so its
typing is diagnostic-only and — because its member set is directed by a *container* argument that
is almost always a formal — defers in nearly every body. It is specified here and scoped, mirroring
the `discrete_alloc` input-obligation discipline.

**Tier 1 — cacheable `SubsetOperator` (SHIPPED, §12.8).** One fresh existential subset unit `U`
(K6, value class `crd(D)` or the group's fixed `uintN`), plus at most one org-relation member:

| group(s) | `OrgRelCreationMode` | result | member | complete |
|---|---|---|---|---|
| `select`, `select_uintN` | `none` | `U` | — | ✓ (EMPTY — `sel/x` errors at definition) |
| `select_with_org_rel`, `select_uintN_with_org_rel` | `org_rel` | `U` | `org_rel : attribute<D>(U)` | ✓ |
| `subset` (runtime-obsolete) | `nr_OrgEntity` | `U` | `nr_OrgEntity : attribute<D>(U)` | ✓ |

The org-rel member's values ride `D` (the condition's domain node) in *both* a values and a domain
role, so its values-unit **identity** is claimed for free (the batch-B K2 rule): `v[s/org_rel]`
type-checks the reverse-index against `v`'s own domain. Verified by `fn_test_subref2`
(`select_with_org_rel` + `union`) and `fn_test_subref_neg4` (the complete-EMPTY `select_uint32`
report). `collect_by_cond`/`collect_by_org_rel` (batch B/D) are single-attribute results, not
composite: `-> attribute<V>(S)` over the passed subset unit `S`.

**Tier 2 — meta `SelectMetaOperator` (SPECIFIED, scoped).** `select_with_attr_by_cond`,
`select_with_org_rel_with_attr_by_cond`, `select_with_attr_by_org_rel` and their `uintN` variants
(`oap = {calc_never, calc_as_result}`). Given `(attrContainer, condition)`, `CreateResult` builds a
fresh subset unit `U` = `select_uintN(condition)` and, for **each data-item child `m` of
attrContainer whose domain unifies with the condition's domain `D`** (children named
`org_rel`/`nr_OrgEntity` skipped), a collected member `m : attribute<Vₘ>(U)` computed by
`collect_by_cond`; the `_with_org_rel_` variants add `org_rel : attribute<D>(U)`. So the composite
type rule is:

```
select_with_attr_by_cond(c: container; cond: attribute<bool>(D))
  -> unit<crd(D)> U { [org_rel: attribute<D>(U);]  mᵢ: attribute<Vᵢ>(U)  ∀ mᵢ∈c with domain≈D }
```

**Why it is scoped, not built.** (1) *Un-inlineable in bodies*: a meta head throws at reduction,
so even a perfectly typed `sel/population` could never inline-reduce — typing buys only
definition-time diagnostics on a body that must be used through the instantiating form. (2)
*Member set almost always unknowable*: `attrContainer` is typically a formal `container`
parameter (`DefType::Kind::Unknown`, typed-by-example), whose children vary per application — so
the member set is not enumerable at definition and the checker defers. It becomes knowable only
when `attrContainer` is a **closed external** (a def-scope container), and even then the
domain-match filter (`D` vs each child's domain) only decides when `D` is concrete — the same
closed-external narrowness as `discrete_alloc`'s obligations. (3) *A new mechanism*: members come
from enumerating a container argument's structure, not a name array, so it needs a
container-directed `MetaMemberLayout` variant distinct from the for_each path — non-trivial surface
for near-zero body value. Building it is deferred until a concrete need (a closed-external
meta-select in a checked body) appears. `collect_attr_by_cond`/`collect_attr_by_org_rel` (the
`CollectWithAttrOperator` meta pair, generating into a *given* domain with an org-rel indirection)
scope out for the same reasons.

**Current sound behavior (pinned).** `fn_test_selmeta` (positive) confirms config-scope
meta-select works end-to-end (the checker never runs there); `fn_test_selmeta_neg` confirms an
*applied* body using meta-select defers cleanly at the definition scan and then surfaces the
honest reduction rejection — proving no false definition-time error is introduced.

## 13. Risk register

| # | Risk | Sev | Mitigation |
|---|---|---|---|
| S1 | **false def-time error on an existing config** (the prohibited mode) | High | defer-by-default verdict table (§6.2); errors only when all candidates are described and eliminated soundly; arity mismatch always defers; K6 memoization; per-batch full battery + tst; any reported false rejection is a release blocker |
| S2 | description drifts from `CreateResult` | High | §9: debug replay verifier, merge audit, per-family tests, soundness bias |
| S3 | merged summary over-claims correlation when a group gains a heterogeneous member (other DLL) | High | congruence check over *all* current members; `Register` generation counter invalidates the cache; `cog_mul` sentinel test in batch A |
| S4 | trial-state leakage from speculation | Med | copy-trial-adopt makes leakage structurally impossible; the harness catches only `ExprParser` throws, only inside the loop |
| S5 | UnitNode surgery destabilizes existing domain checking | Med | dark landing (batch U); rename-dominated diff; class-node invariant confined to `BindUnit`/`LinkUnit`; battery gate before any operator uses it |
| S6 | values-identity checks collide with metric/borrowing semantics | Med | identity compared only via permissive `UnifyDomain` with `UM_AllowRightExpansion` (never `UnifyValues`); explicit default-borrowing regression config in batch B |
| S7 | arity errors contradicting `FindOper`'s widening | Med | arity mismatch always defers (§6.2); revisit only with a `FindOper`-mirroring predicate + tests |
| S8 | meta-thread / token-registry deadlock (the known `TokenStr` pitfall) | Med | materialize `SharedStr` before unifier/trial calls (pattern at `:3063`); summaries built under a once-guard |
| S9 | def-time cost at config load | Low | once-per-function memo, once-per-group summary, trials only for mixed groups; the trail mechanism stays in reserve |
| S10 | negative-test message churn | Low | keep message *shapes*; where roles improve wording, update tests deliberately in the same commit |
| S11 | **`BindDomain`/`LinkDomain` inherited `UnifyDomain`'s directional DC-interning asymmetry** — a one-way compare could throw a false *"inconsistent instantiation of domain variable"* at definition (an S1 instance). The asymmetry itself is **intentional** (commit `766848c9`, issue #361: worker-thread re-checks must not create DCs) | High | **Resolved 2026-07-17**: `UnifyMode` gains `UM_AllowRightExpansion = 64` — an explicit caller contract allowing right-side DC interning, valid only on the meta thread (enforced by `GetOrCreateDataController`'s release-active `MG_CHECK`). The checker's three sites (`BindDomain`, `LinkDomain`, `UnifyData` concrete-concrete) pass it via `TypeUnifier::s_CheckerUM`, making the compare total and symmetric there; the interim `DomainsUnify` two-direction helper is retired. Worker-thread behavior byte-identical (#361 fix preserved). Validated by 70/70 battery + tst + examples. A triggering config was never constructed; the fix rests on the demonstrated code-level inconsistency |

## 14. Rejected alternatives

- **Group-level static description with hand-written constraint sets** — unsound the
  moment the declared set and the registration typelist disagree (accepting a `V` with no
  member, or rejecting one that exists); also cannot describe groups whose members come
  from different typelists.
- **Per-member descriptions consumed individually, no merge** — O(members) trial work per
  application, terrible diagnostics ("none of 15 signatures matched"), and recovering
  variable structure from concrete tuples becomes guesswork.
- **A name-keyed signature registry** (the current stopgap, scaled up) — divorced from the
  implementation, no access to member state, already at its expressive ceiling at ~22
  entries. This design retires it.
- **A builder that writes straight into the unifier per application** — cannot merge across
  members, cannot be compared or cached, duplicates interpretation across consumers, and
  tempts `DescribeSignature` to read per-application state.
- **A pointer-keyed `UnitCreatorPtr` → derivation registry** — the creators are inline
  functions; each DLL holds its own copy, so tic-side pointer identity would not match
  clc/geo pointers. Replaced by `unit_creator_spec` bundling at the registration site.
- **Rigid (skolem) nodes for K6 fresh units** — false errors on legal configs (two
  identical `unique(x)` applications share one DC and unit).
- **`Dom::Unknown` for K6 fresh units** — loses same-application aliasing for no safety
  gain.
- **Trail/undo log first** — a standing correctness tax on a struct about to undergo the
  UnitNode surgery; kept behind a `Begin/Commit/AbortTrial` boundary as a later
  optimization.
- **Deriving `CreateResult`'s `UnifyDomain` role strings from the descriptions** —
  attractive "single source of truth", but converts a checking-layer change into a
  behavior-adjacent edit across ~200 operators. The debug verifier keeps the two aligned
  instead.
- **Probing operators with fabricated placeholder units at definition time** (§19) — the
  tempting reading of "just call `CreateResultCaller` for the hard cases". Rejected as a
  *typing* technique on two independent grounds: (1) fabricating placeholder units is
  **unsound** — two parentless fresh units share `GetFullName()==""`, hence one hash-consed
  key, hence one DataController, hence `UnifyDomain` returns true, so a probe would silently
  accept ill-typed code; (2) the composite operators it would be *for* (`discrete_alloc`,
  `impedance_matrix`) block the probe structurally (throwing, silently-empty, or mutating —
  §19). Retained only where it already lives: the concrete-unit metric oracle
  (`CreateValuesUnit`) and the §9 debug verifier.
- **Residuation / constraint-propagation framing** (§20) — over-poetic and factually
  wrong: there is no shared constraint store between the def-time unifier (state discarded
  at end of check) and reduction (`FuncDC_CreateResult` re-derives imperatively under a
  `FencedBlocker`). The staged-abstract-interpretation framing (§20) is used instead.

## 15. Open questions for review

1. **K11 containers**: v1 defers (prose only). Worth the container-kind work in `DefType`
   later, or do real configs not pay for it?
2. **Application-time `FindOper` adoption**: the record is deliberately sufficient (it
   subsumes the class array *and* the unit relations). Is replacing first-match-wins
   `IsDerivedFrom` selection a goal, or does it stay a checking layer indefinitely?
3. **The `generative` tightening** for K6 (§6.1): ship after batch D soaks, or never?
4. **Metric constraints (K10)** — now understood as *vacuous, not intractable* (§20): the
   metric algebra is a decidable free abelian group, and metrics defer only because a formal
   `unit<float64>` has no declarable metric today. If declared metric constraints on
   parameters land (hof P4/v2), the vocabulary already carries the relations (`MetricProduct`
   etc.) and def-time enforcement becomes a matter of un-deferring them — is that a wanted
   direction, or do metrics stay an instantiation-only concern by policy?

---

# Part II — the staging model (added 2026-07-16)

*Sections §16–§20 record the governing ruling of §1.1 and the analysis behind it. They
answer the question that prompted them — "how about calling `CreateResultCaller` for
ambiguous or complex cases, and how does that connect to definition-time checking and
Robinson unification?" — and they take precedence over §5–§14 wherever the two could be
read to disagree (§5.5's selector, §7's K10 rationale, the batch E/F scope).*

## 16. The staging contract in full

### 16.1 Two stages, two type systems

GeoDMS type checking splits along the boundary between a function's **definition** and its
**instantiation**, and the split is not a pragmatic convenience — it is where the type
system changes character.

**Definition time — symbolic.** A generic formal (`attribute<V>(D)`, `unit<uint32> Rd`)
introduces *placeholders*: the rigid/skolem variables the `TypeUnifier` already carries.
A placeholder is **never** turned into an `AbstrUnit`. The walker reasons about *structure*
— which positions share a unit variable, which value classes a variable may take — and
about the *free fragment* of the theory: unit **identity** relations that hold for every
instantiation. This is exactly what `DescribeSignature` publishes for the well-behaved
operators: `lookup`'s `E2`-in-both-roles (K2), `sum`'s result-domain-is-the-partition
(K5), the arithmetic same-domain/same-class shapes (K1/K9-class-part).

The walker does **not**, at definition time, do any of:
- `CreateResultCaller` or the operator's own `CreateResult` unit machinery,
- `UnifyValues` (values-side *metric/projection* comparison),
- metric derivation (`operated_unit_creator` and its `cog_mul`/`cog_div` probes),
- any check of a *composite/opaque* operator's internal argument constraints.

**Instantiation time — concrete.** Every placeholder is now a real unit; every expression
has an actual type, metric, and unifiability. `FuncDC_CreateResult` →
`CreateResultCaller` (`MoreDataControllers.cpp:627-653`) runs the full concrete semantics,
identical to today. This is where `UnifyValues`, metric algebra, counts, ranges, and the
composite operators' `CreateResult` preambles all fire, and where a declared result type
(§17) is discharged against the computed one.

### 16.2 Why the exclusion list is principled, not pragmatic

The three excluded classes of work are excluded because at definition time they are either
**ill-posed** or **vacuous** on placeholders — not merely expensive:

- **`CreateResultCaller` / composite `CreateResult`** — ill-posed. These read argument
  *values* or *ranges* (a spec string that decides arity, a `GetCount()` that sizes a
  loop, a container's member names); on a placeholder there is no value to read. §19
  shows the concrete failure modes.
- **`UnifyValues` metric/projection** — vacuous. A placeholder values unit has an empty
  metric and null projection; the comparison always succeeds and constrains nothing (§20).
- **Metric derivation** — vacuous for the same reason; the derived result metric is a
  fresh empty metric that no def-time obligation can contradict.

What remains — unit **identity** (`UnifyDomain`, nominal) — is the one fragment that is
both **well-posed on placeholders** (identity is structural, needs no value) and
**non-vacuous** (two placeholders are provably distinct *as variables* under union-find,
even though they would be indistinguishable as fabricated `AbstrUnit`s — see §19). That is
why the def-time checker is precisely a *Robinson unifier over unit identity*, and nothing
more (§20).

### 16.3 Concrete stays concrete

Placeholders arise **only** where a formal's type is generic. A signature that names a real
config unit — `attribute<float64> x (Road)`, `Road` resolved by `ResolveUnitInScope`
(`AbstrCalculator.cpp:2565`) — keeps that unit, and the walker's own symbolic
comparison on it (`UnifyData`'s `Dom::Concrete` case) stays. So the exclusion list means
"the operator's `CreateResult` is not *invoked* at definition time", **not** "the walker is
blind to units": concrete-domain mismatches inside a body (passing a `Road` attribute where
a `Region` domain is declared) are still caught at definition, exactly as the `fn_test_dt`
battery already exercises.

## 17. Declared types as the inference boundary

Because complex expressions are opaque at definition time, the language leans on
**declarations as the inference boundary** — the same discipline the user stated:

```
land_use : attribute<LandUseType>(CompactedDomain) := discr_alloc/landuse;
```

Here `discr_alloc/landuse` is a member of an opaque composite result: its inferred type is
**⊤ ("something")**. The rule is `infer(expr) ⊑ declaration`, and **⊤ conforms to any
declaration vacuously** — "something which could match". The declaration is *assumed* at
definition and *discharged* at instantiation, where `discr_alloc`'s real `CreateResult`
runs and the actual result units are checked against the declared ones.

This is not new machinery: the existing `TreeItem_CreateConvertedExpr`
(`TreeItem.cpp:2880-2905`) already performs exactly this declared-vs-computed check at
instantiation via `CheckResultItem` (`UnifyDomain`/`UnifyValues`), and it even **inserts a
`convert(...)` coercion** when the declared and computed values units differ but are
compatible (`:2899-2903`). Crucially it is **skipped inside templates** (`InTemplate()` at
`:2931`) — which is precisely the definition-time gap the placeholder model formalizes:
templates/functions are checked structurally at definition and concretely at instantiation,
and `TreeItem_CreateConvertedExpr` is the instantiation half. A def-time checker must
therefore **not** claim to model the coercion insertion — it only checks conformance up to
⊤; the coercion is the instantiation stage's business.

**Underdetermined intermediates — warn and defer, never error.** A bare §5.12 item
(`x := <expr>;`, no declared type) whose expression is opaque has inferred type ⊤. It does
not error (that would violate S1 — never error where deferring succeeded); its type stays ⊤
and propagates as unknown, and the walker emits a **definition-time warning** listing the
underdetermined spots ("cannot infer the type of `x` at definition; it will be checked at
each instantiation — add a declaration to check it here"). This can only affect the *new*
§5.12 bare-declaration form: a classic `attribute<V>(D) x := …` already carries a full
declared type, so no legacy config can reach the warning. The declaration is thus the lever
the author pulls to *move* a check from instantiation-time back to definition-time.

## 18. The one oracle that survives: `FindOper` for selection

The exclusion list removes `CreateResultCaller`, but it leaves the **selection** oracle
fully available, because operator selection is a different thing from unit checking and is
already unit-free.

### 18.1 Why `FindOper` is none of the excluded things

`AbstrOperGroup::FindOper(nrArgs, ClassCPtr[])` (`OperGroups.cpp:355-431`) is pure class
dispatch: it walks the member list matching argument **classes** with `IsDerivedFrom`. It
touches no units, no data, no metrics; it allocates nothing and takes no lock on the happy
path. And it is *the very function reduction uses* — `FuncDC::GetOperator` feeds it
`argDC->GetResultCls()` (`MoreDataControllers.cpp:512-518`). A def-time selector that calls
the same `FindOper` on the same argument classes is therefore **exact by construction**: it
inherits whatever the first-match-wins-over-reverse-registration-order (§2.1) resolves to,
identically to reduction, with zero risk of a hand-written model drifting from it.

### 18.2 Synthesizing the argument classes item-free

The walker has, for a well-typed position, a concrete `ValueClass` (and, once §18.4 lands,
a `ValueComposition`). The class is derivable with no item, no unit, no data:

```cpp
// OperSignature.h — pure field reads, all item-free
inline ClassCPtr ClassOfAttr(const ValueClass* vc, ValueComposition comp) {
    auto uc = UnitClass::Find(vc);            if (!uc) return nullptr;   // UnitClass.cpp:177
    auto vt = uc->GetValueType(comp);         if (!vt) return nullptr;   // UnitClass.cpp:183
    return DataItemClass::Find(vt);           // DataItemClass.cpp:186 — nullptr-safe
}
inline ClassCPtr ClassOfUnit(const ValueClass* vc) { return UnitClass::Find(vc); }
```

**Use `Find`, never `FindCertain`** — `DataItemClass::FindCertain` (`:192-204`) calls
`context->throwItemErrorF(...)` on a miss and **null-derefs a null `context`**. Note also
that composition is **not** a separate enumeration axis: `UnitClass::GetValueType`
(`UnitClass.cpp:183-189`) folds Polygon/Sequence/MultiPoint into one `GetSequenceClass()`,
so `DataItemClass` is keyed on the `ValueClass` alone.

### 18.3 Enumeration is the exact ∀-selector

For a rigid ∀-variable with a declared constraint (`<V: numerics>`), the walker enumerates
the **witness classes** and calls `FindOper` per witness. This is exact, not sampled,
because the value-class universe is **closed and finite**: `VT_Count = 69`
(`ValueClassID.h:121`), enumerable via `ValueClass::FindByValueClassID`, and
`MatchesGenericConstraint` (`TreeItem.cpp:120-137`) is a pure predicate, so a constraint's
witness set is computed by a 69-iteration loop rather than hand-written (this is what §5.5's
merge approximates; enumeration makes it exact and demotes the merge to a printing aid).

Units cannot be enumerated this way — they are an **open** universe — which is the deep
reason the two sides of a type are treated so differently: **value classes are discharged
by enumeration; unit identity is discharged by symbolic unification (§20).**

Bounds and rules that keep this cheap and sound:
- **Defer if any position's class is unknown.** `FindOper` takes a full `ClassCPtr[]` with
  no holes; enumerating an unknown position over all 69 classes (× ~95 `cog_mul` members ×
  the throw cost) is a non-starter. Enumerate only the constraint-bounded ∀-variables; a
  2-arg operator over `numerics` (~12 classes) is ≤144 calls, cached per
  `(group, arity, constraint-tuple)` behind the `Register` generation counter (§2.1).
- **Wildcard arg positions** — registrations using `TreeItem`, `AbstrUnit`, `AbstrDataItem`,
  or **`AbstrDataObject`** (the fourth wildcard, via `AbstrDataObject.h`'s `using base_type
  = AbstrDataItem;`) as an arg class match *every* witness, so enumeration returns the same
  member for all witnesses → trivially congruent → sound.
- **Congruence check** replaces "did they all pick the same member?": if every witness
  selects a structurally congruent member, the ∀ is well-typed; if witnesses split across
  incongruent members (a genuinely heterogeneous group like `cog_mul`), **defer**.

### 18.4 Small additive changes required

- `TryFindOper` — a `noexcept`, nullptr-returning twin of `FindOper`; make
  `FindOper = TryFindOper + throw`. (Enumeration would otherwise pay a thrown exception per
  non-matching witness, and `FindOper` also **derefs each arg class with no null check** at
  `:398`, so the caller must null-check anyway.)
- Guard `IsTemplateCall()` before calling (the walker already branches user functions to the
  function path at `:3133`, so this is belt-and-suspenders).
- `DefType` gains a `ValueComposition` field — it carries none today (`:2495-2517`), and
  `PositionType` already holds the item (`:2722`) so `GetValueComposition()` is one line.

## 19. Probing: where it legitimately lives (and where it does not)

The instinct behind "call `CreateResultCaller` for the hard cases" is sound enough that the
tree **already does it** — but only under conditions the definition-time walker cannot meet.

### 19.1 The existing precedent

`AbstrOperGroup::CreateValuesUnit` (`OperGroups.cpp:457-500`) is a production type oracle:
it hand-builds an `ArgRefs` of **concrete** units, stack-constructs a
`LifetimeProtector<TreeItemDualRef>`, calls `FindOperByArgs` + `CreateResultCaller(*ref,
unitSeq, LispPtr())`, reads `resultRef->GetOld()` as the answer, and discards the probe.
It runs on every `mul`/`div`/`sqrt` metric derivation (via `operated_unit_creator`,
`UnitCreators.h:32`), and `Canyon.cpp:128-142` nests the same idiom. So probing is a
legitimate, load-bearing technique — for the **metric algebra**, on **ground** units.

### 19.2 Why it cannot type a function definition

Two independent walls.

**(a) Fabricated placeholder units are unsound.** To probe an operator for a *generic*
formal, the walker would have to fabricate a stand-in unit. But two parentless fresh units
have `GetFullName() == ""` (`persistent.cpp:323-354`), so `Unit<V>::GetKeyExprImpl`
(`Unit.cpp:122-167`) brands both with the identical key `BaseUnit(left("", uint32(0)), VT)`;
LispRefs are hash-consed (`LispRef.cpp:494`), so both resolve to **one** DataController,
so `AbstrUnit::UnifyDomain` returns **true** (`AbstrUnit.cpp:316`) for two units that
should be distinct. A probe built on fabricated placeholders would therefore **silently
accept ill-typed code** — the worst possible failure. (The escape hatches — a unique
`FullName` under a parent, or `IsCacheItem()` status, which skips the DC path at
`:300/:309` — are what production uses, but they require real tree/cache context the def-time
walker has no reason to build.) The staging ruling *dissolves* this: with formals kept as
**union-find variables, not units**, two placeholders are distinct *as variables*, and no
fabrication ever happens.

**(b) The composites block the probe structurally** — and these are exactly the operators
the question was about:
- `discrete_alloc` fails **three** ways: `ggTypeSet->GetCount()` (`DiscrAlloc.cpp:3330`)
  **silently returns 0** on a passor domain → the probe "succeeds" with a structurally empty
  answer (worse than throwing); `DataReadLock` (`:1589`) **throws**;
  `debug_refcast<FuncDC&>(resultHolder)` (`:3405`) **throws** — `MG_CHECK` is release-active
  (`Check.h:180/183`, outside the `MG_DEBUG` guard), so the `debug_` prefix is a misnomer.
- `impedance_matrix` reads its spec string at meta time —
  `const_array_cast<SharedStr>(paramStrA)->GetIndexedValue(0)` (`Dijkstra.cpp:1327`) —
  which **throws** on a dataless arg, and the string's value *decides the arity* (K13), so a
  probe cannot even determine the argument count.
- Whole classes are off-limits: value-reading operators (ForEach/Loop/Overlay/SubItem/
  OperPropValue), `has_external_effects`/`calc_requires_metainfo` (a probe would touch the
  filesystem), and `Checker.cpp:43-45` which **mutates its argument**. And every probe
  passes `LispPtr()` for `metaCallArgs` where reduction passes
  `funcDC->GetLispRef().Right()` (`MoreDataControllers.cpp:653`) — consumed by
  `Subset.cpp:231/453`, `BoostXML.cpp:314` — so a probe **silently diverges** from reduction
  even when it does not throw.

### 19.3 The general law, and probing's real home

**Probing's value scales with argument groundness, and definition time is definitionally
non-ground.** Where the walker has concrete units, a probe is redundant with what reduction
will do anyway; where it has placeholders, a probe returns fabricated-unit answers that
teach nothing (or silently mislead). So probing is **not** a definition-time typing
technique. Its two legitimate homes are both ground: the concrete-unit metric oracle
(`CreateValuesUnit`, §19.1), and the **§9 debug drift-verifier**, which runs *after* a real
`CreateResultCaller` at instantiation, where the units are ground and the probe returns
ground truth to check the description against. That verifier is the right and only place a
`CreateResultCaller`-based check belongs.

## 20. The type theory, precisely

The staging model has a clean theoretical reading, and getting it right matters because two
tempting framings are wrong.

### 20.1 Three fragments

1. **Free fragment — unit identity.** `UnifyDomain` is nominal (identity of the defining
   expression), so unit identity is an *uninterpreted* theory: units are opaque constants,
   equality is syntactic. Over this fragment, **Robinson unification applies directly**, and
   the `TypeUnifier`'s union-find is the standard implementation. Soundness needs `UnifyDomain`
   restricted-to-the-unifier's-mode to be an equivalence relation, which it is: Void is
   excluded at all three entry points (`PositionType:2790-2791` → `Dom::Void`; `UnifyData`
   early-out; the arg binder's `VT_Void` skip), so it never reaches a node and cannot break
   transitivity; and `IsKindOf` (`AbstrUnit.cpp:283`) forces same-value-type, making
   `HasFixedValues` a genuine equivalence class. (Corollary: the `UM_AllowVoidRight` flag on
   the unifier's `UnifyDomain` calls is **vestigial** — kept defensively; no Void unit ever
   reaches those sites. And the *directional* default of `UnifyDomain` itself —
   `GetOrCreate` left, `GetExisting` right, the intentional #361 worker-thread fix — is
   neutralized for the checker by `UM_AllowRightExpansion` (§8's S11 resolution), which
   makes the comparison total and symmetric under the checker's meta-thread contract, so
   union-find over it is a genuine equivalence.)

2. **Interpreted fragment — metrics.** `UnitMetric` is a scale factor plus a
   `map<TokenID, Int32>` of base-unit exponents; `SetProduct`/`SetQuotient`
   (`Metric.cpp:243-311`) are pointwise exponent add/**subtract**. This is a **free abelian
   group** — every element is invertible, `r = a·b` *is* solvable for `b` as `r·a⁻¹` — i.e.
   Kennedy's units-of-measure, which is a **decidable, finitary** unification theory. So
   metrics are **not** out of algebraic reach. They defer at definition time for a different
   reason: they are **vacuous** there. A formal `unit<float64>` has an empty metric and there
   is no surface syntax to declare one, so every metric relation a body induces merely
   *defines* a fresh metric variable rather than constraining anything. *Nothing to check ≠
   can't check.* Hof §11's "by decree" is the right policy; the rationale is "no declarable
   parameter metric yet" (hof P4/v2), not "undecidable". **Do not write that metric
   unification is intractable — it is the opposite.**

3. **Genuinely staged fragment — value-dependent shape, counts, ranges.** K13
   (a spec string decides arity/roles), counts, and ranges are not equational relations at
   all: they are *functions of data* that only exist once arguments are ground. No unification
   theory, decidable or otherwise, applies; these are correctly and permanently
   instantiation-only.

### 20.2 Not residuation — staged abstract interpretation

It is tempting to call the deferral "residuation" (a constraint suspends at definition and
*wakes* at instantiation). That framing is **wrong**: residuation needs one shared constraint
store, and there is none. The def-time unifier's state is **discarded** when the check ends;
`FuncDC_CreateResult` (`MoreDataControllers.cpp:627-653`) re-derives every unit relation
**imperatively** from ground units under a `FencedBlocker`, consulting no def-time state.
Nothing wakes; nothing propagates back.

The correct framing is **staged abstract interpretation**. The description layer is a sound
**over-approximation** of `CreateResult`'s unit behaviour; a deferred/⊤ result is the lattice
**top**; "defer, never error" is the statement that the abstraction is *sound* (it never
concludes false where the concrete would succeed). The §9 debug verifier is then exactly a
check of the **abstraction relation** — α(concrete result) ⊑ described result — run on every
real reduction. This is not decoration: it names the actual proof obligation the whole design
must meet, and it explains defer-by-default as a lattice property (⊤ is always sound) rather
than a hand-wavy hedge.

### 20.3 One-line summary

*Definition-time checking is Robinson unification over the free fragment (unit identity) with
value classes discharged by finite enumeration; the interpreted fragment (metrics) is
decidable but vacuous for want of parameter-metric syntax; and the value-dependent fragment
is genuinely instantiation-only. The whole is a sound abstract interpretation of
`CreateResult`, with `⊤` = defer and the §9 verifier checking the abstraction relation.*
