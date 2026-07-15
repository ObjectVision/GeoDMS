# Operator unit-constraint signatures via a virtual interface

**Status: DESIGN — not implemented. Companion to `typed-hof-language-design.md`
(WP4.1 "operator signature reification"), which this document supersedes for
everything beyond its interim batch 1.**

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
positions with `IsDerivedFrom`, first-match-wins (`OperGroups.cpp:355-431`);
`FuncDC::GetOperator` feeds it the argument DCs' result *classes*
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

An instantiated `BinaryAttrOper<Float32,Float32,Float32>` does not know its family ranges
over `numerics` — that fact lives in the registration typelist
(`BinaryInstantiation<TL,…>`). Conversely, a hand-written family-level constraint set can
silently disagree with what was actually instantiated, which would make the walker
**unsound**.

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

Uses: def-time error text; enriching `FindOper`'s failure message
(`OperGroups.cpp:415-428`, classes-only today) with described signatures; and a future
signature browser / generated operator docs — the first place `discrete_alloc`'s contract
would be stated anywhere findable. Having two consumers from day one is itself a check on
the vocabulary: anything the printer cannot render means the record is too operational.

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
| K10 | `MetricProduct/Quotient/Power`, `Dimensionless` | **declared-deferred** (metrics are per-application by decree, hof doc §11); printer + verifier consume |
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
both directions** — exactly the shape already used for domains at `:2833-2834`. Rationale:
runtime values-side relations run under operator-specific modes (lookup's
`UM_AllowDefaultLeft` borrowing, `lookupImpl.h:74`) and **metrics are per-application by
decree**, so def-time identity comparison must be key-identity/default-borrowing tolerant
and must never call `UnifyValues`-with-metric. When in doubt the block returns (defers).

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
v1 records `GeneratedUnit("U")` and `ResultDeferred` for the container-shaped result (the
`crd(D)` class relation, `Unique.cpp:341-343`, is a printable annotation in v1).

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
| **D** | fresh-unit family: `unique`, `select`/`subset`, `union` | K6 generative nodes | low / modest (ranked by description simplicity + printer value, not unification power) |
| **E** | `discrete_alloc` partial + `connect` family | `ArgContainer`, `DeferredRelation`, K16 | low / diagnostics + docs |
| **F** | `impedance_matrix` / dijkstra | `DynamicShape` | low / docs. Do last |

## 13. Risk register

| # | Risk | Sev | Mitigation |
|---|---|---|---|
| S1 | **false def-time error on an existing config** (the prohibited mode) | High | defer-by-default verdict table (§6.2); errors only when all candidates are described and eliminated soundly; arity mismatch always defers; K6 memoization; per-batch full battery + tst; any reported false rejection is a release blocker |
| S2 | description drifts from `CreateResult` | High | §9: debug replay verifier, merge audit, per-family tests, soundness bias |
| S3 | merged summary over-claims correlation when a group gains a heterogeneous member (other DLL) | High | congruence check over *all* current members; `Register` generation counter invalidates the cache; `cog_mul` sentinel test in batch A |
| S4 | trial-state leakage from speculation | Med | copy-trial-adopt makes leakage structurally impossible; the harness catches only `ExprParser` throws, only inside the loop |
| S5 | UnitNode surgery destabilizes existing domain checking | Med | dark landing (batch U); rename-dominated diff; class-node invariant confined to `BindUnit`/`LinkUnit`; battery gate before any operator uses it |
| S6 | values-identity checks collide with metric/borrowing semantics | Med | identity compared only via two-direction permissive `UnifyDomain` (never `UnifyValues`); explicit default-borrowing regression config in batch B |
| S7 | arity errors contradicting `FindOper`'s widening | Med | arity mismatch always defers (§6.2); revisit only with a `FindOper`-mirroring predicate + tests |
| S8 | meta-thread / token-registry deadlock (the known `TokenStr` pitfall) | Med | materialize `SharedStr` before unifier/trial calls (pattern at `:3063`); summaries built under a once-guard |
| S9 | def-time cost at config load | Low | once-per-function memo, once-per-group summary, trials only for mixed groups; the trail mechanism stays in reserve |
| S10 | negative-test message churn | Low | keep message *shapes*; where roles improve wording, update tests deliberately in the same commit |

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

## 15. Open questions for review

1. **K11 containers**: v1 defers (prose only). Worth the container-kind work in `DefType`
   later, or do real configs not pay for it?
2. **Application-time `FindOper` adoption**: the record is deliberately sufficient (it
   subsumes the class array *and* the unit relations). Is replacing first-match-wins
   `IsDerivedFrom` selection a goal, or does it stay a checking layer indefinitely?
3. **The `generative` tightening** for K6 (§6.1): ship after batch D soaks, or never?
4. **Metric constraints (K10)**: currently declared-deferred at def-time per hof doc §11.
   If declared metric constraints on function parameters ever land (that doc's v2 option),
   the vocabulary already carries the relations and could start enforcing them.
