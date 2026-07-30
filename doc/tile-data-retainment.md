# Tile data retainment: the AbstrDataObject hierarchy

Inventory of all (template) classes deriving from `AbstrDataObject` and analysis of whether each
one **retains** lazily calculated (future) tiles, or **recalculates** them after all consumers
released a tile and a later consumer — one that held supplier interest on the owning
`AbstrDataItem` all along — requests read access to that tile again.

Based on code review of the GeoDMS26 tree (rtc/tic, clc, geo) on branch `lookahead-scheduling`,
2026-07-30.

This is the per-class inventory behind the three **materialization regimes**
(`rtc/dll/src/tic/TicBase.h:56`) that the scheduler's cost model reasons about; §4.7 maps the
classes onto them. `doc/development/schedule-with-lookahead.md` §4.4 is the authority on what each
regime *costs* (it has the measurements); this document is the authority on which class you get and
whether it keeps its tiles.

---

## 1. Preliminaries: what "holding a tile" means

- A readable tile is handed out as `locked_cseq_t = locked_seq<cseq_t, TileCRef>`
  (`rtc/dll/src/tic/DataArray.h:50`, `TileLock.h:36-56`): a raw view (begin/end) plus an owner
  handle `TileCRef = std::shared_ptr<const void>` (`rtc/dll/src/RtcBase.h:315`). The tile's
  backing buffer lives exactly as long as *some* `shared_ptr` to it lives. **Retention therefore
  reduces to one question: does the data object itself co-own the buffer, or do only the
  consumers' locks own it?**
- Per-tile futures: `abstr_future_tile` (`AbstrDataObject.h:55-58`) with typed refinement
  `DataArrayBase<V>::future_tile` (`DataArray.h:119-126`); obtained via
  `GetFutureTile(t)` / `GetFutureAbstrTile(t)` and consumed by calling `GetTile()` on the future.
  `GetFutureTileArray` (`FutureTileArray.h:18-27`) collects one future per tile so pipelined
  consumers can request/release tiles individually.
- Item level: the calculated result is `AbstrDataItem::m_DataObject`. When the last
  `DataReadLockAtom` is released and the item is not `PartOfInterest`, `TryCleanupMem` runs
  (`DataLocks.cpp:154-155`) and — unless the item is kept (`PartOfInterestOrKeep`,
  `GetKeepDataState()`), has no source to recompute from, or is a memory object ≤
  `KEEPMEM_MAX_NR_BYTES` = 128 bytes (`FreeDataManager.h:44`, `AbstrDataItem.cpp:852-896`) —
  drops the whole data object via `DropValue`/`ClearDataObject` (`AbstrDataItem.cpp:216-228`).

The scenario in scope assumes consumer B held (supplier) interest on the `AbstrDataItem`
throughout. That interest keeps `m_DataObject` installed (it blocks the `TryCleanupMem` path
above), so the question becomes strictly a *per-tile* property of the concrete
`AbstrDataObject` subclass. Interest by itself pins the **object**, never a lazy **tile**.

---

## 2. Inheritance tree

`DataArray<V>` is an alias for `TileFunctor<V>` (`TicBase.h:82`). `TileFunctor<V>` derives from
`data_array_traits<V>::type` (`DataArray.h:262-291`), which selects the abstract mid-layer per
value type: `AdditiveArray<V>` for numerics/bit values, `PointArrayAdapter<...>` for points,
`SeqArrayAdapter<DataArrayBase<...>>` for polygons/sequences, plain `DataArrayBase<V>`
otherwise (e.g. `SharedStr`).

```text
SharedObj
└─ AbstrDataObject                                   rtc/dll/src/tic/AbstrDataObject.h:60
   ├─ TileFunctor<Void>   (degenerate specialization) DataArray.h:321
   └─ DataArrayBase<V>    [abstract: GetTile, GetFutureTile pure]  DataArray.h:40
      ├─ NumericArray<V>                              DataArray.h:173
      │  └─ AdditiveArray<V>                          DataArray.h:219
      ├─ GeoArrayAdapter<Base>                        DataArray.h:230
      │  ├─ PointArrayAdapter<Base>                   DataArray.h:241
      │  └─ SeqArrayAdapter<Base>                     DataArray.h:253
      └─ TileFunctor<V> : data_array_traits<V>::type  DataArray.h:294   (= DataArray<V>)
         ├─ GeneratedTileFunctor<V>  [future = call-through]  DataArray.h:327
         │  ├─ HeapTileArray<V>                       TileArrayImpl.h:23    RETAINS
         │  ├─ HeapSingleArray<V>                     TileArrayImpl.h:40    RETAINS
         │  ├─ HeapSingleValue<V>                     TileArrayImpl.h:64    RETAINS
         │  ├─ FileTileArray<V>                       TileArrayImpl.h:82    RETAINS (on disk)
         │  ├─ LazyTileFunctor<V, ApplyFunc>          TileFunctorImpl.h:163 RECALCULATES
         │  └─ ConstTileFunctor<V>                    clc/dll/include/ConstOper.h:20  RECALCULATES
         └─ DelayedTileFunctor<V>  [owns future_tile records]  TileFunctorImpl.h:24
            └─ FutureTileFunctor<V,PrepareState,MustZero,PrepareFunc,ApplyFunc>
                                                      TileFunctorImpl.h:75  RETAINS once computed
```

Side hierarchy of the future-tile objects themselves:

```text
abstr_future_tile                                     AbstrDataObject.h:55
└─ DataArrayBase<V>::future_tile                      DataArray.h:119
   ├─ GeneratedTileFunctor<V>::future_caller          DataArray.h:332
   │     holds SharedPtr<const TileFunctor<V>> — pins the data OBJECT, not any tile data;
   │     GetTile() just forwards to self->GetTile(t) at demand time.
   └─ FutureTileFunctor<...>::tile_record             TileFunctorImpl.h:89
         std::variant<tile_spec, tile_data> under a mutex — flips from spec to data on first
         GetTile() and then RETAINS the data for the record's lifetime.
```

No other derivations exist in the tree: `geo`, `stg`, `shv`, `stx`, `sym`, `tools`, `qtgui`
contain no classes deriving from `AbstrDataObject` or any of the classes above (checked
2026-07-30); `geo/dll/src/Point.cpp` and the `clc` operators only *instantiate* the rtc/clc
classes. `tile_write_channel` (`TileChannel.h:200`) is a writer helper over
`TileFunctor<T>::GetWritableTile`, not a data object.

---

## 3. Per-class analysis

### 3.1 HeapTileArray, HeapSingleArray, HeapSingleValue — retain (in memory)

- `HeapTileArray<V>::m_Seqs` is an owned array of `std::shared_ptr<tile<V>>`
  (`TileArrayImpl.h:28-36`). `GetTile`/`GetWritableTile` allocate a tile on first touch
  (`InitTile`, `TileArrayImpl.h:126-136`) and return a lock sharing that `shared_ptr`
  (`TileArrayImpl.h:151-160`). The object keeps its own reference, so releasing every consumer
  lock never frees a tile; a re-request returns the same buffer. (Allocation is deferred, but
  *calculation* is not lazy — writers fill tiles via `GetWritableTile`.) Footnote: reading a
  never-written tile silently materializes a zero-initialized buffer.
- `HeapSingleArray<V>::m_Seq` and `HeapSingleValue<V>::m_Value` are direct members
  (`TileArrayImpl.h:54`, `77`); their locks pin the whole data object
  (`std::make_shared<SharedPtr<const AbstrDataObject>>(this)`, `TileArrayImpl.h:196`). Trivially
  retaining.

### 3.2 FileTileArray — retains (on disk), remaps on demand

- Each `file_tile<V>` (`mem/tiledata.h:34-58`) owns mappable sequences over a cache file created
  or opened in the ctor (`TileArrayImpl.h:326-426`). The tile lock handed to consumers is a
  `shared_ptr<mapped_file_tile<V>>`; `file_tile` itself keeps only a `std::weak_ptr` to it
  (`m_OpenFile`, `tiledata.h:56`).
- When the last consumer releases, `~mapped_file_tile` unmaps the view (`tiledata.h:75-90`); the
  data stays in the file. A later request re-locks/re-maps (`file_tile::get`,
  `tiledata.h:43-53`) — page-in I/O, **no recomputation**. The mapping also pins the whole
  `FileTileArray` (`mapped_file_tile::m_FileTileArray`, `tiledata.h:62`).
- Whether the file outlives the data object depends on `isTmp`: results of cache items that
  `MustStorePersistent` keep their file, others use temp files (`CreateFileData`,
  `DataLocks.cpp:248-259`).

### 3.3 DelayedTileFunctor + FutureTileFunctor — compute on first demand, then retain

- `DelayedTileFunctor<V>` owns `m_ActiveTiles`: one `std::shared_ptr<future_tile>` per tile
  (`TileFunctorImpl.h:29-30`), handed out by `GetFutureTile` (`TileFunctorImpl.h:49-52`); it
  never resets these pointers, so the records live as long as the functor. It also carries the
  non-owning `m_ResultAdi` back-ref used only for `FailType::Data` propagation
  (`TileFunctorImpl.h:31-34`, cleared by `ImLosingIt`).
- `FutureTileFunctor` fills every slot eagerly in its ctor with a `tile_record` holding
  `variant<tile_spec, tile_data>` (`TileFunctorImpl.h:89-126`; requires > 1 tile). The first
  `GetTile()` on a record — under its mutex, so concurrent requesters compute once — runs the
  apply functor and flips the variant to `tile_data` (`TileFunctorImpl.h:97-110`). The variant
  never flips back: **once computed, the tile stays in memory until the whole data object is
  released** (i.e. until interest on the item ends). After full consumption it is memory-wise
  equivalent to a `HeapTileArray`.
- Supplier coupling: the `tile_spec` carries the prepare state — typically
  `shared_ptr<future_tile>` handles on the argument tiles (e.g. `OperAttrUni.h:148-156`).
  Flipping the variant destroys the spec, so argument futures are released exactly when the
  result tile materializes.
- A consumer holding a `tile_record` future keeps that record (and its data, once computed)
  alive even if the data object dies — the lock returned by `GetTile()` holds
  `shared_from_this()` (`TileFunctorImpl.h:109`).

### 3.4 LazyTileFunctor — recalculates

- Per tile only a mutex plus `std::weak_ptr<tile_data> m_TileFutureWPtr` — commented
  "don't keep it !" (`TileFunctorImpl.h:170-174`).
- `GetTile(t)` (`TileFunctorImpl.h:226-256`): under the tile mutex, try `lock()`; if expired,
  allocate a fresh buffer, store a weak ref, run `m_ApplyFunc` (which writes through
  `GetWritableTile`, valid only during the apply; `TileFunctorImpl.h:212-223`), and return a
  lock in which **only the consumers** own the buffer.
- Consequently: concurrent consumers share one computation, but when the last `TileCRef` for a
  tile is dropped the data is freed immediately, and the next request **re-runs the apply
  functor** — regardless of any interest held on the item. For operator-generated lazy functors
  the apply re-pulls the argument future tiles kept in the closure
  (`make_unique_FutureTileFunctor` lazy branch, `TileFunctorImpl.h:135-148`), so recomputation
  cascades into suppliers that are themselves lazy.

### 3.5 ConstTileFunctor — recalculates (trivially)

- One `std::weak_ptr<tile<V>> m_ActiveTile` shared by *all* tile ids: a buffer of
  `m_MaxTileSize` filled with the constant, sub-ranged per tile (`ConstOper.h:36-61`). Dropped
  when the last consumer releases; refilled on the next request. Recomputation is a
  `fast_fill`, but it does allocate a full tile each time the value is re-requested from cold.

### 3.6 GeneratedTileFunctor (mid-layer) — delegating

- Contributes only the future implementation: `future_caller` stores `(t, SharedPtr self)` and
  forwards `GetTile()` to `self->GetTile(t)` (`DataArray.h:332-348`). Holding such a future
  pins the data object but caches nothing; retention is entirely the leaf class's `GetTile`
  behavior (retaining for the heap/file arrays, recalculating for Lazy/Const).

### 3.7 Abstract mid-layer and TileFunctor&lt;Void&gt;

- `DataArrayBase<V>`, `NumericArray<V>`, `AdditiveArray<V>`, `GeoArrayAdapter`,
  `PointArrayAdapter`, `SeqArrayAdapter` hold no tile storage; they implement typed element
  access, check modes, and (de)serialization on top of the pure virtuals `GetTile` /
  `GetFutureTile`. `TileFunctor<Void>` (`DataArray.h:321`) is a stub for the never-instantiated
  Void case.

---

## 4. Where lazy and future tile functors are created

### 4.1 The single fork: `make_unique_FutureTileFunctor`

Exactly two factories build the deferred functors, both in `rtc/dll/src/tic/TileFunctorImpl.h`,
and one calls the other.

`make_unique_FutureTileFunctor<V, PrepareState, MustZero>(resultAdi, lazy, trd, valueRangePtr,
pFunc, aFunc)` (`TileFunctorImpl.h:131-157`) **is** the retain-vs-recalculate fork:

- `lazy == false` → `FutureTileFunctor<V, PrepareState, MustZero, PrepareFunc, ApplyFunc>`
  (retaining, `:151-156`). Its ctor asserts `trd->GetNrTiles() > 1` (`:123`) and eagerly builds
  one `tile_record` per tile, each seeded with `pFunc(t)`.
- `lazy == true` → it *still* evaluates `pFunc(t)` for every tile up front into an
  `OwningPtrReservedArray<PrepareState>`, captures that array in a `lazyApplyFunc` that writes
  through `GetWritableTile(t, MustZero ? write_only_mustzero : write_only_all)`, and delegates to
  `make_unique_LazyTileFunctor` (recalculating, `:135-148`).

Two consequences worth noting. First, **the lazy branch is lazy in computation only, not in
preparation**: both branches materialize one `PrepareState` per tile at construction and hold it
for the object's lifetime. Since a `PrepareState` is normally a `shared_ptr<future_tile>` on each
argument (§4.2), both branches pin one argument future per tile from the start; only the
*retained result data* differs. Second, all call sites pass `MustZero == false`.

`make_unique_LazyTileFunctor<V>(resultAdi, trd, valueRangePtr, aFunc)` (`:199-203`) is also
called **directly**, bypassing the fork and the `lazy` flag entirely, by the unconditionally lazy
channels of §4.3. It has no `tn > 1` precondition — which is why the `tn > 1` conjunct present in
nearly every gate below is not a heuristic but `FutureTileFunctor`'s precondition, and why the
sites that omit it are exactly the sites that call `make_unique_LazyTileFunctor` directly.

### 4.2 The MT3-gated operator channel

Every gated site has the same shape inside `Operator::CreateResult(resultHolder, args, mustCalc)`:

```cpp
if (<MT3 gate>)
    res->m_DataObject = CreateFutureTile…(make_shared_tree(res, existing_obj{}),
                                          res->GetLazyCalculatedState(), …);   // pipelined
else {
    DataWriteLock resLock(res);
    parallel_tileloop(tn, [&](tile_id t){ this->Calculate(resLock.get(), …, t); });
    resLock.Commit();                                                          // eager heap/file
}
```

The gate sits in the *abstract* operator while the typed factory call sits in a virtual override,
so the channel carries a different hook name per operator family. All gates below share the base
`IsMultiThreaded3() && tn > 1 && !IsInMMD(res)`; the table lists what each one adds.

| Operator family | Gate site + extra conjuncts | Hook | Factory call |
|---|---|---|---|
| Unary attr (`AbstrUnaryAttrOperator`) | `OperAttrUni.h:74` — base only | `CreateFutureTileFunctor` | `OperAttrUni.h:142` |
| Binary attr | `OperAttrBin.h:81` — base only | `CreateFutureTileFunctor` | `OperAttrBin.h:155` |
| Ternary attr | `OperAttrTer.h:94` — base only | `CreateFutureTileFunctor` | `OperAttrTer.h:167` |
| `point(x,y)` / coordinate convert | `geo/dll/src/Point.cpp:119` — base only | non-virtual member `CreateFutureTileFunctor` | `Point.cpp:150` |
| `lookup(E→T, T→V)` | `lookupImpl.h:120` — **and** `tn > arg2Domain->GetNrTiles()` | `CreateFutureTileFunctor` | `lookupImpl.h:256` |
| `rlookup` / indexed search | `RLookupImpl.h:107` — **and** `nrTiles > arg2DomainRange->GetNrTiles()` | `CreateFutureTileIndexer` | `RLookupImpl.h:224` |
| Casted-unary attr (`convert`, `value`, special funcs) | `CastedUnaryAttrOper.h:69` — **and** `!res->GetKeepDataState()` | `CreateFutureTileCaster` | `OperConv.h:689` (transform), `OperConv.h:742` (convert), `CastedUnaryAttrOper.h:278` (special func) |

The two remaining extras are each defensible: the lookup families refuse to pipeline when the
result is not more finely tiled than the key domain (the index/values array is built once and
shared, so pipelining buys nothing), and the casted-unary family declines when the item is
`KeepData` (a retained result would be recomputed-then-kept anyway). Only the casted-unary family
consults `GetKeepDataState()` — which matters for §4.7, because the cost model's predicate assumes
they all do.

Historical note: every one of these gates used to carry a fourth conjunct,
`LTF_ElementWeight(args) <= LTF_ElementWeight(res)`. That function returned 0 unconditionally, so
the test was always `0 <= 0`; it and the conjuncts were deleted on this branch. Its successor,
`EstimateDataBytes` (`AbstrDataItem.h:210`, `AbstrDataItem.cpp:1305`), is a real per-row volume
estimate, but it feeds the *estimator* (`Operator.cpp:176-213`), not these gates — so no gate
weighs streaming the arguments against storing the result today. `ElementWeight`
(`AbstrDataItem.cpp:1288`) survives as the now-unreferenced original heuristic.

**What `prepare_data` carries.** By convention the `PrepareState` is the argument's future
tile(s), so the result functor holds a per-tile handle on each supplier and pulls it inside the
apply:

- one argument: `std::shared_ptr<Arg1Type::future_tile>` (`OperAttrUni.h:148-156`,
  `OperConv.h:688-697`, `CastedUnaryAttrOper.h:276-287`)
- two arguments: a `std::pair` of futures, with the first fetched through `throttled_async` so
  the two supplier tiles overlap (`Point.cpp:149-160`)
- `rlookup`: the argument future plus a `std::shared_ptr<std::any>` holding the *one* index built
  before the fork and shared by every tile's apply (`RLookupImpl.h:214-233`)

Because flipping a `tile_record` to `tile_data` destroys the `tile_spec`, the argument futures are
released exactly when the result tile materializes — that is the pipeline: supplier tiles stay
reachable only until each consumer tile is computed.

### 4.3 Unconditionally lazy channels (bypass the fork and the `lazy` flag)

Four sites call `make_unique_LazyTileFunctor` directly, so they **always** recalculate — the
`LazyCalculated` property is never consulted:

| Site | Gate | Notes |
|---|---|---|
| `AbstrMappingOperator` (`CastedUnaryAttrOper.h:161-182`) | `IsMultiThreaded3() && tn>1 && !IsInMMD(res)` — no `lazy` | Retains interest in both argument units via `SharedUnitInterestPtr` captured in the closure, then re-runs `Calculate` per request. The only MT3-gated site that is lazy regardless of `LazyCalculated`. The `tn > 1` conjunct was added 2026-07-30 to conform to §4.2; before that a single-tile mapping result was lazy too, recomputing its only tile on every access after a release. |
| `AbstrIDOperator` (`ID.cpp:60-81`) | none at all | `id()` is a pure generator; the result also sets `SetFreeDataState(true)` ("never cache", `ID.cpp:57`), so recomputation is the intended design rather than a fallback. |
| `combine()` back-refs (`OperUnit.cpp:132-176`) | none at all | One lazy functor per `first_rel`/`second_rel`/… sub-item; each tile is regenerated arithmetically from `(groupSize, cycleSize, unitCount)`. |
| Storage read (`AbstrDataItem.cpp:319-345`) | `IsMultiThreaded3() && tn>1 && sm->AllowRandomTileAccess()`, values in `typelists::numerics` | Guarded by `if (true \|\| sm->EasyRereadTiles())` (`:339`) — the `EasyRereadTiles()` intent is short-circuited, so *every* random-access storage manager gets the lazy re-reading functor. The closure holds a `reader_clone_farm` (`:256`) so concurrent tile reads use per-thread reader clones. |

For the first three the recompute cost is a small arithmetic kernel, so weak retention is a
deliberate memory/CPU trade. The storage case is the expensive one: a released tile means going
back to GDAL (or whatever the driver is) on the next request.

### 4.4 The `const()` channel

`ConstTileFunctor` is created by `make_unique_ConstTileFunctor` (`ConstOper.h:64-68`) from
`ConstAttrOperator::CreateConstFunctor` (`OperConv.cpp:49-59`). Its gate is neither MT3 nor
`lazy`-flag based: `tn > 1 || (tn == 1 && tileSize >= 256)` (`ConstOper.h:116`) — i.e. use the
functor whenever it saves a meaningful allocation, else compute eagerly. Like `id()`, the result
is marked `SetFreeDataState(true)` (`ConstOper.h:102`).

### 4.5 Channels that install a non-deferred object

For completeness, the other ways something reaches `m_DataObject` — none of them can produce a
lazy or future functor:

- **`DataWriteLock::Commit()`** (`DataLocks.cpp:379-405`) — the eager path taken by the `else`
  branch of every gate above, by `DoReadItem`'s serial fallback, and by config data. The class is
  chosen in the `DataWriteLock` ctor: `FileTileArray` under an `MmdStorageManager`
  (`DataLocks.cpp:292-314`), otherwise `HeapSingleValue`/`HeapSingleArray`/`HeapTileArray`
  (`DataLocks.cpp:316`, `DataArray.cpp:808-831`).
- **Eager install bypassing `Commit()`** — `OperDistrict.cpp:98-103` assigns the write-locked heap
  object into a sub-item after fixing its value range; `DataArrayValue.h:99-107` (`SetValue`,
  `SetTheValue`) does the same for single-value writes.
- **Aliasing an existing object into another item** — `union_data` with a single argument whose
  tiling already matches shares the argument's object outright
  (`res->m_DataObject = argLock;`, `Union.cpp:365-375`), and `PhaseContainer` copies the source's
  object pointer into the phase result (`PhaseContainer.cpp:259-260`). Both mean **one data object
  can be owned by several items**: if that object happens to be a `LazyTileFunctor`, its weak
  per-tile retention is now shared, and interest from any owner keeps the object (but still not
  its tiles) alive.

### 4.6 Gate ingredients, in one place

- `IsMultiThreaded3()` — registry status flag `RSF_MultiThreading3` (`Environment.cpp:533-536`,
  `Parallel.h:18`), user-toggleable in the GUI (`DmsOptions.cpp:366`); when off, the main window
  annotates the status line with `[C3]` (`DmsMainWindow.cpp:1998`). Turning MT3 off makes every
  gated site eager, i.e. converts the whole pipeline to `HeapTileArray`/`FileTileArray`.
- `tn > 1` — `FutureTileFunctor`'s own precondition (§4.1).
- `!IsInMMD(res)` — results living in a memory-mapped store must be materialized
  (`stg` `MemoryMappedDataStorageManager.cpp:115`, decl `AbstrStorageManager.h:441`).
- `res->GetLazyCalculatedState()` → the `lazy` argument: status flag `TSF_LazyCalculated` from the
  config property `LazyCalculated` (`TreeItem.h:432-433`, `TreeItemProps.cpp:297-313`), inherited
  by sub-items and pushed onto referred items (`TreeItem.cpp:577-578`, `1841-1852`). Default
  **false** — so every §4.2 result retains unless the config explicitly asks otherwise.
- `res->GetKeepDataState()` — casted-unary only.
- `!res->GetFreeDataState()` is *not* a gate anywhere, but `SetFreeDataState(true)` on `id()` and
  `const()` results marks them as never worth caching, consistent with their weak retention.
- No byte-weight term survives — see the historical note at the end of §4.2.

### 4.7 The same fork, as the cost model sees it

The scheduler names the outcome of §4.2 rather than the classes. `materialization`
(`TicBase.h:56-62`) has four values, and the mapping to this document is exact:

| `materialization` | Class installed | This document |
|---|---|---|
| `meta` | none (unit or container result) | n/a |
| `eager` | `HeapTileArray` / `HeapSingle*` / `FileTileArray` via `DataWriteLock::Commit` | §3.1, §3.2 — retains |
| `deferred` | `FutureTileFunctor` | §3.3 — retains once computed |
| `streaming` | `LazyTileFunctor` | §3.4 — recalculates |

`PredictMaterialization` (`Operator.cpp:140-145`) reproduces the gate so an estimate describes what
will actually happen:

```cpp
if (!IsMultiThreaded3() || nrTiles <= 1 || IsInMMD(res) || res->GetKeepDataState())
    return materialization::eager;
return res->GetLazyCalculatedState() ? materialization::streaming : materialization::deferred;
```

One mismatch against §4.2/§4.3 remains, and it is currently harmless: the predictor applies
`GetKeepDataState()` to *every* family, but only the casted-unary gate actually tests it. A
`KeepData` result of a unary/binary/ternary/point/lookup operator with MT3 and `tn > 1` really gets
a `FutureTileFunctor`, i.e. `deferred`, while the predictor says `eager`. The *residency* number is
unaffected — `Operator.cpp:199-202` charges the full array for both — so only the reported regime
label is wrong.

The predictor's `nrTiles <= 1` short-circuit used to disagree with `AbstrMappingOperator`, which
installed a `LazyTileFunctor` for single-tile results that the predictor called `eager`. Adding
`tn > 1` to that gate (§4.3) closed the gap, so the two now agree for every tile count.

The regimes also carry the residency model: `streaming` is charged `inflight × choreBytes`
(`Operator.cpp:192-198`, `inflight = min(nrChores, MaxConcurrentTreads())`), everything else the
full result volume. **That charge is measured to be optimistic by ~13×**, and
`schedule-with-lookahead.md` §4.4–§4.5 is where that is being chased; the mechanism side of it is
§5's cross-cutting notes below — a released tile really is freed, but several out-of-step consumers
of one result keep every tile between the slowest and fastest reader alive, which no per-class
property can express.

---

## 5. The scenario, walked through

Timeline: the item holds a calculated `m_DataObject`; consumer **A** obtained
`GetTile(t)`/a future's `GetTile()` and has released the returned lock; consumer **B** held
supplier interest on the `AbstrDataItem` the whole time and now requests read access to tile
`t`.

1. B's interest guarantees the data object is still installed — without it, A's release could
   already have triggered `TryCleanupMem → DropValue` and dropped the entire object
   (`DataLocks.cpp:154-155`), after which B would have to re-run the calculation anyway.
2. Within the living object, what B gets depends on the concrete class:

| Class | Tile after A's release | B's re-request costs |
|---|---|---|
| `HeapTileArray` / `HeapSingleArray` / `HeapSingleValue` | retained in memory (object co-owns) | pointer handout |
| `FileTileArray` | retained on disk; mapping closed | re-map + OS page-in (I/O, no recompute) |
| `FutureTileFunctor` (via `DelayedTileFunctor`) | retained in the owned `tile_record` once computed (first demand computes under mutex) | pointer handout |
| `LazyTileFunctor` | **freed** at A's last release (object holds only a weak ref) | full re-run of the apply functor; cascades into lazy suppliers / storage re-read |
| `ConstTileFunctor` | **freed** at A's last release | reallocate + refill with the constant |
| `GeneratedTileFunctor::future_caller` held by B | caches nothing itself | whatever the leaf's `GetTile` costs (rows above) |

3. The only handle that both defers computation *and* guarantees retention afterwards is a
   `FutureTileFunctor::tile_record` future. For all `GeneratedTileFunctor` descendants a future
   is merely a bookmark; if B wants to avoid recomputation of a lazy tile between two accesses,
   B must keep the `TileCRef` (the lock) itself, not the future, and not just interest.

### Cross-cutting details

- **Whole-array reads** (`GetDataRead(no_tile)` on a multi-tile array) go through a *shadow
  tile* cached in `AbstrDataObject::m_shadowTilePtr` — again a `std::weak_ptr`
  (`AbstrDataObject.h:206`, `DataArray.cpp:184-201`, `301-324`). Fixed-size elements: all tiles
  are copied into one contiguous buffer (per-tile locks released after copying). Sequence
  elements: the shadow additionally keeps `locked_cseq_t` on **every** tile
  (`const_shadow_sequence_tile::m_Seqs`, `DataArray.cpp:90-95`) — pinning all tiles (and fully
  materializing a lazy source) for the shadow's lifetime. In both cases the shadow follows
  weak-retention semantics: shared among concurrent whole-array readers, destroyed at last
  release, rebuilt (including re-pulling all tiles) on the next whole-array request — even on
  top of a fully retaining `HeapTileArray`.
- **Thundering herd**: both `LazyTileFunctor` and `tile_record` compute under a per-tile mutex,
  so simultaneous requesters never duplicate work; duplication happens only across
  *non-overlapping* access intervals.
- **Failure propagation**: `LazyTileFunctor` and `DelayedTileFunctor` report calculation
  failures into the owning item via the weak `m_ResultAdi` back-ref and re-throw on subsequent
  requests (`TileFunctorImpl.h:55-71`, `226-256`).

---

## 6. Summary

| Class | Kind | Verdict on the scenario |
|---|---|---|
| `AbstrDataObject`, `DataArrayBase<V>`, `NumericArray<V>`, `AdditiveArray<V>`, adapters | abstract | n/a (no tile storage) |
| `TileFunctor<V>` / `TileFunctor<Void>` | RTTI anchor / stub | n/a |
| `GeneratedTileFunctor<V>` | mid-layer | delegates; its futures never retain |
| `HeapTileArray<V>` | leaf, heap | **retains** |
| `HeapSingleArray<V>` | leaf, heap | **retains** |
| `HeapSingleValue<V>` | leaf, heap | **retains** |
| `FileTileArray<V>` | leaf, file-mapped | **retains** (on disk; remap on demand) |
| `DelayedTileFunctor<V>` | mid-layer | retains whatever its records hold |
| `FutureTileFunctor<V,...>` | leaf, `deferred` | **retains** once first computed; never evicts |
| `LazyTileFunctor<V,ApplyFunc>` | leaf, `streaming` | **recalculates** after last release |
| `ConstTileFunctor<V>` (clc) | leaf, lazy | **recalculates** (refill) after last release |

Which class an item ends up with is decided entirely at creation time (§4): the seven MT3-gated
operator families fork on `LazyCalculated` (default false → retaining `FutureTileFunctor`), four
channels bypass that flag and are always lazy when they fire, `const()` has its own size-based
gate, and everything else is eager heap/file storage. In the scheduler's vocabulary (§4.7) that is
`deferred` by default, `streaming` only where the config or one of those four channels asks for it,
and `eager` whenever MT3 is off or the result is single-tiled, `KeepData`, or memory-mapped.

Observations that may warrant follow-up:

1. Supplier interest does not pin lazy tiles — only live `TileCRef`s do. Consumers that release
   between accesses silently pay recomputation on every `LazyTileFunctor` /
   `ConstTileFunctor` / shadow-tile path.
2. The storage random-tile-access read path is unconditionally lazy
   (`if (true || sm->EasyRereadTiles())`, `AbstrDataItem.cpp:339`), so repeated tile access
   re-reads from the (GDAL/…) source unless the consumer keeps its locks.
3. `FutureTileFunctor` sits at the opposite extreme: computed tiles are never evicted while
   interest lasts, setting the memory high-water mark for wide pipelined results. This is the
   `deferred` regime, and it is the default — which is why the measured whole-chain saving comes
   from stages being *dropped* as consumers finish, not from tiles being freed
   (`schedule-with-lookahead.md` §4.4).
4. Nothing weighs streaming the arguments against storing the result. The gates' byte-weight
   conjunct was dead (`LTF_ElementWeight` returned 0) and has been removed; its successor
   `EstimateDataBytes` informs the estimator only. Restoring a real weight comparison is listed as
   future work in `schedule-with-lookahead.md` §5.4 — with the caveat from observation 8 that
   "lighter" is not simply "streaming".
5. `AbstrMappingOperator` (`CastedUnaryAttrOper.h:161`) still goes straight to `LazyTileFunctor`,
   so it is the one MT3-gated site that ignores `LazyCalculated` — a `mapping()` result is
   `streaming` whenever MT3 is on, whatever the config says. Its missing `tn > 1` conjunct was
   fixed 2026-07-30, which also brought it into line with `PredictMaterialization` (§4.7); whether
   it should additionally route through `make_unique_FutureTileFunctor` so `LazyCalculated` is
   honoured is the larger open question.
6. "Lazy" only halves the saving it looks like: `make_unique_FutureTileFunctor`'s lazy branch
   still builds every tile's `PrepareState` (typically a supplier future) eagerly and holds it
   for the object's lifetime (`TileFunctorImpl.h:135-148`). It drops the result footprint, not the
   supplier-handle footprint.
7. Two channels alias one data object into a second item (`Union.cpp:372`,
   `PhaseContainer.cpp:260`). If the aliased object is lazy, its weak per-tile retention is now
   shared between items that each believe they own their result — worth keeping in mind when
   reasoning about who pays for a recompute.
8. **Recalculating is not the same as being cheaper, and the measurements say so.** Every "frees
   its tiles" verdict above is a statement about one data object's ownership, not a prediction
   about peak memory. On the 50 M-element chain measured for `schedule-with-lookahead.md` §4.4,
   `streaming` came out at 542 MB against `deferred`'s 416 MB — worse, despite freeing tiles —
   because a tile survives as long as *any* consumer holds it, so several out-of-step consumers of
   one result keep everything between the slowest and fastest reader alive. That skew is invisible
   to the per-class analysis in this document and is why §4.7's `inflight × choreBytes` charge
   under-estimates by ~13×. Read the two documents together: this one tells you whether a released
   tile can come back for free, §4.4–§4.5 there tell you what it costs to keep.
