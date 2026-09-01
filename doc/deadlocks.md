# Deadlock analysis — lock inventory and (potential) deadlocks

*Static analysis of the GeoDMS source, 2026-09-01, at commit `182e66e9` (post-#1227: `...Lock()`
accessor naming, `DMS_ENTERS` ceilings, `DMS_CALLEE_ENTERS` callee contracts). **P1, P3 and P4
were fixed in `b591f683` (issue #1233) and are kept below, marked FIXED, as the record of what the
failure was.** Method: exhaustive
grep inventory of every synchronization primitive and every blocking wait, followed by reading the
wait structures and the sites where locks are held across opaque calls. Each finding states what
was actually read versus inferred. This is a reading of the code, not a proof: absence from this
list is not absence of a deadlock.*

Confidence vocabulary:

- **verified** — the participating code was read in this pass; the cycle (or its impossibility) follows from what it says.
- **structural** — the shape permits a cycle, but no concrete instance was found; listed so a future instance is recognized.
- **historical** — it happened, it is fixed, and it calibrates what the real failures here look like.

---

## 1. Lock inventory

### 1.1 Leveled sections (visible to the Debug lock-level checker)

The checker (`Parallel.h`: `level_type::Allow`, `EnterLevel`) permits acquiring only a *strictly
inner* (higher-ordinal) level, or an equal level when both sides are shared. It exists only under
`MG_DEBUG_LOCKLEVEL` (= `MG_DEBUG`); in Release, `dms_assert` is `CC_ASSUME` — every rule below is
unenforced in exactly the builds users run.

(ordinals as of `b591f683`; `UpdatingInterestSet` moved from 98 to 99 for #1233 P3)

| ordinal | name | instance | file |
|---|---|---|---|
| 1 | (SessionUsageCounter) | `s_SessionUsageCounter` (counted) | tic/SessionData.cpp |
| 93 | AbstrStorage, BoundingBoxCache1 | `cs_BB` | geo/GeoSupport.cpp |
| 94 | SpecificOperatorGroup, DataViewQueue, UpdateActionSet, Storage, BoundingBoxCache2 | `s_OdbcSection`, `sm_UAS`, polygon insert sections | odbc, shv, geo |
| 95 | SpecificOperator, DataRefContainer | `cs_SpatialRefBlockCreation`, `s_DataItemRefContainer`, polygon addition sections | clc, tic, geo |
| 96 | TileShadow | (tile machinery) | |
| 97 | Tile, ItemRegister, ThreadMessing | `cs_lock_map` per-item mutexes, `cs_ThreadMessing` | rtc/cs_lock_map.h, tic/OperationContext.cpp |
| 98 | CountSection, FailSection, OperContextAccess, ActiveProducerSet, TreeItemFlags, GDALComponent | `sg_CountSection`, `sc_FailSection`, `cs_OperContextAccess`, `s_ActiveProducerSetMutex`, `gdalSection` | act, tic, stg |
| 99 | IndexedString, UpdatingInterestSet, OperationContext, TileAccessMap, MoveSupplInterest, MOST_INNER (ExplainAccess) | token registry (counted), `sd_UpdatingInterestSet`, `cs_OcAdm`, `cs_lock_map` map lock, `sc_MoveSupplInterestSection`, `scs_ExplainAccess` | set, act, tic |
| 100 | RegisterAccess, CountedMutexSection, LispObjCache, NotifyTargetCount | `s_RegAccess`, `s_CountedMutexSection`, LispObjRegister CS, `sc_NotifyTargetCount` | utl, ptr, sym, act |
| 101 | ObjectRegister | `cs_ORT`, `cs_lockCounterUpdate` | mci, tic/ItemLocks.cpp |
| 102 | DebugOutStream | `g_DebugStream` | dbg/MsgDispatch.cpp |
| 103 | OperationQueue | | |

`FLispUsageCache` (was 98) is retired since `182e66e9` — see finding N6.

### 1.2 Bare primitives (invisible to the checker)

Over forty `std::mutex` / `std::shared_mutex` / `std::recursive_mutex` instances, all invisible
to the level discipline in both directions. The ones that matter for ordering (held across
non-trivial calls, or paired with a condition variable):

- `sd_SessionDataCriticalSection` + `s_IsSessionTearingDown` (tic/SessionData.cpp) — see rule R3.
- `sd_DataControllerMapCriticalSeciton` + `...WasRevisited` cv (tic/DataController.cpp) — finding P4.
- `s_TileTaskGroupsMutex` + `m_TileTasksDone` cv (tic/ParallelTiles) — finding P9.
- `FileMapHandle::m_ResizeMutex` (shared) + `tiledata.h cs_file` — not analyzed to completion (§5).
- `gdal_vect.h m_xSectionDataItemsStatusInfo` (recursive) — held across per-layer bookkeeping.
- `StoredPropDef::m_Mutex` ×3 — leaf map guards under `assert(IsMetaThread())`.
- `s_MainQueueSection` — queue swap only, released before opers run (verified).
- portable_task_group `m_mutex` + two cvs — pool internals, waits are predicate loops.
- FixedAlloc `allocSection` family — leaf; reporting deliberately happens after release (verified, N4).

### 1.3 Logical locks (wait-graph edges that are not mutex acquisitions)

These block threads but never pass through `EnterLevel`, so no static level assignment covers them:

- **Item production locks** — `TreeItem::m_ItemCount` counter + `cv_lockrelease` under
  `cs_lockCounterUpdate` (tic/ItemLocks.cpp). A negative count is a held write lock; waiting is a
  timed-cv predicate loop.
- **The token registry usage count** — `counted_mutex` (99): exclusive acquire parks *untimed*
  until the shared count reaches zero.
- **Task/supplier joins** — `OperationContext::Join`, `tile_task_group::Join`.
- **Interest counters** — the DemandManagement machinery.

---

## 2. The checker's blind spots

These are the reasons a level-order violation can exist without a Debug assert ever firing.
Recorded in issue #1227 §2; restated here because every finding below lives in one of them.

- **B1 — the item-level short-circuit.** `Allow()` returns true whenever
  `m_ItemLevel > other.m_ItemLevel`, before comparing ordinals. A thread that has taken any
  per-item lock (item level ≥ 1, e.g. via `Actor::DetermineLastSupplierChange`'s
  `MakeMax(m_ItemLevel, item_level_type(1))`) is thereafter *unchecked* against every level-0
  section — the registry, storage, everything.
- ~~**B2 — the explicit suppression.** `LevelCheckBlocker`.~~ **Gone** (#1233): its one caller
  was P3, and with that fixed the class is deleted rather than left for a future caller. An escape
  hatch that switches the ordering check off wholesale means the checker cannot see anything a
  blocked scope does, which is the opposite of what it is for; a section that genuinely may be
  taken in either order needs an ordinal that says so, not a way to stop asking.
- **B3 — bare mutexes** (§1.2) participate in no ordering at all.
- **B4 — logical waits** (§1.3) are blocking edges the checker does not model: a cycle through
  "thread A waits for item X's producer" is invisible however thorough the level discipline is.
- **B5 — Release enforces nothing.** Every historical deadlock here was met in Release. The two
  Release-alive guards are narrow: the registry's per-thread usage counter (self-deadlock only,
  since `208ab52f`), and `try_lock_for` on the teardown drain (#1191).

---

## 3. Findings — potential deadlocks

Ranked by (likelihood × cost of diagnosis when it fires). P1, P3 and P4 are fixed (#1233) and are
kept here as the record of what the failure was; the rest are open.

### P1 — `potential()` ordered accumulation: untimed wait with no exception path — **FIXED (#1233, `b591f683`)**

*Was:* [geo/dll/src/OperPot.cpp](../geo/dll/src/OperPot.cpp). Tile task `ti` waited, per
result tile, on `m_AddingProceeded.wait(lock, [..]{ return m_NrAddedTiles >= ti; })` — a **plain
`std::condition_variable::wait`**: no timeout, no `ASyncContinueCheck`, no cancellation predicate.
The ordering itself is sound — `parallel_tileloop` hands out tile numbers through a sequential
atomic, so tile `ti` only ever waits on tiles started before it. But if any earlier tile **throws**
(tile read error, cancellation surfacing through `ReadableTileLock`/`Calculate`, OOM), its
accumulation never happens, `m_NrAddedTiles` never reaches `ti`, and every later tile parks
forever; the task group's `Join` then never completes. The empty-tile branch waits identically.
What the user sees is the #1227 signature: a computation at ~0% CPU with no message.

*Fixed by* publishing the failure: the unwinding tile calls `AbandonAccumulation`, which sets a
shared flag and wakes every result tile, and `AwaitAccumulationTurn` returns false to any waiter
that sees it, so the waiter abandons its own accumulation without touching a counter. The result
is discarded either way — the original exception is stored by the task group and rethrown from
`Join` — so what mattered was only that no thread stays parked and that the real error is the one
reported. The wait is `wait_for` as well, so a lost notification cannot park either.

### P2 — cross-thread registry cycle: shared holder blocks on a producer that registers — **structural**

Thread A holds a `TokenStr` (registry-shared) and blocks — even in a *timed, retrying* wait — on an
item production lock (§1.3). Thread B, the producer of that item, calls `GetTokenID_mt` →
`GetOrCreateID_mt`, which parks **untimed** until the shared count is zero. A's predicate never
becomes true (B never finishes), B never wakes (A never releases): a two-thread cycle through one
leveled lock and one logical lock. The per-thread usage counter (`208ab52f`) catches only the
*same-thread* case; B1/B4 make this variant invisible to the checker.

No concrete instance is known. The #1227 renames are the practical defense: every registry-holding
value is now spelled `...Lock()`, so "held across a blocking call" is greppable. The sites that
deliberately keep lock accessors (the `DMS_*` C API, `createSimilarSet`, `Object::XML_Dump`) were
each verified not to block.

### P3 — the InterestReporter's suppressed equal-level pair — **FIXED (#1233, `b591f683`)**

*Was:* `InterestReporter::Report` ([AbstrDataItem.cpp](../rtc/dll/src/tic/AbstrDataItem.cpp))
nested `sg_CountSection(98)` → `sd_UpdatingInterestSet(98)` under a `LevelCheckBlocker`, because
Allow rejects an equal-level pair. The suppression meant the checker could never see a thread that
nests them the *other* way — a classic AB/BA with this reporter. Exposure was small (the reporter
exists under `MG_DEBUG_INTERESTSOURCE` only), but the pattern is the dangerous one: a silenced
checker plus a hand-picked order that nothing records as *the* order.

*Fixed by* giving `UpdatingInterestSet` its own ordinal at `CountSection + 1`, so the order the
reporter wants is the order the checker enforces. Verified safe both ways: all six of that lock's
sections are leaves (set a pointer, insert into or erase from `sd_InterestSet`; nothing is taken
inside them), and nothing at the new level is held when they are entered —
`AddTempTarget`/`ReleaseTempTarget` are sequential with `sc_MoveSupplInterestSection(99)`, never
nested. The blocker, now unused, is deleted; see B2.

### P4 — DataController-map revisit wait — **FIXED (#1233, `b591f683`)**

*Was:* [DataController.cpp](../rtc/dll/src/tic/DataController.cpp): a lookup that finds an expired
weak entry waited **untimed** on `...WasRevisited` for the dying DataController's destructor to
erase the entry. That is correct as long as the destructor runs on another thread, or later on
this one. Reached *from within* that same DC's destruction chain, the thread waited for its own
stack to unwind — not hypothetical in kind: the cache-unit teardown hang fixed by the DcRef
KeepAlive change was exactly a self-referential expiry in this neighborhood, and the
`MG_CHECK(IsMetaThread() || !mayCreate)` entry condition concentrates the risk on the meta thread.

*Fixed by* `~DataController` marking the key it is destroying on a per-thread stack, so the lookup
refuses that case with a named error (`invalid recursion: the DataController for {} is being
destructed by this thread`) instead of parking; the remaining wait is timed and reports which key
it is waiting for, so a missed notify or an absent destructor surfaces as a slow report rather
than a silent hang.

### P5 — equal-ordinal families — **structural, Release-only by construction**

Distinct mutexes sharing an ordinal (see §1.1: five sections at 94, five at 98, five at 99, four
at 100) can never be *nested* in a Debug-covered path — Allow rejects equal levels — so no
cross-order cycle can be built there. The residual risk is paths never run under Debug: in Release
nothing rejects the nesting, and any pair nested in both orders on different threads deadlocks
without a diagnostic. The 99 family is the one to watch: token registry, `cs_OcAdm`,
`cs_lock_map`'s map lock and `sc_MoveSupplInterestSection` all sit there, all exclusive.

### P6 — everything below a per-item lock is unchecked (B1) — **structural**

After taking any `cs_lock_map` per-item mutex, the short-circuit permits acquiring *every* level-0
section, including registering a token (registry-exclusive) — the self-case parks and is reported
by the Release counter, but a cross-thread interleaving (holder of registry-shared waiting on that
item) is P2 again, entered from the other side. The blind spot belongs in `Allow` itself; the
per-item locks having no fixable static level (they are map values, created on demand) is what
#1227 §2/§3 records.

### P7 — the registry-exclusive park is untimed while outer locks are held — **structural**

`counted_mutex::lock()` waits without a deadline. The waiting thread keeps every outer lock it
already holds; any shared holder that needs one of those to make progress closes a cycle. This
generalizes P2 beyond item locks to *any* lock held while calling `GetTokenID_mt`. The level
checker covers the leveled cases in Debug (the registry is 99; holding anything ≤ 99 while
registering is rejected) — B3/B5 are the gaps.

### P8 — `TContextNotification` runs inner to the registry — **verified, contract now annotated**

The progress callback is invoked from `ProgressMsg` while the caller holds
`sc_NotifyTargetCount(100)` ([TriggerOperator.cpp:53](../rtc/dll/src/act/TriggerOperator.cpp)) —
*inner* to the registry. A GUI implementation that reads a token, names an item, or synchronously
waits on the meta thread deadlocks or violates the order. Since `182e66e9` the typedef in
`DbgInterface.h` carries `DMS_CALLEE_ENTERS(ObjectRegister, exclusive)` and the rule: copy the
`CharPtr` out and post. Any regression here is a GUI-side review item, not detectable from rtc.

### P9 — `tile_task_group::AwaitRunningSlots` cannot steal — **noted limitation, low**

[ParallelTiles.cpp:434](../rtc/dll/src/tic/ParallelTiles.cpp) carries the code's own TODO: the
waiter deliberately does not steal other tile tasks (re-entrancy on
`FutureTileFunctor::tile_record::GetTile`'s mutex). The wait is timed (`WaitForTaskNotification`,
500 ms re-check) and all slots of the group are commissioned before waiting, so completion depends
only on already-running threads; a cycle needs a slot function that itself joins something waiting
on this group — group joins follow creation scope, which is acyclic. Kept on the list because the
TODO marks the invariant as load-bearing and unchecked.

### P10 — foreign-library locks — **structural, interface rule**

GDAL's CPL locks, PROJ contexts, FFTW's plan mutex (`g_fftwPlanMutex`,
`planCacheMutex`), and COM (`CompoundStorageManager`'s `IStream`) are invisible to the level
system, and our sections (`gdalSection(98)`, `s_OdbcSection(94)`, SpatialRefBlock(95)) are held
across calls into them. No cycle was found: the reverse edges would require a foreign callback
re-entering GeoDMS lock-taking code, and the only such callback (the GDAL/CPL error handler)
reaches only `DebugOutStream(102)`, inner to everything held. The rule to preserve: a foreign
callback may report, and nothing else.

---

## 4. Verified non-findings

Recorded so the next reader does not re-suspect them:

- **N1** — Main-thread and Join waits are all timed (500 ms) predicate loops through
  `WaitForTaskNotification`, which on the main thread uses `MsgWaitForMultipleObjectsEx` with
  timeout precisely to stay pumpable and un-ghosted (#1156, #659). Starvation shows as livelock
  with progress checks, not silent deadlock.
- **N2** — `MsgDispatch` moves the flush pipeline out of the `DebugOutStream` lock before invoking
  `MsgCallbackFunc` sinks; msg callbacks run lock-free on the meta thread, re-entrance blocked
  per sink.
- **N3** — `SendMainThreadOper` never blocks off-thread (it posts); the oper queue's own mutex is
  released before opers run, and since `182e66e9` `operation_queue::Process` *asserts* the pump
  holds no leveled section (`CurrentThreadHoldsNoLevelLock`).
- **N4** — FixedAlloc's huge-alloc attribution report deliberately runs after
  `AllocateFromStock_impl` returns, with no allocator lock held (comment at
  [FixedAlloc.cpp:1165](../rtc/dll/src/mem/FixedAlloc.cpp)); the allocator↔registry inversion this
  analysis went looking for is designed away.
- **N5** — `AsString(LispPtr)` streams through `Print` into a local buffer, lock-free; only
  `AsFLispSharedStr` ever took the FLisp cache lock.
- **N6** — `AsFLispSharedStr`'s guarded global buffer *was* a latent inversion (its guard sat outer
  to the registry while printing reads symbol names); since `182e66e9` the buffer is thread_local
  and the level is retired.
- **N7** — `parallel_tileloop`'s sequential handout makes OperPot's tile ordering itself sound
  (P1 is about the exception path, not the ordering).
- **N8** — Registering a token while holding one's own `TokenStr` no longer parks: reported with a
  named error since `208ab52f`, in Release too.

---

## 5. Not analyzed to completion

- The file-mapping pair `FileMapHandle::m_ResizeMutex` (shared) ↔ `tiledata.h cs_file`, held
  across resize/page-in; a full nesting table of the mem/ser tile paging layer was not built.
- The shv/GUI side beyond the callback contracts (DataView queues at 94, Qt event-loop interplay).
- Interest-count machinery internals (`MoveSupplInterest(99)`, `UpdatingInterestSet(98)`,
  `sg_CountSection(98)`) beyond the P3 site: the pairwise order table for the act/ layer is
  unwritten.
- Storage managers other than odbc/gdal/cfs.

## 6. Historical deadlocks (fixed) — calibration

| what | mechanism | fix |
|---|---|---|
| fn_test_shadow hang (2026-07-14) | TokenStr held across body-item parsing (self-park on registry) | materialize; #1227 renames |
| #1226 export dialog | `GetName()` TokenStr held across meta-info update | materialize; now reported |
| gdal_vect field creation | TokenStr vs `GetTokenID_mt` in write path | hand-scoped, then materialized (#1227) |
| #1191 teardown | session-usage drain could park at exit | `counted_mutex::try_lock_for` give-up drain |
| DiscrAlloc teardown hang | `EnableAutoDelete` worker-drain waited on drained workers | drain order fix |
| cache-unit expiry hang | parentless cache unit weak-expired during use → self-referential wait | DcRef KeepAlive |
| #659 exec_ec | `MsgWaitForMultipleObjectsEx` without a pump | pump added; rule in N1 |
| #1156 window ghosting | cv wait invisible to user32 (not a deadlock, looked like one) | `MsgWaitForMultipleObjectsEx` wait |

## 7. Standing rules that keep the list short

- **R1** — never hold a `...Lock()` value (TokenStr/TokenStrRange) across a call that can
  tokenize or block; materialize first (`Object.h`, `sym/Token.h`).
- **R2** — the error-reporting path reads names and streams, nothing else:
  `DMS_ENTERS(IndexedString, shared)` on `Describe`/`GetDescription`, contracts on everything they
  dispatch to (`DebugContext.h`).
- **R3** — teardown-concurrent code gates on the lock-free `IsSessionTearingDown()`, never on
  `SessionData::Curr()` (which takes `sd_SessionDataCriticalSection` while the tree dies under it).
- **R4** — main-thread opers are unconstrained *because* every pump holds nothing — asserted in
  `operation_queue::Process`.
- **R5** — a foreign callback (GDAL error handler, progress notification) may report or post;
  it may not name items, take DMS locks, or wait.

## 8. Follow-up work, in order of value

1. ~~Retire P1, P3, P4~~ — done in `b591f683` (#1233), which also deleted `LevelCheckBlocker`
   and with it blind spot B2.
2. Fix B1 in `Allow` (compare ordinals when item levels are equal *or* incomparable). This is now
   the largest remaining hole: it is what leaves P6 — and the P2 family entered from the item-lock
   side — invisible to the checker.
3. Build the pairwise nesting table for act/ (interest machinery) and mem/ser (tile paging) — the
   two layers §5 leaves open.
4. The syntactic pass over `DMS_ENTERS` / `DMS_CALLEE_ENTERS` declarations (#1227 §3) — the static
   half that would make §2's blind spots enumerable instead of remembered.
