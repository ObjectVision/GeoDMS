**Pre-release.** GeoDms 20.17.0 follows **20.16.0** with a round of `.mmd` storage fixes — a dictionary written by one configuration and read by another now actually reads — plus fence, teardown and provenance work.

| Flavour | Asset |
|---|---|
| `.m` — Windows, MSBuild | `GeoDms20.17.0.m-Setup-x64.exe` |
| `.c` — Windows, CMake | `GeoDms20.17.0.c-Setup-x64.exe` |
| `.l` — Linux, Ubuntu 24.04 | `GeoDms20.17.0.l-linux-x64.deb`, `.tar.gz` (+ `.sha256`, `.sha256.p7s`) |

## `.mmd` dictionaries

The restrictions that a dictionary carries were introduced one release ago. Reading such a store back into a *different* configuration — the whole point of the decoupling pattern — turned out to fail in three separate ways.

- **Unit paths are now absolute, in declarations and checks alike (#1195).** The restriction spelled each unit with the raw configured token, and so did the declaration beside it, so a domain configured as `../RegioUnit` or `(...)` went into the dictionary verbatim. A reader merges that dictionary somewhere else in its own tree, where the dots resolve against *its* structure and land on whatever sits there: `Unknown identifier '../RegioUnit'`, or `Cannot find operator for these arguments: arg1 of type TreeItem`. The store could then not be read at all — not even where the declaration itself resolved fine. A bare name had the same defect more subtly, binding by up-scope search to whatever the reader declares under that name. Both check and declaration now come from one source and are written as the full path for every unit declared outside the dictionary. Where typed bound literals are emitted, the redundant `PropValue(u, 'ValueType')` term is dropped: a changed value type already fails the bounds term.
- **A differing dictionary root name is provenance, not a warning (#1194).** The root name in `0Dictionary.dms` is the name of the container that *wrote* the store, and a reader names its own container after the read step, so in the decoupling pattern the two differ by construction — and the writing project need not even be the same project. Silencing it by adopting the writer's name is impossible as soon as one file is read into more than one place. One ordinary session produced six such warnings before anything was calculated, which is how people learn to skim past warnings. It is now a trace line that says which name is the writer's and which the reader's.
- **A check that cannot be built now names itself (#1197).** An `IntegrityCheck` whose operator or identifier does not resolve failed with the operator's own message and nothing else — `eq Error: Cannot find operator for these arguments` — with no word that a check was involved, let alone which one. That matters most for the restriction a dictionary carries, since it appears nowhere in the configuration. A check that merely evaluates to false already reported itself.

## Writing vector layers (#711)

`gdalwrite.vect` writes a layer as a whole, but when only one column was of interest it created a field for every storable attribute and left the rest `<null>`. A column of interest now pulls in the other columns of its layer for exactly as long as the outside interest lasts, and field creation follows what is actually written: no field for a column without data, and an explicit error for drivers such as CSV that fix their fields at the first feature.

A unit that both declares a `SpatialReference` and has a calculation rule lost that CRS once its referred item was resolved, so a shapefile got no `.prj` and a GeoPackage got srs_id "Undefined". The declared CRS is now part of the unit's key expression.

## Fences and scheduling

- **A phase runs when a member is reached (#1167).** A `PhaseContainer` was executed only when its container was reached, so a configuration that reaches members at update level — the `Ready` + `ExplicitSuppliers` driver idiom, which is all `GeoDmsRun` ever produces — walked straight past the fence: the work ran unfenced and the phase reported nothing while still paying for its mirror tree.
- **Collecting a phase member is re-entrant (#1201).** With the whole result sub-tree no longer supplied, a completed phase re-emplaced its resource on the next demand and re-collected members that already carried a result — which recalculates the source. That is the shape a `for_each` produces when its generated items each consume one member of the same phase.
- **An indirect expression keeps what it refers to.** It is calculated in place and then dropped, and everything it read is dropped with it, so the next evaluation recalculates all of it. Where a `PhaseContainer` is among those suppliers that means re-running the whole fence: every phase in one test configuration ran and reported four times over.
- **Teardown drain is bounded (#1191).** An item still of interest when the tree is torn down held the session usage count above zero with every worker already idle, so the process parked in an untimed wait. The drain now gives up and reports what is still holding it.

## Elsewhere

- **Source Description covers what a storage feeds (#975).** A storage was listed only for the item literally carrying `StorageName`; its sub-items and everything computed from them showed an empty page. The storage relation is not a supplier relation, so it is now resolved per item — itself when it holds the manager, else the nearest storage parent it reads from or writes to.
- **Tree icons say what an item is (#319)**, instead of which views can be opened on it. That mismatch is why a unit with sub-items drew as a container, an empty container drew as a base unit, and class-break attributes drew as plain attributes.
- **`select_spec` / `collect_spec` (#337)** take the four choices that `select_with_attr_by_xxx` encodes in its name as a `;`-separated word list in their first argument, including the new `ref` reach that follows a template case parameter's binding. Adding the reach as names would have cost 51 of them. The named operators are unchanged and now share one code path.
- **The CalcCache machinery is retired (#1189).** The automatic disk cache of the GeoDMS 7 series was retired with 8.0, but its name survived in a form that reads as current mechanism — most damagingly the versioned `%calcCacheDir%` default, which cost real investigation time on a report of cold/warm run differences that were the filesystem cache. A configuration still spelling `%calcCacheDir%` now gets an unknown-placeholder error.
- The Terminate button no longer clips its own label (#1192).
