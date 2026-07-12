# GeoDMS as a typed higher-order function language — design

*Status: design proposal, 2026-07-11. No code changes; the P0 section is written to be
directly actionable as a follow-up task. All file:line references were verified against
the current `refactor_ownership` tree.*

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

1. **Function scoping is strict args-only by default, with explicit `using` imports**
   (§4.6) — stricter than templates, with the closure visible in the function header.
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

### 4.6 Scoping: strict args-only by default, explicit imports *(decided; implemented in 20.9.0)*

Precision matters here — implementation corrected an earlier overstatement. Template
instance roots receive an injected `using` namespace pointing at the template's
definition parent (`TreeItem::Copy`, `tic/TreeItem.cpp:2130-2166`), and name resolution
*delegates-and-stops* at any level owning a `UsingCache` (`FindTreeItemByID`,
`tic/TreeItem.cpp:4573-4590`) — **but the UsingCache constructor implicitly adds the
context's tree parent as a namespace** (`AddParent`, `tic/UsingCache.cpp:113-123,
147-152`). For a template instance that tree parent is the *call-site* container: the
injected definition namespace takes precedence, yet unshadowed call-site names remain
reachable as a fallback. Template bodies are therefore definition-scoped *with
call-site fallback*, not hygienic.

Functions go strictly further (implemented: `UsingCache::RemoveParentUsing` +
forced cache existence on function instance roots; absolute `using` paths resolve from
the configuration root, `UsingCache::FindNamespace`):

- a body sees (a) its formal parameters, (b) its own local items, and (c) **only
  namespaces it explicitly imports** via the existing `using` property, resolved
  against the definition scope and frozen to absolute paths;
- mechanism: a function definition (and any instance root) *always* owns a
  `UsingCache` = own subitems + declared imports. Since resolution stops at a
  `UsingCache`, the ancestor walk never happens, and the parentless-capture hole
  disappears by construction.

Consequences: the closure is visible in the function header (auditable); bodies
typecheck in complete isolation; shared units (`/geography/…`) are imported once per
function instead of threaded through every call. Call-site items remain invisible in
all cases — the only channels from the caller are the argument expressions, which are
(correctly) resolved in the *caller's* scope, exactly as template arguments are today
(`ArgCalc` grandparent search context, `tic/AbstrCalculator.cpp:736-747`).

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

P0 call sites instantiate like templates (container holder + result access):

```
container cr := CongestionRatio(Road);
attribute<float64> congestion (Road) := cr/result;
```

P1 makes `F(args)` an expression of the result type, nestable (§10).

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
| scoping | definition scope takes precedence via injected `using`, but the implicit parent namespace keeps call-site names reachable as fallback (§4.6) | closed: formals + locals + explicit `using` imports only; parent namespace removed, boundary total |
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

### 5.5 Higher-order functions (P3)

Function-typed parameters and function-valued expressions:

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

2. **Variants** for genuinely type-dependent behavior — one function item containing
   named variant blocks, each with its own telescope and body:

   ```
   function dist
   {
       variant f32 (unit<uint32> D; attribute<fpoint> a (D); attribute<fpoint> b (D))
       :   attribute<float32> (D)  { … }
       variant f64 (unit<uint32> D; attribute<dpoint> a (D); attribute<dpoint> b (D))
       :   attribute<float64> (D)  { … }
   }
   ```

   Variants live *inside one item* deliberately: TreeItem containers require unique
   subitem names, so same-name sibling functions are not representable — and keeping
   the overload set in one place makes it checkable as a whole. This mirrors how one
   OperatorGroup holds many Operators.

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
| **E. Argument completion / bridging** | `Value→convert` (24); `const` reorder (25); `ReadValue→ReadArray` default (165); `collect_by_cond` per-`select_*` arg injection (29-42); property accessors `name/Descr/…→PropValue` (209-215); `BaseUnit` fixups (116-119) | ~15 | **optional/default arguments** (`m_NrOptionalArgs` exists) + **typed overloads** (§5.7) — `collect_by_cond` dispatching on the select flavor is precisely overload resolution on a Σ-result type |

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
`scratch/fn_test*.dms` (value-checked positive suite incl. structured/composite params,
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
P0 instantiating form (access to all instance items). This *is* the "pure inlining"
alternative from the design (§8.3's `inline` behavior, applied as the default for
expression positions): identical applications intern to identical keys, so applicative
identity — including unit-returning functions unifying across call sites — comes free,
with **no new DC class and no R5 teardown risk**. Verified by
`scratch/fn_test_p1.dms` (nested calls, direct typed calls, function-calling-function
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

Class-constrained parameters (∀V over typelist classes) with definition-time checking
per finite class instantiation; `variant` blocks with specificity ordering and
definition-time disjointness (§5.7); reify built-in operator signatures (ClassCPtr
array ⊕ UnitCreator name ⊕ domain-unify pattern) into TypeSpecs; signature browser.
Variadic `switch` and optional-argument adoption unlock retirement of rewrite
categories B/C/E (§8.2).

### P3 — higher order

`lambda` terms with ChroID variables; β-reduction in C++ at the function-application
step (NOT through `ApplyTopEnv`/RewriteExpr.lsp — that engine has a global memo cache
and no binder concept); fresh-ChroID alpha-renaming per reduction step; the §5.6
post-condition scan. Typed `map`/`filter` over containers; deprecate `for_each`.
P1 stays strictly first-order and capture-free by construction (its "variables" are
absolute config paths).

### P4 — refinements and rewrite end-state

`range(a,b)` refinement checks unified with IntegrityChecks (lazy, data-stage);
metric-aware printable signatures; optionally declared metric constraints on
parameters. RewriteExpr.lsp reaches empty and is removed together with the
rule-head name validation; the compiled-in typed simplifier (category D) remains as the
only rewriting, now running with typing information (§8.4).

## 11. Honest limits of definition-time checking

- **`calc_always` / dynamic-arg-policy operators** whose *meta* pass reads argument
  values (subitem names, `BaseUnit` metric strings, `range` specs, for_each specs): if
  such an argument depends on a formal, that sub-DC cannot be evaluated at definition.
  Mark it *deferred*; it re-checks per application automatically (the application's own
  `MakeResult` runs it). Emit a definition-time warning listing deferred spots.
- **Metric/projection constraints**: a formal `unit<float64>` has an empty metric at
  definition; operators that constrain metrics re-check per application. Definition
  checking is sound for kinds/domains/arity — not for metrics (P4/v2: declared metric
  constraints on parameters). Result *units* are always derived per application
  (§5.7), which is a feature, not a gap.
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
| R11 | CalcCache persistence of the new `applyF` key head | Low-Med | version the head; new heads simply miss old caches |
| R12 | GUI predicates test `InTemplate` for badges/eval-suppression; `InFunction` items now carry DCs | Med | treat `InFunction` as template-like in all GUI data-request paths |
| R13 | retiring a rewrite rule (or toggling a function's `inline`-ness) changes expression canonicalization → CalcCache misses AND previously-unifying units stop unifying across spellings | High (transition) | `inline` prelude functions reproduce today's keys byte-identically; retire per-rule with a stated key impact; regression configs asserting cross-spelling unit unification |
| R14 | overload resolution surprise (order dependence, ambiguity) | Med | definition-time pairwise disjointness / strict specificity over the closed 𝕍 universe; ambiguity is an error; never registration-order semantics (unlike `FindOper`'s first-match) |
| R15 | CRS/key restructuring (§4.9): new `(BaseUnit (SRef …))` head + background refs leaving identity → CalcCache misses, and dd-only mismatches stop being unify errors | Med (transition) | key-head versioning (R11 pattern); regression configs asserting CRS mismatch *stays* an error while background-only mismatch stops being one |

Unverified, needs a prototype: the meta pass over calculator-bearing but data-less
parameter items completing without tripping residual asserts; teardown behavior of
chained subtree DCs (R5); CalcCache round-trip for new key heads (R11).

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
