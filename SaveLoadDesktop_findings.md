# Save/Load Desktop (collection of Views) — findings & effort estimate

_Date: 2026-06-13 · Branch: refactor_linux_gui · Status: investigation only, no decision taken_

Context: deciding whether to (a) phase out the Desktop save/load internals completely, or
(b) re-add explicit GUI actions to **save and load desktop definitions in `.dms` syntax**,
triggered by the user (not auto-stored). This document records what exists today, when the
old feature was removed, and a grounded effort estimate for option (b).

---

## 1. When was save/load of Desktops phased out?

The feature was part of the **old Delphi / RAD-Studio-XE2 GUI**, not the current Qt GUI.

- Implementation lived in `XE2/General/uDMSDesktopFunctions.pas`, `fOpenDesktop.pas`
  (the `frmOpenDesktop` dialog), the `TDesktopManager` class, and menu items
  **"New Desktop…" / "Open Desktop…" / "Set Startup Desktop"**.
- Last active maintenance: `7994dd5e` (2022-11-07, "access violation fixes related to
  save/load desktop") and `bfae74a8` (2022-11-08, "Various fixes for save/load Desktops to file").
- Issue [#132](https://github.com/ObjectVision/GeoDMS/issues/132) ("save desktops to file,
  uitgeklede functionaliteit") closed **COMPLETED** 2023-08-28.
- **Removal commit: `3dd9e183` "XE2 code clean-up" (2023-08-31)** deleted the entire Delphi
  GUI — 196 files, 27,526 deletions — including `uDMSDesktopFunctions.pas`, `fOpenDesktop.pas`,
  `TDesktopManager`, and the New/Open/Set-Startup-Desktop menu items.

So the **user-facing save/load of Desktops disappeared on 2023-08-31** with the Delphi GUI
removal, as part of the migration to Qt. The Qt GUI **never re-implemented** the menu.

The lower-level `shv` plumbing partly survives and is still wired in (see §3).

---

## 2. Open issues bearing on the decision

There are effectively **no open issues that request or depend on Desktop save/restore**.

- [#132](https://github.com/ObjectVision/GeoDMS/issues/132) (the dedicated one) — **CLOSED/COMPLETED**.
- `/Desktops/...` issues #508, #700, #624, #398 — all **CLOSED**, and about the *internal*
  Desktops container (views live as `/Desktops/Default/ViewN`), not the save/load-to-file feature.
- [#810](https://github.com/ObjectVision/GeoDMS/issues/810) "GeoDMS componenten planning" — lists
  "Graph functionality" / "Layer control" but nothing about restoring desktops.
- [#75](https://github.com/ObjectVision/GeoDMS/issues/75) "Graphview window" — the new ChartView
  work. Doesn't ask for save/load, but it's the thing that needs re-testing if persistence is
  re-added (its `Sync` is brand new — see §4 / task 5).

**Conclusion:** from the issue-tracker's standpoint, phasing out completely is low-risk — nothing
open is blocked on it. The decision can be deferred without stalling other work.

---

## 3. Key finding: most of the engine work already exists and is current

This would **not** be a fOpenDesktop/TDesktopManager rebuild. The modern `Sync` +
`IncludeFileSave` path supersedes the dead Delphi code and is already maintained.

The view ↔ TreeItem serialization is the `Sync(TreeItem*, ShvSyncMode)` mechanism with
`SM_Save` / `SM_Load`. Every view component implements it (ChartControl, ChartLayer,
DataItemColumn, EditPaletteControl, PaletteControl, ViewPort, …).

- **Save direction:** `SHV_DataView_StoreDesktopData()` → `dv->GetContents()->Sync(vc, SM_Save)`
  writes the live view's full state (ROI, layers, themes, classification, and — new — ChartControl
  draw mode / X-axis / categorical axis) into its view-context TreeItem under
  `/Desktops/Default/ViewN`.
  `shv/dll/src/ShvDllInterface.cpp:124`
- **Load direction is already the live path:** `QDmsViewArea` opens *every* view via
  `SHV_DataView_Create(viewContext, viewStyle, ShvSyncMode::SM_Load)`.
  `qtgui/exe/src/DmsViewArea.cpp:297`
  Today the context is freshly empty, so SM_Load just builds defaults; feed it a *populated*
  context and the same code restores state. The `ChartLayer`/`HistogramLayer` "reconstructed
  from a saved desktop, keep restored ROI" comments confirm SM_Load already honours stored state.
  `shv/dll/src/ChartLayer.cpp:80`, `shv/dll/src/HistogramLayer.cpp:54`
- **TreeItem subtree → `.dms` writer already exists:** `IncludeFileSave()` / `DMS_TreeItem_Dump()`
  serialise any subtree to `.dms` config syntax — this is the ".dms syntax" path requested, and
  it is what `#include` / `configStore` already use.
  `tic/dll/src/Xml/XmlTreeOut.cpp:1379`, caller `tic/dll/src/TreeItem.cpp:4226`
- **`.dms` → tree reader exists:** `DMS_CreateTreeFromConfiguration` plus the include reader.
  `stx/dll/src/StxInterface.cpp:103`
- **Qt view-open path already takes (context, style):** `MainWindow::createView` creates the
  view-context item under `GetDefaultDesktopContainer` and spawns a `QDmsViewArea`.
  `qtgui/exe/src/DmsMainWindow.cpp:1011`

---

## 4. What's genuinely new (the actual work)

| # | Task | Est. |
|---|------|------|
| 1 | **GUI actions** — "Save Desktop…" / "Load Desktop…" menu items + `QFileDialog`, mirroring existing `openConfigSource`/export actions in `DmsActions.cpp` | ~0.5 d |
| 2 | **Save glue** — walk open `QDmsViewArea` MDI children, `Sync(SM_Save)` each, then `DMS_TreeItem_Dump` the `/Desktops/Default` subtree; verify the dump is self-contained/reloadable | ~1 d |
| 3 | **Persist ViewStyle** — the map/table/chart kind is passed *alongside* the context today, not stored in it; add a `SyncValue` for view style (+ chart kind) so Load knows which view type to recreate | ~0.5 d |
| 4 | **Load glue** — read the chosen `.dms` as an *include into the live, already-calculated tree* under `/Desktops`, then iterate the `ViewN` items and spawn a `QDmsViewArea` per item with its stored style | ~1–2 d |
| 5 | **ChartView round-trip testing (#75)** — the ChartControl/ChartLayer `Sync` is days old and has never been through a real file round-trip; verify bar/scatter/line, X-axis pick, categorical axis, thematic colour all survive save+reload | ~1 d |
| 6 | **Robustness / edge cases** — per-view graceful failure when a referenced item is missing (cf. old "CreateView Error style N" #398/#624), `/Desktops` name collisions, `IsEndogenous` items, and MDI window geometry (size/position is *not* in the ROI Sync today — separate GUI-side persistence if wanted) | ~1–2 d |

**Total: ~5–9 working days** for a polished, tested feature.
**Spike (happy path, no polish): ~2–3 days** — dump `/Desktops/Default`, reload by re-include +
re-spawn views.

---

## 5. Where the risk concentrates

- **Task 4 — merging a `.dms` into a running tree** is the only genuinely novel piece.
  `DMS_CreateTreeFromConfiguration` builds a *fresh root*; injecting a subtree into a live,
  partly-calculated tree touches interest counts, item locks, and update state.
  **Prototype this first to de-risk the estimate.**
- **Task 5 — ChartView serialization** is new code that has never round-tripped; budget real
  testing, not a smoke check.
- **Semantics to confirm:** a saved desktop references data items by **path/expression**, so it
  only reloads meaningfully against the *same config* (exactly like the old Delphi desktops).
  Cross-config-portable desktops would be a different and much larger problem.

---

## 6. Open scope question (settle before implementing)

Should MDI **window size/position** be saved too, or just logical view contents
(layers, ROI, classification, draw mode)? The current `Sync` captures contents only; window
geometry would be a small but separate addition.

---

## 7. Key code anchors (for whoever picks this up)

- `shv/dll/src/ShvDllInterface.cpp:124` — `SHV_DataView_StoreDesktopData` (view → context, SM_Save)
- `shv/dll/src/ShvDllInterface.cpp:67` — `SHV_DataView_Create(context, style, sm)`
- `qtgui/exe/src/DmsViewArea.cpp:297` — `QDmsViewArea` ctor calls Create with `SM_Load`
- `qtgui/exe/src/DmsMainWindow.cpp:1011` — `createView`, view-context under `GetDefaultDesktopContainer`
- `tic/dll/src/Xml/XmlTreeOut.cpp:1379` — `IncludeFileSave` (subtree → `.dms`)
- `tic/dll/src/TreeItem.cpp:4226` — `XML_Dump` / include-save call site, ST_DMS syntax
- `stx/dll/src/StxInterface.cpp:103` — `DMS_CreateTreeFromConfiguration` (`.dms` → tree)
- `shv/dll/src/ChartControl.cpp:160`, `ChartLayer.cpp:71` — new ChartView `Sync` (SM_Save/SM_Load)
- Removal commit of old Delphi feature: `3dd9e183` (2023-08-31)
