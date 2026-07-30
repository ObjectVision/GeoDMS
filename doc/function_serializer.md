# Faithful `function` representation in the DMS config dump

*Status: design + implementation (branch `hof_syntax`). File:line references verified against
that tree.*

## Motivation

The GUI **Configuration** detail page and the `DMS_TreeItem_Dump` file-save path share one
DMS-syntax serializer, `TreeItem::XML_Dump` (`rtc/dll/src/tic/TreeItem.cpp`). Function items are
template-like containers (`SetIsFunction` = `SetIsTemplate` + `TSF_IsFunctionItem`), so they used
to serialize as the generic container form:

```
container compose:
    IsTemplate = "True"
{
    container f;
    container g;
    container result: IsTemplate = "True" { attribute<V> x(d); attribute<V> result(d): = f(g(x)); }
}
```

This is misleading (it hides the typed telescope, type variables and result spec), and it is not
pastable back as a function. The goal is to render the item as its `function` declaration:

```
function compose<V: numerics, D: domains>(f: nuf<V, D>; g: nuf<V, D>) -> nuf<V, D> := result
{
    function result(attribute<V> x (D)) -> attribute<V> (D) := f(g(x));
}
```

**Round-trip (dump → re-parse → re-compute) is the completeness yardstick**: if the dumped
configuration re-parses to an equivalent, still-computing config, the representation captured
everything relevant. Save/reload of GUI-edited configs is no longer an operational feature, but
round-trip remains the measure of representational completeness.

Two independent prerequisites, both shipped ahead of the serializer:

- `AbstrDataItem::HasVoidDomainGuarantee()` returns **false** on a null (unresolved, in-template
  generic) domain rather than throwing — otherwise the page truncated with unclosed braces at the
  first generic-domain parameter (the throw unwound past the brace-closing `EndSubItems`). Commit
  `b2eb620f`.
- A headless `GeoDmsRun @dumpconfig <out.dms>` verb (calls `DMS_TreeItem_Dump`), used for
  inspection and the round-trip harness. Commit `b2eb620f`.

## Domain / values fidelity (three fixes the round-trip yardstick forced)

The round-trip sweep surfaced three ways the *type spelling* of a typed item was lost or mangled;
each is fixed so the dumped declaration re-parses to the same runtime item:

1. **Void domain ⇒ `parameter<V>`, not `attribute<V> (void)`.** `DMS_WriteValuesPrefix` picks the
   keyword purely from `HasVoidDomainGuarantee()`, and `DMS_WriteDomainSuffix` emits no suffix when
   that guarantee holds. (The earlier `DomainUnitToken().empty() && …` guard misfired for a
   `parameter<uint32>` whose token is the non-empty `"void"`.)
2. **A bare-name source token beats a resolved *relative path* — in the DUMP accessor only.**
   The resolved `GetScriptName` is a `../Rd`-style path for an item nested inside a function body.
   That path ascends above root once the template body is instantiated at the call site (a
   different depth), so a nested `attribute<float64> a (Rd)` in
   `function Nested{ container helpers{ a … } }` reloaded as `a(../Rd)` and threw
   `FollowDots: relative pathname ascended above root`. The serializer therefore prefers the
   **bare** source token (`m_tDomainUnit`/`m_tValuesUnit`) when it has no `/` and the resolved form
   does — a bare name is up-scope searched and thus re-instantiation-safe. (Only that narrow case
   substitutes; explicit source paths and same-level names keep the resolved name.)

   **This preference lives in `GetRawValue`, never in `GetValue`** (`AbstrDataItem.cpp`). `PropDef`
   exposes two accessors and they answer different questions:

   | accessor | reached via | consumer | value |
   |---|---|---|---|
   | `GetValue` | `GetValueAsSharedStr` | `TreeItemPropertyValue` → the **`PropValue()` operator** | RESOLVED script name (`../meter`) |
   | `GetRawValue` | `GetRawValueAsSharedStr` | `OutStreamBase::DumpPropList` → the **config dump** | bare source token (`meter`) |

   Putting the preference in `GetValue` (as `fc15f892` first did) silently changed the user-visible
   `PropValue(item,'DomainUnit'/'ValuesUnit')` from the documented relative path back to the bare
   name, which the tst **Operator** suite asserts since 18.x
   (`Miscellaneous/PropValue/A/test_valueunit` expects `../meter`) — it regressed
   `/results/tests_regression` to `False` in 20.8.0. Dump fidelity and property semantics are
   independent concerns; keep them on their own hooks.

   The values unit rides in the **tag**, not in the property list, so `AbstrDataItem::GetSignature()`
   (`attribute<vu>` / `parameter<vu>`) must read `GetRawValue` too. It is a serializer accessor —
   its only callers are the DMS dump tag and the function-decl writer, neither GUI text nor the C
   API. Guarded by `fn_test_vurt`, whose values unit is a function parameter (`attribute<mu>` inside
   a body container); with `GetSignature` on the cooked accessor its dump emits `attribute<../mu>`
   and the reload ascends above root. The domain side has been guarded all along by
   `fn_test_p1/Nested`.
3. **A non-Single composition survives even on a `.`/empty domain.** `DMS_WriteDomainSuffix` used to
   drop the whole suffix for a `.` (self) or empty domain — discarding the `arc`/`polygon`
   composition with it. The typed-HOF walker's `connect_info` member typing rewrites an arc-geometry
   parameter (`attribute<fpoint> arcs (Arc)`) to a self-domain (token `.`) carrying `arc`
   composition; dropping the composition made `arcs` render as plain `attribute<fpoint> arcs`, and
   `connect_info`'s "arc geometry (Sequence or Polygon)" arg (`Connect.cpp:692`) no longer matched
   any overload. The suffix now keeps a `(., arc)` form (self-domain retained, or `.` synthesized
   for an empty token) whenever the composition is non-Single, so the arc/polygon nature reloads.

## What is stored, and the storage additions

`FunctionSpecData` (`s_FunctionSpecAssoc`, `rtc/dll/src/tic/TreeItem.cpp`) already holds `nrParams`,
`resultName`, `paramSigs` (index, weak exemplar, typeArgs), `genericParams`, `typeVars`,
`metaRefParams`, `hasRestParam`, `isVariantSet`. Accessors in `TreeItem.h`.

Two facts made faithful rendering impossible without additions:

- **signature-only** functions (`nuf = function<...>(...) -> ...;`) had no persisted flag
  (parse-state only); they were indistinguishable at runtime from a designation-form function
  whose result child happens to be expr-less.
- the **result signature** (`-> nuf<V, D>` / `resultIsFunction`) persisted nothing — the result
  item carries its own declared type, but a function-valued result's alias name + typeArgs were
  lost.

Added to `FunctionSpecData`: `signatureOnly`, `resultIsFunction`, `resultSig` (weak exemplar),
`resultSigTypeArgs`; accessors `TreeItem_{Set,Is}FunctionSignatureOnly`,
`TreeItem_SetFunctionResultSig`, `TreeItem_IsFunctionResultFunction`,
`TreeItem_GetFunctionResultSig[TypeArgs]`; extended `IsDefaultValue`. `CopyFunctionSpec` copies the
whole struct, so the fields ride instantiation copies. Set at parse time in
`ConfigProd::OnFunctionResultSig` (capture the pending result-sig exemplar + typeArgs) and flushed
in `ConfigProd::OnFunctionDeclEnd`.

## Serialization (grammar-verified against `stx/dll/src/ConfigParse.cpp`)

Branch in `TreeItem::XML_Dump`: `GetSyntaxType() == ST_DMS && IsFunctionItem()` →
`TreeItem::XML_DumpFunctionDecl`. Non-DMS streams (XML/HTM detail pages) keep the generic form.

Canonical forms:

1. **Normal function** — `function name<tvs>(p1; p2; ...)[, using = ns] -> <resultType> := <resultName>`
   then a body block `{ … }` with all non-parameter children (the designated result child renders
   there too; the `:= name` designation is always used — uniform, no synthesized-vs-designated
   discriminator needed).
2. **Variant set** — `function name { variant v1(...) -> T := e; … }` (each child dumps with the
   `variant` keyword; detected via `TreeItem_IsFunctionVariantSet(parent)`).
3. **Signature alias** — `name = function<tvs>(params) -> <resultType>;` (name precedes the
   keyword; no designation, no body).

Parameter rendering (dispatch order matters — the first three are plain `TreeItem`s whose
`GetSignature()` would say `container`): rest `...x` → meta-ref `item t` → signature-typed
`f: nuf<V, D>` → data item `attribute<vTok> name (dTok[, comp])` / `parameter<vTok>` from the
**source tokens** `ValuesUnitToken()`/`DomainUnitToken()` → unit `unit<vt>` (+ inline member block)
→ `container name`.

### Stream mechanics (hybrid)

`OutStream_DMS` is stateful. The serializer constructs one `XML_OutElement(out, "function", name)`
(preserving indentation via `m_Level` and the terminating `;` from `~XML_OutElement`→`ItemEnd`
for the inline/alias forms), then **hand-writes the whole rest of the header** (`<tvs>`, `(…)`,
`, using = …`, `-> …`, `:= …`) as raw text via `operator<<`. The first raw write harmlessly
`CloseAttrList`s (zero attrs ⇒ no stray `()`). The branch **never** calls
`WriteAttr`/`DumpPropList`/`DumpSubTags` (which is why `IsTemplate`/`Using` subtags disappear
automatically, and which also sidesteps an rtc-vs-tic DLL layering issue). Two hard grammar
constraints drove this: function params are `;`-separated (`,` is a parse error, so the attr
machinery cannot emit them), and multiple `using` clauses must be **repeated** `, using = ns` (the
`;`-joined `UsingPropDef` value is not parseable in a header).

### Robustness

Both the generic sub-item loop and the function body loop route recursion through
`TreeItem_XML_DumpSubItemSafe`, which wraps `subItem->XML_Dump` in try/catch and, on a throw,
emits an inline `// ERROR dumping <name>: …` comment (parse-neutral) and continues so the enclosing
`EndSubItems` (the closing `}`) always runs. This closes the truncation class of bugs for every
future thrower, not just the generic-domain one.

## Accepted v1 limitations (documented)

- A bare `f: function` parameter (no signature alias) and a `-> unit<V>` result are stored as plain
  `TreeItem`s and render as `container` — binding-compatible, but not fully faithful.
- A `container` parameter carrying a member block (`function f(container P { parameter<float64> a; })`)
  renders as bare `container P` — the member declarations are dropped. Binding-compatible (a container
  parameter accepts the argument regardless), so it recomputes on reload; only the documentary shape
  is lost. (Unit parameters *do* render their member block; only the container-param case is skipped.)
- A renamed designated result label (`-> x: T := y`) loses the label `x` (semantically equivalent).
- Domain/values *case* may normalize when a reference resolves (DMS identifiers are
  case-insensitive, so this is round-trip-safe); the serializer reads source tokens to minimize it.
- **A config that explicitly `#include`s the auto-imported prelude cannot round-trip.**
  `fn_test_prelude` deliberately does `#include <%exeDir%/prelude.dms>` — the same file the engine
  auto-imports. `#include` directives are erased at parse time (the items are materialized), so the
  dump emits the prelude functions as literal `function sqr…` declarations at root, which collide
  with the auto-imported prelude on reload (`SubItem 'sqr' is already defined`). The function
  *rendering* is correct; the collision is inherent to include-erasure vs. auto-import. The
  round-trip harness skips this one config with an inline reason (the way it skips negatives).

## Verification

- `testcases/run_testcases.bat` — the config suite: **180/180, BAD=0** in Release *and* Debug.
  The domain/values/composition changes are dump/display-only, so computation is unaffected.
- `testcases/run_roundtrip.bat` (harness `run_roundtrip.ps1`): for each positive config,
  `@dumpconfig` then reload the dumped `.dms` and recompute its item (the dumped IntegrityChecks
  re-verify the semantics). **All 79 round-trip, BAD=0.** `fn_test_prelude` is the one documented
  skip (explicit `#include` of the auto-imported prelude — see limitations above).
- The **tst Operator suite** (`C:/dev/tst/Operator/cfg/operator.dms`, item `/results/tests_regression`)
  is the guard on the *property* side of the raw/cooked split: it asserts
  `PropValue(A,'ValuesUnit') == '../meter'`. Any change to `DomainUnitPropDef`/`ValuesUnitPropDef`
  must be checked against **both** it and the round-trip sweep — they pull in opposite directions,
  which is precisely why the two accessors exist.
- Manual: the GUI Configuration page for `compose` shows the `function …` form, brace-closed.

The round-trip yardstick drove the three fidelity fixes above: without them the sweep reported
`fn_test_ci` (arc composition dropped → `connect_info` overload mismatch), `fn_test_p1`
(`FollowDots: relative pathname ascended above root` from a `../Rd` body domain), and a
`parameter<uint32>` result mis-rendered as `attribute<uint32> (void)`.
