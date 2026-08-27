# GeoDMS as a typed higher-order function language — design

*Status: design + implementation log. Started 2026-07-11 as a design proposal; the bulk
of it is now implemented on branch `hof_syntax` (v20.9.0). All file:line
references were verified against that tree.*

**Implementation status (v20.9.0).** Done and tested end-to-end:
- **P0** — `function` keyword, typed parameter telescope, designated `result`, strict
  args-plus-imports scoping, arity + name-collision checks (§5).
- **P1** — function applications as expressions via meta-time β-reduction; applicative
  identity (identical calls intern to identical keys) with no new DataController (§10 P1).
- **P2** — value-type polymorphism (`function f<V: numerics>`), dependent-position
  signature checks, and **variants** (`function name { variant … }`, §5.7).
- **P3** — function-valued parameters, partial application (`F(a, _)`, WP3.1), typed
  `map` over containers (WP3.3), definition-time scope/shape checking (WP3.4).
- **Types** — `alias = type;`, declared function signatures, refinement aliases
  (`alias = type, IntegrityCheck = …`, WP4.3), metric-via-alias (WP4.4).
- **Groundwork** — the Prolog `Occur` occurs-check fix (WP4.1 prerequisite).
- **§5.9 application forms** — the explicit `apply`/`instantiate` keywords with the
  no-holder-magic rule (Phase A), container literals `domain { m: e; … }` in
  argument position, destructured at β-reduction (Phase B), and `apply T(args)` for
  templates via β-reduction of the CI-unique `result` sub-item (Phase C). See §5.9.
- **WP4.5 prelude** — `res/prelude.dms` auto-imported as the implicit outermost
  namespace; 25 RewriteExpr.lsp rules retired as typed functions (§8.4).
- **§4.6 revision** — lexical definition-scope for function bodies (call-site
  isolation kept); no `using` needed to see siblings of the definition.
- **WP3.2 / §5.10** — function-valued results, closures (capture-by-value,
  hygienic), applied call results `f(g)(x)`, generic signature aliases
  (`nuf = function<V: numerics, D: domains>(…) -> …`), type application
  (`f: nuf<V, D>`), domain type-variables.
- **§5.7 v2** — variant specificity (most-specific dispatch over constraint
  acceptance sets) + definition-time disjointness of variant sets.
- **WP4.1 t1+t2+t3** — type-application binding enforcement; full application-time
  unification with variable-variable links and constraint-set intersection; the
  definition-time typed body walker with rigid (skolem) variables — a generic body
  is now rejected at first reference when it does not hold for EVERY instantiation
  (§5.10 status block).
- **§5.11 tier A** — the brace-disambiguation rule (unbracketed `{` after a rule =
  sub-item block; braces are expression content only inside parentheses), anonymous
  whole-rule functions (`value := function(…) …`), designation-by-name
  (`-> restype { result := …; }`), and anonymous result-position functions
  (`:= function(…) -> T := e;` replacing the nested `function result` idiom).
- **§5.13 meta-reference parameters** (`item x`, 2026-07-17) — the argument binds as
  a raw item reference (the `sourceDescr` form), unlocking the retirement of the six
  `PropValue` property-accessor rewrite rules to prelude functions with
  alias≡direct key identity for containers, units and attributes.
- **§5.14 variadic rest parameters** (`...x`, 2026-07-18) — a function's last
  parameter may bind the argument TAIL (one or more); recursive folds with strictly
  decreasing arity are permitted; variant sets now also dispatch on body-nested and
  argument-nested calls. Retires the `concat`, `replace_value` and `combine_data`
  rewrite chains to prelude variadic functions.
- **§5.16 the `@checkfunctions` audit** (2026-07-22) — an opt-in pass that runs the
  definition-time checker on every function DEFINITION, closing the last gap in
  "definitions type-checked": a defined-but-never-referenced function was otherwise
  never checked (the checker fires only on application). Config load stays untouched;
  surfaces are the `GeoDmsRun @checkfunctions` verb and the GUI Tools action.

Not yet implemented (specs below remain actionable): the redundant top-level
`{ X(args) }` sugar (§5.9 deferred list), the opt-in `applyF` boundary
DataController (WP4.2), the declarative operator-signature interface (WP4.1
remainder, §10 P2 — designed in `operator-signature-interface.md`: operators
describe their unit constraints through a virtual, unlocking the relational and
composite families that the interim `OperSigKind` registry cannot express), and
the WP4.5 tranche-3 heads (variadic / optional-argument rules). Verification
configs live in the git-tracked `testcases/` directory (`fn_test*.dms` plus a few
`fn_probe_*` probes and `tmpl_regress.dms`); run the whole suite through
`testcases/run_testcases.bat` (positives must exit 0, `_neg`/`defcheck` cases must
exit nonzero). The shipped example is `examples/function.dms`.

## 1. Motivation and scope

GeoDMS already has a type system — it is just implicit, distributed over C++ meta-code,
and invisible to users. Every modeller reasons daily in terms of it: attributes are
materialized mappings `V[D]` from a domain unit `D` to a values unit `V`; containers
group items; templates abstract over parameters. This document makes that system
explicit, completes it, corrects the places where a naive formalization would create
surprise, and specifies what is needed — syntactically and in rtc.dll — to extend it
with **typed, closed, first-class functions**, up to higher-order functions, while
leaving the tiled data engine untouched.

The starting sketch to complete and correct:

- attributes as mappings `V[D]` (type of an attribute with domain D, values unit V);
- units as of type `range(a, b)`;
- `container C { a: A; b: B }` as `struct C { a: A; b: B }`;
- `range(a,b) { name_i : range(a,b) -> vt_i }` as a table type;
- **functions**: template-like items with a fixed typed argument list `(a_i : A_i)`, no
  visibility of the instantiation site except the provided arguments, and a designated
  `result : R`, giving the item the type `(A_i) -> R`.

Decisions fixed up front:

1. **Function scoping** — originally strict args-only with explicit `using` imports;
   **revised 2026-07-13 to lexical definition-scope with call-site isolation** (§4.6):
   identifiers in a definition's expressions resolve to what is visible at the
   definition point, as everywhere else in the language.
2. **RewriteExpr.lsp is to be phased out** (§8): its definitional rules become typed
   functions with argument-type-specific result-type *derivation* (§5.7), its
   normalizations become variadic/optional-argument operator features, and its
   simplifications become a compiled-in typed pass.
3. **Type-dependent overloading of functions** is part of the design (§5.7): one
   function name, multiple typed variants, resolved by argument types — the user-space
   counterpart of what OperatorGroups do for built-ins.
4. This document is the deliverable; implementation (starting with P0, §10) follows
   separately.

## 2. The two-stage model — where types live

GeoDMS is a two-stage language:

- **Meta stage**: calculation-rule evaluation, unit creation, template instantiation,
  `for_each` — this stage builds the DataController (DC) DAG.
- **Data stage**: tiled array computation over that DAG.

Types live at the **meta stage**. This has one immediate, load-bearing consequence:
domain ranges and cardinalities are frequently *data-stage* values (`select` results,
dynamic `nrofrows`), so they **cannot be components of static types**. They enter the
system only as *refinements*: optionally-static predicates, checked lazily
(IntegrityCheck-style) when data materializes.

The stage split is also what makes higher-order functions cheap to add: all
function/closure machinery evaluates and disappears at the meta stage (§5.6); the data
engine only ever sees first-order operator applications on concrete units.

## 3. The type system GeoDMS already has, made explicit

### 3.1 Sorts

- **𝕍** — the closed, finite universe of value types, enumerated by `ValueClass`
  (`rtc/dll/src/mci/ValueClassID.h:20-123`): `uint2..uint64`, `int8..int64`,
  `float32/64/80`, `bool`, `string`, `spoint..dpoint`, `void`, plus range/sequence
  forms. Type *classes* over 𝕍 (numeric, integral, ordered, point, domainable) already
  exist as the rtc typelists (`rtc/dll/src/RtcTypeLists.h:24-139`; note the dedicated
  `domain_ints`/`domain_points`/`domain_elements`/`domain_types` lists at 105-129).
- **𝕄** — metrics: the free abelian group over base-unit symbols with a rational scale
  factor — Kennedy-style units of measure. Already implemented: `UnitMetric` = factor +
  base-unit power map (`rtc/dll/src/tic/Metric.h:18-47`), with `SetProduct`/
  `SetQuotient` merge-adding/subtracting powers (`Metric.cpp:243-311`). Projections
  (`UnitProjection`, `tic/Projection.h:22-50`) play the same role for grid domains.

### 3.2 Type grammar (the meta-stage object language)

```
UnitType    ::= unit<vt> & refinement*        refinement ::= metric(μ) | projection(π)
                                                          |  crs(σ)                (§3.5, §4.9)
                                                          |  range(a,b) | nrofrows(n)
DomainType  ::= unit<vt>                      vt domainable: integrals, points, bool, void
AttrType    ::= V[D] ^vc                      V, D are unit ITEMS (dependency);
                                              vc ∈ {single, arc, polygon}
ParamType   ::= V[void]                       scalars = attributes over the one-point domain
RecordType  ::= { n_i : T_i }                 containers; DEPENDENT telescope — a later
                                              T_j may reference an earlier n_i
TableType   ::= Σ(U : DomainType) · { n_i : V_i[U] }      sugar: unit<vt> U { … }
FuncType    ::= (a_1:T_1; …; a_n:T_n) -> R    dependent product (telescope): later T_j
                                              and R may mention earlier a_i
Scheme      ::= ∀ V:class . FuncType          value-type polymorphism only; unit
                                              polymorphism is free via unit-typed params
Overload    ::= { FuncType_k }                one name, k typed variants with pairwise
                                              disjoint (or strictly ordered) domains of
                                              applicability (§5.7)
```

Correspondence to the implementation:

- An item's type is the **triple (DomainUnit, ValuesUnit, ValueComposition)** — the
  composition sits on the data item, not the unit (`DataItemClass` is keyed by values
  type only, `tic/DataItemClass.h:54-115`; the triple is stored on `AbstrDataItem`,
  `tic/AbstrDataItem.h:164-165`). This is `V[D]^vc` verbatim.
- Domain eligibility is a class property: `AbstrUnit::CanBeDomain()` defaults false and
  is true only via `IndexableUnitAdapter` under `CountableUnit`
  (`tic/Unit.h:228-261`, with a `static_assert` on integrality). This is the
  `DomainType` sort.
- Parameters *are* `V[void]`: `CreateCacheParam<V>` builds a data item over the default
  `Unit<Void>` (`tic/Param.h:42-50`); literals intern as void-domain const params
  (`NumbDC::MakeResult` → `CreateConstParam<Float64>`,
  `tic/MoreDataControllers.cpp:912-925`).

### 3.3 Dependency, kept decidable

This is a *dependent* type system — types mention unit **items**, not just value types
(`attribute<meter> length (Road)` mentions two items). It stays decidable and fast
because of three restrictions, all already true of the implementation:

1. **Unit equality = identity of the defining expression.** Every expression is a
   hash-consed `LispRef` (interned per node kind, `sym/LispRef.cpp:381-494`; structural
   equality *is* pointer equality, `sym/LispRef.h:106`), and every expression keys a
   `DataController` in the global `s_DcMap` (`tic/DataController.cpp:394-476`). So unit
   equality is an O(1) comparison, and identical defining expressions yield the *same*
   unit.
2. **Refinements are either static literals** (checked by interval inclusion at meta
   time) **or deferred to data-stage checks** (IntegrityCheck-style).
3. **No type-level computation** beyond substitution of item references.

### 3.4 The categorical reading (why the design is coherent)

Domain units are objects; an attribute `r : D[E]` is a morphism `E → D`;
`lookup(r, x) = x ∘ r` is composition; `id(D) : D[D]` is identity; parameters are
morphisms out of the terminal object `void`; aggregation by a partitioning attribute
`p : P[D]` is pushforward along `p`. A table is a record of morphisms out of one object.

This is not just decoration: it fixes the arrow-direction conventions that confuse
users (partition vs. index attributes), and it is already in the language — the
expression operator `index -> expr` substitutes `expr` with the search context swapped
to `index`'s **values unit** and produces a lookup (`token::arrow`,
`tic/AbstrCalculator.cpp:1303-1351`). `->` in expressions *is* morphism composition.

### 3.5 Anatomy of unit-type identity — the two regimes (verified)

Domain unification and values unification deliberately test *different things*
(`tic/AbstrUnit.cpp:272-326` vs `:328-365`):

- **Domain role — nominal.** `UnifyDomain` = pointer identity → same ultimate item →
  equal DataController results of the two key expressions. Domains are index sets;
  equal counts must not make unrelated entity sets joinable.
- **Values role — structural.** `UnifyValues` = value class (`UM_AllowTypeDiff`
  relaxes) + metric equality + projection equality (null = dimensionless/absent).
  Values units are quantity dimensions; commensurability is exactly what `add`
  needs, and requiring same defining expression here would make every independently
  declared `km` incompatible with every other.

The asymmetry is *sound* because whenever a values unit is **used as** a domain — a
rel attribute's target in `lookup` — the check applied at that indexing site is the
nominal `UnifyDomain`, not `UnifyValues`. Corollary worth documenting for users:
`unit<float64> eur;` and `unit<float64> usd;` **unify as values units** (both carry
the neutral metric); branding the values role requires a metric
(`BaseUnit('eur', float64)`). Naming alone brands only the domain role.

Both regimes are engineered in one place, `UnitBase<V>::GetKeyExprImpl`
(`tic/Unit.cpp:108-176`), for config base units (no calculation rule) of separable
value types (`has_var_range_v` = fixed-size non-bit — all numerics and points,
`geo/ElemTraits.h:308-309`):

- **without SpatialReference**: key = `(BaseUnit (left '<fullName>' (UInt32 0)) (<VT>))`
  (`Unit.cpp:150-155`) — an expression whose *evaluation* is the empty string (neutral
  metric ⇒ values-compatible) but whose *syntax* embeds the unit's full name (distinct
  key ⇒ domain-distinct). This is §4.1's "branded, generative" made literal: nominal
  brand, structural commensurability, one term.
- **with SpatialReference**: key = `(BaseUnit "<SR>\xFF<DialogData>" (<VT>))`
  (`Unit.cpp:136-146`; `MG_CHECK` at `:139` bans 0xFF inside the SR itself). Evaluating
  it turns the whole string into a **metric base-unit name**: metric =
  `{ "<SR>\xFF<dd>" : 1 }`.
- **declared ranges are appended to the key** (`USF_HasConfigRange` →
  `GetRangeDataAsLispRef`, `Unit.cpp:174-175`): a *declared* range is part of the
  brand; a computed range is not — consistent with §4.1.

Component table:

| component | stored | propagates via | UnifyDomain | UnifyValues | in key expr |
|---|---|---|---|---|---|
| value class | UnitClass | — | yes | yes (unless UM_AllowTypeDiff) | head |
| defining expr | DC key | n/a | **yes — the test** | no | *is* the key |
| metric μ | `RangedUnit::m_Metric` — all separable V **incl. points** (`Unit.h:92-102`) | `SetProduct`/`SetQuotient` in unit creators | no | **yes** (AreEqual) | via BaseUnit terms |
| projection π | `GeoUnitAdapter::m_Projection` — point types only (`Unit.h:144-169`) | gridset etc.; composite-base chain | no | **yes** (AreEqual) | via gridset terms |
| SpatialReference σ | flag + side-assoc on the declaring unit (`AbstrUnit.cpp:377-384`); pre-0xFF of a mono-dimensional power-1 metric base name on derived units (`:403-417`) | rides the metric string | indirectly (key) | indirectly (metric string) | inside the BaseUnit string |
| background ref | `DialogData` property; post-0xFF fallback (`:386-401`) | rides the metric string | indirectly | indirectly — **presentation data in type identity** | inside the BaseUnit string |
| range | RangeData | — | declared: via key; computed: no | no | declared only |

The SR getter's "mono-dimensional, power 1" guard encodes a real semantics: a unit is
still *linearly a quantity of that coordinate base* — scalar multiples and coordinate
sums/differences keep power 1 and so keep the CRS; products and ratios (areas,
densities) drop to power ≠ 1 and lose it, correctly. The conflation cost: a
coordinate *difference* also keeps power 1, so a displacement's "metric" prints as
`EPSG:28992` rather than `m` — and a point unit cannot carry a physical metric and a
CRS simultaneously, because both compete for the same string slot.

Consumers of the machinery: scale-bar unit/label via GDAL-OGR
(`s_GetUnitlabeledScalePairFunc`, `stg/gdal/gdal_base.cpp:360-392`;
`shv/Controllers.cpp:129-131`, delegating through the projection's composite base,
`tic/Metric.cpp:421-424`); default background layer for map views
(`GetBackgroundReference` → `shv/GraphDataView.cpp:152`); SRS tagging on GDAL storage.

## 4. Corrections to the sketch (reducing surprise)

### 4.1 `range(a,b)` is a refinement, not the identity of a unit type

Two independent reasons:

- **Semantics.** Equal row counts are not semantic identity. If `range(0,10)` were the
  *type* of a unit, any two ten-element domains would silently unify, and unrelated
  same-size tables would join without complaint — the classic relational footgun.
- **Staging.** Ranges are frequently data-stage values (§2); a static checker cannot
  see them.

So: units are **branded, generative by defining expression** (applicative generativity —
same expression, same unit; different declaration, different unit), and `range(a,b)` is
a *refinement* on top. This is literally what `UnifyDomain` already implements
(`tic/AbstrUnit.cpp:272-326`): pointer identity → same ultimate item → equal
DataController results of the two key expressions — **no range comparison anywhere**.
The count-based escape exists only as the opt-in flag `UM_AllowAllEqualCount`
(`tic/AbstrUnit.h:60-68`) and must stay opt-in. The sketch as stated would have
*weakened* the existing discipline; the correction is to formalize what GeoDMS already
does, not to change it.

`UnifyValues` is the values-side counterpart: class + metric + projection equality
(`tic/AbstrUnit.cpp:328-365`), again without ranges.

### 4.2 Containers are dependent records; tables are the shared-domain special case

`container C { a: A; b: B }` as `struct { a: A; b: B }` is right but incomplete: the
normal GeoDMS idiom declares a member unit followed by attributes *over that unit*, so
later fields' types mention earlier fields. Containers are **telescopes** (dependent
records). The table type

```
unit<uint32> City : nrofrows = 42
{
    attribute<string>  name;
    attribute<float64> pop;
}
```

is then the Σ-type `Σ(U : DomainType) · { name : string[U]; pop : float64[U] }` — the
special case where the record shares a single domain, with the unit itself as the first
component. (Pleasant regularity: since parameters are `V[void]`, a container of
parameters is a one-row table over `void`.)

### 4.3 Exactly one implicit coercion: broadcast `V[void] ↪ V[D]`

The rule that makes literals and parameters usable in attribute expressions should be
*named and unique*, not folkloric. It is already mechanized three ways: a left-Void
domain always unifies (`tic/AbstrUnit.cpp:291`); `UM_AllowVoidRight` covers the right
side; binary operators pick the non-void operand's domain (`e1Void ? e2 : e1`,
`clc/dll/include/OperAttrBin.h:52-57`). Everything else stays explicit — `convert`/
`value(x, U)` for value coercions, lookup along declared rel attributes for domain
bridges. Signatures (§7) should display the broadcast rule explicitly.

### 4.4 Width subtyping for tables/records — and nothing else

A function parameter typed as "a City-table with a `pop : float64` column" accepts any
table over that domain with *at least* that column (structural match by field name and
type). This is what makes table-typed parameters practical. No other structural
subtyping exists: in particular no depth coercions, and no structural unification of
distinct domains (§4.1).

### 4.5 Applicative function semantics

`F(args)` applied twice to arguments with identical DC keys yields the **same result
item** — in particular the same result *units* — so downstream items from both call
sites are mutually compatible (OCaml's applicative functors are the precedent).

Nuance found during verification: the DC layer *already* bridges identical template
instantiations at the data level — two copied bodies whose substituted key expressions
intern to the same `LispRef` share DataControllers, and `UnifyDomain` equates their
units through key-expression comparison. What applicative *function application* adds
is: no per-call subtree copy (memory, meta-time, duplicated tree/UI entries), one check
instead of N, and sharing that is structural rather than emergent.

### 4.6 Scoping: lexical definition-scope, call-site isolation *(revised 2026-07-13)*

**Revision (user decision 2026-07-13).** The original decision (strict args-plus-
explicit-imports) is relaxed to the general language rule: *in each expression of a
definition, identifiers resolve to the items that are visible from the point of
definition*. A function body therefore sees, in order:

1. its formal parameters,
2. its own local items (nearest enclosing body scope first),
3. namespaces it explicitly imports via `using` (unchanged, now additive),
4. **the lexical scope of its definition** — the definition parent and its ancestors,
   with their `using` directives, exactly as any ordinary calculation rule at that
   spot would see them,
5. the auto-imported prelude as the implicit outermost namespace (§8.4).

What does NOT change: **call-site isolation**. A body never sees the call site; the
only channels from the caller are the argument expressions, which are resolved in the
*caller's* scope (`ArgCalc` grandparent search context). This remains the crucial
difference from templates, whose instances retain an unshadowed call-site fallback
through the UsingCache constructor's implicit `AddParent` (see history below).

Rationale for the revision: uniformity — a sibling function `compose` or a shared
parameter `factor` is visible to every ordinary expression written at the same spot,
and requiring `using = /hof` on a function to see its own sibling was an anomaly. It
also makes closure capture (§5.10) *just lexical scoping applied to nested functions*:
a nested function's body references its enclosing function's parameters because those
are visible at its definition point. The costs, accepted: the closure is no longer
enumerable from the function header alone, and definition-time checking validates
against the definition environment rather than in complete isolation.

*History (pre-revision, implemented in 20.9.0 and now superseded):* template instance
roots receive an injected `using` namespace pointing at the template's definition
parent (`TreeItem::Copy`), and name resolution delegates-and-stops at any level owning
a `UsingCache` — but the UsingCache constructor implicitly adds the context's tree
parent (`AddParent`), so template bodies are definition-scoped *with call-site
fallback*, not hygienic. Functions went strictly further: `RemoveParentUsing` + forced
cache = params, locals and explicit imports only. The revision keeps the
`RemoveParentUsing` mechanism on *instance roots* (call-site isolation) and injects
the definition parent as a namespace (as template copies do), while inline reduction
resolves externals through the definition-parent chain.

### 4.7 `attribute<float64>` vs `attribute<meter>`

Today `<…>` accepts either a value-type keyword or an item path, with folklore
semantics. Make it official: a **value-type spec** means an implicitly ∀-quantified
values-unit variable constrained to that value type; an **item spec** means exactly
that unit (a singleton type). Both stay; in function signatures the distinction
determines whether metric checking is deferred (§11) or exact.

### 4.8 Totality

No general recursion. Iteration stays with bounded combinators (the existing
`iterate`/`loop` family); the meta stage remains terminating DAG construction, and
recursive function application is rejected with a cycle guard (§10, R9).

### 4.9 CRS is a type component; background-layer refs are not

Diagnosis (§3.5): `SpatialReference` and the background-layer reference (`DialogData`)
are multiplexed into one 0xFF-separated string posing as a metric base-unit name. The
construction is *effective* — it rides the existing metric propagation for free, makes
CRS mismatches type errors, and enters applicative identity — but obfuscated, split
across two storage regimes (side-assoc on the declaring unit, string-parsing on derived
units), and it **over-identifies**: two units differing only in background layer fail
`UnifyValues`, i.e. GUI presentation participates in type identity.

Corrections, keeping CRS in type identification:

1. **`crs(σ)` becomes a first-class refinement** (grammar §3.2). Storage generalizes
   the existing flag + side-assoc; `UnifyValues` gains an explicit third check with its
   own diagnostic ("incompatible SpatialReferences"); propagation gets the rule that is
   today implicit in the getter: *σ survives exactly the power-1 single-base
   combinations* (scalar multiples, coordinate sums/differences) and is dropped by
   products/ratios — a `CrsOperation` next to `MetricOperation` in the unit-level
   operators (`clc/dll/include/OperUnit.h:95-121`).
2. **Structured key term**: `(BaseUnit (SRef "EPSG:28992") (<VT>))` instead of an
   embedded `"<SR>\xFF<dd>"` string — CRS stays part of the defining expression, no
   in-band separator, no `MG_CHECK` on forbidden bytes. Key-head versioning per R11.
3. **Background refs leave type identity.** `DialogData` stays a plain property on the
   declaring base unit, resolved at *display* time by walking the projection
   composite-base chain (the lone consumer is `shv/GraphDataView.cpp:152`; the
   scale-bar path already walks that chain, `tic/Metric.cpp:421-424`); an SR→background
   registry covers base units created purely by expression.
4. **Payoff beyond hygiene**: with σ separated from μ, a coordinate base unit can carry
   *both* `crs = EPSG:28992` *and* `metric = {m:1}` — coordinate differences then
   derive an honest meter metric while the CRS propagates independently. Dimensional
   analysis over coordinates becomes correct instead of printing EPSG tokens as if they
   were physical units (§3.5's conflation cost).

Behavioral note: units differing only in background ref *start unifying* — that is the
intended fix, but it is config-visible; regression-test it alongside R13, and keep the
CRS-mismatch-is-an-error tests as the guard that the strictness that matters is
preserved.

## 5. The `function` construct

### 5.1 Surface syntax *(as implemented in 20.9.0)*

Parameters are ordinary declarations forming a telescope (later parameters may
reference earlier ones). The result specification follows `->`: an optional name, a
type, and a **result expression** after `:=` that relates the result to the body. The
body block is optional; three forms:

```
// form a: result expression designates a body item by name
function CongestionRatio(
    unit<uint32>       Road {
        attribute<float64> flow;
        attribute<float64> cap;
    }
)
-> attribute<float64> (Road) := Result
{
    attribute<float64> raw    (Road) := Road/flow / Road/cap;
    attribute<float64> result (Road) := min_elem(raw, 1.0);
}

// form b: the result expression IS the result's calculation rule, over body items
function CongestionRatio2(unit<uint32> Rd; attribute<float64> flow (Rd); attribute<float64> cap (Rd))
-> attribute<float64> (Rd) := min_elem(raw, 1.0);
{
    attribute<float64> raw (Rd) := flow / cap;
}

// form c: expression-only, no body block
function CongestionRatio3(unit<uint32> Rd { attribute<float64> flow; attribute<float64> cap; })
-> attribute<float64> (Rd) := min_elem(Rd/flow / Rd/cap, 1.0);

// name:type declaration style, composite type by example, named result
network_links: unit<uint32> { nodeset: unit<uint32>; F1, F2: attribute<nodeset>; }
function connectedness( nw: network_links )
-> link_counts: attribute<uint32> (nw/nodeset) := pcount(nw/F1) + pcount(nw/F2);

// explicit imports (strict scope otherwise)
function Capped(unit<uint32> Rd; attribute<float64> x (Rd)), using = shared
-> attribute<float64> (Rd) := min_elem(x, limit);
```

P1 makes `F(args)` an expression of the result type, nestable (§10). *Historical
note:* in the first implementation the holder type selected the semantics — a
container holder silently instantiated (`container cr := CongestionRatio(Road);` +
`cr/result`), a typed holder inlined. That holder-driven rule is superseded by the
explicit `apply`/`instantiate`/container-literal forms of §5.9.

Notes on the notation:

- **`->` introduces the result specification**; it does not collide with the expression
  operator `->` (relational dereference): declarations and expressions are separate
  Spirit grammars.
- The result expression after `:=` either *designates* an existing body item (a plain
  name; matched case-exactly, else case-insensitively-unique) or becomes the
  calculation rule of a synthesized result item (named per the spec, default `result`).
- **Structured parameters**: a `unit` parameter may carry a member block declaring the
  attributes the argument table must provide; body references go through the parameter
  (`Road/flow`). Members are declared interface — at instantiation the parameter binds
  by reference to the actual argument (case-parameter bodies are not copied).
- **Composite types by example**: a parameter may be typed by an item reference
  (`nw: network_links`); P0 binds it as a plain item (class/member checking follows in
  P2 with the signature machinery).
- **name:type declarations** (`Road: unit<uint32>`, `F1, F2: attribute<nodeset>;`) are
  accepted everywhere item declarations are; multi-name declarations share one
  calculation rule; a block may not follow a multi-name declaration.
- The familiar "`:=`" surface form is untouched — it is not one token but `:` followed
  by `directExpr = '=' >> expr` (`stx/dll/src/ConfigParse.cpp:134-137, 220`).
- Imports use the `using` clause after the parameter list; import paths are resolved at
  the definition site and frozen to absolute paths at instantiation.
- Function names are validated at parse time against operator names and RewriteExpr.lsp
  rule heads (§8.2).

### 5.2 What `function` adds over `template` (verified deltas)

| aspect | template today | function |
|---|---|---|
| parameters | untyped, marker-less: "first N subitems", N = however many arguments the call passes (`TreeItem::Copy`, `tic/TreeItem.cpp:2119-2120`) — surplus arguments silently consume body items | declared, named, typed telescope; arity checked at the call |
| checking | body never evaluated until instantiation (`TSF_InTemplate` suppresses calculators, `tic/TreeItem.cpp:854-857`); every call site re-checks independently | typechecked **once at definition** against the parameter types |
| composability | barred as sub-expression (`tic/AbstrCalculator.cpp:1375-1396` throws) | `F(args)` is an expression of type R, nestable |
| instantiation | deep tree copy into the holder (`InstantiateTemplate` → `CopyTreeContext`, `tic/AbstrCalculator.cpp:996-1015`) | applicative DC keyed by (F, argument keys); no copy — or full inlining for `inline` functions (§8.3) |
| scoping | definition scope takes precedence via injected `using`, but the implicit parent namespace keeps call-site names reachable as fallback (§4.6) | lexical definition scope (§4.6 rev. 2026-07-13): formals + locals + explicit `using` imports + the definition-parent chain; call-site isolation total |
| result | conventionally some subitem; untyped | designated `result` with checked annotation; the item has type `(A_i) -> R` |
| overloading | none (one body per name) | typed variants resolved by argument types (§5.7) |

### 5.3 Typing rules (sketch)

Let `Γ` be the definition context (imports only). Judgments:

```
Γ ⊢ U : unit<vt>                        unit well-formed
Γ ⊢ D domain                            D : unit<vt>, vt domainable
Γ ⊢ x : V[D]^vc                         attribute
unifyD(D₁,D₂), unifyV(V₁,V₂)            equality judgments — DEFINED AS the implemented
                                        algorithms (AbstrUnit.cpp:272-326, 328-365)

broadcast (the single subsumption):
    Γ ⊢ x : V[void]   ⟹   Γ ⊢ x : V[D]        for any Γ ⊢ D domain

record width subtyping:
    { n_i : T_i }_{i∈I}  <:  { n_j : T_j }_{j∈J}       when J ⊆ I

application (telescope substitution):
    Γ ⊢ F : (a₁:T₁; …; a_n:T_n) -> R
    Δ ⊢ e_i : T_i[a₁↦e₁, …, a_{i-1}↦e_{i-1}]           args typed in CALLER context Δ
    ─────────────────────────────────────────
    Δ ⊢ F(e₁, …, e_n) : R[a_i ↦ e_i]
```

Unit polymorphism needs no machinery: a parameter `unit<uint32> D` *is* a unit
variable, instantiated by the argument. Value-type polymorphism (∀V:class) is the only
genuine scheme layer and is deferred to P2, mapping directly onto the existing typelist
classes and the per-value-type operator instantiation mechanism
(`BinaryInstantiation<TL, MetaFunc>` / `tl_oper::inst_tuple_templ`,
`clc/dll/src/OperAttrBin.cpp:406-504`).

### 5.4 Unit- and table-valued results

Functions returning **units** are essential (select-like abstractions). Precedent
exists: `unique` is a cached DC whose result root *is* a unit with a `values` subitem
(`clc/dll/src/Unique.cpp:330-360`), and `UnifyDomain`'s key-expression branch makes
attributes over the same function-call unit compatible across call sites. Typical
shapes:

```
function SelectWith(unit<uint32> D; attribute<bool> cond (D))
:   unit<uint32> { attribute<D> org_rel; }              // Σ-result: domain + witness
{
    unit<uint32> result := select_with_org_rel(cond);
}
```

The Σ-typed result (a fresh domain plus its `org_rel` morphism back to D) names a
pattern GeoDMS operators already follow; the type system just makes it a first-class
citizen. One syntactic constraint carries over from today: a *declared* attribute's
domain must be an item reference, so unit results are landed in a named unit
(`unit<uint32> S := SelectWith(City, cond);`) before use as a declared domain — exactly
as with `unique` now.

### 5.5 Higher-order functions (P3) — core IMPLEMENTED in 20.9.0

**Implementation status: function-valued parameters work** through the inline
reduction path. A parameter declared `f: function` binds a function reference passed
as an argument (`ApplyTwice(Road, lib/Halve, Road/flow)`); the body applies it
(`f(D, f(D, x))`) or passes it onward to another function; a function reachable
through the strict scope (import) can likewise be passed from inside a body. Misuse
as a value (`f + 1.0`) and member access through a function-valued parameter are
rejected with dedicated diagnostics. Bindings are meta-stage only — by DC time every
application is reduced away, so the erasure guarantee (§5.6) holds. Verified by
`testcases/fn_test_p3.dms` (apply-twice, two-parameter composition, body-level
pass-through) and a value-misuse negative test. Not yet: declared function *types*
(signatures `(T)->R` on parameters — currently any function binds, arity-checked at
application), lambdas/partial application, and the typed `map`/`filter` container
combinators below.

Function-typed parameters and function-valued expressions (the full design):

```
function apply_twice(
    unit<uint32> D;
    function f : (attribute<float64> (D)) -> attribute<float64> (D);
    attribute<float64> x (D))
:   attribute<float64> (D)
{
    attribute<float64> result (D) := f(f(x));
}
```

The flagship application is a typed replacement for the `for_each_XX` suffix zoo
(verified: a bitmask suffix language `n/e/d/v/x/l/a/t/s/c/u…` parsed by `ScanFirstArg`,
`clc/dll/src/ForEach.cpp:366-406`, generating per-row subitems + property setting under
a `CreateCacheRoot` holder):

```
map : (f : (T) -> R;  xs : { n_i : T }) -> { n_i : R }
```

The suffix combinatorics become ordinary typed arguments; the parallel name/expr/prop
arrays become a function body. Untyped string metaprogramming (leading-`=` eval
indirection, `MustEvaluate`) remains available outside functions for the residual
cases, but most uses become typed applications.

### 5.6 Staging and erasure

All function machinery — application, closures, β-reduction — happens at the meta
stage on `LispRef` terms. By the time DataControllers are created, every `lambda`/
application has been eliminated; the reduced network is first-order operator
applications on concrete units. Enforce this with a post-condition: no `IsVar()` leaves
or lambda heads may reach `GetOrCreateDataController`. The tiled data engine is
untouched by the entire feature.

### 5.7 Overloading and result-type derivation

Built-in operators already overload: one `AbstrOperGroup` per name holds many
`Operator`s (typically one per value type), selected by argument classes
(`FindOper`/`IsDerivedFrom`, `tic/OperGroups.cpp:355-431`). RewriteExpr.lsp rules also
dispatch on argument *form* — `collect_by_cond` has one rule per `select_*` flavor,
`pow` has per-exponent-literal rules. User functions need the same power, typed.

**Two mechanisms, preferring the first:**

1. **Class-constrained parameters** (P2) for *uniform* polymorphism — one body, one
   derivation scheme:

   ```
   function sqr<V: numerics>(attribute<V> x (D)) { attribute<...> result (D) := x * x; }
   ```

2. **Variants** for genuinely type-dependent behavior — **IMPLEMENTED in 20.9.0**. One
   function item contains named variant blocks, each a full function (params, `->`
   result, `:=` expression, optional body):

   ```
   function describe
   {
       variant asFloat(attribute<float64> x (D)) -> attribute<float64> (D) := x * 100.0;
       variant asInt  (attribute<int32>   x (D)) -> attribute<int32>   (D) := x + x;
   }
   attribute<float64> df (D) := describe(D/f);   // dispatches to asFloat
   attribute<int32>   di (D) := describe(D/i);   // dispatches to asInt
   ```

   Variants live *inside one item* deliberately: TreeItem containers require unique
   subitem names, so same-name sibling functions are not representable — and keeping
   the overload set in one place makes it checkable as a whole. This mirrors how one
   OperatorGroup holds many Operators.

   *Implementation:* `function name { variant v(...) ... }` parses via a `variantSet`
   grammar branch (`functionDecl = FUNCTION identifier (functionBody | variantSet)`);
   the set item is flagged (`TreeItem_SetFunctionVariantSet`) and each variant is an
   ordinary function sub-item. At a call, `ResolveVariant` (`tic/AbstrCalculator.cpp`)
   materialises each argument's value class and selects the variant whose parameter
   value classes all match (an untyped/wildcard parameter position matches anything);
   the chosen variant is then reduced like a normal function. Ambiguous matches and
   no-match both error with the variant list. Verified by `testcases/fn_test_variant.dms`
   (float64→asFloat, int32→asInt) and a no-match negative (string).

   **v2 implemented (2026-07-14): specificity ordering + definition-time
   disjointness.** Each variant parameter denotes an *acceptance set* over the closed
   value-class universe (`std::bitset<VT_Count>`): a concrete script-named class → a
   singleton; a generic values-variable → its constraint's subset (so matching is now
   constraint-aware: a `<V: floats>` variant no longer swallows integer arguments); a
   plain/composite/function-typed/item-spec position → everything (a "soft"
   wildcard). Dispatch (`ResolveVariant`) collects all matching variants and picks
   the unique most specific one by per-parameter subset comparison
   (`TreeItem_CompareVariantSpecificity`), so `exact float32 > <V: floats> >
   <V: numerics>` layers dispatch as expected (`testcases/fn_test_variant2.dms`).
   At definition (`OnVariantSetEnd` → `TreeItem_CheckVariantSetDisjointness`,
   token-based and parse-safe), two same-arity variants whose acceptance sets overlap
   must be strictly specificity-ordered; identical or incomparable overlapping
   coverage — e.g. `(numerics, float64)` vs `(float64, numerics)` — is rejected
   immediately: *"variants 'a' and 'b' overlap without one being more specific than
   the other"* (`fn_test_variant2_neg.dms`). Pairs involving a soft position are left
   to the call-time ambiguity guard. Remaining v1 limits: variant-set calls are
   always inline (data/unit holder) and variant `using` imports are per-variant.

**Resolution.** Name resolution finds the function item by the normal scope rules
(nearest declaring scope; no cross-scope merging of overload sets). Within the item,
select by argument TypeSpecs with a *specificity order*: exact unit item > concrete
value type > value-type class > unconstrained. Two rules that deliberately differ from
`FindOper` (which returns the first registered full match — registration-order
semantics would be a surprise in config space):

- **Definition-time disjointness**: variant applicability domains must be pairwise
  disjoint or strictly ordered by specificity. This is decidable and cheap — 𝕍 is
  closed and small, so class constraints enumerate finitely.
- **Ambiguity is an error**, at definition where detectable, else at the call.

User functions can neither shadow nor extend built-in operator groups (definition-time
error on name collision; dispatch order is: engine canonicalization → registered
operator groups → nearest-scope user function/template). Extending built-in groups with
user variants "when no built-in matches" is explicitly deferred: it would make config
meaning change silently whenever the engine gains an overload.

**Result-type derivation.** A function's result type is **derived from the body**, not
merely declared — this is what makes typed functions strictly stronger than rewrite
rules ("argument-type-specific result type derivation"):

- *kinds/classes* of R: derived at definition, once per value-type instantiation of
  class variables (finite enumeration over the typelist — the config-level analogue of
  `BinaryInstantiation`), and cached;
- *exact units, including metrics*, of R: derived per application by the reduced
  network's own `MakeResult` — e.g. `sqr(x)` with `x : U[D]` yields `U²[D]` because
  `mul`'s unit creator computes the product metric (`clc/dll/include/OperUnit.h:95-220`);
  no declaration could express this better than the derivation does;
- the optional result annotation is a *check* against the derivation, not the source of
  truth; it may mention class variables and telescope parameters;
- per-variant, the declared telescope + derived result form the variant's `fun-sig`
  TypeSpec (§7), which is what resolution, diagnostics, and the signature browser use.

Applicative keys name the *resolved* variant — `(applyF "/path/F" <variantTok>
argKey_1 …)` — so keys are self-contained and resolution changes cannot silently
re-route cached results.

### 5.8 Type aliases and declared function signatures — IMPLEMENTED in 20.9.0

`alias = type;` declares a **type alias**, deliberately distinguished from
`name : type;` (which declares an *item* of that type):

```
flt      = attribute<float64>;                 // plain type alias
dom      = unit<uint32>;
dom2     = dom;                                // alias of alias / type by example
unary_fn = (unit<uint32> D; attribute<float64> v (D)) -> attribute<float64> (D);

x1: flt (Road) := Road/flow * 1.0;             // aliases usable wherever types are
D2: dom2 : nrofrows = 2;

function ApplyTwiceT(unit<uint32> D; f: unary_fn; attribute<float64> x (D))
-> attribute<float64> (D) := f(D, f(D, x));    // f's bindings are CHECKED against unary_fn
```

Semantics and implementation:

- An alias creates a hidden, inert **exemplar item** of the aliased type in the tree
  (`SetIsTemplate` inertness), so aliases scope lexically, survive `#include`
  boundaries, and unify with the existing composite-type-by-example mechanism: *any*
  previously declared item can serve as a type (`nw: network_links` now clones the
  exemplar's class — unit refs re-resolve at the use site, which is exactly the
  telescope behavior wanted in signatures).
- Plain-type aliases **expand at parse time** (`ConfigProd::ResolveTypeRef`:
  declared-before-use, parent-chain resolution, no using-directives) — no runtime
  representation needed.
- A **function-signature alias** `(params) -> resultType;` becomes a signature-only
  function item (no result expression, no body; applying one is an error). A parameter
  declared with it (`f: unary_fn`) records the exemplar in the function spec, and
  every binding is checked at application time (`CheckFunctionSignature` in
  `tic/AbstrCalculator.cpp`): arity, per-parameter item classes (a plain-item
  signature position is a wildcard), and result class. Unit *relationships* between
  signature positions (the dependent part) are not yet compared — that is P2 unifier
  territory.
- Verified by `testcases/fn_test_sig.dms` (aliases, alias-of-alias, alias with explicit
  domain, signature-checked higher-order application) and an arity-mismatch negative
  test with a dedicated diagnostic.

### 5.9 Application vs instantiation: `apply`, `instantiate`, container literals *(implemented 2026-07-12)*

**Implementation status (Phase A).** The `apply`/`instantiate` keywords and the
no-holder-magic rule are implemented. A bare function call is always its result value
(inline); binding one to a *container* holder is an error pointing at `instantiate`.
`instantiate X(args)` copy-instantiates the body into the holder (function or template);
bare template calls are unchanged. `apply X(args)` = the value: for a function it is the
bare call, for a template it errors "not yet implemented" (the context-keyed cache
instantiation of decision 3 is the remaining piece). The keywords are contextual —
matched only as a complete word before a call, so `ApplyBin(x)` (identifier) and
`apply(x)` (a call of an item named `apply`) are unaffected (`stx/dll/src/ExprParse.h`:
`lexeme_d[strlit >> epsilon_p(space_p)] >> functionCallReq`; marker heads
`apply_item`/`instantiate_item` dispatched in `tic/AbstrCalculator.cpp`).

**Implementation status (Phase B — container literals).** `domain { m: e; … }` and
`{ m: e; … }` are accepted **in function-argument position** and are destructured at
β-reduction — the literal materializes **no item**. Each member's value and the domain
resolve in the caller scope, with `.` inside a member rebound to the literal's domain
unit; a parameter bound to a literal reduces `P` to the domain and `P/member` to the
member value (`stx/dll/src/ExprParse.h` `argument`/`memberList`/`containerMember` →
`(container_literal <domain|no_domain> (member name value)…)`; built by
`ProdContainerLiteral`/`ProdContainerMember`; consumed in `tic/AbstrCalculator.cpp` via
`ContainerLiteralArg` + `BuildContainerLiteral` + `ReplaceDot`, with member-access
resolution in `ResolveBodySymbol` and a visit-only supplier walk in
`SubstituteExpr_impl`). Both `name: value` and `name := value` are accepted. Confining
literals to argument position is deliberate: it avoids any clash with an item-body
`{ … }` following a whole calculation rule (`directExpr` runs the same expression grammar
to find the rule's extent). Verified: `CongestionRatio( range(0,10) { flow:
float64(id(.)); cap: 2.0; } )` and `examples/function.dms` `inline_load`.

**Implementation status (Phase C — `apply T(args)` for templates, 2026-07-13).**
Implemented as **β-reduction of the template's CI-unique `result` sub-item**: the
provided arguments bind to T's first N sub-items (the template binding rule), and the
`result` member's expression reduces through the same `FunctionApplication` machinery
functions use — nearest-scope within the template, then the template's own scope
(definition scope, ancestors included). Clear errors when `result` is absent/ambiguous
or has no calculation rule, or when more arguments than sub-items are provided.
`examples/function.dms` `evening2` demonstrates the adoption lever.

*Mechanism note — deviation from decision 3's sketch, same semantics.* The
cache-copy-instantiation route (a PhaseContainer-style operator around
`CreateCacheRoot` + `InstantiateTemplate`) was built first and hit two structural
walls, both verified empirically: members of a rootless cache root are **passors**
(`MakeCalculator` refuses them by design), and expr-string resolution inside a
parentless cache tree does not treat the instance as a config-like scope (keys came
out as name-trees or unresolved symbols; declared holder domains failed
`UnifyDomain` with "different CheckedKeyExpr"). The β-reduction route sidesteps all
of it and is *semantically equivalent for the cases the cache route could have
served*: a parentless cache instance has no call-site fallback either, so resolution
is definition-scoped in both designs. Identity comes out finer-grained than the
planned context-string discriminator: two applies merge exactly iff their substituted
keys coincide — which, with definition-scope resolution, is precisely when they
denote the same computation. Restriction inherited from the reducer: the body slice
reachable from `result` must be expression-only (no storage/dot-relative/absolute
refs), with clear errors otherwise — same contract as functions.

**Deferred (redundant):** the top-level lone-call sugar `{ X(args) }`
(equivalent to the already-working `instantiate X(args)`); general sub-expression
literals (e.g. `2.0 * X{…}`); a value-type variable inferred *through* a literal member
(a type var used only inside a structured parameter is already unconstrained after the
member-generic fix, so the generic `CongestionRatio<V>` accepts a literal); nested
`apply T(…)` in sub-expressions (decision 2: root-only; convert to a function for
inline composition).

The first implementation let the *holder type* select the semantics of a call
(container holder → copy-instantiate the body; typed holder → inline the result).
That rule is replaced: **the call site says what you get**, independently of the
holder. Two optional, kind-independent prefix keywords plus a container-literal form:

```
// functions: a call IS a value (default); instantiation is explicit
attribute<float64> morning  (Road) := CongestionRatio(Road);            // result value
attribute<float64> morning2 (Road) := apply CongestionRatio(Road);      // same (explicit)
attribute<float64> evening  (Road) := 2.0 * CongestionRatio(Road);      // nestable
container morning_calc_steps  := instantiate CongestionRatio(Road);     // all steps as items
container morning_calc_stepsB := { CongestionRatio(Road) };             // same (literal form)

// templates: instantiation is the (unchanged) default; the value form is explicit
container steps2 := CongestionRatioT(Road);                             // as always
container steps3 := instantiate CongestionRatioT(Road);                 // same (explicit)
container steps4 := { CongestionRatioT(Road); }                         // same (literal form)
attribute<float64> m3 (Road) := apply CongestionRatioT(Road);           // its 'result' sub-item
```

Semantics matrix:

| form | function F | template T |
|---|---|---|
| bare `X(args)` | result value (inline reduction) | instantiate into holder (**unchanged**) |
| `apply X(args)` | result value (synonym of bare) | instantiate (hidden) + follow the `result` sub-item |
| `instantiate X(args)` | copy-instantiate all body items into the holder | instantiate (synonym of bare) |
| `{ X(args) }` | = `instantiate` | = `instantiate` |

Design rationale: the asymmetric *defaults* (function→value, template→instantiate)
are the price of template backward compatibility; the explicit keywords neutralize it —
`apply X(…)` / `instantiate X(…)` mean the same regardless of X's kind, so a template
can be refactored into a function (or back) without touching call sites that use the
explicit forms. `apply T(…)` is the adoption lever: existing template libraries become
usable in value position without migration.

**Container literals are expressions** *(refined 2026-07-12)*. `{ … }` is an anonymous
container **value**, usable anywhere a value is expected — including as a function
argument, which is the point:

```
CongestionRatio( range(0,10) { flow: float64(ID(.)); cap: 2.0; } )  // literal as argument
```

So `{ … }` joins the expression grammar (an expression can now start with `{`; there is
no ambiguity because a bare `{` in value position is always a literal). Forms:

- `{ X(args) }` — exactly one lone call: the instantiation form (all steps as members);
- `{ flow: float64(ID(.)); cap: 2.0; }` or `{ a := F(x); b := 2.0 * G(y); }` — named
  members: an anonymous container whose members are *type-derived*; `a`/`flow` get the
  **result value** of their rhs (consistent with the bare-call rule). A member may be
  written `name: type-expr` (a domain-carrying attribute, as in the argument example) or
  `name := expr`. Prefix keywords compose per member (`{ a := instantiate F(x); }`).
- A literal is *either* one lone call *or* a list of named members — mixing, or multiple
  lone calls, is an error (two instantiations would collide member names).
- Trailing semicolons optional. This is the composite-value / record mechanism (§3.2):
  a literal argument like `range(0,10) { flow: …; cap: … }` builds an anonymous table on
  the fly to satisfy a structured parameter, without a named container.

Decisions *(confirmed 2026-07-12)*:

1. **Bare `container x := F(args)` is a hard error** with a fix-it message ("use
   `instantiate F(…)` or `{ F(…) }` for the calculation steps") — no holder-driven
   magic. Breaking vs v20.9.0; only this repo's tests and `examples/function.dms` are
   affected.
2. **`apply T(args)` is root-only** (the whole calculation rule), like template
   instantiation today. Nested `2.0 * apply T(R)` is *not* allowed: two identical
   nested template applies would (if allowed) instantiate once as a shared cache item,
   but templates can capture call-site-visible names, so identifying them is unsound —
   keep it disallowed for simplicity. Error text: "convert to a function for inline
   composition". `apply F(…)` composes anywhere (a function call is already a value).
3. **`apply T(args)` = cache-instantiate T, take its `result` sub-item.** T(args) is
   instantiated as a **cache item** (as `PhaseContainer` and function applications
   already are), and the holder follows the case-insensitively-unique sub-item named
   `result` (reuse the function-result designation; error if absent). *Identity keys on
   the instantiation context as an extra parameter* — because a template body may read
   names visible (and different) at each call site, two `apply T(args)` with identical
   arguments at different sites must NOT be merged. So template result-value semantics
   are "function application with the call-site context as an implicit extra argument",
   which both preserves template scoping and gives correct, non-shared instances. (This
   supersedes the earlier hidden-endogenous-holder idea, which would have made the value
   depend on hidden sub-item results.)
4. **Contextual keywords**: `apply`/`instantiate` recognized only in rule-prefix
   position followed by `identifier(…)`; `apply(x)` stays a call of an item named
   `apply` (verify no operator-group collision, as `CreateFunction` does).
5. `apply` on a partial application or signature-only alias: error. `instantiate` on a
   variant set: resolve the variant from the arguments first, then copy that variant.

Note on identity: a **function** application has applicative identity — args only, so
`instantiate F(args)` twice yields two instances with distinct tree items but expressions
that substitute to identical DC keys (data shared, units unify; materialization is a
presentation choice). A **template** application via `apply`/`instantiate` is
context-dependent — the call site is part of the key (decision 3), matching the existing
template rule that domain identity takes the full instantiation path. Explicitly
instantiated functions inherit this full-path domain identity, which is acceptable.

Implementation sketch: `directExpr` gains the two contextual prefix keywords; the
**expression grammar** (`ExprParse.h`) gains the container-literal `{ … }` production
(so literals nest as arguments); `SubstituteExpr`'s function branch drops the
holder-class test in favor of the explicit marker; `apply T(…)` = a `MetaFuncCurry`
variant that cache-instantiates `T` keyed by the call-site context (decision 3) and sets
the holder's follow-source to the instance's `result`; a container literal reduces to an
anonymous container value (a `CreateCacheRoot` populated from its members, à la
`InstantiateMap`) usable as an argument. Migration updates `testcases/fn_test*.dms` and
`examples/function.dms` (bare `container := F(args)` becomes `instantiate F(args)`).

GUI inspection of inline applications (no materialized steps): the Value-Info /
explain-value trace already walks the reduced DC graph per operator node; an
"expand steps" detail action (re-run the reduction in instantiating mode on demand)
and showing the function body source with arguments substituted are the candidate
additions.

### 5.10 Function-valued results, closures, applied call results *(Stages 1+2 implemented 2026-07-14)*

**Implementation status (Stage 1).** All three pieces below are implemented and
verified (`testcases/fn_test_closure.dms`, `examples/function.dms` `hof2`): grammar =
call-suffix chain in `functionCallOrIdentifier` + `apply_value` marker emission in
`ProdFunctionCall` (list-valued head) + `-> function` result type with implicit
designation of a nested function named `result` (`':=' now grammar-optional, enforced
semantically for data results); reducer = `ReduceValue`/`ReduceMergedValue` return a
`CallArg` (data key OR closure binding), closures = `ClosureEnv` {enclosing function,
bound args by value, `next` chain} consulted by body symbol/head/arg resolution before
imports and definition scope, `apply_value` dispatched at all five expression
positions (root, sub-expression, body-data, body-arg, caller-arg) plus the
definition-time checker. The recursion guard now keys on (function, environment):
distinct closures of one nested function may stack in a single reduction chain
(`compose(compose(double, inc), inc)(x)` works); genuine self-application still
errors. Retired along the way: the dead `(result _T)` unwrap rule in RewriteExpr.lsp
(no config calls `result(…)` in expression position; the head collided with nested
`result` functions). v1 limitation: a closure captures the enclosing application's
*parameters*; referencing an enclosing function's *locals* from a nested body errors
("reference to (part of) a template or function") — lift on demand.

Target (user sketch, typing layer elided to Stage 2):

```
container hof2
{
	function compose(f: function; g: function) -> function
	{
		function result<V: numerics>(attribute<V> x) -> attribute<V> := f(g(x));
	}

	function pow4<V: numerics>(attribute<V> x) -> attribute<V> := compose(sqr, sqr)(x);
}
```

Three pieces:

1. **Function-valued results.** A function may declare `-> function` and designate a
   *nested function* named `result` as its result. Reducing a call to such a function
   yields a **function value**, not a data key.
2. **Closures = lexical scoping applied to nested functions (§4.6).** The nested
   `result` references `f` and `g` because they are visible at its definition point —
   the enclosing function's parameters. Operationally the returned value is the nested
   function item plus the enclosing application's parameter environment, captured **by
   value** (the already-substituted `CallArg`s: keys, items, or bindings). Because the
   captured environment consists of concrete interned keys — never unresolved symbols —
   capture is hygienic by construction; the alpha-renaming machinery feared in R10 is
   not needed on this path. Identity remains purely applicative: two closures from
   identical calls carry identical environments and reduce any application to identical
   keys.
3. **Applied call results.** The call grammar chains: `identifier (args)* ` — each
   further `(args)` applies the *result* of the previous call. Since every expression
   head must remain a plain symbol (a FuncDC/rewriter invariant), the production emits
   a marker form rather than a list-headed call: `compose(sqr, sqr)(x)` parses to
   `(apply_value (compose sqr sqr) x)` (pattern of the §5.9 `apply_item` markers). The
   reducer dispatches `apply_value` by reducing the inner expression to a function
   value — a closure, a plain function reference, or a partial application — then
   binding the outer arguments and beta-reducing. NO over-application: extra arguments
   to a call are an arity error, as before; application of a result is always written
   explicitly.

Placement rules: a function value can be (a) applied via `(args)`, (b) passed as an
argument, or (c) returned as a result. It cannot be bound to a data item or holder —
same rule as partial applications today. `pow4(a)` reduces through the closure to
`(mul (mul aK aK) (mul aK aK))` — key identity with `pow(a, 4)` preserved end-to-end.

**Stage 2 implemented (2026-07-14).** The typed form runs
(`examples/function.dms` `hof2`, `testcases/fn_test_gsig.dms`, `fn_test_domvar.dms`):

- **Generic signature aliases**: `nuf = function<V: numerics, D: domains>
  (attribute<V> (D)) -> attribute<V> (D);` — the `function` keyword and type-params
  clause on the alias form, and anonymous parameters (a positional name `_1, _2, …`
  is synthesized). Pitfall re-found: the alias name must be captured on its own
  identifier action — the type-params clause overwrites `m_strIdentifierID` (the same
  bug P2 fixed for `functionDecl`).
- **Type application**: `f: nuf<V, D>` on parameters and `-> nuf<V, D>` as result
  type. Arguments must name active type variables (typo check); the binding is
  documentation-level in v1 — signature checks stay kind-level (arity, item class,
  value composition, and the alpha-invariant positional domain/values relationships,
  with signature-side *generic-variable* positions acting as wildcards). A
  signature-typed result implies a function-valued result (`-> function` semantics).
- **Domain type-variables**: a parameter domain `(D)` with `D: domains` in the
  type-params clause binds D to the argument's domain unit at each application;
  parameters sharing D must agree (`UnifyDomain`), with a clear error naming both
  parameters; a void domain broadcasts into any D (the language's single implicit
  coercion) and does not constrain it. Type variables are lexically visible to
  nested function declarations (the constraint lookup walks the enclosing
  declaration stack).

Deviations from the sketch, accepted: parameters use the `name: type` form with `;`
separators (not type-first with commas).

**WP4.1 tranche 1 implemented (2026-07-14): type-application binding enforcement.**
The `sig<V, D>` bindings are no longer documentation: at each application, every
`sig<V, D>`-typed parameter contributes the bound function's **concrete** positions
(declared value classes; declared domains resolvable in the bound function's
definition scope) as instantiation constraints on the applied variables, merged into
the same variable-binding maps the data arguments populate — so conflicts are caught
across function arguments, between function and data arguments, and against the
variables' declared constraints, each with precise attribution:
`compose(fhalf, iinc)` → *"inconsistent instantiation of type variable 'V': Int32
(from function 'iinc' bound to parameter 'g') vs Float64 (via parameter 'f')"*;
`mixap(road_only, Rail/flow)` → *"inconsistent instantiation of domain variable 'd':
the domain declared by function 'road_only' (bound to parameter 'h') differs from
the domain bound via parameter 'y'"*. Generic positions of a bound function
constrain nothing (a fully generic `sqr` remains bindable to any instantiation).
Mechanism: the ordered `<var: constraint>` list and per-parameter type-application
arguments persist in the function spec (`TreeItem_SetFunctionTypeVars`,
`TreeItem_GetFunctionParamSigTypeArgs`); `ReduceValue` maps signature variables to
applied variables positionally and merges per-position constraints
(`SigConstraint` pass in `tic/AbstrCalculator.cpp`).

**WP4.1 tranche 2 implemented (2026-07-14): Robinson unification with
variable-variable links.** The per-application bind-or-conflict maps are replaced by
a unification store (`TypeUnifier`, `tic/AbstrCalculator.cpp`): type and domain
variables — identified by *(owner function, name)* — live in union-find equivalence
classes; a class carries at most one concrete binding (a value class, resp. a domain
unit merged via `UnifyDomain`) and, for value variables, the **intersection** of all
member constraints as an acceptance set over the closed value-class universe (the
§5.7 v2 bitset mechanism). A `sig<V, D>`-typed parameter now propagates the bound
function's **generic** positions too: a position naming the bound function's own
variable `W` links `W ≡ V` (tranche 1 skipped these — "generic positions constrain
nothing"). Consequences, each attributed:

- a constraint reaches an applied variable *through a link*: `ap(fhalf, int_data)`
  with `fhalf<W: floats>` bound to `f: nuf<V, D>` errors *"Int32 (parameter 'x')
  does not satisfy 'W: floats' (function '/fhalf' bound to parameter 'f')"* — at
  `ap`'s application, instead of deep inside `fhalf`'s later one;
- an **empty constraint intersection** errors with no concrete anchor anywhere:
  `ap2(fhalf, uinc)` with `uinc<U: uints>` → *"no value type can instantiate type
  variable 'V': 'W: floats' (function '/fhalf' bound to parameter 'f') conflicts
  with 'U: uints' (function '/uinc' bound to parameter 'g')"* — pure
  variable-variable reasoning, invisible to tranche 1;
- a bound function using **one** variable in two signature positions forces the
  mapped applied variables equal (`A ≡ W ≡ B`), so conflicting data arguments are
  caught transitively (*"inconsistent instantiation of type variable 'B': Int32
  (parameter 'y') vs Float64 (parameter 'x')"*).

The occurs check is trivially satisfied at application time: §5 type terms are
shallow (concrete units are opaque, compared by key identity per §2). Composite
parameter types — a network table with `node_rel`s and an impedance attribute — do
not change that: record/table types enter the unifier as one-level field-wise terms,
and the only self-reference (a member attribute's domain being the enclosing unit)
is a *binder*, not a free variable, so no cyclic substitution can arise; structural
member checks remain per-application until the definition-time walker types them.
Tranche-1 message wording was unified under the store's source attribution:
`compose(fhalf, iinc)` now reads *"inconsistent instantiation of type variable 'V':
Float64 (function '/fhalf' bound to parameter 'f') vs Int32 (function '/iinc' bound
to parameter 'g')"*, and `mixap(road_only, Rail/flow)` *"… the domain bound via
parameter 'y' differs from the domain bound by function '/road_only' bound to
parameter 'h'"* (tranche-3 wording — the shared LinkSignatureBinding helper builds
the source). The store is per-application and monotonic. Tests
`testcases/fn_test_vv{,_neg1,_neg2,_neg3}.dms`.

**WP4.1 tranche 3 implemented (2026-07-14): the definition-time typed body walker.**
The WP3.4 checker now derives TYPES: the function's own type/domain variables (and
its unit parameters) enter the unification store as **rigid** (skolem) variables —
the body must be well-typed for *every* instantiation, so pinning one to a concrete
type/unit, forcing two of them equal, or narrowing one below its declared constraint
is a definition error, caught at the first reference without any application
evidence. Inference is bottom-up over the body reachable from the result
(`DefType` terms: value = concrete class | unifier node | unknown; domain
additionally knows `void`; function values carry the item whose declared signature
types their application). Each callee instantiates its declared signature under a
fresh variable instance; dependent positions (a parameter's declared domain naming
another parameter) share the callee's per-instantiation identity node, so passing a
unit parameter plus an attribute over a *different* domain fails definitionally.
Signature-typed parameters and sig-typed results type HOF plumbing end-to-end
(`compose(sqr, sqr)(x)` in a body is fully checked); declared annotations on locals
and the result unify against the inferred types. Deliberately DEFERRED (type
Unknown, still per-application): built-in operators (signature reification is the
remaining tranche), externals, variant selections, partial applications,
member/container accesses, and names captured from an *enclosing* function (closure
environment — resolved at reduction). Definition-rejection examples, all with a
single valid-looking call site that application-time checking would accept:

- `bad<V: numerics>(x) := fonly(x)` with `fonly<W: floats>` → *"the definition of
  '/bad': type variable 'V' must satisfy 'W: floats' (function '/fonly') for every
  instantiation, which its declaration does not guarantee"*;
- `both<A, B>(x, y) := pick(x, y)` with `pick<W>(p, q)` → *"the body requires type
  variables 'B' and 'A' to be equal (parameter 'q' of function '/pick'), but they
  are independent generic parameters of the definition"*;
- `bad<V>(x) := float64(gid(x))` declared `-> attribute<V>` → *"the body requires
  type variable 'V' to be Float64 (the calculation rule of 'result'), but 'V' must
  remain generic in the definition"* (conversions type as their class, preserving
  the argument's domain);
- the domain analog via a shared callee domain variable → *"the body requires
  domains 'D2' and 'D1' to be equal …, but they are independent in the definition"*.

`CheckFunctionDefinition` now also runs at every application entry (`ReduceValue`,
once per function via the `definitionChecked` flag), so closures, prelude functions
and variant members are covered uniformly — previously only direct call heads were
checked. The same tranche fixed a tranche-2 defect: variables are now additionally
keyed by an **instance** number, so two independent bindings of the same generic
function no longer link through a shared node (binding `gid` to both `f: nuf<A, D>`
and `g: nuf<B, E>` must not force `A ≡ B`; regression `testcases/fn_test_vv4.dms`).
A parser defect found in the same round is fixed: a bare-identifier result `:= x;`
naming a *parameter* was captured by the designation scan (parameters are sub-items
too) and designated the expression-less parameter item, so the applied function had
no calculation rule ("function signature without implementation"); designation now
skips the first `paramCount` sub-items, matching §5.1 ("designates an existing
**body** item"), and a bare parameter name becomes the result's calculation rule
(regression `testcases/fn_test_bareid.dms`, incl. exact-case and case-insensitive
body-item designation guards). Tests
`testcases/fn_test_dt{,_neg1,_neg2,_neg3,_neg4}.dms`.

*Adversarial review round (4 dimensions, 13 agents, findings verified against the
built binaries with live repros) confirmed five walker defects, all fixed before
landing:*
(1) **data-capturing closures** were rejected — `FindItem` ascends the parent chain,
so an enclosing function's data/unit parameter was "found" and hit the
reference-into-template error before the closure-capture deferral could fire; the
deferral now also guards that throw (`fn_test_closure2.dms`, the canonical `adder`).
(2) **argument-position parameter matching by name** bypassed body-local shadowing
that the reducer honors — `InferArg` now short-circuits only function-typed
parameters (mirroring `ResolveBodyArg`) and resolves data names by the
nearest-scope walk (`fn_test_shadow.dms`).
(3) **rigid variables lexically owned by an enclosing function** (named only in a
type application) were seeded with an all-set acceptance set, making the for-all
check spuriously strict — the seed now falls back to the enclosing declaration's
constraint (`fn_test_encl.dms`).
(4) **token-resolution precedence**: an own `<…>` clause now shadows the origin's
variables, and unmapped tokens of a type application resolve only in the
signature's own lexical world (never the checked function's variables).
(5) **declared-type tokens of body items** could resolve to a same-named unit in
the definition scope past a sub-container-local shadower — such tokens now defer
(`HasBodyShadower`, `fn_test_shadow.dms`).

**WP4.1 operator signatures, batch 1 + §5.12 auto-typed declarations (implemented
2026-07-14).** The typed walker's operator branch now consults a hand-curated
signature registry (`FindOperatorSignature`/`InferOperator`,
`tic/AbstrCalculator.cpp`): each signatured application instantiates an implicit
generic signature — one fresh value node and one fresh domain node shared by all
positions and (per kind) the result — exactly like a generic callee, so the
existing unification (rigid ∀-semantics, void broadcast through the shared domain
node) does all the reasoning. Batch 1, chosen strictly for kinds-level certainty:
`add sub mul neg min_elem max_elem MakeDefined` (same value class in and out —
metric products differ at the unit level, which stays per-application per §11;
`+` on strings is covered by the unconstrained variable), `eq ne lt le gt ge`
(→ bool over the shared domain), `and or not` (bool), `sqrt exp log sin cos tan`
(floats-constrained — so `bad<V: numerics>(x) := sqrt(x)` is now a definition
error: *"type variable 'V' must satisfy 'sqrt: floats' (operator 'sqrt') for
every instantiation"*, and `x + boolattr` pins V and errors likewise). An arity
outside the recorded range simply DEFERS, so signatures can only add judgments,
never new arity rejections; unsignatured operators keep deferring.

**Batch 1 is an interim.** `OperSigKind`'s expressive ceiling is "all positions
share one unit", which covers the arithmetic core and nothing else: the whole
relational family (`lookup`, `rlookup`, `index`, `invert`), the aggregations
(result domain = the partitioning attribute's *values* unit) and the composites
(`discrete_alloc`, `impedance_matrix`) impose constraints it cannot state — above
all *values-of-A == domain-of-B*, which is additionally blocked by the walker's
value/domain node split (`DefType`'s value side carries a ValueClass, never a
unit identity). The successor design — operators presenting their unit
constraints through a virtual `Operator::DescribeSignature(AbstrSignatureBuilder&)`,
consumed by the unifier, a signature printer, overload selection, and a debug
drift-verifier — is specified in **`operator-signature-interface.md`**, together
with the `DomainNode`→`UnitNode` generalization that unblocks the relational
family. That design retires this registry in its batch A.

**§5.12 auto-typed declarations**: `name := expr;` declares a PLAIN item whose
class and domain follow from its calculation — the DC layer derives them at meta
time (the `map`-children precedent), and inside function bodies the typed walker
infers them (`raw := x * y;` is typed V[D] via the mul signature with no
declaration). This also makes an untyped `result := …;` body item valid, so
`-> restype { result := …; }` works without a typed result declaration. Grammar:
`bareExprDecl` ordered after `anonFnDecl` (which claims `:= function` literals);
`OnBareExprHeading` creates a SignatureType::TreeItem item. Limit: a direct
FUNCTION-application rule on a bare item still requires a typed holder (the §5.9
container-holder guard, with its existing instructive error); operator and
data rules work. Tests `testcases/fn_test_opsig{,_neg1,_neg2}.dms`; all prelude
bodies re-validated under typed operator positions (battery + tst /Rescale +
/Arithmetics). **Staging semantics for the underdetermined case** (ruling
2026-07-16, `operator-signature-interface.md` §17): a bare item whose expression
is *opaque* at definition (a call into a composite/unsignatured operator, e.g.
`land_use := discr_alloc/landuse;`) has inferred type ⊤ — it does **not** error;
its type stays unknown, propagates as such, and a definition-time **warning** lists
the underdetermined spots ("add a declaration to check it here"). Only the new bare
form can reach this (a classic `attribute<V>(D) x := …` already carries a full
declared type), so no legacy config is affected. The declaration is thus the lever
that moves a check from instantiation-time back to definition-time.

**Batches 0 + A of the successor SHIPPED (2026-07-19)** — see
`operator-signature-interface.md` §12.1. `Operator::DescribeSignature` +
`OperSignature.h/.cpp` (records, merge, group cache, printer) exist; the clc
attribute families (unary/binary/ternary, casted convert/value, and the
`min_elem`/`argmin` variadic family) describe themselves; the walker's
`InferOperatorApplication` applies the unique surviving merged record and the
hand-curated registry above is **retired**. Signatures are now *derived from the
registrations*: member class tuples carry cross-position co-variance (all-agree
positions link hard — the old shared-node semantics — and all-agree classes bind,
though never onto a rigid ∀-variable), support sets are **soft** on rigid
variables (the prelude's `<T: any>` predicates over `eq`/`lt` mandate this; the
batch-1 `sqrt: floats` ∀-error is retired — sqrt is in fact registered over
`num_objects`, so the hand-written claim over-constrained), and a concrete
argument class that no member accepts errors at definition. `DefType` gained a
`ValueComposition` field (§18.4) so Single-composition arguments eliminate
sequence-registered members. Coverage widened from ~22 hand-listed names to every
group with described members (`iif`, `div`, `mod`, `pow`, string ops, predicates,
rounding, `convert` …), with `and`/`or`/`not` no longer falsely pinned to bool.

**Batch U SHIPPED (2026-07-19)** — the `DomainNode`→`UnitNode` generalization
(`operator-signature-interface.md` §8): one unit-identity pool serving both the
domain role and (new) the values-unit identity of a data term, each unit node
carrying an eagerly created companion class node keyed like `ValueVar` so class
reasoning and unit identity stay consistent through the `BindUnit`/`LinkUnit`
invariant. The K2 join-key contract is now expressible and checked in FUNCTION
signatures: `unit<uint32> E; attribute<E> rel (D); attribute<V> vals (E)` flows
both roles of `E` through one node — inconsistent instantiations (even between
units sharing a value class, where only identity distinguishes them) error at
the definition's first reference (`fn_test_unitnode{,_neg1,_neg2}.dms`).
Operator records still claim no values identity (dark; batch B turns it on).
Adversarial review before landing corrected two S1 hazards: concrete-vs-concrete
values units DEFER (reduction compares values units by `UnifyValues`
class+metric, under which key-distinct metric-less units unify — key-identity
errors would reject working configs), and a unit parameter's companion class
node is pinned to its declared class regardless of which resolution path
creates the node first (`UNode` scans the owner's parameters;
`UnitVar` reconciles a later-supplied declared class).

**Batch B SHIPPED (2026-07-19)** — the relational family
(`operator-signature-interface.md` §12.2): `lookup`/`collect_by_org_rel` (the
K2 join-key: one variable in org_rel's VALUES role and values' DOMAIN role),
`rlookup` and kin (K4: the result's VALUES unit IS arg2's domain), `invert`
(double cross-role), `index` (conservative result — not categorical at
reduction). The walker claims values-unit identity exactly for record
variables used in both a values and a domain role; values-only variables stay
class-level (their reduction discharge is `UnifyValues`, where key identity
would over-reject). Join-key mismatches now error at the definition's first
reference: *"inconsistent instantiation of unit variable 'E2': the unit bound
argument 1 of operator 'lookup' differs from the unit bound argument 2"*.
Adversarial review caught one S1 hazard pre-landing: sequence/polygon members
register COMPOSED classes while walker terms carry FIELD classes — fixed
centrally by normalizing `MemberValueClass` to the field class (also curing
batch A's latent sequence-`iif` variant); the borrowing regression
(default-metric rel values against a named domain) ships as a positive in
`fn_test_opsigB.dms`.

**Batch C SHIPPED (2026-07-19)** — the aggregations
(`operator-signature-interface.md` §12.3): total (K15 void results) and
partitioned (K1 shared domain + K5: the result ranges over the PARTITIONING
argument's VALUES unit) unary/binary families plus `pcount`. The classic error
— declaring a partitioned aggregation's result over the data domain instead of
the partition set — now fails at the definition's first reference
(`fn_test_opsigC_neg1`). Wildcard (AbstrDataItem) argument classes — the
partitioning position, weighted-modus weights — leave their variables
member-unconstrained rather than suppressing the description (the first
describe bailed on them; the batch's own negative caught it, and the review
caught the weight variant).

Still open in WP4.1: batches D–F (fresh-unit family + LispPtr memoization,
composite printers), and lifting the function-application-into-bare-item
restriction.

### 5.11 Anonymous functions and the brace-disambiguation rule *(tier A implemented 2026-07-14)*

**The disambiguation rule** (user decision 2026-07-14). `:= expr { … }` is ambiguous:
is the brace block part of the expression (a literal's body) or the declared item's
sub-item block? Resolution, adopted as a language principle:

> **An unbracketed `{` following a calculation rule always opens the declared item's
> sub-item block. Braces are expression content only when enclosed — directly or
> transitively — in parentheses** (a call's argument list, or an explicitly
> parenthesized expression).

This generalizes the existing container-literal restriction (§5.9 B: argument
position only — which *is* inside parentheses), keeps the rule-extent finder trivial
(no lookahead: an expression ends at any top-level `{`), matches the JavaScript
precedent for `{` at statement start, and is backward compatible (no existing
expression contains a top-level `{`). It also charts the path for the deferred
sub-expression container literals: `2.0 * (X { m: e; })`.

**Anonymous functions** need no new runtime machinery — every reduction-side
mechanism operates on function *items*, so nameless functions are a parse-time
feature. Implemented forms (tier A, stx only):

- **A1 — whole-rule literal**: `value := function[<typevars>](params) -> restype …;`
  reroutes at `:= function` into the `functionDecl` productions with `value` as the
  function's name (`anonFnDecl`; the group action fires only after a
  `function (`/`function <` lookahead confirms, so no action pollution on other
  `name := …` forms — note bare `name := expr` is not otherwise a legal item
  declaration, so the form is unambiguous). The declared item IS the function, so a
  following unbracketed block is simultaneously its body and its sub-item block —
  no ambiguity, no parentheses needed. A `;` after the block is tolerated (the form
  reads like an assignment).
- **A2 — designation by name without `:=`**: `-> restype { … result … }` (named and
  anonymous forms alike). `OnFunctionDeclEnd` now attempts implicit designation for
  ANY missing result expression — the result name (default `result`) must name a
  body item (parameters excluded, per the 8a34df56 rule); if none exists the error
  says so. Previously implicit designation existed only for `-> function`.
- **A3 — result-position literal**: `-> sig<V, D> := function(params) -> T := e;`
  declares the anonymous function under the enclosing result name and designates it
  — byte-for-byte the semantics of the nested `function result(…)` idiom, without
  the magic name. Requires a `-> function` or signature-typed result (a data result
  type with a function value is rejected at parse). Per the disambiguation rule the
  result-position literal takes no brace tail of its own (an unbracketed `{` after
  it is the ENCLOSING function's body block; the literal's locals live there).
  Grammar: `anonResultFunction` as an alternative inside `functionResultSpec`;
  the shared `functionSigAndResult` rule factors the header for all forms, and the
  rule recursion (result spec → literal → result spec) permits nested anonymous
  results.

**Tier B implemented (2026-07-14): function literals in parenthesized expression
positions, via parse-time lambda lifting.** Two placements, per the brace rule:
argument position (`compose(function<W: numerics, E: domains>(attribute<W> y (E))
-> attribute<W> (E) := y + 1.0, sqr)`) and an explicitly parenthesized group with
call suffixes (`(function(…) -> … := 3.0 * z)(Road/flow)`). Mechanism: the
expression grammar recognizes a `function`-anchored **balanced extent** only
(`functionLiteral` + `balancedParens`/`balancedBraces` scanners in `ExprParse.h`;
string literals skipped atomically; the `:=` tail uses the full expression rule, so
strings and nested literals are handled); the config-side capture
(`EmptyExprProd` → `FunctionLiteralSink`) records the extent, and at rule-storage
time (`DoExprProp` / `OnFunctionResultExpr`) each outermost literal is
**hoisted**: `ConfigProd::HoistFunctionLiteral` re-parses
`function _lambda_<n> <literal-tail>` as a nested declaration
(`ParseNestedDeclaration`, re-entrant under the recursive Spirit mutex) into the
CURRENT context — a hidden sibling of the declared item, or a body item of the
function whose result expression contains it — exactly the literal's lexical
position, so §4.6 scoping and capture-by-value hygiene come free, and enclosing
type variables (`(D)` in a literal inside a generic function) resolve through the
live `FuncProdState` stack. The synthesized name is then **spliced** over the
literal in the stored rule text; for the parenthesized-group form the splice
swallows the parentheses, so `(function …)(x)` re-parses as the ordinary suffixed
call `_lambda_n(x)`. Nested literals are dropped from the pending list when an
enclosing literal completes (the enclosing hoist's own parse re-lifts them in the
right inner context). The hooks are templated over the scanner's iterator type
(the expression grammar is instantiated with position iterators AND plain
`CharPtr` for HTML rendering). Applying two textually identical lambdas to the
same arguments β-reduces to the same interned key, so applicative identity is
preserved at the data level.

*Adversarial review round (2 dimensions, 13 agents, empirical repros against the
built binaries) confirmed 11 defects; the substantive ones fixed before landing:*
(1) the speculative extent scan captured legacy `function(args) -> item` arrow
expressions on items literally named `function` (they exist in real tst configs) —
the literal now REQUIRES an implementation (`:= expr` and/or `{ body }`), which no
legacy text contains, and the result-type position backtracks instead of asserting;
(2) the angle scans were unbounded (could swallow `< … >` across expression
boundaries) — now bounded to type-clause content; (3) extents fired by
backtracked speculative branches were hoisted anyway — identical re-captures now
dedup, and a divergent stale extent yields a clear "ambiguous function-literal
capture" error instead of a garbage declaration; (4) a literal in a PARAMETER
declaration would land the hidden item between the parameters, breaking the
params-are-the-first-N binding — now rejected with a dedicated message; (5) the
`_lambda_<n>` counter was per-parse-session (`#include` collisions) and the prefix
reservation unenforced — the hoist now PROBES for a free name against the target
container (also collision-proof against user items); (6) a literal beside the
configuration root gave a misleading error — now a dedicated message; (7) a bare
parenthesized literal as the whole result expression under a DATA result type
silently designated the function — now rejected (a `-> function`/signature-typed
result is required), the inverse of the existing `'-> function'` designation guard.
Accepted limits (v1): literals carry no `using` clause and no named result;
hoisted items record the synthesized declaration's line-1 source location (GUI
go-to-source lands at the file top for `_lambda_` items); a literal reaching the
calculation-time parser (only possible through leading-`=` string evaluation)
errors cleanly; the GUI shows the spliced name in calculation rules. Tests
`testcases/fn_test_anon{,_neg1,_neg2}.dms`, `testcases/fn_test_lambda{,_neg}.dms`,
`testcases/fn_test_lambda2.dms` (legacy `function`-named items, bounded angle scan,
name-probe collision).

## 6. Verified mechanism inventory

Facts the design rests on, all verified in the current tree:

**Parser and expressions (stx):**

- Call syntax is already generic: `functionCallOrIdentifier = identifier >> !('(' >>
  exprList >> ')')` (`stx/dll/src/ExprParse.h:257-267`); heads are undistinguished
  identifiers, resolved at calc time via `AbstrOperGroup::FindName` — any non-operator
  name falls back to the shared `theTemplGroup` (`tic/OperGroups.cpp:220-228`),
  dispatched in `SubstituteExpr` (`tic/AbstrCalculator.cpp:1466-1490`). No call-site
  grammar change is needed.
- Typed formals need new grammar: today the `(...)` after an item name only accepts
  item paths + composition keywords (`itemParam = itemRef`,
  `stx/dll/src/ConfigParse.cpp:141-146`; `ConfigProd.cpp:413-423`). The parameter
  telescope is the main stx work.
- `SignatureType::Function` **already exists as a reserved stub**
  (`stx/dll/src/ConfigProd.h:26`) — enum member, no keyword, no `CreateItem` case.

**Expression core (sym):**

- `LispRef` is hash-consed per node kind (`sym/LispRef.cpp:381-494`); structural
  equality is pointer equality (`sym/LispRef.h:106`) ⇒ TypeSpecs encoded as LispRef
  terms get O(1) comparison and free applicative keying.
- Substitution/unification machinery exists: `Match` (`sym/Lispeval.cpp:198-215`),
  `AssocList::ApplyOnce/ApplyMany` (`sym/Assoc.h:142,197`), fixpoint driver
  `ApplyTopEnv` (`sym/Lispeval.cpp:576-645`), pattern variables via `ChroID`
  (`SymbObj::IsVar`, `sym/LispRef.cpp:215`). An applicative evaluator (`EvalLet`,
  LET/CASE) exists but is compiled out (`MG_USE_LISPFUNCS`). Gap: no alpha-renaming —
  fresh-ChroID renaming per reduction is the natural fix (ChroID was designed for
  Prolog-style renaming).
- Rewrite rules are data, loaded from `res/RewriteExpr.lsp` (deployed next to the exe;
  `tic/ExprRewrite.cpp:54-78`). The parse-time hook `RewriteExprTop_InParse` is a
  **no-op** (`tic/ExprRewrite.h:45`); actual rewriting runs at *meta* time —
  `RewriteExpr(GetLispExprOrg())` in `GetMetaInfo` (`tic/AbstrCalculator.cpp:682,
  1518`) and `RewriteExprTop` inside `SubstituteExpr_impl` (`:1409`) — i.e. **before
  head dispatch**, so a rule head can capture a user-chosen item name (§8.2).

**Tree-item calculus (tic):**

- DataControllers are keyed by interned LispRef ("expr + root of context") in the
  global `s_DcMap` (`tic/DataController.cpp:394-476`) — the applicative-identity
  substrate already exists.
- Overload resolution: `Operator` stores a `ClassCPtr` array + result class
  (`tic/Operator.h:55-112`; `VariadicOperator` at `:316-329`, optional-argument counts
  at `:104-107`); `AbstrOperGroup::FindOper` matches with `IsDerivedFrom`
  (`tic/OperGroups.cpp:355-431`), driven by `FuncDC::GetOperator` on the argument DCs'
  result classes (`tic/MoreDataControllers.cpp:494-521`). Human-readable signatures are
  synthesized by `GenerateArgClsDescription` (`tic/OperGroups.cpp:328-344`). What the
  ClassCPtr layer does *not* capture is unit relationships (which argument domains must
  coincide; how the result unit derives) — those live imperatively in each
  `CreateResult` plus a small named vocabulary of `UnitCreatorPtr` functions
  (`compatible_simple_values_unit_creator`, `operated_unit_creator(cog_*)`,
  `arg1_values_unit_creator`, `default_unit_creator_and_check_input`, … —
  `clc/dll/include/UnitCreators.h`). Full TypeSpec signatures are therefore **largely
  mechanically derivable**: ClassCPtr array ⊕ unit-creator name ⊕ domain-unify pattern.
- Compound and unit-valued DC results are established patterns: `unique`
  (unit-with-subitem result, `clc/dll/src/Unique.cpp:330-360`), `PhaseContainer`
  (container deep-copied into a parentless cache tree, then per-member data movement,
  `clc/dll/src/PhaseContainer.cpp:40-121`), `SubItemOperator` + the `slSubItemCall` key
  form for member access (`clc/dll/src/SubItem.cpp:54-96`,
  `tic/AbstrCalculator.cpp:108-123`), and endogenous shadow copies that expose cache
  subtrees for GUI browsing (`tic/TreeItem.cpp:2389-2393`).
- Metric algebra: see §3.1/§4.7; add/sub require a shared values unit
  (`compatible_simple_values_unit_creator`, `clc/dll/include/UnitCreators.h:125-128`);
  mul/div combine metrics through unit-level operators registered into
  `cog_mul`/`cog_div` (`clc/dll/include/OperUnit.h:95-220`,
  `clc/dll/src/OperAttrBin.cpp:443-472`); `pow` demands dimensionless input
  (`UnitCreators.h:50-70`).

## 7. Signatures as data

Encode signatures as interned LispRef terms (TypeSpecs):

```
(unit-param  <name> <ValueClassID>)
(attr-param  <name> <domain-ref> <ValueClassID> <vc>)     domain-ref = telescope index
                                                          of an earlier unit param, or
                                                          an absolute key expr
(fun-sig     (params …) (result …))                       function-typed params nest;
                                                          one fun-sig per variant
```

Uses: definition-time checking (§10 P0), overload resolution and disjointness checking
(§5.7), printable signatures for diagnostics/GUI/docs, and P2's declarative operator
signature table (derived mechanically per §6, with unsignatured operators falling back
to today's per-application checking — gradual adoption). Diagnostics should explain
unit mismatches in the system's own terms: *"different defining expressions"*, with
both expressions printed.

## 8. Retiring RewriteExpr.lsp

### 8.1 Why

The rewrite layer (`res/RewriteExpr.lsp`, ~90 rules) is the untyped macro system of
GeoDMS: pattern → resolvent on LispRef terms, applied to fixpoint at meta time before
head dispatch. Its problems are structural:

- **Untyped**: rules fire on syntactic shape only. Some are type-unsafe as written —
  `[(mean _X (id _E)) _X]` (line 275) *assumes* domain compatibility ("don't aggregate
  over an id relation; assume that domain is compatible") without checking it.
- **Name capture**: rewriting precedes item lookup, so a rule head (`sqr`, `abs`,
  `rjoin`, …) silently captures any user item of that name.
- **No result-type discipline**: a rule's resolvent determines result types only
  emergently, through whatever the rewritten expression happens to do.
- **Engine-global**: rules apply to every config, are versioned with the executable,
  and even contain application-domain logic — `claim_divF32`/`claim_corrF32` reference
  the config path `/Classifications/OperatorType` (lines 230-235), i.e. RuimteScanner
  model logic living in the engine's data file. That is exactly what user-space
  functions are for.

Precedent for retirement exists in the file itself: the catch-all
`[(pow _X _Y) (exp (mul (log _X) _Y))]` was already removed when the first-class `pow`
operator landed (issue #839), keeping only integer-literal fast paths for unit-aware
exact powers (comment at lines 55-59). The phase-out below generalizes that pattern:
*retire a rule only together with its typed replacement*.

### 8.2 Rule inventory and replacement mapping

| category | examples (lines) | count≈ | replacement |
|---|---|---|---|
| **A. Definitional sugar** — fixed-arity algebraic/predicate/model definitions | `sqr`, `log₂`, `plogp`, `abs`, `pow _X 2..6` (46-54); `order`, `isOverlapping`, `median`, `*_or_*_null` family (123-161); `rescale`, `normalize`, `distribute`, `scalesum` (169-199); `llpart`, `ll1` (239-240); `rjoin`, `sort_str`, `reverse`, `reversed_id`, `combine_data` (244-259); `claim_*` (230-235); `replace_value` base, `MakeDefined` binary (219-222) | ~45 | **typed `inline` functions in a standard prelude** (§8.3); `claim_*` move out of the engine into model configs |
| **B. Variadic normalization** — unary collapse + fold-left chains | `(add _a1)→_a1` etc. (62-70); n-ary `add/mul/or/and/MakeDefined/union` folds (72-77); `concat` (79-81); `replace_value`/`replace`/`combine_data`/`index`/`subindex` chains (223-259) | ~20 | **variadic operator registration** (`VariadicOperator` exists, `tic/Operator.h:316-329`) or parser-level canonicalization; with P3, expressible as `fold` |
| **C. Control-flow sugar** | `switch`/`case` unrolling to `iif` chains incl. constant short-circuits (85-91) | 4 | **real variadic `switch` operator** with typed checking (all case values unify; conditions bool over one domain) |
| **D. Simplification/optimization** — not definitions | boolean/constant algebra (95-113); `add _X 0`, `mul _X (div 1 _Y)` (203-205); `MakeDefined` idempotence (219); self-join elimination `lookup(rlookup a a)` (245); pseudo-aggregation eliminations incl. the type-unsafe `mean/sum/modus _X (id _E)` (266-277) | ~25 | **compiled-in typed simplifier pass** owned by the engine (not a user-editable data file); each rule fires only after its typing precondition (e.g. `UnifyDomain`) verifies — fixing the unsafe ones |
| **E. Argument completion / bridging** | `Value→convert` (24); `const` reorder (25); `ReadValue→ReadArray` default (165); `collect_by_cond` per-`select_*` arg injection (29-42); ~~property accessors `name/Descr/…→PropValue`~~ (retired 2026-07-17 via §5.13 `item` parameters); `BaseUnit` fixups (116-119) | ~15 | **optional/default arguments** (`m_NrOptionalArgs` exists) + **typed overloads** (§5.7) — `collect_by_cond` dispatching on the select flavor is precisely overload resolution on a Σ-result type |

### 8.3 The key-identity hazard, and `inline` functions

Rewrite rules do a second job besides definition: they **canonicalize spellings into
shared DC keys**. Today `sqr(x)` rewrites to `mul(x, x)` before keying, so both
spellings intern to the *same* LispRef → same DataController → and, via `UnifyDomain`'s
key-expression branch, **units defined through either spelling unify**. Naively
replacing the rule with a boundary-keyed function application would give `sqr(x)` the
key `(applyF /std/sqr <variant> xKey)` ≠ `(mul xKey xKey)` — cached results would miss
and, worse, previously-compatible units would silently stop unifying.

Resolution: a per-function **`inline` attribute**. An `inline` function's applications
are fully β-reduced into the surrounding key expression — no `applyF` boundary — so
`sqr(x)` keys as `(mul xKey xKey)`, byte-identical to today. Non-`inline` functions
keep the `applyF` boundary (attribution anchor, memoization of large bodies, R7).
Inline-ness is therefore **part of a function's interface**: changing it changes cache
identity and unit unification across spellings, and must be flagged as such in
documentation and diffs. All category-A prelude replacements are declared `inline`.

### 8.4 Transition plan

**Tranche 1 implemented (2026-07-13).** `res/prelude.dms` ships next to the exe
(deployed by the `Clc.vcxproj` CustomBuild copy and `tools/DeployResources.cmake`,
same mechanism as the .lsp); configs opt in with `#include <%exeDir%/prelude.dms>`.
Retired (14 rules): `sqr`, `plogp`, `llpart`, `ll1`, and the ten `*_or_*_null`
predicates. Every prelude body is the exact fixpoint expansion of its retired rule, so
keys are byte-identical (verified: `range(uint32,0,sqr(p))` and `range(uint32,0,p*p)`
intern to one domain key — the R13 regression in `testcases/fn_test_prelude.dms`).
Retained rules whose resolvents referenced retired heads carry the expansions inline
(`pow 2/3/4/6` → `mul` chains; `isOverlapping` → expanded `le_or_lhs_null`), keeping
their output keys unchanged. Not retired, with reasons: `order` (its
`(interval …)`-producing role feeds the *pattern-matching* rules `isOverlapping`,
`median`-on-interval, which destructure syntactic `interval` nodes — a function
application would hide the node from the pattern), `median`/`isOverlapping`/
`float_isNearby`/`point_isNearby` (destructuring patterns), `abs` (kept with them for
now), multi-arity `rescale`/`normalize`/`distribute`/`scalesum` (tranche 2: arity
dispatch via variant sets), `claim_*` (moves to RuimteScanner configs — needs
coordination), categories B/C/D/E per the plan below. A config using a retired head
without the prelude gets "'sqr': unknown operator and no template or function was
found with this name".

**Tranche 2 implemented (2026-07-13).** The multi-arity `rescale` (1/3/4 args),
`normalize` (1/2/3), `scalesum` (2/3) and `distribute` (2/3) families retired
(10 rules) as **variant sets dispatching on arity** — `ResolveVariant` already skips
variants whose parameter count differs, so arity overloading came free. Enabling
grammar change: `variantDecl` accepts a `<V: constraint>` type-params clause
(`stx/dll/src/ConfigParse.cpp`; the heading handler already moved the pending type
vars). The partition parameter is declared `container P` — a plain-item parameter
whose null value class acts as a dispatch wildcard, so any partition value type
matches. Bodies are fixpoint-faithful *including* the D-rule collapses the 1-arg
forms used to undergo (`rescale(x)` keys as
`(div (sub x (min x)) (sub (max x) (min x)))`, `normalize(x)` as
`(div (sub x (mean x)) (sd x))` — the raw-literal `0`/`1` of the arity-completion
resolvents triggered `add _X 0`/`mul _X (div 1 _Y)`, which explicit config literals
never did, so only the 1-arg forms collapse). Note: `rescale`'s `min`/`max` (and
`normalize`'s `e`/`s`) are scalars, as in the retired rules — the trailing
`+ mn` broadcasts void into the data domain.

**Tranche 3 implemented (2026-07-14).** Seven more rules retired: `abs`
(`isNegative(x) ? -x : x` — exact old resolvent), `sort_str`
(`lookup(index(x), x)`), `reversed_id` (a `container D` plain parameter binds any
domain unit; body is the exact `UpperBound/LowerBound/id/convert` expansion),
`reverse<T: any>` (points and strings included — `lookup(reversed_id(DomainUnit(x)),
x)`, the sibling call β-reduces to the old fixpoint), and the proximity family
`neighbourhood`/`float_isNearby`/`point_isNearby` — the key insight being that their
bodies SPELL `order(…)`/`isOverlapping(…)` calls, on which the two **retained**
pattern-matching rules still fire at body-parse fixpoint, so the reduced keys equal
the old rule chain without hand-expanding anything. Key identity re-proven by a
range-domain unification probe (`range(uint32,0,uint32(abs(pp)))` from both
spellings; `testcases/fn_test_wp45t3.dms`) and by the unmodified tst `/Arithmetics`
(its `abs` and four `float_isNearby` tests). Enabling grammar fix: the optional
result NAME (`-> name: type`) must not claim the `:` of `:=` — a not-`=` lookahead
after the colon (`-> container := …` previously mis-parsed; latent since P0, first
exposed by `reversed_id`). Still in the .lsp, with reasons: `order`/`isOverlapping`/
`median`-on-interval (destructuring patterns — and now also load-bearing for the
tranche-3 bodies), 3-arg `median` and 2-arg `log` (their heads shadow retained
rules/operators — need arity-aware head dispatch or optional args), `concat`/
**The .lsp end-state (Maarten's ruling, 2026-07-18): the rewrite file legitimately
KEEPS simplifying rewrites (boolean shortcuts, the pseudo-aggregations over `id(x)`
as partitioning, symbolic-constant removals, the MakeDefined collapse, the pow
integer fast paths) and pseudo-functions (`switch`/`case`, `interval` with its
`order`/`isOverlapping`/`median` destructurers).** Also staying, each with a
structural reason: the `collect_by_cond`/`select` injections (they destructure the
select argument; the eventual replacement is overload resolution on the select
result type — an operator-signature matter), `Value→convert` (a coercion alias the
retained-rule technique itself leans on), the `min_elem`/`max_elem`(`_fast`) unary
collapses (allow_extra_args groups — dispatch cannot fire), the NlLater
`BaseUnit`/`convert` fixups (data compat), and `rjoin` (self-join collapse rule,
below). `claim_divF32`/`claim_corrF32` are RuimteScanner model logic whose resolvent
bakes in the absolute path `/Classifications/OperatorType` — a prelude function
cannot reference it (strict scope); the right home is a user `function` in the
RuimteScanner configuration itself (needs coordination with that model's owners). Retired meanwhile: the six property accessors via
meta-reference parameters (§5.13, 2026-07-17); `concat`/`replace_value`/
`combine_data` via rest parameters (§5.14, 2026-07-18); `MakeDefined` (all arities;
the structural collapse rule stays as a composing normalizer), 3-arg `median` and
`ReadValue` (pure rule heads); the `add`/`mul`/`or`/`and` collapses+folds, 2-arg
`log`, 5+-arg `replace` and 3-arg `const` via arity-aware head dispatch (§5.15,
2026-07-18). The `union` fold rule was REMOVED by
ruling (the variadic operator is authoritative; multi-arg `union` now keys flat) and
the dead multi-arg `index`/`subindex` rules were deleted.

**Two negative findings (2026-07-14), on *why* a rule cannot become a prelude
function — the boundary of the technique. The second is RESOLVED (§5.13):**

- **`rjoin` (and any rule whose output re-triggers rewriting).** `RewriteExpr` runs
  on the original expression *before* `SubstituteExpr` β-reduces function
  applications (`tic/AbstrCalculator.cpp:695`), and a body is itself rewritten before
  substitution (`:2039`) — but the β-reduction *output* is never re-rewritten. So a
  function is key-safe only when the retained rules that would fire on its result are
  either inlined into the body or fire during the body's own pre-substitution rewrite
  (the tranche-3 `order`/`isOverlapping` case). `rjoin(a,b,c) → lookup(rlookup(a,b),
  c)` is fine on its own, but the *separate* self-join collapse `lookup(rlookup(a,a),
  c) → c` fires only when the two `rlookup` args are structurally identical, which
  only becomes true *after* substitution (`a` and `b` both bind the same key). A
  prelude `rjoin` would therefore key `rjoin(x, x, c)` as `lookup(rlookup(x,x), c)`
  instead of the old `c` — a key-identity break. Stays.

- **The property accessors need a *meta-reference* argument, which plain parameters
  don't pass** *(RESOLVED 2026-07-17 by §5.13's `item` parameter kind)*. `PropValue`
  is a `calc_requires_metainfo` operator: its item argument must reach it as a raw
  reference to the config item, not as a data expression. The rewrite rule
  `[(name _T) (PropValue _T "name")]` kept `_T` syntactic, so PropValue saw the
  reference; a prelude `name(container t) := PropValue(t, 'name')` instead resolved
  the argument through the ordinary *data* path (`t` → a unit's range expression, an
  attribute's calc rule), so PropValue read the *computed item's* metadata. Verified
  at the time: `name(container)` worked (no data key), `name(unit)`/`name(attribute)`
  returned the wrong value. The `item` parameter kind removes exactly this limit.

### 5.13 Meta-reference parameters: `item x` *(implemented 2026-07-17)*

A function parameter declared with the new `item` keyword is a **meta-reference
parameter**: its argument binds as a *raw item reference* — the very `sourceDescr`
key form (`CreateLispTree(argItem, false)`, `rtc/dll/src/tic/LispTreeType.cpp`) that
a `subst_never` operator argument gets in a direct call — never as the argument's
calculation/range key. `PropValue` & co consequently read the CONFIG item's metadata
for containers, units and attributes alike, and each application keys **identically**
to the direct call (`name(Road)` ≡ `PropValue(Road, 'name')` as interned LispRefs;
the `sourceDescr` token is the argument's *absolute* full name, so the key is
caller-scope-independent).

Mechanics: `SignatureType::MetaRef` (stx) parses `item x` in the parameter telescope;
the guard `lexeme_d[ITEM >> ¬(alnum|'_')] >> ¬(':'|'=')` keeps `items`, `item := …;`
(§5.12 bare decls) and `item : type` (an item *named* item) parsing as before —
`item` is only a keyword where a parameter type can stand. The parameter item itself
is a plain TreeItem; the kind is recorded per index in `FunctionSpecData
::metaRefParams` (`tic/TreeItem.cpp`, rides along `TreeItem_CopyFunctionSpec` for
closures/variants). At β-reduction (`FunctionApplication::ReduceValue` param loop),
a meta-ref parameter requires the argument to be an item reference (`m_ArgItems[i]`;
a calculated expression is rejected with a dedicated error) and rebinds
`m_ArgKeys[i] = CreateLispTree(argItem)` — one binding site; body occurrences and
`m_Reductions` then substitute the raw reference uniformly. No walker change: the
parameter is opaque (⊤) like a `container` parameter, and `sourceDescr` nodes already
round-trip through reducer, walker and DC layer (`SymbDC` resolves them back to the
config item by absolute `FindItem`).

With this, the six accessors are prelude functions
(`function name(item t) -> parameter<string> := PropValue(t, 'name');` etc.) and
their rewrite rules are retired. Tests: `testcases/fn_test_props.dms` (alias values +
alias≡direct key-identity probes for a container, a **unit** and an **attribute**,
plus a user function with an `item` parameter), `fn_test_props_neg1` (calculated
expression into an `item` parameter → clean error), `fn_test_itemname` (keyword
guards: `items`, bare `item := …;`, colon-typed `item : alias := …;` all parse
unchanged). Deferred: member access *through* a meta-ref parameter beyond what
container parameters already support, and `item` in result position (rejected with
the parameter-only error).

### 5.14 Variadic rest parameters: `...x` *(implemented 2026-07-18)*

A function's **last** parameter may be declared `...x`: a **rest parameter**, binding
**one or more** trailing arguments. In the body, `x` may appear only as the **trailing
argument of a function call**, where it splices the captured argument tail — no list
values enter the language; the mechanism is purely capture-and-splice. Combined with
arity-dispatched `variant` sets, this expresses the classic N-ary folds as structural
recursion:

```
function concat
{
    variant none() -> parameter<string> := '';
    variant one(attribute<string> a) -> attribute<string> := MakeDefined(a, '');
    variant more(attribute<string> a; ...rest) -> attribute<string> := MakeDefined(a, '') + concat(rest);
}
```

Mechanics (all in `tic/AbstrCalculator.cpp` + `tic/TreeItem.cpp` + stx):
- `FunctionSpecData::hasRestParam`; stx parses `... identifier` as a
  `functionParamItem` alternative (plain-TreeItem param item; must be last, at most
  one, not in signature aliases — validated at parse).
- Arity checks become `argCount >= nrParams` for rest functions (reducer, root
  pre-check, `MergeBinding` — whose last hole absorbs all surplus fills); the binder
  records the rest param and leaves `m_ArgKeys[nrParams-1 ..)` as the spliceable tail.
- **Recursion**: the parent-chain guard now permits a self-application with **strictly
  fewer arguments** (well-founded on the chain); equal-or-more arguments still error.
  Each fold step consumes at least one argument and the base variant has fixed arity,
  so termination is structural.
- **Variant dispatch on nested calls** (a fix beyond variadics): `ResolveVariant` used
  to run only at the direct-call substitution site, so calling a variant set
  (`rescale`, `scalesum`, …) from *inside* another function body — or as a nested
  argument — failed with a spurious arity error. Both nested paths now dispatch,
  which the recursive fold steps also rely on. Test: `rescale` applied inside a
  user function body (`fn_test_variadic.dms`).
- Variant matching: a rest variant matches `argCount >= nrParams` with the tail
  matched against the rest param's (wildcard) acceptance set; across different
  declared arities, the variant with more declared params is more specific. The
  def-time walker defers rest-function applications (per-application checking, like
  variant sets).

**Retired to the prelude** (bodies spell the exact rule resolvents; retained rules
`MakeDefined→iif` and `Value→convert` fire at body parse, so reduced keys equal the
old fixpoints): `concat` (right add-fold with `MakeDefined(_,'')` wrap, incl. the
0-arg and 1-arg base cases), `replace_value` (pairwise left fold onto
`iif(x==v,w,x)`), `combine_data` (left fold with `combine_unit` accumulator onto the
`Value(...)` linearization formula — its `ValuesUnit`/`LowerBound`/`NrOfRows`
arguments are `calc_as_result`/`subst_allowed`, so body parameters substitute
identically to the rule's syntactic `_a`/`_b`).

**Still in the .lsp, with reasons sharpened by this tranche:** `switch`/`case`
destructure `case(c,v)` wrappers — pattern parameters, a separate feature; `index`/
`subindex`/`replace` fold onto **registered operator heads**, so a prelude function
cannot capture the name — they need arity-aware head dispatch (an operator-group miss
by arity falling back to a same-named function), the same feature `median`/2-arg
`log` need. Also observed: the multi-arg `index` rule's resolvent references
`(index_a)` (one symbol, likely a typo for `(index _a)`) and expands into
`rank_sorted`/`sub_rank_sorted` heads that no rule or operator defines — the
multi-argument `index`/`subindex` chains appear to be dead as written.

Tests: `fn_test_variadic.dms` (concat 1–3 args over parameters and attributes;
replace_value base + fold; combine_data base + fold with value checks; rescale from a
body), `fn_test_variadic_neg1` (rest not last → parse error), `fn_test_variadic_neg2`
(rest used as a bare value → application error). The same-arity recursion negative
(`p1_rec`) still fails as before.

### 5.15 Arity-aware head dispatch *(implemented 2026-07-18)*

A registered operator group owns its head for the arities its members accept — and
**only** those. `AbstrOperGroup::AcceptsArity(n)` (cached member-arity envelope
`[minRequired, maxSpecified]`, invalidated by `Register` for late-loading DLLs;
non-caching groups and counts ≥ min for `allow_extra_args` groups always accept)
answers conservatively: **false guarantees `FindOper` would throw**. On such an arity
miss, head resolution falls back to a same-named function (scope, then prelude) — a
strict conversion of errors into function calls, so no working config changes
behavior. Two dispatch sites (the direct-call substitution and the body-expression
substitution; nested-argument positions reach them by fallthrough).

The dispatch count is the **effective** arity: a trailing `...x` rest symbol expands
to its captured argument count. This composes with §5.14 into the elegant fold shape
— **the operator is the base case**:

```
function add { variant one(container a) -> container := a;
               variant more(container a; container b; ...rest) -> container := add(add(a, b), rest); }
```

Arity 2 never reaches the function; the body's inner `add(a,b)` resolves to the
operator; the recursive call's effective arity shrinks until the last step (`rest` =
1 element) resolves to the binary operator. Rest symbols may consequently trail
**operator** calls too (spliced as keys; `a + rest` inside a fold body is legal and
folds).

Coexistence is validated arity-aware at declaration: a function may share an
operator's name iff **none of its (variants') arity ranges** intersects the group's
envelope — per-variant, so `add`'s `{1} ∪ [3,∞)` legally skips the operator's `[2,2]`
(`ValidateFunctionArityVsOperator`, replacing the flat name-collision rejection; the
rule-head guard was refined in the same tranche to reserve only *capturing* rules —
all-variable patterns — so structural rules like the MakeDefined collapse and the
median-interval destructurer compose with same-named functions).

**Retired**: the `add`/`mul`/`or`/`and` unary collapses and N-ary left folds, the
2-arg `log` rule (`two(x,b) := log(x)/log(b)`), and the 5+-arg `replace` pairwise
fold (`more(x,v,w,...rest) := replace(replace(x,v,w), rest)` — the ternary replace
operator is the base case), as prelude functions with key-identical reductions. **Stays**: the `min_elem`/`max_elem`(`_fast`) unary
collapses — their groups are `allow_extra_args`, so the operator claims every arity ≥
its minimum and dispatch can never fire (the union situation). **Deleted**: the dead
multi-argument `index`/`subindex` fold rules (`(index_a)` typo + undefined
`rank_sorted`/`sub_rank_sorted` heads — any use already errored).

Tests: `fn_test_aritydisp.dms` (add 1/3/4-arg, mul/or/and 3-arg, log 2-arg, dispatch
from inside a function body incl. the folds' recursion), `fn_test_aritydisp_neg1`
(user `function add(a;b)` at the operator's own arity → declaration error).

**Auto-import implemented (2026-07-13).** The prelude is auto-imported at config load
(`stx/dll/src/StxInterface.cpp`, `CreateTreeFromConfiguration`): parsed into a hidden,
endogenous `prelude` container under the config root. A root `using` cannot point at
its own descendant (circularity check), so visibility is instead a resolution rule in
`tic/AbstrCalculator.cpp` (`FindPreludeFunction`): **the prelude is the implicit
outermost namespace for call heads** — consulted when normal lookup finds nothing, when
it finds a *non-callable* item (a documentation container named `sqr`/`rescale` does
not capture a call head — the tst Operator suite has exactly those), and inside strict
function scopes (body heads see params → locals → imports → prelude). User definitions
shadow prelude names by the normal nearest-scope rules; only *callable* nearer items
capture. Verified against the unmodified tst suite: `Operator.dms` `/Rescale` (scalesum
2/3-arg, rescale, normalize inside self-named doc containers) and `/Arithmetics`
(`sqr(5)` inside `container sqr`, pow 2..6, abs), and `MicroTst.dms`
`parameter<meter2> gridsize_sqr := sqr(gridsize)` (metric derivation through the
prelude function). Configs need no edits; `#include <%exeDir%/prelude.dms>` remains
valid and simply shadows the auto-imported copy at nearer scope.

1. Rules retire **per category, gated on their typed replacement**: E needs
   optional-arg + overload support; B/C need variadic registrations; A needs P0/P1
   functions + the prelude; D needs the compiled-in simplifier. Each retirement notes
   its key impact (most are identity-preserving via `inline`; D-rules keep firing
   compiled-in, so keys are unchanged).
2. The prelude is a shipped, auto-imported function container (engine-versioned, but
   written in the config language — readable, typed, and overridable per the normal
   scope rules where today's rules are invisible magic).
3. During transition, the P0 name-collision validation (§10) protects user functions
   from surviving rule heads; when the .lsp file reaches empty, both the file and the
   validation-against-rule-heads retire together. `ApplyTopEnv` remains only as the
   engine of the compiled-in simplifier (category D), operating after typing
   information is available instead of before.
4. End state: **no untyped user-visible macro layer**. Definitions are typed functions;
   normalizations are operator features; simplifications are typed engine passes.

### 5.16 Checking never-referenced functions: the `@checkfunctions` audit *(implemented 2026-07-22)*

The definition-time checker (§5.10 status block, WP4.1 t3) fires at every application
entry (`ReduceValue`) — a function is validated the first time it is applied, covering
closures, prelude functions and variant members. A function that is *defined but never
referenced* is therefore never checked: the ordinary path has nothing to hang the check
on. This section closes that gap — the last piece of "function definitions type-checked".

Config load is deliberately left untouched. Forcing a definition-time pass at load
would add cost to every configuration, re-check the auto-imported prelude on every
load, and could turn a latent type error in an unused helper into a load-time verdict
on configurations that open cleanly today. Instead the checker is exposed as an
**explicit, opt-in audit**: `CheckAllFunctionDefinitions(root, reporter, clientHandle)`
(`rtc/dll/src/tic/TicInterface.h`) walks the parse-created structure of a loaded
configuration and runs the definition-time checker on every function *definition*. Two
surfaces:

- **GeoDmsRun** — the `@checkfunctions` verb (`GeoDmsRun cfg.dms @checkfunctions`): one
  line per audited function, process error level raised (exit 1) if any definition
  fails. The headless / CI surface (`run/exe/src/MainRun.cpp`).
- **GUI** — Tools → *Check all function definitions*: reports each verdict to the event
  log and shows a pass/fail summary (`qtgui/exe/src/DmsMainWindow.cpp`).

Scope of the walk (`CheckFunctionDefinitionsInSubtree`, `tic/AbstrCalculator.cpp`):

- The walk is **raw** (`_GetFirstSubItem`), forcing no whole-tree meta-update — only
  each checked function's reachable-from-`result` slice is parsed, exactly as at an
  application.
- **Closures** — a function nested in another *ordinary* function's body — are
  **skipped**: they capture an enclosing environment that exists only at application,
  where they are checked. **Template-internal functions** (defined inside a `template`
  container) are skipped for the same reason: their references to template siblings stay
  inert (`InTemplate`) until instantiation, so checking them cold would falsely fail
  (an S1 violation — reduction accepts them once the template is instantiated). A variant
  SET is not a body and is not a plain template (a function is *itself* a template, so the
  test is `IsTemplate() && !IsFunctionItem()`), so its variant members — and functions in
  ordinary containers — stay checkable. The walk carries an `insideUncheckableScope` flag
  that a variant set and a plain container pass through, but an ordinary function or a
  template container sets for its children.
- **Signature-only functions** (declared signatures) have an empty result expression
  and are checked as a no-op.
- Each verdict is *reported* but **not** persisted onto the item — the audit is
  non-invasive and never fails the tree, so it cannot change the configuration's state.

Running the checker **cold** (before the configuration is otherwise resolved) exposed one
Debug-only ordering issue: the checker resolves a call head through `TreeItem::ResolveItemPath`,
which builds the scope's `UsingCache` for the first time, and the namespace-merge walk
inside `UsingCache::UpdateCache` calls `GetReferredItem()` — triggering a fresh
`UpdateMetaInfo` under that method's own (Debug-only) no-update-metainfo guard. At
application the referred chain is already warm, so it is a no-op; cold it asserts. Fixed
by warming the referred-item chain in the permitted context *before* the guard
(`tic/UsingCache.cpp`, inside `#if defined(MG_DEBUG)` — Release is byte-identical, and it
is a no-op once the chain is warm).

The audit was the first thing ever to definition-check the shipped prelude, and it
earned its keep immediately: it flagged `prelude/neighbourhood`
(`-> container := order(x, f*x)`). A naked `order(x, f*x)` rewrites (via the retained
`order` rule) to a bare `interval(…)` — a constructor with no operator, resolvable only
when consumed by the `isOverlapping`/`median` rule patterns — so it is **non-reducible
as a standalone result**: a config-scope `order(a, b)` fails reduction identically with
*"'interval': unknown function"*. This is a true positive, not an S1 over-rejection (the
checker rejects exactly what reduction rejects, because `order` is a capturing rule that
produces the `interval` form regardless of argument groundness). The function had no
callers and was removed. The full prelude now audits clean.

Tests: `testcases/fn_test_checkall.dms` — unreferenced well-typed functions (`good_sqr`,
`good_id`, `fonly`) reported OK; unreferenced ∀-violating functions (`bad_numerics`, and
the `multi/bad_gen` variant, each applying the floats-only `fonly` under
`<V: numerics>`) reported FAILED; a closure (`withClosure/bump`) correctly not audited
standalone; and the config's own `/checks` computes cleanly — the ill-typed functions
never touch the ordinary path, which is the whole point of the audit.

## 9. Verified template/for_each context

(Reference summary; details in §4.6, §5.2, §5.5.) Templates are containers flagged
`TSF_IsTemplate`, bodies inert under `TSF_InTemplate` (no calculators/passors,
`tic/TreeItem.cpp:1330-1349`); case parameters are the untyped "first N subitems"
convention; instantiation is a per-holder deep copy (`InstantiateTemplate`,
`tic/AbstrCalculator.cpp:996-1015`), root-level-only. `for_each` builds per-row
subitems under `CreateCacheRoot()` from a suffix-encoded spec
(`clc/dll/src/ForEach.cpp:141-340, 366-406`). Both remain supported; functions are the
typed successor, with a lint path template→function where bodies qualify.

## 10. Implementation plan

### P0 — typed, closed functions (still copy-instantiated) — **IMPLEMENTED in 20.9.0**

Calls behave exactly like template calls (root-level only, `InstantiateTemplate` deep
copy). New: the typed telescope, designated result, closed scope, arity check.

**Implementation status (v20.9.0):** grammar per §5.1 (three declaration forms, `->`
result specs with `:=` result expressions, name:type and multi-name declarations,
structured and composite-typed parameters, `using` clauses) in
`stx/dll/src/ConfigParse.cpp` + `ConfigProd.{h,cpp}`; `TSF_IsFunctionItem` +
`TreeItem::SetIsFunction` (rides template inertness) + function-spec side-assoc
(arity, result name) in `tic/TreeItem.{h,cpp}`; arity check at call dispatch in
`tic/AbstrCalculator.cpp` (SubstituteExpr template branch); strict scoping via
`UsingCache::RemoveParentUsing` + forced cache on function instance roots + absolute
import resolution in `UsingCache::FindNamespace` + absolute-frozen import copying in
`TreeItem::Copy`; name-collision validation via `AbstrOperGroup::FindName` +
`HasRewriteRuleForHead` (`tic/ExprRewrite.{h,cpp}`). Functional tests in
`testcases/fn_test*.dms` (value-checked positive suite incl. structured/composite params,
designation, imports, multi-name; arity + strict-scope negative tests). NOT yet in P0:
definition-time body checking (bodies stay template-inert; checks run per
instantiation) — moved to P1 alongside the applicative DC, since both need the meta
pass over function bodies. Composite-typed parameters bind as plain items (a benign
class warning appears when binding a unit argument); member checking follows with P2
signatures.

Remaining design detail for the original P0 scope:

1. **stx**: `strlit<> FUNCTION("function")` as an `itemSignature` alternative,
   completing the reserved `SignatureType::Function` stub; `CreateFunction` =
   `CreateContainer` + `SetIsFunction()` (pattern: `CreateTemplate`,
   `stx/dll/src/ConfigProd.cpp:306-310`). Typed parameter telescope + result
   annotation grammar. **Name validation**: reject function names colliding with
   registered operator names *or RewriteExpr.lsp rule heads* — rewriting runs at meta
   time *before* head dispatch (§6), so a function named `sqr` would be rewritten into
   `mul` before item lookup. (Transitional: retires with the .lsp file, §8.4.)
2. **tic — flags**: `TSF_IsFunction` + `TSF_InFunction` (pattern of
   TSF_IsTemplate/TSF_InTemplate, `tic/TreeItemFlags.h:71-72`). Crucial difference:
   `SetInTemplate` does three things — flag, `SetPassor()`, calculator suppression
   (`tic/TreeItem.cpp:1330-1340`) — while `SetIsFunction` must set only the flag so the
   body stays meta-evaluable.
3. **tic — definition-time check = the ordinary meta pass; no new engine.**
   Parameters are their own placeholders: they are real typed items, and body
   references resolve to them as `sourceDescr` leaves (slSupplierExprImpl → `SymbDC`).
   Running `UpdateMetaInfo()` + `UpdateDC()` over the body drives `MakeResult` on the
   DC network with `mustCalc = false`, which executes unit unification
   (`UnifyDomain(…, UM_Throw)`, `clc/dll/include/OperAttrBin.h:52-65`), overload
   resolution (`FindOper`), the unit-creator metric algebra, and declared-vs-computed
   checks (`TreeItem_CreateConvertedExpr`, `tic/TreeItem.cpp:2513-2538`) — once, at
   the definition. The work is *flag surgery* over the ~47 `InTemplate()` call sites;
   load-bearing ones: `tic/TreeItem.cpp:856` (allow calculators), `:2564` (allow
   UpdateDC), `:3212` (supplier visits), `tic/AbstrCalculator.cpp:918-922` (outside
   references into a body still throw), `tic/DataLocks.cpp:102` (data locks on bodies
   stay impossible — clean failure, not assert). Check lazily on first use with a
   once-guard (R8).
4. **tic — call dispatch**: keep the existing fallback (unknown head → `theTemplGroup`
   → `FindOrVisitItem`) and dispatch on `IsTemplate()` vs `IsFunction()`. Do **not**
   register user functions in the global operator registry (they are config-scoped and
   shadowable; the registry is global/static). Check call arity == declared parameter
   count.
5. **Closed scope**: function definition and instance roots always own a `UsingCache`
   (own subitems + declared imports only) — no ancestor walk, no parentless hole
   (§4.6).
6. **Tests** (tst fixtures): definition-time failures (domain mismatch in body, wrong
   arity, missing `result`, telescope order violation, name collision with `sqr` /
   operator names), acceptance cases, template regression cases.

### P1 — applicative application + nested calls + `inline`

**Implementation status (v20.9.0): the inline half of P1 is IMPLEMENTED.** Function
applications used as *expressions* — nested inside larger expressions, or bound to a
typed holder (attribute/parameter/unit) — are meta-time β-reduced into self-contained
key expressions by a dedicated body resolver (`FunctionApplication` in
`tic/AbstrCalculator.cpp`, dispatched from `SubstituteExpr`/`SubstituteExpr_impl`):
parameter references become the substituted argument keys, body items reduce
recursively (memoized, cycle-guarded), member access through structured/composite
parameters resolves against the actual argument item, imports/externals resolve through
the function's strict search space (function definitions also get a strict UsingCache
with hidden-parent `using`-url resolution). Binding a call to a **container** keeps the
P0 instantiating form (access to all instance items) — a holder-driven rule that §5.9's
explicit `apply`/`instantiate` forms supersede. This *is* the "pure inlining"
alternative from the design (§8.3's `inline` behavior, applied as the default for
expression positions): identical applications intern to identical keys, so applicative
identity — including unit-returning functions unifying across call sites — comes free,
with **no new DC class and no R5 teardown risk**. Verified by
`testcases/fn_test_p1.dms` (nested calls, direct typed calls, function-calling-function
via `using = /lib`, unit-returning select function, cross-binding domain unification,
container-bound calls with imports equal to the definition parent, nested body
containers with sibling references, structured parameters with multi-name member
declarations, functions defined inside template bodies) and the recursion negative
test. An adversarial multi-agent review of the implementation confirmed and led to
fixes for: import loss on container-bound instantiation when the import equals the
definition parent/ancestor; body-symbol resolution now walks from the referencing
item's scope outward (nearest-scope, matching instantiated semantics) instead of only
the function root; recursion detection follows the nested-application parent chain
(not a thread-global stack), so re-entry through `UpdateMetaInfo` of externals cannot
raise false positives; parameter-member lookups descend into the argument only
(`FindSubItem`) instead of `FindItem`'s ancestor walk; dot-relative and absolute paths
are rejected inside bodies; arity counting reads the declaration before its member
block; function definitions copied inside larger subtrees keep flag/spec/strict scope;
the hidden-parent `using`-url fallback is gated so BUSY-window identifier lookups
cannot escape strict scope; body-resolved externals register as suppliers of the
calling item. Not yet: the `applyF` boundary form below (error
attribution, memoization of very large bodies — R6/R7), definition-time checking,
`arrow`/`scope`/`subitem` constructs inside inlined bodies (currently rejected with a
pointer to the container form), and function-typed parameters (P3). The original P1
plan below remains the design for the boundary-keyed variant:

1. **Key form**: new `token::applyF`; key `(applyF "/path/F" <variantTok> argKey_1 …
   argKey_n)` — the F-path keeps keys small; body edits invalidate through supplier
   visitation (function item + subtree are suppliers).
2. **β-reduction helper**: assoc { param `sourceDescr` leaf → substituted argument
   key }; result expression = `AssocList::ApplyOnce(result->GetCheckedKeyExpr())`;
   recursion rejected via a visiting-set cycle guard (precedent
   `tic/TreeItem.cpp:2474-2477`).
3. **FuncApplDC** (new DC subclass; factory `CreateDC`,
   `tic/DataController.cpp:339-387`):
   - attribute/unit results: forward to `GetOrCreateDataController(reducedExpr)` —
     result-DC chaining exists (`tic/DataController.cpp:489-495`); precedent `unique`;
   - container results: `CreateCacheRoot()` + per-subitem mirrors carrying
     pre-substituted calculators (`ConstructFromLispRef(item, expr,
     CalcRole::Calculator)`) — the `PhaseContainer` pattern; member access via
     `slSubItemCall`; GUI browsing free via endogenous shadow copies.
4. **`inline` attribute** (§8.3): inline applications skip the FuncApplDC entirely and
   emit the fully β-reduced expression as the buffered sub-expression — identity-
   preserving replacement for category-A rewrite rules; default remains the `applyF`
   boundary.
5. **Nested calls**: in `SubstituteExpr_impl`, replace the template throw
   (`tic/AbstrCalculator.cpp:1375-1396`) for functions: substitute the arguments in
   caller scope (existing `SubstituteArgs`), emit the applyF key (or the inlined
   expression) as the buffered sub-expression — precedent: `CanResultToConfigItem`
   operators already resolve to items mid-substitution
   (`tic/AbstrCalculator.cpp:1416-1433`). Root-level calls return MetaInfo index 1
   (LispRef) instead of `MetaFuncCurry`.
6. Keep P0 copy-instantiation available behind a config flag for one release, for A/B
   validation of results.

### P2 — polymorphism, overloading, declarative signatures

**Implementation status (v20.9.0): value-type polymorphism is IMPLEMENTED.**
`function f<V: numerics>(unit<uint32> D; attribute<V> x (D)) -> attribute<V> (D)` —
class-constrained type variables over the closed 𝕍 universe. Constraint vocabulary:
`any, numerics, integers, floats, uints/unsigned_ints, sints/signed_ints, domains,
points, domain_points, signed_domain_points, unsigned_domain_points`
(`MatchesGenericConstraint` in `tic/TreeItem.cpp`, built on ValueClass predicates;
`domain_points` = 2-dimensional, single composition, countable coordinates — the
point types eligible as grid domains; `signed_domain_points` / `unsigned_domain_points`
partition it by the signedness of the COORDINATE type — spoint/ipoint vs wpoint/upoint.
Signedness is not a property of the point value class itself, since `is_signed<Point<T>>`
is false for every T, so those two consult `GetScalarClass()->IsSigned()`, exactly as
`IsCountable()` consults the scalar class for integrality. Only the signed ones can carry
a negative cell offset, which is what a neighbourhood displacement such as
`add_or_null(id(Dom), Value(point_xy(-1s, -1s), Dom))` needs — see ObjectVision/GeoDMS#1163).
`attribute<V>`/`parameter<V>` positions record the variable per parameter in the
function spec; `unit<V>` parameters (and `-> unit<V>` results) become plain binders
whose types follow from the arguments. Checking happens at application time in the
reducer: each generic argument's value class is derived from its key expression's
DataController result, checked against the constraint, and checked for *consistency*
per variable across parameters — with dedicated diagnostics ("does not satisfy
'V: numerics'", "inconsistent instantiation of type variable 'V': Float64 (parameter
'a') vs UInt32 (parameter 'b')"). Since the reducer is type-agnostic, one definition
serves every instantiation (no per-value-type expansion needed); generic *locals* in
bodies come free. Also implemented: **dependent-position checks in declared function
signatures** (closing the §5.8 gap): domain/values references that name a parameter
must name the positionally same parameter on both sides (alpha-invariant, `#j`
normalization), other references compare literally, and value compositions must
match. Verified by `testcases/fn_test_p2.dms` (+2 negatives).

Remaining P2 work: `variant` blocks with specificity ordering and definition-time
disjointness (§5.7); the declarative operator-signature interface (the ClassCPtr array
⊕ UnitCreator ⊕ domain-unify pattern, presented by each operator through a virtual —
designed in `operator-signature-interface.md`, which also covers the signature browser
and the printable-signature machinery it needs). Variadic `switch` and
optional-argument adoption unlock retirement of rewrite categories B/C/E (§8.2).
Type-variable clauses on signature aliases are not yet parsed (function declarations
only).

### P3 — higher order: remaining work packages (implementation-ready, see §15 for conventions)

The P3 core (function-valued parameters) is implemented. The remaining P3 work,
specified for direct implementation:

**WP3.1 — Partial application — IMPLEMENTED in 20.9.0.**
`F(a, _)` in any argument position yields a function value with `a` bound and one hole.
A function argument (and a function-valued parameter's binding) is carried as a
`std::shared_ptr<FunctionBinding>` — `{ SharedTreeItem funcItem; std::vector<CallArg>
slots; }` where a `CallArg` is one of {data key + plain-ref item, nested binding, hole}
and a slot with `isHole` is unbound (`tic/AbstrCalculator.cpp`, anonymous namespace).
A plain function reference is `MakeAllHoles` (every slot a hole). `_` is a plain symbol
(`t_Hole = GetTokenID_st("_")`, unaffected by RewriteExpr — `MakeVarsOfUnderscores`
only runs on rule loading). Argument resolution is unified through `ResolveCallerArg`
(caller side, with `resolveData`/`findItem` callbacks) and
`FunctionApplication::ResolveBodyArg` (body side): a function-head call whose arguments
contain a hole `MergeBinding`s into a partial binding; a full one is `ReduceMerged` to
a data key. `MergeBinding` fills holes left-to-right and arity-checks against the number
of holes. A residual (partial) binding may only be passed as an argument — binding one
to an item errors ("a partial application can only be passed as an argument"). Signature
checking (`CheckFunctionSignature`) runs the full structural check on plain bindings and
a residual-arity check on partial ones. Verified by `testcases/fn_test_partial.dms`
(partial passed to a higher-order applier, applied twice: `Scale(Road,3.0,_)` → ×9;
`Add2(Road,_,_)` → doubler) and a negative test.

**WP3.2 — Lambdas (optional; machinery notes).**
Named functions + partial application cover the practical space; full lambdas are
optional. If wanted: represent as `(lambda (formals…) bodyExpr)` LispRef terms with
formals as ChroID variables (`LispRef(token, chroId)`; `SymbObj::IsVar`,
`sym/LispRef.cpp`). **Alpha-renaming is a solved problem in this codebase**: the
Prolog processor (`sym/Prolog.cpp`, re-included, working) contains
`Renum(LispPtr, TTimeStamp)` (line ~59): stamps every variable in a term with a
chromosome — hash-consed, O(size). Use the `Solve` pattern (Prolog.cpp:196-208): a
monotonically increasing chromosome counter; at each β-step, `Renum` the lambda term
with a fresh chromosome, then substitute via `Add(assoc, Assoc(var, argKey))` +
`AssocList::ApplyOnce`. β-reduction runs in C++ at the function-application step —
NOT through `ApplyTopEnv`/RewriteExpr.lsp (global memo cache keyed on interned
pointers, no binder concept). Enforce the §5.6 erasure post-condition: scan the final
reduced expression for residual `IsVar()` leaves / `lambda` heads before any
`GetOrCreateDataController` call. There is a second, assoc-threading
`Renum(assocList, expr, int& nextFreeVarNum)` in `sym/Lispeval.cpp` (~line 670, used
by the disabled evaluator) if per-variable renaming maps are needed. R10 (capture,
memo confusion, self-aliasing) is addressed exactly by fresh-chromosome renaming.

**WP3.3 — Typed `map` over containers — IMPLEMENTED in 20.9.0.**
`container out := map(F, src);` creates one child per data-item / unit child of `src`,
each computed as `F(child)`. `map` is a reserved metafunction head
(`t_Map = GetTokenID_st("map")`); it is intercepted in the root
`AbstrCalculator::SubstituteExpr` `og->IsTemplateCall()` branch (before item lookup),
requires a container holder, registers the function and source as suppliers, and
returns `MetaFuncCurry{ isMapCall = true }`. `MetaFuncCurry::operator()` dispatches to
`InstantiateMap` (next to `InstantiateTemplate`): F must be a one-parameter function;
for each data-item/unit child `c` of the source it reduces `F(c)` via a
`FunctionApplication` (argKey = `c->GetCheckedKeyExpr()`, argItem = `c`) and creates a
child of the holder named `c->GetID()` with `SetCalculator(ConstructFromLispRef(child,
reducedKey, CalcRole::Calculator))` — a plain holder child that follows the reduced
data result. Nested/sub-expression use is rejected (map yields a container).
Generic mapped functions work (the per-child value class is derived and constraint-
checked in the reduction). Verified by `testcases/fn_test_map.dms` (a `Halve` function
and a generic `DoubleG` mapped over a two-attribute container). Not yet: partial-
application F (`map(Scale(k,_), src)`), `filter`/`fold` (same pattern), and `for_each`
deprecation.

**WP3.4 — Definition-time checking — IMPLEMENTED in 20.9.0.**
A function body is scope- and shape-checked once, at its first *reference*, instead of
only failing inside the reduction. Implemented as a standalone, argument-independent
walker (`FunctionChecker` in `tic/AbstrCalculator.cpp`) rather than a check-only
reduction mode, so it cannot destabilise the working reducer: it walks the body
reachable from the designated result and validates that every identifier resolves
(parameter / local / import — nearest-scope, same rules as the reducer), that
operator/function heads are known, that dot-relative/absolute refs and
arrow/scope/subitem/leading-`=` are rejected, and that *direct* function calls have the
right arity. It deliberately skips everything argument-dependent: types, units,
metrics, generic constraints, member existence, and the application of function-valued
parameters (all still verified per application by the reduction). Value-type heads
(`float64(x)`) are recognised as conversions, not calls. Triggered from the inline
dispatch (`SubstituteExpr_impl` function branch) via `CheckFunctionDefinition`, guarded
by a `definitionChecked` flag in `FunctionSpecData` so it runs once; a failure re-runs
next time (consistent errors) while success caches. Verified: an unresolved-identifier
body errors with function-level attribution (`unknown identifier in body of function
'/Bad'`); no false positives across the generic/member/HOF/map test suite.
**Upgraded by WP4.1 tranche 3 (2026-07-14)**: the walker now performs full
definition-time TYPE checking with rigid variables — see the §5.10 status block —
and is additionally triggered at every application entry (`ReduceValue`), covering
closures, prelude functions and variant members. **Never-referenced functions** are
covered by the opt-in `@checkfunctions` audit (§5.16, 2026-07-22). Typed operator
positions are handled by the declarative signature interface
(`operator-signature-interface.md`), which superseded the interim `OperSigKind`
registry.

### P4 — unifier, boundary keys, refinements, rewrite end-state (implementation-ready)

**WP4.1 — TypeSpec unifier on Robinson unification. Core + definition-time walker
IMPLEMENTED in 20.9.0 (tranches 1-3, §5.10 status); operator signatures remain —
interim registry shipped, successor designed in `operator-signature-interface.md`.**
Application-time checking runs on a dedicated union-find store (`TypeUnifier`,
`tic/AbstrCalculator.cpp`) over value-class and domain variables, with
variable-variable links, constraint-set intersection (§5.7 v2 acceptance-set
bitsets), concrete bindings via value-class identity resp. `UnifyDomain`, and
per-source attribution — a Robinson unifier specialized to §5's shallow type terms,
where the occurs check is trivially satisfied (concrete units are opaque key-identity
atoms; record/table self-reference is a binder, §5.10). Variant selection shipped
separately on the same acceptance-set mechanism (§5.7 v2 specificity), not on
unification. The Prolog machinery (`UnifyRobinson`, `sym/Prolog.cpp:100-133`,
occurs-check `Occur` FIXED to `||` in 20.9.0; `Renum` :59) remains the design anchor
for the *remaining* consumers, where terms become genuinely recursive: encode
operator signatures and inferred body types as LispRef TypeSpecs (§7) with ChroID
variables; per candidate, `Renum` with a fresh chromosome and unify against the
argument TypeSpec vector; constraint variables verify via `MatchesGenericConstraint`
(or intersect acceptance sets, as the store already does). Composite parameter types
(tables/records, e.g. a network with `node_rel`s and an impedance attribute) unify
field-wise with width subtyping (§II.4). Operator signature reification stays
mechanical: ClassCPtr array ⊕ UnitCreator name ⊕ domain-unify pattern (§6) generated
per registered `Operator`, opt-in per group — unsignatured operators defer to
per-application checking, so adoption is gradual. The consumer is already in place:
the tranche-3 typed walker types operator positions as Unknown today; reified
signatures drop into `InferExpr`'s operator branch, upgrading definition-time
inference body-wide and enabling auto type derivation (`a := mul(b, c)`).

**WP4.2 — `applyF` boundary keys (R6/R7, opt-in).** Inline reduction stays the
default (it preserves rewrite-rule key identity, §8.3). For very large bodies or
better error attribution, add per-function opt-out: property `inline = "false"` on
the function item → applications produce `(applyF "/path/F" argKey_1 …)` with a new
`token::applyF` and a `FuncApplDC` whose `MakeResult` forwards to
`GetOrCreateDataController(reducedExpr)` (result-DC chaining exists,
`tic/DataController.cpp` `CallCalcResult`). Container-shaped results keep the
copy-instantiating form; do NOT attempt per-member DC trees (risk R5) until a
concrete need arises.

**WP4.3 — Refinements as checked aliases — IMPLEMENTED in 20.9.0.** A plain-type alias
may carry a refinement clause:
`frac = parameter<float64>, IntegrityCheck = "frac >= 0.0 && frac <= 1.0";`. The
`aliasPlain` grammar now accepts `, itemProp` after the type; the exemplar stores the
property. When an item is declared with the alias (`good: frac := 0.75;`),
`ConfigProd::CloneAliasRefinement` copies the exemplar's `IntegrityCheck` onto the new
item, rebinding the reference to the value by whole-identifier substitution of the alias
name with the item's name (`frac >= 0.0` → `good >= 0.0` /
`SubstituteWholeIdentifier`). Alias-of-refined-alias inherits and re-rebinds. The check
is an ordinary IntegrityCheck (lazy, data-stage — §2 staging). No new checking
machinery. Verified: `good`/`good2` (via `ratio = frac`) pass; `bad: frac := 1.5` fails
`'bad >= 0.0 && bad <= 1.0' is not true` (`testcases/fn_test_refine{,_neg}.dms`). A
`range = [lo, hi]` sugar generating the check string can follow.

**WP4.4 — Metric constraints via aliases — VERIFIED in 20.9.0 (no code change).**
An alias whose exemplar references a concrete values *unit* pins the metric: the cloned
values-unit token re-resolves at the use site and `UnifyValues` enforces metric
equality at calc time. Verified by `testcases/fn_test_metric.dms` —
`length = parameter<meter>; duration = parameter<second>;` (with
`meter := BaseUnit('m', float64)`): two `length` values add (same metric), and
`length + duration` errors with *"Arguments must have compatible units, but arg1 has
Metric m and arg2 has Metric s"*. This is the idiom; remaining work is only *surfacing*
(printable signatures include the metric; WP4.1's TypeSpecs carry it as `metric(μ)`
terms).

**WP4.5 — RewriteExpr.lsp retirement, first tranche.** Prerequisites are now in
place: inline reduction reproduces rule keys byte-identically (a prelude
`function sqr<V: numerics>(unit<uint32> D; attribute<V> x (D)) -> attribute<V> (D) := x * x;`
reduces `sqr(a)` to `(mul aKey aKey)` — the same interned key the rewrite rule
produces today, so cache identity and cross-spelling unit unification are preserved).
*Steps:* (1) ship `res/prelude.dms` with the definitional rules of §8.2 category A
that are pure compositions (predicate family `*_or_*_null`, `order`, `isOverlapping`,
`neighbourhood`, `float/point_isNearby`, `median`, `plogp`, `sqr`, `rescale`,
`normalize`, `distribute`, `scalesum`); (2) configs opt in via
`#include <%exeDir%/prelude.dms>` (auto-inclusion can follow once trusted);
(3) delete each rule only after a key-identity regression (two spellings of a unit
expression must still unify) passes with the prelude replacement; (4) `claim_*` rules
move to RuimteScanner configs; (5) categories B/C/E wait for variadic/optional-arg
operator work; category D stays engine-side (§8.4).

## 11. Honest limits of definition-time checking

- **`calc_always` / dynamic-arg-policy operators** whose *meta* pass reads argument
  values (subitem names, `BaseUnit` metric strings, `range` specs, for_each specs): if
  such an argument depends on a formal, that sub-DC cannot be evaluated at definition.
  Mark it *deferred*; it re-checks per application automatically (the application's own
  `MakeResult` runs it). Emit a definition-time warning listing deferred spots.
  *Status 2026-07-20*: the CLOSED complement is now processed at definition scan — the
  impedance spec string and the for_each name arrays (storage-backed included) are
  evaluated when they do not depend on the formals, per the §12.7 ruling and both
  shipped tranches (operator-signature-interface.md §12.7); only genuinely
  formal-dependent meta arguments still defer.
- **Metric/projection constraints**: a formal `unit<float64>` has an empty metric at
  definition; operators that constrain metrics re-check per application. Definition
  checking is sound for kinds/domains/arity — not for metrics (P4/v2: declared metric
  constraints on parameters). Result *units* are always derived per application
  (§5.7), which is a feature, not a gap. **Note the deferral is *vacuity*, not
  intractability** — the metric algebra is a decidable free abelian group (`SetProduct`/
  `SetQuotient` are invertible; Kennedy units-of-measure), so metrics defer only because
  a formal has no *declarable* metric to constrain, not because metric unification is hard.
  See `operator-signature-interface.md` §20 for the full free/interpreted/staged-fragment
  account and the staging contract (definition = placeholders + structural checks;
  instantiation = full concrete `CreateResult` semantics).
  **RULING (Maarten, 2026-07-29): metric compatibility STAYS an instantiation-time
  concern — permanently, by design.** Do not build declared-metric syntax, definition-time
  metric unification, or any "fuss at definition time" for metrics. This closes the
  P4/v2 "declared metric constraints" work package and everything gated on it
  (`unit_creator_spec`/`uc_*` factories, WP4.4 metric surfacing beyond cosmetics); the
  §9 SigUnitChecker's metric checks stay log-only for the same reason.
- **Tier-2 `SelectMetaOperator`s on formal arguments** (`select_with_attr_by_cond`,
  `select_with_org_rel_with_attr_by_cond`, `collect_attr_by_*` & co): their member set
  comes from a container argument that is usually a *formal*, so their composite results
  are only evaluated at the specific instantiation — the attributes are unknown to the
  function definition. **RULING (Maarten, 2026-07-29): this is ACCEPTED, not a gap** —
  per-instantiation evaluation of these meta selections is fine; no definition-time
  typing effort is warranted. These operators are borderline deprecation-candidates
  *because* of such troubles; as long as they remain harmless (defer cleanly, no false
  definition-time errors), leave them be and do not invest further. The Tier-2
  composite-result typing work package is closed accordingly.
- **Counts, ranges, tiles**: inherently data-stage; never part of static checking.
- **Leading-`=` string-eval** is incompatible with once-checking unless the indirection
  string is parameter-independent — restrict it inside function bodies.
- **Storage properties inside bodies: forbidden in P0/P1** (`HasStorageManager` asserts
  `!InTemplate` today, `tic/TreeItem.cpp:4528`; file state vs DC keys needs its own
  design later — templates remain the vehicle for parameterized imports meanwhile).
- **Recursion: rejected** (cycle guard); iteration stays with bounded combinators.

## 12. Risk register

| # | risk | severity | mitigation |
|---|---|---|---|
| R1 | storage-bearing items in bodies (file state vs DC keys, write-lock fights) | High | forbid in P0/P1; revisit with a read-only source design |
| R2 | leading-`=` string-eval in bodies breaks once-checking | High | allow only parameter-independent indirections; else reject |
| R5 | interest/teardown of a *tree* of chained DCs (only single result-chaining exists today) | High (P1 containers) | prototype PhaseContainer-style (apply-DC moves data itself) before per-member DC chaining; teardown tests |
| R6 | error attribution inside β-reduced keys | Med | keep the applyF call boundary in keys as the attribution anchor (non-inline default); `mc_OrgItem` back-links on mirrors (same mechanism as template copies) |
| R7 | key/memo blowup for large bodies | Med | measure; `inline` only where identity demands it; fallback = per-member keys via `slSubItemCall` instead of inlining |
| R8 | definition-check cost at config load (meta thread) | Med | check lazily on first use, once-guard |
| R9 | recursion diverges at reduction | Med | cycle guard → clean error |
| R10 | alpha-capture in P3 HOF (no renaming exists; memo caches key on interned pointers) | High (P3) | fresh ChroID per β-step; assert no residual variables reach the DC layer |
| R11 | ~~CalcCache persistence of~~ the new `applyF` key head | Low | Largely moot: the CalcCache was retired with the 8.0 series (#1189), so no key outlives a session and a new head cannot miss an old cache. What remains is in-session DC identity in `s_DcMap` and cross-spelling unit unification — see R13. |
| R12 | GUI predicates test `InTemplate` for badges/eval-suppression; `InFunction` items now carry DCs | Med | treat `InFunction` as template-like in all GUI data-request paths |
| R13 | retiring a rewrite rule (or toggling a function's `inline`-ness) changes expression canonicalization → previously-unifying units stop unifying across spellings (the CalcCache-miss half of this risk lapsed with the cache itself, #1189; unification is now the whole risk, and it is the serious half) | High (transition) | `inline` prelude functions reproduce today's keys byte-identically; retire per-rule with a stated key impact; regression configs asserting cross-spelling unit unification |
| R14 | overload resolution surprise (order dependence, ambiguity) | Med | definition-time pairwise disjointness / strict specificity over the closed 𝕍 universe; ambiguity is an error; never registration-order semantics (unlike `FindOper`'s first-match) |
| R15 | CRS/key restructuring (§4.9): new `(BaseUnit (SRef …))` head + background refs leaving identity → dd-only mismatches stop being unify errors (the CalcCache-miss half lapsed with the cache, #1189) | Med (transition) | regression configs asserting CRS mismatch *stays* an error while background-only mismatch stops being one |

Unverified, needs a prototype: the meta pass over calculator-bearing but data-less
parameter items completing without tripping residual asserts; teardown behavior of
chained subtree DCs (R5). (The CalcCache round-trip for new key heads is no longer on
this list: there is no persistent cache to round-trip through — see R11.)

## 13. Prior art

- **Kennedy, units of measure (F#)** — 𝕄 as a type-level abelian group; GeoDMS already
  implements the algebra, this design surfaces it in signatures.
- **Dependent ML / index refinements (Xi & Pfenning)** — `range(a,b)` as decidable
  refinements rather than full dependency.
- **OCaml applicative functors** — same arguments, same abstract types: the semantics
  chosen for function application (§4.5), already latent in the DC cache.
- **Dex / Futhark typed index sets** — `V[D]` with domains as type-level index sets is
  precisely the typed-array-language reading of attributes.
- **Relational width subtyping / row polymorphism** — table parameters (§4.4).
- **Haskell type classes vs. C++ overloading** — the two poles for §5.7; the design
  takes class-constrained polymorphism for uniform bodies and closed, disjointness-
  checked variant sets for heterogeneous ones, avoiding open ad-hoc overloading.

## 14. Summary

GeoDMS's implicit type system is a two-stage, dependently-typed record calculus over a
fixed first-order data algebra, with dimensional types — and most of its checker
already exists: unit unification *is* nominal-by-defining-expression (hash-consed
LispRef keys), broadcast *is* the single implicit coercion, the metric algebra *is*
Kennedy's, and `MakeResult` with `mustCalc=false` *is* a typechecker. The corrections
that matter: ranges are refinements, not identities; containers are telescopes; tables
are shared-domain records; exactly one coercion; applicative application; strict
args-plus-imports scoping. The `function` construct then needs surprisingly little new
machinery — a keyword completing an existing enum stub, two flags that do *less* than
the template flags, flag surgery to let the ordinary meta pass run at definition time,
and one new DataController subclass keyed by an `applyF` expression. Typed functions
with derived result types and disjointness-checked overloads then subsume the
definitional half of RewriteExpr.lsp (`inline` functions preserving today's cache and
unit identities), variadic/optional-argument operator features absorb its
normalizations, a compiled-in typed pass keeps its simplifications — and higher-order
functions arrive as a meta-stage-only extension that the tiled engine never sees.

## 15. Implementation handoff (state as of v20.9.0)

For the implementer of the §10 P3/P4 work packages. Everything below is committed on
`refactor_ownership` (`e6df2a3b`, `4b858327`, `7acfe341`, `73058e5e` + constraint
extension).

### 15.1 Inventory — what exists and where

- **stx/dll/src/ConfigParse.cpp** (Spirit classic `config_grammar`): rules
  `functionDecl`, `functionParamItem`, `functionResultType`, `functionResultSpec`,
  `typeParamsClause`, `aliasFunctionSig`, `aliasPlain`, `colonHeading` (name:type +
  multi-name declarations). `itemDecl = (itemHeading | colonHeading) >> ...`;
  `item = functionDecl | aliasFunctionSig | aliasPlain | itemDecl... | #include`.
- **stx/dll/src/ConfigProd.{h,cpp}**: `FuncProdState` stack (paramCount,
  signatureOnly, inParamList, result-signature capture, resultExpr, paramSigs,
  typeVars, genericParams); `OnFunctionHeading/SigHeading/DeclEnd`,
  `OnFunctionParamDecl`, `OnFunctionResultSig/Expr`, `DoFunctionUsing`,
  `DoColonItemHeading`, `DoAliasDecl`, `DoRefTypeSignature` (type-by-example cloning),
  `ResolveTypeRef` (parse-time, parent-chain only), `ConsumeGenericParamMarker`,
  `OnTypeVarName/Constraint`. Aliases create inert exemplars (`SetIsTemplate`).
- **rtc/dll/src/tic/TreeItem.{h,cpp}**: `TSF_IsFunctionItem` (0x01000000),
  `SetIsFunction` (= `SetIsTemplate` + flag), `s_FunctionSpecAssoc`
  (`FunctionSpecData`: nrParams, resultName, paramSigs (weak exemplars),
  genericParams) with `TreeItem_Set/CopyFunctionSpec`,
  `TreeItem_Get/AddFunctionParamSignature`, `TreeItem_Add/GetFunctionGenericParam`,
  `TreeItem_MakeStrictScope`; `IsKnownGenericConstraint` /
  `MatchesGenericConstraint` (vocabulary: any, numerics, integers, floats,
  uints/unsigned_ints, sints/signed_ints, domains, points, domain_points,
  signed_domain_points, unsigned_domain_points; built on ValueClass predicates —
  the last two via `GetScalarClass()->IsSigned()`, since a point value class is
  never itself signed). `TreeItem::Copy`: function scopes keep ALL imports (skip
  guard bypassed), stream them absolute, `RemoveParentUsing`; nested function copies
  keep flag+spec+strict scope.
- **rtc/dll/src/tic/UsingCache.{h,cpp}**: `RemoveParentUsing` + `m_ParentIsHidden`;
  `FindNamespace(url, mayResolveViaHiddenParent)` — hidden-parent fallback only for
  `using`-url resolution, absolute urls resolve from the tree root.
- **rtc/dll/src/tic/AbstrCalculator.cpp**: the reducer — anonymous-namespace
  `FunctionApplication` { m_FuncItem, m_Parent (recursion chain), m_SubstBuff
  (supplier registration), m_ErrorHolder, m_ArgKeys/m_ArgItems, m_Params,
  m_Reductions/m_InProgress; `Reduce`, `ReduceBodyItem`, `SubstituteBodyExpr(refScope,
  expr)` (nearest-scope), `ResolveBodySymbol` } and `CheckFunctionSignature`
  (classes + value composition + alpha-invariant `#j` domain/values-token
  normalization). Dispatch: `SubstituteExpr` root (typed holder → inline via
  `goto skipTemplInst`; container holder → `MetaFuncCurry` copy-instantiation; arity
  check) and `SubstituteExpr_impl` (function branch before the template throw:
  argument substitution with `metainfo_policy_flags::subst_allowed`, function-valued
  arguments bind without key substitution).
- **sym/Prolog.cpp** (re-included, working): `Renum` (variable stamping by
  chromosome), `UnifyRobinson` (occurs-check; **fix `Occur`'s `&&`→`||` first**),
  `ReduceCaneghem`/`Unify`, `Solve` (fresh-chromosome-per-step pattern).
  `sym/Lispeval.cpp` has a second assoc-threading `Renum` (~line 670) and the
  commented-out applicative evaluator.
- **Tests** (session-local, `scratch/`, gitignored — recreate as needed):
  `fn_test.dms` (P0 forms), `fn_test_p1.dms` (inline/nested/applicative, 9 checks),
  `fn_test_p3.dms` (HOF), `fn_test_sig.dms` (aliases+signatures), `fn_test_p2.dms`
  (generics incl. sints/uints), negatives `fn_test_{arity,scope,p1_rec,p3_neg,
  sig_neg,p2_neg1,p2_neg2}.dms`; committed example: `examples/function.dms`.

### 15.2 Pitfalls (each cost a debugging round — read before editing)

1. **Spirit classic actions fire on partial matches and are NOT undone by
   backtracking.** Attach creation actions to sequences that cannot be a prefix of
   something else (e.g. `(kw >> identifier >> LPAREN)`), and make state-capture
   actions idempotent/re-startable (`StartPendingNames` clears).
2. **`m_strIdentifierID` is a single slot** overwritten by every identifier match —
   capture names into dedicated fields at the earliest action (function name capture
   moved into the identifier action because type-variable clauses overwrite it).
3. **A bare `char` inside `assert_d[...]` breaks Spirit** (`composite.hpp` C2825
   errors) — wrap in `chlit<>`.
4. **Never pass `theTemplGroup` to `SubstituteArgs`**: `TemplOperGroup::GetArgPolicy`
   returns `calc_never` → `subst_never` → sourceDescr trees instead of key
   expressions. Substitute function-call arguments directly with `subst_allowed`.
5. **UsingCache's constructor calls `AddParent`** — every cache implicitly contains
   the tree parent as fallback namespace. Strictness requires `RemoveParentUsing`
   AND forcing the cache into existence (delegation-stops-the-walk only if a cache
   exists).
6. **`TreeItem::Copy`'s namespace loop skips `sns == parent` and ancestors** because
   template instances get the parent injected separately; function scopes get no
   parent → keep all their imports (already fixed; preserve when touching Copy).
7. **Item names collide case-insensitively** (`Nested` vs `nested` → "SubItem already
   defined") — in configs and tests.
8. **Body items are `InTemplate`**: never acquire calculators, `GetCheckedKeyExpr`
   yields stable nominal leaves, `slSupplierExprImpl` throws on references INTO them
   from outside (function-item references are exempted for pass-on positions).
9. **`ExprCalculator::GetLispExprOrg()` parses lazily** — `ConstructFromStr` +
   `GetLispExprOrg` is the side-effect-free parse of a body expression.
10. **No new source files** without editing `*.vcxproj` (msbuild lists files
    explicitly; CMake globs). All function machinery deliberately lives in existing
    files.
11. **Recursion detection must follow the nested-application parent chain**, not a
    thread-global stack — `UpdateMetaInfo` of externals re-enters the reducer for
    unrelated applications (false positives otherwise).
12. **Member lookups through parameters must descend only** (`FindSubItem`);
    `FindItem` walks ancestors and silently binds call-site items.
13. **`static_quick_assoc` values need an `IsDefaultValue` overload** (ADL) when the
    value type has no `operator==` (weak_ptr members).
14. **GeoDmsRun needs absolute config paths**; verify values with
    `IntegrityCheck = "item == expected"` (wrong values then fail the run); expected
    error tests assert exit 1 + the diagnostic text.

### 15.3 Build & verify recipe

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' `
    all22.sln -p:Configuration=Release -p:Platform=x64 -m
& bin\Release\x64\GeoDmsRun.exe /L<abs>\run.log <abs>\config.dms /checks
```

Always build the whole solution (VS18 msbuild, toolset v145 — see AGENTS.md); after
any change to the function machinery, re-run the full battery of §15.1's test configs
plus a classic-template regression (definition-scope reference through the implicit
namespace must keep working) before committing.
