# Tile data retainment: the AbstrDataObject hierarchy

Inventory of all (template) classes deriving from `AbstrDataObject` and analysis of whether each
one **retains** lazily calculated (future) tiles, or **recalculates** them after all consumers
released a tile and a later consumer — one that held supplier interest on the owning
`AbstrDataItem` all along — requests read access to that tile again.

Based on code review of the GeoDMS26 tree (rtc/tic, clc, geo), 2026-07-30.

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
  `KEEPMEM_MAX_NR_BYTES` = 128 bytes (`FreeDataManager.h:44`, `AbstrDataItem.cpp:846-896`) —
  drops the whole data object via `DropValue`/`ClearDataObject` (`AbstrDataItem.cpp:216-228`).

The scenario in scope assumes consumer B held (supplier) interest on the `AbstrDataItem`
throughout. That interest keeps `m_DataObject` installed (it blocks the `TryCleanupMem` path
above), so the question becomes strictly a *per-tile* property of the concrete
`AbstrDataObject` subclass. Interest by itself pins the **object**, never a lazy **tile**.

---

## 2. Inheritance tree

`DataArray<V>` is an alias for `TileFunctor<V>` (`TicBase.h:52`). `TileFunctor<V>` derives from
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

## 4. Provenance: which class ends up in `m_DataObject`

| Creation path | Resulting class | Where |
|---|---|---|
| `DataWriteLock` on an item under an `MmdStorageManager` | `FileTileArray` | `DataLocks.cpp:292-314` |
| `DataWriteLock` otherwise (eager operator results, storage reads, config data) | `HeapSingleValue` (range size 1) / `HeapSingleArray` (1 tile) / `HeapTileArray` | `DataLocks.cpp:316`, `DataArray.cpp:808-831` |
| Pipelined attr operators when `IsMultiThreaded3() && tn>1 && !IsInMMD(res)` (+ weight gate), `lazy == false` | `FutureTileFunctor` | `OperAttrUni.h:81-82`, `OperAttrBin.h:81-82`, `OperAttrTer.h:94-95`, `lookupImpl.h:120-121`, `RLookupImpl.h:107`, `geo/dll/src/Point.cpp:119`; dispatch in `TileFunctorImpl.h:131-157` |
| Same, but `lazy == true` | `LazyTileFunctor` | same dispatch |
| Storage read with `tn>1`, numeric values, `AllowRandomTileAccess` | `LazyTileFunctor` (re-reads tiles from the storage manager per request) | `AbstrDataItem.cpp:312-341` — note `if (true \|\| sm->EasyRereadTiles())`: unconditional |
| Unit range attributes, `id()`, casted-unary fallback | `LazyTileFunctor` (unconditional) | `OperUnit.cpp:149`, `ID.cpp:70`, `CastedUnaryAttrOper.h:168` |
| `const(v,u)` operator | `ConstTileFunctor` | `OperConv.cpp:57` |

The `lazy` flag is `res->GetLazyCalculatedState()`: status flag `TSF_LazyCalculated` from the
config property `LazyCalculated` (`TreeItem.h:432-433`, `TreeItemProps.cpp:297-313`), inherited
from the parent and pushed onto referred items (`TreeItem.cpp:577-578`, `1841-1852`). Default
is **false**, so pipelined results retain by default; only explicitly `LazyCalculated` items
(and the unconditional cases above) recalculate.

Note: the weight gate `LTF_ElementWeight(args) <= LTF_ElementWeight(res)` is currently
neutralized — `LTF_ElementWeight` returns 0 unconditionally (`AbstrDataItem.cpp:1277-1280`);
the intended `ElementWeight` heuristic (`AbstrDataItem.cpp:1265-1275`) is bypassed.

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
| `FutureTileFunctor<V,...>` | leaf, deferred | **retains** once first computed; never evicts |
| `LazyTileFunctor<V,ApplyFunc>` | leaf, lazy | **recalculates** after last release |
| `ConstTileFunctor<V>` (clc) | leaf, lazy | **recalculates** (refill) after last release |

Observations that may warrant follow-up:

1. Supplier interest does not pin lazy tiles — only live `TileCRef`s do. Consumers that release
   between accesses silently pay recomputation on every `LazyTileFunctor` /
   `ConstTileFunctor` / shadow-tile path.
2. The storage random-tile-access read path is unconditionally lazy
   (`if (true || sm->EasyRereadTiles())`, `AbstrDataItem.cpp:333`), so repeated tile access
   re-reads from the (GDAL/…) source unless the consumer keeps its locks.
3. `FutureTileFunctor` sits at the opposite extreme: computed tiles are never evicted while
   interest lasts, setting the memory high-water mark for wide pipelined results.
4. The `LTF_ElementWeight` gate that was meant to weigh laziness against argument/result sizes
   is currently a stub returning 0, making the pipelined-functor path purely
   thread/tiling-gated.
