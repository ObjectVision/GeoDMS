# Schedule with lookahead — resource-estimating, throttling and (re)ordering OperationContext execution

*Status: design plan (no code changes yet). Drafted 2026-07-28 on branch `hof_syntax`.*
*Scope: `rtc/dll/src/tic` scheduling core, `Operator` interface, storage-read path, `PhaseContainer`.*

---

## 0. Summary

GeoDMS today schedules `OperationContext`s (OCs) demand-driven and essentially blind:
per-phase FIFO, one binary "is RAM low?" brake, no notion of what a task will cost or
how much memory it will retain. Yet almost everything needed to *predict* cost and
footprint is already available at schedule time: result skeletons (and thus domain
cardinalities) are created on the meta thread before calculation, element widths are
metadata, tiling is known, storage geometry is known before a byte is read, and an
(unused) `Operator::EstimatePerformance` scaffold already exists.

This plan works out:

1. a **resource-estimation interface** on `Operator` (and on the storage-read path,
   which is not an `Operator`) producing per-task estimates of CPU work, transient
   working memory, retained result memory and I/O — derived from argument **domain
   counts, value ranges/widths and tiling**, refined per operator family, bounded by
   modeler-declared expectations (`SizeUpperbound`, §4.6), calibrated from
   measurements;
2. a **scheduler** that uses those estimates for admission control (throttling),
   ordering (finish memory-retaining sub-task groups before opening new ones), and
   parallelism shaping (how many tile chores, pipelined or eager);
3. **automated phasing**: tasks whose estimates depend on data produced by earlier
   tasks become *estimation barriers*; completed producers publish actual counts,
   triggering re-estimation and re-planning — which generalizes what `PhaseContainer`
   does manually today, and lets most throttling-motivated fences be removed;
4. a mapping to the **CS literature** (memory-bounded DAG scheduling / pebbling,
   RCPSP, adaptive query execution, runtime systems with calibrated cost models) so
   we reuse known results instead of rediscovering them;
5. an incremental **roadmap** (P0–P5) where each step is independently shippable and
   testable against the tst regression battery.

---

## 1. Problem statement

Motivating incidents (all from regression history):

- **Peak-memory blowups.** t301: a `geos_buffer` variant with 16-byte allocation
  granularity drove process commit to ~134 GB and thrashed the pager; nothing in the
  scheduler limited how much intermediate data was simultaneously live. The only
  defenses were a reactive `EmptyWorkingSet` trim and a binary low-RAM admission stop.
- **Manual phasing burden.** Production models (RSopen `Iter_T.dms`,
  `Iter_Landbouw_T.dms`, Hestia) wrap every allocation iteration in
  `PhaseContainer(...)` purely to keep intermediate data volumes bounded and to force
  summarization before the next group starts. Modelers hand-partition the DAG because
  the engine cannot.
- **Scheduling fragility.** The 20.0.3 worker-pool starvation episode (post-order
  drain serialising supplier updates, 60× slowdown on t060) and the t641_2
  `EnableAutoDelete` teardown deadlock show that ordering/throttling changes interact
  subtly with interest management — hence the incremental, flag-guarded roadmap and
  the insistence on full-battery regression runs (`doc/development/testing-strategy.md`).
- **Inert heuristics.** The one cost comparison that exists in the hot path — the MT3
  pipelining gate `LTF_ElementWeight(arg) <= LTF_ElementWeight(res)` — is stubbed to
  `0 <= 0` (always true), so "pipelined operations" is effectively an unconditional
  mode rather than a decision.

Goal: **bounded peak footprint with good parallelism**, chosen by the engine, with
manual `PhaseContainer` demoted from *necessary* to *advisory*.

---

## 2. Inventory: how scheduling, throttling and estimation work today

References are `path:line` in this repo. This section is descriptive; §3+ is the plan.

### 2.1 Execution pipeline

| Mechanism | Where | Notes |
|---|---|---|
| Work unit | `rtc/dll/src/tic/OperationContext.h:129` | One OC per FuncDC evaluation or per item-writer (storage read/write) task; states `task_status` (`OperationContext.h:96`). |
| DAG edges | `OperationContext.cpp:840-852` (`connect`), `:1856-1902` (`connectArgs`) | Strong `m_Suppliers` / weak `m_Waiters` sets; `ScheduleCalcResult` adds `FuncDC::m_OtherSuppliers` too (`:2066-2090`). |
| Readiness trigger | `OperationContext.cpp:882-961` (`disconnect_supplier`) | Edge-driven: last supplier `done` ⇒ waiter re-enters the runnable queue. **This is the natural hook for re-estimation events (§6).** |
| Run queue | `OperationContext.cpp:585-604` | `s_ScheduledContextsMap`: `map<phase_number, deque<weak_ptr<OC>>>` — FIFO within a phase; single global mutex `cs_ThreadMessing`. |
| Phase barrier | `OperationContext.cpp:1021-1045` | Only the lowest phase is activated; higher phases wait until `s_NrActivatedOrRunningOperations` of all lower phases is 0. A **hard** barrier, not a priority. |
| Activation → run | `:1056-1094` (activate FIFO), `:2362-2384` (`FindAndLicenceOnePriorityTasks`, FIFO pop, refuses phases ≥ `s_CurrBlockedPhaseNumber`), `:1552-1595` (`getUniqueLicenseToRun`) | License gate re-queues tasks whose phase exceeds the active phase, and (#933) tasks whose storage critical section is contended (`:1583-1591`) — a proven *cooperative re-queue* pattern the admission throttle can reuse. |
| Worker pool | `rtc/dll/src/parallel/portable_task_group.h:28-45`, ctor size at `OperationContext.cpp:718-721` = `GetNrVCPUs()` (`act/MainThread.cpp:431-437`) | Plain FIFO queue of closures, no priorities. Non-MSVC build **detaches unbounded `std::thread`s instead** (`OperationContext.cpp:1130-1147`) — must be unified before any throttling work (§8 P0). |
| Tile layer | `tic/ParallelTiles.h:23-76` (`tile_task_group`), commissioning cap `OperationContext.cpp:233-269`, stealing `:158-229` | Ticket dispenser; commissions ≤ `GetNrVCPUs() − s_NrRunningTileTaskThreads` workers; stolen **LIFO** (newest group first) — incidentally the memory-friendly order (§7, work-stealing space bounds). |
| Inline vs async | `Operator.h:129` (`CanRunParallel`), `OperationContext.cpp:2047` (`doASync`), `:1417-1418` (MT2 off ⇒ inline), `:1336-1369` | Explain-contexts, non-parallel operators and MT2-off runs bypass the pool entirely on the meta thread. Any admission control must exempt these paths. |
| MT flags | `utl/Environment.h:87-92`, consumers §2.2 | MT0 suspend-for-GUI; MT1 tile tasks; MT2 async OCs; MT3 pipelined (lazy) tile functors. |
| Dead code worth noting | `OperationContext.cpp:2271-2341` (`prioritize_impl`, call site commented out at `:2478-2494`) | A DFS that would prioritize the supplier subtree of the item a joiner waits for — prior art *inside the codebase* for priority scheduling; to be revived in §5.2. |

### 2.2 Throttling today — the complete list

1. **Low-RAM activation brake** — `OperationContext.cpp:1051-1067`: during an
   activation pass, if `IsLowOnFreeRAM()` and there are at least as many
   activated/running operations as threads blocked in `Join`, stop admitting. Binary,
   size-blind, at most one probe per pass. The `s_NrWaitingJoins` exception is a
   crude progress guarantee — the germ of the "never starve the task that unblocks
   memory release" rule in §5.1.
2. **`throttled_async`** — `ParallelTiles.h:138-161`: helper fan-outs degrade to
   inline execution when `!IsMultiThreaded1() || IsLowOnFreeRAM()`.
3. **Per-storage-manager serialization** — `stg/AbstrStorageManager.h:301-312`
   (`std::binary_semaphore m_CriticalSection`) with the cooperative try-acquire gate
   (`OperationContext.cpp:1583-1591`); parallel tile reads bypass it via a
   `reader_clone_farm` bounded by a `counting_semaphore(MaxConcurrentTreads())`
   (`tic/AbstrDataItem.cpp:249-283`).
4. **Phase barrier** (§2.1) — the only *ordering* constraint beyond FIFO.

Memory relief is reactive, not anticipatory: every 100 MB of cumulative allocation,
at most once per second, `ConsiderMakingFreeSpace` trims the working set with
`K32EmptyWorkingSet` (`utl/MemGuard.cpp:127-178`). The old blocking back-off loop in
`WaitForAvailableMemory` is **commented out** (`:184-248`) — no allocation ever waits.
Allocation failure latches a global `s_BlockNewAllocations`
(`xct/DmsException.cpp:228-231`). The only *forward-looking* computation in the whole
engine is `ExpectedMemoryLoad(requestedSize)` (`MemGuard.cpp:51-56`), fed by
`GlobalMemoryStatusEx` clamped by the `MemoryRAM_MAX_GB` registry setting, whose own
comment already says it "*also throttles operation activation via IsLowOnFreeRAM*"
(`utl/Environment.cpp:615-621`). `MemoryFlushThreshold` (default 80%) is the load
ceiling (`MemGuard.cpp:62-76`).

### 2.3 Memory lifecycle (who retains, who frees)

- Results stay resident while of-interest: `DecInterestCount` → `StopInterest` →
  supplier-interest cascade (`act/Actor.cpp:1197-1276`).
- Actual freeing happens on read-lock release: `DataLocks.cpp:155` →
  `TryCleanupMem` → drop data object unless ≤ `KEEPMEM_MAX_NR_BYTES` = 128 bytes
  (`tic/FreeDataManager.h:44`, `tic/AbstrDataItem.cpp:846-896`).
- `FreeDataManager.cpp:37-50` contains a genuine byte-size predictor
  (`AbstrDataByteSize = ElemSize(vc) × domain->GetCount()`), currently referenced only
  by the commented-out swap-to-file heuristic (`:52-71`).

Consequence for the planner: **"resulting memory" is retained until the last consumer
releases interest** — so execution *order* directly determines peak resident bytes.
That is exactly the lever §5.2 pulls.

### 2.4 Existing estimation scaffolding — present but disconnected

| Piece | Where | State |
|---|---|---|
| `PerformanceEstimationData { calc_time_t expectedCalcTime; SizeT inputSize, inputSizePerChore; SizeT workingMemorySize, workingMemorySizePerChore; SizeT resultingMemory; UInt16 extraTasks; }` | `tic/Operator.h:41-49` | The right *vocabulary* already. Two latent defects: `inputSize` is the only member without a default initializer; and the default estimator's aggregate return `{ calc_time_t(nrElements), resultingMemory }` lands the memory number in **`inputSize`**, not `resultingMemory` (`Operator.cpp:89`). |
| `virtual Operator::EstimatePerformance(resultHolder, args)` | `Operator.h:86`, default `Operator.cpp:80-90` | Default = run `CreateResultCaller` (meta-time result skeleton), then `GetEstimatedCount() × ElementWeight()`. **Zero call sites in the entire tree.** One override: `clc/dll/include/OperAttrUni.h:42-47`, which multiplies by the group's `m_CalcFactor`. |
| `AbstrOperGroup::m_CalcFactor` | `tic/OperGroups.h:83,151` | Declared, defaulted to 1.0, never set to anything else. |
| `ElementWeight(adi)` | `tic/AbstrDataItem.cpp:1265-1275` | Bits-per-element from pure metadata: void→0, string→256, sequences→`bitSize×32`, else `GetBitSize()`. Note: **bits, not bytes** — unit hygiene needed. |
| `LTF_ElementWeight(adi)` | `AbstrDataItem.cpp:1277-1280` | **Stubbed to `return 0;`** — all MT3 pipelining gates (`OperAttrUni.h:81`, `OperAttrBin.h:81`, `OperAttrTer.h:94`, `CastedUnaryAttrOper.h:69,156`, `clc/dll/src/lookupImpl.h:120`, `RLookupImpl.h:107`, `geo/dll/src/Point.cpp:119`, `tic/AbstrDataItem.cpp:313`) degenerate to always-true. |
| `AbstrUnit::GetEstimatedCount()` | `tic/AbstrUnit.cpp:788-817` | Ready data → exact `GetCount()`; else evaluate the modeler-declared **`SizeEstimator`** property (`TicPropDefConst.h:37`, `TreeItem.cpp:1002-1016`); else `ASSUMED_SIZE = 1'000'000`. A three-tier confidence ladder already exists here; §4.6 adds a declared upper-bound sibling (`SizeUpperbound`) and renames `SizeEstimator` to `SizeExpectation`. |
| Tiling oracle | `tic/TiledRangeData.h:66-108` | `GetNrTiles`, `GetTileSize(t)`, `GetMaxTileSize`, and notably `GetNrMemPages(log2BitsPerElem)` — tiling × element width combined. |
| Measurement primitives | `mem/FixedAlloc.cpp:748-817` (`GetFixedAllocStatus` incl. running maxima), `:780-802` (`GetMemoryStatus`), `dbg/Timer.h:30-50` | Exist, but used only for rate-limited logging. **No CPU-time accounting, no per-operator history, no bytes-read counters** (per-read log is a bare "Read X from Y", `tic/TreeItem.cpp:4039-4044`). |

### 2.5 Phase numbers and `PhaseContainer`

- Every `Actor` gets `m_PhaseNumber` = max over suppliers (≥ 1), assigned during
  `UpdateMetaInfoImpl` (`tic/TreeItem.cpp:2803`) via
  `SupplierVisitFlag::FenceNumberScan` (`act/Actor.cpp:1513-1547`). Cache items are
  stamped with their FuncDC's phase (`tic/MoreDataControllers.cpp:601-616`); OCs
  inherit it (`OperationContext.cpp:1223-1227`, `:1398-1407`); read-locks assert
  consumer-phase ≥ producer-phase (`tic/ItemLocks.cpp:56,96`).
- `PhaseContainer(container, msgs)` (`clc/dll/src/PhaseContainer.cpp`; token
  `LispTreeType.h:162`; predecessor `FenceContainer`, switched at GeoDMS 17.06.00 in
  RSopen configs):
  - `CreateResultCaller` (`:46-121`): deep endogenous copy of the source container
    (`DataCopyMode::MakeEndogenous|InFenceOperator|CopyReferredItems`), stamped with a
    **fresh global phase number** (`GetNextPhaseNumber()`), members mirrored without
    suppliers.
  - `PreCalcUpdate` (`:138-211`): on the scheduling thread, walks the mirror,
    collects the interest-bearing members, drives `SuspendibleUpdate` +
    `CallCalcResult` on their sources, parks (interest, future) pairs in
    `m_ReadAssets` (`phase_resource`), publishes the diagnostics globals
    `s_CurrBlockedPhaseNumber` / `s_CurrPhaseContainer` (`OperationContext.h:357-363`)
    that the license gate honors.
  - `CalcResult` (`:213-295`): `WaitReady` per source, then *shares* the source's
    `m_DataObject` / `m_RangeDataPtr` into the mirror items, emits the message
    argument as `PhaseContainer(n): ...` trace lines, releases supplier interest.
  - Declared intent, verbatim from the sources: separate calculations into serially
    executed groups; "*the cardinality of fenced domains are assumed to be known as
    part of the fenced results and can be used in the schedule execution plan of the
    front items*" (`PhaseContainer.cpp:27-30`); "*force summarization of intermediate
    results*", "*partition and serialize parallel work to avoid too much simultaneous
    intermediate data*" (`clc/dll/src/SubItem.cpp:41-52`).
  - Known issues: #902 (targets added during/after phase execution), #1128 (phase
    progress/commit deferred until the final consumer joins — `doc/issues.md:50`),
    residual red items in Hestia.

So the *concept* this plan generalizes — serialize groups so that cardinalities and
memory of the back are settled before planning the front — is already articulated in
the code, but only as a **manual, all-or-nothing, modeler-placed** barrier.

### 2.6 Storage reads: what is knowable before a read runs

Reads are not `Operator`s; they are item-writer OCs created in
`TreeItem::PrepareDataRead` (`tic/TreeItem.cpp:4209-4360`) whose payload is
`StorageReadHandle::Read()`. Pre-read knowledge available to a planner:

- **Mutual exclusion group**: `OperationContext::m_RequiredStorageManager` — which
  reads serialize against each other (per manager instance; ODBC additionally has a
  process-global section, `stg/dll/src/odbc/OdbcStorageManager.cpp:238`).
- **Cardinality**: `StorageMetaInfo::PrepareReadDataOrSuspend`
  (`tic/stg/AbstrStoragemanager.cpp:71-80`, grid variant
  `stg/dll/src/GridStoragemanager.cpp:177-182`) *guarantees* the domain count and
  values-unit range are resolved before the gated task runs. `ReadUnitRange`
  (`AbstrStorageManager.h:287`) is the cheap cardinality/extent-only read: raster dims
  via `GetRasterXSize/YSize` (`stg/dll/src/gdal/gdal_grid.cpp:525-538`), DBF header
  record count (`dbf/dbfStorageManager.cpp:71-75`), SHP header
  (`shp/ShpStorageManager.cpp:463-482`), etc. Caveat: GDAL vector
  `GetFeatureCount()` may fall back to a full scan (`gdal_vect.cpp:2217-2234`) — a
  known-expensive probe the estimator must not trigger eagerly.
- **Bytes**: count × `ElementWeight`/`ValueClass::GetSize` (`mci/ValueClass.h:170-177`);
  actual file size via `FilePtrHandle::GetFileSize()` (`stg/dll/src/FilePtrHandle.h:35`).
- **Tiling / chore geometry**: native block size cached without pixel I/O
  (`gdal_grid.cpp:95-118`, `GetNativeTileSizeX/Y`, exposed as `StorageTileSizeX/Y`
  props, `tic/TreeItemProps.cpp:681-706`); `GridBlockSubdivide`
  (`AbstrStorageManager.h:202-216`); per-tile source rect via
  `ViewPortInfoProvider::GetViewportInfoEx(t, smi)` (`stg/dll/src/ViewPortInfoEx.h:70`).
- **Strategy bits**: `AllowRandomTileAccess()` / `EasyRereadTiles()`
  (`AbstrStorageManager.h:259-262`) decide fan-out over cloned readers vs serial loop
  (`tic/AbstrDataItem.cpp:285-355`).
- **Missing entirely**: measured bytes/sec or elapsed per read — nothing records them.

---

## 3. Scan of `class Operator`: which existing virtuals can feed a planner

The requested interface scan (`rtc/dll/src/tic/Operator.h`), evaluated for
estimation value:

| Member | Estimation value |
|---|---|
| `CreateResultCaller` / `CreateResult(…, mustCalc=false)` (`Operator.h:62-75`) | **High.** Builds the result skeleton on the meta thread *before* calculation: result domain and values units exist, so result cardinality and element width are readable pre-run for the (large) class of operators whose result domain is a function of argument meta. The default `EstimatePerformance` already exploits this. Cost: it may create cache items; but for FuncDC-driven OCs it runs anyway during scheduling, so estimation piggybacks nearly free. |
| `PreCalcUpdate` (`Operator.h:77`) | **Medium.** A pre-run hook on the scheduling thread (today only `PhaseContainer` overrides it). Natural place for last-moment estimate refresh once suppliers are done. |
| `CalcResult` (`Operator.h:79`) | Measurement bracket for calibration (§4.7): wrap its invocation in `RunOperator` (`OperationContext.h:302`) with a timer + allocation snapshot. |
| `EstimatePerformance` (`Operator.h:86`) | The seed to rework (§4). Currently uncalled. |
| `GetArgPolicy` / group `oper_policy` flags (`Operator.h:95-130`, `OperGroups.h:68-99`) | Classify: `has_external_effects` / `calc_requires_metainfo` ⇒ inline-only (exempt from throttling); `is_transient`, `dont_cache_result` ⇒ result-retention differs; `is_template_call`/meta operators ⇒ zero data cost. |
| `DescribeSignature` / `DescribeSpecSignature` / `DescribeMetaSignature` (`Operator.h:102-125`, `OperSignature.h`) | **Strategic.** The typed-HOF signature layer already declares, per member, how result domain/values units relate to argument units. A generic estimator can *derive* result cardinality and element width from the signature record without instantiating anything — the same records serve type checking and cost lookahead. Where a signature exists, the default estimate needs no per-operator code at all. |
| `CanRunParallel` (`Operator.h:129`) | Already the async eligibility bit; keep as-is. |
| `AbstrOperGroup::m_CalcFactor` / `CreateValuesUnit` (`OperGroups.h:83,133`) | Group-level cost scalar (to be *calibrated*, not hand-set — §4.7) and the unit-creator that determines result element type. |

**Gaps** (what the interface cannot express today):

1. No working-memory model (scratch during `CalcResult` — sort buffers, index
   tables, rasterization bitmaps).
2. No complexity class — everything is implicitly O(n) with factor 1.
3. No chore decomposition info (`extraTasks`/`…PerChore` fields exist in the struct
   but nothing fills or reads them).
4. No I/O model for read operations (not `Operator`s at all).
5. No uncertainty/confidence marking — "exact count" vs "SizeEstimator" vs "assumed
   1M" are indistinguishable to a consumer.
6. No way to say "my result cardinality is unknowable until I run" (`select`,
   `unique`, …) — the *estimation barrier* notion of §6.
7. Units are inconsistent (bits vs bytes) and the one default implementation
   mis-assigns its own struct fields.

---

## 4. Proposed estimation interface

### 4.1 The estimate record

Replace/extend `PerformanceEstimationData` (keeping the name is fine) with explicit
units (bytes, abstract element-ops) and confidence:

```cpp
enum class estimate_confidence : UInt8 {
    measured,   // actual value from a completed run / ready data
    derived,    // computed from ready supplier meta (exact counts, exact widths)
    declared,   // from a SizeExpectation / SizeUpperbound property (§4.6)
    bounded,    // sound upper bound (e.g. select <= src count); expected unknown
    assumed     // ASSUMED_SIZE-style fallback
};

// How the result's data comes into existence -- the single biggest factor in what an
// operation costs in memory. See §4.4 for the regimes and their measured footprints.
enum class materialization : UInt8 {
    meta,       // unit/container result: no data at all
    eager,      // full array written under a DataWriteLock before CalcResult returns
    deferred,   // FutureTileFunctor: tiles computed on pull and KEPT (strong tile records)
    streaming,  // LazyTileFunctor: tiles freed when the consumer releases them (weak refs)
};

struct ResourceEstimate
{
    calc_time_t elemOps          = 0;   // abstract work units; wall-time = elemOps * calibrated group factor
    SizeT ioBytes                = 0;   // storage traffic (reads/writes)
    SizeT workingSetBytes        = 0;   // transient scratch, freed when CalcResult returns
    SizeT workingSetBytesPerChore= 0;   // per-tile/chore share, for parallelism shaping
    SizeT resultBytes            = 0;   // eventual full result volume, once every tile exists
    SizeT residentBytes          = 0;   // what the ledger charges: see §4.4 -- resultBytes for
                                        // eager/deferred, inflight*choreBytes for streaming
    SizeT choreBytes             = 0;   // one tile's worth of the result
    SizeT resultBytesUpperBound  = 0;   // sound bound when resultBytes is a guess (structural or declared, §4.6)
    UInt32 nrChores              = 1;   // tile/chore fan-out this task will commission
    materialization regime = materialization::eager;
    estimate_confidence confidence = estimate_confidence::assumed;
    // estimation barrier: args whose *data* (not meta) must exist before the numbers
    // above can leave 'bounded/assumed' state; empty means statically estimable.
    // (bitset<max_args> or small vector<arg_index>)
    arg_flags_t dependsOnDataOf  = {};
};
```

Design points:

- **Bytes everywhere.** Introduce `ElementByteWeight(adi)` beside the existing
  bits-based `ElementWeight` and fix all consumers; while at it, implement or delete
  `LTF_ElementWeight` — an inert gate is worse than none (it misleads readers into
  thinking a decision exists).
- **Three memory numbers, not two.** `workingSetBytes` (transient, charged only while
  the OC runs), `resultBytes` (the eventual full volume) and `residentBytes` (what the
  ledger actually charges from completion until interest release — §5.1). The third is
  needed because a pipelined result's peak is *not* its full volume; §4.4 derives it
  per regime. This is what lets the scheduler prefer "finish and shrink" over "start
  and grow" without mistaking a streamed chain for a whale.
- **Bounds as first-class.** Many "unknown" cardinalities have sound cheap bounds:
  `select ≤ |src|`, `unique ≤ |src|`, `union = Σ|args|`, raster→polygon ≤ #cells.
  A `bounded` estimate lets admission be conservative without blocking planning.
  Where the structural bound is uselessly loose (a select over a full OD product),
  the modeler-declared `SizeUpperbound` (§4.6) replaces it.
- Fix the two latent defects in the current struct/default (missing initializer;
  aggregate-init mis-assignment) as an immediate P0 item, independent of everything
  else.

### 4.2 The virtual

```cpp
// Operator.h — replaces EstimatePerformance (keep old name as a thin shim if desired)
TIC_CALL virtual auto EstimateResources(TreeItemDualRef& resultHolder,
                                        const ArgRefs& args) const -> ResourceEstimate;
```

Default implementation (generalizing today's `Operator.cpp:80-90`):

1. Ensure the result skeleton exists (`CreateResultCaller` — as today).
2. For a data-item result: `n = domain->EstimateCount()` (§4.6 ladder: ready ⇒
   `derived`; `SizeExpectation` expected / `SizeUpperbound` bound ⇒ `declared`;
   structural subset/union/product rules ⇒ `bounded`; else `assumed`);
   `resultBytes = n × ElementByteWeight(res)`;
   `elemOps = n × Σ ElementByteWeight(arg_i)/reference_width`; `nrChores` from
   `domain->GetNrTiles()`; per-chore numbers from `AbstrTileRangeData::GetMaxTileSize`
   (or the existing `GetNrMemPages(log2Bits)` helper).
3. For unit/container results: near-zero cost (meta work only).
4. If any *data-dependent* argument is not `IsDataReady`, mark it in
   `dependsOnDataOf` and degrade confidence to `bounded`/`assumed` accordingly.

Where a `DescribeSignature` record exists (§3), step 2 can run **without**
instantiating the result: the signature names which argument's domain the result
ranges over and which values unit it carries — enough for `n`, widths and chores.
Prefer that path when available; it keeps estimation allocation-free.

### 4.3 Family-level overrides ("the various classes of operators")

Override once per family base, mirroring how `CreateResult` is already factored —
*not* per concrete instantiation:

| Family (base class) | Estimate refinement |
|---|---|
| `AbstrUnaryAttrOperator` / `Bin` / `Ter`, casted variants (`clc/dll/include/OperAttr*.h`, `CastedUnaryAttrOper.h`) | Exact: elementwise, `elemOps = n`, `workingSet ≈ tile-sized`, result = n×width. The existing `OperAttrUni.h:42` override pattern (× group factor) becomes the default behavior. |
| Aggregations (`OperAccUni.h`, `OperAccBin.h`, partitioned totals) | `elemOps = n`; `resultBytes = |partition-domain| × width` — the *values/partition unit* count, which is typically ready meta; workingSet = accumulator array = result-sized (or #threads × result-sized for parallel accumulation — reflect the actual `throttled_async` structure). |
| `lookup`/`rlookup`/index building (`lookupImpl.h`, `RLookupImpl.h`, `IndexAssigner`) | workingSet includes the index table (m×width or hash-table factor); `elemOps = n + m log m` for sort-based paths. |
| Sort/order/`unique`/`nth_element` (`clc/dll/src/Unique.cpp`, `geo/dll/src/nth_element.cpp`) | `elemOps = n log n`; workingSet ≈ 2n×width (copy + sort buffer); `unique` result: `bounded` by n. |
| Relational producers with data-dependent cardinality (`select_*` family, `subset`, `collect_by_cond`) | result count unknown ⇒ `bounded` by source count — or by a declared `SizeUpperbound` on the consuming config unit (§4.6) — `dependsOnDataOf = {condition}`; after the condition's data exists, `pcount`-style refinement is exact. **Prime estimation-barrier clients (§6).** |
| Geometric/geo operators (`geo/dll/src/BoostPolygon.cpp`, `BoostGeometry.cpp`, `geos*`, `Poly2GridOper.cpp`, `Dijkstra.cpp`, `Connect.cpp`) | Superlinear and value-dependent: per-group declared complexity class + calibrated factor (§4.7); inputs from counts *and value ranges* (e.g. Dijkstra: #nodes/#links/#origin-zones are all ready meta; poly2grid: output ≈ covered-cell count bounded via bounding boxes from the values units' ranges). These operators already keep local progress `Timer`s (`BoostPolygon.cpp:864`, `Dijkstra.cpp:617`) — the calibration hook replaces ad-hoc logging. |
| Sequence/string-valued inputs | Per-element width unknown (256-bit / avg-32-elements guess today) ⇒ declared total-element/character bound when present (`SizeUpperbound` on the attribute, §4.6 — covers both compositions and the `string` value type), else **pilot-tile probing** (§4.8). |
| Meta/tree operators (`PhaseContainer`, `subitem`, `for_each*`, template calls) | Zero data cost; `PhaseContainer` charges nothing itself but *its suppliers* dominate — see §6.4. |
| **Storage reads** (not `Operator`s) | Attach a `ResourceEstimate` to the item-writer OC at creation (`TreeItem.cpp:4326-4355` / `OperationContext::CreateItemWriter`): `ioBytes = count × width` (or actual file size when smaller), `resultBytes` likewise, chores from native tiling, seriality = the manager's CS group, throughput factor calibrated **per storage-manager class** (GDAL-raster vs FSS vs ODBC…). All inputs are pre-read-knowable per §2.6. |

### 4.4 Materialization regimes: what a result actually occupies

An estimate that charges every intermediate its full `n × width` is wrong for most of a
GeoDMS calculation, because tiled results are usually *not* built in one go. The regime
is decided per operator instance, by the very predicate the operator families apply
(`clc/dll/include/OperAttr*.h`, `CastedUnaryAttrOper.h`, `clc/dll/src/{lookup,RLookup}Impl.h`,
`geo/dll/src/Point.cpp`), so the estimator can predict it from the same inputs:

```
tn          = domain->GetNrTiles()
canPipeline = IsMultiThreaded3() && tn > 1 && !IsInMMD(res) [&& !res->GetKeepDataState()]
regime      = !canPipeline                      ? eager
            : res->GetLazyCalculatedState()     ? streaming   // LazyTileFunctor
            :                                     deferred    // FutureTileFunctor
```

| Regime | Mechanism | What is resident |
|---|---|---|
| **eager** | `parallel_tileloop` under one `DataWriteLock` (`OperAttrUni.h:85-95`) | the whole array, from `CalcResult` until interest release |
| **deferred** | `FutureTileFunctor`: `m_ActiveTiles` is `unique_ptr<shared_ptr<future_tile>[]>` (**strong**) and each `tile_record::GetTile` stores its computed tile in `m_State` for good (`TileFunctorImpl.h:89-126`) | grows tile-by-tile toward the whole array as consumers pull; released only when the data object is dropped |
| **streaming** | `LazyTileFunctor`: `m_ActiveTiles[t].m_TileFutureWPtr` is **weak**, so a tile dies with the consumer's lock and is recomputed if pulled again (`TileFunctorImpl.h:211-256`) | `inflight × choreBytes`, where `inflight ≈ min(nrConcurrentPullers, tn)` — plus recompute cost on re-pull |

**Measured** (P0 instrumentation + sampled peak working set; `scratch/pipe_{A,B,C}.dms`,
a 50 M-element `id → add → mul → add → float64/div → sum` chain, 4 attribute stages):

| Run | Regime | Peak working set |
|---|---|---|
| `/C3` (MT3 off) | eager | **1303 MB** ≈ Σ stages (3 × 200 MB uint32 + 400 MB float64) |
| default (MT3 on) | deferred | **416 MB** |
| `LazyCalculated = "True"` | streaming | **542 MB** |

Three consequences for the plan:

1. **Pipelining is a 3.1× memory lever, and it is the ordinary case** — so the ledger
   must charge `residentBytes` by regime. Charging `resultBytes` unconditionally would
   have throttled this harmless chain at 1.3 GB while a genuine whale (an eager sort, an
   aggregation accumulator) went unnoticed.
2. **The default is *deferred*, not streaming.** Tiles are computed on demand but then
   kept, so a stage still tends toward its full volume while it is of interest; the
   whole-chain saving comes from stages being *dropped* as their consumers finish, not
   from tiles being freed. A per-worker constant applies only to `streaming`, which today
   requires the modeler to set `LazyCalculated`. Whether that should be the engine's
   default for consumed-once intermediates is a real question for §5.4 — it is a
   behavioural change and needs the ledger to measure it, so it stays out of P1.
3. **Streaming is not automatically cheaper** (542 MB > 416 MB here): its footprint is
   `inflight × choreBytes`, i.e. tile geometry × concurrency. With few, large tiles and
   many workers it can beat eager and lose to deferred. That makes **tile geometry a
   first-class estimator output** (`nrChores`, `choreBytes`) and raises the value of
   §5.4's shaping lever — bounding in-flight tiles is how streaming is made to pay off.

**Validated against the estimator** (P1, same three configs; the estimator predicts the regime
from the resolved tiling and reports it on every line):

| Run | Predicted regime | `nrChores` × `choreBytes` | Estimated resident/stage | Measured peak (whole chain) |
|---|---|---|---|---|
| default | deferred | 764 × 256 KB | 200 MB (uint32) / 400 MB (float64) | 416 MB |
| `LazyCalculated` | streaming | 764 × 256 KB | **8 MB** (= 32 workers × 256 KB) | **542 MB** |
| `/C3` | eager | — | 200 MB / 400 MB | 1303 MB |

Two open discrepancies, both of which say *do not grant admission on these numbers yet*:

- **Deferred over-reserves.** Σ per-stage resident = 1.0 GB against a 416 MB measured peak,
  because stages are dropped as their consumers finish rather than all being live. The §5.1
  progressive-charging bullet is the decision this forces.
- **Streaming under-estimates by ~13×** (40 MB summed over five stages vs 542 MB measured). The
  `inflight × choreBytes` model is therefore *optimistic*, and admission must not trust a
  streaming estimate until the extra residency is accounted for — candidates: per-thread
  accumulators in the pulling aggregation, tiles held across the whole chain by one read lock,
  or weak tile refs not being released as promptly as the type suggests. Identifying it is the
  first task of P2, ahead of any grant logic.

#### 4.4.0 The regime is a property of the data object, not of a predicate

**`doc/tile-data-retainment.md` is the authoritative inventory** of which `AbstrDataObject`
subclass retains its tiles and which recalculates them; its **§4.7** holds the
class ↔ `materialization` mapping and the audit of `PredictMaterialization` against the real
gates. Division of labour: that document says *which class you get and whether a released tile
comes back for free*, this section says *what it costs*. Two corrections it made here:

1. **There are more regimes than three, and one is a spill.** `FileTileArray` (chosen by the
   `DataWriteLock` ctor under an `MmdStorageManager`) retains **on disk** and maps on demand:
   the last consumer release unmaps the view, a re-request pages back in with no recomputation.
   Its RAM cost is bounded by live mappings, not by the array — so a ledger must not book it as
   heap. `ConstTileFunctor` is a fifth class, recalculating with its own non-MT3 gate.
2. **The regime cannot be predicted from `LazyCalculated` alone.** Four channels build a
   recalculating object regardless of it — `id()`, `combine()` back-refs,
   `AbstrMappingOperator`, and the random-access storage read (whose `EasyRereadTiles()` intent
   is short-circuited by `if (true || …)`) — plus `const()` with its own gate. So the estimator
   now **measures** the regime through a new `AbstrDataObject::GetMaterialization()` virtual and
   reports it beside its prediction, flagging disagreement as `!regime=… predicted=…`. First run
   caught `id()` exactly as the inventory says: actual `streaming`, predicted `deferred`.

It also explains §8.1.1's negative result mechanically: for any `GeneratedTileFunctor`
descendant — which `LazyTileFunctor` is — a held future is a `future_caller` **bookmark** that
pins the data object, not the tile. So in a *streaming* chain the prepare-states array holds
bookmarks rather than tile data, and dropping it frees nothing; the prepare state only holds
real data when the argument is a `FutureTileFunctor`, and there the `tile_record` variant
already releases it the moment the consumer tile materializes. The inventory's own observation 6
states the same conclusion independently.

Its §4.7 audit also found a defect in `PredictMaterialization`: the predictor applied
`GetKeepDataState()` to all seven gated families, but only the casted-unary gate tests it, so a
`KeepData` result of a unary/binary/ternary/point/lookup operator was labelled `eager` when it
really gets a `FutureTileFunctor`. Residency was unaffected (both charge the full array) but the
label was wrong and would now trip the mismatch marker on every such result. Fixed: the base
predictor drops the term and `AbstrCastedUnaryAttrOperator` adds it in its own override. Its
observation 8 independently reaches §4.4.1's conclusion — `streaming` measured *worse* than
`deferred` because a tile lives as long as any consumer holds it, so out-of-step consumers keep
everything between the slowest and fastest reader alive, which no per-class property can express.

One simplification to keep visible: §4.7 maps `FileTileArray` onto `eager`, and the estimator
charges it the full array. That over-charges it — a file-backed result's RAM cost is its live
mappings, not its volume, and a released tile costs a page-in rather than a recompute. A ledger
that books MMD-backed results as heap will refuse work it could have run.

Two further leads from it for the still-unexplained streaming residency: whole-array reads build
a **shadow tile** (`m_shadowTilePtr`, weak) that copies every tile into one contiguous buffer —
and for sequence values additionally pins `locked_cseq_t` on *all* tiles, fully materializing a
lazy source; and two channels **alias one data object into two items** (`union_data`,
`PhaseContainer`), so a ledger keyed on items would double-count it. Neither applies to the
measured probe (the aggregation reads per-tile via `GetFutureTileArray` + `GetTile(t)`), which
leaves the per-thread accumulators and allocator-pool slack as the candidates to separate next.

#### 4.4.1 What actually forces a tile to be retained: consumer skew

Regime alone is too coarse. A pipelined tile has to exist only while some consuming operation
still holds it — consumers take `GetFutureTileArray(...)` (`tic/FutureTileArray.h:19-27`), an
array of `shared_ptr<future_tile>` *usage holders*, and pull each tile when their own loop
reaches it. So the retention a result imposes is governed by **how far apart its consumers'
tile loops run**, not by its element count:

- **one consumer, in step** — tile `t` is live only while that consumer processes it:
  `residentBytes ≈ inflight × choreBytes`, the streaming figure, *whatever* the regime, because
  nothing else references the older tiles.
- **several consumers, out of step** — every tile between the slowest and fastest consumer's
  position must stay alive: `residentBytes ≈ (skew + inflight) × choreBytes`, degrading to the
  whole array when one consumer is at tile 0 while another is at tile *n*.
- The two regimes differ in *who* holds the tile, and that is what makes skew payable or not:
  `FutureTileFunctor` keeps a strong `tile_record` per tile, so a retained tile stays retained
  for the functor's life even after every consumer has passed it, whereas `LazyTileFunctor`
  holds only weak refs, so its retention really is the skew window — at the price of
  recomputing a tile pulled twice.

That yields a lever the plan did not have: **either retain, or synchronize.** For a result with
multiple pipelined consumers the scheduler can
(a) accept retention and book `nrChores × choreBytes` (today's deferred behaviour), or
(b) **keep the consumers' tile loops in step** — dispatch tile `t` to all consumers of a result
before advancing — and book `(bounded skew + inflight) × choreBytes` instead. (b) is the
memory-cheap option and needs no new data structure: the tile dispatch order already goes
through `tile_task_group` (`OperationContext.cpp:233-269`), which is where a
consumers-of-the-same-result group could be advanced together, with the skew bound as its knob.
This is also the honest explanation candidate for §4.4's unexplained streaming residency
(542 MB measured against 40 MB predicted): five stages pulled by one aggregation is exactly the
multi-consumer, unsynchronized case, so the skew term may *be* the missing memory. Confirming
that is the first P2 measurement, and `estimate.nrChores`/`choreMemory` are already reported per
operation to support it.

For diagnosis, the two modes answer different questions, and the estimator covers both:
run with **`/C3`** (eager) to attribute compute time and full volumes per operator, and
with **`/S3`** (default) to see the footprint production will actually have. The residual
report names the regime on every line so the two are never compared by accident.

### 4.5 Value ranges, not just counts

The user-visible promise "probe with argument domain **and values ranges**" concretely
means:

- element widths and sub-byte packing from the values unit's `ValueClass`;
- partition/result cardinalities from *values* units of indirection args
  (aggregations, `pcount`, histogram);
- geometric extent products from `GetRangeAsDPoint/IRect` of coordinate values units
  (raster intersection areas, poly2grid output bounds, buffer inflation);
- overflow/precision-driven algorithm choice (checked vs unchecked paths) where it
  changes cost class.

`StorageMetaInfo::PrepareReadDataOrSuspend` already forces values-unit ranges for
reads; the estimator gives the same guarantee a use.

### 4.6 Declared expectations: the `SizeUpperbound` property

The biggest residual gaps in §4.2–§4.3 are data-dependent cardinalities (subsets,
unions, sparse OD matrices) and variable-width value volumes (sequence/arc/polygon
and string attributes), where structural bounds are sound but uselessly loose. These are
exactly the places where the modeler *has* cheap knowledge the engine cannot derive.
Add a config property **`SizeUpperbound`**: a calculation rule (an expression
string, in the style of the existing `SizeEstimator` — renamed `SizeExpectation`
below) that is deliberately **cheaper to evaluate than the actual
cardinality/size** and declares a **sound upper bound**:

- on a **domain unit**: an upper bound on its `GetCount()`;
- on an **attribute with variable-sized elements** — `ValueComposition !=
  VC_Single` (sequence/arc/polygon) **or the `string` value type** (`VC_Single`
  but variable-width): an upper bound on the **total element count** (Σ over rows
  of the per-row sequence length; for strings, the total character/byte count) —
  the real driver of the data block `totalElems × fieldWidth + n × indexWidth`
  (`fieldWidth` = 1 byte for strings), where today's `ElementWeight` can only
  guess 32 elements per row for compositions and 32 bytes per string
  (`tic/AbstrDataItem.cpp:1269-1273`). Declaring it on a fixed-width attribute
  (non-string `VC_Single`) draws a config warning (widths are exact there).

Sketch (expression vocabulary to be fixed in P1 — parameters, counts of source
domains, arithmetic):

```dms
unit<uint32> ODpairs := select_uni(within_reach)          // structural bound = #origins·#dests
,   SizeUpperbound = "#origins * MaxDestsPerOrigin"; // sparse expectation, O(1) to evaluate

attr<dpoint> route (ODpairs, arc)
,   SizeUpperbound = "#origins * MaxDestsPerOrigin * MaxRouteVertices";

attr<string> label (ODpairs)
,   SizeUpperbound = "#origins * MaxDestsPerOrigin * MaxLabelChars";  // total characters
```

Integration:

- **Ladder rework.** `AbstrUnit::GetEstimatedCount()` (`AbstrUnit.cpp:788-817`)
  becomes `EstimateCount() → {expected, upperBound, confidence}`: ready ⇒ exact;
  `SizeExpectation` ⇒ expected (`declared`); `SizeUpperbound` ⇒ upperBound
  (`declared`), **overriding** the structural bound — the point of the sparse-OD
  case, where select-over-product would bound at #origins·#dests; structural rules ⇒
  upperBound (`bounded`); else `ASSUMED_SIZE`. Admission reserves on the bound
  (§5.1); ordering ranks on the expected (= the bound when nothing better exists).
  The two properties compose: expected from one, bound from the other.
- **Attr-side byte model.** `EstimateDataBytes(adi)` uses the declared total-element
  bound for variable-width attributes (VC≠Single compositions and `string`);
  per-chore shares are prorated by tile fraction
  (the bound cannot be localized per tile; admission charges the total, chore
  shaping uses the prorata).
- **Plumbing.** Exactly the `SizeEstimator` template: name constant beside
  `SIZE_ESTIMATOR_NAME` (`tic/TicPropDefConst.h:37`), lazy member beside
  `mc_SizeEstimator` in `TreeItem::ConfigProperties` (`tic/TreeItem.h:625`),
  accessors + Void-domain single-numeric validation as in `TreeItem.cpp:1002-1016`,
  property registration in `tic/TreeItemProps.cpp`.
- **Cheapness discipline.** Evaluated via `CalcCertainResult` on the meta thread at
  estimation time. If the rule's own suppliers are not data-ready, the estimator
  does **not** force them: the property counts as temporarily unavailable and the
  depending estimate is marked refreshable through the same `dependsOnDataOf`
  machinery (§6.2) — a declared bound may itself resolve at a phase boundary.
  Authoring guidance (P5 docs): reference only parameters, source-domain counts and
  cheap aggregates of small, ready data.
- **Trust and the violation tripwire.** `declared` bounds are trusted for planning —
  which is what turns an `assumed` whale into an admissible, ordered task. When the
  actual count/size materializes, it is compared against the declaration: exceeding
  it logs a warning with both numbers and flags the item in the estimate-vs-actual
  report (§4.7). Planning-only semantics keep violations harmless to results (the
  `IsLowOnFreeRAM` backstop absorbs the under-reservation); a strict
  IntegrityCheck-like failure mode can follow later if wanted.
- **Where consumers find it.** Operator-created cache units carry no config
  properties; the declaration lives on the *config* unit/attribute that refers to
  the cache result — and that is what downstream consumers' estimates consult,
  since their argument units are the config items. The producing OC itself (the
  `select_uni` above) keeps its structural bound; the declaration pays off across
  all consumers ranging over the subset/OD unit, which is where the volume is.
- **Relation to `SizeEstimator` — renamed `SizeExpectation`.** Keep both
  properties, with sharpened roles and symmetric names: the existing
  `SizeEstimator` becomes **`SizeExpectation`** = expected value (a point
  estimate, may err either way, never used for reservations); `SizeUpperbound` =
  sound bound (reservations allowed). A clean rename — the property has not been
  used in production configs yet, so no compatibility alias is needed; the
  `GetEstimatedCount` error texts and the C++ identifiers (`SIZE_ESTIMATOR_NAME`,
  `mc_SizeEstimator`, `Has/GetSizeEstimator`) follow. Most models will want only
  the bound.

### 4.7 Calibration instead of hand-tuned constants

Hand-maintained per-operator constants rot (witness `m_CalcFactor ≡ 1.0` since
inception). Instead, close the loop the way StarPU's per-codelet history models and
SQL Server's memory-grant feedback do:

- **Measure**: wrap `RunOperator`/`CalcResult` and `StorageReadHandle::Read()` with
  wall-clock + `GetFixedAllocStatus()` deltas (allocation high-water is attributable
  per OC as a first approximation; refine later with per-thread tallies in
  `FixedAlloc` if needed). Record `(group, value-class, n, chores) → (elapsed, peakWS,
  resultBytes)`.
- **Learn**: per `(group name, arg value-class)` keep an EMA of
  `elapsed / elemOps` and `peakWS / predictedWS`. Persist per machine (LocalAppData,
  beside the existing registry settings); ship nothing.
- **Feed back**: `EstimateResources` multiplies by the learned factor; the
  estimate-vs-actual residual is logged under a new `MsgCategory::performance` so
  drift is visible (and regression-testable: the tst battery gains an
  estimate-accuracy report).

### 4.8 Pilot-tile probing for value-dependent costs

For sequence/string/polygon payloads, per-element cost and width are data-dependent.
The tiling architecture gives a cheap sampler: run **tile 0 as a probe chore first**
(it must run anyway), measure its elapsed/bytes, extrapolate to the remaining
`GetNrTiles()−1` tiles, and only then decide chore fan-out and admission of siblings.
This composes naturally with MT3 lazy tile functors (the probe is just the first
materialized tile) and is the sampling-based-estimation idea from adaptive query
processing applied to tiles.

---

## 5. Scheduler: using the estimates

Everything here lives at the three existing choke points — activation
(`collectOperationContexts`), licensing (`getUniqueLicenseToRun`), and tile
commissioning (`tile_task_group` ctor) — plus the queue ordering. No new global
architecture.

### 5.1 Admission control (throttling)

- **Budget** `B` = usable physical RAM per the existing `memory_info` /
  `MemoryRAM_MAX_GB` clamp × `MemoryFlushThreshold`. Same knobs, new semantics:
  they already claim to throttle activation; now they will, proportionally.
- **Ledger**: the scheduler tracks `committed = Σ workingSetBytes(running OCs) +
  Σ residentBytes(completed-but-still-of-interest results it admitted)`. Charging
  `residentBytes` rather than `resultBytes` is what makes the regimes of §4.4 count:
  a streamed stage is charged `inflight × choreBytes`, an eager or deferred one its
  full volume. On the measured 50 M chain that is the difference between 1.3 GB and
  ~0.4 GB of booked memory for identical work. Result retirement is observed by hooking
  the existing release path (`TryCleanupMem`/interest-drop already funnel through few
  sites, §2.3); precision can be approximate — the ledger is a planning device, not an
  allocator.
- **Deferred results are charged progressively, not at once.** A `FutureTileFunctor`
  reaches its full volume only as consumers pull it, so booking `resultBytes` at
  completion over-reserves for a chain whose stages are dropped as they are consumed.
  Book `choreBytes × nrChores` as the ceiling but let the *observed* growth (tile pulls)
  drive the charge, or accept the over-reservation and rely on the retirement-first
  ordering of §5.2 to keep the error harmless. Decide with the ledger's own numbers in
  P2; do not guess.
- **Rule**: admit the next queued OC iff `committed + estimate ≤ B`, where
  `estimate` uses `resultBytesUpperBound`/pessimistic numbers for
  `bounded`/`assumed` confidence, declared `SizeUpperbound` bounds at face
  value (§4.6), and expected numbers for `derived`/`measured` (robustness against
  the misestimates §7's DB literature warns about).
- **Progress guarantee** (deadlock-freedom): always admit when nothing is running
  and nothing can retire — i.e. the head-of-queue task is admitted unconditionally
  if `committed == 0` for running work, and a task that is the sole blocker of a
  `Join` inherits the current `s_NrWaitingJoins` exception (`OperationContext.cpp:1060`).
  An over-budget estimate alone must never wedge the engine; it serializes it.
- **Exclusive mode** for whales: an OC whose estimate exceeds a fraction (e.g. B/2)
  runs alone (admission admits nothing else until it retires) — the automatic
  version of what modelers use `PhaseContainer` for today.
- **Mechanism**: reuse the #933 pattern — the license gate returns the task to the
  queue on budget refusal instead of blocking a worker (`OperationContext.cpp:1583-1591`
  is the template) — and keep the once-per-pass `IsLowOnFreeRAM` probe as the reality
  backstop for estimate error.
- **Not hysteresis.** An earlier draft of this bullet prescribed an admit-below-75% /
  refuse-above-90% band by analogy with load-shedding controllers. That was wrong for
  this gate. Hysteresis damps oscillation around a threshold, which needs a mode that
  flips, a control action that moves the measured quantity, and a jittery signal. This
  gate has none: `AdmitOrRequeue` is a **per-task predicate** with no global
  "now refusing" state; an admitted task **runs to completion**, so the
  admit→overshoot→evict→re-admit cycle cannot occur; and `committed` moves only at our
  own discrete events (charge at licence, release at completion, unbook at
  `ClearDataObject`), not as a sampled OS signal. A band would also be actively
  counter-productive: a task refused at 91% would then have to wait for a drawdown to
  75%, i.e. more refusals and more wall time for a marginally lower peak — which
  lowering `/SB` already delivers directly. The one mechanism here that *is* a
  classical hysteresis candidate is the pre-existing `s_IsInLowRamMode` brake
  (`OperationContext.cpp:1051-1067`), because that genuinely is a mode driven by a
  sampled OS signal — but it is a separate mechanism from this ledger and there is no
  evidence of it flapping, so it stays untouched until measured.
- **Drain mode** (user ruling 2026-07-30): a refused task becomes the *claimant*, and
  while it waits, admission defers every operation that would **add** retained memory —
  even one that fits — so ready leaves of other branches cannot consume the budget the
  claimant waits for. Operations that do **not** add retained memory keep flowing:
  *compressors*, i.e. `residentMemory ≤ reclaimableInputMemory`, where
  `reclaimableInputMemory` counts the inputs the operation is the **last** consumer of
  (interest count 1, cache item, not kept) — `C := A + B` with A and B unneeded
  afterwards, size-reducing aggregations. The refusal is **lifted** when (a) nothing
  else runs, or (b) no memory-decreasing operation is available — detected without any
  new signalling: refused tasks re-queue at the *back*, so if the drain **generation**
  (bumped on every admission and every release) is unchanged since the claimant's
  previous refusal, a whole queue cycle admitted no compressor and released nothing,
  and waiting longer cannot drain. This is the admission-side enforcement of §5.2's
  retirement-first ordering, and it subsumes the reservation idea: freed memory is
  protected not arithmetically but categorically.
- **Hysteresis, revisited for drain mode.** Drain mode *does* have the three
  ingredients the plain threshold gate lacked: a mode that flips (claim set/cleared), a
  control action that moves the measured quantity (deferral drains `committed`), and
  observable flapping (each admitted claimant can be followed immediately by the next
  refusal, re-entering drain — visible in the mixed-probe trace as successive short
  claim epochs). So an **exit band** — keep deferring growers until `committed` drops
  below `L·budget` even after the claimant was admitted — is now a *coherent* option,
  unlike before: it would batch admissions and cut claim-epoch churn, at the price of
  parallelism inside the band and a shift of the mode's owner from a specific task
  (the user's intent: the whale gets in) to a memory level. Entry-side hysteresis
  remains pointless (entry is an event, a refusal, not a threshold crossing). Verdict:
  defer until a real-model run shows epoch flapping that the cheaper retry-discipline
  fix does not already remove — each epoch admits at least one task, so flapping here
  is inefficiency, not livelock.
- **Exemptions**: inline/runDirect paths, explain contexts, `CanRunParallel()==false`
  operators, and the meta thread are never budget-gated (they are today's correctness
  paths); tile chores are shaped (§5.4), not admission-gated.

### 5.2 Ordering: retire memory before opening new memory

Replace the per-phase FIFO deque with a priority order (stable, FIFO tiebreak) under
the same `cs_ThreadMessing`:

```
priority(OC) =  w_r · retirementBytes(OC)      // bytes its completion lets the ledger drop:
                                               // suppliers for which OC is the last
                                               // interested waiter, minus its own resultBytes
              + w_c · criticalPathRank(OC)     // upward rank over elemOps (HEFT-style)
              − w_g · growthBytes(OC)          // resultBytes it will newly retain
```

Effects, in order of importance:

1. **Consumers of large live intermediates run before unrelated producers** — this is
   precisely "sub-task groups that retain much internal memory are prioritized to
   complete before other groups start", now emergent instead of hand-fenced.
   (Formally: a greedy heuristic for min-peak-pebbling; see §7.)
2. Depth-first bias: with retirement dominating, the schedule follows chains to
   completion rather than fanning out breadth-first — consistent with the
   work-stealing space-bound argument (§7) and with the tile layer's existing LIFO
   stealing.
3. The dead `prioritize_impl` DFS (`OperationContext.cpp:2271-2341`) is revived in
   spirit: the supplier subtree of the item a joiner (GUI) waits for gets a
   criticalPath boost, restoring latency-focus for the interactive case.

Phase numbers stay as hard outer barriers (semantics unchanged); the priority order
applies *within* a phase.

### 5.3 Group scheduling

Cluster the runnable set by "shares a heavy live supplier" (union-find over supplier
edges weighted by `residentBytes`). Admission opens at most K clusters concurrently
(K small, e.g. 2); a cluster is "open" while any of its heavy intermediates is
retained. This is the coarse-grained complement of §5.2's fine-grained priority and
subsumes the *throttling* role of `PhaseContainer`: an open cluster ≙ an implicit
phase. Start with K as a registry knob; auto-derive later from B and cluster
estimates.

### 5.4 Parallelism shaping

- `tile_task_group` ctor (`OperationContext.cpp:233-269`) currently commissions
  `GetNrVCPUs() − running` threads regardless of chore weight. Use
  `workingSetBytesPerChore`: commission `min(cores, floor(B_tile / perChoreBytes))`
  threads for fat chores (the `reader_clone_farm` semaphore gets the same number for
  parallel reads).
- Make the MT3 pipelining gate real — and note §4.4 makes this a **three-way** choice,
  not two: eager, deferred (`FutureTileFunctor`, keeps its tiles) or streaming
  (`LazyTileFunctor`, frees them). The measurement says the interesting lever is
  promoting a consumed-once intermediate from *deferred* to *streaming*, which today
  only the `LazyCalculated` property does; the engine could infer it when the result has
  exactly one pipelined consumer and no `KeepData`/MMD requirement. That is a
  behavioural change, so it belongs after the ledger can measure it. Choose with honest
  byte weights — the comparison the stubbed `LTF_ElementWeight` was meant to make — plus a bound
  on in-flight tiles per pipeline (KPN backpressure, §7).
- `MaxAllowedConcurrentTreads() == 32` (`MainThread.cpp:447-450`) and the fixed pool
  remain; shaping only decides how many slots a given group occupies.

### 5.5 What deliberately does *not* change

Licensing/unique-run, suspend semantics (`SuspendTrigger` stays meta-thread-only),
interest-driven cleanup, cancellation, phase barrier semantics for explicit fences,
and the inline paths. The scheduler gets smarter about *order and admission*; it does
not take over memory management.

---

## 6. Automated phasing: estimates that depend on earlier results

### 6.1 The barrier taxonomy

For each OC at schedule time, `EstimateResources` classifies its numbers:

| Class | Example | Planner treatment |
|---|---|---|
| static-exact (`derived`) | elementwise ops, aggregations with ready partition units, storage reads after `ReadUnitRange` | plan directly |
| declared | unit/attr with `SizeExpectation` or `SizeUpperbound` (§4.6) | plan directly (reserve on declared bounds), flag for post-hoc verification |
| bounded | `select ≤ n`, `unique ≤ n`, product units = #A·#B | admit pessimistically OR defer (§6.3) |
| data-dependent (`assumed` + `dependsOnDataOf`) | count of a `select` result's *consumers*' inputs, iterative allocation state, polygon overlay output | **estimation barrier** |

### 6.2 Event-driven re-estimation

`disconnect_supplier` (`OperationContext.cpp:882-961`) already fires exactly when a
producer completes. Extend it: if the completing producer appears in a waiter's
`dependsOnDataOf`, re-run the waiter's estimate (counts are now `measured`:
`GetCount()` real, actual bytes from the ledger), re-rank it in the queue, and
re-evaluate admission. This is *mid-plan re-optimization at materialization points* —
Spark-AQE/POP-style (§7) — and it costs one virtual call per resolved dependency
edge.

The invariant that makes this cheap is the one `PhaseContainer`'s header comment
already states: a completed group's domain cardinalities are known facts usable to
plan "the front". We generalize it from "per explicit fence" to "per completed OC".

### 6.3 Auto-fencing

When a `bounded` estimate's upper bound busts the budget, the planner has two sound
choices; pick per confidence gap:

- **pessimistic-admit**: reserve the bound, run anyway (small gap);
- **defer-behind-producer**: hold the consumer group until the producer's actual
  count materializes, i.e. insert an *implicit phase edge* (large gap). This is a
  scheduling decision only — no copy tree, no new cache identity, unlike
  `PhaseContainer` — and it disappears automatically when the estimate resolves.

Together with §5.3's cluster cap, this reproduces the memory behavior modelers
currently buy with manual fences, but scoped, data-driven and reversible.
Declared `SizeUpperbound` bounds (§4.6) shrink the gap between bound and
expectation, converting defer-behind-producer cases into pessimistic-admit ones —
the modeler's expectation buys back parallelism.

### 6.4 What remains of `PhaseContainer`

Keep the operator; change its job description:

1. **Semantic sequencing & progress reporting stay explicit.** The message argument
   (progress trace per allocation iteration) and deliberate serialization of
   iterative model steps are modeler intent the scheduler cannot infer. Also its
   fence-copy identity semantics (consumers reference the *phase* results, not the
   sources) are load-bearing for cache identity in iterative models.
2. **Throttling use becomes unnecessary.** With §5/§6 active, a fence placed purely
   to bound simultaneous intermediates is redundant; document this in the operator
   docs and, once P4 (roadmap) is validated on RSopen, advise removing such fences
   (measure both ways on `Iter_T.dms`).
3. **Fix the known defects while touching it**: #1128 — commit phase results and
   emit the progress message when the phase's own `CalcResult` completes, not when
   the final consumer joins; #902 — targets that gain interest during/after phase
   execution must either join the running phase's `phase_resource` or schedule a
   follow-up mini-phase, instead of being silently late.
4. **`PhaseContainer` as calibrator**: an explicit phase boundary is a natural
   checkpoint where the ledger reconciles estimates against `GetFixedAllocStatus()`
   actuals — cheap ground truth for §4.7.

Automation extent, stated honestly: the *mechanism* (serialize groups, then plan the
front with known cardinalities) is fully automatable and this plan automates it; the
*placement intent* (which groups form a meaningful model step, what to log) is not,
and stays in the language.

---

## 7. Related work in the CS literature

The problem decomposes onto well-studied territory; the mapping, cluster by cluster:

**Problem frame.** Scheduling a DAG of tasks with per-task resource demands on
bounded resources is Resource-Constrained Project Scheduling (RCPSP) — NP-hard in
essentially every variant (Kolisch & Hartmann's surveys of priority-rule heuristics,
EJOR 1999/2006). Consequence: we are *right* to ship greedy priority heuristics with
feedback rather than seek optimal schedules.

**Peak-memory-minimizing DAG evaluation.** Register sufficiency / pebble games:
optimal evaluation order of an expression DAG under bounded memory is NP-complete
(Sethi, SIAM J. Comput. 1975); for *trees* the Sethi–Ullman numbering (JACM 1970)
is optimal and is the direct ancestor of §5.2's "finish the heavy subtree first".
Liu's tree-traversal results for multifrontal sparse factorization (1986/87) and
their modern parallel extensions — Jacquelin/Marchal/Robert/Uçar (IPDPS 2011),
Marchal et al. "Parallel scheduling of task trees with limited memory" (ACM TOPC
2015), Marchal/Simon/Vivien on limiting the footprint when dynamically scheduling
DAGs on shared memory (IPDPS/JPDC 2018-19) — study exactly our trade-off: parallelism
inflates the resident set; a *memory-booking* admission scheme (book a task's memory
before starting it, refuse when the booked total would exceed the budget) with a
guaranteed-progress fallback is their standard remedy and is precisely §5.1's ledger.
The memory-aware out-of-core multifrontal work (Agullo, Guermouche, L'Excellent)
is prior art for pairing such scheduling with spill-to-disk (our dormant
`IsFileableSize` / CalcCache spilling).

**List scheduling and its hazards.** Graham (1966/69): list scheduling is a
2-approximation for makespan, *and* exhibits anomalies — adding processors or
shortening tasks can lengthen the schedule. Consequence: throttling changes need
A/B wall-time measurements (roadmap exit criteria), not reasoning alone. HEFT
(Topcuoglu et al., IEEE TPDS 2002) supplies the upward-rank critical-path priority
of §5.2; Kwok & Ahmad (ACM CSUR 1999) survey the space.

**Work stealing and space.** Blumofe & Leiserson (JACM 1999): busy-leaves/DFS-order
work stealing bounds space by P·S₁ — the theoretical justification for depth-first
bias (§5.2) and for the tile layer's existing LIFO steal order. OpenMP task-creation
throttling / cutoff strategies (Duran et al., ICPP 2008) correspond to our chore
shaping (§5.4).

**Databases — the closest engineering analogue.** Cost-based planning from
cardinality estimates is Selinger et al. (SIGMOD 1979); its Achilles heel is
estimate error compounding through joins — "How Good Are Query Optimizers, Really?"
(Leis et al., VLDB 2015) — hence §5.1's preference for bounds + robustness over
point estimates. Memory grants per operator with *feedback* from observed usage
(SQL Server's memory-grant feedback) is exactly §4.7's loop. Mid-query
re-optimization at materialization points (Kabra & DeWitt, SIGMOD 1998; Markl et
al. POP, SIGMOD 2004; Graefe & Cole's dynamic plans; Eddies, Avnur & Hellerstein,
SIGMOD 2000) and Spark's Adaptive Query Execution — which re-plans at *stage
boundaries*, Spark's `PhaseContainer` moment — are §6.2/§6.3. Morsel-driven
parallelism (Leis et al., SIGMOD 2014) is the database twin of the tile/chore layer,
including adaptive degree-of-parallelism.

**Runtime systems.** StarPU (Augonnet et al., CCPE 2011) schedules from
*auto-calibrated per-codelet history-based performance models* — the direct
precedent for calibrated `m_CalcFactor` (§4.7); PaRSEC and Legion likewise separate
cost models from mapping. Dask's static task ordering is explicitly designed to
minimize resident intermediates ("run tasks that free memory first") and pairs with
spilling — the same two levers as §5.2 + CalcCache. Rematerialization /
gradient-checkpointing (Chen et al. 2016 sublinear-memory training; Griewank &
Walther's `revolve`, ACM TOMS 2000) formalizes recompute-vs-retain — relevant to
CalcCache policy for large intermediates with cheap producers.

**Pipelines/streaming.** Bounded scheduling of Kahn process networks (Parks' thesis,
1995) and reactive-streams backpressure justify bounding in-flight tiles per MT3
pipeline (§5.4) instead of unbounded lazy chains.

**Uncertainty.** Non-clairvoyant scheduling (Motwani et al. 1994) bounds what is
achievable with unknown task sizes; practical systems answer with the
measure-and-recalibrate loop we adopt rather than with worst-case guarantees.

---

## 8. Roadmap

Each phase is independently landable, flag-guarded, and validated on
`testcases/run_testcases.bat` (semantics) plus the tst regression battery
(t720/2BURP, t641/RSopen — wall time *and* peak commit via `GetFixedAllocStatus`
maxima; the perf-`.bin` infrastructure already records timings per test).

- **P0 — measure & mend (no behavior change). DONE** — see §8.1 for what it
  established. Fixed the `PerformanceEstimationData` defects (missing initializer,
  the mis-assigned aggregate return) and replaced the bits-vs-bytes muddle with
  `EstimateDataBytes(adi, nrElements)` (sub-byte packing included); deleted the inert
  `LTF_ElementWeight` and simplified its seven MT3 gates; unified the non-MSVC
  detached-thread path onto `portable_task_group`; instrumented the `CalcResult`
  payload and `StorageReadHandle::Read` under a new `MsgCategory::performance`, off
  unless the `PerformanceLogging` setting (or `/SP`) is on; the estimate is computed
  at schedule time and reported against the measured outcome, **consumed by nothing**.
- **P1 — the estimator.**
  Regime prediction and per-regime `residentBytes`/`choreBytes` (§4.4) +
  `EstimateResources` default (signature-derived where available) + family overrides
  (§4.3) + the `SizeUpperbound` property (§4.6: registration beside
  `SizeExpectation` — the renamed `SizeEstimator`, a clean rename since it is
  unused in production configs — plus the `EstimateCount` ladder rework,
  attr-side byte model, bound-violation warning) + storage-read estimates +
  calibration store
  (write-only). Exit: ≥ 80% of OCs in t720/t641 estimated `derived`/`declared`;
  median byte-estimate within 2× of actual; report of worst offenders drives the
  next overrides; a sparse-OD scratch config demonstrates a declared bound flipping
  an `assumed` whale to plannable.
- **P2 — admission throttle** (flag: new `RSF_` bit, e.g. "resource-aware
  scheduling", default off).
  Ledger + budget + progress rule + retry discipline + anti-starvation + whale-exclusive
  mode at the activation/licensing choke points. Exit: on a RAM-clamped run
  (`MemoryRAM_MAX_GB`), peak commit respects the budget on 2BURP-class tests with
  wall-time regression ≤ 10%; no new hangs across the battery (hang history: §1).
- **P3 — ordering.**
  Priority queue (retirement + critical path), joiner-subtree boost (revive
  `prioritize_impl`), cluster cap K. Exit: measurable peak-commit reduction on
  RSopen with fences *removed* in a scratch config, wall time neutral-or-better.
- **P4 — dynamic re-planning.**
  `dependsOnDataOf` barriers, re-estimation on `disconnect_supplier`, auto-fencing;
  `PhaseContainer` #1128/#902 fixes + eager phase commit. Exit: scratch RSopen
  without throttling-fences matches the fenced config's peak memory within 20%.
- **P5 — shaping & maturity.**
  Chore-count shaping from per-chore bytes; pilot-tile probing; persisted
  calibration warm-start; revisit CalcCache spilling (`IsFileableSize`) for whales;
  modeler-facing docs (when to still use `PhaseContainer`; authoring guidance for
  `SizeExpectation` (né `SizeEstimator` — currently an undocumented power
  feature) and `SizeUpperbound`, §4.6).

### 8.1 What P0 measured (and what it changes about the plan)

Landed as `rtc/dll/src/tic/PerfMeasurement.{h,cpp}` plus the touch-ups listed above.
Enable with `/SP` on any exe (or the `PerformanceLogging` registry DWORD); `/CP`
turns it off again. Verification: whole-solution Release build clean, all 186
`testcases/` cases pass unchanged, and with logging off the added cost is two relaxed
atomic loads per payload (measured wall time within run-to-run noise; enabling it cost
~3 ms on a ~50 ms run emitting 14 report lines).

Three findings that matter for P1–P5:

1. **At schedule time the result domain's count is usually *not* resolved.** On a
   plain `id → add → mul → float64 → div → sum` chain over a 3.2 M-element calculated
   unit, every elementwise operator estimated `assumed` confidence with the
   `ASSUMED_SIZE` = 1 M fallback, i.e. **3.20× under** the actual count; only the
   void-domain parameters came out `derived`. So the P1 exit criterion ("≥ 80 %
   `derived`/`declared`") cannot be met by better per-family formulas alone — it needs
   the §6.2 re-estimation at supplier completion, or a declared `SizeUpperbound`. This
   is the plan's central hypothesis, now measured rather than assumed.
2. **Estimation must degrade per field, never all-or-nothing.** Asking an uncomputed
   unit for `GetCount()`/`GetNrTiles()` throws; a single try/catch around the whole
   estimator turned that into an all-zero record that *looked* like a confident
   "costs nothing". `EstimateDomainCount` now isolates each query, and confidence is
   the **worst** over the result and all inputs.
3. **Elapsed time at the `CalcResult` boundary is not an operator's compute cost.**
   Pipelined operators return once the tile functor is built: the 3.2 M-element
   operators each reported ≤ 0.1 ms while the `sum` that pulled their tiles reported
   7.9 ms. Per-operator time therefore has to be measured per tile chore (§4.8), and
   until then the calibration of §4.7 can only be trusted for eager operators —
   equivalently, attribute compute per operator by running with `/C3`. Investigating
   this produced the regime model and the peak-memory measurements now in **§4.4**,
   which replace the plan's original "every intermediate costs `n × width`"
   assumption; that assumption would have over-reserved by 3.1× on the measured chain.

Also worth recording: the `[performance]` category shares the event log's "other"
filter checkbox, and the `RegStatusFlags` DWORD is **out of bits** (`RSF_WasRead` is
0x80000000), which is why `PerformanceLogging` is a `RegDWordEnum` entry and `/SP`
sets that rather than a status flag. P2's throttle flag will need the same treatment.

### 8.1.1 P2 step 1: the streaming residency is still unexplained

§4.4 makes explaining the streaming gap (542 MB measured against 40 MB predicted) the
precondition for P2's grant logic. First hypothesis tested and **rejected**:

*Hypothesis.* A `PrepareState` holds strong `shared_ptr<future_tile>` handles on the
operation's **arguments** (the `prepare_data` pairs in `clc/dll/include/OperAttr*.h`), and
`make_unique_FutureTileFunctor`'s lazy branch pre-builds them for **every** tile up front and
captures the whole array in the apply lambda (`TileFunctorImpl.h:137-146`). That pins every
upstream tile for the functor's life, so the streaming variant — whose purpose is to free
tiles — would retain more than the deferred one it replaces.

*Test.* Prepare each tile's state on demand inside the apply instead, so an argument tile
lives only while the tile consuming it is computed.

*Result.* No improvement: **598 MB** after the change against 542 MB before (deferred control
420 MB, matching its earlier 416 MB). Reverted.

*What it teaches.* The pre-built holder is not primarily a leak, it is a **memoization
anchor**: holding the argument's `future_tile` keeps that tile's computed data alive in the
upstream `tile_record`, so dropping the holder trades memory for recomputation — and here
bought no memory at all. Two consequences: (a) the dominant streaming residency is elsewhere
(the §4.4.1 consumer-skew term, per-thread accumulators in the pulling aggregation, and the
`sum` over five stages are the remaining candidates, in that order); (b) any future attempt to
bound retention by *releasing* holders must budget for the recompute it causes, which is the
argument for the synchronize-consumers option of §4.4.1 over the drop-holders one.

The admission gate is therefore **not implemented**: its estimate would be the one number
still known to be ~13× optimistic, and granting on it is exactly the failure mode §5.1 warns
about. What P2 needs next is a measurement that separates those three candidates — e.g. peak
with the aggregation replaced by a per-tile sink (isolates accumulators), and peak with a
single-consumer chain (isolates skew).

### 8.1.2 P2: the ledger and the gate (landed, default off, refusal path unvalidated)

`FileTileArray` no longer over-charged: `IsInMMD(res)` is exactly the file-backed predicate, so it
now yields a fifth regime **`spilled`** (`TicBase.h`), charged `inflight × choreBytes` for its live
mappings with the full volume booked as `ioBytes` instead. `FileTileArray::GetMaterialization()`
reports it, so prediction and measurement agree.

Admission (§5.1) is implemented at the licensing choke point:

- **Ledger** — `sd_LedgerCommittedBytes` / `sd_LedgerRunningOps` in `OperationContext.cpp`, charged
  in `getUniqueLicenseToRun` and released in `separateResources`, both already under
  `cs_ThreadMessing`. It books the *running* footprint only; retained-result accounting (the
  interest-release hook of §5.1) is not in yet.
- **Budget** — `TotalAllowedPhysicalMemory()` (new, `MemGuard.h`: RAM after the
  `MemoryRAM_MAX_GB` clamp) × `MemoryFlushThreshold`. The knobs that already claimed to throttle
  activation now do.
- **Charge policy — deliberately conservative.** Streaming's residency estimate is measured ~13×
  optimistic (§4.4), so booking it would admit a chain that then overruns. Until that is
  explained, `streaming` and `deferred` are both charged their **full result volume**; only
  `spilled` gets its in-flight discount, because there the data provably lives in a cache file.
  Over-charging can only refuse work that would have fit — a slowdown; under-charging is what
  causes the collapse this exists to prevent.
- **Progress guarantee** — when nothing else is running, the head of the queue runs whatever it
  costs. An over-budget estimate serializes the engine, it can never wedge it.
- **Mechanism on refusal** — `scheduleRunnableTask(self)` and return false, the #933 pattern that
  frees the worker instead of blocking it. `runDirect`/inline paths are never gated: they are
  today's correctness paths, and a meta thread withholding its own work would deadlock.
- **Flag** — `ResourceAwareScheduling` (`RegDWordEnum`, since `RegStatusFlags` is out of bits):
  0 = off (**default**), 1 = shadow (decide and log, never withhold), 2 = enforce.
  `/Sq` = shadow, `/SQ` = enforce, `/Cq`/`/CQ` = off.

**Validated:** builds clean; 186/186 testcases pass with the gate off; off/shadow/enforce each run
the 50 M-element chain to completion with no wall-time change (166/160/157 ms) and no hangs across
repeated enforce runs.

**Budget switch added, refusals forced, and the result is negative: this ledger cannot throttle a
pipelined chain.** `/SB<MB>` (`SchedulerBudgetMB`) overrides the budget for a run — deliberately a
ledger-only knob, because forcing refusals by clamping `MemoryRAM_MAX_GB` would also trip the
pre-existing `IsLowOnFreeRAM` activation brake and confound what is being measured. Two things came
out of driving it:

1. **The gate needs a gate-time estimate.** It was charging `m_Estimate`, the *schedule-time*
   figure, which is the blind `ASSUMED_SIZE` one (§8.1 finding 1) — 4 MB against a real 200 MB, so
   nothing ever exceeded any budget. `RefreshEstimateForAdmission()` now re-estimates in
   `TryRunningTaskInline` just before licensing, where suppliers are done and the domain is
   resolved, and deliberately outside `cs_ThreadMessing`.
2. **A leaked counter faked a win.** `MemoryLedger_Release` keyed on the byte count, but a
   legitimate charge can be 0 (void-domain result, no working memory), so those ops incremented
   `sd_LedgerRunningOps` and never decremented it. With the count stuck at 6 the progress guarantee
   never fired and the gate refused 35 601 times, serializing the run: peak 393 → 58 MB, wall
   206 ms → 5 932 ms. That looked like a 6.8× memory win and was a bug. Fixed with an explicit
   `m_LedgerBooked` flag.

With both fixed, the measured truth: **at a 1 MB budget, across 15 operations, zero refusals** —
because the operations in a pipelined chain never overlap, so `sd_LedgerRunningOps` is 0 whenever
one asks to start and the progress guarantee admits it whatever it costs. Peak is unchanged
(401 MB unthrottled vs 371 MB at a 200 MB budget — noise), and every run completes correctly.

The reason is structural: **the peak is caused by retained results of *completed* operations**, not
by concurrent running ones. Each `deferred` stage keeps its tiles while it is of interest (§4.4),
and the ledger booked only the running footprint.

**Retained-result accounting, and what it does and does not fix.** `MemoryLedger_Retain` books a
completed result's `residentMemory` on the item (`AbstrDataItem::m_LedgerRetainedBytes`) and
`MemoryLedger_ReleaseRetained` unbooks it at `ClearDataObject`, the one funnel a data object leaves
through. The counter is atomic rather than `cs_ThreadMessing`-guarded, because the release side runs
inside `ClearDataObject` under whatever locks its caller holds and must not invert lock order.
Admission now charges running + retained.

That still does nothing for a **linear** chain, and cannot: each stage must run to free its
predecessor, so refusing it would deadlock and the progress guarantee correctly admits it. **Depth
is not throttleable — breadth is.** On a wide probe (`scratch/wide.dms`: 8 independent
`id → mul/add → float64 → sum` branches over a 12 M domain, all consumed by one total):

| Run | Peak working set | Wall | Refusals |
|---|---|---|---|
| gate off | **371 MB** | 271 ms | 0 |
| enforce, `/SB400` | **222 MB** | 290 ms | 358 |
| enforce, `/SB200` | **218 MB** | 299 ms | 407 |

**~40 % peak reduction for ~7–10 % wall time**, every run correct, and refusal counts in the
hundreds rather than the tens of thousands the leaked-counter bug produced — the requeue path is
not spinning. Peak does not fall below ~218 MB even at a 200 MB budget: the progress guarantee
floors it at one branch's footprint, which is the intended behaviour.

Still open before recommending the flag — and note that **hysteresis is not among these**, see the
"Not hysteresis" bullet in §5.1; the refusal counts point at two different problems, which an
earlier draft mislabelled as churn:

1. **Retry discipline.** A refused task is re-queued and re-driven both by
   `StartOperationContexts` on completion *and* by the `Join`/`DoWorkWhileWaiting` poll loops, so
   it is re-tested when nothing about its budget situation has changed. Measured on the wide probe:
   358 refusals across **21 distinct targets** (≈17 re-tests each; the worst single task was
   refused 100 times) and 407 across 30 at `/SB200`. The fix is to retry on **release events**
   rather than on every re-drive — cheaper, and it does not change which tasks run.
2. **Starvation of large tasks — RESOLVED by drain mode** (see the §5.1 drain-mode bullet; user
   rulings 2026-07-30). The first task refused for budget becomes the claimant; growers are
   deferred while it waits, compressors keep flowing, and the refusal is lifted when nothing else
   runs or when a queue cycle shows no drain activity (generation unchanged). Measured on the
   mixed-size probe (`scratch/mix.dms`: one 30 M "whale" branch + six 3 M branches):

   | Run | Peak WS | Wall | refused | deferred | lifted |
   |---|---|---|---|---|---|
   | gate off | 245 MB | 227 ms | 0 | 0 | 0 |
   | enforce `/SB300` | 226 MB | 209 ms | 62 | 24 | 2 |
   | enforce `/SB200` | 242 MB | 178 ms | 93 | 18 | 5 |

   All runs complete correctly; the trace shows growers visibly deferred while a claimant waits
   and both lift arms firing. Two honest observations: the probe is too small for the peak numbers
   to mean much (its value is the demonstrated *ordering*), and the claimant is whoever is refused
   **first** — in the trace a small leaf, not the whale, because the leaves race at startup. The
   policy is size-agnostic by design; if protecting the *largest* refused task specifically ever
   matters, claim ownership could prefer the biggest charge, but nothing measured yet calls for
   that.

Also still open: retained bookings for results the scheduler did *not* admit (only admitted
operations are booked, so data already resident is invisible to the ledger), and a battery run in
enforce mode on a real model rather than a synthetic probe.

### 8.2 P1 status — complete

**Landed** (all with 186/186 testcases passing, nothing scheduling on any of it):

- Regime prediction and per-regime `residentMemory`/`choreMemory`/`nrChores` (§4.4),
  validated against measured peaks in all three modes.
- **Estimates taken twice** — at schedule time and again just before the payload, where
  suppliers are done and the domain is resolved. This is §6.2's mechanism proven in the
  small: `est n 1000000 / assumed / eager` at queue time becomes `n 50007071 / derived /
  deferred` at run time, exact. The run-time point is also where §5.1's gate sits, so it
  is the estimate a throttle would use.
- **`SizeEstimator` renamed `SizeExpectation`, `SizeUpperbound` added** (§4.6): both
  registered as `StoredPropDef`s, both readable from config (verified by `PropValue` +
  `IntegrityCheck` on a declaring unit), and `AbstrUnit::EstimateCount()` now returns
  `{expected, upperBound, confidence}` with the bound preferred for reservations and the
  expectation only informing ordering. `GetEstimatedCount()` is a thin wrapper, so
  existing callers are unaffected. The estimation vocabulary (`estimate_confidence`,
  `materialization`) moved to `TicBase.h` so units and storage managers can describe
  themselves without depending on the operator interface.

- **The `GetNew()` trap, fixed.** The estimator inherited `resultHolder.GetNew()` from the
  dead pre-P0 implementation. `GetNew()` is `MG_CHECK(!IsOld())`, so it **throws for every
  operator whose result is an existing item** (`oper_policy::existing` — `subitem` and
  friends), and the guard turned that into an all-zero record: `subitem` estimated
  `n=0 / assumed / eager` where the truth was `n=32,492,000 / derived / deferred, 496
  chores`. It now uses `GetUlt()` (falling back to `GetOld()`), which is also the item
  `RunOperator` measures, so estimate and actual describe the same item. Any operator
  family added later must not reintroduce `GetNew()` here.
- **Aggregation family override** (`clc/dll/include/OperAccUni.h`): the accumulator is
  working memory, not result memory — one slot per chore for a total, a whole
  partition-sized array per chore for a partitioned aggregation, times
  `MaxConcurrentTreads()`. A void-domain result is charged 0 bytes by design, which would
  have made the total-aggregation accumulator vanish, so it charges one slot minimum.
- **Storage reads now carry an estimate** (`EstimateReadResources`), taken before the read
  in `StorageReadHandle::Read` — reads are the one case where size is genuinely knowable up
  front, since `PrepareReadDataOrSuspend` has already resolved count and values range. The
  read line reports actual-vs-estimated bytes and MB/s per manager, which is the input a
  read cost model gets calibrated on.
- **Report format compacted.** `MsgDispatch` truncates a message at 256 characters, which
  was silently cutting off the schedule-vs-run comparison — the most informative part.
  Lines are now terse (`n=… (1.00x derived) B=123M deferred 496x256K res=123M | sched
  n=1000000 eager assumed`) and fit.

**Deliberately not in P1:**

- Lookup/sort/geometric family terms (index tables, sort buffers, superlinear complexity
  classes) — the base estimator scales work by the widest domain involved, which is honest
  for elementwise and aggregation shapes and an under-estimate for these.
- Feeding calibration back into the estimate (§4.7) — measurements are reported, not learned
  from, and per §8.1 finding 3 they can only be trusted for eager operators anyway.
- The consumer-skew retention term of §4.4.1: the estimator reports `nrChores`/`choreMemory`
  so the skew hypothesis can be tested, but it does not model skew, and the two footprint
  discrepancies of §4.4 (deferred over-reserves, streaming under-estimates ~13×) remain the
  gate on P2's grant logic.

---

## 9. Risks and open questions

**Risks**

- *Estimate error* is certain (Leis et al.). Mitigations are structural: bounds +
  the `IsLowOnFreeRAM` reality backstop + never-block-only-serialize.
- *Graham anomalies*: any admission/ordering change can slow specific DAGs. The flag
  stays user-visible until the battery shows consistent wins; per-config opt-out
  remains.
- *Deadlock/starvation*: the ledger must charge only what it admitted (inline paths
  exempt), and result-retirement observation must be conservative (unobserved
  release ⇒ ledger overestimates ⇒ safe direction). The t641_2/EnableAutoDelete
  history mandates full-battery + GUI-close testing for every P2+ change.
- *Meta-thread cost*: estimation runs where `CreateResultCaller` already runs; the
  signature-derived path avoids new allocations; `SizeExpectation`/`SizeUpperbound`
  rule evaluation must be bounded (they call `CalcCertainResult` — cap to
  parameter-sized expressions, as the existing error messages already demand).
- *Lock granularity*: priority queue and ledger live under the existing
  `cs_ThreadMessing`; operations must stay O(log n) with small constants, or the
  single-mutex design (§2.1) becomes the bottleneck.

**Open questions**

1. Working-set attribution when several OCs run concurrently — per-thread
   `FixedAlloc` tallies, or accept smeared attribution plus tile-level sampling?
2. Should `resultBytes` charging end at interest-release (correct, harder to
   observe) or at first-consumer-completion (observable, undercounts fan-out)?
   Start with the conservative former via the `TryCleanupMem` funnel.
3. Cluster cap K vs. per-cluster byte budgets — which is the better primary knob?
4. `SizeUpperbound` (§4.6) covers declared *size* expectations. Do we also
   want a declared *CPU-cost* annotation for black-box operators (external
   effects, `geos`), or is calibration alone sufficient there?
5. Spilling: revive swap-to-file for whales (the commented `IsFileableSize`), or
   rely on recompute-vs-retain against the CalcCache? The checkpointing literature
   (§7) suggests recompute wins when producers are cheap and I/O is the bottleneck.
6. Linux: `MemGuard`'s hard-coded 12% margin (`MemGuard.cpp:83-111`) should adopt
   the same budget model once P2 lands.

---

## Appendix A — key code anchors (quick index)

| Concern | Anchor |
|---|---|
| Estimation seed | `tic/Operator.h:41-49,86`, `tic/Operator.cpp:80-90`, `clc/dll/include/OperAttrUni.h:42-47` |
| Cardinality ladder | `tic/AbstrUnit.cpp:788-817` (`GetEstimatedCount`, `SizeEstimator`, `ASSUMED_SIZE`) |
| Declared-bound plumbing template (for `SizeUpperbound`, §4.6) | `tic/TicPropDefConst.h:37`, `tic/TreeItem.h:381-382,625`, `tic/TreeItem.cpp:1002-1016`, `tic/TreeItemProps.cpp` (registration) |
| Widths & tiling | `tic/AbstrDataItem.cpp:1265-1280`, `mci/ValueClass.h:170-177`, `tic/TiledRangeData.h:66-108` |
| Queue & phases | `tic/OperationContext.cpp:585-604,855-870,1021-1094,2362-2384` |
| License gate (re-queue pattern) | `tic/OperationContext.cpp:1552-1595` |
| Readiness edge (re-estimation hook) | `tic/OperationContext.cpp:882-961` |
| Low-RAM brake | `tic/OperationContext.cpp:1051-1067`, `utl/MemGuard.cpp:51-76` |
| Tile commissioning (shaping hook) | `tic/OperationContext.cpp:233-269`, `tic/ParallelTiles.h:138-161` |
| Memory lifecycle | `act/Actor.cpp:1197-1276`, `tic/DataLocks.cpp:155`, `tic/AbstrDataItem.cpp:846-896`, `tic/FreeDataManager.*` |
| Phase numbers | `act/Actor.cpp:1513-1547`, `tic/TreeItem.cpp:2803`, `tic/MoreDataControllers.cpp:601-616`, `tic/ItemLocks.cpp:56,96` |
| PhaseContainer | `clc/dll/src/PhaseContainer.cpp` (intent: `:27-30`), `clc/dll/src/SubItem.cpp:41-52`, issues #902/#1128 |
| Storage pre-read knowledge | `tic/stg/AbstrStoragemanager.cpp:71-80`, `stg/dll/src/gdal/gdal_grid.cpp:95-118,525-538`, `tic/AbstrDataItem.cpp:249-355`, `tic/TreeItem.cpp:4209-4360` |
| Measurement primitives | `mem/FixedAlloc.cpp:748-817`, `dbg/Timer.h:30-50` |
| P0 instrumentation (landed) | `tic/PerfMeasurement.{h,cpp}`, gate `utl/Environment.cpp` (`IsPerformanceLogging`, `/SP`), call sites `tic/OperationContext.cpp` (`RunOperator`, `ScheduleCalcResult`) and `tic/stg/AbstrStoragemanager.cpp` (`StorageReadHandle::Read`) |
