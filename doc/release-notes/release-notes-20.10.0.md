**Preview release.** GeoDMS 20.10.0 builds on the **20.9.0** typed-function preview and adds, on top of it, a **coordinate-reference-system (CRS) decoupling** refactor, an **experimental resource-aware scheduler**, and a set of map-view and operator fixes. The typed user-defined functions and type language introduced in 20.9.0 are unchanged and still included.

This is a preview so the new work can be tried out and reported on before a full (all-flavours, release-tested) release. It ships the **`.m` (Windows, MSBuild) flavour only**.

| Flavour | Included |
|---|---|
| `.m` — Windows, MSBuild | **yes** — `GeoDms20.10.0.m-Setup-x64.exe` |
| `.c` — Windows, CMake | no (full release only) |
| `.l` — Linux (Ubuntu 24.04) | no (full release only) |

Built from branch `lookahead-scheduling`, which merges the 20.9.0 typed-function work and adds the CRS decoupling and the (default-off) resource-aware scheduling changes below.

## CRS decoupling

Coordinate-reference-system handling was moved out of a unit side-table and made a first-class property of a unit.

- A **`UnitCrs` value object** carried in a new `AbstrUnit` slot replaces the former CRS side table.
- A **`CrsUnit` operator**, a nested key term, and a CRS **drift detector**.
- **Derivation rules** for the CRS, and a **CRS → background-layer registry**.
- **Unification**, a **projection fallback**, and a **raw / cooked split** of the CRS representation.
- The **linear metric** is now derived, and the legacy **`0xFF` metric packing** was removed.
- Stage 0 also fixed three pre-existing defects in the affected code.

## Resource-aware scheduling (experimental, default-off)

Work towards scheduling that accounts for memory before admitting operations. **All of this is off by default** and does not change behaviour unless explicitly enabled.

- **Retained-result accounting** and an **admission gate** (default off) that cuts peak memory on wide workloads.
- A **materialization-regime cost model** — the regime is measured rather than predicted.
- **Retry discipline** (estimate once per operation context; retry on release or idleness) and a **drain mode** that defers retained-memory growth behind a refused task.
- A **`/SB` budget switch** and **ledger diagnostics** on their own switch, with periodic ledger-vs-reality sampling.

## Fixes & smaller improvements

- **#1129**: `+` / `-` keys and a mouse-wheel notch zoom the map view again.
- **#1143**: the detail page now shows the storage of an `.mmd`-backed unit.
- **#597**: `min_ifdefined` / `min_alldefined` (and the `max` variants) aggregations.
- **#1159**: TableView columns follow the TreeView text colours.
- **#292**: warn about unknown escape codes in parsed strings.
- Take interest before reading `FileName` / `FileType` of an `ExportSettings` / `MetaInfo` (export fix).
- `SaveValueInfo` test-script fixes (actually save the page; `t1640` / `t1642` compare no files).
