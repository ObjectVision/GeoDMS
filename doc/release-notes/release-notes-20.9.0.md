**Preview release.** GeoDMS 20.9.0 introduces **user-defined functions** and a **type language for them**, including higher-order functions. A configuration can now declare a `function` with typed parameters and a declared result type, apply it inline, pass functions as arguments and return them as results — and the engine checks all of that **at definition time**, before any data is read.

This is a preview: it ships the **`.m` (Windows, MSBuild) flavour only**, so that the new language can be tried out and reported on before a full release.

| Flavour | Included |
|---|---|
| `.m` — Windows, MSBuild | **yes** — `GeoDms20.9.0.m-Setup-x64.exe` |
| `.c` — Windows, CMake | no (full release only) |
| `.l` — Linux (Ubuntu 24.04) | no (full release only) |

Built from branch `release_20_9_0_preview`, which branches from the typed-function work on `hof_syntax` and deliberately excludes the in-progress resource-aware scheduling changes.

## User-defined functions

- **`function` declarations** with typed parameters, a declared result type, and inline application.
- **Higher-order functions**: function-valued parameters, function-valued results, and closures.
- **Partial application** — `F(k, _)` — and `map(F, src)` / `map(F(k, _), src)` over containers, whose children are first-class typed items.
- **Anonymous functions** (lambdas), both as standalone definitions and as arguments, with a brace-disambiguation rule.
- **Variadic rest parameters** (`...x`) and **meta-reference parameters** (`item x`).
- **`apply` / `instantiate`** keywords, container literals as arguments, and templates applied as values by beta-reduction.

## A type language for functions

- **Type aliases** (`alias = type;`) and **declared function signatures**.
- **Value-type polymorphism**: generic type variables on functions, with generic constraints (including `signed_domain_points` / `unsigned_domain_points`).
- **Refinements** as checked type aliases.
- **Variants**: type-dependent overloading of user functions, with specificity ordering and definition-time disjointness checking.
- **Robinson unification** with variable-variable links, type application bindings enforced at application, and domain type-variables.
- **Structured and container parameters** (K11): parameters that are a composition of units, attributes and containers with sub-items, whose members are typed at definition time — including generic member types, by-example members, and a contract check at the instantiation point.
- **Composite results are typed too**: the select family, `connect_info`, Dijkstra OD impedance-matrix sub-items, `discrete_alloc` allocation members, and `for_each` result containers.

## Definition-time checking

Errors that used to surface only when data was computed are now reported when the configuration is read.

- Scope and shape checking of function bodies, and a definition-time typed body walker.
- A declarative **operator-signature interface** (`DescribeSignature`) covering the attribute, relational, aggregation, fresh-unit and K13 families — so classics such as a wrong result domain in an aggregation, or a join-key identity mismatch, are caught at definition.
- **SigUnitChecker**: a merge-time structural kind audit plus a runtime unit-replay drift verifier.
- **`@checkfunctions`**: opt-in definition-time audit of every function definition in a configuration; definition-check failures are persisted on the function and surfaced.
- Definition-time processing of closed K13 meta specifications, including `for_each_*` / `for_each_ind` / `loop`, with honest reporting of cleanly-evaluated invalid specs.

## A typed prelude replaces the expression-rewrite rules

An auto-imported **typed prelude** now provides, as ordinary typed functions, what used to be hard-coded definitional rewrite rules in `RewriteExpr.lsp`: `abs`, `sort_str`, `reverse`/`reversed_id`, the `isNearby` family, `rescale`/`normalize`/`scalesum`/`distribute`, the `PropValue` accessors, the `concat`/`replace_value`/`combine_data` chains, `MakeDefined`, 3-arg `median`, 3-arg `const`, `ReadValue`, the `add`/`mul`/`or`/`and` folds, 2-arg `log`, and the 5+-argument `replace` fold. The prelude acts as the implicit outermost namespace for call heads.

## New: the testcases battery ships with the setup

The typed-function regression suite is now installed as **`examples\testcases`** (180 `.dms` configurations). It can be run against the installed engine with `examples\testcases\run_testcases.bat`, and the repository's `batch\TestDebugUnit.bat` / `batch\TestReleaseUnit.bat` run it as part of the unit tests. This doubles as a large, executable set of worked examples of the new syntax.

## Fixes

- `PropValue(item,'ValuesUnit')` / `'DomainUnit'` regressed to returning a bare name instead of the documented relative path; restored, with config-dump fidelity kept separate.
- **#1161**: value type names are now lowercase.
- **#1164**: completed the `boost::format` → `std::format` migration in `shv`.
- **#1166**: function-body identifiers resolve against the whole definition scope, and a nested body can reach the enclosing function's parameters and locals.
- Fixed GUI crashes on template-internal generic items (null-unit access violations).
- Fixed a crash on generic type variables used inside structured-parameter members.
- Rate-limited an `EmptyWorkingSet` storm on large-RAM machines.
- `prelude.dms` is now packaged in all three setups (it was missing, which broke the unit tests).
- Setup scripts abort or confirm when another build is running, preventing a torn snapshot.

## Not included in this preview

- The `.c` and `.l` flavours.
- The resource-aware scheduling work (`schedule-with-lookahead`), which is still in progress. Only its design document is present.
- The CRS / metric decoupling that removes the `0xFF`-packed spatial reference, so `area(geom, m2)` on a CRS-tagged coordinate unit still emits the issue **#1119** deprecation warning and treats the second argument as a label.
