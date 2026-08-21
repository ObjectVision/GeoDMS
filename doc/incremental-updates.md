# Incremental updates: invalidation, timestamping, and external change detection

Evaluation of the GeoDMS invalidation/update mechanism, its failure modes, and the design
work needed to safely re-activate external (file) change detection (`DetermineExternalChange`).

Based on code review of the GeoDMS26 tree (rtc, tic, clc, stg, stx, qtgui), 2026-07.

---

## 1. How the mechanism works

### 1.1 Timestamping

- Internal time is a global 32-bit counter `std::atomic<TimeStamp> tsLast`, starting at
  `tsBereshit = 1` (`rtc/dll/src/act/UpdateMark.cpp:33`, `UpdateMark.h:56`). `TimeStamp` is
  `UInt32` (`rtc/dll/src/cpc/Types.h:132`); `FileDateTime` is `UInt64` (Windows FILETIME).
- A fresh TS is minted lazily: `TriggerFreshTS` increments only when the previous "frame" was
  committed (`bCommitted`, `UpdateMark.cpp:206-224`); reading `GetLastTS()` commits the frame.
  Minting is restricted to the meta thread (`GetFreshTS` asserts `IsMetaThread()` and
  `!IsInActiveState() && !IsInDetermineState()`, `UpdateMark.cpp:226-234`).
- Each actor carries `m_LastChangeTS` (when it last changed) and `m_LastGetStateTS` (when it
  last checked) (`rtc/dll/src/act/Actor.h:258-264`).
- `Renumber()` (`UpdateMark.cpp:236-267`) recompresses persisted timestamps into the current
  counter; the global `tsLast` itself is **not** persisted — every session restarts at 1.

### 1.2 Invalidation is pull-based

- There is no dependents/back-reference list. `Actor::DetermineState()`
  (`rtc/dll/src/act/Actor.cpp:828-887`) recomputes `DetermineLastSupplierChange()` by visiting
  suppliers (`SupplierVisitFlag::DetermineState`) and taking the max of their
  `GetLastChangeTS()`; if the actor's own TS is older, it calls `InvalidateAt` (clears
  progress and failure, drops the DataController, resets the supplier cache;
  `Actor.cpp:295-330`, `tic/dll/src/TreeItem.cpp:3366-3409`) and retries.
- Short-circuit: when `m_LastGetStateTS >= UpdateMarker::LastTS()` (`Actor.cpp:847`) the whole
  determination is skipped — nothing is re-examined until a new global TS is minted anywhere.
- Progress states: `None → MetaInfo → Validated → Committed`; failure types
  `Determine/MetaInfo/Data/Validate/Committed` (`rtc/dll/src/act/ActorEnums.h:103-125`).
  Update phases run through `SuspendibleUpdate` (`Actor.cpp:437-557`) with cooperative
  suspension via `SuspendTrigger`.

### 1.3 Calc rules and caching

- A rule is canonicalized to a `LispRef` s-expression which *is* the key into the global
  in-memory `s_DcMap` of DataControllers (`tic/dll/src/DataController.cpp:396-476`). No
  hashing; structural LispRef identity. Changing a rule produces a different LispRef, hence a
  new DC; old DCs stay in the map until their last interest drops
  (`DataController.cpp:420`).
- Context that affects the result is folded into the key: values-unit conversions wrap the
  expression via `slConvertedLispExpr` (`tic/dll/src/TreeItem.cpp:2535`), integrity checks via
  `TreeItem_CreateCheckedExpr`.
- A storage-read (loadable, no calc rule) item's key leaf is built by `CreateLispTree`
  (`tic/dll/src/LispTreeType.cpp:343-361`):
  `(sourceDescr <fullName> <loadNumber> <subtree>)`, where `loadNumber` is the config-load
  generation from `ConfigurationFilenameContainer` (`stx/dll/src/ConfigFileName.h:52`).
- Declared vs computed type/unit is checked in `AbstrDataItem::CheckResultItem`
  (`tic/dll/src/AbstrDataItem.cpp:568-611`) via `UnifyDomain`/`UnifyValues`
  (`tic/dll/src/AbstrUnit.cpp:271-364`), failing with `FailType::Determine`.

---

## 2. Failure-mode list

### A. External (file) changes — the biggest gap

1. **File-mtime → internal-TS mapping is disabled.** In
   `TreeItem::DetermineLastSupplierChange` the folding line is commented out:
   `//MakeMax(lastChangeTS, DSM::Curr()->DetermineExternalChange(lastFileChange))`
   (`tic/dll/src/TreeItem.cpp:3466`); `GetCachedConfigSourceTS` at line 3442 is disabled too.
   `DSM` no longer exists in the tree — this needs re-implementation, not just uncommenting.
   Consequence: a source file replaced on disk mid-session does not invalidate anything.
2. **Polling is gated on internal changes.** `GetCachedChangeDateTime` re-stats only when
   `m_LastCheckTS < UpdateMarker::GetLastTS()`
   (`tic/dll/src/stg/AbstrStoragemanager.cpp:737-746`); an idle session never re-polls.
3. **No re-validation between metainfo and data-read phases.** A file swapped after
   `DetermineState` but before/during tile reads yields mixed data undetected. Acknowledged by
   `// TODO: lock deze file vanaf hier` in `tic/dll/src/stg/FileSystemStorageManager.cpp:59`
   and `MemoryMappedDataStorageManager.cpp:47`.
4. **Writes don't reset the mtime cache** (`CloseStorage` leaves `m_FileTime`/`m_LastCheckTS`
   stale) — a trap once external detection is re-enabled: read and write paths share one
   cached `m_FileTime` per storage manager.
5. **mtime granularity/clock issues unhandled**: FAT 2s resolution, mtime-preserving copies,
   backup restores with *older* mtimes. Relevant once detection is restored; compare FDT by
   `!=`, not `<`.

### B. Timestamp mechanics

6. **32-bit counter, debug-only overflow check** (`assert(impl::tsLast)`,
   `UpdateMark.cpp:216`) — release-build wraparound silently breaks the "newer is larger"
   invariant.
7. **Changes within one uncommitted frame collapse to one TS**; a consumer whose
   `m_LastGetStateTS` equals that TS short-circuits (`Actor.cpp:847`) and misses the second
   change. Correctness depends on every mutation path calling `TriggerFreshTS` at the right
   moment — a missed call is undetectable at runtime.
8. **`Invalidate()` is a no-op on never-timestamped actors** (`m_LastChangeTS == 0`,
   `Actor.cpp:355`) — side effects of `DoInvalidate` (DC drop, GUI notify) silently skipped.
9. **Global `tsLast` not persisted; per-actor TSs are** ("for persistency only",
   `Actor.h:154`). Persisted TSs are only meaningful after `Renumber()`. This only bites if
   results themselves are persisted across sessions: stale persisted TSs comparing as newer
   would then validate stale data. **No such store exists today** — the CalcCache that used
   to be one was retired with the 8.0 series, and its `ASF_WasLoaded` flag has since been
   removed (#1189; see `doc/development/g8-todos.md` §4). So this is a constraint on any
   future persistent result store, not a live defect.

### C. Pull-model blind spots

10. **Nothing pushes staleness to passive consumers** — views/exports holding earlier results
    keep them until they re-pull.
11. **Suppliers mid-`DetermineState` are skipped during traversal** (`Actor.cpp:666-669`,
    recursion break) — `lastSupplierChange` can under-report; relies on retry loop + a later
    fresh TS.
12. **`AF_InvalidationBlock` suppresses `DoInvalidate` but not state clearing**
    (`Actor.cpp:323`) — half-invalidated actors if the skipped part isn't re-run.
13. **Dependencies outside the expression graph are invisible:** `ExplicitSuppliers` is a
    manually maintained string (rename → "not found" at evaluation,
    `tic/dll/src/SupplCache.cpp:125`; omission → permanently stale); `SqlString` and
    `StorageName` `%placeholder%` expansion (`AbstrStoragemanager.cpp:429-512`) are evaluated
    at read time and are not suppliers; indirect `='…'` expressions only participate via what
    they resolved to at substitution time.
14. **No in-session config reload.** `DMS_IsConfigDirty` (`tic/dll/src/SessionData.cpp:101-120`)
    only reports "any TS issued since load"; it does not watch `.dms` files.

### D. Rule-change / cache-key edge cases (incl. "rule change → different unit type")

15. **What `CheckResultItem` catches, by case:**
    - Declared values unit explicit + different **value type** → caught
      (`cu->IsKindOf(GetDynamicClass())` before any default-unit escape,
      `AbstrUnit.cpp:339`).
    - Declared explicit + same type, different **metric/projection** → caught
      (`AbstrUnit.cpp:349-362`) or silently coerced via inserted conversion
      (`TreeItem.cpp:2535`).
    - Declared values unit **default (unspecified)** → `UM_AllowDefaultLeft` returns true
      right after the type check (`AbstrUnit.cpp:346`), skipping metric and projection
      entirely: a rule change from meters to kilometers on an undeclared item propagates
      unflagged. Value-type changes are still caught; unit-semantics changes are not.
    - **Domain** with `HasFixedValues()` always unifies (`AbstrUnit.cpp:291`); under
      `UM_AllowAllEqualCount` two different domains unify merely on equal `GetCount()`
      (`AbstrUnit.cpp:317`) — silent element misalignment.
    - Failures are recorded as `FailType::Determine` and are sticky until a fresh global TS
      (see B7).
16. **`CheckResultItem` mutates state during checking** — `SetTSF(TSF_Categorical)` in the
    categorical branches (`AbstrDataItem.cpp:598,604`) even if the check later fails.
17. **Old DataControllers linger in `s_DcMap`** after a rule change until interest drops —
    memory growth plus stale-result availability.
18. **Dangling raw supplier pointers** in `s_SupplierLevels`: "registered suppliers may
    already be destroyed (and locations even be reused!)" (the comment now sits in
    `tic/dll/src/TicDataSupport.cpp`, where `DataStoreManager.cpp` was merged); cleanup
    callback is a no-op.
19. ~~**CalcCache directory validity is version-only**~~ — historical. The cache directory
    was named `CalcCache…v<major>.<minor>`, so validity was keyed on the GeoDMS version
    alone and nothing tied cached results to source dates or config content. That is *why*
    a persistent store needs the source version in the key (§3.3); the cache itself is gone
    (#1189).

### E. Concurrency / exception paths

20. `SetDC` exception-safety TODO (`tic/dll/src/TreeItem.cpp:791`) — residual interest count
    possible on throw.
21. `DecInterestCount` fast path races with concurrent increments (acknowledged,
    `Actor.cpp:1179`); 0→1 transitions are main-thread-only by assertion only.
22. `UpdateMetaInfo` spawns a `std::async` continuation when stack runs low
    (`TreeItem.cpp:2790-2807`) — fragile vs `IsMetaThread()` asserts on TS minting.
23. On exception, `UpdateMetaInfo` forces progress ≥ MetaInfo and normalizes via `CatchFail`
    — the failure, not the cause, is what is cached; recovery depends on a fresh TS.

### Top three

1. External change detection is dead code (A1/A2): mtime is fetched, cached, and discarded.
2. `UM_AllowDefaultLeft` swallows metric/projection changes for items without an explicit
   values unit (D15).
3. Frame-collapse + `m_LastGetStateTS` short-circuit (B7): every correctness argument reduces
   to "someone minted a fresh TS at the right moment", with no runtime cross-check.

---

## 3. Re-activating `DetermineExternalChange` safely

### 3.1 File-handle locking: per read-transaction, not per interest lifetime

Three windows to consider:

- **stat → open (TOCTOU).** `GetLastChangeDateTime` stats via `FindFileBlock`
  (`rtc/dll/src/utl/Environment.cpp:898-909`) without opening the file. Fix: after opening,
  read the authoritative mtime from the handle (`GetFileTime(hFile)`) and use that FDT as the
  version of this read. This addresses the `lock deze file vanaf hier` TODOs.
- **During the read / live mapping: already covered by share modes.**
  `FileHandle::OpenForRead` uses `GENERIC_READ` + `FILE_SHARE_READ` only — no
  `FILE_SHARE_WRITE`, no `FILE_SHARE_DELETE` (`rtc/dll/src/ser/FileMapHandle.cpp:220-233`);
  while an MMD/FSS handle or mapping is open, no other process can modify or delete the file.
  Exception: sources read through external libraries (GDAL, ODBC) — there, compare
  handle/path FDT before and after the read.
- **After the read, until interest drops to zero: do NOT hold handles.** Once data is in
  memory the result is a faithful function of the snapshot read; a later change should be
  *detected* at the next pull, not *prevented*. Holding deny-write handles for the interest
  lifetime would block external tools from refreshing sources — the very workflow being
  enabled. Release at `DataReadLock` end; record the handle-FDT as the computed-from version.

**Handle budget:** per-process kernel handle limit is ~16.7 million (2^24); a `CreateFile`
handle costs a few hundred bytes of kernel memory. The 512/8192 `_setmaxstdio` ceiling
applies only to CRT streams. The historical per-tile-mmap resource exhaustion was the number
of section objects + mapped views (VAD entries, page tables, system commit), not file
handles. With .MMD's one-file-per-item, even ~100k items is far below any hard limit.

### 3.2 Fresh TS on application re-activate

**Currently NOT done in qtgui** — zero `GetFreshTS`/`TriggerFreshTS` calls outside rtc; the
`QEvent::WindowActivate` handler in `qtgui/exe/src/DmsMainWindow.cpp:716` only restores
keyboard focus. The hook presumably existed in the GeoDMS 7 Delphi GUI and was never ported.

It is the right trigger: a fresh TS breaks both gates at once — consumers'
`m_LastGetStateTS >= LastTS()` shortcut (`Actor.cpp:847`) and the storage re-stat gate
(`AbstrStoragemanager.cpp:740`). Activate → fresh TS → next pull re-runs `DetermineState` →
re-stats files → changed FDT becomes newer supplier TS → invalidation propagates.

Caveats:

- **Architectural subtlety (likely why line 3466 was commented):** `GetFreshTS` asserts
  `!IsInDetermineState()` (`UpdateMark.cpp:230-231`), but the FDT is observed *inside*
  DetermineState. Split responsibilities: **poll and mint at the activation event** (outside
  any determine context: walk registered storage managers, stat each; for every FDT that
  differs from the recorded one, mint a fresh TS and record the pair in a session map
  `{expandedStorageName → (FileDateTime, TimeStamp)}`), and make `DetermineExternalChange` a
  pure **lookup** in that map. Then restoring `TreeItem.cpp:3466` is safe.
- Use `TriggerFreshTS` (lazy), not `GetFreshTS`; hook `QEvent::ApplicationActivate` on the
  `QApplication` (fires once, on the meta thread) rather than per-window `WindowActivate`;
  debounce.
- Only mint when a stat actually shows a differing FDT — a no-change activation should keep
  the global shortcut intact (free re-activation, no full supplier-tree re-walk).
- Not covered: changes while the app stays in the foreground (background batch, network
  share). If needed: low-frequency `QTimer` poll or `ReadDirectoryChangesW` watchers.
  GeoDmsRun (headless, single pass) needs none of this.

### 3.3 External timestamp in LispExpr key leaves

The leaf already has a versioning slot: `(sourceDescr <fullName> <loadNumber> <subtree>)`
(`LispTreeType.cpp:343-361`). Extend to
`(sourceDescr <fullName> <loadNumber> <sourceVersion> <subtree>)` for authentic-source
items, at the `GetCheckedKeyExpr` fallback for loadable items (`TreeItem.cpp:2654-2656`).

Design points, in order of importance:

1. **Keys complement, not replace, the TS mechanism.** Keys are rebuilt only after
   invalidation re-derives the calculator; without the TS bump nobody rebuilds keys. TS route
   = "when to look again"; FDT-in-key = "identity of what was read". In-session invalidation
   works with the TS route alone (stable keys); FDT-in-key is what would make a *persistent*
   result store safe, because raw FILETIME (UInt64, UTC) is session-stable while internal
   `TimeStamp` resets to 1 each session. (The GeoDMS 7 CalcCache had no such key component,
   which is why its validity was version-only — see failure mode 19.)
2. **Sample once per epoch, from one place** — `GetCachedChangeDateTime`'s epoch gating
   already provides this if it is the single source of the FDT used in keys.
3. **Version only authentic sources** (`IsDataReadable`, has storage parent, not cache item —
   cf. "Track changes in authentic sources", `TreeItem.cpp:3450-3452`). Exclude storages
   GeoDMS writes this session, or a write → new FDT → new key → recompute loop results.
   `CreateLispTree` is also used for moniking/Convert tests — do not version those paths.
4. **Compare FDT by `!=`, not `<`** (backup restores have older mtimes; FAT granularity;
   mtime-preserving copies). For full robustness add file size or a content hash.
5. **Representation pitfall:** `loadNumber` goes in as `LispRef(Number(...))`; if `Number` is
   float64, raw FILETIME exceeds the 53-bit mantissa and silently collides. Use two UInt32
   leaves, a string token, or UI64 leaf support (`UI64DC`,
   `DataController.cpp:384-386`) — or store a small per-storage change counter from the DSM
   map (compact, but cross-session cache reuse then needs the map persisted with the cache).
6. **Placeholder expansion moves earlier:** the FDT belongs to the *expanded* storage name
   (`ExpandImpl`, `AbstrStoragemanager.cpp:429-512`); keying it forces expansion at
   metainfo/substitution time. Arguably a feature (storage path becomes part of dependency
   identity — closes gap C13), but the early expansion must go through the identical code
   path as the read-time one.
7. **Garbage:** every observed external change re-keys the full downstream DC subgraph; old
   DCs linger in `s_DcMap` until interest drops. Pair with an eviction sweep of zero-interest
   DCs.

### 3.4 Suggested order of attack

1. Activation-event poll + a DataStoreManager-style `{expandedStorageName → (FDT, TimeStamp)}`
   map; reinstate the commented-out `DetermineExternalChange` call in
   `TreeItem::DetermineLastSupplierChange` as a lookup → correct in-session invalidation,
   stable keys. Note this needs re-implementing, not uncommenting: the `DSM` it named no
   longer exists (#1189), and the two commented lines are kept only as markers for this step.
2. Fix the stat/open TOCTOU: record handle-based FDTs at `DataReadLock` time, feed them back
   into the map.
3. *If* a persistent result store is ever introduced: extend the `sourceDescr` leaf with the
   source version, with the exclusions from point 3.3-3. There is no such store today and
   none is planned; the CalcCache that used to be one was retired with the 8.0 series.
