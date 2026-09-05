# Storage reads as key-expression operators (#587)

*Status: S0 and S1 implemented and committed (2026-09-05, see the status notes under section 4); S2 next. First draft 2026-09-05; revised the same day after two review rounds by the maintainer (section 5 records the rulings, all made; 10 and 11 are the maintainer's own proposals from the second round, adopted). Code anchors re-pinned on 2026-09-05 against the working tree at `main` `b81d6eea` (20.19.3) plus another session's uncommitted edits in `OperationContext.*`, `PhaseContainer.cpp`, `OperMisc.cpp`, `Union.cpp` and the grid managers.*
*Scope: `rtc/dll/src/tic` (key expressions, DataControllers, `PrepareDataUsage`, `OperationContext`), `rtc/dll/src/tic/stg` and `stg/dll/src` (storage managers), `clc` only for the PhaseContainer analogue and the `do` operator.*

---

## 0. Summary

Issue [#587](https://github.com/ObjectVision/GeoDMS/issues/587) was filed in 2023 as a
`SubstitutionError` on an IntegrityCheck that refers to the very item it guards when that
item is read from a storage. The 2026-09-05 comment on the issue widens it into a design
goal: **a storage read is an operator application in the key expression**, taking the
storage definition (name, type, sql string, table) as *values*, one call per table that
delivers the domain plus its attributes. The purpose, in the maintainer's words, is
referential transparency: separate the value from the context that configured it. Then

- every item read from a storage has a non-empty key expression, so the filed bug
  disappears by construction, together with the #1209 special exit that patched around it;
- the set of attributes to read is determined before the read starts, and attributes
  nobody asked for are not read (the PhaseContainer pattern: collect the members that
  carry interest in `PreCalcUpdate`, read exactly those in `CalcResult`);
- a read is scheduled like any other calculation, a `FuncDC` with an `OperationContext`
  whose suppliers are ordinary arguments, so the admission ledger, phase numbering,
  estimation and the storage critical-section gate all see one kind of task;
- two configurations reading the same table share one result, and a stored item gets a
  calculator like every calculated item has, which retires `PrepareDataRead` and the
  item-writer half of `OperationContext`.

The work is staged; every stage is compiled and tested before the next starts, and
none ships separately (20.19.3 was just pre-released, the next pre-release is at least a
week away):

| Stage | Delivers | Deletes |
|---|---|---|
| S0 | the filed symptom: a self-referring check on a stored item substitutes the source reference | nothing |
| S1 | `storage_read_*` operators, key synthesis, the `DC_Ptr` read calculator, `str` and `gdal.vect` on the new route | nothing (old route stays for the other managers) |
| S2 | every storage manager on the new route, `MMD` included | nothing |
| S3 | the cleanup | `PrepareDataRead`, item-writer read plumbing, the `#1209` exit, read-side supplier visits |
| S4 | one scan per table for all requested columns; identity by storage definition | the temporary holder reference in the spec; not optional, the work is not complete until it is gone |

Section 1 states the problem with the reproduction, section 2 inventories how a read
works today, section 3 is the design, section 4 the stages, section 5 the decisions and
their rulings, section 6 the risks, section 7 verification and documentation.

---

## 1. Problem statement

### 1.1 The filed bug, reproduced today

`scratch/issue587/probe587a.dms` (payload file beside it, one line `payload-42`):

```
container probe587a
{
	parameter<string> x
	:	StorageType    = "str"
	,	StorageName    = "%currDir%/payload.txt"
	,	IntegrityCheck = "strlen(x) > 0";

	parameter<string> y := x + '!';

	container checks
	{
		parameter<bool> c1 := y == 'payload-42!', IntegrityCheck = "c1 == true";
	}
}
```

`GeoDmsRun /S1 /S2 /S3 probe587a.dms @statistics /checks` on the 20.19.3 build of 2026-09-05 02:53:

```
Failure: SubstitutionError
/x

Context:
1. while building the IntegrityCheck 'strlen(x) > 0' of /x
```

exit 1. `probe587b.dms`, identical except that the check sits on the sibling `y`
(`IntegrityCheck = "strlen(x) > 0"` on `y`), reads the file and passes, exit 0.

The mechanism, all on the meta thread while the checker's expression is substituted:

1. `AbstrCalculator::slSupplierExprImpl` (`rtc/dll/src/tic/AbstrCalculator.cpp:1015`) takes
   the **raw** key expression for the checker's own holder --
   `(m_CalcRole == CalcRole::Checker && holder.get() == supplier) ? supplier->GetKeyExprImpl() : supplier->GetCheckedKeyExpr()`
   -- so that a check does not fold itself into the expression it guards.
2. `TreeItem::GetKeyExprImpl` -> `GetBaseKeyExpr` -> `GetCurrMetaInfo`
   (`rtc/dll/src/tic/TreeItemMetaInfo.cpp:211-252`). For an item without a calculator that
   `IsCurrLoadable()`, `GetCurrMetaInfo` returns the `MetaFuncCurry` arm
   (`MetaFuncCurry{ .fullLispExpr = CreateLispTree(this, false) }`, line 246-249, with the
   comment "will result in a SymbDC"), and `GetBaseKeyExpr` returns an empty `LispRef` for
   that arm (line 275-276).
3. Empty result -> `supplier->throwItemError("SubstitutionError")` (`AbstrCalculator.cpp:1022`).

Every other consumer of a stored item goes through `GetCheckedKeyExpr`, whose fallback
(`TreeItemMetaInfo.cpp:703-707`) produces the `sourceDescr` tree, and through the loadable
exit of `GetCheckedDC` (`:629-655`, the #1209 fix). Only the checker-on-holder path has no
fallback. So the filed symptom is a two-line matter (stage S0); the reason the issue is
still open is the second half of its title: *storage read functies in keyExpr opnemen*.

### 1.2 What the 2026-09-05 comment asks for

Determine all items a calculation needs from a storage before reading begins, and read
them through one operator call per table that takes the storage definition as arguments,
in the manner of `PhaseContainer` (a result tree whose members are collected on demand)
or `parse_xml` (a result tree built from one source). Separate functions for storage types
that need extra arguments. Examples given:

- `StorageReadTable(storageName, storageType; sqlString; domainElementType, attrName_0, attrValuesType_0, ...) -> domain -> { attribute<attrValuesType_0> attrName_0, ... }`
- `StorageReadAttributes(storageName, storageType; sqlString; existing_domain, attrName_0, attrValuesType_0, ...) -> container -> { attribute<...> attrName_0(domain), ... }`
- `StrStorageValue(storageName) -> parameter<string>`
- `StrStorageAttr(storageName; fileNames: i->string) -> attribute<string>(i)`
- `GDalVect(..., driver_options, etc.)`

The review of the first draft sharpened this (section 5): the storage definition travels
as one `attribute<string>` argument built with `union_data`; the domain type is a unit
argument, not a string; an `ExplicitSupplier` is expressed by wrapping the storage name
in `do(supplier, name)`; a stored item gets a real `DC_Ptr` calculator; and any reference
to the configuring holder item is a temporary aid that must be gone when the work is done.

### 1.3 Why the current shape costs more than the one bug

- **Reads are not calculations.** `doc/development/schedule-with-lookahead.md` §2.6 and §3
  list it as gap 4: "No I/O model for read operations (not `Operator`s at all)". A read is
  an `OperationContext` with a lambda payload (`CreateItemWriter`), so
  `RefreshEstimateForAdmission` returns early for it (`OperationContext.cpp:1913-1915`, no
  `FuncDC`), its estimate is computed only at run time inside `StorageReadHandle::Read`
  (`rtc/dll/src/tic/stg/AbstrStorageManager.cpp:1046-1075`), and `OperationContext.cpp`
  carries some thirty lines that branch on `m_FuncDC` or `m_TaskFunc` for the two kinds of task.
- **Supplier bookkeeping is hand-rolled.** `PrepareDataRead`
  (`rtc/dll/src/tic/TreeItemDataUsage.cpp:283-476`) assembles the read's futures itself:
  Calc-visited suppliers plus `ExplicitSuppliers` (release 20.16.0, after a parquet read
  raced ahead of the `exec_ec` writing it), plus the domain and values unit DCs; then
  `OperationContext::Schedule` retains them for the OC's lifetime in `m_KeptArgInterests`,
  `m_KeptArgItems`, `m_KeptArgUnits` (`OperationContext.h:374-393`, commit `df732ab7`),
  which a `FuncDC` gets for free through its `SupplInterest` and `OC_CalcResultFunc`.
  `doc/interest-and-futures.md` §2.5(b) lists the pre-charge `FutureData tmpFut = dc;`
  sites in this function as a systematic irregularity.
- **A table is scanned once per attribute.** Each attribute is read through its own
  `StorageReadHandle`: `GdalVectlMetaInfo::OnOpenForRead` re-selects the layer
  (`stg/dll/src/gdal/gdal_vect.cpp:307-360`), `ReadAttrData` enables one field and walks
  the features (`:1823-1920`), `SetCurrFeatureIndex` re-positions per tile (`:1925-1941`).
  For a CSV, GeoJSON or OSM source that is one full parse per attribute. ODBC iterates the
  recordset once per column (`stg/dll/src/odbc/OdbcStorageManager.cpp:736-764`); dbf reads
  its fixed-width records once per field.
- **Interest is announced twice.** `NonmappableStorageManager::StartInterest` keeps a per
  (holder, item) map of the manager's own suppliers (`AbstrStorageManager.cpp:860-885`),
  because those suppliers are not arguments of anything.
- **A stored item cannot be collected by a PhaseContainer.** `PhaseContainerOperator::PreCalcUpdate`
  fails a mirror member whose source has no `mc_DC` (`clc/dll/src/PhaseContainer.cpp:206`);
  a stored item has none.
- **Identity is the config item, not the data.** Two holders reading the same file and
  table read it twice and produce two domains that do not unify.

---

## 2. Inventory: a storage read today

### 2.1 Key expression and DataController

- `CreateLispTree(self, false)` (`rtc/dll/src/tic/LispTreeType.cpp:356-375`) yields
  `sourceDescr(<fullName>, <loadNumber>, (SigAndSub <sign> <storageSpec>))`; the storage
  spec is already `read(storageName, storageType, relPath)` or
  `readSql(storageName, storageType, relPath, sqlString, relPathToSqlString)`
  (`CreateStorageSpec`, `:220-259`). So the storage definition is in the key today, but
  only as identity text under a passive head.
- `CreateDC` maps a `sourceDescr` head to a `SymbDC` (`rtc/dll/src/tic/DataController.cpp:375-388`).
  `SymbDC::CallCalcResult` resolves the config item and calls
  `curr->PrepareDataUsage(DrlType::Suspendible)` on it (`rtc/dll/src/tic/MoreDataControllers.cpp:1242-1288`).
  The config item is both the request and the result; there is no cache item.
- `TreeItem::GetCurrMetaInfo` deliberately keeps the loadable case out of `GetOrgDC`
  ("not as variant 2, as that would create an infinite recursion from GetOrgDC",
  `TreeItemMetaInfo.cpp:251`), and `GetCheckedDC`'s loadable exit (`:629-655`) wraps the
  `sourceDescr` in the ancestors' checks for consumers. Its comment records that
  `PrepareDataUsageImpl` must keep consulting `GetCheckedDC` only for items with a
  calculator, or the SymbDC -> `PrepareDataUsage` -> `GetCheckedDC` cycle closes.

### 2.2 Calculators and how a DC is attached

- `HasCalculatorImpl()` (`rtc/dll/src/tic/TreeItem.cpp:939-957`) is true for a configured
  calculator member, a non-empty expression, or a unit with a configured range.
  `MakeCalculator()` (`:1079-1130`) builds the calculator from the expression and returns
  early for an item without one (`:1106`).
- An endogenous shadow of a cache sub-item, made by `TreeItem::Copy` when a config item
  refers to a cache root with members, gets a `DC_Ptr` calculator holding an already
  substituted key (`CreateCalculatorForTreeItem`, `TreeItem.cpp:1916`;
  `GetLispRefForTreeItem` -> `slSubItemCall`, `AbstrCalculator.cpp:129-143`). That is the
  engine installing calculators on config items, the precedent section 3.2 builds on.
- The DC is attached lazily: `VisitSuppliers(DetermineCalc)` calls `MakeCalculator()` and,
  when not determining state, `UpdateDC()` (`TreeItem.cpp:2192-2210`); `UpdateDC` ->
  `GetOrgDC` -> `GetCurrMetaInfo` -> `calc->GetMetaInfo()` -> `GetOrCreateDataController`
  -> `SetDC` (`TreeItemMetaInfo.cpp:565-612`). `ApplyCalculator` (`TreeItem.cpp:1068-1077`)
  only handles the meta-function arm (template instantiation, `for_each`), not `LispRef` keys.
- `TreeItem::DoInvalidate` (`TreeItem.cpp:2326-2366`) resets the calculator only for an
  item with an expression, and with it the `DC_Ptr` calculators of its sub-items
  ("reflection of composite result component").

### 2.3 Data preparation

`TreeItem::PrepareDataUsageImpl` (`TreeItemDataUsage.cpp:478-728`):

- the mmd branch maps a file into the config item directly (`:540-604`);
- `if ((drlType != UpdateNever) && HasCalculator()) PrepareDataCalc(...)` (`:619`),
  which calls `GetCheckedDC()->CallCalcResult()` (`:211-281`);
- `if (refItem->IsDataReadable()) PrepareDataRead(...)` (`:635`), where
  `IsDataReadable() = IsLoadable() && !HasCalculatorImpl() && !HasConfigData()`
  (`rtc/dll/src/tic/TreeItem.cpp:1743-1753`);
- a unit with neither gets `SetMaxRange()` (`:663`); a data item with neither fails
  with "No calculation rule or storage manager was specified" (`:726`).

`PrepareDataRead` (`:283-476`) in order: visit `CalcAndExplicitSuppliers`
(`rtc/dll/src/act/SupplierVisitFlag.h:31-34`) and collect a future per supplier DC
(`:320-334`); prepare the domain unit for its cardinality (`:348-358`); get the manager's
`StorageMetaInfo` and run `PrepareReadDataOrSuspend` (`:364-370`; the #933 pre-lock
step, `AbstrStorageManager.cpp:71-80`, waits for the values unit range); add the domain
and values unit DC futures (`:381-407`); `OperationContext::CreateItemWriter` with a lambda
that builds a `StorageReadHandle` (adopting the critical section the #933 gate acquired)
and calls `ReadItem` (`:409-438`); park the OC in `m_ReadAssets` under
`TSF_ReadAssetsInterestScoped` (`:440-445`); the #1152 "no primary data" failure (`:462-474`).

`TreeItem::ReadItem` (`:85-135`) logs `Read x from <storage>` and calls
`StorageReadHandle::Read` -> `DoReadItem`. `AbstrDataItem::DoReadItem`
(`rtc/dll/src/tic/AbstrDataItem.cpp:327-420`) either installs a `LazyTileFunctor` served by
a `reader_clone_farm` when the manager `AllowRandomTileAccess()` (grids, `:355-388`), or
reads all tiles into a `DataWriteLock` and commits (`:390-406`). `AbstrUnit::DoReadItem`
(`rtc/dll/src/tic/AbstrUnit.cpp:803-812`) calls `ReadUnitRange`.

### 2.4 Scheduling

`OperationContext::CreateItemWriter` (`OperationContext.cpp:853-859`) sets
`m_RequiredStorageManager` and `Schedule`s. `Schedule` (`:1001-1072`) connects the arg
futures as OC suppliers (`connectArgs`, `:2259-2306`, using `GetOperationContext(item)` for
non-`FuncDC` suppliers) and retains them (`:1044-1069`). `getUniqueLicenseToRun`
(`:1262-1316`) re-queues the task while the manager's critical section is contended
(`:1288-1302`) and while its phase is ahead of the active phase. `RefreshEstimateForAdmission`
skips it (`:1901-1925`). `Run_with_catch` runs the lambda (`:2614-2664`). `FuncDC::GetArgs`
(`MoreDataControllers.cpp:676-734`) calculates an argument at result-creation time when its
policy is `calc_always` (`MustCalcArg`, `:454-470`), and only at calculation time when it is
`calc_as_result` or `calc_subitem_root`.

### 2.5 Storage-manager entry points

`AbstrStorageManager` (`rtc/dll/src/tic/stg/AbstrStorageManager.h:214-335`):
`ReadDataItem(smi, ado, tile)`, `ReadUnitRange(smi)`, `VisitSuppliers(svf, visitor, holder, self)`
(read-side suppliers: `StrFilesStorageManager` adds the `FileName` attribute,
`stg/dll/src/str/StrStorageManager.cpp:214-224`; `AbstrGridStorageManager` adds the grid
domain, `stg/dll/src/GridStorageManager.cpp:113-136`), `DoUpdateTree` (schema sync per
`SyncMode`, meta thread). `NonmappableStorageManager` adds `GetMetaInfo(holder, curr, action)`,
`StartInterest/StopInterest`, `ReaderClone`. `StorageMetaInfo` (`:129-166`) is built from
`(storageHolder, curr)` and derives `m_RelativeName` from the config tree; the derived
infos walk the config tree for more: `GdalVectlMetaInfo` for the `SqlString` and the layer
name (`gdal_vect.cpp:252-300`), `OdbcMetaInfo` for the table holder and its sql string
(`OdbcStorageManager.cpp:347-360`), `GdalMetaInfo` for the `GDAL_Options`/`GDAL_Driver`/
`GDAL_LayerCreationOptions`/`GDAL_ConfigurationOptions` items, which it prepares with
`PrepareDataUsage(DrlType::Certain)` in its constructor (`AbstrStorageManager.cpp:959-984`).

Registered types (`IMPL_DYNC_STORAGECLASS`): `bmp`, `cfs`, `dbf`, `FSS`, `gdal.grid`,
`gdal.vect`, `gdalwrite.grid`, `gdalwrite.vect`, `MMD`, `odbc`, `pal`, `shp`, `strfiles`,
`str`, `tif`, `xdb`, `xyz`. All but `MMD` derive from `NonmappableStorageManager`; the
`gdalwrite.*` ones are write-only (`IsWriteOnlyStorage`, so never loadable).

### 2.6 The precedents

- **PhaseContainer** (`clc/dll/src/PhaseContainer.cpp`): `CreateResultCaller` builds the
  full mirror tree without data (`:46-121`); `PreCalcUpdate` on the meta thread collects the
  members that carry interest and are not yet ready, parking (interest, future) pairs in
  `m_ReadAssets` as a `phase_resource` under `TSF_ReadAssetsInterestScoped` (`:138-224`);
  `CalcResult` waits and hands the data over, then clears the resource (`:225-315`). A member
  that gains interest after completion re-enters the operator (#1167), because
  `FuncDC::CallCalcResult` restarts when `IsAllInterestedCalculatingOrDataReady` is false
  (`MoreDataControllers.cpp:428`) and the finished OC has been detached
  (`resetOperContextImpl`, `:164-186`). Its arg 0, the source container, is `calc_never`
  (`:32`), so it is resolved but never calculated, and the fence scan skips it (#1224).
  `DataController_SuppliesWholeResultTree` (`:1046-1052`) keeps a `subitem(phase, 'x')`
  reference from taking interest on every data member (values/shape split, #1167).
- **parse_xml** (`clc/dll/src/BoostXML.cpp:294-442`): builds a result container from a
  schema template in `CreateResultCaller` and fills all of it in `CalcResult`; it is
  `calc_requires_metainfo`, so it runs inline. Useful as the shape of "one call, one tree",
  not as the scheduling model.
- **do** (`clc/dll/src/OperExec.cpp:312-408`): `CommonOperGroup cop_do("do", calc_requires_metainfo)`
  served by `OperExpand`: `do(context: item, strings: attribute<string>) -> attribute<string>`
  returns the strings, placeholder-expanded in the context of the first argument. Both
  arguments are `calc_as_result` (`CommonOperGroup::GetArgPolicy`, `OperGroups.cpp:296-300`),
  so the first argument's result is calculated before `do` runs and `do` delivers the second
  argument's value. That is the ordering primitive section 3.1 uses for `ExplicitSuppliers`.
- **gridset** (`clc/dll/src/OperUnit.cpp:551-600`): its fourth argument is a unit that only
  carries the result's value type (`calc_never`); the operator creates the result unit from
  that argument's `UnitClass`. That is the shape of the domain-type argument in section 3.1.

---

## 3. Design

### 3.1 The synthesized key expression

For an item that is read from a storage (no calculation rule, no config data, a readable
storage parent, the storage exists) the engine synthesizes a key expression instead of the
passive `sourceDescr`:

| Config item | Key expression |
|---|---|
| table unit `T` (a unit below or at the holder, with stored attributes) | `storage_read_table(<spec>, <domainType>, <attrSpecs>)` |
| attribute under `T`, at any depth (`oppervlak`, `meta/status`) | `subitem(storage_read_table(...), 'meta/status')`, installed by the cache-root merge (3.2) |
| attribute `a` over a domain that is *not* read from this storage | `subitem(storage_read_attrs(<spec>, <domain>, <attrSpecs>), 'a')` |
| parameter read whole (`str`) | `storage_read_value(<spec>)` |
| grid attribute, stream item (`FSS`, `cfs`), `strfiles` attribute | `storage_read_attr(<spec>, <domain>, <attrSpec>, <extras>)` |

The `subitem` hop is the existing `slSubItemCall` (`LispTreeType.cpp:33-38`), the same
key the engine gives an endogenous shadow of a cache sub-item (`GetLispRefForTreeItem`,
`AbstrCalculator.cpp:129-143`), and `SubItemOperator` (`clc/dll/src/OperMisc.cpp:249-289`)
already resolves it to the existing cache member (`oper_policy::existing`, arg 0
`calc_subitem_root`).

**The spec argument.** One `attribute<string>` over the four-element default unit
`uint2`, built with `union_data`, carrying the four storage-definition strings:

```
spec := union_data(uint2, <storageName>, 'gdal.vect', 'SELECT ...', 'layer')
```

No new unit: `uint2` is `bit_value<2>` (`rtc/dll/src/RtcBase.h:80`) with the fixed range
`[0, 1 << 2)` (`rtc/dll/src/tic/TiledRangeData.h:193-202`), exactly four elements, which
is what `union_data` requires: it verifies that the part counts add up to the domain count
(`clc/dll/src/Union.cpp:290-296`). The review named `uint4`; in this code base that is
`bit_value<4>` (`RtcBase.h:81`) with sixteen elements, the wiki's "4 bits unsigned
integer", so `uint2` is the literal fit. Per-type string extras (`gdal.*`: driver and
options) are separate string arguments of the per-type operator, not a longer spec, since
no default unit has six elements. Non-string extras are separate arguments too: the
`GDAL_Options`/`GDAL_Driver`/`GDAL_ConfigurationOptions` items become `attribute<string>`
arguments (which retires the synchronous `PrepareDataUsage(DrlType::Certain)` calls in
`GdalMetaInfo`'s constructor), `strfiles` takes its `FileName` attribute as a data
argument (the comment's `StrStorageAttr(storageName; fileNames)`), grids take the grid
domain.

**Explicit suppliers.** The storage name element is wrapped: `do(ES1, do(ES2, 'C:/data/x.gpkg'))`.
`do` (section 2.6) calculates its first argument before it runs and returns the second, so
the read waits for every declared supplier, and the identity of the result says what it
waited for; #1218 already treats the `ExplicitSuppliers` relation as key-relevant when it
folds supplier-borne checks. The name is already expanded when it enters the key
(`GetNameStr()` of the manager), so `do`'s own expansion, done in the context of the
supplier item, is a no-op on it.

**The domain-type argument.** A unit expression, not a string: `uint32()` for a plain
table, `WPoint(rdc_meter)` for a TIFF whose `DialogData` names `rdc_meter` as the base
projection. The operator creates the result unit from the argument's `UnitClass` (as
`gridset` does from its fourth argument) and takes metric and base projection from it; the
storage supplies what the definition does not fix: the count for a table, range, cell
size and offset for a grid. Two consequences: two configurations reading the same table
with the same domain type get the *same* domain unit and unify, which is the referential
transparency the exercise is after; and the CRS wrapping that `Unit<V>::GetKeyExprImpl`
(`rtc/dll/src/tic/Unit.cpp:86-175`) adds for a unit with a calculation rule applies as
for any calculated unit. No existing unit operator yields a projected point unit without
factor and offset (`gridset` needs both), so the grid stage (S2) adds that carrier form.

**Attribute specs.** Per attribute, its name and its values unit: `'oppervlak', <vuKey>, 'meta/status', <vuKey>, ...`.
The name is the attribute's path relative to the table unit, so a table with multi-level
attributes, `unit<uint32> pand { attribute oppervlak; container meta { attribute<string> status; } }`,
lists `'meta/status'`: the result skeleton mirrors the path with a plain cache container
`meta`, `subitem(..., 'meta/status')` resolves it (`GetCurrItem` takes a path), and the
storage manager finds its column by the leaf name, as it does today through
`m_RelativeName` and `adi->GetName()` (`gdal_vect.cpp:1823-1835`). The value composition
is a suffix only when it is not the default `Single`: `'geometry:poly'` for `Polygon`,
`:arc` for `Sequence`, `:multipoint` for `MultiPoint` (`rtc/dll/src/mci/ValueComposition.h:18-26`);
strings and plain values carry no suffix. Only data items whose domain is the table unit
enter the table's spec; a parameter or an attribute over another domain below the table
follows its own domain (the `attrs` form) or fails as it does today.

The values unit is its checked key expression, not a type name: the result attribute must
carry the configured unit (metric, range, CRS) for `CheckResultItem` and `UnifyValues`,
so that no `convert` pass is needed afterwards, and a values unit read from another table
is then a real supplier. Names are string literals because `CreateResultCaller` needs them
at meta time to build the skeleton (`SubItemOperator::CreateResult` reads its name literal
the same way). Alternatives considered: names and compositions as two `union_data` lists
(more compact, but then the skeleton builder waits for a calculation on the meta thread);
composition folded into the values unit (it is a property of the attribute, not of the
unit).

**Argument policies.** The spec, the domain type and the values units are `calc_as_result`:
the spec because a `do(...)` inside it names a supplier with side effects that must run
when the read is *calculated*, not when its result skeleton is made (a `calc_always`
argument is calculated during `FuncDC_CreateResult` -> `GetArgs(true, false)`,
`MoreDataControllers.cpp:753-770`, i.e. whenever the item's meta info is determined); the
units because `MakeResult` already yields class, metric and projection at meta time, and
their ranges are needed at read time only. The attribute name literals are immediate.

**Where the key is produced.** A new virtual on `AbstrStorageManager`:

```cpp
struct ReadCallSpec { TokenID operName; LispRef args; };
virtual bool SupportsReadOperator() const;                              // false in the base; true per migrated class
virtual ReadCallSpec DescribeReadCall(const TreeItem* holder, const TreeItem* table) const;
```

`NonmappableStorageManager` gives the generic table/attrs/attr/value forms; a class that
needs more overrides it. Key synthesis in tic calls `DescribeReadCall` and never needs to
know the operator names; the operators live beside their managers in the Stg DLL and
register like `parse_xml` does in Clc. Names are lower case (`storage_read_table`,
`storage_read_attrs`, `storage_read_attr`, `storage_read_value`; the token table is
case-folded and the first interned spelling wins, #1161, `LispTreeType.cpp:261-265`),
engine-generated in S1 (`better_not_in_meta_scripting`, not documented as user operators);
whether a modeller may write them is decided after S2.

**The temporary holder reference (S1 to S3 only).** Until every manager can build its
`StorageMetaInfo` from the spec (S4), the managers' config-tree walks need the storage
holder. It enters as one more *string* element of the spec, the holder's full name,
resolved by the operator through the config root (as `SymbDC::MakeResult` does). Not as a
`sourceDescr` argument: a `DC_Ptr` calculator substitutes its key (section 3.2), and
`slSupplierExprImpl` refuses a `sourceDescr` of an item that contains the calculator's
holder as a circular dependency (`AbstrCalculator.cpp:999`), which is exactly the case
when the table unit *is* the storage holder. A string is opaque to substitution, carries
no interest, and is obviously a context leak to be removed. S4 removes it; until then two
holders reading the same table still get two DCs, the status quo. S4 also drops the
`sourceDescr`-derived `read`/`readSql` spec from `CreateLispSubTree` where it is no longer
consulted.

### 3.2 The calculator of a stored item: a `DC_Ptr` (decision 1, elaborated)

The first draft proposed attaching the read `FuncDC` to the item through `SetDC` while
leaving `mc_Calculator` empty, gated by a new predicate. What that meant, concretely:
`GetCheckedDC`'s loadable exit hands out exactly such a DC today, without storing it
(`TreeItemMetaInfo.cpp:629-655`), and `SetDC` itself does not require a calculator;
`config-cache-separation.md` §3 classifies `mc_DC`/`mc_RefItem` as shared wiring rather
than config attributes. The motive was to leave `HasCalculator()` false for stored items,
so that `CommitDataChanges` and the storage managers' "has a calculation rule" checks
keep meaning what they mean. The review found the notion foreign, and on inspection it
saves nothing: every place that asks "is this item calculated?" would need the inverse
audit (which sites must treat a stored item as calculated after all), `DoInvalidate` and
the config dump would need the same care, and `HasCalculator()` would keep lying in the
other direction. So: a stored item gets a real calculator.

**The mechanism.** A table unit `T` gets `DC_Ptr(storage_read_table(...))` from
`DescribeReadCall`. Each stored attribute under it, at any depth, gets
`DC_Ptr(subitem(<T's checked key>, 'meta/status'))` without any synthesis of its own: the
cache-root merge that gives the shadows of cache sub-items their calculator today does it.
`UpdateMetaInfoImpl` copies the result root's members onto the config item with
`MergeProps` (`TreeItemMetaInfo.cpp:163-185`), `Copy` installs `CreateCalculatorForTreeItem`
on every existing config sub-item that has no calculator (`TreeItem.cpp:1916`), and
`GetLispRefForTreeItem` keys it as `subitem(T->mc_DC->GetLispRef(), <relative path>)`
(`AbstrCalculator.cpp:129-143`), the nested container `meta` being matched by name and
left as it is. Because that key is built from `T`'s *checked* DC, a check on the table
guards every attribute (#1180). `DescribeReadCall` is therefore needed for table units,
for the `str` parameter and for single attributes (grids, streams), not for attributes
under a table.
`GetCurrMetaInfo` then takes its normal `HasCalculatorImpl()` branch (`TreeItemMetaInfo.cpp:224`),
`GetKeyExprImpl` is non-empty (the filed bug is gone), `UpdateDC` folds the ancestors'
checks and attaches the DC, `PrepareDataUsageImpl` takes `PrepareDataCalc` (`:619`),
and `IsDataReadable()` turns false (`TreeItem.cpp:1748`), which skips `PrepareDataRead`
without touching it.

What it entails:

1. **Marking.** The read calculator must be distinguishable from a configured one: a
   `DC_Ptr` subclass (`StorageReadCalc`) or a `CalcRole::StorageRead` value (the enum has
   room, `AbstrCalculator.h:20-28`). Predicates: `HasCalculator()` keeps its contract
   ("`GetCalculator()` returns a calculator") and becomes true for stored items;
   `HasConfiguredCalcRule()` is the old `HasCalculatorImpl()` body (expression, data
   block, configured range); `IsReadFromStorage()` is "has a storage-read calculator".
2. **Installation and its timing.** Not in `MakeCalculator()`: `DetermineState` visits
   `DetermineCalc` (`TreeItem.cpp:2193`) *before* `UpdateMetaInfoImpl2` runs the manager's
   `UpdateTree` (`TreeItemMetaInfo.cpp:821`), and the table key enumerates the table's
   stored sub-items, which `SyncMode` may still add. The read calculator is installed at
   the end of `UpdateMetaInfoImpl2`, after `UpdateTree`, followed by `UpdateDC()` and by
   the cache-root merge of `UpdateMetaInfoImpl` (`:163-185`, factored into a helper so it
   can run after `UpdateTree` as well), which hands the attributes their calculators; the
   commented `// UpdateDC();` at `TreeItemMetaInfo.cpp:152` marks the older intention.
   For an attribute, its parent has been updated first (`UpdateMetaInfoImpl2` starts with
   `GetTreeParent()->UpdateMetaInfoIfNotAlready()`), so the table key exists when the
   attribute asks for it. `HasCalculatorImpl()` is `noexcept` and hot, so "is a stored
   item" is a cached flag set at installation, not a file stat.
3. **Substitution.** `DC_Ptr` inherits `AbstrCalculator::GetMetaInfo`, which substitutes
   the key. The key holds string literals, `do(...)` applications and already substituted
   unit keys, so substitution is the identity, except for a `sourceDescr` head, which is
   re-resolved and checked for circularity. Hence the holder as a string (3.1) and the
   rule that no unit or supplier key inside the read key may be a `sourceDescr` of an
   item inside the table's own subtree (a configured circularity, reported as such).
4. **Invalidation.** `DoInvalidate` (`TreeItem.cpp:2352-2363`) resets the calculator of an
   item with an expression and the `DC_Ptr` sub-calculators under it. Extend: a read
   calculator is reset like an expression's, and a table's attribute calculators with it,
   so a re-sync after invalidation rebuilds the key from the current sub-items.
5. **Copy and templates.** `Copy` copies a calculator member only for data blocks
   (`TreeItem.cpp:1920`); `HasOwnCalculatorNow` (`:1756-1760`) stays false for stored
   items, so `SetInheritFlag` semantics do not move. Items in templates are never
   loadable (`GetStorageParent` returns nothing under `TSF_InTemplate`).
6. **Dump and display.** `XML_Dump` writes `GetExpr()` and the data-block calculator only
   (`rtc/dll/src/tic/TreeItemXmlDump.cpp:240,405-410`); `TreeItem::GetExpr` returns the
   configured expression member (`TreeItem.cpp:819-824`). The config dump is unchanged.
   The detail page's key expression shows the operator form.

**The `HasCalculator` audit** (69 sites in 26 files, `HasCalculator` and `HasCalculatorImpl`).
Sites where a stored item must *not* count as calculated, to be switched to
`HasConfiguredCalcRule()` (or `!IsReadFromStorage()`):

| Site | Why |
|---|---|
| `rtc/dll/src/tic/TreeItemDataUsage.cpp:862` `CommitDataChanges`, and `:854` (`IsDataReadable()` early return, now false) | the write-back guard: an item read from a storage is never written to that storage; make it explicit with `if (IsReadFromStorage()) return true;` |
| `TreeItemDataUsage.cpp:146` `DoWriteItem` | reached only through commit; same rule |
| `rtc/dll/src/tic/stg/AbstrStorageManager.cpp:772` `DoUpdateTree` | "both a Calculation Rule and a read-only storage spec" would fire on every stored item |
| `AbstrStorageManager.cpp:825` `VisitSuppliers`, `ExportInfo` | export sidecar applies to items that are written |
| `stg/dll/src/gdal/gdal_vect.cpp:210` (`IsValidGeometry`), `:3042`, `:3066` (`DoUpdateTree`) | schema sync and the "has a calculation rule and a configured data source" warning at `:3049` |
| `stg/dll/src/tif/TifStorageManager.cpp:402-406,425`, `stg/dll/src/bmp/BmpStorageManager.cpp:403`, `stg/dll/src/gdal/gdal_grid.cpp:669`, `stg/dll/src/shp/ShpStorageManager.cpp:432` | `DoUpdateTree` skips a storable item with a rule |
| `stg/dll/src/DllMain.cpp:77` `TreeItem_AsColumnItem` | a read-only column with a rule is not a column |
| `rtc/dll/src/tic/stg/MemoryMappedDataStorageManager.cpp:204`, `TreeItemDataUsage.cpp:548-549` | mmd: a holder with a rule is written, a read holder merges the dictionary; must mean the configured rule once mmd reads are operators (S2) |
| `rtc/dll/src/tic/TicItemSupport.cpp:517` `item_origin` | a stored item is `exogenic`, not `calculated` |
| `rtc/dll/src/tic/Xml/XmlTreeOut.cpp:488,553,763,770` | detail page: source item, calculator description, `DataSource`/`DataTarget` rows |
| `qtgui/exe/src/DmsTreeView.cpp:554-556` | tree icons: a stored item under a read-only storage is a storage item |
| `shv/dll/src/ShvDllInterface.cpp:500` | `vsfExprEdit` offers expression editing |
| `rtc/dll/src/tic/TreeItemSet.cpp:121` | `CSS_NoCaseParams` skips items whose calculator `IsDcPtr()`; a read calculator is a `DC_Ptr` too and must not be skipped |
| `rtc/dll/src/tic/TreeItem.cpp:132-140` `IsEditable`, `:1040-1060` `AssertDataChangeRights` | no predicate change: both already refuse an item with a non-data-block calculator, which is the ruling of decision 7 (below); only the refusal message should name the storage |

Sites where the new meaning is wanted or indifferent: `TreeItemDataUsage.cpp:619` (the
gate), `:651`, `:299` (unreachable for stored items); `TreeItem.cpp:974`
(`CanSubstituteByCalcSpec`), `:1236`, `:1748`, `TreeItem.h:329` (`IsDerivable`);
`AbstrCalculator.cpp:131,1012`; `TreeItemMetaInfo.cpp:224`; `Explain.cpp:663,704` (a
stored item is now explained as a calculation, with the leaf rendering of 3.8);
`TreeItemPath.cpp:562,576` and `TicInterface.cpp:864,878,886,905,1120,1175` (template-source
and error-source walks: a read calculator has no template source, `MustEvaluate` on an
empty expression is false); `TreeItemProps.cpp:775` and `TicInterface.cpp:463` (the
`HasCalculator` property and C API now report true for stored items; no consumers in
qtgui, shv or python); `clc/dll/src/BoostXML.cpp:332,392,423` (schema items live in
templates); `LispTreeType.cpp:223` (a comment).

**The edit path (decision 7, ruled).** `IsEditable()` is true today for a stored item in
a writable storage (`!IsCurrLoadable() || IsCurrStorable()`), and shv lets a user edit
such values (`DataItemColumn.cpp:1229,1247`, `PaletteControl.cpp:607`,
`PaletteCopyOnWrite.cpp:49-98`, `EditPalette.cpp:182`, `ClassBreakClipboard.cpp:681`).
That editability is dropped: an item read from a storage has its authentic value in the
storage, and a change of that value is an external source change (3.10), not an edit.
The read calculator settles it without a new predicate, because `IsEditable()` already
returns false for an item whose calculator is not a data block (`TreeItem.cpp:136-137`),
and `AssertDataChangeRights` already refuses an item without config data
(`TreeItem.cpp:1044`); only its message should name the storage instead of "a
calculatable item". Palettes read from a storage then take the copy-on-write route the
palette editors already have for non-editable attributes.

What stays editable, unchanged: items with *config data*, a data block
(`GetCalculatorMember()->IsDataBlock()`) or an authentic array in a data item without a
storage, such as under `/Desktops/ViewData`. Their edit model is the referentially
transparent one this plan extends to storages: a user edit mints a new epoch, the
data-block calculator's key is `union_data(<domain>, <values...>)` built from the values
themselves (`GetCheckedKeyExpr`, `TreeItemMetaInfo.cpp:670-700`), so the key changes with
the edit and every dependent downstream item gets a new key expression and a new DC that
represents the updated values. A data-block item below a storage holder is written, not
read (`IsDataReadable()` is already false for it through `!HasConfigData()`), so it is not
a stored item in the sense of this plan and keeps its editability.

### 3.3 The result and how config items bind to it

`storage_read_table` produces a cache root unit of the domain-type argument's class with
one cache `AbstrDataItem` per attribute spec, over that root, with the configured values
unit and composition; no data (`CreateResultCaller`, meta thread). `storage_read_attrs`
produces a cache container with attributes over the domain argument. `storage_read_value`
and `storage_read_attr` produce a single cache data item.

- The table unit `T` binds through `SetDC` (`TreeItem.cpp:851-910`) -> `MakeResult` ->
  `SetReferredItem(root)`: `T`'s range is the root's range once read, as for
  `unit<uint32> u := unique(x)`.
- An attribute `a` binds through the `subitem` `FuncDC` to the cache member; `mc_RefItem`
  points at it and `GetCurrUltimateItem()` delivers its data, the shadow-item mechanics
  that `unique(x)/values` and `select_with_attr_by_org_rel(...)/org_rel` use today.
- Interest: `TreeItem::StartInterest` (`TreeItem.cpp:2596-2638`) holds `mc_DC` and
  `mc_RefItem`; the `subitem` `FuncDC`'s `SupplInterest` reaches the table `FuncDC`; its
  `TreeItemDualRef::StartInterest` holds the root. Nothing else is needed, which is what
  makes `NonmappableStorageManager::StartInterest`'s holder map redundant (S3).
- The values/shape split of #1167 applies: naming `T/a` must not take interest on every
  member. Generalize the `token::PhaseContainer` test in
  `DataController_SuppliesWholeResultTree` (`MoreDataControllers.cpp:1046-1052`) into an
  `oper_policy` bit (`members_on_demand`) set by both PhaseContainer and the read table
  operators.

### 3.4 Selective reading and re-entry

The operators follow `PhaseContainerOperator` step for step:

1. **`CreateResultCaller`** (meta thread, at `MakeResult`): the skeleton of 3.3, from the
   attribute name literals and the units' `MakeResult`.
2. **`PreCalcUpdate`** (meta thread, at every `CallCalcResultImpl`,
   `MoreDataControllers.cpp:981`): walk the result; collect the members with
   `GetInterestPtrOrNull()` that are not `IsDataReady`; keep a `SharedTreeItemInterestPtr`
   per collected member so their interest cannot drop during the read; store the request
   in the root's `m_ReadAssets` under `TSF_ReadAssetsInterestScoped`. The root unit counts
   as a member: when it carries interest and has no range yet, the request includes the
   range read, so a bare `#T` reads the header only. Values unit ranges no longer need
   `PrepareReadDataOrSuspend`: they are arguments, ready before the OC runs.
3. **`CalcResult`** (worker, under the manager's critical section, 3.6): read the spec
   (its data is ready, it is an argument); open the storage once; `ReadUnitRange` if
   requested; read the requested attributes, tile by tile; commit each member's
   `DataWriteLock`; publish the measured element width and the `[storage read]` progress
   line per member (both in `StorageReadHandle::Read` and `ReadItem` today);
   `SetIsInstantiated`; clear the request. Grid attributes get the `LazyTileFunctor` plus
   `reader_clone_farm` that `AbstrDataItem::DoReadItem` installs today
   (`AbstrDataItem.cpp:355-388`), so tile reads stay lazy and parallel.
4. **Re-entry**: a member demanded later makes `IsAllInterestedCalculatingOrDataReady`
   false (`rtc/dll/src/tic/ItemLocks.cpp:687-724`), `CallCalcResult` restarts,
   `PreCalcUpdate` collects only the new members, and a fresh OC reads only those. This
   is the #1167 mechanism; the read operators must be `CanRunParallel()` (no
   `has_external_effects`, no `calc_requires_metainfo`), because the inline branch of
   `CallCalcResult` tests `IsAllDataCurrStandby`, which would force every member.
5. **Failure**: a failed read fails the collected members and the DC with
   `FailType::Data`, carrying the `while reading data from <file>` context that
   `ReadItem` adds today, named through the #795 origin machinery. Storage existence is
   still decided at key-synthesis time (`IsCurrLoadable` -> `DoCheckExistence`), so an
   absent storage falls through to the existing "No calculation rule or storage manager"
   failure. `OpenForRead`'s `storageHolder->CatchFail(FailType::MetaInfo)`
   (`AbstrStorageManager.cpp:887-908`) becomes a data failure of the read DC instead of a
   meta failure stamped on the holder from a worker thread.

In S1 to S3 `CalcResult` reuses the per-item readers unchanged: for each requested member
it calls `AbstrDataItem::DoReadItem` / `AbstrUnit::DoReadItem` on the *cache* item with a
`StorageMetaInfo` whose data target is the cache item and whose relative name and
property walks come from the config item found through the holder string (a second
pointer, `m_ConfigCurr`; the relative name is passed in, since
`curr->GetRelativeName(storageHolder)` is meaningless for a cache item). Per-manager
assumptions that `CurrWD()` is the locked config item (for instance
`OdbcStorageManager.cpp:742`) are adjusted in the same step. The one-scan `ReadDataItems`
and the spec-only meta infos come in S4.

### 3.5 Storage-manager interface after the change

Added:

```cpp
// AbstrStorageManager
virtual bool SupportsReadOperator() const;
virtual ReadCallSpec DescribeReadCall(const TreeItem* holder, const TreeItem* table) const;

// NonmappableStorageManager, S4: one scan for all requested columns of one tile
struct ReadTarget { const AbstrDataItem* cacheItem; AbstrDataObject* target; SharedStr relName; };
virtual FileResult ReadDataItems(StorageMetaInfoPtr tableInfo, std::span<ReadTarget> targets, tile_id t);
    // default: ReadDataItem per target; gdal.vect, dbf, odbc override with a single pass
```

Changed: `StorageMetaInfo` gets a constructor taking the cache target, the config item
and the relative name (S1), and per class a constructor from the spec strings instead of
a config-tree walk (S4: `GdalVectlMetaInfo` from sql string and layer name, `OdbcMetaInfo`
from the sql string, `GridStorageMetaInfo` from the grid-domain argument). For
`MmdStorageManager` (S2) the read does not copy: the member's data object is the file
mapping that `OpenFileData` creates today (`TreeItemDataUsage.cpp:585-591`), so the
operator base takes an `AbstrStorageManager`, not only a `NonmappableStorageManager`, and
`ReadDataItem` is not needed there. In S4 the
operator constructs a read-only manager instance per DC from the spec's type and name (as
`ReaderClone` does, `AbstrStorageManager.cpp:786-795`), so the read needs no holder at
all; the holder's own `m_StorageManager` remains for `UpdateTree` and for writing.

Removed in S3: `PrepareReadDataOrSuspend`, `StartInterest/StopInterest` and
`m_InterestHolders`, the read-side halves of the `VisitSuppliers` overrides (`ExportInfo`
stays: it is the write side). `ReadDataItem`, `ReadUnitRange`, `OpenForRead`,
`DoOpenStorage`, `ReaderClone`, the handles and `DoUpdateTree` are unchanged.

### 3.6 Scheduling integration

- **One kind of task.** A read is a `FuncDC` OC: `ScheduleCalcResult`
  (`OperationContext.cpp:2426-2543`) collects the arg futures, `Schedule` connects them,
  `RunOperator` measures it. `ExplicitSuppliers` arrive as arguments of `do` inside the
  spec (3.1), so the OC waits for them like for any supplier.
- **Storage critical section (#933).** A new `Operator` virtual
  `GetRequiredStorageManager(const ArgRefs&) -> SharedPtr<NonmappableStorageManager>`
  supplies the gate. It cannot run at `ScheduleCalcResult`: the spec is `calc_as_result`
  and its `do(...)` may still be computing a side-effect supplier, and reading the spec
  there would join that supplier on the meta thread. It runs when the OC becomes runnable,
  the moment `RefreshEstimateForAdmission` already uses (all suppliers done, spec data
  resident, no `cs_ThreadMessing` held), and sets `m_RequiredStorageManager` before
  `getUniqueLicenseToRun` tests it (`:1288-1302`). `CalcResult` finds its OC through
  `CancelableFrame::CurrActive()` and adopts the held section into the
  `StorageReadHandle(adopt_storage_lock)`, the hand-over the lambda performs now
  (`TreeItemDataUsage.cpp:423-428`); `releaseStorageLockIfHeld` (`:863`) stays as the backstop.
- **Estimates.** `Operator::EstimatePerformance` on the read operators is today's
  `EstimateReadResources(item)` summed over the requested members; `m_Estimate` is filled
  at schedule time and refreshed by `RefreshEstimateForAdmission` like any operator. This
  closes gap 4 of `schedule-with-lookahead.md` §3 and makes the §4 table's "attach a
  ResourceEstimate to the item-writer OC" row moot.
- **Phases.** `FuncDC::GetArgs` raises the DC's phase to its arguments' phases, and
  `MarkCacheItems` (`MoreDataControllers.cpp:736`) stamps the result; the config item's
  phase follows through `FenceNumberScan`. A stored item inside a fenced container is now
  collectable by a PhaseContainer (it has an `mc_DC`), which the #1167 comment lists as an
  open sore.
- **Identity.** From S4 on, the key is the storage definition: two configurations reading
  the same table share one DC, one read, one domain. Indirect properties
  (`StorageName = "=expr"`) are evaluated at meta time as now, so that `UpdateTree` can
  open the storage; the expanded name lands in the key, which is what
  `doc/incremental-updates.md` §3.3-6 wants for gap C13.

### 3.7 IntegrityCheck folding and the SubstitutionError

With a real calculator the checker's holder substitutes normally: `strlen(x) > 0` on a
stored `x` becomes `gt(strlen(storage_read_value(...)), 0)`, `UpdateDC` folds it into
`IntegrityCheck(storage_read_value(...), gt(...))`, and consumers of `x` carry the guard
as a scheduled supplier. The #1209 loadable exit of `GetCheckedDC` becomes unreachable
for these items and is deleted in S3; `fn_test_icheck_storage` and its negative twin must
keep passing through the regular `UpdateDC` fold. The validate phase
(`TreeItem::DoUpdate`, `TreeItemMetaInfo.cpp:1124-1151`) then finds the folded condition
ready, as it does for calculated items (#1181), instead of being the only evaluator.

### 3.8 Interactions to keep working

- **Explain (F2).** `CalcExplImpl::AddLispExplanation` stops at a `sourceDescr` head
  (`rtc/dll/src/tic/Explain.cpp:613`); the read heads need the same treatment, rendered
  as "read from <storage>" with the attribute, so that explaining a value read from a
  file does not descend into the spec strings.
- **`@sourcedescr` / FullSource.** `SourceDescr.cpp:114` decides "read from" by
  `IsDataReadable() || IsStorable()`; add `IsReadFromStorage()`.
- **Detail pages and the `HasCalculator` property.** The key expression shown for a
  stored item changes from the `sourceDescr` tree to the operator form; the
  `HasCalculator` property reports true. The dump is unaffected (3.2 item 6).
- **MMD (push-back 1, adopted).** `MmdStorageManager` reads go through the operator route
  too, in S2, like `FSS` and `cfs`: one `storage_read_attr` per stored data item, keyed by
  the store's spec, domain and name; the dictionary's units keep their configured range
  (`USF_HasConfigRange`, a range calculator, not a read). What stays mmd-specific is the
  data object: `CalcResult` does not copy but installs the file mapping that
  `OpenFileData` creates today (`TreeItemDataUsage.cpp:585-591`) as the cache member's
  `m_DataObject`, so a `DataReadLock` still maps the file into memory instead of reading
  it. The write side is untouched: the `SetReferredItem` convert hack
  (`TreeItem.cpp:1367-1381`) and `CommitDataChanges` -> `UpdateDictionary`. The read half
  of the mmd branch in `PrepareDataUsageImpl` (`:565-599`) goes in S3 and
  `IsDataReadable()` with it; the write-side open (`:548-564`) stays.
- **Write side.** `CommitDataChanges`, `DoWriteItem`, `StorageWriteHandle`,
  `m_DataItemsStatusInfo`, `IsStorable` unchanged in mechanism; the commit guard of 3.2
  decides who is written: an item with a configured rule, never one read from the storage.
- **Config-data items.** A data block, or an authentic array such as under
  `/Desktops/ViewData`, has no read calculator and keeps its data on the config item; its
  `union_data` key changes with an edit, so dependents re-key (3.2). A data block below a
  storage holder is written to that storage as today.
- **Templates and `for_each`.** Storage properties copy with `stg_cpy_mode`; the key is
  synthesized per instantiated item after its own `UpdateTree`.
- **`DisableStorage`, `StorageReadOnly`, write-only types.** Excluded from the attribute
  specs exactly as they are excluded from `IsLoadable` today.
- **`SyncMode`.** `UpdateTree` still runs at meta time and creates the synced sub-items;
  they are config items by the time the table key is synthesized, so `AllTables`/`Attr`
  attributes appear in the key like configured ones.
- **`shv/dll/src/Theme.cpp:481`** also uses `CreateItemWriter` (class breaks); it keeps
  working. Its move to an operator, which would let the `task_func_type` constructor and
  the `m_FuncDC`/`m_TaskFunc` branches go, is filed separately (section 5, decision 6).

### 3.9 What `PrepareDataRead` becomes

| In `PrepareDataRead` today | After |
|---|---|
| Calc-supplier visit -> futures (`FileName`, grid domain) | arguments produced by `DescribeReadCall` |
| `ExplicitSuppliers` -> futures (20.16.0) | `do(supplier, name)` inside the spec |
| `adu->PrepareDataUsageImpl` for the cardinality | the domain is the result, or a domain argument |
| `GetMetaInfo` + `PrepareReadDataOrSuspend` (values range wait) | values units are arguments |
| `CreateItemWriter` + `m_RequiredStorageManager` | `FuncDC` OC + `Operator::GetRequiredStorageManager` |
| `m_ReadAssets` parking of the OC, `TSF_ReadAssetsInterestScoped` | `FuncDC::m_OperContext` lifecycle; `m_ReadAssets` holds the member request instead |
| `m_KeptArgInterests/Items/Units` in `Schedule` | `FuncDC` `SupplInterest` + `OC_CalcResultFunc::allInterests` |
| domain/values DC futures | arguments |
| #1152 "no primary data" failure | operator failure |
| `ReadItem` -> `StorageReadHandle::Read` -> `DoReadItem`, estimate and report | `CalcResult`, `EstimatePerformance`, `RunOperator` |
| `SetMaxRange()` for units without storage data | stays in `PrepareDataUsageImpl` |

Deleted in S3 (S2 exit condition: every storage manager, `MMD` included, reports
`SupportsReadOperator()`): `PrepareDataRead` and the `IsDataReadable` branch of
`PrepareDataUsageImpl`; the read use of `CreateItemWriter`; `TreeItem::ReadItem` and the
`DoReadItem` virtuals (their bodies become operator helpers); `StorageReadHandle::Read`;
`StorageMetaInfo::PrepareReadDataOrSuspend`; `NonmappableStorageManager::StartInterest/StopInterest`;
the read-side `VisitSuppliers` overrides; `SupplierVisitFlag::CalcAndExplicitSuppliers`;
the #1209 exit in `GetCheckedDC` and the loadable arm of `GetCurrMetaInfo`; the
pre-charge sites listed in `interest-and-futures.md` §2.5(b) for `TreeItem.cpp:3750,3813,3825`.
`OperationContext(task_func_type)`, `m_TaskFunc` and the `m_KeptArg*` members stay until
the class-break writer follows (decision 6). `CreateStorageSpec` inside the `sourceDescr`
tree goes in S4 with the holder string; `sourceDescr` itself remains the key of meta
references (`subst_never` arguments, function meta parameters, `HofApplication.cpp:159`).

### 3.10 Source versions and external changes

The review asked how observable and non-observable external changes of source data fit
this design, when data gets a last-change timestamp, and how dependent timestamps can be
administered for `.MMD` files that are read in a later session. What holds today, from
`doc/incremental-updates.md` §1-2 and the code:

- `m_LastChangeTS` is an internal `TimeStamp` minted lazily on the meta thread and
  restarting at 1 every session (`rtc/dll/src/act/UpdateMark.h:56-65`); an actor's value is
  the maximum over its suppliers, recomputed by `DetermineState`.
- A stored item's `TreeItem::DetermineLastSupplierChange` (`TreeItem.cpp:2396-2436`)
  fetches the file's change time through `GetCachedChangeDateTime` and discards it; the
  fold into the timestamp has been commented out since the `DSM` it named was retired.
  So no source change is observed in a session, and nothing about a source is persisted.
- `GetCachedChangeDateTime` re-stats only when an internal timestamp advanced
  (`AbstrStorageManager.cpp:736-746`), so an idle session never re-polls.

With reads as operators keyed by the storage definition, the design has a natural slot
for a source version, and the timestamp question splits in three:

1. **Observable sources** (a file's date and size, a GDAL dataset's mtime, a dbf header):
   the observed version becomes an element of the spec, sampled from a session-level
   `{expanded storage name -> (file date time, sampled at)}` map that is refreshed once
   per activation epoch (`incremental-updates.md` §3.2), never from inside
   `DetermineState`. A changed source is then a *different key*: a new DC, a new read,
   the old result lingering until interest drops. Consumers see a supplier change through
   the ordinary `FuncDC` timestamping (`MarkCacheItems`), which is the answer to "when
   does the data get a last-change timestamp": at the epoch in which its source version
   was observed.
2. **Non-observable sources** (ODBC, web sources, shares without mtime semantics): an
   empty version by default, and an optional `SourceVersion` property on the holder
   (indirect allowed) so a modeller can declare a new extract; plus an action in the GUI
   and `GeoDmsRun` that bumps every non-observable source at once. Without such a
   declaration these sources behave as today: read once per session.
3. **Stores written by GeoDMS and read in another session (`.MMD`)**: the internal
   timestamps of the writing session are meaningless to the reader, so what has to be
   persisted with a stored result is the set of *source versions* its supplier closure
   depends on: for every read operator in that closure, its spec (name, type, sql string,
   layer) and the observed or declared version. The MMD dictionary
   (`rtc/dll/src/tic/stg/MemoryMappedDataStorageManager.cpp`, `0Dictionary.dms`) is the
   natural place, a sidecar the alternative. A reader merging the dictionary compares
   the recorded versions with the current ones and treats a differing source as making
   the dependent items stale; what stale means (refuse, warn, recalculate) is open. The
   recording side depends on this plan: after S4 the supplier closure carries exactly the
   spec strings to record, so the writer reconstructs nothing.

None of this is in scope for S1 to S4; two issue drafts are in
`scratch/issue587/issue_source_versions_DRAFT.md` and
`scratch/issue587/issue_mmd_dependent_versions_DRAFT.md`, not posted (Appendix B).

---

## 4. Stages

All stages are sequential: compile, run the tests of section 7, then the next. Nothing
ships separately; the next pre-release is at least a week away.

### S0. The filed symptom

`slSupplierExprImpl` (`AbstrCalculator.cpp:1015`): when `m_CalcRole == CalcRole::Checker && holder == supplier`
and `GetKeyExprImpl()` is empty, substitute `CreateLispTree(supplier, false)`. A check on
a stored item that refers to itself then evaluates `gt(strlen(sourceDescr(/x ...)), 0)`;
the `SymbDC` reads `x` once, shared with the guarded reference, and the recursion the
raw-key rule prevents cannot arise because a `sourceDescr` folds nothing. `probe587a`
passes; `probe587b` unchanged. S1 makes this fallback unreachable for migrated managers;
it stays for the unmigrated ones until S3, then goes. Tests:
`testcases/fn_test_icheck_storage_self.dms` (by name and as `this`) and `_neg1`.

*Status 2026-09-05: implemented in `AbstrCalculator.cpp` (`slSupplierExprImpl`), the two
testcases written, the wiki paragraph added to `IntegrityCheck.md` ("Since GeoDMS 20.20.0").
Not compiled and not run: the Release build started for it was aborted at the maintainer's
request because another session needed the machine. Next: build, run `probe587a`/`b` and
the battery, then commit engine and wiki.*

### S1. The operators, the read calculator, `str` and `gdal.vect`

1. `oper_policy::members_on_demand`; `DataController_SuppliesWholeResultTree` tests it.
2. `Operator::GetRequiredStorageManager`; resolution when the OC becomes runnable;
   `RunOperator`/`CalcResult` hand-over of the held section.
3. `AbstrStorageManager::SupportsReadOperator` / `DescribeReadCall`; the `DC_Ptr` read
   calculator with its marker; `HasConfiguredCalcRule()` and `IsReadFromStorage()`;
   installation after `UpdateTree` in `UpdateMetaInfoImpl2`; the `DoInvalidate` reset;
   the `HasCalculator` audit of 3.2 applied.
4. `rtc/dll/src/tic/stg/StorageReadOperators.{h,cpp}`: the shared base (skeleton,
   request collection, `CalcResult` loop reusing `DoReadItem`, estimate, failure
   naming), the generic `storage_read_table/attrs/attr/value` groups, Explain leaf
   handling.
5. `StrStorageManager` (`storage_read_value`) and `GdalVectSM` (`storage_read_table`,
   spec with sql string, layer, driver, options; `GDAL_*` items as arguments) report
   `SupportsReadOperator()`.
6. `StorageMetaInfo` constructor with cache target + config item + relative name;
   adjust the per-manager `CurrWD()` assumptions the battery trips over.
7. Stored items stop being editable (decision 7): no predicate change, `IsEditable()` and
   `AssertDataChangeRights` already refuse a non-data-block calculator; the refusal
   message names the storage; config-data items unchanged; GUI smoke: a palette read
   from a storage takes the copy-on-write route.

Exit: battery green including the new cases of section 7; Debug runs assert-free; a
`scratch/` probe with a 3-column CSV read through `gdal.vect` shows, in the `/SP` log,
one `[storage read]` line when one attribute is demanded and a second read when a second
attribute is demanded in the same run; `fn_test_icheck_storage*` pass through the
`UpdateDC` fold (verify with the Debug `IntegrityCheck(...)` trace, as #1218 did); a
writable dbf read through the new route is not written back (mtime unchanged); GUI smoke
on the detail page, the tree icons and F2 of a stored item.

*Status 2026-09-05: implemented, Debug battery 293/0 (the 286 cases plus the seven
`stor_read_*` cases of section 7), committed. Where the implementation departs from the
list above: (4) one file, `StorageReadOperators.cpp`, with the two groups S1 needs,
`storage_read_table` and `storage_read_value`; `storage_read_attrs`/`_attr` have their
tokens and their Explain leaf handling but no operator yet (S2); no `EstimatePerformance`
override (the default estimate; a read's size is known only after the table's range is
read, which is what the first pass does). (5) the spec carries the storage name, the
storage class name (`GetDynamicClass()->GetNameID()`), the sql string and the table name;
the `GDAL_*` option items are not arguments yet, they are still resolved by
`GdalMetaInfo` from the configured item (S4 with the spec-only meta info). (6) no new
`StorageMetaInfo` constructor: the meta info is still built for the configured item and
`SetDataTarget` redirects the read to the cache member, so `m_RelativeName` and the
manager-specific members (layer, sql string, field) stay as they were; the table's range is
read through `ReadUnitRange` directly, because `AbstrUnit::DoReadItem` asserts that storage
is not disabled and every result unit has it disabled. (7) `AssertDataChangeRights` refuses
an item with a read calculator explicitly ("an item read from a storage"), before the
config-data test. Not in the list: an attribute whose values unit is the table itself is
not a member of the table's read (its key would be the key under construction) and stays on
the item-writer path; `TreeItem_InstallStorageReadCalculator` gives a member that lost its
calculator through `DoInvalidate` the `subitem(...)` key back when the table still has its
read. Exit criteria: battery green, Debug assert-free, the selective probe is the
`stor_read_table_selective` case (`[storage read]` lines for `name` and `oppervlak` only,
`id` and `status` untouched; both members were collected in one pass because both checks
had interest before the read ran), `fn_test_icheck_storage*` fold as
`IntegrityCheck(storage_read_value(...), ...)`, a writable `gdal.vect` CSV read through the
new route keeps its mtime (`scratch/issue587/probe_writable.dms`; dbf is S2). Not done: the
GUI smoke.*

### S2. The remaining managers

`dbf`, `shp`, `xdb`, `odbc` through the generic table form (odbc supplies its sql string);
`FSS`, `cfs` and `MMD` through `storage_read_attr`/`storage_read_value` per item, `MMD`
with the file mapping as the member's data object and the dictionary's units keeping their
configured ranges (3.8); `strfiles` with
the `FileName` attribute as a data argument; `gdal.grid`, `tif`, `bmp`, `pal`, `xyz`
through `storage_read_attr` with the grid domain argument, the projected-point carrier
unit of 3.1, and the lazy tile functor. Exit: every storage manager, `MMD` included, reports
`SupportsReadOperator()`; `batch\TestShippedContent.bat` and the `tst` storage
regression configurations (user-run) green.

*Status 2026-09-05: implemented, Debug battery green (the S1 cases plus `stor_gdalgrid_u8_sf`,
`stor_fss_1_write`/`_2_read`, `stor_dbf_1_write`/`_2_read`, `stor_strfiles_read`), committed.
Every manager reports `SupportsReadOperator()`, `odbc` and `xyz` untested here (no data source,
no fixture); `gdalwrite.*` are write-only and never reached. Departures from the list above:
the grid domain is not read through a carrier unit form but stays the configured unit whose
range and projection `DoUpdateTree` sets from the file, and enters `storage_read_attr` as the
domain argument (`AbstrGridStorageManager::DescribeReadCall` describes data items only); the
`GDAL_*` option items are still resolved by `GdalMetaInfo` from the configured item (S4). MMD
attributes are mapped by the operator (`MapMember`), the units keep the dictionary's ranges;
the legacy MMD block of `PrepareDataUsageImpl` is skipped for an item with a read calculator
and asks `HasConfiguredCalcRule` for its write-through decision. `storage_read_attrs`
(several attributes over one foreign domain in one scan) is not implemented: every such
attribute reads on its own through `storage_read_attr`, which is also what the shapefile
geometry beside its `ShapeID` unit and an FSS attribute over a unit defined elsewhere do; the
dispatch (`NonmappableStorageManager::DescribeReadCall`) leaves an attribute to its table's
merge only when the table is read from the same storage holder and contains the attribute.
`StorageMetaInfo` grew the described-item / data-target split (`CurrRD` vs `CurrWD`,
`SetDataTarget`), and the managers that read names, relative paths or table keys from the
target were pointed at the described item (str, odbc, xyz, tif/gdal.grid palette test,
stream `ReadUnitRange` loads into the target). Found on the way: a request left in
`m_ReadAssets` by a failed read kept interest, and thereby keys and their string literals,
alive up to the token registry's teardown (`StrnObjCache.empty()` assertion at exit of the
`_neg` cases); the request is now released on every exit of `CalcResult`, under the storage
section so that the meta infos may close the storage. Not done: `TestShippedContent.bat` and
the `tst` storage regressions (S5 runs the unit suite and full.py).*

### S3. The cleanup

The deletions of 3.9; `IsDataReadable` deleted, with the read half of the mmd branch of
`PrepareDataUsageImpl` (`:565-599`; the write-side open at `:548-564` stays); the loadable comments in
`TreeItemMetaInfo.cpp` removed; `OperationContext.cpp` `m_FuncDC` branch count measured
before and after (the class-break writer keeps some). Docs:
`schedule-with-lookahead.md` §2.6 and §3 gap 4, `interest-and-futures.md` §2.5(b),
`doc/IntegrityCheck.md` (#1209 section superseded), `tile-data-retainment.md`
creation-channel table.

### S4. One scan per table, identity by value

`ReadDataItems` with single-pass implementations for `gdal.vect` (enable all requested
fields, one `GetNextFeature` loop per tile filling N tiles), `dbf`, `odbc`; the
spec-only `StorageMetaInfo` constructors; a read-only manager instance per DC; the holder
string removed from the spec and `CreateStorageSpec` from the `sourceDescr` tree; measure
on a real geopackage and a large CSV: N attributes cost one scan instead of N; two holders
reading the same table share one read. The work is complete when the holder is gone.

---

## 5. Decisions

| # | Question | Ruling (2026-09-05 review) |
|---|---|---|
| 1 | Attach the read DC without a calculator, or install a `DC_Ptr` calculator? | `DC_Ptr`, agreed after the elaboration in 3.2; the `HasCalculator` call sites of the audit are updated so that data read from a storage is never written back, and the read DC gets the IntegrityCheck wrapper folded around it by `UpdateDC` like any calculated item |
| 2 | Operator names | lower case: `storage_read_table`, `storage_read_attrs`, `storage_read_attr`, `storage_read_value` |
| 3 | A holder reference in the key during S1 to S3 | accepted as a temporary aid only if it makes the earlier stages better testable; it goes in S4, as a string element of the spec, never as a `sourceDescr` argument (3.1) |
| 4 | `ExplicitSuppliers` | `do(supplier, storageName)` wrapping in the spec, nested for several suppliers; no trailing arguments, no `AddDependency` |
| 5 | Ship S0 ahead of S1 | no; all stages one by one, compile, test, next; nothing before the next pre-release |
| 6 | Class-break item writer to an operator, then simplify `OperationContext` statuses | separate issue, not for this week: ObjectVision/GeoDMS#1248 |
| 7 | Editing values of a stored item in a writable storage (3.2, edit path) | dropped: an item read from a storage is not editable. Items with config data stay editable, a data block or an authentic array such as under `/Desktops/ViewData`: an edit mints a new epoch, the data-block calculator's `union_data` key changes with the values, and every dependent gets a new key and DC |
| 8 | The four-element spec domain | ruled: an existing default unit, no new unit. The review said `uint4`; in this code base `uint4` is `bit_value<4>` with sixteen elements and `uint2` the four-element one (`RtcBase.h:80-81`, `TiledRangeData.h:193-202`), so the spec domain is `uint2` |
| 9 | Attribute spec encoding | ruled: relative path plus values unit key; the composition suffix only for a non-default composition (`:poly`, `:arc`, `:multipoint`), never for `Single` |
| 10 | Reading from `.MMD` | the maintainer's proposal, adopted: the same route as `FSS` and `cfs` in S2, the member's data object being the file mapping so a `DataReadLock` still maps instead of reads (3.8); `IsDataReadable` then disappears entirely in S3 |
| 11 | Tables with multi-level attributes (`pand { oppervlak; meta { status } }`) | the maintainer's proposal, adopted: the spec names the relative path `'meta/status'`, the skeleton mirrors the path, the cache-root merge installs the attribute's calculator through the nested container (3.1, 3.2) |
| 12 | Source versions and MMD dependent versions (3.10) | out of scope for S1 to S4; two issue drafts ready in `scratch/issue587/`, not posted |

---

## 6. Risks

- **Data moves to cache items.** Consumers reach it through `GetCurrUltimateItem()`, as
  for calculated items, but code that assumes a stored config item owns its
  `m_DataObject` needs an audit: `HasConfigData`, `ClearDataObject`, `PublishMeasuredElementWidth(FocusItem())`,
  `qtgui/exe/src/DmsExport.cpp:884`, the `[storage read]` message's item reference, and
  the edit path of 3.2.
- **Read-write storages.** `IsStorable()` and `IsReadFromStorage()` are both true for a
  dbf without `StorageReadOnly`; the `CommitDataChanges` guard is what prevents a
  write-back. Pinned by a test that reads a writable dbf and checks the file's mtime.
- **`HasCalculator()` semantics change.** A stored item now has a calculator; every site
  in the audit table that is missed writes, syncs or displays wrongly. The audit is a
  checklist for S1, and the Debug battery plus the GUI smoke are the net under it.
- **Meta-time calculation of arguments.** A `calc_always` argument is calculated whenever
  the result skeleton is made; a spec with `do(...)` inside it would run a side-effect
  supplier on tree browsing. The spec is `calc_as_result`; a test with an `exec_ec`
  supplier and a tree-level update (no data demand) pins that nothing runs.
- **Worker-side failures.** Today `OpenForRead` fails the holder at MetaInfo level from
  the read thread; the new route fails the read DC at Data level. Every member of the
  table shares the DC, so fail-fast per table is preserved; other tables of the same
  storage keep trying, which is a behaviour change worth a release note.
- **Fences.** Reads inside a PhaseContainer become `FuncDC` OCs with the item's phase;
  verify with the fence configurations the #1167/#1199 work used before trusting it.
- **GDAL concurrency in S4.** Per-DC reader instances open the same file several times;
  GDAL allows independent handles, `GDALAllRegister` is already serialized (#1234), and
  each instance keeps its own critical section, but this is the step to measure, not to
  assume.
- **Key length and DC count.** One table DC, one `subitem` DC per attribute, string DCs
  shared by value; the same shape as `for_each` and `unique` results. Debug builds carry
  `md_sKeyExpr` strings per DC; watch memory in the Debug battery, not in Release.
- **Ordering of key synthesis.** The table key enumerates sub-items, so it must be built
  after the manager's `UpdateTree` of that table; `MakeCalculator` runs too early for it.
  The `UpdateMetaInfoImpl2` placement is load-bearing.
- **MMD mappings on cache items.** A mapped file as a cache member's data object lives as
  long as the member keeps its data; `TryCleanupMem` on the result root closes the mapping
  when interest drops, as `ClearDataObject` does for the config item today, and the write
  path keeps its own mapping through the `SetReferredItem` convert. `stor_mmd_group_2_read`
  must pass unchanged, and a probe that reads one item of a store while another is written
  guards the two paths against each other.
- **Unification by value.** Two configurations that read the same table now share a
  domain. Configurations that relied on two reads of one table being *different* domains
  (unlikely, but possible with `combine` or `union_unit` over them) change meaning; a
  release note item.

---

## 7. Verification and documentation

Offline battery (`testcases/`, cheap, no network):

- `fn_test_icheck_storage_self.dms` + `_neg1`: probe A (S0).
- `stor_read_table_all.dms`: `gdal.vect` over a 5-row, 3-column `read_table.csv`
  fixture; all three attributes checked against literals.
- `stor_read_table_selective.dms`: two checks, one per attribute, evaluated in one run
  (re-entry); the selective-read *measurement* is a `scratch/` probe on the `/SP` log,
  reported in the issue debrief, since the battery classifies by exit code only.
- `stor_read_domain_only.dms`: `parameter<uint32> n := #T`, no attribute read.
- `stor_read_nested_attr.dms`: `pand { oppervlak; meta { status } }` over the CSV fixture
  (a column named `status`), the `'meta/status'` spec of decision 11.
- `stor_read_composition.dms`: a `gdal.vect` layer with a polygon geometry, the `:poly`
  suffix of decision 9.
- `stor_read_icheck_holder.dms`: a check on the table unit referring to its own attribute,
  and the #587 shape on a `gdal.vect` attribute.
- `stor_read_phase_member.dms`: a PhaseContainer over a container with a stored member
  that a consumer demands (fails today with "Source ... has no calculation rule").
- `stor_read_rw_no_writeback.dms`: the writable-dbf guard.
- `stor_read_explicit_supplier.dms`: an `ExplicitSuppliers` producer and a tree-level
  update that must not run it (the `calc_as_result` pin).
- `stor_read_shared_table.dms` (S4): two holders reading one table unify.
- `stor_strfiles_filename_arg.dms` (S2).
- Existing: `fn_test_icheck_storage*`, `stor_shp_ringclose_2_read`, `stor_mmd_*`,
  `fn_test_opsigK13_stor`, `fn_test_fe_stor_neg` unchanged.

Suites run by the user: `batch\TestDebugUnit.bat` / `TestReleaseUnit.bat` after each
stage; `batch\TestShippedContent.bat` (real `gdal.vect` data) and the `tst` storage and
fence configurations after S2 and S3.

Wiki (`C:\dev\GeoDMS.wiki`, pull first): `IntegrityCheck.md` (a check on a stored item
may refer to that item), `StorageManager.md` and `SyncMode.md` (reading is scheduled as a
calculation; only requested attributes are read; what the detail page and the
`HasCalculator` property now show; two configurations reading one table share it),
`Str-StorageManager.md` (S2, `strfiles` argument), a page for the `storage_read_*`
operators only if they become typeable. Issue #587: the debrief per
`.claude/skills/geodms-issues`, with the probe output above as the "before".

---

## Appendix A. Code anchors (as of `b81d6eea`)

| What | Where |
|---|---|
| Checker takes the raw key; SubstitutionError; circularity refusal | `rtc/dll/src/tic/AbstrCalculator.cpp:1015-1022`, `:999` |
| `GetLispRefForTreeItem` -> `slSubItemCall`; `sourceDescr` branch of `SubstituteExpr_impl` | `AbstrCalculator.cpp:129-143`, `:1239` |
| `GetCurrMetaInfo` loadable arm; `GetBaseKeyExpr` empty for index 0 | `rtc/dll/src/tic/TreeItemMetaInfo.cpp:211-278` |
| `UpdateDC`, `GetCheckedDC` #1209 exit, `GetCheckedKeyExpr` fallback | `TreeItemMetaInfo.cpp:565-712` |
| `UpdateMetaInfoImpl` (ApplyCalculator `:150`, commented UpdateDC `:152`), `UpdateMetaInfoImpl2` (UpdateTree `:821`) | `TreeItemMetaInfo.cpp:92-194`, `:747-837` |
| `DoUpdate` validate phase, `TreeItem_ValidateIntegrity` | `TreeItemMetaInfo.cpp:952-1151` |
| `sourceDescr` tree, `read`/`readSql` storage spec, `slSubItemCall`, #1161 case folding | `rtc/dll/src/tic/LispTreeType.cpp:33-38`, `:220-259`, `:261-265`, `:356-375` |
| `CreateDC` sourceDescr -> SymbDC; DC registry | `rtc/dll/src/tic/DataController.cpp:356-470` |
| `SymbDC::MakeResult`, `SymbDC::CallCalcResult` -> `PrepareDataUsage` | `rtc/dll/src/tic/MoreDataControllers.cpp:1191-1288` |
| `FuncDC::CallCalcResult` restart test, `CallCalcResultImpl` (PreCalcUpdate), `GetArgs`, `MustCalcArg`, OC reset, `FuncDC_CreateResult`, `MarkCacheItems` | `MoreDataControllers.cpp:344-452`, `:925-1015`, `:676-734`, `:454-470`, `:164-186`, `:753-840`, `:736` |
| values/shape split (`DataController_SuppliesWholeResultTree`), `FuncDC::VisitSuppliers` | `MoreDataControllers.cpp:1046-1116` |
| `PrepareDataUsageImpl`, `PrepareDataCalc`, `PrepareDataRead`, `ReadItem`, `CommitDataChanges` | `rtc/dll/src/tic/TreeItemDataUsage.cpp:85-135`, `:211-281`, `:283-476`, `:478-728`, `:844-919` |
| `IsDataReadable`, `GetStorageParent`/`IsLoadable`, `HasCalculatorImpl`/`HasCalculator`, `GetCalculator`/`ApplyCalculator`/`MakeCalculator`, `SetDC`, `SetReferredItem` (+ mmd hack), `Copy`, `DoInvalidate`, interest and `m_ReadAssets` release, `DetermineLastSupplierChange`, `HasStorageManager`, `IsEditable`, `GetExpr`, `VisitSuppliers` (DetermineCalc) | `rtc/dll/src/tic/TreeItem.cpp:1743`, `:1542-1600`, `:939-970`, `:1062-1130`, `:851-910`, `:1328-1381`, `:1762-1960`, `:2326-2366`, `:2596-2680`, `:2396-2436`, `:2495`, `:132-140`, `:819-824`, `:2114-2245` |
| `CreateItemWriter`, `releaseStorageLockIfHeld`, `Schedule` kept-arg block, storage gate, `RefreshEstimateForAdmission`, `connectArgs`, `ScheduleCalcResult`, `Run_with_catch` | `rtc/dll/src/tic/OperationContext.cpp:853-870`, `:1001-1072`, `:1262-1316`, `:1901-1925`, `:2259-2306`, `:2426-2543`, `:2614-2664` |
| `m_RequiredStorageManager`, `m_KeptArg*` | `rtc/dll/src/tic/OperationContext.h:281-283`, `:374-393` |
| `StorageMetaInfo`, `GdalMetaInfo`, `AbstrStorageManager` (VisitSuppliers `:277`, ReadDataItem `:280`, ReadUnitRange `:283`, DoUpdateTree `:293`), `NonmappableStorageManager` (ReaderClone `:347`, GetMetaInfo `:350`, StartInterest `:352`), handles | `rtc/dll/src/tic/stg/AbstrStorageManager.h:129-178`, `:214-335`, `:337-370`, `:377-433` |
| `PrepareReadDataOrSuspend`, `DoUpdateTree`, `ReaderClone`, `VisitSuppliers`, interest holders, `OpenForRead`, `GdalMetaInfo` ctor, `StorageReadHandle::Read` | `rtc/dll/src/tic/stg/AbstrStorageManager.cpp:71-80`, `:765-776`, `:786-795`, `:823-855`, `:860-885`, `:887-908`, `:959-984`, `:1046-1075` |
| `AbstrDataItem::DoReadItem` (lazy tiles `:355`, serial `:390`), `AbstrUnit::DoReadItem` | `rtc/dll/src/tic/AbstrDataItem.cpp:327-420`, `rtc/dll/src/tic/AbstrUnit.cpp:803-812` |
| `Unit<V>::GetKeyExprImpl` | `rtc/dll/src/tic/Unit.cpp:86-175` |
| readiness predicates, `GetOperationContext` | `rtc/dll/src/tic/ItemLocks.cpp:600-724`, `:956-972` |
| `CalcAndExplicitSuppliers` | `rtc/dll/src/act/SupplierVisitFlag.h:31-34` |
| Explain stops at sourceDescr; SourceDescr's storage test | `rtc/dll/src/tic/Explain.cpp:613`, `rtc/dll/src/tic/SourceDescr.cpp:114` |
| `gdal.vect` meta info, layer open, per-field read, cursor, `DoUpdateTree` | `stg/dll/src/gdal/gdal_vect.cpp:252-300`, `:307-360`, `:1823-1960`, `:3028-3084` |
| ODBC meta info, recordset per table holder, read, range | `stg/dll/src/odbc/OdbcStorageManager.cpp:347-360`, `:681-690`, `:736-773` |
| `strfiles` FileName supplier | `stg/dll/src/str/StrStorageManager.cpp:166-240` |
| grid suppliers and meta info | `stg/dll/src/GridStorageManager.h:91-114`, `stg/dll/src/GridStorageManager.cpp:113-193` |
| PhaseContainer, SubItem, parse_xml, do/OperExpand, gridset, union_data count rule | `clc/dll/src/PhaseContainer.cpp:32-315`, `clc/dll/src/OperMisc.cpp:249-289`, `clc/dll/src/BoostXML.cpp:294-442`, `clc/dll/src/OperExec.cpp:312-408`, `clc/dll/src/OperUnit.cpp:551-600`, `clc/dll/src/Union.cpp:290-296` |
| other `CreateItemWriter` user | `shv/dll/src/Theme.cpp:481` |
| timestamp API, MMD dictionary | `rtc/dll/src/act/Actor.h:135-172`, `rtc/dll/src/act/UpdateMark.h:56-65`, `rtc/dll/src/tic/stg/MemoryMappedDataStorageManager.cpp:196-320` |
| probes and issue drafts | `scratch/issue587/probe587a.dms`, `probe587b.dms`, `payload.txt`, `issue_*.md` |

## Appendix B. Follow-ups outside this plan

- Class-break item writer as an operator, then `OperationContext` status simplification:
  filed as ObjectVision/GeoDMS#1248 (body in `scratch/issue587/issue_classbreak_operator.md`).
- Source versions for storage reads (observable and non-observable external changes):
  draft in `scratch/issue587/issue_source_versions_DRAFT.md`, not posted.
- Dependent source versions persisted with MMD stores: draft in
  `scratch/issue587/issue_mmd_dependent_versions_DRAFT.md`, not posted.
