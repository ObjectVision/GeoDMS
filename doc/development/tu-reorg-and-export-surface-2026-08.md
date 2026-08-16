# TU reorganization and export-surface reduction — findings and plan, 2026-08

*2026-08-16, branch `lookahead-scheduling`, HEAD `ecbccab7`. Follow-up to
`header-hygiene-2026-08.md` (headers) and to the export-surface follow-up recorded in
`compile-time-refactor-analysis-2026-07.md` after the rtc+sym+tic merge. This document is
the authoritative TODO for: (1) de-exporting symbols not imported by any other binary,
(2) moving TUs/headers to the DLL that actually consumes them, (3) splitting, merging and
renaming TUs for functional cohesion and compile time.*

Method: three sweeps at this HEAD against the freshly built Release binaries
(2026-08-16 09:27) — a `dumpbin /exports`+`/imports` cross-match over all nine GeoDMS
binaries (decorated-name exact matching; `undname` grouping to identifiers), an
export-macro mechanics audit, and a TU inventory (LOC, Release `.obj` sizes, cohesion
inspection of the largest TUs). Numbers below are from those sweeps; the usual caveats
(symbol-level grep blind spots) do not apply to the dumpbin data, which is ground truth
for the *current* in-tree link graph.

## 1. The export/import surface today

| DLL | exports | live | dead | dead % |
|---|---:|---:|---:|---:|
| Rtc.dll | 5672 | 3178 | 2494 | 44.0 |
| Stx.dll | 15 | 8 | 7 | 46.7 |
| Stg.dll | 87 | 26 | 61 | 70.1 |
| Clc.dll | 17 | 11 | 6 | 35.3 |
| Geo.dll | 7 | 7 | 0 | 0 |
| Shv.dll | 208 | 32 | 176 | 84.6 |

"Live" = imported by ≥1 other GeoDMS binary. The import evidence is complete: there are
no GeoDMS→GeoDMS delay-load edges (only `gdal.dll` is delay-loaded, by Stg/Clc/Geo), and
the only `GetProcAddress` site in the tree (`rtc/dll/src/dllimp/RunDllProc.cpp`, used by
clc's `DllFunc`/exec operator) loads config-supplied *third-party* DLLs, never GeoDMS
ones. `geodms.pyd` links all six DLLs as a full C++ importer, so its usage is in the
import tables too.

Dependency graph (importer → imported-symbol count from Rtc): Stx 248, Stg 494,
**Clc 2549, Geo 1656**, Shv 492, geodms.pyd 68, GeoDmsRun 62, GeoDmsGuiQt 249.
Clc+Geo account for **71% of all cross-DLL import traffic** (4205 of 5918 records).

Rtc's live exports by consumer set: `{Clc,Geo}` 1276 + `{Clc}` 822 dominate — the
`DataArrayBase<T>`/`NumericArray<T>`/`TileFunctor`/`sequence_array<T>` instantiation farm
(~2100 of the 3178 live exports serve only Clc/Geo). Single-consumer slices: Shv-only
132, Stg-only 130, GuiQt-only 83, Stx-only 74, Geo-only 43. Only 16 symbols are imported
by all seven consumers (the true core API).

### 1a. Structure of the dead 44%

Of Rtc's 2494 dead exports, **987 identifiers are entirely dead (all overloads and
instantiations) → 2113 symbols, 85% of the dead surface**. Only 56 identifiers are
partially dead (381 symbols) — template-instantiation tails where the same member is
live for some value types and dead for others; since the export decoration sits on the
template member (one macro covers all ~54 instantiations), **partially-dead identifiers
must keep their macro** and are out of scope for the bare-removal pass.

Dominant entirely-dead families:

| family | dead/total | note |
|---|---|---|
| `ValueWrap<T>` members | 557/594 | AsCharArray, AsString, AsFloat64, IsNull, AssignFromCharPtr*, GetValueClass, GetDynamicClass, ctor — each ×~54 types |
| free functions | 550/925 | incl. `CreateHeapTileArray_impl` ×36 (private helper, exported only because the macro sits on the template), `AsCharArray` ×14 |
| `sequence_array<T>` | 218/315 | |
| `SA_Reference<T>` | 204/358 | |
| `heap_sequence_provider` | 33/33 | whole scope dead |
| `IndexableUnitAdapter` | 32/32 | whole scope dead |
| `SignatureRecorder` | 30/30 | whole scope dead |
| `VarNumRangeUnitAdapter` | 20/20 | whole scope dead |
| `FloatUnit` | 12/12 | whole scope dead |

60 scopes have *every* exported member dead (275 symbols): `XmlElement`/`XmlParser`/
`XML_Table`/`XML_DataBracket`/`OutStream_XML`, `FindFileBlock`, `PlatformInfo`,
`FileViewHandle`, `portable_task_group`/`suspendible_task_queue`, `MmdStorageManager`,
`DebugOutStream`(+`scoped_lock`), `Big`/`Big::UInt`, the LISP-object family
`ListObj`/`NumbObj`/`StrnObj`/`SymbObj`/`UI64Obj`, the hasher/equality functors,
`AbstrPropDef`, `TreeItemClass`, `TreeItemDualRefContextHandle`, `CopyPropsContext`,
`CallbackOutStreamBuff`, `UsingCache`, and the component structs `ElemAllocComponent`/
`IndexedStringsComponent`/`TokenComponent` — whose keep-justification comment
(`RtcComponents.h:22-24`, "so objects in other modules can derive from them") the merge
has invalidated.

Rtc exports no dead vftables (no class-level decoration there — §2); Shv does
(`ViewHost`, `Win32ViewHost`). A handful of dead exports are **data**
(`TreeItem::s_ConfigReadLockCount`, shv `s_DrawingSizeTresholdInPixels`, clc
`cog_lookup`) — see the data gotcha in §2.

### 1b. The other DLLs

- **Shv, 176/208 dead — worst ratio; sole consumer is GeoDmsGuiQt.exe (32 live).** The
  dead surface is coherent: the whole `Win32ViewHost` class (~30 symbols incl. vftable),
  the whole `Region` class (~25), `MenuItem`, `ViewHost`, ~64
  `GetDynamicClass`/`GetStaticClass` RTTI accessors across the view hierarchy, the DIP
  helpers, boost::asio `executor_function_view` instantiations leaked from the WMS code,
  and 5 dead `SHV_*` entries of `ShvDllInterface.h` (C++-typed, qtgui-only — not part of
  the C-ABI promise, droppable).
- **Stg, 61/87 dead**: all 25 storage-manager `GetDynamicClass`/`GetStaticClass`
  accessors, the whole `TNameSet` class, the `AbstrGridStorageManager` virtuals,
  `ViewPortInfoProvider`, `gdalThread`, and the grid-domain free functions
  (`GetGridData` ×2, `GridDomain`, `CheckedGridDomain`, `GetPaletteData`, …).
- **Stx, 7/15 dead**: `DataBlockTask` ctor, `StringProd::ProdStringLiteral1/2`,
  `problemlocAsString`, + 3 `DMS_*` C-entries (keep-list).
- **Clc, 6/17 dead**: `ClassifyLogInterval`, `ClassifyNZEqualCount`, `cog_lookup`
  (data), `XML_ReportAllOperGroups`/`OperGroup`/`Operator`.
- **Geo: 7 exports, all live** (`DMS_Geo_Load` + the `AbstrBoundingBoxCache` family) —
  already minimal; the model for what the others should look like.

The `GetStaticClass`/`GetDynamicClass` accessor pattern is the dominant dead-export
idiom in Stg and Shv: those classes register via static initializers inside their own
DLL, so the accessors never need external linkage.

### 1c. The C-ABI keep-list (decision: keep ALL)

166 of Rtc's 212 extern-"C" exports (`DMS_*` ×157, `DBG_*` ×3, `RTC_*` ×2, +4), plus 3
in Stx and 3 in Clc, are unimported in-tree but form the **published C ABI**: the
installer ships no import libraries and no headers, so out-of-tree consumers (e.g. the
LoadConfigFromPython component) can only reach GeoDMS via
`LoadLibrary`/`GetProcAddress` on exactly these undecorated names, which leaves no trace
in any import table. **Decision (user, 2026-08-16): the whole extern-"C"
`DMS_*`/`DBG_*`/`RTC_*`/`STG_Bmp_*` surface (~239 names across the Interface headers +
`StgBase.h:63-65`) is a permanent keep-list**; the de-export pass touches only
C++-mangled symbols (2565 of the 2744 dead). `ShvDllInterface.h`'s `SHV_*` entries are
C++-typed and qtgui-only — *not* covered by the promise.

## 2. Export-macro mechanics (how a de-export is done here)

Macros and their flip symbols:

| macro | defined at | flips on | notes |
|---|---|---|---|
| `RTC_CALL` | `rtc/dll/src/RtcBase.h:91-99` | `DMRTC_EXPORTS` | |
| `SYM_CALL` | `rtc/dll/src/sym/SymBase.h:15-25` | `DMSYM_EXPORTS` | |
| `TIC_CALL` | `rtc/dll/src/tic/TicBase.h:26-32` | `DMTIC_EXPORTS` | |
| `TICTOC_CALL` | `TicBase.h:34` | = `TIC_CALL` | "TODO G8: REMOVE", 31 uses |
| `CLC_CALL` | `clc/dll/include/ClcBase.h:18-24` | `DMCLC_EXPORTS` | |
| `GEO_CALL` | `geo/dll/src/GeoBase.h:14-22` | `GEO_EXPORTS` (def'd `GeoPCH.h:19`) | |
| `MDL_CALL` | `GeoBase.h:24-28` | `MDL_EXPORTS` | conditionally empty; 7 uses (Potential) |
| `STGDLL_CALL` | `stg/dll/src/StgBase.h:42-52` | `DMSTGDLL_EXPORTS` (def'd `StoragePCH.h:22`) | |
| `STGIMPL_CALL` | `StgBase.h:54` | — | unconditionally EMPTY — existing "internal" marker |
| `SHV_CALL` | `shv/dll/src/ShvBase.h:25-38` | `DM_SHV_EXPORTS` (def'd `ShvDllPCH.h:20`) | see the DM_SHV_DLL hole below |
| `SYNTAX_CALL` | `stx/dll/src/StxBase.h:14-20` | `DMSTX_EXPORTS` | |

DmRtc defines all three of `DMRTC_EXPORTS`/`DMSYM_EXPORTS`/`DMTIC_EXPORTS`
(`DmRtc.vcxproj:48`, `rtc/dll/CMakeLists.txt:23`), so `RTC_CALL`/`SYM_CALL`/`TIC_CALL`/
`TICTOC_CALL` are functionally one macro set (3026 decorated lines); any tool scanning
"the DmRtc export macro" must handle all four spellings.

Facts that shape the pass:

- **Decoration is per-member everywhere except four shv types**: `struct SHV_CALL
  MenuItem` (`MenuData.h:46`), `struct SHV_CALL Region` (`Region.h:43`), `class SHV_CALL
  ViewHost` (`ViewHost.h:26`), `class SHV_CALL Win32ViewHost` (`Win32ViewHost.h:18`).
  The vtable/partial-de-export complication exists only there — and all four are
  entirely dead, so the fix is simply dropping the class-level decoration.
- **RTTI accessors** go through `DECL_RTTI`/`DECL_ABSTR(CALLTYPE, CLASS)`
  (`mci/Object.h:57/:68`) which take the macro as a parameter (SHV 33+8, STG 15,
  TIC 10+4, RTC 6+3, SYM 5+1 uses). De-export = pass an empty CALLTYPE argument.
- **Template exports** arise two ways: decorated members of class templates, exported by
  every implicit instantiation (`mci/ValueWrap.h:29-53` — the AsCharArray family), and
  decorated free-function templates (`CreateHeapTileArray_impl`,
  `tic/DataArray.cpp:824`). On MSVC, dllexport *forces* instantiation of decorated
  members; the GCC/Linux counterpart is the `#if !defined(_MSC_VER)`
  explicit-instantiation blocks (`DataArray.cpp:1012-1026`, `Unit.cpp:1403-1430`).
  Removing a macro from a template member can therefore orphan intra-DLL users in other
  TUs on MSVC (no more forced instantiation) — per family, check whether explicit
  instantiations must be added (possibly by making the GCC blocks unconditional). Both
  the .dll and the .so build must be verified for these.
- **The macro is usually repeated on the definition** (257 `RTC_CALL` + 338 `TIC_CALL`
  `.cpp` lines) → two edits per symbol; but *data* definitions are not uniformly
  decorated (`LispTreeType.cpp:65` is, `TreeItemProps.cpp:833` is not) — locate
  definitions by symbol, not by expecting a decorated definition line.
- **Out-of-module hand-written re-declarations** exist at 7 sites and must be included
  in any rewrite: `qtgui/exe/src/DmsMainWindow.cpp:859/:1485/:1528`,
  `qtgui/exe/src/FindTreeItemWindow.cpp:14`, `stg/dll/src/gdal/gdal_base.cpp:387`,
  `stg/dll/src/DllMain.cpp:45`, `clc/dll/src/PhaseContainer.cpp:25`.
- **Exported data needs a higher bar.** ~205 exported-data declarations exist (13
  `RTC_CALL extern` incl. `UpdateMark.h` `tsLast`/`bCommitted` and `g_DebugStream`; 116
  `extern TIC_CALL TokenID` in `LispTreeType.h`; 11 `CommonOperGroup` in `OperGroups.h`;
  the 29 PropDef pointers of `TreeItemProps.h`/`AbstrDataItem.h`; …). A function
  de-export mistake is a clean link error; a *data* de-export mistake can be silent:
  the `s_IsDetectingIncInterest` hazard is still half-live — `stg/DllMain.cpp:45` and
  `clc/PhaseContainer.cpp:25` declare `RTC_CALL bool s_IsDetectingIncInterest;`
  **without `extern`** (they compile today only because dllimport implies extern; if the
  symbol were de-exported these would silently become private per-DLL copies). Fix the
  two sites up-front; symbol-name-wide grep before de-exporting any data symbol.
- **The DM_SHV_DLL hole**: `ShvBase.h:33` keys the consumer branch of `SHV_CALL` on
  `DM_SHV_DLL`, which is defined nowhere — consumers see `SHV_CALL` = *nothing* (not
  dllimport). Functions still link via import-lib thunks; exported shv *data* would
  silently break. Fix the else-branch to plain dllimport. Related: `ShvDLL.vcxproj:63`
  defines `SHVDLL_EXPORTS`, which nothing reads (dead define), and `rtc/dll/cpp.hint:8-9`
  still hard-codes the pre-merge state (IntelliSense-only, stale).
  *Step-A implementation note: fixing the else-branch immediately surfaced a real bug of
  exactly the predicted class — `DrawPolygons.h:35` held a decorated DEFINITION
  `SHV_CALL Float64 s_DrawingSizeTresholdInPixels = 0.0;` in a header included by both
  shv (FeatureLayer.cpp, which reads it while drawing) and qtgui (DmsOptions.cpp:247,
  which writes it from the options dialog). Under the empty-macro hole qtgui compiled
  its own private copy, so the GUI "drawing size threshold" option never reached the shv
  drawing code (dumpbin showed the export dead). Fixed: extern declaration in the
  header, one `SHV_CALL` definition in FeatureLayer.cpp; the dllimport branch turns the
  qtgui write into a store through the import, and the export becomes genuinely live.*
- **Convention for survivors**: symbols that stay exported for a single niche consumer
  get a keep-justification comment, e.g. `TreeItem.h:251`
  `// exported: shv GraphicContainer::SaveOrder calls it`.
- **Release import tables understate Debug-link needs** (found during B1): downstream
  references sitting in code that `/OPT:REF` dead-strips in Release still require the
  import in Debug links (e.g. `InpStreamBuff::~InpStreamBuff` and `ViewData::~ViewData`,
  reached only via inline dtors of otherwise-unused wrapper classes;
  `AbstrDataItem::GetDataRefLockCount`; `AsFileDateTime`). Consequence: the Debug-first
  build gate is not just an assert-catcher here — it is the authoritative link check;
  such symbols keep their export with an `/OPT:REF` keep-comment.
- **Decision (user, 2026-08-16): de-export = bare macro removal** (undecorated =
  internal, matching house style). No marker macro.

## 3. TU landscape

346 TUs / 174k LOC. Release `.obj` totals: **Clc 1584 MB** (73 objs), **Geo 373 MB**
(30), Rtc 148 MB (115), Shv 110 MB (72), GuiQt 89 MB, Stx 76 MB (13 TUs — highest
density after clc/geo), Stg 48 MB. In clc, LOC and obj size are *decorrelated*: the
17-25-LOC per-value-type stubs are the 40-84 MB stragglers (typelist instantiation
cost), so "split" there means splitting the *typelist*, not the text.

Top obj stragglers: `OperConvNumeric_sint` 84 MB, **`OperAttrBin` 81 MB (624 LOC, never
split)**, `RLookup`/`RLookup_sint` 77 MB, `BoostGeometry` 77 MB, `RLookup_numseq` 68 MB,
`BoostPolygon` 68 MB, `lookup_numseq` 66 MB, **`OperAttrUni` 52 MB (193 LOC, 33 oper
groups, never split)**, `OperPolygon` 45 MB.

### 3a. Split candidates (cohesion and/or obj size)

- **`rtc/tic/AbstrCalculator.cpp` 7072 LOC** — largest TU in the repo, but only 36
  `AbstrCalculator::` methods; the bulk is helper machinery. Four subjects: expression
  substitution (`SubstitutionBuffer`), meta-function expansion (`MetaFuncCurry`,
  `for_each`/`loop` at :6344), template instantiation, and the C-API (:6989). Line 69
  literally says `// Section: to be located into following code`.
- **`rtc/tic/TreeItem.cpp` 5883 LOC**, 180 methods; its own banners name the seams:
  RTTI(:249), ctor/dtor(:292), `TreeItemAdmLock`(:354 — a separate class),
  NameTreeReg(:628), parent&name(:643), containment(:653), meta-info query(:786),
  find(:2109), Actor callbacks(:2809). Split by concern into ~4 TUs.
- **`rtc/tic/OperationContext.cpp` 3530 LOC = two schedulers**: `OperationContext` and
  `tile_task_group` (banner :114 — a self-contained work-stealing pool with global
  registry). Move `tile_task_group` to `parallel/` (currently one 110-LOC TU).
- **`rtc/utl/Environment.cpp` 2881 LOC — still 5-in-1.** The header was split 4-way in
  the header-hygiene pass but the TU was not: it includes all five sibling headers
  (:15-19) and implements all five concerns twice (a ~1500-LOC `#if defined(_MSC_VER)`
  half and a ~1250-LOC Linux half, each with per-concern banners). Aggravating:
  `utl/Registry.cpp` (254 LOC) already exists, yet Environment.cpp *also* implements
  `GeoDmsRegKey*`/`RegStatusFlags`/the Linux INI backend — registry logic is split
  across two TUs on an arbitrary line. → 5-way split mirroring the headers, merging the
  registry parts into `Registry.cpp`.
- **clc**: `OperAttrBin.cpp` (81 MB) + `OperAttrUni.cpp` (52 MB, 33 groups) +
  `OperAttrVar.cpp` (28 groups) → per-value-type splits via the proven
  `RLookup_*`/`OperConv*` mechanism. `OperUnit.cpp` (1404 LOC, 23 oper groups, ≥8
  unrelated unit-operator families, mid-file `#include`s at :1129/:1278/:1279) is the
  worst cohesion in clc. `Modus.cpp` (939) mixes the entropy family with
  modus/unique_count/frequency_table. `OperConv.cpp` residual still holds 25 groups.
- **geo**: `BoostGeometry.cpp` (1777 LOC / 77 MB) is a *backend × operation* matrix in
  one TU — four backends (`bg_*` boost.geometry, `bp_*` boost.polygon, `cgal_*`,
  `geos_*`) × four operations. Split by backend = the biggest compile-time win in geo
  (each TU then pulls only one heavy third-party header set). `OperPolygon.cpp`
  (2488 LOC / 47 groups) mixes five unrelated families (point-in-polygon /
  bounds-centroid / length-area metrics / sequence construction ×20 groups /
  dyna_point ×8) → 5-way split. `DiscrAlloc.cpp` (4127) and `GridDist.cpp` (719) are
  big but cohesive — leave (or split DiscrAlloc only by size).
- **`shv/ShvUtils.cpp` 1259 LOC, 45 includes** — the shv grab-bag, ≥6 unrelated
  families → `ShvSync.cpp` (DialogData/SyncRef/SyncValue persistence), GDI+DPI utils,
  `ShvPalette.cpp` (palette + class-break creation — the reason shv pulls clc's
  `CalcClassBreaks.h`), desktop-container/naming utils.
- **`stg/DllMain.cpp` 695 LOC, seven subjects**: `DMS_Stg_Load` / stream-type helpers /
  table-domain validation / geo-ref file I/O (:128-330) / `TNameSet` (:331-488) /
  `CreateTreeItemColumnInfo` / `ViewPortInfoEx<Int>` (:535-625 — what geo consumes).
- **`stx/ConfigProd.cpp` 1419 LOC**: the function/lambda machinery (:719-1290, ~360
  LOC, the newest block) is a clean `ConfigProd_functions.cpp` extraction.

### 3b. Merge candidates

124 TUs are <200 LOC (excluding the 9 PCH stubs); realistic yield ~60-70 fewer compiler
invocations. **Never merge the deliberate per-value-type obj-size splits**
(`lookup*`/`RLookup*`/`OperConv*` — that would re-create a 700 MB obj).

- rtc `act/` small ×5 (`garbage_can` 50, `ActorEnums` 59, `Waiter` 97, `ActorSet` 105,
  `InvalidationBlock` 40) → `ActorSupport.cpp` (unless InvalidationBlock moves to shv,
  §4).
- rtc `tic/` small ×15 (`DataLockContainers` 80, `LispContextHandle` 82,
  `FreeDataManager` 84, `Aspect` 98, `Crs` 96, `Param` 109, `SupplCache` 138,
  `StateChangeNotification` 144, `DataStoreManager` 151, `DisplayValue` 163,
  `ExprRewrite` 176, `TreeItemContextHandle` 178, `TreeItemUtils` 186, `ExtLockMgr` 185,
  `AbstrDataObject` 198; 2068 LOC) → 3-4 grouped TUs — the biggest single
  invocation-count reduction in the repo.
- rtc `tic/stg/` small ×4 → `StorageSupport.cpp`; `sym/` (`Assoc` 39, `Parser` 110) →
  fold into the Lisp TUs; `xml/XmlConst` 39 → `XmlParser.cpp`; `ser/`
  (`BaseStreamBuff` 72, `AsString` 133) → `MoreStreamBuff.cpp`; `vt/`
  (`iterrangefuncs` 26, `BaseBounds` 32) → fold.
- clc `OperAcc` family ×6 (`OperAccBin` 33, `OperAccCount` 33, `OperAccFirstLast` 20,
  `OperAccUniStatistics` 55, `OperAccMinMax` 141, `OperAccUniStr` 292) → `OperAcc.cpp` —
  **only after a limits pre-check**: these were separated once, in the (abandoned) x86
  era, precisely for compiler/linker limits. The MSVC section-count limit is covered
  (`/bigobj` is global: `DmsDef.props:74`, `CMakeLists.txt:83`), so the pre-check is:
  sum the six current obj sizes, trial-compile the merged TU in .m Debug+Release and in
  GCC (WSL .l) watching compile RAM; abandon if the combined obj lands in the straggler
  class (~>40 MB) or GCC memory spikes. The same pre-check applies to any merge of
  template-heavy TUs.
- clc misc small ×10 (`AnyAll`/`Sort`/`Checker`/`SubItem`/`ExprCalculator`/
  `ClassBreak`/`ID`/`Loop`/`Ramp`/`SeparableMapping`, 1197 LOC) → 2-3 thematic TUs.
- shv small ×13 (907 LOC) → 2-3; qtgui small windows ×7 (600 LOC) →
  `DmsSmallWindows.cpp` (mind Qt AUTOMOC); geo small ×4 (255 LOC) → `GeoSupport.cpp`;
  stx: at most `StringProd`+`SpiritTools` (Spirit obj density warning).
- Rtc carries three PCH stubs (`RtcPCH`/`SymPCH`/`TicPCH`) — a merge leftover; whether
  one PCH could serve the whole DLL is a possible follow-on, not part of this pass.

### 3c. Rename candidates (name lies about content)

- `rtc/utl/StrFormat.cpp`: 12 of its 16 functions are `splitPath.h` path helpers and 2
  are `FixedBufferFormat.h` functions → split into `splitPath.cpp` +
  `FixedBufferFormat.cpp` (StrFormat.h itself keeps only 2 declarations).
- `rtc/tic/ExtLockMgr.cpp` (C-API AddRef/Release; no such header),
  `tic/DataStoreManager.cpp` (does not include its sibling header; implements
  `DataStoreManagerCaller.h` + the analysis-target functions), `dbg/DebugStream.cpp` →
  `MsgDispatch.cpp` (implements `DbgInterface.h`), `mci/persistent.cpp`
  (`DMS_Class_*`/`DMS_Object_*` C-API), `tic/attr_Interface.cpp` (the only
  lowercase-prefixed file; `DMS_DataItem_*` C-API — rename or fold into
  `TicInterface.cpp`), `tic/SourceDescr.cpp`, `ptr/SharedObjBase.cpp` (implements
  `SharedBase.h` plus lock-level machinery that belongs in `lock/`),
  `gen/General.cpp` (the version C-API as sole occupant of a dir named `gen`).
- stg: `dbfImp.cpp` vs `dbfImpl.h` (every other backend is `XxxImp.h`); lowercase
  `dbfImp`/`dbfStorageManager`; `ODBCImp` vs `OdbcStorageManager` casing clash.
- clc: `lookup*` (lowercase) vs `RLookup*` (upper) for a matched operator pair;
  `random.cpp`, `regex.cpp` lowercase among UpperCamel siblings.
- Several TUs do not include their same-named sibling header (`lookup.cpp`,
  `SeparableMapping.cpp`, geo `AbstrBoundingBoxCache.cpp`, stx `DataBlockProd.cpp`,
  rtc `dbg/DebugContext.cpp`) — fix or rename.

Verified clean: no case-mismatched `.h`/`.cpp` sibling pairs remain, and no duplicate
basenames across projects (the flat include-dir list is collision-free — keep it so).

## 4. Cross-DLL moves (reduce exports/imports and dependency edges)

Cross-project include edges (consumer → owner, include-site counts): shv→rtc 656,
clc→rtc 631, stg→rtc 411, geo→rtc 340, qtgui→rtc 141, stx→rtc 131, qtgui→shv 33,
geo→clc 29, shv→stg 24, shv→clc 12, shv→geo 8, **shv→qtgui 2 (upward — layering
violation)**, clc→stg 4/5, geo→stg 4.

- **stg surface used only by shv/qtgui**: `GetUnitSizeInMeters(const AbstrUnit*)`
  (declared `gdal_base.h:182`; only shv calls it — already flagged in the header-hygiene
  doc) and `GetBaseProjectionUnitFromValuesUnit` (shv only) → extract a thin
  `stg/ProjectionUnits.h`; `STG_Bmp_Get/SetDefaultColor` (shv resp. qtgui only) →
  `stg/DefaultColors.h`. This cuts all 6 shv→`gdal_base.h` include edges (GDAL/OGR
  headers out of the viewer). `ViewPortInfoEx<Int>` (defined in stg `DllMain.cpp`!) is
  consumed by stg+geo → its own TU in stg (§3a split).
- **rtc `geom/` headers**: `Area.h`/`CalcWidth.h`/`Centroid.h`/`DynamicPoint.h`/
  `GeoDist.h`/`IsInside.h` have ZERO rtc-internal consumers (shared geo+shv library
  living in rtc — acceptable); but **`BoostPolygon.h`, `NeighbourIter.h` and
  (effectively) `SpatialIndex.h` are geo-only → move to the geo DLL**. The Rtc vcxproj
  filter `ipolygon` is likely stale after this.
- **clc public headers with zero clc users**: `clc/dll/include/InvertedRel.h` (1 geo
  user) → geo; `RemoveAdjacentsAndSpikes.h` (geo ×4 + shv ×1) → rtc `geom/` (shared).
- **rtc TUs whose header has ~zero rtc-internal use and exactly one downstream
  consumer** (moving them internalizes their exports): `utl/TypeInfoOrdering.cpp` → shv,
  `tic/DedicatedAttrs.cpp` → shv, `act/InvalidationBlock.cpp` → shv,
  `tic/stg/AbstrStreamManager.cpp` → stg (503 LOC total). Other single-consumer headers
  (`Crs.h`, `Case.h`, `AsmUtil.h`, `garbage_can.h`, `DebugReporter.h`, `ActorEnums.h`,
  `SupplCache.h`, `CopyTreeContext.h`, `LispContextHandle.h`, `Mathlib.h`, `OLEPtr.h`,
  `SeqLock.h`, `TimeFmt.h`, `Cache.h`, …) have 3-13 rtc-internal users — they stay.
- ~~**stx/DijkstraString.cpp** (226 LOC) + `DijkstraFlags.h` → move to geo.~~
  **Reconsidered during step C**: the parser uses stx-internal Spirit machinery
  (`SpiritTools.h`); moving it would drag boost::spirit into geo. Stays in stx; the cost
  is a single imported symbol (`ParseDijkstraString`).
- ~~**Layering violation**: shv includes qtgui's `resource.h`.~~ **Retracted during
  step C**: `../res/resource.h` in `TableControl.cpp`/`Win32ViewHost.cpp` resolves to
  shv's OWN `shv/dll/res/resource.h` (cursor resources) — no upward edge exists.
- **clc → stg edges — inspected, legitimate**: `OperConv.h` genuinely uses GDAL/PROJ
  (`<gdal_priv.h>`, `<proj.h>` — projection conversion operators); `ReportFunctions.cpp`
  includes the gdal surface for the storage-manager doc generator; `OperExec.cpp` uses
  `ODBCStorageManager::GetDatabaseFilename` by design. No thin-header extraction
  available; left as-is.
- ~~rtc `tic/DedicatedAttrs.cpp` → shv~~ **Blocked during step C**: the TU carries
  published `DMS_*` C-API entry points (`DMS_DataItem_VisitClassBreakCandidates`,
  `DMS_Unit_GetNrDataItemsIn/Out`, …) — moving it would relocate C-ABI exports from
  Rtc.dll to Shv.dll, which the keep-all decision forbids. Stays in rtc.

Bookkeeping: every add/move/rename lands in BOTH the authored `.vcxproj`(+`.filters`)
AND the module `CMakeLists.txt`. Rtc/Stg filters have real taxonomies (19 resp. 10
groups); Clc/Geo/Shv/Stx/qtgui are flat `Source Files` — introduce real taxonomies
there while touching them (clc: Attr/Aggregation/Conversion/Relational/Sequence; geo:
Polygon/Network/Grid/Allocation/Backends).

## 5. Merge-feasibility: one DLL for rtc+clc+geo? (analysis; deferred)

The consumer-set data (§1) makes the question unavoidable: ~2100 of Rtc's live exports
serve only Clc/Geo, and Clc+Geo generate 71% of all cross-DLL import traffic. Measured
limits (2026-08-16):

| PDB | Release | Debug |
|---|---:|---:|
| Clc.pdb | 916 MB | **3198 MB** |
| Geo.pdb | 204 MB | 712 MB |
| Rtc.pdb | 53 MB | 138 MB |
| sum | 1173 MB | **4048 MB** |

- **The PDB cap is the binding constraint.** MSF default page size 4096 → 4 GB PDB cap;
  no `/pdbpagesize` is set anywhere in the repo. Debug Clc.pdb alone is at 3.2 GB (78%
  of the cap); incremental Debug links grow the PDB through free-page fragmentation —
  which is exactly the observed "clc.pdb too large after some incrementations" error. A
  merged Debug PDB (~4.0 GB before type dedup) crosses the cap immediately.
- **Mitigation**: `/pdbpagesize:16384` raises the cap to 16 GB — a props one-liner that
  also fixes today's recurring clc.pdb error, so it is adopted in step A regardless of
  any merge. Tool support verified: the installed CDB/DbgHelp/DbgEng are
  10.0.26100.8249 (Windows 11 SDK; large-page-PDB support landed around 10.0.22000) and
  the VS18 debugger/DIA are of the same generation as the linker offering the switch.
  `/DEBUG:FASTLINK` for the Debug flavour is a further option (types stay in the objs;
  much smaller PDB; slower debugger startup). Type dedup in a merged PDB would land
  well under the 4 GB sum (clc/geo massively re-instantiate the same rtc templates).
- **Non-issues**: PE SizeOfImage cap 4 GB vs a merged DLL of ~65 MB (45.8+14.3+4.9);
  export-ordinal limit 65,535 vs ~6k (falling after the de-export pass); 64-bit linker
  memory over the ~2.1 GB obj input.
- **The real cost is recurring, not a hard limit**: today an edit in clc relinks only
  Clc.dll; merged, every edit in rtc/clc/geo pays one big link (Release /OPT:REF/ICF
  over 2.1 GB of obj plus a ~1 GB PDB write). Plus one-off rework: static-init order
  audit (the rtc+sym+tic playbook exists: StaticTokenID / Meyers singletons),
  the `DMS_Clc_Load`/`DMS_Geo_Load` plugin-load protocol, installers, CMake+sln.

**Verdict (user-agreed): technically feasible with `/pdbpagesize` — but not in this
pass.** Execute de-export + targeted moves + splits first; they shrink the surface and
sharpen the numbers. Tracked as its own GitHub issue referencing this section and
`compile-time-refactor-analysis-2026-07.md` Finding 3.

## 6. Implementation ladder

Gates (user-specified): per step, **build the Debug config first and run
`batch\TestDebugUnit.bat`** (stated asserts may be missed in Release); green → next
step. After the last step: build Release (.m), then the .c flavour (CMake
windows-x64-release) and the .l flavour (WSL) to catch flavour-specific build errors and
fix them; when all green, run `full.py` on the .m flavour locally, plus the testcases
battery. `/bigobj` may be set additionally wherever a merge/split needs it.

| step | content | status |
|---|---|---|
| 0 | this document; gh: comment on #1105 (pyd↔python313 mismatch), new issue for the rtc+clc+geo merge follow-up | — |
| A | hygiene pre-fixes: `extern` on the 2 tentative definitions; `SHV_CALL` dllimport else-branch (DM_SHV_DLL hole — surfaced + fixed the DrawPolygons.h private-copy bug, see §2); drop dead `SHVDLL_EXPORTS` define; stale `cpp.hint`; `RtcComponents.h` comment; **`/pdbpagesize:16384`** in DmsDef.props + top CMakeLists (note: the page size only applies when a PDB is CREATED — existing PDBs must be deleted once; verified 16384 + cdb reads them, "private pdb symbols") | ✅ |
| B | de-export, entirely-dead identifiers only, C-ABI keep-list excluded: B1 plain functions/members; B2 template families (instantiation care, .l check); B3 data symbols (symbol-wide grep each); B4 `DECL_RTTI` empty-CALLTYPE; B5 shv class-level decorations; B6 dumpbin re-sweep, record numbers | ✅ |

Step-B outcome (measured on the Debug binaries, 2026-08-16): **exports
5999 → 4070 (−1929)** — Rtc 5672→3930, Shv 208→84, Stg 87→27, Stx 15→12, Clc 17→17,
Geo 7→7. ~990 decorated lines stripped (Rtc ~900, Stg 47, Shv 53, Stx 4, Clc 1) + 72
RTTI accessor pairs via an empty `DECL_RTTI`/`DECL_ABSTR` CALLTYPE argument +
Win32ViewHost class decoration dropped. No template-instantiation fallout: the
`#if !defined(_MSC_VER)` blocks needed NO extension — removing dllexport-forcing left
every intra-DLL use satisfied, matching the GCC-parity argument (GCC never had the
forcing and links today). 17 symbols/classes were restored with `// exported:`
keep-comments after Debug link errors: 14 members/free functions whose downstream
references are /OPT:REF-stripped in Release, plus the shv `Region`/`MenuItem`/`ViewHost`
class decorations (qtgui's QDmsViewArea uses them; RELEASE INLINING of small members
hides those imports from the Release evidence — a second understatement mechanism
besides /OPT:REF). The strip machinery honours `// exported:` comments as a durable
keep-marker; `s_DrawingSizeTresholdInPixels` was excluded as its liveness changed in
step A. Residual: 52 entirely-dead C++ identifiers (170 symbols) in Rtc whose decorated
lines the scope-aware mapper could not match safely (UNMATCHED) + 44 dead Shv symbols
(members of the restored class-decorated types) — optional second pass, low value.
Fresh evidence + maps live in `scratch/deexport*` (gitignored); tooling in the session
scratchpad (`deexport_evidence.py`, `deexport_map2.py`, `deexport_strip.py`).
| C | cross-DLL moves + edge hygiene (§4); dumpbin re-sweep for newly-dead exports | ✅ (ProjectionUnits.h extraction, geom→geo ×3 + InvertedRel→geo + RemoveAdjacentsAndSpikes→rtc geom/, TypeInfoOrdering+InvalidationBlock→shv, AbstrStreamManager→stg; DedicatedAttrs blocked by its DMS_* C-API entries, DijkstraString and the clc→stg edges reconsidered — see §4 strikethroughs) |
| D | TU splits (§3a) | ✅ for this pass — DONE (7): clc OperAttrBin (Impl.h + _muldiv/_addsub/_compare/_bits; residual = strings/pow/units), clc OperAttrUni (→ OperAttrUni_str.cpp), geo BoostGeometry (BoostGeometryImpl.h + bg/bp/cgal/geos TUs), stg DllMain (→ GeoRef/NameSet/TreeItemColumnInfo/ViewPortInfoEx.cpp), rtc tile_task_group (→ tic/ParallelTiles.cpp), stx ConfigProd typed-HOF block (→ ConfigProd_functions.cpp), shv ShvUtils (→ ShvSync/ShvGdi/ShvDesktopData.cpp). SKIPPED after inspection (recorded reasons): OperAttrVar (one cohesive argmin/argmax family, 19.9 MB obj); geo OperPolygon and clc Modus (their per-type instantiations are BUNDLED in aggregate structs — SequenceOperators/GeometricOperators resp. the AggrFuncInst bundle — a family split first needs aggregate restructuring); clc OperUnit (operator templates interleave across its sections; needs template-to-header extraction first); OperConv residual (small). **DEFERRED to a dedicated follow-up session** (dense file-local-static graphs need mapping before any cut): tic TreeItem.cpp (20+ static helpers, 4 anon namespaces), tic AbstrCalculator.cpp, utl Environment.cpp (dual-platform halves + shared statics like s_RegAccess) |
| E | TU merges (§3b, with the template-TU limits pre-check) | ✅ — ~49 fewer TUs: rtc tic 15→3 (TicData/TicItem/TicCalcSupport), act 4→1 (ActorSupport), tic/stg 3→1 (StorageSupport), ser BaseStreamBuff+AsString→MoreStreamBuff, sym Assoc→LispEval, vt 2→1 (VtSupport), xml XmlConst→XmlParser, shv 13→2 (ShvControlsSupport/ShvDrawSupport), geo 4→1 (GeoSupport), clc misc 10→2 (OperMisc/OperMappings, ~22 MB combined objs), stx StringProd→SpiritTools, qtgui 7→1 (DmsSmallWindows; no moc in the .cpps). **OperAcc merge ABANDONED by the pre-check**: its six Release objs total 90.4 MB — merging would re-create the obj-size straggler the separation prevents (the x86-era split still pays, now for obj size). Merge-tool gotchas: interior UTF-8 BOMs must be stripped; bare `#pragma hdrstop` and PCH-less (/FI) TUs need prolog-cut fallbacks |
| F | TU renames + non-included-sibling fixes + filter taxonomies (§3c) | ✅ renames done (DebugStream→MsgDispatch, persistent→MciInterface, attr_Interface→AttrInterface, SharedObjBase→SharedBase, StrFormat.cpp path helpers→splitPath.cpp, dbfImpl.h→dbfImp.h, clc lookup family→Lookup* + Random/Regex casing; ExtLockMgr/DataStoreManager were absorbed by the E merges). DEFERRED as cosmetic follow-ups: filter taxonomies for the flat projects (clc/geo/shv/stx/qtgui), gen/General.cpp + tic/SourceDescr.cpp renames, non-included-sibling include fixes (stx DataBlockProd, rtc dbg/DebugContext) |

Every new/moved TU gets the `.cpp` prolog convention and touched headers the header
prolog convention (`header-hygiene-2026-08.md` §7); new names follow the
abbreviation-case rule; same-family splits follow the underscore-suffix precedent
(`RLookup_sint.cpp`). Expected end state: Rtc ~5672 → ~3550 exports (the ~2100
C++-mangled entirely-dead symbols go; the 179 C-ABI names and the partially-dead
template tails stay), Shv 208 → ~37, Stg 87 → ~26; de-exported dead code additionally
becomes /OPT:REF-strippable, so a small Release binary-size drop is expected and will be
recorded here.

**Final Release numbers (measured after the full ladder, 2026-08-16)**: total exports
**5999 → 3984 (−2015, −34%)** — Rtc 5672→3838, Shv 208→84, Stg 87→27, Stx 15→12,
Clc 17→16, Geo 7→7. Residual Rtc dead = 669: the 166-name extern-"C" keep-list, the
partially-dead template tails (~380, macro must stay on the template member), and 123
symbols of mapper-UNMATCHED lines (optional second pass). Binary-size effect:
Rtc.dll 4.9→4.7 MB, Shv.dll 3.5→3.4 MB (dead code became /OPT:REF-strippable);
Release Clc.pdb 916→854 MB.

No wiki page: nothing in this pass changes modeller-observable behaviour.
