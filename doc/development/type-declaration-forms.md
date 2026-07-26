# Type-declaration & by-example forms — complete catalog

A reference for every way a **type** can be written in a GeoDMS `.dms` config: the
direct keyword signatures, the **by-example** (item-reference) forms, and the function
forms. Covers where each is legal (item declaration, function parameter, function
result, type alias) and pins the file:line of the grammar rule + production handler.

Grammar lives in `stx/dll/src/ConfigParse.cpp`; the semantic handlers in
`stx/dll/src/ConfigProd.cpp` (`SignatureType` enum in `ConfigProd.h:25`). The
definition-time type checker consumes these in `rtc/dll/src/tic/AbstrCalculator.cpp`
(`ParamType`, `PositionType`).

---

## 0. The two type worlds

There are exactly two ways the parser resolves a type token:

| World | Grammar hook | Meaning |
|---|---|---|
| **Direct signature** | `itemSignature` (`ConfigParse.cpp:355`) | a *keyword* (`unit<…>`, `attribute<…>`, `container`, …) names the shape from scratch |
| **By-example / reference** | `itemRef → DoRefTypeSignature` (`ConfigProd.cpp:878`) | an *identifier* naming a previously-declared item copies that item's type |

Every declaration position accepts **both** worlds (item decl, param, result, alias).
The "by-example" question is entirely about the second column.

---

## 1. Direct signature forms (`itemSignature`, `ConfigParse.cpp:355-369`)

| Form | `SignatureType` | Handler | Notes |
|---|---|---|---|
| `container` | `TreeItem` | `:356` | a namespace / record container |
| `item x` | `MetaRef` | `:361` | raw item-reference **parameter** (argument binds unevaluated); word-boundary + not-`:`/`=` guarded |
| `template` | `Template` | `:364` | classic pre-HOF template |
| `attribute<vu>` | `Attribute` | `:365` `DoAttrSignature` | values-unit `vu`; optional trailing `(domain)` (`itemHeading` `:351`) |
| `parameter<vu>` | `Parameter` | `:366` | void-domain attribute (single value of unit `vu`) |
| `unit<basicType>` | `Unit` | `:367` | a unit of value class `basicType` (`uint32`, `float64`, …) |
| `entity` | `Unit` (`uint32`) | `:368` `DoEntitySignature` | shorthand for `unit<uint32>` (a domain) |

**Structured (composite) unit form** — a `unit<…>` (or `entity`) signature followed by
a member block `{ … }` (`itemBlock`, `:137`; attached in `functionParamItem`, `:282`):

```
unit<uint32> nw {
    unit<uint32> nodeset;          // a member unit
    attribute<nodeset> F1, F2;     // member attributes relating nw -> nodeset
}
```

This is the K11a case: the members are **statically declared** in the block, so the
checker types `nw/F1` at the definition (`BuildParamMembers`, `AbstrCalculator.cpp`;
K11a-1 `08ac5c8f`, member-unit identity K11a-1b `4a116d62`).

---

## 2. By-example forms (`DoRefTypeSignature`, `ConfigProd.cpp:878-924`)

Writing `name : Exemplar` (or `alias = Exemplar`) where `Exemplar` names an existing
item copies that item's type. `DoRefTypeSignature` resolves the exemplar and branches
on its kind — **four kinds today**:

| Exemplar kind | Becomes | What is copied | What is **NOT** copied |
|---|---|---|---|
| **Unit** (`:886`) | `unit<vc>` signature | value class + `IntegrityCheck` refinement (`CloneAliasRefinement`, `:1008`) | **member sub-items** ← the K11 gap |
| **DataItem** (`:893`) | `attribute<vu>(dom)` | values-unit token, domain token, value-composition, `IntegrityCheck` | — |
| **Function item** (`:905`) | function-signature type | the callee's signature (params + result) | only legal in param / result position |
| **Container / other** (`:915`) | plain `TreeItem` | nothing (bind by reference) | — |

Plus the **unresolved-reference** case (`:919`): an identifier that does *not* resolve
yet — legal **only inside a function declaration** (`f: function`, or a composite type
declared further down). Binds by reference; typing deferred to reduction.

### Type application on a by-example ref — `Sig<V, D>`

A by-example ref may carry a type-argument list (`typeArgsOpt`, `ConfigParse.cpp:184`):
`f: nuf<V, D>` applies the function-signature exemplar `nuf` to the enclosing type
variables. Documentation-level bind in v1 (§5.10 Stage 2).

---

## 3. Where each form is legal (positions)

| Position | Grammar rule | Direct sig | By-example | Structured block |
|---|---|:--:|:--:|:--:|
| **Item declaration** `name : T` | `colonHeading` `:154` | ✓ | ✓ (`:159`) | ✓ (item can have a block) |
| **Function parameter** | `functionParamItem` `:278` | ✓ | ✓ (via `itemDecl`) | ✓ (`:282`, `!itemBlock`) |
| **Anonymous sig parameter** | `functionSigParamItem` `:327` | ✓ | ✓ (`:330`) | — (name synthesized) |
| **Function result** | `functionResultType` `:284` | ✓ | ✓ (`:287`) | — |
| **`-> function` result** | `:285` `OnFunctionResultIsFunction` | function-valued | — | — |
| **Plain type alias** `A = T;` | `aliasPlain` `:336` | ✓ | ✓ (`:339`) | — |
| **Function-sig alias** `A = (…)->R;` | `aliasFunctionSig` `:310` | (params only) | ✓ in params | — |
| **Variadic rest** `...x` | `functionParamItem` `:279` | — | — | — |
| **Bare / auto-typed** `name := expr;` | `bareExprDecl` `:207` | inferred | inferred | — |

Note the asymmetry driving the earlier probes: a **structured member block** is only
attached in **parameter** position (`functionParamItem :282`), and function **results**
only accept `-> parameter<…>` / `attribute<…>` / by-example — not a structured block.
That is why a structured-param function will not parse an attribute-result declaration
over the param's member domain.

---

## 4. The generalization — what "generalize the by-example forms" means

The by-example forms are already *uniform across positions* (§3). The one place they are
**not uniform across exemplar kinds** is member copying (§2, last column):

- **DataItem by-example** copies everything that types it (values, domain, VC).
- **Unit by-example** copies only the value class + `IntegrityCheck` — **not the member
  sub-items**. So `network_links: unit<uint32>{ nodeset; F1; F2 }` used *by example*
  (`nw: network_links`) loses its members; only the *inline structured* form (§1) keeps
  them.

**Generalization target (the K11 "by-example" remainder):** make the Unit branch of
`DoRefTypeSignature` (`ConfigProd.cpp:886-891`) clone the exemplar's declared member
sub-items onto the new item (as `CloneAliasRefinement` already does for `IntegrityCheck`),
so a by-example composite parameter carries the same member map the inline structured
form gets in `BuildParamMembers`. Risks to respect (scope doc §5 R-b): clone only the
declared **kind/type** of each member, never its **data** (an exemplar like fn_test.dms's
`network_links` carries `[0,1,2]` values the checker must ignore).

With that one change, all four by-example kinds become structurally complete, and the
inline-structured and by-example composite forms coincide — a single "composite type"
concept with two spellings.

### Not a generalization, but adjacent

Making the member-identity payoff *observable at def-time* is a **separate** gate
(described combining operators; the `add`/`+` deferral) tracked in
`k11-container-types-scope.md §5.2` — independent of how the type is spelled.

---

## 5. One-glance summary

```
TYPE  ::=  DIRECT-SIG  |  BY-EXAMPLE
DIRECT-SIG ::= container | template | item
             | unit<BT> [ { MEMBERS } ]      // structured composite (K11a)
             | entity   [ { MEMBERS } ]
             | attribute<VU> [ (DOM) ]
             | parameter<VU>
             | function [<TVARS>] ( PARAMS ) -> RESULT
BY-EXAMPLE ::= ItemRef [ <TYPE-ARGS> ]       // copies ItemRef's type:
             //   Unit      -> unit<vc>  (+IntegrityCheck; members NOT cloned ← gap)
             //   DataItem  -> attribute<vu>(dom) (+VC, +IntegrityCheck)
             //   Function  -> function-signature type   (param/result only)
             //   Container -> plain item (bind by reference)
AUTO       ::= name := expr ;                 // type inferred from the rule
```
