# Deadlock analysis — lock inventory and (potential) deadlocks

*Static analysis of the GeoDMS source, 2026-09-01, at commit `182e66e9` (post-#1227: `...Lock()`
accessor naming, `DMS_ENTERS` ceilings, `DMS_CALLEE_ENTERS` callee contracts). **P1, P3 and P4
were fixed in `b591f683` (issue #1233) and are kept below, marked FIXED, as the record of what the
failure was; B1 was fixed -- and its earlier statement corrected -- in `d9d791ba`.** Method: exhaustive
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

(ordinals as of `e2c6033c` — every section has its own since #1233 P5; the former families are noted)

| ordinal | name | instance | file |
|---|---|---|---|
| 1 | SessionUsageCounter | `s_SessionUsageCounter` (counted; raw `try_lock_shared`/`unlock_shared`, invisible to `Allow`) | tic/SessionData.cpp |
| 2 | WmsTileCache | `TileCache::s_ImageAccess` | shv/WmsLayer.cpp |
| 50 | (ItemProductionWait) | **not a section**: the per-item ceiling a production *wait* declares — §3.8 | tic/ItemLocks.cpp, tic/OperationContext.cpp |
| 60 | BoundingBoxCache1 (was 93) | `cs_BB` | geo/GeoSupport.cpp |
| 61 | AbstrStorage (was 93) | | |
| 62 | SpecificOperatorGroup (was 94) | polygon-overlay insert sections | geo |
| 63 | DataViewQueue (was 94) | | shv |
| 64 | UpdateActionSet (was 94) | `sm_UAS` | shv/GraphicObject.cpp |
| 65 | Storage (was 94) | `s_OdbcSection` | stg/odbc |
| 66 | BoundingBoxCache2 (was 94) | | |
| 68 | SpecificOperator (was 95) | `cs_SpatialRefBlockCreation`, polygon addition sections | clc, geo |
| 69 | DataRefContainer (was 95) | `s_DataItemRefContainer` | tic/AbstrUnit.cpp |
| 70 | PrepareDataUsageLock (was 96) — per-item | `sg_PrepareDataUsageLockMap` | tic/TreeItemDataUsage.cpp |
| 72 | TileShadow (was 96) | tile machinery | |
| 73 | Tile (was 97) | tile machinery; may schedule or join operation contexts, so outer to ThreadMessing | |
| 74 | ItemRegister (was 97) — per-item | `sg_ActorLockMap` | rtc/cs_lock_map.h |
| 75 | ThreadMessing (was 97) | `cs_ThreadMessing`; takes CountSection, FailSection, OperContextAccess inside | tic/OperationContext.cpp |
| 76 | DataFlagsLock (was 98) — per-item | `sg_DataFlagsLockMap` | tic/AbstrDataItem.cpp |
| 78 | CountSection (was 98) | `sg_CountSection` | act/Actor.cpp |
| 79 | FailSection (was 98) | `sc_FailSection`; takes MoveSupplInterest inside | act/Actor.cpp |
| 80 | OperContextAccess (was 98) | `cs_OperContextAccess`; taken under ThreadMessing | tic/MoreDataControllers.cpp |
| 81 | ActiveProducerSet (was 98) | `s_ActiveProducerSetMutex` | tic/ItemLocks.cpp |
| 82 | TreeItemFlags (was 98) | | |
| 83 | GDALComponent (was 98) | `gdalSection`; its error handler may read tokens and report | stg/gdal/gdal_base.cpp |
| 85 | UpdatingInterestSet (was 99) | `sd_UpdatingInterestSet`; taken under CountSection | act/TriggerOperator.cpp |
| 86 | OperationContext (was 99) | `cs_OcAdm` | tic/OperationContext.cpp |
| 87 | TileAccessMap (was 99) | | |
| 88 | MoveSupplInterest (was 99) | `sc_MoveSupplInterestSection`; takes NotifyTargetCount inside | act/Actor.cpp |
| 89 | ExplainAccess (was 99) | `scs_ExplainAccess` | tic/Explain.cpp |
| 90 | IndexedString (was 99) | the token registry (counted): shared to read, exclusive to register — innermost of its former family, so a token can be read under any of them | set/IndexedStrings.cpp |
| 92 | NotifyTargetCount (was 100) | `sc_NotifyTargetCount`; the TContextNotification callback runs under it | act/TriggerOperator.cpp |
| 93 | RegisterAccess (was 100) | `s_RegAccess` | utl/Environment.cpp |
| 94 | LispObjCache (was 100) | LispObjRegister CS | sym |
| 95 | CountedMutexSection (was 100) | `s_CountedMutexSection`; every `counted_mutex` op takes it | ptr/SharedBase.cpp |
| 97 | ObjectRegister (was 101) | `cs_ORT` | mci/MciInterface.cpp |
| 98 | ItemCounter (was 101, shared with ObjectRegister) | `cs_lockCounterUpdate`: the item production read/write counter | tic/ItemLocks.cpp |
| 100 | DebugOutStream (was 102) | `g_DebugStream` | dbg/MsgDispatch.cpp |
| 101 | OperationQueue (was 103) | | |

`FLispUsageCache` (was 98) is retired since `182e66e9` — see finding N6.

The three `cs_lock_map` instances carry ordinals `PrepareDataUsageLock` 70, `ItemRegister` 74
(`sg_ActorLockMap`) and `DataFlagsLock` 76, but they live in the *per-item* dimension, where they
are outer to every global section and are not ordered against each other (§3.2) — so today those
ordinals are never compared; they only record the one same-item nesting that is known,
`PrepareDataUsage(X)` enclosing `DataWriteLockAtom(X)`. Since `d9d791ba` these locks enter the checker;
before that they never did (B1).

### 1.2 Bare primitives (invisible to the checker)

Over forty `std::mutex` / `std::shared_mutex` / `std::recursive_mutex` instances, all invisible
to the level discipline in both directions. The ones that matter for ordering (held across
non-trivial calls, or paired with a condition variable):

- `sd_SessionDataCriticalSection` + `s_IsSessionTearingDown` (tic/SessionData.cpp) — see rule R3.
- `sd_DataControllerMapCriticalSeciton` + `...WasRevisited` cv (tic/DataController.cpp) — finding P4.
- `s_TileTaskGroupsMutex` + `m_TileTasksDone` cv (tic/ParallelTiles) — finding P9.
- `FileMapHandle::m_ResizeMutex` (shared) + `tiledata.h cs_file` — not analyzed to completion (§6).
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

- **B1 — per-item locks: invisible until `d9d791ba`, and still unordered against each other.** The
  earlier statement of this spot ("the item-level short-circuit") was wrong in an instructive way:
  `Allow`'s item-level rules had **never executed**. `cs_lock_map::Lock` called `m_Lock.lock()` on
  the `std::mutex` directly, bypassing `scoped_lock_impl` — the only place `EnterLevel` runs — so
  the three per-item maps (`sg_ActorLockMap`, `sg_DataFlagsLockMap`, `sg_PrepareDataUsageLockMap`)
  were as invisible to the checker as a bare mutex (B3), and every ordinal it ever compared was
  between two item-level-0 sections. Since `d9d791ba` a per-item lock enters the checker like any
  scoped lock. What that checks: a global section or a ceiling held while a per-item lock is
  taken is refused, and everything taken under a per-item lock is checked as before. What it
  deliberately does **not** check: two per-item locks against each other. Their nesting follows
  the interest recursion (`IncInterestCount` holds the consumer while `StartInterest` takes the
  suppliers), and the item levels do not track that relation — measured under cdb: a values unit at
  level 3 taking its supplier DataController at level 5, because `DetermineLastSupplierChange`
  folds in a different supplier set than `StartSupplInterest` visits. What keeps that nesting
  acyclic is the supplier DAG itself, which no level can see. The other residue: a per-item lock
  whose item reports level 0 — a passor (every base unit is one) or an item whose state has not
  been determined — is not entered at all.
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
- **B6 — a lifetime is not a scope.** A `TokenStr` argument temporary,
  `reportF(st, "{}", x.GetNameLock().c_str())`, holds its registry usage to the end of the full
  expression: across `mgFormat2string`, `reportD`, the `DebugOutStream` section and the post. A
  ceiling is scope-shaped and no `DMS_ENTERS` on either side can state that (Parallel.h, "Two
  things it deliberately does not do"). It is not an order violation today: the report funnel
  (`reportD_without_cancellation_check_impl`) declares `(IndexedString, shared)`, which admits a
  shared holder and refuses everything a holder could conflict with — an acquire ≤ 89, a
  registration, a production wait — and `IndexedString_shared_lock` is a
  `RequestMainThreadOperProcessingBlocker`, so the msg sinks never run inline under the holder.
  Only the `TokenID` form ends its usage inside the format call (`mgFormatArg` renders it through
  its `mgFormatArgOf` opt-in: one registry read into an SSO-sized `std::string`, no stream, no
  allocation for a short name — also the cheapest form, ~8 ns against ~25 ns for an
  `AsSharedStr().c_str()` and ~130 ns for the `std::ostringstream` detour a `TokenID` took before
  the opt-in); a `TokenStr` form is the caller's temporary. Covered statically, before any
  build, by `tools/check-lock-across-sink.ps1` (run by `analyze.bat`), which flags a `...Lock(`
  accessor in the argument text of any format sink. At a throw-family sink the span is harmless
  (after the format nothing runs in that frame but the throw, and unwinding destroys the
  temporary first) but it is flagged all the same: the `TokenID` form is shorter and cheaper.
  The form to write is `reportF(st, "{}", item->GetID())`. Survey of 2026-09-04: 2 report-family
  and 68 throw-family sites, all rewritten to pass the `TokenID` (74 call sites counting the
  `mySSPrintF` ones), 0 named locals spanning a sink.

---

## 3. What a ceiling permits — the exact rules

*This section answers: given `DMS_ENTERS(L, dms_shared_v)` or `DMS_ENTERS(L, dms_exclusive_v)`,
what may the scope and everything it calls actually do? It is derived from `level_type::Allow`
in `Parallel.h`, which is the whole of the enforcement.*

### 3.1 What a ceiling is

`DMS_ENTERS(L, MODE)` constructs a `DmsLockCeiling`, which calls `EnterLevel` with
`level_type{ descr, L, item_level_type(0), MODE, m_IsCeiling = true }` and restores the previous
level on scope exit — the same push/restore `scoped_lock_impl` performs. It takes **no mutex**. It
only makes the thread *claim to hold* `(ordinal L, mode MODE, item level 0)` for the rest of the
scope, so that every subsequent acquire is checked against that claim. It exists only under
`MG_DEBUG_LOCKLEVEL` (= `MG_DEBUG`); in Release the macro is `((void)0)` and none of this section
applies.

Since `2052ee75` a ceiling is **marked** as such (`m_IsCeiling`), and there is a second form:
**`DMS_ENTERS_ITEM(L, MODE)`** enters the same claim at item level 1. It is the declaration for a
scope whose outermost acquire is a *per-item* lock (`cs_lock_map`) — taken directly, or through
the interest pointers the scope creates and destroys, since `IncInterestCount`/`DecInterestCount`
take the item's actor lock. The value 1 is a sentinel: two per-item levels are never ordered
(rule 3), so one value serves every such function. The `descr` of both forms now carries
`__FILE__:__LINE__` of the declaring site, and `EnterLevel` prints both sides of a refusal
(`lock level refused: held ... -> requested ...`) to stderr before the assert, so a battery
`.out` names the declaration and the acquire that disagreed — without that line the checker's
verdict was a bare `Allow(level)` assert and a cdb session per case.

A ceiling is itself an acquire for checking purposes: declaring one while something is already held
is checked by the same rules, and so is nesting one ceiling inside another.

### 3.2 The decision procedure

`held.Allow(target)` — `held` is the thread's current level (a real lock *or* a ceiling), `target`
is what is about to be taken. In order:

1. `target.ordinal == 0` — asserted against; a section must have a real ordinal.
2. **Nothing held** (`held.ordinal == 0`) → allow.
3. **Both per-item** (`held.item ≠ 0 && target.item ≠ 0`) → allow: two per-item locks are not
   ordered by the checker at all (B1, since `d9d791ba`).
4. **`held.item > target.item`** → allow — a per-item lock is outer to every global section, and
   the ordinal is not consulted.
5. **`held.item < target.item`** → refuse — no global section or ceiling may be held when a
   per-item lock is taken.
6. Equal item levels (both 0 in practice) → compare ordinals: `held < target` allow;
   `held > target` refuse; **equal**: if `held` is a real lock → allow only if BOTH are shared; if
   `held` is a **ceiling** → allow when `target` is shared, or when the ceiling itself is exclusive
   (`target.shared || !held.shared`, since `2052ee75`). That is: a ceiling admits its own declared
   acquire at a mode no stronger than declared, so a function declares *exactly* the outermost
   acquire it makes and nothing has to be stated one level off. Once the lock is really taken it
   sits over the ceiling and the strict rule applies again.

Until `d9d791ba` the item dimension had never been exercised: nothing that reached `Allow` carried an
item level other than 0, because the per-item maps bypassed `EnterLevel` (B1). A per-item lock now
enters with `GetItemLevel(item)`, non-zero once the item's state has been determined; every global
section and every ceiling uses `item_level_type(0)`. Only zero versus non-zero carries meaning: a
per-item lock is **outer** to every global (rule 4 permits any global under it, rule 5 refuses it
under any global or ceiling), and two per-item locks are not ordered at all (rule 3).

Why not: the obvious refinement — order per-item locks by item level, consumer outer to supplier —
was tried and measured false. `IncInterestCount` holds the consumer's actor lock while
`StartInterest` takes the suppliers', which is the right direction, but `DetermineLastSupplierChange`
computes the level over the `DetermineState` supplier set while `StartSupplInterest` visits a
different one, so a supplier can sit *deeper* than its consumer (a values unit at 3 taking its
DataController at 5). The nesting is acyclic because the supplier DAG is, and that is the only
thing that guarantees it; a level rule would refuse legitimate nestings and prove nothing.

An item that reports level 0 — a passor or a not-yet-determined item — is not entered into the
checker at all: judged as a global it would refuse a level-1 attribute preparing its passor values
unit. In that one respect the check is still a property of the data.

### 3.3 The table

Ceiling `(L, MODE)` — since a ceiling always carries item level 0, rules 3 and 4 fix the item
column before ordinals are ever reached:

| what the scope (or anything it calls) takes | under `(L, shared)` | under `(L, exclusive)` |
|---|---|---|
| global section, ordinal **M > L**, either mode | allowed | allowed |
| global section, ordinal **M == L**, **shared** | **allowed** | **allowed** (the ceiling's own acquire, weaker mode) |
| global section, ordinal **M == L**, exclusive | refused | **allowed** (the ceiling's own acquire) |
| global section, ordinal **M < L**, either mode | refused | refused |
| per-item lock with item level **≥ 1** | refused | refused — unless the ceiling is `DMS_ENTERS_ITEM`, which admits it (rule 3) |
| per-item lock whose item reports level 0 (passor / undetermined) | **not entered — invisible**, as before (B1) | idem |
| a nested ceiling `(M, mode2)` | same rules as a global section at `(M, mode2)` | idem |
| a bare `std::mutex` / `shared_mutex` / `recursive_mutex` | **invisible — never checked** | idem |
| a blocking wait (`Join`, item production lock, any cv) | **invisible — never checked** | idem |

So for *callees* the two ceilings differ in exactly one row — whether a callee may still take `L`
shared — while for the scope's *own* acquire of `L` both admit it (an exclusive ceiling admits
either mode, a shared ceiling only shared).

### 3.4 The exclusive ceiling is the STRICTER one

This is the counterintuitive part and the one most likely to be got backwards. The mode is **not**
"the mode in which I intend to take `L`". It is "the mode in which I am to be treated as *already
holding* `L`". Holding a lock exclusively means nobody — including this thread — may take it again;
holding it shared means this thread may still take it shared. Therefore:

- `DMS_ENTERS(L, dms_shared_v)` = *"nothing outer than `L`, and `L` itself read-only."*
- `DMS_ENTERS(L, dms_exclusive_v)` = *"strictly inner to `L`."*

and the exclusive form forbids a strict superset of what the shared form forbids.

Two consequences worth writing down:

- **Declare exactly the outermost acquire.** Until `2052ee75` a function that took `L` itself could not
  declare `(L, exclusive)` — `Allow` rejected its own acquire — and the rule was "declare only what
  you do not yourself take". That rule is retired: a ceiling admits its own declared acquire (rule
  6), so `GetOrCreateID_mt` declares `(IndexedString, exclusive)`, `TokenID::GetStrLock` declares
  `(IndexedString, shared)`, and a function whose outermost acquire is a per-item lock declares
  `DMS_ENTERS_ITEM`. The alternative that was considered and rejected — "if you take `L`
  exclusively, declare `L-1`" — is off by one: a caller that legitimately holds exactly `L-1` would
  be refused at the call while the acquire itself is in order.
- **`DMS_ENTERS_NOTHING`** is `(EntersNothing = 0xFFFFFFFF, exclusive)`. No real section has an
  ordinal above it, so rule 5 refuses every acquire at equal item level and rule 4 refuses every
  per-item lock: the scope may take nothing at all.

### 3.5 The ceilings actually in use

Since `2052ee75` every function that directly acquires a leveled section and does not transitively pump
main-thread opers carries its ceiling (118 declarations, battery 246/247 — see §3.7 for what the wave
established). The two that carry the semantics worth knowing by heart:

- **`DMS_ENTERS(ord_level_type::IndexedString, dms_shared_v)`** — the reporting path
  (`AbstrMsgGenerator::Describe`, `MsgGeneratorPolicy::GetDescription`, `ConfigProd::Describe`),
  `Object::GetFullName` and the raw `AbstrPropDef` accessors. With `IndexedString` at 90: reading a
  token (90 shared) is allowed, **registering** one (90 exclusive, i.e. `GetOrCreateID_mt`) is
  refused, everything from 92 up is allowed (`NotifyTargetCount`, `RegisterAccess`, `LispObjCache`,
  `CountedMutexSection`, `ObjectRegister` 97, `ItemCounter` 98, `DebugOutStream` 100), and
  everything ≤ 89 is refused — `ExplainAccess` 89 down through `MoveSupplInterest` 88, GDAL 83,
  count/fail 78/79, thread-messing 75, tile 73, storage 65 — as is any per-item lock.
  That is precisely "may name things and may report; may not compute, store or intern".
- **`DMS_ENTERS_NOTHING`** — currently declared only as a callee contract
  (`DMS_CALLEE_ENTERS_NOTHING` on `Object::GetID`, `GetLocation`, `PersistentObject::GetParent`),
  not as a scope ceiling.

### 3.6 What a ceiling does not and cannot say

- **Bare mutexes are invisible.** A ceiled scope may take `StoredPropDef::m_Mutex`,
  `s_TileTaskGroupsMutex`, `FileMapHandle::m_ResizeMutex` or any of the forty-odd others of §1.2
  and the checker will not object, in either direction.
- **Blocking is invisible.** Nothing stops a ceiled scope from calling `OperationContext::Join`,
  waiting on an item production lock, or parking on a condition variable. P1 is the proof: an
  untimed `cv.wait` that hangs the whole operation involves no leveled lock at all, so no ceiling
  anywhere would have caught it. A ceiling constrains **lock order**, never **liveness**.
- **Release enforces nothing** (B5).
- **A `DMS_CALLEE_ENTERS` annotation is a declaration, not a check.** It states what a virtual,
  callback or opaque function is permitted to reach, so a call site can be checked against it by
  the static pass that does not exist yet (#1227 §3). Today it bites only when the callee actually
  runs under a caller's `DMS_ENTERS` — which is why the annotations and the ceilings are worth
  having together rather than either alone.

### 3.7 Choosing the level when annotating something new

The default, for a function with no declaration, is that it may take any lock. A declaration
narrows that, and is checked on entry against what the caller holds. The rules the first wave
(`2052ee75`) settled:

1. Find the **outermost** (numerically lowest) leveled section the scope may reach, directly or
   through anything it calls. Declare exactly that: `(L, exclusive)` if it is ever taken
   exclusively, `(L, shared)` if only ever shared. When in doubt between two candidates declare the
   **outer** one — a weaker promise that is still true beats a stronger one that a rare path
   breaks.
2. If any path takes a **per-item lock** — directly, or by constructing or destroying an interest
   pointer (`SharedTreeItemInterestPtr`, `garbage_can` contents, an `OldDcInterestDecrementer`),
   or by dropping a `DataReadLock` — declare **`DMS_ENTERS_ITEM(ItemRegister, exclusive)`**
   (`DataFlagsLock` / `PrepareDataUsageLock` for the other two maps), whatever global sections the
   scope also takes: a per-item lock is outer to every global, so that is the outermost. This is
   why the `OperationContext` finishers (`OnEnd`, `HandleFail`, `CancelIfNoInterestOrForced`,
   `CollectOperationContextsImpl`, `FindAndLicenceOnePriorityTasks`, `~tg_maintainer`) are
   per-item and not `ThreadMessing`: each releases interest-holding garbage after its lock.
3. A scope that **pumps** main-thread opers — `Join`, `DoWorkWhileWaiting`, `ReadLockInit`,
   `lock_shared` of the item counter, `AwaitAncestorWrites`, `UpdateMetaInfo`, and everything
   above them — cannot carry a *global* ceiling: the opers are its callees and are unconstrained,
   and `operation_queue::Process` asserts no global level is held. It may carry
   `DMS_ENTERS_ITEM` (pumping under a per-item lock is by design), or nothing.
4. The session-usage counter is a `counted_mutex`: every `lock_shared`/`unlock_shared` on it takes
   `CountedMutexSection` (100), so anything that touches it — `ItemReadLock`, `ItemWriteLock` —
   is at 100 at the outermost, not at the item counter's 101. The first battery of the wave
   refused 204 + 22 cases on exactly this, before the diagnostic named it.
5. Run it in Debug and read the `lock level refused` lines, not the assert. Only the **first**
   refusal in a `.out` is evidence: the assert calls `exit(3)` without unwinding, so static
   teardown runs on that thread under the stale ceiling and every later line is an artefact.
6. What the first wave left undeclared, as "may take anything": everything under rule 3;
   `IncInterestCount`'s callers above `StartInterest`; the polygon and Dijkstra sections, which
   are declared inside operator bodies rather than at function scope; `ParseRegStatusFlags`
   (takes nothing itself); and every function whose only lock is a bare primitive (§1.2).
7. **The second wave (`e2c6033c`, 89 declarations)** walked the callers of the declared functions up to
   the frontier: every function whose level is the minimum of its callees' levels and its own
   acquires got that minimum, with the per-item rule (2) deciding most of them — the interest
   machinery (`TreeItemDualRef`, `AbstrDataItem::StartInterest`/`StopInterest`, `~TreeItem`,
   `SetDC`, `Copy`, `DoInvalidate`, the `OptionalInterestInc`/`Dec` templates every interest
   pointer goes through) and the scheduler (`Schedule`, `Run_with_cleanup`, `StealOneTask`,
   `StartOperationContexts`, `~OperationContext`). A function that pumps got the production-wait
   ceiling (§3.8), which is per-item and therefore admits pumping. A declaration goes **after** a
   fast-path or null check, not before it: `OptionalInterestDec(nullptr)` — an empty interest
   pointer dying, which happens under `sg_CountSection` all the time — takes nothing, and a
   ceiling ahead of the check refused 46 cases for an acquire that never came; the same for an
   empty `ItemWriteLock` being released (move-assignment into a fresh local under
   `cs_ThreadMessing`, 167 cases) and for `CurrActiveCancelIfNoInterestOrForced` with no active
   frame (57). Two kinds of caller stay undeclared on purpose: functions whose acquires depend on a thread-local mode (`DoFail` and
   everything that fails an item; the leaf declaration on `DecInterestCount` is the detector), and
   the status-flag getters (`GetRegStatusFlags`, `IsMultiThreaded*`): they take `RegisterAccess`
   only on a never-read slow path, so a function-level declaration would refuse every hot call
   made under an inner lock while the real acquire never happens there — P12's shape, one level
   up. Also left: the operator bodies in clc/geo (`CreateResult` and friends), which are the
   unconstrained context the checker is *for*, and the storage managers' `DoUpdateTree` /
   `ReadDataItem` overrides outside odbc, which the battery does not reach.

### 3.8 The production-wait ceiling — what makes P2 checkable

A wait for another thread's production is not an acquire, and B4 said the checker cannot see it.
Since `e2c6033c` it can, by declaration: `treeitem_production_task::lock_shared`/`lock_unique`,
`cs_lock::AwaitAncestorWrites`, `ReadLockInit`, the `ItemReadLock` constructor,
`WaitForReadyOrSuspendTrigger`, `OperationContext::Join`, `JoinSupplOrSuspendTrigger` and the two
`DoWorkWhileWaiting` variants each open with

    DMS_ENTERS_ITEM(ord_level_type::ItemProductionWait, dms_exclusive_v);

That is a per-item ceiling (item level 1), so by the rules of §3.2 it is **admitted under nothing
but a per-item lock or another per-item ceiling**: a thread holding any global section — and a
`TokenStr` is exactly a registry-shared hold at ordinal 90 — is refused the moment it would start
waiting. Under it, every global and every per-item acquire is allowed (rules 3 and 4), which is
what a wait that pumps main-thread opers and takes the item counter needs. The ordinal 50 is
immaterial (per-item levels are never ordered); it exists so the table can say what the ceiling
is. The token registry is thereby *at a higher ceiling* than production: a `TokenStr` may be held
across anything but a wait, and a wait may take anything but a `TokenStr`'s caller's locks.

What this does not cover: waits on bare condition variables and semaphores outside those entry
points (§1.2), and the pump inside `WaitForTaskNotification` when it is reached by a path other
than the ones listed. The four named `TokenStr` locals found by the P2 survey that spanned a
*foreign* call rather than a wait (`SendStatusText`, `FontArray`, `Gdal_DoOpenStorage`,
`GdalVectlMetaInfo::OnOpenForRead`/`WriteLayer`) were materialized to `SharedStr` in the same
commit; the survey found no site that spans a production wait.

## 4. Findings — potential deadlocks

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

### P2 — cross-thread registry cycle: shared holder blocks on a producer that registers — **checkable since `e2c6033c`**

Thread A holds a `TokenStr` (registry-shared) and blocks — even in a *timed, retrying* wait — on an
item production lock (§1.3). Thread B, the producer of that item, calls `GetTokenID_mt` →
`GetOrCreateID_mt`, which parks **untimed** until the shared count is zero. A's predicate never
becomes true (B never finishes), B never wakes (A never releases): a two-thread cycle through one
leveled lock and one logical lock. The per-thread usage counter (`208ab52f`) catches only the
*same-thread* case; until `e2c6033c` B4 made this variant invisible to the checker (it is a *wait*, not an acquire — the
per-item lock side was checked since `d9d791ba`, the wait on it was not). Now every production wait
declares the `ItemProductionWait` ceiling (§3.8), so a thread that starts one while holding a
`TokenStr` — or any global section — is refused at that point.

No concrete instance is known. The #1227 renames are the practical defense: every registry-holding
value is now spelled `...Lock()`, so "held across a blocking call" is greppable. The sites that
deliberately keep lock accessors (the `DMS_*` C API, `createSimilarSet`, `Object::XML_Dump`) were
each verified not to block. Since `e2c6033c` the Debug build checks it: every production wait declares
the `ItemProductionWait` ceiling (§3.8), which a thread holding a `TokenStr` is refused. A survey
of all 86 functions that still use a `...Lock()` accessor found none spanning a wait; the four
that spanned a foreign (GDI/GDAL) call were materialized.

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

### P5 — equal-ordinal families — **FIXED (`e2c6033c`): every section has its own ordinal**

Distinct mutexes sharing an ordinal (see §1.1: five sections at 94, five at 98, five at 99, four
at 100) can never be *nested* in a Debug-covered path — Allow rejects equal levels — so no
cross-order cycle can be built there. The residual risk is paths never run under Debug: in Release
nothing rejects the nesting, and any pair nested in both orders on different threads deadlocks
without a diagnostic. The 99 family was the one to watch: token registry, `cs_OcAdm`,
`cs_lock_map`'s map lock and `sc_MoveSupplInterestSection` all sat there, all exclusive.

*Fixed:* `LockLevels.h` now gives every section a distinct ordinal (the §1.1 table, with the old
values beside the new). Within a former family the order follows the nestings that were known —
`CountSection` before `UpdatingInterestSet`, `FailSection` before `MoveSupplInterest`,
`MoveSupplInterest` before `NotifyTargetCount`, `ThreadMessing` before `OperContextAccess`, the
registry innermost of its family so a token can be read under any of them, `CountedMutexSection`
innermost of its family because every counted-mutex op takes it. Where no nesting was known the
order is a choice, and the first Debug run that nests such a pair the other way round will refuse
it and name both sides; that is the point — a pair that could never be nested in Debug now has one
checked order instead of none.

### P6 — per-item locks against each other, and below an undetermined item — **structural, narrowed by `d9d791ba`**

*Was:* everything below any `cs_lock_map` per-item mutex was unchecked — not, as first written,
because of an item-level short-circuit, but because the lock never entered the checker at all
(B1). *Now:* a per-item lock on a determined item is checked against every global section and
ceiling in both directions (a global or ceiling held when it is taken is refused). Two things
remain unchecked, by decision: two per-item locks against each other, because item level does not
track the interest recursion (§3.2) and the supplier DAG is what keeps that acyclic — a cycle
there would need a cycle in suppliers, which the calculator refuses; and everything below the
lock of an item that reports level 0, which is skipped as before. The registry-exclusive self-case
under a per-item lock is still reported by the Release counter; the cross-thread variant is P2.

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

### P11 — `GetSequenceBoundingBoxCache` / `GetPointBoundingBoxCache` hold a global over a per-item lock — **FIXED (`e2c6033c`)**

`geo/dll/src/BoundingBoxCache.h`: both take `cs_BB` (`BoundingBoxCache1`, 93 — a global section)
and then construct a `DataReadLock(featureAttr)`, which takes the item's actor lock. That is rule
5 in its exact form — a global held while a per-item lock is taken — and the checker refuses it
the first time a Debug run reaches the bounding-box cache (the testcases battery does not).
The reverse edge exists: `~AbstrBoundingBoxCache` takes `cs_BB` and runs when a feature's data
object dies, which happens under `sg_CountSection` (98) in `TryCleanupMem` and under the item's
own locks. Two threads — one building a cache for feature A while A's data is being cleaned up
on another — close the cycle. Fix shape: take the `DataReadLock` first and `cs_BB` only around the
registry lookup/insert, which is what the section actually protects. *Fixed:* both getters now take the `DataReadLock` first and hold `cs_BB` only around the registry
lookup and the insert; building the cache — the expensive part, which used to run under `cs_BB`
and so serialized every cache build in the process — runs outside it, and a lost race is settled
by re-checking under the section (the loser's cache dies; its destructor finds the winner in the
registry and leaves it). Both declare `DMS_ENTERS_ITEM(ItemRegister, exclusive)`, truthfully.

### P12 — `SetStatusFlag` re-enters `RegAccessSection` on the never-read path — **open, low**

`rtc/dll/src/utl/Environment.cpp`: `SetStatusFlag` takes `RegAccessSection()` and, still holding
it, calls `ReadOnceRegisteredStatusFlags`, which takes the same section on its slow path. The
section is a `leveled_std_section` over a plain `std::mutex` — not recursive — so that path is a
self-deadlock. It is masked because the slow path runs once, at startup, before any caller of
`SetStatusFlag` exists; the checker refuses it too (equal ordinal, both exclusive). The ceiling on
`ReadOnceRegisteredStatusFlags` is therefore declared after its fast-path return, before the
acquire. Fix shape: read the flags before taking the section in `SetStatusFlag`.

### P13 — the result's `ItemWriteLock` is released inline under `cs_ThreadMessing` — **FIXED (`e2c6033c`)**

`OperationContext::separateResources` runs under `cs_ThreadMessing` (97) and, by its own comment,
moves everything whose destruction could destroy a `TreeItem` into a `garbage_can` that is emptied
*after* the section — except the write lock: on the cancel/exception path it assigns
`m_WriteLock = ItemWriteLock()` inline. `ItemWriteLock::releaseHeldLock` drops an **owning**
`shared_ptr<const TreeItem>`; if that were ever the last owner, item destruction — supplier
interest release, per-item locks — would run under a global section (rule 5). The wave first
declared `releaseHeldLock` per-item on that account and the checker refused it in 224 cases, all
from this call. The declaration now states what the function actually takes (the session counter
at 100 and the item counter at 101) and the premise it rests on — the write lock is never the last
owner of its item — is written at both sites. *Fixed:* `separateResources` now does `releaseBin |= std::move(m_WriteLock)`, so the release
joins the controlled path outside the section like every other holder, and
`ItemWriteLock::releaseHeldLock` declares `DMS_ENTERS_ITEM(ItemRegister, exclusive)` — the
premise is no longer needed, the checker sees the release where it happens.

### P14 — `DoFail` released supplier interest under `cs_ThreadMessing` — **FIXED in the wave (`2052ee75`)**

`Actor::DoFail` moves the failing item's supplier interest out (`MoveSupplInterest`) and let the
list die at the end of the function, which takes each supplier's per-item actor lock.
`OperationContext::HandleFail` calls `m_Result->Fail(item)` while holding `cs_ThreadMessing` (97,
global) — so those per-item locks were taken under a global section, which is the exact inverse of
the everyday edge `IncInterestCount` (holds the consumer's actor lock) → `StartInterest` →
`Schedule` → `cs_ThreadMessing`. Two threads, one failing a result while another starts interest in
one of its suppliers, close the cycle. The checker refused it on 7 battery cases the moment
`DoFailCaller` carried a truthful per-item declaration.

Fix: a per-thread `SupplInterestWasteCollector` (Actor.h). `HandleFail` installs one over its
`separatedResources` can — the can it already empties after the section — and `DoFail` hands the
waste to an active collector instead of dropping it. `DoFail`/`DoFailCaller` themselves carry **no**
ceiling, on purpose: what they take depends on whether a collector is active, and a declaration
cannot be conditional; the per-item declaration on `DecInterestCount` is what checks the
uncollected case against whatever the caller holds, so any other caller that fails an item under a
global section will be refused there.

---

## 5. Verified non-findings

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
  holds no *global* leveled section (`CurrentThreadHoldsNoGlobalLevelLock`; a per-item lock is by
  design — the meta thread pumps from inside `PrepareDataUsage` — and an oper under it may still
  take every global section).
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

## 6. Not analyzed to completion

- The file-mapping pair `FileMapHandle::m_ResizeMutex` (shared) ↔ `tiledata.h cs_file`, held
  across resize/page-in; a full nesting table of the mem/ser tile paging layer was not built.
- The shv/GUI side beyond the callback contracts (DataView queues at 94, Qt event-loop interplay).
- Interest-count machinery internals (`MoveSupplInterest(99)`, `UpdatingInterestSet(98)`,
  `sg_CountSection(98)`) beyond the P3 site: the pairwise order table for the act/ layer is
  unwritten.
- Storage managers other than odbc/gdal/cfs.

## 7. Historical deadlocks (fixed) — calibration

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

## 8. Standing rules that keep the list short

- **R1** — never hold a `...Lock()` value (TokenStr/TokenStrRange) across a call that can
  tokenize or block; materialize first (`Object.h`, `sym/Token.h`). At a report-family sink pass
  the `TokenID` itself, never `...Lock().c_str()` (B6); `tools/check-lock-across-sink.ps1`
  checks that syntactically and `analyze.bat` runs it.
- **R2** — the error-reporting path reads names and streams, nothing else:
  `DMS_ENTERS(IndexedString, shared)` on `Describe`/`GetDescription`, contracts on everything they
  dispatch to (`DebugContext.h`).
- **R3** — teardown-concurrent code gates on the lock-free `IsSessionTearingDown()`, never on
  `SessionData::Curr()` (which takes `sd_SessionDataCriticalSection` while the tree dies under it).
- **R4** — main-thread opers are unconstrained *because* every pump holds at most per-item locks — asserted in
  `operation_queue::Process`.
- **R5** — a foreign callback (GDAL error handler, progress notification) may report or post;
  it may not name items, take DMS locks, or wait.

## 9. Follow-up work, in order of value

1. ~~Retire P1, P3, P4~~ — done in `b591f683` (#1233), which also deleted `LevelCheckBlocker`
   and with it blind spot B2.
2. ~~Fix B1 in `Allow`~~ — the premise was wrong: the item-level rules were dead code because
   per-item locks never reached `Allow`. Done differently in `d9d791ba`: `cs_lock_map::ScopedLock` and
   `ScopedTryLock` enter the checker (item level ≥ 1 only), so a global or ceiling held when a
   per-item lock is taken is now refused; two per-item locks are deliberately left unordered
   (§3.2 says why, with the measurement). Battery 244/245 (the one failure is a tile-zeroing assertion in the #1236 connect_matrix write path, TileArrayImpl.h:230, unrelated to locking) with the per-item locks live. Residue:
   level-0 items stay invisible, and per-item-vs-per-item rests on DAG acyclicity — B1.
3. ~~Annotate every direct acquirer with the ceiling it demands~~ — first wave done in `2052ee75`
   (§3.7 has the rules; 118 declarations; battery 246/247). Second wave done in `e2c6033c`
   (§3.7 rule 7, §3.8; 89 declarations; battery 246/247): callers up to the pumping frontier, the
   production-wait ceiling that makes P2 checkable, distinct ordinals for P5, and P11 and P13
   fixed. Still open: P12 and the status-flag getters' slow path (the same shape); the operator
   bodies and the non-odbc storage managers stay undeclared.
4. Build the pairwise nesting table for act/ (interest machinery) and mem/ser (tile paging) — the
   two layers §6 leaves open.
5. The syntactic pass over `DMS_ENTERS` / `DMS_CALLEE_ENTERS` declarations (#1227 §3) — the static
   half that would make §2's blind spots enumerable instead of remembered. With every leaf now
   declared, that pass has something to check call sites against.
