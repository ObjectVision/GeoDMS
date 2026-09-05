# Code fixes: latent bugs, clarifications and renames — a phased pick-list

*2026-09-05, branch `main`, HEAD `27817fcb`. Produced by a two-wave agent review: pattern sweeps over
all nine module trees, then per-module verification that re-read every candidate in context. Line
numbers are leads at this HEAD, not gospel; every item names the function so it can be re-found.*

## How to read this

- **ID** — module prefix (`RTC` runtime core, `TIC` tree/calc, `STX` parser, `CLC` operators, `GEO`
  geometry, `STG` storage, `SHV` viewer, `QT` Qt GUI; `-Nnn` = found during verification, not by the
  sweep; `R`/`C` = rename / clarification batches).
- **Verdict** — CONFIRMED (trigger path read end to end), LIKELY (defect is real, trigger needs an
  unusual but reachable input or timing), SMELL (correct today, but fragile or misleading).
  REFUTED candidates are listed in Appendix A so nobody re-audits them.
- **Severity** — impact if triggered × plausibility. Critical = silently wrong results or memory
  corruption from ordinary input.
- Each item heading reads `ID · verdict / severity · effort / fix-risk — title`. Effort S (< 1 h),
  M (half a day), L (days). Fix-risk = chance the fix changes behaviour someone depends on.
- Phases are ordered by value ÷ risk. Inside a phase every item is independent unless a
  "prerequisite" says otherwise; each phase can be split into one commit per item or per file.
- Verification tiers: **T0** build `all22.sln` Debug + Release via the committed scripts;
  **T1** unit exes (`batch\TestDebugUnit.bat` / `TestReleaseUnit.bat`);
  **T2** `testcases\run_testcases.bat`; **T3** serializer round trip; **T4** GUI smoke.

### Why `dms_assert` matters for almost every item

`dms_assert(E)` is `assert(E)` in Debug but `CC_ASSUME(bool(E))` in Release
(`rtc/dll/src/dbg/Diagnostics.h:84,91-92`; `cpc/CompChar.h:52,125`), i.e. MSVC `__assume` / GCC
`__builtin_unreachable`. That is **not a no-op**: a false condition is undefined behaviour, and the
optimizer may delete the code that would have handled it. Only `MG_CHECK*`, `MG_USERCHECK*`,
`MG_PRECONDITION*`, `dms_check`, `FileResult::require` and `MG_ASSERT` survive in Release.
`dbg_assert`, `MG_DEBUGCODE`, `MGD_*` compile to nothing. Many items below are "an invariant over
external bytes is guarded only by `dms_assert`".

### Not in this document (already tracked elsewhere)

Recursion depth and the 64 MB stack (`RECURSION_REFACTOR_PLAN.md`), Win32 leakage / Linux port
(`PORTING_STATUS.md`), build-system drift, Boost Spirit V1, CI (`TECH_DEBT_REVIEW.md`), the header
renames of `doc/development/header-hygiene-2026-08.md`, the G8 backlog incl. `DataArray→TileFunctor`,
`AbstrCalculator→AbstrExprKey`, `DataReadLock→…Handle`, `CopyData` ignoring `DomainChangeInfo`
(`g8-todos.md`), TU reorg / export surface, pointer-safety items (all fixed 2026-07), std-ptr
migration residue, config/cache separation, typed-HOF, deadlock rules (`doc/deadlocks.md`), the
security backlog (WMS TLS, GDAL allowlist, raster overflow, temp names), and the open GitHub issues
in `doc/issues.md`. The Stage-0 format-string bugs of the boost-format migration doc are fixed.

---

$1
**Status 2026-09-05:** every item below except STG-14 is implemented, built (Release x64) and
committed as 3d0db896 on 2026-09-05; the battery runs after Phase 5. STG-14 was refuted while implementing: see its entry. The
regression configs `testcases/combine_uint8_empty.dms`, `dyna_point_dist.dms`,
`dyna_point_zero_dist_neg.dms`, `indirect_cycle_neg.dms` and `diversity_circle.dms` were written but
have not been run either. No TIFF, registry or XML fixture was added: those need binary or
machine-state inputs.

Overview (details follow; all S effort, low fix-risk):

| ID | Severity | Title |
|---|---|---|
| GEO-27 | **Critical** | Circle kernel predicate corrupts `diversity(…, r ≥ 4, true)` |
| STG-N02 | High | ModelTransformation `TIFFGetField` vararg misuse (rotated GeoTIFF) |
| STG-13 | Medium | Optional TIFF tags read into uninitialised variables |
| STG-14 | refuted | TIFF read error `-1` as a byte count: the consumer already resets it to 0 |
| CLC-25 | High | `combine08/16` with an empty unit divides by zero |
| RTC-33 | High | Registry REG_SZ round trip drops the last character |
| RTC-07 | Medium | `cs_lock_map::TryLock` leaks a refcount on contention |
| RTC-01/03/04/N02 | High | XML entity stack overflow, dangling map keys, EOF loop |
| SHV-42 | Medium | One HFONT leaked per paint |
| SHV-44 | Medium | WMS zoom-out crash with an empty tile-matrix set |
| GEO-29 | Medium | `dyna_point` distance never validated (inf/NaN → SizeT) |
| GEO-30 | Medium | `dyna_point_with_ends` null deref on void-domain parameters |
| TIC-07 | Medium | Self-referential indirect expression loops forever |
| STG-23 | Medium | `fwrite` result ignored in the str storage manager |
| QT-59 | Medium | Tree-view `parent()` row disagrees with `index()` |
| TIC-04 | Low | Two tile helpers miss the null check their siblings have |

### GEO-27 · CONFIRMED / Critical · S / low — Circle kernel test compares each coordinate with its own chord
- **Where:** `geo/dll/src/SpatialAnalyzer.cpp:58-59` `TForm::ContainsCentered`; `m_CirclePoint` defined :35-41, used correctly at :64-71, :119-141.
- **Defect:** `m_CirclePoint[i] = floor(sqrt(r²-i²))` is the maximal *other* coordinate at offset i (exactly how `GetOtherCoordinateCentered` uses it). The test `abs(p.Row()) > m_CirclePoint[abs(p.Row())]` and the same for Col instead checks "|row| > r/√2" and "|col| > r/√2": an inscribed square, non-monotone along a row. `NextContainedPoint` abandons a row at the first miss, so the initial `DiversityCountAll` misses in-grid cells for r ≥ 4; the (correct) incremental border updates later subtract cells that were never added, `divVector` underflows, and every `diversity(…, r, true)` value after that is wrong. For r ≤ 3 the missed cells lie above the grid, which is why small-radius tests pass.
- **Fix:** replace both lines by the single symmetric test `if (abs(p.Col()) > m_CirclePoint[abs(p.Row())]) return false;` and add a comment defining `m_CirclePoint`.
- **Test:** brute-force disc count vs `diversity(grid, ub, r, true)` for r = 1..8 on a random 40×40 grid; add to `testcases/`.

### STG-N02 · CONFIRMED / High · S / low — `TIFFGetField(ModelTransformationTag)` called with the wrong vararg signature
- **Where:** `stg/dll/src/tif/TifImp.cpp:213` (correct siblings :227-236); consumer `stg/dll/src/tif/TifStorageManager.cpp:440-446`.
- **Defect:** the call passes a single `double*` (`transform.begin()`), but tag 34264 is a pass-count field whether registered by libgeotiff/GDAL's extender or created by libtiff as an anonymous field. libtiff then writes the count through the first vararg and the data pointer through a **non-existent second** vararg (garbage register/stack slot): undefined behaviour, and even when it survives `transform` holds zeros plus a small integer, so the georeference derived at :216-221 is nonsense. Trigger: any GeoTIFF carrying a ModelTransformation tag (rotated/sheared rasters, `gdalwarp` output with rotation).
- **Fix:** `uint16_t n = 0; double* m = nullptr; if (TIFFGetField(h, TIFFTAG_ModelTransformationTag, &n, &m) && n == 16) { push m[0], m[1], m[4], -m[5], m[3], m[7]; }`. Use `uint16_t` for the tie-point/scale counts at :228 too (libgeotiff registers them `TIFF_VARIABLE`; the `uint32_t count` only works because it is zero-initialised).
- **Test:** a small rotated GeoTIFF in the tif tests, geotransform compared with GDAL's.

### STG-13 · CONFIRMED / Medium · S / low — Optional TIFF tags read into uninitialised variables
- **Where:** `stg/dll/src/tif/TifImp.cpp:497` (RowsPerStrip), `:523-527` (BitsPerSample, PlanarConfig, SamplesPerPixel); benign at :475/477/494 (required tags).
- **Defect:** `TIFFVGetField` returns 0 and leaves the argument untouched when the tag is absent; `UInt32 result, rps, il;` and `UInt16 bps, spp, config;` are uninitialised. Single-strip, bilevel/fax and single-band files omit exactly these tags, so `Min<UInt32>(rps, il)` yields a garbage tile height (wrong strip geometry in `GridStorageManager.h:142-233`) and `bps *= spp` a garbage bit depth.
- **Fix:** initialise to the TIFF defaults before the calls: `rps = il; bps = 1; spp = 1; config = PLANARCONFIG_CONTIG;`.
- **Test:** an 8-bit single-strip TIFF without RowsPerStrip; a 1-bit TIFF without BitsPerSample.

### STG-14 · REFUTED on implementation (2026-09-05) — TIFF read error `-1` as a byte count
- **Where:** `stg/dll/src/tif/TifImp.cpp:666-672` `ReadTile`; consumer `stg/dll/src/GridStorageManager.h:183-220` `ReadTiles`.
- **Why refuted:** `ReadTiles` stores the result in `Int32 read_result`, and the `else read_result = 0;` at `GridStorageManager.h:214-215` belongs to `if (read_result > 0)`: a negative result is reset to 0 and the whole tile is filled with `defaultColor`, while libtiff's error message is captured by the `TifErrorFrame` and thrown afterwards. No pixels of a previous strip are copied.
- **Residue:** the implicit `SizeT → Int32` conversion of the return value (make it explicit; SMELL).
### CLC-25 · CONFIRMED / High · S / low — `combine08/16` with an empty argument unit divides by zero inside a lazy tile functor
- **Where:** `clc/dll/src/OperUnit.cpp:150-185` (functor :159-178); `rtc/dll/src/tic/TiledRangeData.h:164-165`; `rtc/dll/src/tic/Unit.cpp:786-787, 1407-1413`.
- **Defect:** with any argument unit of count 0, `productSize` becomes 0 and `cycleSize = groupSize * unitCount` is 0 for that and all later sub-items; the functor does `SizeT cyclePos = tileStart % cycleSize;`. For `combine`/`combine32/64` the empty result range has 0 tiles so the functor never runs, but `combine08/16` (`UInt8`/`UInt16`, `has_small_range_v`) install a `SmallRangeData` whose `GetNrTiles()` is always 1 with `GetTileSize(0) == 0`; `LazyTileFunctor::GetTile(0)` allocates the empty tile and calls the apply-func, which executes the `%` before the (empty) row loop: integer division by zero, hardware trap.
- **Fix:** start the functor with `if (!tileSize) return;` (and/or guard `cycleSize == 0` before the `%`).
- **Test:** `combine08(emptyUnit, other)` reading `first_rel` in `testcases/`.

### RTC-33 · CONFIRMED / High · S / low — Registry REG_SZ round trip loses the last character
- **Where:** `rtc/dll/src/utl/Registry.cpp:151-156` `WriteString`, `:122-140` `ReadString`.
- **Defect:** `WriteString` passes `wcslen(strW) * sizeof(wchar_t)` — **without** the terminating NUL that `RegSetValueEx` requires for REG_SZ — so the registry stores 2n bytes. `ReadString` computes `nr_wchars = n`, then unconditionally `--nr_wchars; assert(wcharResult[nr_wchars] == 0)` and converts n-1 characters. Values written by the GUI (`LocalDataDir`, `SourceDataDir`, `DmsEditor`: `qtgui/exe/src/DmsOptions.cpp:538-543`, `DmsMainWindow.cpp:364,1644`) are read back by `GetGeoDmsRegKey` one character short; in Debug the assert fires. Values written by regedit or the installer (with NUL) read correctly, which is why it goes unnoticed.
- **Fix:** write `(wcslen+1)*sizeof(wchar_t)`; in `ReadString` strip the last wchar only if it is 0.
- **Test:** set LocalDataDir in the Options dialog, restart, expand `%localDataDir%`.

### RTC-07 · CONFIRMED / Medium · S / low — `cs_lock_map::TryLock` leaks the per-key refcount on contention
- **Where:** `rtc/dll/src/cs_lock_map.h:169-182`; user `rtc/dll/src/tic/TreeItemDataUsage.cpp:507` (`PrepareDataUsageImpl`).
- **Defect:** `GetorCreateMutex` does `++m_Counter`; when `try_lock()` returns false the function does `return {};` without `ReleaseMutexRef(ptr)` — only the `catch` path releases, and `~ScopedTryLock` releases only when it holds the lock. Every contended `ScopedTryLock` on the hot MT path leaks one count: the node is never erased, and `~lock_value` (`dms_assert(m_Counter == 0)`) fires at static destruction in Debug.
- **Fix:** `if (!ptr->second.m_Lock.try_lock(...)) { ReleaseMutexRef(ptr); return {}; }`.
- **Test:** Debug run of a config with MT2 on: `scm_TileAccessLocks` must be empty at exit.

### RTC-01 + RTC-03 + RTC-04 + RTC-N02 · CONFIRMED / High · S / low — XML entity parsing: stack overflow, dangling map keys, EOF-dependent loops
- **Where:** `rtc/dll/src/xml/XmlParser.cpp:111-123` `TransformChar`, `:320-323` `SymbolGetChar`, `:316-317` globals, `:114/:132` loop conditions; `xml/XmlConst.h:12` (`MAX_TOKEN_LEN = 4`); reachable from any `.xml` configuration via `stx/dll/src/StxInterface.cpp:259`.
- **Defect (a):** `char nextToken[MAX_TOKEN_LEN+1]` is filled by `while (nextChar != ';' && nextChar != EOF) *nextTokenPtr++ = nextChar;` with no bound; the `dms_assert` is post-hoc (`__assume` in Release). `&#8212;` writes 5 chars + NUL into 5 bytes, `&hellip;` writes 7, an unterminated `&` runs to end of file: stack buffer overflow.
- **Defect (b):** `return XmlConstMap[symbol];` inserts `(symbol, 0)` on a miss; `symbol` is that stack buffer or a pointer into a `SharedCharArray` that is erased right after (:260). Any entity other than lt/gt/amp/apos/quot leaves a key pointing at dead memory that every later lookup `strcmp`s (UB, wrong decode), and the mutation of a non-static global is unsynchronised.
- **Defect (c):** `ReadText`/`TransformChar` test `!= EOF` (-1) but `ReadChar()` returns 0 once `AtEnd()`; the loops terminate only because `FileInpStreamBuff::ReadBytes` fills one `EOF` byte past the end. With a `MappedFileInpStreamBuff` or an in-memory buffer (no sentinel) `ReadText` pushes zeros forever and `TransformChar` overflows.
- **Fix:** read the entity into a bounded buffer with `MG_USERCHECK2` inside the loop; `SymbolGetChar` uses `find()` and returns 0 on miss; make `XmlConstMap` `static` (read-only after `RegisterConst`); test `AtEnd()` in `ReadText` and `TransformChar` (`ReadDQuote` and `NextWord` in `FormattedStream.cpp` already stop on the 0 that `ReadChar` answers at the end).
- **Test:** XML with `&#8212;`, with `&amp` at EOF, with `&nbsp;`.

### SHV-42 · CONFIRMED / Medium · S / low — One HFONT leaked per paint that sets a font
- **Where:** `shv/dll/src/GdiDrawContext.cpp:176-182` `SetFont`, `:197-203` `SetBold`, `:22-26` destructor; caller order `shv/dll/src/DataView.cpp:1745, 1756`.
- **Defect:** `HFONT old = (HFONT)::SelectObject(m_hDC, hFont);` is never re-selected. In `OnPaint` the `PaintDcHandle` outlives the `GdiDrawContext`, so at `~GdiDrawContext` the owned font is still selected into a live DC and `DeleteObject` fails. One HFONT leaks per paint that calls `SetFont`/`SetBold` (AxisControl, GraphVisitor, LabelDrawer, FeatureLayer labels, TableHeaderControl). After ~10k paints GDI object creation fails: blank labels, and SHV-52's null pens/fonts.
- **Fix:** store the first `old` in `m_OrgFont`; in the destructor `if (m_OrgFont) ::SelectObject(m_hDC, m_OrgFont);` before `DeleteObject(m_OwnedFont)`; same for `SetBold`.
- **Test:** the Task Manager GDI-object column stays flat while panning a labelled layer.

### SHV-44 · CONFIRMED / Medium · S / low — WMS `ZoomOut` indexes an empty tile-matrix set
- **Where:** `shv/dll/src/WmsLayer.cpp:1059-1080` `ZoomOut`; `ChooseTileMatrix` :774-789; `Zoom1To1` :1035-1045; trigger `shv/dll/src/ViewPort.cpp:668-671` `ZoomOut1`.
- **Defect:** `ChooseTileMatrix` returns `UNDEFINED_VALUE(SizeT)` when nothing fits. With an empty `m_TMS` (capabilities never loaded) the size-clamp at :1067-1069 sets `m_ZoomLevel = 0` then `--` → `SizeT(-1)`; `Zoom1To1` returns early; the ROI did not change; `if (!m_ZoomLevel)` is false; a second `--` gives `SizeT(-2)` and `Zoom1To1` indexes `m_TMS[SizeT(-2)]`. Level 0 is the coarsest, so the `if (!m_ZoomLevel) return false` itself is correct.
- **Fix:** `if (m_TMS.empty()) return false;` at the top (as `ZoomToFinestLevel` :1105 does) and use `IsDefined(m_ZoomLevel)` instead of the clamp trick; rewrite the wrap-as-logic at :1088/:1095 the same way (SHV-45).
- **Test:** zoom out on a WMS layer with an unreachable capabilities URL.

### GEO-29 · CONFIRMED / Medium · S / low — `dyna_point` distance never validated
- **Where:** `geo/dll/src/OperPolygon.cpp:1192, 1219, 1277` (creation :1072-1133); also `:1297` (GEO-N01).
- **Defect:** `Float64 dist = ...GetDataRead()[0];` then `SizeT nrPointsHere = (segmLength+carry) / dist;`. `dist == 0` gives `inf`, a negative `dist` a negative quotient, an undefined parameter (NaN) `NaN`; all three Float64→UInt64 conversions are UB (MSVC yields 0x8000000000000000), so `resDomain->SetCount(nrPoints)` receives garbage and the two passes may disagree. Separately, `UInt32 nrRemainingPoints = nrPointsHere` truncates a `SizeT`: a segment producing ≥ 2³² points writes fewer points than the count pass allocated.
- **Fix:** `MG_USERCHECK2(IsDefined(dist) && dist > 0, "dyna_point: distance must be a positive defined value")` before the loops; use `SizeT` for `nrRemainingPoints` and bound `nrPoints` with a user check.
- **Test:** `dyna_point(p, q, 0d)` and with a null distance must fail cleanly.

### GEO-30 · CONFIRMED / Medium · S / low — `dyna_point_with_ends` writes through a null sub-item on void-domain parameters
- **Where:** `geo/dll/src/OperPolygon.cpp:1319-1331` (contrast the guarded writes at :1270-1273 and :1292-1295).
- **Defect:** `ri3.WriteUInt32(nrOrgEntity); ri4.Write(ordinalID++);` in the `withEnds` tail are unguarded. `resSub3` is `nullptr` when the point domain is `Unit<Void>` (:1106); `locked_abstr_tile_write_channel(nullptr)` is tolerated but `WriteUInt32 → FillWithUInt32Values → GetTileFunctor()->…` dereferences it. Every `*_with_ends` variant carries `DoIncludeEndPoints`, so `dyna_point_with_ends(param_p, param_q, d)` on parameters with a segment length that is not a multiple of `d` crashes.
- **Fix:** wrap the two writes in `if (resSub3)` / `if (resSub4)` like the siblings.
- **Test:** `dyna_point_with_ends` on void-domain parameters.

### TIC-07 · LIKELY / Medium · S / low — Indirect-expression evaluation loop is unbounded
- **Where:** `rtc/dll/src/tic/AbstrCalculator.cpp:608-671` `EvaluateExpr`; same pattern `:549-580` `GetErrorSource`.
- **Defect:** `while (nrEvals-- && !resultStr.empty()) { … nrNewEvals = CountIndirections(resultStr); … nrEvals += nrNewEvals; }`. A string item whose value resolves back to itself (`parameter<string> p := '=p'`, or a→b→a) adds one per iteration and never ends; the DC is cached so it is a tight loop that freezes the meta thread. No cap, no cancellation check.
- **Fix:** cap the total number of evaluations (e.g. 64) and keep a small `std::set<SharedStr>` of strings already evaluated in this call; on repeat `context->throwItemErrorF("indirect expression cycle: '{}' evaluates to itself")`.
- **Test:** a `testcases/*.dms` case expecting that error.

### STG-23 · CONFIRMED / Medium · S / low — `fwrite` return ignored in `StrStorageManager::WriteDataItem`
- **Where:** `stg/dll/src/str/StrStorageManager.cpp:107, 123`.
- **Defect:** `fwrite(dataBegin, dataSize, 1, file);` in both branches; a short write (disk full, quota, network share drop) leaves a truncated file and the operation reports success. The read side checks (`MG_CHECK(dataSize == 0 || fread(...) == 1)`, :77).
- **Fix:** `MG_CHECK(dataSize == 0 || fwrite(dataBegin, dataSize, 1, file) == 1)` (`fwrite` with size 0 returns 0, hence the guard).
- **Test:** write to a full RAM disk in the str tests.

### QT-59 · CONFIRMED / Medium · S / low — `DmsModel::parent()` rows disagree with `index()`
- **Where:** `qtgui/exe/src/DmsTreeView.cpp:57-79` `GetRow` and `:208-227` `parent()` versus `:179-206` `index()` and `:229-242` `rowCount()`.
- **Defect:** `GetRow` starts at `int row = 1;` while `index()` enumerates from 0 and skips `TSF_IsHidden` items when `!show_hidden_items`, which `GetRow` counts; and for children of `m_root` it returns `createIndex(0, 0, root)` instead of `QModelIndex()`. QTreeView identifies items by (row, internalId), so `viewIndex()` fails for `parent()`-derived indexes: Left-arrow-to-parent, `sibling()`, and expand/scroll of parent chains misbehave. If `ti` is not among `p`'s children the loop dereferences null in Release (`assert(si)` only).
- **Fix:** rewrite `GetRow` to mirror `index()`: start at 0, skip hidden items identically, break out when `si` is null, and return `QModelIndex()` from `parent()` when the parent is `m_root`.
- **Test:** a Debug run under `QAbstractItemModelTester`.

### TIC-04 · LIKELY / Low · S / low — `GetTileCount` and `IsCovered` skip the `MG_CHECK(range_item)` their siblings have
- **Where:** `rtc/dll/src/tic/AbstrUnit.cpp:1002-1010`, `:1017-1023`; siblings `:975, :984, :993`.
- **Defect:** `GetCurrRangeItem()` returns an empty `shared_ptr` when the ultimate item is mid-destruction (`no_zombies`), and `AsUnit(empty)->GetTiledRangeData()` is a virtual call through null, where the three neighbours throw a clean `MG_CHECK`.
- **Fix:** add `MG_CHECK(range_item);` after `GetCurrRangeItem()` in both.

**Phase 0 verification:** T0 Debug + Release, T1, T2, plus the per-item regression configs above added to
`testcases/`. Suggested grouping: one commit per item (disjoint files), or three commits (geo, stg/tif,
rtc+shv+qt).

---

$1
**Status 2026-09-05:** implemented, built (Release x64, no warnings in the touched files) and committed on 2026-09-05, with these deviations. STG-24 is mostly refuted:
`StrFilesStorageManager::DoUpdateTree` already unifies the two domains (`StrStorageManager.cpp:237`); the
two `dms_assert`s became `MG_CHECK`s. STG-15 (`GetTileByteWidth` return type) is deferred: it is mirrored
by `GDalGridImp` and its callers store the value in a `UInt32` anyway. The unreachable-marker assert at
`act/ActorSupport.cpp:271` was left alone: it follows a `reportD` of an intransitive supplier order and
the code continues past it, so turning it into a check would change behaviour. `dbfImp.cpp:657` is a
tautology (`UInt8 < 256`) and was left too. Added during implementation: STG-N03 below, a real write bug
in the compound storage manager. Null shape records in polygon shapefiles, which used to be read as if
they had a box and counts, now read as empty polygons.

Theme: bytes from a file, GDAL, TIFF, DBF, SHP or config text are trusted through `dms_assert`/`assert`
only. Fix by making the checks real (`MG_CHECK` for internal contracts, `MG_USERCHECK2` for user-fixable
input); on these per-file/per-record paths the cost is nil.

### 1a. Shapefile reader

#### STG-N01 · CONFIRMED / High · S / low — A stale or mismatching `.shx` drives the record loop past `m_Polygons`
- **Where:** `stg/dll/src/shp/ShpImp.cpp:194-196, 253-254, 274-278`; `stg/dll/src/shp/ShpStorageManager.cpp:224-225, 139-176, 898-926`.
- **Defect:** `m_NrRecs` comes from the `.shx` header, sizes `ShapeSet_PrepareDataStore`, and after `Read()` is corrected only when it is 0. `ReadDataItem` validates the domain against that value, and `ReadSequences` iterates `polyData.size()` calling `ShapeSet_NrPoints(p)`/`ShapeSet_NrParts(p)`, which index `m_Polygons[recNr]` with assert-only bounds. A `.shx` claiming more records than the `.shp` holds (regenerated `.shp` with an old `.shx`, truncated `.shp`) reads past the vector; fewer records are silently dropped. The intended invariant exists only as Debug asserts (:276-278).
- **Fix:** in `Read()`: `MG_CHECK(m_Polygons.size() == m_NrRecs)` (points: `m_Points.size()`), or set `m_NrRecs` from the container and let `ValidateCount` report the mismatch.
- **Test:** a `.shp` with 10 records and a `.shx` claiming 12 must raise an error.

#### STG-05 · CONFIRMED / High · M / low — Record part/point counts unchecked against `ContentLength`
- **Where:** `stg/dll/src/shp/ShpImp.cpp:790-798` `ShpPolygon::Read` (readers :664-679, :721-734, :759-766); `Check()` :822-835; consumers :942-958, `ShpStorageManager.cpp:141-176`.
- **Defect:** the record's `Int32 m_NumParts/m_NumPoints` go straight to `resize_uninitialized` and `fread`; `Read()` checks `ContentLength` against the remaining file but never the counts against `ContentLength`. A declared count larger than the bytes present leaves the tail of `m_Parts`/`m_Points` uninitialised (`ConvertLittleEndian` is applied only to the elements read) and `Check()` is assert-only, so garbage part offsets reach `NrPoints(partNr)` / `ShapeSet_GetPoints` / `ReadSequences`, which compute iterator ranges from them: out-of-bounds reads. When the truncated record is the last one, `pos` reaches `m_FileLength` and the loop ends without the RecordNumber check firing.
- **Fix:** in `ShpPolygon::Read`: `MG_CHECK(m_NumParts >= 0 && m_NumPoints >= 0 && SizeT(2)*ContentLength >= 4 + 32 + (HasParts() ? 4 + 4*NumParts : 0) + 4 + 16*NumPoints)`; `MG_CHECK` that each `fread` returned the requested count; turn `Check()` into a `void CheckInvariants() const` of `MG_CHECK`s (parts ascending, `parts[0] == 0`, `parts.back() < NumPoints`) called unconditionally (STG-70).
- **Test:** a truncated-last-record `.shp` fixture must raise an error.

#### STG-04 · CONFIRMED / Medium · M / low — No-`.shx` sentinel `UInt32(-1)` never fixed up for points
- **Where:** `stg/dll/src/shp/ShpImp.cpp:134-139, 153, 199, 218, 253-256, 274-275`; `stg/dll/src/shp/ShpStorageManager.cpp:224-225, 471-477`.
- **Defect:** for polygons `ShapeSet_PrepareDataStore(0, 0)` sets `m_NrRecs = 0` so the `if (!m_NrRecs)` fixup fires; for points `m_NrRecs` stays `UInt32(-1)`, `ReadDataItem` does `ValidateCount(4294967295)`, and `ReadUnitRange` never reads records, so `au->SetCount(0xFFFFFFFF)` for every shape type without a `.shx`. `Open()` even `assert(m_FHX.IsOpen())` while :134-139 tolerates the missing file, so "no `.shx`" exists only in Release and is broken there.
- **Fix:** either scan record headers in `ReadUnitRange` when `m_FHX` is absent, or reject with a clear "missing .shx" error and delete the tolerated path (this also removes the contradiction at :153).

#### STG-01 + STG-02 + STG-03 + STG-06 · SMELL–LIKELY / Low · S / low — Header arithmetic on file-supplied `Int32`s; untested header `fread`s
- **Where:** `stg/dll/src/shp/ShpImp.cpp:188-189, 194-196, 218, 253-254, 614-623`.
- **Defect:** `m_FileLength = head.m_FileLength * 2` (signed overflow for ≥ 2 GiB, negative header → huge `UInt32`); `(FileLength-50)/4` negative → `UInt32 m_NrRecs` ≈ 4e9 → `reserve(4e9)` / `sequence_array::Resize` throw `bad_alloc` (DoS on a corrupt `.shx`, no corruption); five untested `fread`s in `ShpPolygonHeader::Read` make a short read surface one record late with a misleading message.
- **Fix:** `MG_CHECK(head.m_FileLength >= 50)` and `% 4 == 0`; `m_FileLength = UInt32(head.m_FileLength) * 2u` checked against `m_FH.GetFileSize()`; cap `m_NrRecs` by `(m_FileLength - 100) / 28`; `MG_CHECK(pos == expected)` after the header reads.
- **Test:** a 0-byte `.shx` next to a valid `.shp` must give a clean error.

### 1b. TIFF, XDB/XYZ, STR, DBF, GDAL

#### STG-10 · LIKELY / Medium · S / low — xyz column type never checked against the item's value type
- **Where:** `stg/dll/src/xdb/XdbImp.cpp:188-217` `ReadColumn`; `stg/dll/src/xdb/XdbStorageManager.cpp:38-68, 118-164`.
- **Defect:** `ReadColumn` stores `Int32`/`Float32`/`Float64` according to the *column* type into `buf`, which is the *item's* buffer (`GetDataWriteBegin`); `ReadDataItem` never checks `imp.ColType(col)` against `ado->GetValuesType()`, and `OverlappingTypes` in `DoUpdateTree` is skipped for `SyncMode::None`. An `attribute<uint8>` (or `bool`) configured on an xyz column receives 4-byte stores into a 1-byte-element (or bit) buffer: heap overflow. Other column types leave `buf` untouched and return `true`.
- **Fix:** in `ReadDataItem`: `MG_CHECK(ValueClass::FindByValueClassID(imp.ColType(colIdx)) == vc)` (or `OverlappingTypes`) before `ReadColumn`; add `default: return false;` to the switch.

#### STG-07 + STG-08 + STG-12 + STG-72 · CONFIRMED / Low (unreachable code) · S / low — The `.xdb` sidecar-header and write path of the xdb/xyz manager is unreachable
- **Where:** `stg/dll/src/xdb/XdbImp.cpp:266-310` `ReadHeader`, `:223-247` `WriteColumn`, `:249-263` `freadln`, `:314-335` `WriteHeader`, `:428-` `AppendColumn` (message at :440); `stg/dll/src/xdb/XdbStorageManager.cpp:70-97` `WriteDataItem`, `:168-205` `SyncItem`, `:212-234` `XyzStorageManager`.
- **Status of the manager (correction of an earlier "dormant" claim):** `XdbStorageManager` is **live**: it is the base class of `XyzStorageManager`, which is registered as `"xyz"` (:234) and only overrides `UpdateColInfo` with the fixed X/Y/Z layout. Every `.xyz` file goes through `ReadDataItem` → `XdbImp::Open(saveColInfo = false)` → `NrOfRows` → `ReadColumn` (the STG-10 path), through `ReadUnitRange` and through `DoUpdateTree`. Only the `"xdb"` registration itself is commented out (:210).
- **What is unreachable:** every manager call passes `saveColInfo = false`, so `ReadHeader` (called only under `saveColInfo`, `XdbImp.cpp:109-111`) never runs. `WriteDataItem` calls `Open(FCM_OpenRwFixed)`, and `Open` throws `MG_USERCHECK2(!alsoWrite, "writing to .xdb is no longer supported")` (:104) before returning, so the `Create` fallback (:80), `WriteColumn` (:92) and, through them, `AppendColumn` and `WriteHeader` are dead. `SyncItem` (the only `AppendColumn` caller) and `freadln` have no callers anywhere in the repository.
- **Defects inside that dead code:** `fscanf(*this, "%s %ld %d", fldName, ...)` into `char fldName[400]` with the `MG_CHECK(StrLen < 400)` after the overflow; `WriteColumn` throws unconditionally at :229 and then carries an inverted `FileResult::require(UInt32(col_index) >= ColDescriptions.size(), ...)`; `freadln` conflates `EOF` with byte 0xFF; "Column alredy exists".
- **Live message defect:** writing an `.xyz` attribute fails with "writing to .xdb is no longer supported"; the user configured xyz, not xdb.
- **Fix:** delete `ReadHeader`, `WriteHeader`, `AppendColumn`, `WriteColumn`, `Create`, `freadln`, `SyncItem` and the `saveColInfo` parameter (Phase 4 C1), and let `WriteDataItem` throw "writing to .xyz is not supported" directly. If `.xdb` support is meant to return instead, fix the four defects (`"%399s"`, drop the inverted `require`, `int ch`, the typo) and re-register the class.
#### STG-24 · LIKELY / Medium · S / low — `StrFilesStorageManager` never unifies the two domains it indexes together
- **Where:** `stg/dll/src/str/StrStorageManager.cpp:61-66, 95-107` (loops), `:188-191` `GetNrFiles`, `:224-234` `DoUpdateTree`.
- **Defect:** `n` is the domain count of the sibling `FileName` attribute while `sdoData` is the string item's own tile; the only relation is the Debug `dms_assert(sdoData.size() == n)`. `DoUpdateTree` verifies the values unit but never `UnifyDomain`s the two domains, although the error text (:173) promises "with the same domain as this". A FileName attribute on a larger domain drives `sdoData[i].resize_uninitialized(...)` for `i >= size()`: out-of-bounds write.
- **Fix:** in `GetFileNameAttr`/`DoUpdateTree`: `AsDataItem(storageHolder)->GetAbstrDomainUnit()->UnifyDomain(fileNameItem->GetAbstrDomainUnit(), ..., UM_Throw)`; replace both asserts by `MG_CHECK`.

#### STG-16 · LIKELY / Medium · S / low — GDAL numeric readers dereference a null feature at layer exhaustion
- **Where:** `stg/dll/src/gdal/gdal_vect.cpp:1688-1690` (also :1701-1703, :1724-1726, :1736-1738, :1760-1761, :1772-1773); `GetNextFeatureInterleaved` :698-718; `FeaturePtr::operator->` = `get_nonnull()` (`rtc/dll/src/ptr/PtrBase.h:88`, assert-only).
- **Defect:** the six numeric loops call `feat->GetFieldAs…` before `GDALFieldCanBeInterpretedAsInteger/Double`, which *do* test `!feat` (evidence the authors expected null); `GetNextFeatureInterleaved`/`GetNextFeature` return `nullptr` once the layer is exhausted. `ReadStrAttrData` (:1619-1624) guards correctly. Trigger: the feature count used for the domain (`ReadUnitRange`, :2228-2246) exceeding what the cursor yields (source modified between range and data read; a driver whose interleaved cursor and `SetNextByIndex` (:1895-1911) disagree).
- **Fix:** `if (!feat) { Assign(dataElemRef, Undefined()); continue; }` in the six loops, or `throwErrorF` "fewer features than the domain count".

#### STG-17 · LIKELY / Low · S / low — `GetFeatureCount()` `-1` becomes the domain count
- **Where:** `stg/dll/src/gdal/gdal_vect.cpp:2235-2245`; consumer `:2777-2784`.
- **Defect:** `SizeT count = layer->GetFeatureCount();` turns `-1` into `SizeT(-1)`; the interleaved fallback at :2239 triggers only for `ODsCRandomLayerRead` datasets, otherwise `au->SetCount(SizeT(-1))`. With `bForce=TRUE` most drivers count by iteration, so this needs a driver that refuses even when forced (some WFS/VRT configurations).
- **Fix:** `if (count == SizeT(-1)) count = ReadUnitRangeInterleaved(...)` regardless of the capability, or `MG_CHECK(count != SizeT(-1))`.

#### STG-18 + STG-19 + gdal `[[maybe_unused]] OGRErr` · LIKELY / Low · S / low — Field creation/lookup failures are only noticed if the driver also CPLErrors
- **Where:** `stg/dll/src/gdal/gdal_vect.cpp:2371-2376`; `:2585-2586, 2616-2617`; `:2630, 2632`.
- **Defect:** `GetFieldDefnRef(feat->GetFieldCount() - 1)` after a `CreateField` whose `OGRErr` is `[[maybe_unused]]`: a driver returning `OGRERR_FAILURE` without CPLError on a zero-field layer gives `GetFieldDefnRef(-1)` = `nullptr` and `->GetNameRef()` dereferences it. The `assert(field_index >= 0)` before `SetField` is Release-unchecked, but OGR CPLErrors "Invalid index" and the error frame converts it, so only the message is useless. `CreateFeature`/`SetFeature` results are dropped the same way.
- **Fix:** `MG_CHECK(createFieldErr == OGRERR_NONE && feat->GetFieldCount() > 0)` and look the field up by name; `MG_CHECK2(field_index >= 0, "field … not found in layer …")`; check the two other `OGRErr`s.

#### STG-15 + STG-25 + STG-21 + STG-20 · SMELL / Low · S / low — Small consistency items
- **STG-15** `stg/dll/src/tif/TifImp.cpp:481-487`, `GridStorageManager.h:151,536`: `UInt32 GetTileByteWidth()` truncates `tmsize_t` (needs a ≥ 4 GiB scanline; harmless) → return `SizeT`.
- **STG-25** `stg/dll/src/dbf/dbfImp.cpp:911` vs `:848`: `fseek(ActualPosition(0,0) - 1)` equals `ActualPosition(0)` because byte 0 is the deletion flag; magic `-1`, unchecked `fseek` → use `ActualPosition(0)` and `MG_CHECK(fseek(...) == 0)`.
- **STG-21** `stg/dll/src/gdal/gdal_base.cpp:711-729`: the `return;` before `if (!--s_ComponentCount)` is intentional (GDAL is initialised once per process; `gdalFinalCleanup` tears down), but :724-728 are dead and `isActive()` reads as a live-component test → delete the dead lines, replace the counter with `bool s_Initialised`, drop the unused `gdalCleanup` call site.
- **STG-20** `stg/dll/src/gdal/gdal_vect.cpp:1662`: `GetFieldAsString` returns the feature-owned scratch buffer, valid until the next call on that feature; here it is consumed immediately → replace `// who owns this ? lifetime ?` with that statement.

#### STG-N03 · CONFIRMED / High · S / low — Compound storage wrote a block above 1 GB once per chunk
- **Where:** `stg/dll/src/cfs/CompoundStorageManager.cpp:127-141` `CompoundStorageOutStreamBuff::WriteBytes`.
- **Defect:** the loop that was meant to write in 1 GB chunks handed the whole `size` to every `IStream::Write` and never advanced `data`: a block between 1 GB and 4 GB was written twice or more, and the `dms_assert(chunkSize <= size)` next to it was a tautology.
- **Fix:** write `chunkSize` bytes per iteration, advance `data`, check the result before the bookkeeping. Implemented.
$1

#### GEO-32 · LIKELY / High · M / med — Dense OD with `endPoint(…, DstZone_rel)` uses the wrong zone
- **Where:** `geo/dll/src/Dijkstra.cpp:513-530` (`Res2EndPoint`, `Res2DstZone`); consumers :940-951, :983-984, :1028; regime :351, :381-384, :394-397.
- **Defect:** the comment "wrong for dense + endPoint(..,DstZone_rel)" is still accurate. `IsDense()` is `!m_LastCommittedSrcZone`, which is allocated only for `OD && SparseResult`, and `SparseResult` only comes from `cut()`/`limit()`; a dense OD with `DstZone_rel` is an ordinary spec. In that case `Res2DstZone(j)` returns `Zone_rel[j]` for a *dst zone* index `j` (an end-point-indexed array), so `endPoints.Impedances[dstZone]`, `dstMinImp[dstZone]`, `tgDstMass[dstZone]` use the wrong zone: silently wrong `orgZone_MaxImp`, `orgZone_Factor`, `pot_ij`, D_i/A_j whenever a dst zone groups several end points.
- **Fix:** implement the comment's dense-correct forms: `Res2DstZone(r) = IsDense() ? r : LookupOrSame(Zone_rel, LookupOrSame(m_FoundYPerRes, r))`, `Res2EndPoint(r) = IsDense() ? DstZone2EndPoint(r) : …`.
- **Test:** dense OD with two end points per dst zone versus the same network with `limit(inf)` (sparse) — results must match.

#### CLC-N01 · LIKELY / Medium · S / low — Param-unit operators accept a factor ≤ 0 or NaN
- **Where:** `clc/dll/src/OperUnit.h:190-196, 218-219, 238-239`; consumer `rtc/dll/src/tic/Metric.cpp:113`.
- **Defect:** `ParamUnitOperator::CreateResult` feeds `adi->GetCurrRefObj()->GetValueAsFloat64(0)` straight into `m->m_Factor`. A config `unit<float32> u := 0.0 * meter;`, `-1.0 * meter` or a null parameter (NaN) produces a metric whose display path executes `dms_assert(factor > 0)` — `__assume` UB in Release — and `UnitPowerOperator` computes `exp(log(0) * power)`.
- **Fix:** throw an operator error unless `IsDefined(factor) && factor > 0`; add a battery case expecting it.

#### CLC-N02 · LIKELY / Low · S / low — `UnitPowerOperator` silently truncates fractional base-unit exponents
- **Where:** `clc/dll/src/OperUnit.h:301-307` versus `UnitSqrtOperator` :361-362.
- **Defect:** `(*b1).second = Int32((*b1).second * power)` turns `pow(meter, 0.5)` into exponent 0 (unit dropped, factor still sqrt'ed), whereas `UnitSqrtOperator` throws "non-square metric" for the same situation.
- **Fix:** check `exp * power` is integral (within epsilon) and throw the same error otherwise.

#### CLC-N03 · LIKELY / Low · S / low — `TiledUnit(lb, ub)` accepts empty, overlapping or unsorted tile ranges
- **Where:** `clc/dll/src/OperUnit.cpp:1184-1210` `TiledUnitOper::Calculate`; `rtc/dll/src/tic/AbstrUnit.cpp:968-969`; `rtc/dll/src/tic/TiledRangeDataImpl.h:198-209`.
- **Defect:** `(lb[i], ub[i])` pairs are copied into `IrregularTileRangeData` without validating `lb <= ub`, disjointness or order; an empty non-first tile makes `GetTileSizeAsI64Rect` hit `dms_assert(!t || t == no_tile)` (Release `__assume`), and overlapping tiles make `GetTiledLocationForValue` silently pick the first tile.
- **Fix:** `MG_USERCHECK` per tile in `Calculate` (non-empty, `ub[i] <= lb[i+1]`); turn the two `dms_assert`s at `AbstrUnit.cpp:968-969` into `MG_CHECK`.

#### TIC-02 · RULED, not a defect (2026-09-05) — `CheckNrTiles` and the tile-count limits
- **Where:** `rtc/dll/src/tic/AbstrUnit.cpp:1032-1039` `CheckNrTiles`; `rtc/dll/src/tic/TiledUnit.h:25` `MAX_NR_TILES`; `rtc/dll/src/tic/AbstrUnit.h:58` `MAX_TILE_SIZE`; sole caller `clc/dll/src/OperUnit.cpp:1194`.
- **Ruling:** the number of tiles may be any UInt32 except 0xFFFFFFFF (`no_tile`), and for a 2-D tiling each dimension of the tiling extent is a UInt16; `CheckNrTiles` guards exactly the `SizeT → tile_id` narrowing of the irregular range count. Nothing to fix.
- **For the record, what was investigated:** the tiling extent of a point domain is a `WPoint` (`TiledRangeData.h:33`) and `RegularAdapter::CalcTilingExtent` (`Unit.cpp:323-330`) narrows through the `UInt16 CeilDivide` overload (`TiledUnit.h:65-74`); per-tile bookkeeping is linear in the tile count per data object (`TileFunctorImpl.h:39, :190`, `TileArrayImpl.h:336-341`); irregular tilings look a value's tile up by a linear scan over `m_Ranges` (`TiledRangeDataImpl.h:198-270`), used per element by the GUI value lookups and `DataArray.cpp:529-570`.
- **Cosmetic residue only:** the message names `MAX_NR_TILES` (65536) while the condition tests `MAX_VALUE(tile_id)`, and `MAX_TILE_SIZE` has no use at all; both are listed under Phase 4 C1.
#### GEO-38 · LIKELY / Low · S / low — `n = ring.size() - 1` on a possibly empty ring
- **Where:** `geo/dll/src/minkowski.h:337` `MinkowskiEdgeCell` (:221 is guarded); `geo/dll/src/BoostGeometryImpl.h:453-461` `cgal_make_kernel_polygon` (called at :616 before the `kernel.size() < 3` test at :466).
- **Defect:** `for (i = 0, n = size()-1; i != n; ...)` gives `n == SizeT(-1)` for an empty ring and reads out of bounds; today callers happen to pass non-empty rings.
- **Fix:** `if (ring.size() < 2) return {};` in both functions.

#### GEO-33 · REFUTED→SMELL · S / low — Negative destination mass gives NaN potentials
- **Where:** `geo/dll/src/Dijkstra.cpp:1013` (the `log(impedance)` sites at :953-971 are guarded).
- **Fix:** `MG_USERCHECK(tgDstMass >= 0)` when reading the mass attribute.

#### STX-N01 · LIKELY / Low · S / low — `&*problem.where` dereferences a possibly-end iterator
- **Where:** `stx/dll/src/ConfigParse.cpp:475, 549`; `stx/dll/src/ExprParse.cpp:64`; `stx/dll/src/DataBlockTask.cpp:100`.
- **Defect:** a Spirit `assert_d` failure at end of input yields `problem.where == end`; `&*where` reads `*end`. For `ParseString`/`parseExpr` that is the NUL terminator, but `ParseFile` parses a memory-mapped view (`fv.DataEnd()`) and `DataArrayOperator` parses a `SharedStr` element of a sequence array, neither of which guarantees a readable byte past the end.
- **Fix:** one helper in `SpiritTools.h` (`where == end ? bufferEnd : &*where`) used by all four catch blocks.

#### STX-19 · LIKELY / Low · S / low — `s_AuthErrorDisplayLockCatchCount`: plain global + `__assume` deletes its own fallback
- **Where:** `stx/dll/src/SpiritTools.cpp:27-28`; writers `stx/dll/src/DataBlockTask.cpp:98`, `ConfigParse.cpp:473/547`; readers `ConfigParse.cpp:469/486, 543/562`.
- **Defect:** `DataArrayOperator::CreateResult` (its group can run parallel) increments the non-atomic counter on worker threads while the meta thread may be parsing; and `dms_assert(!s_AuthErrorDisplayLockCatchCount)` at :469/:543 becomes `__assume(count == 0)`, which entitles the compiler to fold the `if (s_AuthErrorDisplayLockCatchCount) return nullptr;` at :486/:562 away. `AuthErrorDisplayLock` resets the counter per top-level parse, which is why nothing is visible.
- **Fix:** `std::atomic<UInt32>` like its sibling; replace the two `dms_assert`s by `MG_CHECK`; delete the dead `if`s; drop the worker-thread increment in `DataBlockTask.cpp` (no reader there).

#### STX-21 · SMELL · S / low — `problemLoc + 80` formed before clamping
- **Where:** `stx/dll/src/SpiritTools.cpp:108`. **Fix:** `problemLoc + Min<SizeT>(80, bufferEnd - problemLoc)`.

### 1d. Policy: `dms_assert` → real check on external-input paths (C3)

Rule: (1) a condition over bytes from a file, the registry, config text, or a GDAL/ODBC/TIFF/BMP/DBF/SHP
call → `MG_USERCHECK2` (user-fixable) or `MG_CHECK` (internal contract); (2) `dms_assert(0)` /
`dms_assert(false)` as "unreachable" markers → `MG_CHECK(false)` / `throwIllegalAbstract` (7 sites:
`stx/ConfigProd.cpp:320`, `clc/Subset.cpp:340`, `clc/OperConv.h:210`, `act/ActorSupport.cpp:271`,
`tic/TreeItemDataUsage.cpp:629,644`); (3) pointer/precondition asserts on internal contracts may stay.
Add the Release-semantics table (top of this document) as a 6-line comment at
`rtc/dll/src/dbg/Diagnostics.h:82-95`.

Concrete candidates (cost nil — per file/record/tile):

- `xml/XmlParser.cpp:178` (`NextChar() == '<'` on external XML).
- `stx/SpiritTools.cpp:104-105, 209, 218`; `stx/ExprProd.cpp:232, 247`; `stx/ConfigProd_functions.cpp:249` (function-literal length; safe by grammar today — STX-16); `stx/ExprParse.cpp:35` (`IsMetaThread()` at the parser entry, once per parse).
- `stg/dbf/dbfImp.cpp:422, 657, 700-701, 771, 797` (DBF header values).
- `stg/bmp/BmpImp.cpp:345, 475, 591, 613, 707, 734, 802, 897, 1232, 1349, 1376, 1390` (BMP header fields, per row/palette).
- `stg/tif/TifImp.cpp:317, 338, 636, 647-648, 660`; `stg/tif/TifStorageManager.cpp:318, 345-346, 357` (TIFF tags, palette sizes).
- `stg/shp/ShpImp.h:110, 183, 189, 195, 269, 274` (shape type / part / point / record indices from the `.shp`).
- `stg/gdal/gdal_grid.cpp:356-358, 376-380` (tile vs dataset bounds) and `:370, 392, 820` (`resultCode == CE_None` → `MG_CHECK2` with `CPLGetLastErrorMsg()`).
- `stg/gdal/gdal_vect.cpp:1257, 1810, 2079, 2091, 2190` (data sizes, layer defn, feature count vs tile size).
- `stg/cfs/CompoundStorageManager.cpp:136, 281, 362` (chunk headers); `stg/dbf/dbfStorageManager.cpp:138`.
- `geo/Canyon.cpp:175-178` (GEO-36: the assert's `GetDataRead()` takes a lock on GCC's `CC_ASSUME` but not on MSVC's — assert on the already-locked view instead).
- Leave `stg/tif/TifStorageManager.cpp:284` (internal precondition).

**Phase 1 verification:** T0 Release (the point is Release behaviour), T1 (`stg/tst` covers
BMP/TIF/DBF/SHP), T2, plus the fixtures named per item (truncated `.shp`, mismatched `.shx`, 0-byte
`.shx`, single-strip TIFF, xyz with a `uint8` attribute). Effort M overall (~45 assert sites in 12
files plus the items above).

---

$1
**Status 2026-09-05:** implemented, built (Release x64) and committed, with these deviations. RTC-36: the
objects are now relocated by their move constructor (a per-bin `relocate` function used on growth and in
`merge_from`) rather than whitelisted as trivially relocatable, which would not have covered the lambda
payloads. RTC-70: the mitigation now reports once per process at warning level, and `m_InterestCount` is
`std::atomic`; the §9 split is still the follow-up. RTC-43: `~ListObj` is implicitly noexcept, so a throw
from `nodes.push` terminates rather than leaving the flag set; the RAII guard is kept for the early-return
paths. TIC-03: instead of weak keys the entries record a weak reference to their TreeItem and an entry
whose item does not match counts as absent; non-TreeItem actors stay as visited-markers, `ProcessDeletion`
is gone. RTC-58: the three `SetGeoDmsRegKey*` wrappers and `RegistryHandle::Write*` now return the real
status (the callers ignore it); no report is added, since HKLM writes fail routinely for non-admin users.
RTC-31 affects only invalid UTF-8 / Latin-1 input. RTC-16: the four run-time sites use `_mt`, and both
`st` entry points assert `NoOtherThreadsStarted()`. Left alone: the unused `m_Hasher` members (referenced
by an /analyze suppression comment) and `UnorderedMapCache::remove`, which is fixed rather than deleted.

All S effort / low fix-risk unless stated. Suggested commits: (i) token registry + interest holders,
(ii) fail-reason bookkeeping, (iii) hygiene.

### RTC-16 · LIKELY / Medium · S / low — Run-time `GetTokenID_st` sites race the token registry
- **Where:** `rtc/dll/src/set/IndexedStrings.cpp:198-201, 221-231`; `rtc/dll/src/sym/Token.cpp:72-82, 121-133`; callers `rtc/dll/src/tic/AbstrCalculator.cpp:1560` (`GetTokenID_st("_")` during substitution), `tic/MetaFuncApply.cpp:230`, `tic/HofClosure.cpp:91`, `stx/dll/src/StxInterface.cpp:96`.
- **Defect:** `GetOrCreateID_st` calls `GetOrCreateID_impl` without `GetCS()`, mutating `m_Idx`/`m_Vec` and the function-local `static std::vector<bool> s_AlreadyReportedBitmap` that the `_mt` path mutates under the exclusive section. The four sites run at run time on the meta thread while workers call `GetTokenID_mt` (storage managers tokenise attribute names): an unlocked `find` during a concurrent rehash is UB. Only the `CharPtr` overload asserts `NoOtherThreadsStarted()`; the range overload asserts just `IsMetaThread()`.
- **Fix:** switch those sites to `GetTokenID_mt` (or a namespace-scope `StaticLateTokenID`); make `GetOrCreateID_st` `dms_assert(NoOtherThreadsStarted())`; align the two `st` overloads.
- **Test:** a `map(F(k,_), src)` config under MT2 with a GDAL source.

### RTC-11 + RTC-68 + RTC-10 · LIKELY / Medium · S / low — Move-assignment implemented as swap keeps the old target alive in `rhs`
- **Where:** `rtc/dll/src/ptr/InterestHolders.h:168-172` (move-assign), `:174, :182` (copy/convert-assign return `void`); `rtc/dll/src/ptr/SharedPtr.h:104-108`.
- **Defect:** `omni::swap(m_Item, rhs.m_Item)` transfers the *old* interest into `rhs`; interest keeps data computed and resident, so `member = std::move(otherMember)` retains the old item's data for as long as `otherMember` lives — a trap in exactly the bookkeeping where lifetime is the point. `SharedPtr` does the same with the pointee. The three assignment operators disagree on return type.
- **Fix:** `InterestPtr tmp(std::move(rhs)); omni::swap(m_Item, tmp.m_Item);` (old interest dropped at the end of the operator, `rhs` null); same for `SharedPtr`; return `*this` from all three.

### RTC-23 + RTC-24 + RTC-25 + RTC-64 + RTC-42 · LIKELY / Medium · S / low — Fail-reason bookkeeping can leave a failed actor without a reason, then deref null
- **Where:** `rtc/dll/src/act/Actor.cpp:940-952` `ClearFail`, `:985-1047` `DoFail`, `:1086-1096, 1121-1127` `ThrowFail(const Actor*)` / `Fail(const Actor*, FailType)`; `rtc/dll/src/set/StaticQuickAssoc.h:23-33` `assoc`, `:62-70` `GetExisting`; sole `ThrowFail(src)` caller `tic/MetaFuncApply.cpp:194`.
- **Defect:** `assoc(this, msg)` **erases** the entry when `msg` is null and returns false (ignored), while `m_State.SetFailure(ft)` runs anyway and `msg->TellWhere(...)` then dereferences null. `ThrowFail(src)` forwards `src->GetFailReason()` unchecked (the caller only `dms_assert`s the supplier failed). `ClearFail` uses `GetExisting`, whose `dms_assert(i != end)` is `__assume` in Release, and declares an unused `errMsgPtr` after the lock (so it neither defers `~ErrMsg` nor is used).
- **Fix:** `MG_CHECK(msg)` at the top of `DoFail` (or substitute a generic ErrMsg); in `Fail(const Actor*, FailType)` substitute "supplier failed without reason" when null; `ClearFail` → tolerant `erase(this)` and drop `errMsgPtr`; rename `assoc` → `assocOrErase` (R2).

### RTC-70 + RTC-C02 · LIKELY / Medium · S (diagnostic) / low; L / med for the split — `DecInterestCount`'s "mitigation" hides double-decrements
- **Where:** `rtc/dll/src/act/Actor.cpp:1259-1262` (`if (!m_InterestCount) return {}; // DEBUG, MITIGATION OF ISSUE`); resets in `tic/AbstrDataItem.cpp:106-110` and `tic/TreeItem.cpp:353-357`; `GetInterestPtrOrNull` :1509-1532.
- **Defect:** the only legitimate way to reach `DecInterestCount` with count 0 is a holder that outlived a count reset: the destructors set `m_InterestCount = 0` and run `StopInterest` while consumers may still hold interest. Weak holders no-op (they `lock()` first), but an intrusive `SharedActorInterestPtr` on a std-managed TreeItem (the split flagged in `doc/development/std-ptr-migration-plan.md` §9) reaches this line on a dead or count-reset object; the early return hides it. The read is also non-atomic and outside `sg_CountSection`.
- **Fix:** keep the return but `reportF(ST_Warning, ...)` in Release / `MG_CHECK` in Debug so the double-decrement becomes visible; `std::atomic` for `m_InterestCount` (already proposed at `Actor.h:34`); finishing the §9 split is the L / med follow-up.

### RTC-36 + RTC-N01 · LIKELY / Medium · M / low — `garbage_can` relocates type-erased objects bytewise and drops contents on move-assign
- **Where:** `rtc/dll/src/act/garbage_can.h:48-57` `ensure_capacity`, `:85-94` move-assign; `rtc/dll/src/act/ActorSupport.cpp:27-53` `merge_from`.
- **Defect:** every `add()` that grows `storage` (a `std::vector<Block>`) and every `merge_from` `memmove` relocates previously added objects **without their move constructor**; only trivially relocatable types survive. Current payloads (`SupplInterestListPtr`, `SharedPtr`, `std::shared_ptr`, `movable_scoped_exit<lambda>`, `rtc::any::Any`, a `std::set` at `OperationContext.cpp:584`) survive destruction after relocation on MSVC and libstdc++; a `std::list`, `std::unordered_map` (inline sentinel/bucket) or MSVC `std::function` would be corrupted silently. Move-assign `assert(bins.empty() || that.bins.empty())` then `bins = std::move(that.bins)`: with a non-empty lhs the old `TypeBin`s are destroyed without running `destroy`, so their deferred decrements never run (callers assign into empty cans today; nothing enforces it in Release).
- **Fix:** `static_assert` an opt-in `is_trivially_relocatable<T>` trait in `get_type_bin` (whitelist the current types), or move-construct on growth (a `std::vector<T>` per bin via a virtual bin); `if (this != &that) { clear(); bins = std::move(that.bins); }`.

### RTC-43 · LIKELY / Low · S / low — `s_LispObjStackActive` re-entrancy flag without RAII in a destructor
- **Where:** `rtc/dll/src/sym/LispRef.cpp:768-795` `~ListObj`.
- **Defect:** `nodes.push` (:764) can throw `bad_alloc` between `s_LispObjStackActive = true` and `= false`; the flag then stays set for the thread and every later `~ListObj` returns early: its cache entry stays (dangling `ListObj*` in `m_USet`), its children leak, and a later `apply` with the same key dereferences the dangling pointer via `GetKey()`.
- **Fix:** an RAII guard that clears the flag.

### RTC-38 · CONFIRMED / Low · S / low — `strncpy` truncates Lisp string constants at an interior NUL
- **Where:** `rtc/dll/src/sym/LispRef.cpp:452-461`.
- **Defect:** `strncpy(b, v.first, len)` stops at the first NUL and zero-pads; the key stored in `StrnObj` then differs from the hashed/compared range, so the cached value is silently altered and every later lookup with the real range misses and creates a duplicate.
- **Fix:** `memcpy(b, v.first, len)`.

### RTC-27 · LIKELY / Low · S / low — Lisp cache lifetime depends on cross-TU static init order
- **Where:** `rtc/dll/src/sym/LispRef.cpp:599-603` `GetLispCaches()`; `rtc/dll/src/sym/LispEval.cpp:75-80` (namespace-scope `LispRef` globals) and `:415` (that TU's `LispComponent`).
- **Defect:** `GetLispCaches()` returns a pointer into uninitialised storage when `s_LispComponentCount == 0` (plain `assert`); the `LispRef` globals in `LispEval.cpp` are constructed *before* the TU's own `LispComponent` and depend on `LispRef.cpp:40` having run first — it works by link order today.
- **Fix:** move the `LispComponent` above the globals (or make them function-local statics); `MG_CHECK` in `GetLispCaches`.

### RTC-08 · LIKELY / Low · S / low — `s_SupplTreeInterest` entry inserted on the failed branch is never removed
- **Where:** `rtc/dll/src/act/Actor.cpp:1382-1394` (and :1414-1427, where the entry already exists).
- **Defect:** `(*s_SupplTreeInterest)[this]` inserts before `if (!WasFailed(FailType::Data))`; on the failed branch `AF_SupplInterest` is not set, so `MoveSupplInterest` (:1437) never erases the null entry and the map can never become empty again. Needs a Data failure raised inside `GetSupplInterest()`.
- **Fix:** `find`/`try_emplace` after the `WasFailed` check, or erase on the failed branch.

### RTC-12 + RTC-C14 + TIC-10 · LIKELY / Low · S / low — `noexcept` functions that can throw
- **Where:** `rtc/dll/src/act/Actor.cpp:1249-1290` `DecInterestCount() noexcept` (constructs `actor_section_lock_map::ScopedLock` → `GetorCreateMutex` → `std::map::insert`), `:1317-1331` `StopInterest() noexcept` (calls `ReportSuspension`); `rtc/dll/src/tic/OperationContext.cpp:1584, 1600, 1695` `RetainedBytesOf` / `SpilledResidentBytes` `noexcept` (call `TotalAllowedPhysicalMemory()` outside their `try`; `memory_info`'s ctor throws on `GlobalMemoryStatusEx` failure, `RTC_GetRegDWord` MG_CHECKs).
- **Defect:** a throw becomes `std::terminate`. Low plausibility (a failed ~100-byte allocation means the process is dying), but the memory-info path is a real OS-failure branch.
- **Fix:** catch and `DBG_ReportBoundaryException` / return an empty can; give `TotalAllowedPhysicalMemory` a noexcept cached variant, or move the call inside the `try` with a `SizeT(-1)` fallback.

### RTC-14 + RTC-15 · LIKELY / Low · S / low — Status-flag globals read outside the lock that writes them
- **Where:** `rtc/dll/src/utl/Environment.cpp:549-550, 585` (POSIX :2744-2745, :2764) versus writers under `RegAccessSection()` at :544-578 and :596-601 (GUI options apply via `qtgui/.../DmsOptions.cpp:545-549`).
- **Defect:** plain `UInt32 g_RegStatusFlags`, `g_OvrStatusMask`, `g_OvrStatusFlags` — a formal data race (UB), benign on x64 for aligned 32-bit words.
- **Fix:** `std::atomic<UInt32>` with relaxed loads/stores; combine mask and flags into one `std::atomic<UInt64>` if a consistent pair matters.

### RTC-18 + RTC-21 + RTC-19 · LIKELY / Low · S / low — `portable_task_group` shutdown edges
- **Where:** `rtc/dll/src/parallel/portable_task_group.cpp:71-80` `run()`, `:82-86` `cancel()`, `:108-127` `GetPortableTaskGroup()`; user `rtc/dll/src/tic/ItemLocks.cpp:815-826`.
- **Defect:** `run()` silently drops the task while stopping/cancelling; `ItemLocks.cpp` sets `s_RunTaskActive = true` *before* `run(RunTasks)`, so a dropped task leaves the flag set and `RunTask` never schedules again (shutdown-only today). `GetPortableTaskGroup()` dereferences a plain global with an assert-only null check. `cancel()` stores the flag and notifies without `m_mutex`; a missed wake-up is covered only because the destructor's locked notify always follows.
- **Fix:** `run()` returns `bool` (or throws `task_canceled`) and callers undo their bookkeeping; `MG_CHECK` the pointer; take the mutex in `cancel()`.

### TIC-N01 · LIKELY / Low · S / low — Lost wake-up between `GetATask` and clearing `s_RunTaskActive`
- **Where:** `rtc/dll/src/tic/ItemLocks.cpp:775-780, 815-826`.
- **Defect:** `RunTasks` clears `s_RunTaskActive` in a scoped-exit **after** `GetATask` returned empty and released the mutex; a `RunTask` inserting in that window sees the flag set and does not launch a new `RunTasks`, so the inserted item waits until the next poll (bounded by consumers re-polling `IsDataReady`, but a wasted round-trip).
- **Fix:** clear `s_RunTaskActive` inside `GetATask` under the mutex when the set is empty (keep the scoped exit for the exception path).

### TIC-N04 (+ TIC-08) · LIKELY / Low · S / low — `GetNrTileBytesNow` returns `size_t(-1)` for an empty tile
- **Where:** `rtc/dll/src/tic/DataArray.ipp:93-100`; consumers `:84-90`, `:103-111`, `rtc/dll/src/tic/AbstrDataItem.cpp:933, 1429-1462`.
- **Defect:** `if (tile.size() == 0) return std::size_t(-1); // can be anything` poisons `GetNrBytesNow` sums and makes `IsSmallerThan(KEEPMEM_MAX_NR_BYTES)` answer false for a 1-tile empty array, so `TryCleanupMemImpl` misses the keep-small fast path; `PublishMeasuredElementWidth` carries a wrap-tolerance comment for it (the TIC-08 candidate).
- **Fix:** return 0; delete the tolerance comment.

### TIC-03 · CONFIRMED / Low · M / low — `ProcessDeletion` is disabled by a leading `return;`
- **Where:** `rtc/dll/src/tic/TicDataSupport.cpp:254-262` (registered at :273), `:310` (TODO acknowledging recycled addresses), reader `:332-339` `TreeItem_GetSupplierLevel`.
- **Defect:** `s_SupplierLevels.erase(self)` never runs; keys are raw `const Actor*`. The map is only read by `find(ti)` with a live pointer and never dereferences its keys, so the effect is a false supplier level for a new item allocated at a recycled address (wrong "analysis source" highlighting) until `TreeItem_SetAnalysisTarget(…, mustClean=true)`. The `return;` was probably added because `NC_Deleting` can fire off the meta thread.
- **Fix:** key the map on `std::weak_ptr<const TreeItem>` (`owner_less`), skip expired entries in `TreeItem_GetSupplierLevel`, delete `ProcessDeletion` and its registration.

### RTC-30 · LIKELY / Low (performance) · S / low — XOR-only chunk accumulation in `hash_in`
- **Where:** `rtc/dll/src/ptr/SharedStr.cpp:541-547` (used :549-593); the intended mixing prime is commented out at :554/:577.
- **Defect:** `hash ^= low; hash ^= high;` per 16-byte chunk: chunk permutations collide and identical chunk pairs cancel to the empty-string hash; only `avalanche` at the end. Users are `GenericHasher` and the token registry hasher, both backing `unordered_set`s with full equality, so correctness holds; buckets degrade to O(n) for structured keys ≥ 32 bytes.
- **Fix:** `hash = (hash ^ low) * prime; hash = (hash ^ high) * prime;`. Verify nothing persists hash values.

### RTC-31 · LIKELY / Low · S / low — `fold_ascii_uppercase` signed compare folds bytes ≥ 0xC1
- **Where:** `rtc/dll/src/ptr/SharedStr.cpp:448-461`; used by the CI compare (:502-529) and hasher (:572-593).
- **Defect:** `_mm_cmplt_epi8(shifted, 26)` on the `_mm_subs_epu8` result is true for `shifted >= 0x80`, i.e. bytes `0xC1..0xFF` get `0x20` OR'd in. Self-consistent and never stored; two *valid* UTF-8 strings cannot be merged by it, but Latin-1 / invalid input is accidentally case-folded in `0xC1-0xDE`.
- **Fix:** unsigned compare: `is_upper = _mm_cmpeq_epi8(_mm_min_epu8(shifted, _mm_set1_epi8(25)), shifted)`.

### RTC-34 + RTC-35 + RTC-58 + RTC-59 + RTC-60 + RTC-C09 · SMELL / Low · S / low — Registry and environment wrappers hide every failure
- **Where:** `rtc/dll/src/utl/Registry.cpp:176-187` (REG_MULTI_SZ parse), `:27-45, 73-76, 142-156, 204-226` (every `RegOpenKeyExW/RegCreateKeyExW/RegSetValueExW/RegDeleteValueW` status dropped, `RegCloseKey(0)` on failed opens); `rtc/dll/src/utl/Environment.cpp:438-480` (three `Set*` wrappers `catch (...) {}` then `return true`; `auto result` unread), `:405-421` (`goto exit` + `catch(...)`: read failure, absent key and `#DELETED#` indistinguishable), `:559-578` vs `:2753-2757` (POSIX ORs `RSF_Default` when `StatusFlags` is explicitly 0; Windows distinguishes absent vs 0), `:494` vs `:508` (`"C:/LocalData"` vs `"C:\\SourceData"`, neither through `ConvertDosFileName`).
- **Fix:** return the `RegSetValueExW` status and `reportF` on failure; `if (wc == 0) { if (!empty) emit; continue; }`; `std::optional` return for `GetGeoDmsRegKey`; sentinel default `UInt32(-1)` on POSIX; one separator style through `ConvertDosFileName`.

### RTC-22 · SMELL · S / low — Zombie retry is a busy-wait on a mutex
- **Where:** `rtc/dll/src/set/Cache.h:84-103`. Progress is guaranteed (`~ListObj` removes the node under the same lock). **Fix:** `std::this_thread::yield()` after `continue`.

### RTC-48 + RTC-49 + RTC-50 + RTC-47 + RTC-51 + RTC-52 + RTC-53 + RTC-N03 · SMELL / latent ill-formedness · S / low — One-liners
- `rtc/dll/src/set/rangefuncs.h:19, 202`: `static_assert(!std::is_trivially_move_assignable_v<T, T>)` (two arguments, under an always-defined macro; GCC diagnoses at parse, MSVC tolerates because the overload is never instantiated; the intent is inverted anyway) → one argument or delete.
- `rtc/dll/src/set/Cache.h:196-205`: `UnorderedMapCache::remove` uses `(*i)->IsOwned()` on a pair (uninstantiable, no callers) → delete or `i->second->IsOwned()`; `:131` unused `m_Hasher`.
- `rtc/dll/src/set/StaticQuickAssoc.h:85-90`: `new std::map<K, V>` ignores `Pred` → `std::map<K, V, Pred>`.
- `rtc/dll/src/mem/FixedAlloc.cpp:1039`: `#endif defined(MG_CACHE_ALLOC)` missing `//`; `:1005-1014` `org_sz` dead store. (`MG_CACHE_ALLOC_SMALL` is commented out at :100, so the lock-free free list RTC-44/45/46 is dead code — leave it.)
- `rtc/dll/src/ptr/StaticPtr.h:47-48`: `pointer m_Ptr;` uninitialised (safe only at static scope) → `= nullptr`.
- `rtc/dll/src/ser/FormattedStream.cpp:64-74`: `inp->CurrPos()` in the mem-init list before `MG_PRECONDITION(inp)` → check first.
- `rtc/dll/src/ptr/SharedBase.h:39`: `DuplRef()` lacks `RTC_CALL` (links only because every intrusive `no_zombies` instantiation is inside Rtc.dll) → add it.
- `rtc/dll/src/cs_lock_map.h:268`: unused `assoc_ptr m_A2`.

### RTC-05 + RTC-37 · SMELL · S / low — Raw storage idioms
- `rtc/dll/src/set/IndexedStrings.cpp:35`: placement storage without `alignas` and with external linkage → `alignas(...) static Byte …` (as `sym/LispRef.cpp:592` does).
- `rtc/dll/src/act/Actor.cpp:158-167`: `reinterpret_cast<SupplInterestListPtr&>` of a TLS raw pointer guarded by `dms_assert(sizeof == sizeof)` → `static_assert` size and standard layout with a comment saying why (avoids a `thread_local` with a destructor).

### RTC-40 + RTC-39 + RTC-C01 + RTC-C03 + RTC-C13 · SMELL · S / low — XML parser hygiene
- `rtc/dll/src/xml/XmlParser.cpp:28-37`: move ctor copies `m_AttrValues` → `std::move`.
- `:171-226`: `m_Parent` raw pointers into a reallocating `std::vector<XmlElement>` — safe only because `m_Parent` is used while the parent is still on `openStack` and `XmlTreeParser::ReadElemCallback` returns false → document, or store a parent index.
- `:244`: external-linkage `HtmlDecode(SharedStr&)` unrelated to `utl/Encodes.h:15 HtmlDecode(WeakStr)` → `static HtmlDecodeInPlace` (R2).
- `xml/XmlConst.h:14-25`: `CompCharPtr` is a hidden `;`-terminated comparator (`comp(";", "")` is true) → document (C4).
- `:325-342`: `RegisterConst`'s `// etc.` invites entities that exceed the 4-char limit → note it.

### Refuted rtc candidates worth a comment only
`InterestHolders.h:64-68` noexcept copy ctor (a non-null copy takes the non-throwing ≥ 1 path — add `assert(GetInterestCount())`); `InterestHolders.h:227-236` discarded `garbage_can` (dies after the lock is released); `Actor.cpp:1292` `s_IsDetectingIncInterest` (can never be true — delete, Phase 4); `portable_task_group.cpp:94-104` destructor without `m_idle_cv` notify (document "no concurrent wait()"); `LispRef.cpp:668-676` (`ZeroSymbObjCache` grown before any `SymbObj` exists, never shrinks); `IndexedStrings.cpp:21-30` (`&ref.back()` — the single writer always appends the NUL); `VectorMap.h:108` (`std::vector::back` contract).

**Phase 2 verification:** T0 Debug (asserts on) + Release, T1, T2 with MT2 on; the `map(F(k,_), src)`
GDAL config for RTC-16; a Debug run to process exit for the RTC-07/RTC-08 map emptiness asserts.

---

$1
**Status 2026-09-05:** implemented, built (Release x64) and committed, with these deviations. SHV-43 became
an `MG_CHECK2` rather than an early return (the caller asserts the invariant; a violation is now a clean
error instead of undefined behaviour). SHV-46/73: the bounds keep `<=` (the end pointer stays a valid
result) but are `MG_CHECK`s, as is the freshness precondition. SHV-53: the empty handle is gone; the task
is still detached (the TODO stays). CLC-27: the interest holder is kept under the name `adiInterest`
with the analysis as its comment; removing it needs a Debug run of the unit-metric configurations.
QT-60: the two `remove(0)` calls are guarded; the three meta-info policies of the tree enumeration are
left as they are (a behaviour choice, not hygiene). Also in this commit: `act/Actor.h` forward-declared
`garbage_can` as a `struct` (C4099 in every TU); it is a `class`.

### SHV-43 · LIKELY / Low · S / low — "Show selected only" with no selections theme dereferences null
- **Where:** `shv/dll/src/ShvUtils.cpp:266-272`; caller `shv/dll/src/TableControl.cpp:401-413` `UpdateShowSelOnly`.
- **Defect:** the caller passes `NrEntries() ? GetColumn(0)->m_Themes[AN_Selections].get() : nullptr`; the invariant "ShowSelectedOnly implies a column with a selections theme" is a Debug-only `dms_assert`. Toggle "show selected only", then remove the last column: `selTheme->GetThemeAttr()` runs under `__assume`.
- **Fix:** `if (!selTheme) return;` (or `MG_CHECK`) at the top of the `ShowSelectedOnly()` branch.

### SHV-52 · LIKELY / Low · S / low — Null GDI handles stored unchecked
- **Where:** `shv/dll/src/PenIndexCache.cpp:237-253`; `shv/dll/src/FontIndexCache.cpp:274-275`.
- **Defect:** `CreatePen`/`ExtCreatePen`/`CreateFontIndirectW` results are pushed into the collection without a null check; `SelectObject(hDC, NULL)` later fails silently and drawing proceeds with the previous pen/font. GDI creation fails on handle exhaustion, which SHV-42 makes reachable in long sessions.
- **Fix:** `MG_CHECK(pen)` / `throwLastSystemError("CreatePen")`.

### SHV-41 · LIKELY / Low · S / low — `int` pixel arithmetic in the clipboard bitmap copy
- **Where:** `shv/dll/src/GridLayer.cpp:971-972`.
- **Defect:** `int bytesPerRow = ((Width * biBitCount + 31) / 32) * 4; memcpy(pvBits, drawer.m_pvBits, bytesPerRow * Height);` — a selection of ≥ 2 GiB (32 bpp at ~25k × 25k) makes the product negative and `memcpy` receives `size_t(negative)`; `CreateDIBSection` can succeed for that size on 64-bit.
- **Fix:** compute in `SizeT`, `MG_CHECK` against `bmi->bmiHeader.biSizeImage`, refuse selections above a sane limit.

### SHV-53 · SMELL / Low · M / med — Detached update task on a drawing path cannot be suspended or cancelled
- **Where:** `shv/dll/src/GraphicObject.cpp:372` (`dms_task updater = dms_task(prepareDataTask); // XXX, TODO: WaitForReadyOrSuspend …`); `rtc/dll/src/parallel/dms_task.h:21-24`.
- **Defect:** `dms_task` detaches its `std::thread` in the constructor, so `updater` is an empty handle and the task runs unsupervised; the lambda locks weak pointers before use (no dangling access), but suspend/cancel are indeed not handled.
- **Fix:** track the task in the owner and cancel/join on destruction; at least remove the misleading local.

### SHV-54 · SMELL · S / low — `SuspendTrigger::Resume(); // REMOVE` is load-bearing in Release
- **Where:** `shv/dll/src/ShvDllInterface.cpp:173-174`.
- **Defect:** `assert(!SuspendTrigger::DidSuspend())` is Debug-only; in Release the `Resume()` marked for removal is the only thing guaranteeing `AddLayer`'s precondition.
- **Fix:** keep `Resume()` and delete the comment, or convert the assert into an `MG_CHECK`.

### SHV-46 + SHV-73 · SMELL · S / low — Grid-coordinate getters trust Debug-only freshness and bounds
- **Where:** `shv/dll/src/GridCoord.cpp:326-354` (`GetGridRowPtr`/`GetGridColPtr`), invariant `AdjustGridNrs` :234-283; `shv/dll/src/GridCoord.h:38` and `.cpp:328, 342, 409, 429, 446` (`dms_assert(!IsDirty())`).
- **Defect:** the bound is checked against `m_GridRows` while the pointer is taken into `m_LinedRows` — safe because `AdjustGridNrs` keeps `linedCoords.size() == (showLines ? gridCoords.size() : 0)`, but `<=` permits a one-past-end pointer and every freshness precondition is Debug-only; a stale read silently returns old coordinates.
- **Fix:** `MG_CHECK(currViewRelRow < size())` in the two pointer getters if callers never use the end pointer; consider a lazy `EnsureUpdated()`.

### SHV-45 · SMELL · S / low — Wrap arithmetic used as logic in WMS zoom
- **Where:** `shv/dll/src/WmsLayer.cpp:1067-1069, 1088, 1095`. Folded into SHV-44: write it with `IsDefined`/`empty()`.

### SHV-55 + SHV-49 + SHV-51 + SHV-47 · SMELL · S / low — Comment-only items
- `shv/dll/src/PenIndexCache.cpp:263-269`: `~PenArray` comment says HFONT (copy-pasted from `FontIndexCache.cpp:299-309`); the code is right for pens.
- `shv/dll/src/ShvUtils.cpp:220-231`: `"Copy"` → `"CopyCopy"` is intentional (stripping would leave an empty name).
- `shv/dll/src/FocusElemProvider.cpp:92`: the unnamed `DataWriteLock(...).Commit()` materialises an all-zero selection attribute; nothing needs the lock afterwards.
- `shv/dll/src/ShvUtils.h:332-335`, `shv/dll/src/ViewPort.cpp:892-895`: `InterpolateColor`/`interpolate` divide by `n` under a caller-guaranteed `n > 0` (doubles in the second) → `dms_assert(n)` documents the contract.

### QT-57 · LIKELY / Low · S / low — Unguarded `MainWindow::TheOne()` in the native event filter
- **Where:** `qtgui/exe/src/main_qt.cpp:358` (siblings :327, :342, :352 null-check).
- **Defect:** a `WM_COPYDATA` from another process arriving after `~MainWindow` (or before construction) while the filter is installed dereferences null.
- **Fix:** `auto mw = MainWindow::TheOne(); if (mw && msg->hwnd == (HWND)mw->winId())`.

### QT-58 · LIKELY / Low · S / low — `WM_COPYDATA` payload used as a NUL-terminated string
- **Where:** `qtgui/exe/src/main_qt.cpp:229, 253, 279` (`Get4Bytes` at :190-195 shows the intended pattern).
- **Defect:** `CharPtr(pcds->lpData)` is read without consulting `cbData`; any local process can send a non-terminated buffer, or `cbData == 0` with `lpData == nullptr`.
- **Fix:** a helper `std::string PayloadAsString(pcds)` using `strnlen(lpData, cbData)` and a null check.

### QT-67 · LIKELY / Low · S / low — `delete` of a `QTimer` possibly mid-signal
- **Where:** `qtgui/exe/src/DmsViewArea.cpp:800-808` `VH_KillTimer`, `:810-827` `onTimerTimeout`.
- **Defect:** `onTimerTimeout → dv->OnTimer(timerId)` can call `VH_KillTimer` for the same id, deleting the emitting `QTimer` inside its own `timeout` emission. Qt 6 tolerates sender deletion in `doActivate`, but the documented contract is `deleteLater()`.
- **Fix:** `it->second->stop(); it->second->deleteLater();`.

### QT-56 + QT-61 + QT-62 + QT-63 + QT-65 + QT-69 + QT-60 · SMELL · S / low — Qt hygiene
- `qtgui/exe/src/main_qt.cpp:206-207`: `case CommandCode::SendApp: //break;` — falling through to the main window is the only working behaviour (`SendMessage(nullptr)` is not a broadcast); the comment is stale → `[[fallthrough]]; // main window receives app-level commands`.
- `qtgui/exe/src/DmsMainWindow.cpp:780-788`: `QTimer::singleShot(0, [this]…)` without a receiver context, guarded only by `g_IsTerminating` (contrast :853/868/881, which pass `this`) → pass `this`.
- `DmsMainWindow.cpp:2152-2168`: timer lambda reads the global `s_CurrMainWindow` instead of its captured pointer; the timer has `mainWindow` as context so Qt discards it on destruction → capture and use `mainWindow`.
- `DmsMainWindow.cpp:663-672`: parentless 3 s splash timer owning a `unique_ptr<DmsSplashScreen>`; if the process exits within 3 s the splash is leaked (no double free) → `QTimer::singleShot(3000, this, …)`.
- `DmsMainWindow.cpp:485-493` vs `:464-474`: a throwing `FindURL` *enables* the meta-info action while the histogram sibling disables → comment or align.
- `qtgui/exe/src/DmsTreeView.cpp:248-264`: `if (!show_hidden_items == reg_show_hidden_items)` is correct for bools but unreadable → `!=`; rename `updateChachedDisplayFlags` (R1).
- `DmsTreeView.cpp:139-152`: `remove(0)` before an emptiness test is safe (`split`/`splitPath` return ≥ 1) but fragile → optional guard.
- `DmsTreeView.cpp:71` vs `:192` vs `:234`: three different meta-info-update policies (`isWaiting ? _GetFirstSubItem() : GetFirstSubItem()`, `_GetFirstSubItem()`, `GetFirstSubItem()`) for the same enumeration → unify with QT-59.
- `main_qt.cpp:290-306`: `assert(commandCode <= WmCopyActiveDmsControl)` is the only thing tying the `switch` to the two payload layouts; a new enum value falls into the "code >= 4" branch by default → `MG_CHECK` or a `default:` that reports.

### CLC-27 · SMELL · M / med — `hackToFixFuncDcMakeResultDueToUnderspecifiedOperatorgroup`
- **Where:** `clc/dll/src/OperUnit.h:187` (`InterestPtr<const TreeItem*> …(adi); // REMOVE, FIX`); `rtc/dll/src/tic/MoreDataControllers.cpp:655-680, 727, 795`.
- **Analysis:** `FuncDC_CreateResult` calls `GetArgs(true,false)`; `MustCalcArg → FuncDC::GetArgPolicy → GetOperator()` now resolves the operator from the arg DCs' result classes *before* the policy question, so `ParamUnitOperator::GetArgPolicy(0) == calc_always` is honoured: `FutureData fd = argIter->m_DC` (an `InterestPtr`) is taken before `CalcResultWithValuesUnits()` and lives in `*args` for the whole `CreateResult`. Hence `adi` already holds interest when `DataReadLock lck(adi)` runs. The "underspecified group" was `cog_mul`/`cog_div` (`calc_as_result` for every arg) from the time the group's policy was consulted instead of the operator's.
- **Fix:** remove the `InterestPtr` and run the unit-metric battery configs in Debug (where `DataReadLock` asserts interest); if any assert fires, the proper fix is a dedicated `SpecialOperGroup` with `{calc_always, calc_as_result}` for the param-unit operators.

### CLC-28 + CLC-32 + CLC-26 · SMELL · S / low — Operator hygiene
- `clc/dll/src/Modus.cpp:733, 818`: `catch (...) {}` in `EstimatePerformance` also swallows `task_canceled` (observed at the next scheduler check; no corruption) → `catch (const task_canceled&) { throw; } catch (...) {}`.
- `clc/dll/src/ReadData.cpp:161, 259`: `ThrowingConvert<UInt32>(readPos + CurrPos())` gives a generic conversion error for > 4 GiB text → `MG_USERCHECK2(pos <= MAX_VALUE(UInt32), "ReadElems: text longer than 4 GiB")`.
- `clc/dll/src/SeparableMapping.h:392-403`: `PickProbeLines` `assert(n >= 2)` (constant `k = 8`, callers pass grid dims above `g_SeparableMapping_MinCells`, so not reachable today) → `if (n < 2) return {0};`.

**Phase 3 verification:** T0, T4 (open a config, table view with selections, WMS background, options
dialog, close during calculation), a Debug run under `QAbstractItemModelTester` for the tree view.

---

$1
**Status 2026-09-05:** implemented, built (Release x64) and committed. C1: every enumerated item is gone
(`LoadBlobBuffer`, `GetThisCurrTileID`, `s_IsDetectingIncInterest` with its externs and checks, the
`SetWritability` husk, the `REMOVE` blocks, the one-line leftovers, `MAX_NR_TILES`/`MAX_TILE_SIZE`, the
commented Prolog/Query sections of `LispEval.cpp` (465 lines; the doc comment on `ApplySubstList` stays)
and the retired `.xdb` column path: `XdbImp` now only opens and reads, `XdbStorageManager::WriteDataItem`
throws the message that `XdbImp::Open` used to throw, `SyncItem` is gone). C2: the design essay lives in
`DataController.cpp` in English, the located Dutch comments are translated, the stale `REMOVE`/`OBSOLETE`
markers on live code say what the code is instead, and the listed stale comments are rewritten
(`Dijkstra.h` gets its include guard fixed rather than discussed). Not done: the general sweep of 347
commented-out statements in 161 files stays a policy, not a batch; `TifImp::GetValueClassFromTiffDataTypeTag`
is documented rather than changed and raises an open question, STG-N04: a TIFF without a SampleFormat
tag yields `VT_Unknown`, which `TifStorageManager` reports as an error rather than taking the
specification default (unsigned integer); needs a test with such a file before changing.

### C1 — delete dead code (all evidence = grep at HEAD)

- **`TreeItem::LoadBlobBuffer` (+ decl) and the commented `StoreBlobBuffer`** — `rtc/dll/src/tic/TreeItem.h:560-561`, `TreeItem.cpp:2710-2735`. 0 callers; the live loop at :2716 advances with `this->GetNextItem()` instead of `si->GetNextItem()` (TIC-01: infinite loop or single iteration, unreachable). Check whether the `BlobBuffer` typedef becomes unused.
- **`AbstrUnit::GetThisCurrTileID`** — `tic/AbstrUnit.h:163`, `AbstrUnit.cpp:956`. Declaration + definition only; returns 0 ignoring both parameters (TIC-11).
- **`s_IsDetectingIncInterest` + its two `MG_CHECK`s + 4 externs** — definition `act/Actor.cpp:1292`, checks `Actor.cpp:1305` and `tic/ItemLocks.cpp:704`, externs `ItemLocks.cpp:687`, `clc/PhaseContainer.cpp:26`, `stg/DllMain.cpp:44`, `stg/ViewPortInfoEx.cpp:49`; the only live writer sets **false** (`ViewPortInfoEx.cpp:154`), the `true` writer (`PhaseContainer.cpp:193`) is commented out. Both checks are tautologies (RTC-17). Removes one `RTC_CALL` data export (in-tree only); update the two dev docs that mention it.
- **`ViewPortInfoEx::SetWritability` husk** — `stg/dll/src/ViewPortInfoEx.h:33`, `.cpp:138-143` (body fully commented: "REMOVE, CLEAN-UP THIS FUNCTION AND ALL ITS CALLERS"); callers `stg/gdal/gdal_grid.cpp:167`, `stg/tif/TifStorageManager.cpp:168`.
- **`/*REMOVE … GetAsFLispExpr */`** — `tic/AbstrCalculator.cpp:511-517`, `AbstrCalculator.h:139`. Commented on both sides. **Land as a micro-commit before anything else touches `AbstrCalculator.cpp`** (g8-todos: three efforts contend for that file).
- **`/* REMOVE */` in `IrregularTileRangeData<V>::Load`** — `tic/Unit.cpp:463-468` (old reverse-order load).
- **`/* MOVE TO SubSet variant operator … END MOVE */`** — `clc/dll/src/Overlay.cpp:444-508`, 65 commented lines (`count`, `Compact`); the `TODO, TEMP, XXX` marker is inside it (CLC-31).
- **One-line commented leftovers** — `clc/Modus.cpp:772`, `clc/OperUnit.cpp:186`, `tic/ParallelTiles.h:14`, `stx/DataBlockProd.cpp:35`, `sym/LispRef.cpp:929`, `shv/GeoTypes.h:654`, `shv/GraphicObject.h:49`.
- **`sym/LispEval.cpp` commented-out Prolog/Query sections** — `/*…*/` at 138-148, 220-228, 231-237, 252-257, 271-282, 284-289, 325-333, 399-413, 417-445 (`/* REMOVE`), 551-557, 665-691, 694-706, **707-960** (254 lines: `PrologMatch`, class `Query`), **962-1022** (`BoolEvalCond*`). About 400 lines; the Dutch precondition blocks at 967-971/995-999 live inside them, so delete rather than translate. Do **not** touch the live `Match` at 199-216 (recursion plan OP6).
- **Unreachable `.xdb` sidecar-header and write path** — `stg/dll/src/xdb/XdbImp.cpp` `ReadHeader`, `WriteHeader`, `AppendColumn`, `WriteColumn`, `Create`, `freadln`; `stg/dll/src/xdb/XdbStorageManager.cpp:168-205` `SyncItem` and the `Create`/`WriteColumn` lines of `WriteDataItem` (:79-96). The manager itself is live as the base of `XyzStorageManager` (`"xyz"`, :234); STG-07 explains why these functions cannot be reached (every call passes `saveColInfo = false`, `Open` throws for write modes, `SyncItem`/`freadln` have no callers).
- **Dead constants `MAX_NR_TILES` and `MAX_TILE_SIZE`** — `rtc/dll/src/tic/TiledUnit.h:25` (used only inside the `CheckNrTiles` message) and `rtc/dll/src/tic/AbstrUnit.h:58` (no use at all); see TIC-02.
- **Unused locals/members** — `freadln` (`XdbImp.cpp:249-263`), `extraStartPoint` (`geo/OperPolygon.cpp:1197`), `org_sz` (`mem/FixedAlloc.cpp:1005-1014`), `m_Hasher` (`set/Cache.h:131`), `m_A2` (`cs_lock_map.h:268`).
- **Stale `// REMOVE` / `OBSOLETE` markers on live code** — either say what replaces the code or drop the marker: `tic/OperGroups.cpp:494` `FindOperByArgs` (4 live callers); `tic/AbstrDataItem.cpp:759` `HasUndefinedValues()` (15+ callers); `tic/DataController.cpp:568` / `MoreDataControllers.cpp:568` `CalcResultWithValuesUnits` (`// TODO G8: REMOVE`, 3 live callers: `MetaFuncApply.cpp:408`, `MoreDataControllers.cpp:727`, `shv/Theme.cpp:475`); `tic/DedicatedAttrs.cpp:220`; `tic/TreeItemProps.cpp:127,656,819`; `tic/SessionData.cpp:182,190`; `stg/GridStorageManager.h:41,212,341`; `shv/DataView.cpp:1226,1234`; `shv/DataItemColumn.cpp:807`; `shv/ShvSync.cpp:211`; `shv/GridLayer.cpp:158,444,525`; `clc/Union.cpp:470`; `clc/OperMisc.cpp:49`; `clc/OperUnit.cpp:520`; `tic/TicInterface.cpp:791`; `tic/TreeItem.cpp:2413`; `tic/TreeItemFlags.h:97`; `tic/TreeItemDataUsage.cpp:647,687`; `qtgui/DmsTreeView.cpp:568,811`; `stg/dbf/dbfImp.cpp:566`; `stg/dbf/dbfStorageManager.cpp:191`; `stg/gdal/gdal_grid.cpp:374`. Keep: `geo/DiscrAlloc.cpp:4344` (scheduled v21, #1177), `clc/Subset.cpp:1352-1373`, `clc/BoostXML.cpp:252,260`.

Commented-out statements (`// if/for/while/return/auto/const/dms_assert/MG_CHECK …`): 347 lines in
161 files; top files `tic/TreeItem.cpp` 11, `tic/TreeItemMetaInfo.cpp` 10, `geo/DiscrAlloc.cpp` 9,
`tic/OperationContext.cpp` 8, `stg/gdal/gdal_vect.cpp` 8, `tic/TreeItemDataUsage.cpp` 8. Policy:
delete unless annotated with a reason; the "activated DD-MM-YYYY, see if it holds" family
(`act/Actor.cpp:315`, `ser/FileMapHandle.cpp:167`, `shv/MenuData.cpp:163`, `shv/SelCaret.cpp:139,160`)
is decided by Phase 1d (make the assert a check, then delete the fallback). There are **0** `#if 0`
blocks repo-wide.

### C2 — translate, dedupe, rewrite comments

- **Duplicated design essay.** `rtc/dll/src/tic/DataController.cpp:41-66` ≡
  `rtc/dll/src/tic/MoreDataControllers.cpp:59-84` (verbatim, 23 lines, Dutch): the "ISSUES" list about
  explicit supplier items, lookahead locks versus managed-actor interest, temporary read/write locks,
  the flush-vs-store trade-off, and invalidation. Keep one English copy — in `DataController.cpp`
  under a "historical design notes" header, or better in `doc/tile-data-retainment.md` which owns the
  topic — and delete the other.
- **Dutch comments (~95 lines / 46 files).** Translate in place. Gists where the meaning matters:
  `tic/MoreDataControllers.cpp:292-299` (FuncDC::CalcResult contract: the found unit must be a config
  item; IsOld() holds, so with doCalc PrepareDataUsage does the real work; CacheTree → DoCalc arg1; a
  change of the 2nd arg requires changing m_Data); `tic/ItemLocks.cpp:882` ("look up the
  OperationContext and oc->Join()"); `stx/ConfigProd_functions.cpp:116` ("integrate with
  DoNrOfRowsProp()"); `geo/Canyon.cpp:89` ("swap arg1 and arg2" — stale, the code is consistent;
  delete); `tic/TreeItemMetaInfo.cpp:1155-1158` (when is Commit redundant; deleted export file;
  timestamp collisions); `tic/TreeItem.cpp:345, 1414, 1416, 1913`; `sym/LispList.h:21-23` (the
  Head/Tail guarantee); `tic/AbstrDataItem.cpp:893, 898`; `xml/XmlParser.h:45-46` (per-DLL `std::map`
  `_NIL`); `clc/OperUnit.cpp:256-257`; `clc/PhaseContainer.cpp:136-137`; `clc/Modus.cpp:921, 938`;
  `clc/OperLinInterpol.cpp:92`; `clc/OperPropValue.cpp:238`; `clc/OperAttrBin.cpp:306`;
  `clc/OperMisc.cpp:501`; `tic/AbstrUnit.h:238`; `tic/DataItemClass.cpp:274`; `tic/UsingCache.cpp:557`;
  `tic/UnitCreators.h:98`; `tic/TreeItemProps.cpp:805`; `tic/TreeItemDataUsage.cpp:687`;
  `tic/OperGroups.cpp:515`; `tic/MetaFuncApply.cpp:501`; `tic/stg/AbstrStorageManager.cpp:919`;
  `vt/SequenceArray.cpp:533`; `utl/Environment.cpp:210`; `stg/fss/FileSystemStorageManager.cpp:59`;
  `tic/stg/MemoryMappedDataStorageManager.cpp:184`; `stg/str/StrStorageManager.cpp:121`;
  `stg/odbc/OdbcStorageManager.cpp:830`; `stg/tst/src/main.cpp:8`; shv: `TableControl.cpp:396`,
  `GraphDataView.cpp:238`, `CounterStacks.cpp:125`, `FontIndexCache.cpp:294`, `PenIndexCache.h:136`,
  `GraphicPoint.h:18`, `PaletteControl.cpp:364`, `ViewPort.h:66`, `ScrollPort.h:30`; geo:
  `RegCount.cpp:23`, `RasterMerge.cpp:22`. Keep the issue-#1129 quotations in `WmsLayer.cpp:1055`,
  `ViewPort.cpp:1041`. `sym/LispEval.cpp:967-971, 995-999` go with C1.
- **Stale comments to rewrite.** `tic/TreeItem.h:247-248` ("Raw links for now; these become
  std::shared_ptr" — they already are: "owning single-linked sub-item list; these accessors return raw
  borrows"); `tic/TiledUnit.h:25` + `AbstrUnit.cpp:1032-1039` (TIC-02 message/limit mismatch);
  `act/Actor.cpp:1618-1638` (20-line "High-level Suggestions" restating `doc/deadlocks.md` — delete;
  move the two items not there — Was/Is/WasFailed/IsFailed naming, deferring StopInterest off-thread —
  to `g8-todos.md`); `stx/ConfigParse.cpp:485-487, 561-563` (unreachable `if`, resolved by STX-19);
  `stg/gdal/gdal_vect.cpp:1662` (answer the lifetime question); `geo/Dijkstra.h:63-68` (a comment
  discussing a stray parenthesis in the include guard instead of fixing it); `geo/Perimeter.cpp:63-69`
  (describes the fixed #1169 history — say so); `geo/OperPolygon.cpp:1246-1259` (`carry` is not reset
  when `isFirstPoint` starts a new polyline, so without `withEnds` the sampling phase leaks across
  polylines — document or reset); `act/Actor.cpp:1206-1207` (duplicate `dms_assert` with two comments
  — keep one); `act/Actor.cpp:124-134, 1279` (commented `UpdateLock` with "Maybe this is not the main
  thread" — decide); `stx/SpiritTools.cpp:11` (history note in code); `stg/gdal/gdal_base.cpp:711-717`
  (`isActive()` is now "was initialised"); `stg/tif/TifImp.cpp:257-269`
  (`GetValueClassFromTiffDataTypeTag` returns `VT_Unknown` when the optional SampleFormat tag is absent
  — the default is UINT, so most plain 8/16-bit TIFFs are reported unknown; fix or document);
  `stg/xdb/XdbStorageManager.cpp:220-228` (the xyz reader hard-codes 33-byte records with no validation
  of the actual line length — document or check).
- **Copy-paste comments.** `shv/PenIndexCache.cpp:263-269` says HFONT (SHV-55); `TileFunctorImpl.h:270`
  should state that lazy functors are invoked for size-0 tiles (root of CLC-25).

**Phase 4 verification:** T0 (Debug and Release, since deletions can expose unused-variable warnings),
T2. Effort S–M (~560 lines deleted, ~95 comment lines translated).

---

## Phase 5 — Renames and contract comments (R1–R4, C4)

Every count is a Grep over `*.{cpp,h,ipp,inc}` at HEAD. Conventions applied: engine functions
PascalCase `Get/Set/Is/Has`, `m_`/`s_`/`g_` prefixes, qtgui keeps Qt camelCase, snake_case for the
value/traits layer.

### R1 — typos in identifiers (S, low risk, ~120 sites / 30 files)

| old → new | files / occurrences — risk notes |
|---|---|
| `sd_DataControllerMapCriticalSeciton` → `…Section` | 1 / 4 (`tic/DataController.cpp:424,465,482,530`) + `doc/deadlocks.md:88`. No risk (TU-static mutex). |
| `tileFileChuncSize` → `tileFileChunkSize` | 1 / 2 (`tic/TileArrayImpl.h:339-340`). Local. |
| `updateChachedDisplayFlags` → `updateCachedDisplayFlags` | 4 / 5 (`qtgui/exe/src/DmsTreeView.h:59`, `.cpp:248`, `DmsMainWindow.cpp:260,1650`, `DmsOptions.cpp:282`). Keep qtgui camelCase. |
| `s_DrawingSizeTresholdInPixels` → `…Threshold…` | 3 / 6 (`shv/DrawPolygons.h:38,215,365`, `shv/FeatureLayer.cpp:2282,2307`, `qtgui/DmsOptions.cpp:249`) + 3 doc lines in `tu-reorg-and-export-surface-2026-08.md`. A `SHV_CALL` **data** export is renamed; only in-tree qtgui consumes it. |
| `SetDrawingSizeTresholdValue`, `onFlushTresholdValueChange`, `setInitialMemoryFlushTresholdValue` → Threshold | 3 / 4, 2 / 4, 2 / 3 (`DmsOptions.h:61,99,115`, `DmsOptions.cpp:247,287,396,408,478,519,648`, `DmsMainWindow.cpp:313`). Slot is connected by member pointer, not `SLOT("…")` — safe. |
| uic names `m_flush_treshold`, `m_flush_treshold_text` → `m_flush_threshold(_text)` | `.ui` 2 (`qtgui/exe/res/ui/DmsLocalMachineOptionsWindow.ui:128,389`) + 8 in `DmsOptions.cpp` + local `flush_treshold` :480; GUI text at `.ui:705,725`. Edit the `.ui` `name=` attributes (generated base class). The registry key `RegDWordEnum::MemoryFlushThreshold` is already spelled right — nothing persisted changes. |
| `TSF_Depreciated` → `TSF_Deprecated` | 5 / 6 (`tic/TreeItemFlags.h:87`, `TreeItemMetaInfo.cpp:108,111`, `TreeItem.cpp:1418`, `geo/ConnectedParts.cpp:54`, `geo/Connect.cpp:933`). Bit value unchanged. |
| `oper_policy::depreciated` → `deprecated`; `IsDepreciated()` → `IsDeprecated()` | 6 / 7 + 2 / 3 (`tic/OperPolicy.h:40`, `OperGroups.h:79,219`, `MoreDataControllers.cpp:140`, geo/clc operator groups). No collision (0 existing `IsDeprecated`). |
| `RSF_EventLog_HideDepreciated` → `…HideDeprecated`; `EventLog_HideDepreciatedCaseMixupWarnings()` | 3 / 7 + 3 / 4 (`utl/Registry.h:108,136`, `Environment.cpp:679,681,719,2821,2851`, `set/IndexedStrings.cpp:232`, `qtgui/DmsEventLog.cpp:439,447`). **Only the C++ enumerator changes**: the flag persists as bit 0x800000 of the DWORD registry value `StatusFlags` (value name untouched); the `/SW` command-line letter is unaffected. |
| `GetGeosNonDPointDepreciationFlag` → `…DeprecationFlag`; `isDepreciatedKernelSuffix`; locals `resNrOrg_depreciated`, `resSub_depreciated` | 1 / 5, 1 / 2, 2 / 12 (`geo/BoostPolygon.cpp`, `Connect.cpp`, `ConnectedParts.cpp`). None. |
| `"AbstractStorageManager"` → `"AbstrStorageManager"` | `tic/stg/AbstrStorageManager.cpp:116` (banner), `:632` (`CDebugContextHandle` string — surfaces in debug-context traces). |
| comment typos | `neccesar*` ×6, `occurence(s)` ×7, `teh` ×2, `wich`, `allready`, `loosing`, `poven`, `negiative`, `fullfill` ×3, `nonexistance`, stray backtick `throwItemError.h:27`. Skip `tic/AbstrCalculator.cpp:952` (contended file; fold into whichever effort lands there first). |

### R1b — typos in user-visible text (own commit, revertable alone)

"Depreciated" → "Deprecated" in 8 messages (`clc/Union.cpp:543`, `clc/OperExec.cpp:47`,
`tic/TreeItem.cpp:1161`, `mci/ValueWrap.cpp:92`, `ser/RangeStream.h:70`, `geo/BoostGeometry.h:699`,
`set/IndexedStrings.cpp:234`, `tic/MoreDataControllers.cpp:141`); `arguement`/`Attributues` in the
`MG_USERCHECK2` at `clc/Subset.cpp:1302,1318,1328`; `teh domain` `geo/RasterMerge.cpp:114`;
`occurences` `clc/BoostXML.cpp:126`; `alredy` `stg/xdb/XdbImp.cpp:440`; `.ui` labels at :705/:725.
T2 does not diff message text and the word appears only in generated `testcases/_out*` logs, but an
external wiki page or regression harness may match on it.

### R2 — names that lie (behaviour-preserving, ~75 sites / 35 files)

| old → new | files / occurrences — why |
|---|---|
| `DecCountIfAboveZero` → `DecCountIfAboveOne`; `DecCount` → `DecCountLeavesInterest` | `act/Actor.cpp:1235,1264,1276`; `:1227,1275`. The first decrements only when `> 1`; the second returns "count still non-zero". TU-local. |
| `XML_OutElement::IncAttrCount()` / `AttrCount()` → `void IncAttrCount()` + `bool HasAttrs() const` | `xml/XMLOut.h:215-216`, `XMLOut.cpp:410,576,587`. A bool named `…Count`, an `Inc` used as a predicate; rewrite callers as `if (HasAttrs()) …; IncAttrCount();`. Touches the `.dms`/XML serializer's delimiter emission — **must stay byte-identical, verify with T3**. |
| `leveled_section::isLocked()` → `IsHeldByAnyThread()` | `Parallel.h:204`, `cs_lock_map.h:203`, `act/TriggerOperator.cpp:63`, `tic/MoreDataControllers.cpp:191`, `tic/OperationContext.cpp:574,1967,2007,2253`. It is a `try_lock`/`unlock` probe; 7 of 7 callers assert "I hold it", which it cannot tell (and `try_lock` by the owner is formally UB). Do not confuse with the 30+ `handle.IsLocked()` of `DataReadLock`/`SequenceArray`. |
| `static_quick_assoc::assoc()` → `assocOrErase()` | `set/StaticQuickAssoc.h:23`, `act/Actor.cpp:1006`, `tic/TreeItemFunctionSpec.cpp:129,433`. Returns false **and erases** when the value is the default; all callers ignore the result. |
| `InterestPtr::release()` → `dismiss()` | `ptr/InterestHolders.h:188`, `tic/TreeItem.cpp:1391,2632-2634`, `tic/AbstrDataItem.cpp:722-723`. Keeps the count incremented and drops the shared reference — not `unique_ptr::release`. Leave the 5 `make_releasable_scoped_exit(...).release()` sites (different type). |
| `HtmlDecode(SharedStr&)` → `static HtmlDecodeInPlace(SharedStr&)` | `xml/XmlParser.cpp:244, 302`. External-linkage overload of the exported `utl/Encodes.h:15 HtmlDecode(WeakStr)` with a different table. |
| `AbstrUnit::GetTileCount(tile_id)` → `GetTileSize(tile_id)` (+ `GetPreparedTileCount` → `GetPreparedTileSize`, 5 / 6) | 17 / 27: `tic/AbstrUnit.h:184`, `AbstrUnit.cpp:854,1002,1029`, `Unit.h:129`, `Unit.cpp:1322,1327`; clc `CastedUnaryAttrOper.h` 2, `OperAttrVar.cpp` 1, `Union.cpp` 2; geo `BoostPolygon.cpp` 1, `ConnectMatrix.cpp` 1, `Connect.cpp` 2, `OperPolygon.cpp` 1, `nth_element.cpp` 3, `Poly2GridOper.cpp` 2; shv `FeatureLayer.cpp` 1, `GridDrawer.cpp` 2, `GridLayer.cpp` 1; stg `gdal_vect.cpp` 1. It is rows-in-tile-t and forwards to `AbstrTileRangeData::GetTileSize` (`TiledRangeData.h:72`); `GetNrTiles` is the tile count. Virtual on an exported class (in-tree only); no `DMS_*TileCount` C-API; no collision. **Own commit** (widest fan-out). |
| `TreeItem::SetTSF(sf, bool)` → `AssignTSF(sf, bool)` | 4 / 10 (`tic/TreeItem.h:427`, `TreeItem.cpp:1433,1442,1488,1511,1524,1535,1975`, `TreeItemDataUsage.cpp:769`, `UnitClassReg.h:60`). The 1-arg form (85 sites) sets *on*; dropping the bool silently picks it. |
| `GDALFieldCanBeInterpretedAsDouble` / `…AsInteger` → `GDALFieldHasGenuineDoubleValue` / `…IntegerValue` | `stg/gdal/gdal_vect.cpp:1640, 1654, 1762, 1774` (and take `SizeT`, not `SizeT&`). Answers "is the 0 GDAL returned a real zero", not "can be interpreted". |
| `ShpPolygon::Check()` → `void CheckInvariants() const` of `MG_CHECK`s | `stg/shp/ShpImp.h:206`, `ShpImp.cpp:781,800,813,822`. A `bool` that cannot be false (STG-70); lands with Phase 1a. |
| local `hackToFixFuncDcMakeResultDueToUnderspecifiedOperatorgroup` → `adiInterest` + comment | `clc/OperUnit.h:187`. The name is a rant (or remove per CLC-27). |

### R3 — counter / lock vocabulary (M, ~135 sites / 20 files)

| old → new | files / occurrences — notes |
|---|---|
| `TreeItem::m_ItemCount` → `m_ItemLockCount` | 9 / 56 (`tic/ItemLocks.cpp` 42, `DataLocks.h` 3, `AbstrDataItem.cpp` 2, `shv/DataItemColumn.cpp` 2, `TreeItem.h` 2, `ItemLocks.h` 2, `TreeItemDataUsage.cpp` 1, `OperationContext.cpp` 1, `DataLocks.cpp` 1). It is the signed item usage-lock count (`< 0` one writer, `> 0` readers) and `GetItemLockCount()` already calls it that; the name cost a debugging session (`stdptr-migration-handoff.md`). `AbstrDataItem::m_DataLockCount` (`AbstrDataItem.h:188`) is the *data* counter and stays distinct; 0 existing `m_ItemLockCount`. Fix `TreeItem.h:158` ("negiative"; state the sign convention). **Not concurrently with g8's `DataReadLock→…Handle` (704 occ) / `GetLockedDataRead→GetDataRead` (~107) — same files.** |
| `GetDataObjLockCount` / `GetDataRefLockCount` / `GetItemLockCount` | 14 + 15 + 6 sites. **Do not rename**: Obj = this item's own `m_DataLockCount`, Ref = the ultimate (referred) item's; `GetDataRefLockCount` is `TIC_CALL` and backs the C-API at `tic/AttrInterface.cpp:108`. Add a 3-line doc block at `AbstrDataItem.h:124` and a cross-ref from `ItemLocks.h:93` (C4). |
| `num*` → `nr*` outside gdal (40 sites) | `tic/TileChannel.h` 30 (`numElems/numWritable/numWrite` → `nrElems/nrWritable/nrToWrite`, incl. the `NrFreeInTile()` vs `numWritable` clash at :88/:114), `ser/FormattedStream.cpp:372-380` `numCount` 4, `geo/GridDist.cpp:646-651` 5, `geo/Dijkstra.cpp:1233`, `geo/BoostPolygon.cpp:950`, `stg/shp/ShpImp.h:324`. Measured convention: `nr[A-Z]` 1971 / 193 files vs `num[A-Z]` 119 / 7. Leave `stg/gdal/gdal_vect.cpp` (79, deliberately mirrors OGR's `getNumPoints()` family). |
| `numElems` (18 / 1) + `nrElements` (18 / 4) → `nrElems` (49 / 8 already) | `geo/BoostGeometryImpl.h` 6, `tic/AbstrDataItem.h` 2, `AbstrDataItem.cpp` 6, `tic/PerfMeasurement.cpp` 4. Leave `nrElem` (37 / 9, parameter idiom in `mem/*SequenceProvider.h`, `tic/DataArray.cpp` — TileFunctor-rename territory). |
| trace labels `RefCnt=` / `InterestCnt=` → `RefCount=` / `InterestCount=` | `tic/TreeItem.cpp:225`. `Cnt` appears nowhere else; nothing in `tools/` parses the `ST_MinorTrace` line. |

### R4 — name-accessor family (document, do not rename)

923 call sites / 167 files (`GetNameID|GetName|GetNameLock|GetFullName|GetDisplayName|GetFullCfgName|GetSourceName`);
#1227 just moved 234 lines / 108 files, so a further rename has no payoff. Extend the block at
`rtc/dll/src/mci/Object.h:163-172` into one table: `GetNameID()` → `TokenID`, pure, the canonical
format argument; `GetName()` → materialised `SharedStr` (allocates); `GetNameLock()` → `TokenStr`
holding the registry shared for its lifetime (deadlock rule R1/B6); `GetFullName()` → path from root;
`GetFullCfgName()` (`TreeItem.h:589`) → configuration path through the back-ref for cache roots;
`GetSourceName()` → diagnostic/source-location text; `GetDisplayName()` (`TreeItem.h:189`) → GUI label;
note `AbstrOperGroup::GetName()` (`OperGroups.h:104-107`, refcount bump) vs `Object::GetName()`
(allocation). Optional micro-rename with real value: `TreeItemDualRef::GetUlt()` → `GetCurrUlt()`
(9 / 6 — the only one of `GetCurr/GetNew/GetOld/GetUlt` that also *resolves*).

### C4 — one-line contract comments

| where | say |
|---|---|
| `rtc/dll/src/RtcBase.h:79` | `Bool` is `bit_value<1>`, the packed element type of boolean attributes; converts to/from `bool` but is not `bool`; never a return/predicate type (cf. `geo/geom/SpatialIndex.h:203` vs `:256`, `clc/include/AttrUniStruct.h:26-66`). |
| `rtc/dll/src/cpc/Types.h:204` | `SizeT` = `UInt64` on every platform, not `size_t`; it is the streamed/persisted size type. |
| `tic/TreeItem.h:408-410` | "Checked" = integrity-check-guarded (#1180/#1209): folds the IntegrityChecks of the item and its ancestors; calls `UpdateDC` and may *build* `mc_DC`; meta-thread only. |
| `tic/TreeItemDualref.h:159` | `GetUlt()` is the one accessor that resolves (`GetCurrUltimateItem`); the arm-kind vs liveness note at :139-173 already exists. |
| `tic/AbstrUnit.h:164,182,184` | `GetNrTiles()` = number of tiles; `GetCount()` = rows; `GetTileCount(t)` = rows **in** tile t (moot after R2). |
| `tic/AbstrDataItem.h:124-126` + `tic/ItemLocks.h:93` | Obj = own `m_DataLockCount` (−1 write, > 0 readers); Ref = the ultimate item's; `GetItemLockCount` = `TreeItem::m_ItemLockCount`. |
| `rtc/dll/src/xml/XmlConst.h:14` | `CompCharPtr` orders keys that are `;`-terminated (NUL also terminates); keys in `XmlConstMap` are `"name;"`; `SymbolGetChar` may receive a NUL-terminated token or a `;`-terminated slice — anything else compares wrongly. |
| `rtc/dll/src/act/garbage_can.h:26, 64-75` | Objects are placement-new'd into `std::vector<Block>` storage and relocated **bytewise** on growth/merge; `T` must be trivially relocatable (no interior pointers, no self-registration by address). |
| `rtc/dll/src/Parallel.h:204` | After R2: a `try_lock` probe — tells whether *some* thread holds the section; cannot verify the current thread does. |
| `rtc/dll/src/dbg/Diagnostics.h:82-95` | The Release-semantics table (Phase 1d). |
| `rtc/dll/src/ptr/InterestHolders.h:64-68` | Copying a non-null `InterestPtr` takes the non-throwing ≥ 1 path, which is why the copy ctor may be `noexcept`. |
| `rtc/dll/src/sym/Token.cpp:72-82 vs 121-133` | The two `st` overloads promise different things (`NoOtherThreadsStarted()` vs `IsMetaThread()`) — align (with RTC-16). |

**Phase 5 verification:** T0 Debug + Release (R1 renames a `SHV_CALL` data symbol; C1 removes an
`RTC_CALL` one), T1, T2, **T3 for the XMLOut item**, T4 (Options slider label; event-log "case mix-up"
checkbox still flips the same bit). Recommended order inside the phase: R1 → R1b → R2 (`GetTileSize`
as its own commit) → R3 (slotted around the g8 lock-handle rename) → C4 → R4.

---

## Summary table

| Phase | Title | Items | Effort | Fix-risk | Prerequisite |
|---|---|---|---|---|---|
| 0 | Confirmed defects, small fixes | 15 implemented 2026-09-05, 1 refuted (STG-14) | S each, ~1–2 days total | low | build + battery pending |
| 1 | Storage-reader validation + assert policy | implemented 2026-09-05 (1a, 1b incl. STG-N03, 1c incl. GEO-32, 1d); STG-15 deferred, STG-24 mostly refuted | M | low, except GEO-32 (med) | build + battery pending |
| 2 | Runtime-core robustness | implemented 2026-09-05 (all groups; RTC-36 by move-construct relocation, TIC-03 by validated entries) | S–M | low; RTC-70 follow-up split L/med | build green; battery after Phase 5 |
| 3 | Viewer and GUI | implemented 2026-09-05 (all groups; CLC-27 renamed, not removed) | S, two M | low; SHV-53 / CLC-27 med | build green; T4 GUI smoke pending |
| 4 | Dead code and comments | implemented 2026-09-05 (~1200 lines deleted incl. the .xdb path; STG-N04 opened) | S–M | none | build green |
| 5 | Renames and contracts | R1 ~120 sites, R1b 13, R2 ~75, R3 ~135, R4 doc, C4 12 comments | S–M | low (T3 for XMLOut) | R3 not concurrent with the g8 lock-handle rename |

---

## Appendix A — Refuted candidates (do not re-audit)

Each with the reason it is safe, so the same lead is not chased twice.

- **rtc:** `XmlParser.cpp:256` range end (`CompCharPtr` treats `;` as terminator); `IndexedStrings.cpp:37-41` `GetCS()` (the component constructs the section first); `InterestHolders.h:227-236` discarded `garbage_can` (dies after the lock is released); `InterestHolders.h:64-68` noexcept copy ctor (a non-null copy takes the fast path); `Actor.cpp:1292` `s_IsDetectingIncInterest` (never true — delete instead); `portable_task_group.cpp:82-86` missed notify (the destructor's locked notify follows); `Cache.h:84-103` zombie spin (progress guaranteed; add yield); `LispRef.cpp:668-676` (`ZeroSymbObjCache` grown before any `SymbObj` exists, never shrinks); `IndexedStrings.cpp:21-30` `&ref.back()` (the single writer always appends the NUL); `VectorMap.h:108` (`std::vector::back` contract); `FixedAlloc.cpp:901-953` ABA/pair/consume (dead code: `MG_CACHE_ALLOC_SMALL` off at :100; correct anyway — push with the current tag is what `boost::lockfree::stack` does); `SharedBase.h:39` `DuplRef` export (all instantiations inside Rtc.dll; add `RTC_CALL` anyway); `XmlParser.cpp:171-226` `m_Parent` (used only while the parent is on `openStack`).
- **tic/stx/clc:** `TicCalcSupport.cpp:93` `GetEnv` (all callers meta-thread — add the assert); `HofTypeChecker.cpp:1861` (meta-thread; `Eraser` only after a successful insert); `AbstrDataItem.cpp:1459-1462` avg (unsigned; the wrap is rejected by `avg <= 1 MiB`; fix TIC-N04 instead); `OperationContext.cpp:1483-1487` double count (deliberate, comment :1443-1448); `ItemLocks.cpp:752` `s_ActiveProducerSet` (every access under its mutex); `Metric.cpp:115` `== 0.01` (display special case for "%"); `AbstrStorageManager.h:198-212` `minParts*2` (callers pass 0x400/0xFFFF; use `X_GRANULARITY` instead of the literal 256); `TileChannel.h:146, 371` (`tn == 0` handled just above); `ConfigProd_functions.cpp:249` (grammar guarantees ≥ 8 + `()->x:=e`); `ConfigProd.cpp:566` (`nrofrows` rule only admits `uint64_p`/`hex64_p`); `AbstrDataBlockProd.cpp:124-195` (`DoSecondIntervalValue` does handle `VT_Unknown` at :164-168); `SpiritTools.cpp:66-117` untab (both passes apply the identical rule with the position carried over); `ExprProd.cpp:250-259` (`FindByScriptName` looks up by the registered name ID, so they are equal by construction — use `vc->GetNameID()` anyway); `ExprParse.cpp:29-41` (each call has its own grammar instance; recursion is intended for nested declarations); `SpiritTools.cpp:138` (last label); `SeparableMapping.h:392-403` (`k = 8` constexpr, `n >= 2` asserted); `SeparableMapping.h:681-700` per-thread map (locked, bounded by pool size); `Subset.cpp:895` (`int i` ranges over a small constant); `TreeItem.h:417/427` `SetTSF` (all nine 2-arg sites pass an explicit bool).
- **geo/stg/shv/qt:** `TifImp.cpp:666-672` / `GridStorageManager.h:214-215` (STG-14: the consumer resets a negative read result to 0 and default-fills the tile); `XdbImp.cpp:382-387` (`RecSize()` ≥ 1; the live xyz caller sets `headersize = 0`); `XdbImp.cpp:307/185` (:307 sits in the unreachable `ReadHeader`; the live xyz `ReadColumn` clips `cnt` to `NrOfRows()`); `TifImp.cpp:481-487` (needs a ≥ 4 GiB scanline); `gdal_vect.cpp:2585` (OGR CPLErrors on -1, the error frame converts); `gdal_vect.cpp:1662` (feature-owned scratch buffer, consumed immediately); `gdal_base.h:57` throwing dtor (guarded by `uncaught_exceptions`, standard scope-guard idiom); `dbfImp.cpp:889-899` (fresh impl, empty descriptions, copy loop skipped); `OperPolygon.cpp:1277-1282` (`nrPointsHere == 0` for zero-length segments when `dist > 0`); `Dijkstra.cpp:953-971` (`impedance <= 0` continues / is guarded); `DiscrAlloc.cpp:862-882` (`s ≥ 1`, unsigned wrap defined, `GetNrSteps` 0); `DiscrAlloc.cpp:471, 678` (single-threaded design; no parallel loops in the file); `Perimeter.cpp:63-69` (`write_only_mustzero` zeroes first; the comment is history); `Potential.h:89`, `Dijkstra.h:56` (per-call contexts); `GridCoord.cpp:326-354` (`AdjustGridNrs` keeps the sizes equal); `ShvUtils.h:332`, `ViewPort.cpp:892` (callers bound `n`; doubles); `DataItemColumn.cpp:564`, `TableControl.cpp:1357` (`SelRange` uses `UNDEFINED_VALUE`, checked before); `ShvUtils.cpp:220-231` (`"Copy"→"CopyCopy"` intentional); `LayerSet.cpp:388-411` (`pos` defined ⇒ non-empty); `FocusElemProvider.cpp:92` (temporary lock materialises the attribute); `DmsTreeView.cpp:139-152` (`split` returns ≥ 1); `DmsMainWindow.cpp:2274-2285` (lists are disjoint by construction at :1491-1492); `DmsViewArea.cpp:95-108` (no `WM_DESTROY` handling in shv, so :105 is the sole release); `DmsValueInfo.cpp:85-93` (`deleteAfterCurrentIndex` keeps the invariant; accessors use `.at()`).

## Appendix B — Conventions cheat-sheet used for the renames

Prefixes `m_` (member), `mc_` (config/cache-time TreeItem member, meta-thread only), `s_`/`sd_`/`sg_`
(statics), `cs_` (critical section), `g_` (exported global). Engine functions PascalCase
`Get/Set/Is/Has/Do/On`; `_impl` suffix for private helpers; qtgui camelCase. `Abstr*` = type-erased
base with the runtime `Class`; concrete = templated on the value type or named after a backend. `Curr`
infix = "read what is there, do not resolve". Value/traits layer snake_case (`tile_id`, `tile_offset`,
`row_id`, `task_status`, `oper_policy`). `SizeT` = `UInt64`, `CharPtr` = `const char*`, `Bool` =
`bit_value<1>`, `TokenID` interned symbol, `TokenStr` a registry read lock (never hold one across a
format sink — rule R1/B6). Ownership: intrusive `SharedPtr`/`WeakPtr` (raw, no liveness) for
non-TreeItem types; `std::shared_ptr`/`weak_ptr` for the TreeItem family with `newly_obj` /
`existing_obj` / `no_zombies` construction tags and `lock_or_cancel` for stored weaks; `InterestPtr`
keeps data computed, orthogonal to ownership.
