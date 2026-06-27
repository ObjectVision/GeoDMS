# Teardown leak & TreeItem ownership cycles (investigation handoff)

Branch `MapView_Tilting`, 2026-06-27. **All work below is uncommitted.** This note is the restart
handoff for the DataController / LispRef teardown leak hunt and the surrounding ownership-model work.

---

## 1. Symptom

A Debug build asserts at process exit:

```
Assertion failed: NumbObjCache.empty(), file sym/dll/src/LispRef.cpp
```

`~LispCaches` finds interned Lisp objects still referenced at static teardown. They are the `m_Key`
(key-expression `LispRef`) of **DataControllers that never got destroyed**. For `reverse.dms`
(`/S1 /S2 /S3 ... test_log`) it is a stable **45 DataControllers** left in `s_DcMap`.

### It is a Heisenbug
- **Plain (un-debugged) run: leaks deterministically** (always 45).
- **Under cdb: never reproduces** (`s_DcMap == 0`, clean exit). The debugger's timing lets whatever
  is normally racing complete in time.
- Therefore you **must run plain** to observe it, and you **cannot** inspect it live under cdb.

### Dialog-free plain diagnosis
Plain Debug runs pop the modal assert/abort dialog. To diagnose without a blocking dialog, the
**Debug-only temp instrumentation** routes the failure to stderr (in `~LispCaches`):
```cpp
_set_error_mode(_OUT_TO_STDERR);
_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);   // needs <cstdlib>
```
With this, a plain run writes its diagnostic files and exits silently. (For *non*-Heisenbug asserts,
prefer running under cdb with `bu USER32!MessageBoxW ".kill;q"` — but that hides this bug.)

---

## 2. Root cause (data-confirmed)

The 45 DCs survive **every** teardown step (config release, full worker cancel+wait+destroy, dropping
every result `m_DataObject`). Per-DC dump shows: `dcIC=0` (not interest), `m_OtherSuppliers` empty,
a pure ownership DAG via `FuncDC::m_Args` rooted at a single DC.

**The decisive probe** — config-root refcount in `TreeItem::EnableAutoDelete`:
```
configRoot BEFORE ReleaseIt: rootRc=2   (GeoDmsRun)   /  rootRc=3 (GeoDmsGuiQt)
configRoot AFTER  ReleaseIt: rootRc=1   (GeoDmsRun)   /  rootRc=2 (GeoDmsGuiQt)
```
`SessionData::ReleaseIt` drops only `SessionData::m_ConfigRoot`. **One owning `SharedPtr` to the
config root persists and is never released** → the whole config tree stays alive → config items keep
their `mc_DC` → the root DC → the entire 45-DC supplier graph leaks. (`AutoDeletePtr cfg` is
non-owning; the `items` vector is destroyed before `EnableAutoDelete`; `CreateTreeFromConfiguration`'s
`res` is released after `Open`. So the extra ref appears **during `DMS_TreeItem_Update`**.)

**Both GeoDmsRun and GeoDmsGuiQt leak the same 45** via the same shared ref (GUI adds one more
root holder = desktop/views, hence rootRc 3 vs 2). GeoDmsGuiQt does **not** assert only because its
exit sets `g_IsTerminating`, so `~LispCaches` returns early — the leak is silently unchecked there.

### The mechanism: ownership reversal turned upward refs into cycles
Object lifetime is now **parent-owns-child** (downward); child→parent (`m_Parent`) is weak. Any
member that still holds an **owning `SharedPtr` upward / to an ancestor** is now a retain cycle.
Confirmed/likely instances:

| ref | where | status |
|---|---|---|
| `UsingCache::m_Usings` (`TreeItemCRefArray` = owning) | `tic/UsingCache.h:79` | **likely THE root pinner** — a namespace's using-cache owns the namespaces it pulls in, transitively the parent namespaces up to the root |
| `AbstrDataItem::m_DomainUnit` / `m_ValuesUnit` (owning `SharedPtr<const AbstrUnit>`) | `AbstrDataItem.h:157` | table↔attribute cycle (attribute's domain = its parent table). A back-registry already exists: `AbstrUnit::AddDataItemOut/DelDataItemOut/GetNrDataItemsOut` |
| calculator `m_NamedSuppliers` (the `using =` refs) | calculators | supplier→ancestor cycles |
| `FuncDC::AddDependency` / `m_OtherSuppliers` | DataController | DC→ancestor-DC (empty for `reverse`, but possible) |

---

## 3. Fixes implemented this session — KEEP (correct in themselves)

These are uncommitted and were validated to build clean (Debug) + smoke (`units.dms /x3000`); the
deeper teardown leak is independent of them.

- **Factory contract → owning `SharedPtr`** (the "orphanage detected early" work):
  - `MetaClass::createFromXmlFuncType` + `MetaClass::CreateFromXml` and the 3 concrete factories
    (`TreeItemClass`/`DataItemClass`/`UnitClass::CreateFromXml`) now return **`SharedPtr<SharedActor>`**
    (NOT `SharedPtr<Object>` — `Object` is non-refcounted; `SharedActor` = `SharedObjWrap<Actor>` is
    the nearest rtc-visible refcounted, polymorphic, `Release()`-providing base all products share).
    `#include "act/Actor.h"` added to `mci/Class.h`.
  - `TreeItem::CreateConfigRoot` + `CreateCacheRoot` return `SharedMutableTreeItem` (owned from birth).
  - `XmlTreeParser::ReadTree` returns `SharedMutableTreeItem` + a `m_RootHolder` member owns a
    brand-new root for the parse lifetime.
  - Removed dead `DMS_CreateTree` (only `stg/tst/src/main.cpp` referenced it; that file is in no `.sln`).
- **`TreeItemDualRef::Set` adopts a new cache result** (`isNew` → `MakeSharedForNewlyCreatedObject`)
  instead of borrowing (`existing_obj`) — fixes a real Debug assert: parentless cache items
  (`CreateCacheDataItem`) are refcount-0, and the DualRef is now their primary owner (pin removed).
- **`m_ResultAdi` → non-owning + `ImLosingIt`** (the lazy/future tile-functor self-cycle
  `result → m_DataObject → m_ResultAdi → result`): `DelayedTileFunctor`/`LazyTileFunctor` `m_ResultAdi`
  is now `mutable WeakPtr<AbstrDataItem>`; `AbstrDataObject` gained `virtual void ImLosingIt() const {}`;
  the functors override it to null `m_ResultAdi`; `GetTile` null-guards it. The owning virtual
  `TreeItem::ClearData` was **renamed to `ClearDataObject`** (base + `AbstrDataItem`/`RangedUnit`
  overrides + callers) and `AbstrDataItem::ClearDataObject` calls `m_DataObject->ImLosingIt()` before
  releasing; `~AbstrDataItem` now calls `ClearDataObject` directly. (Real cycle, but not the dominant
  pin for `reverse` — most results take the single-tile sync path with no future functor.)
- **Fix B — session termination cancels + drains workers** (`SessionData::SetCancelling()` + an
  exclusive `s_SessionUsageCounter` drain barrier at the start of `EnableAutoDelete`'s config-root
  branch). It did NOT fix this leak (the leak isn't live workers), but the user wants it kept:
  *session termination should cancel and drain worker threads regardless.*

---

## 4. TEMPORARY instrumentation still in the tree — REMOVE when leak is resolved

- `sym/dll/src/LispRef.cpp` `~LispCaches`: `<cstdlib>` include; `_set_error_mode`/`_set_abort_behavior`;
  fprintf dump of leaked cache contents to `C:\dev\GeoDMS_2026\leaked_lisp.txt`.
- `tic/dll/src/DataController.cpp`: `DBG_DumpDcMapSize`, `DBG_DropCacheResultData`, `DBG_DumpDcDetails`
  (writes `dcmap_trace.txt` / `dcmap_detail.txt`).
- `tic/dll/src/MoreDataControllers.h`: `FuncDC::DBG_GetOtherSuppliers()` (TEMP public accessor).
- `tic/dll/src/DataController.cpp`: `g_DBG_ConfigRoot` global + the `resRc`/`resCache`/`RES==ROOT`
  fields in `DBG_DumpDcDetails` (session 2 additions).
- `tic/dll/src/TreeItem.cpp` `EnableAutoDelete` config-root branch: the `DBG_*` calls, the
  `extern g_DBG_ConfigRoot; g_DBG_ConfigRoot = this;` assignment, + the
  `configRoot BEFORE/AFTER ReleaseIt rootRc=` fprintf. NOTE: the AFTER-ReleaseIt rootRc read now
  dereferences freed memory when the fix works (root is destroyed at ReleaseIt) — prints garbage,
  harmless, but remove with the rest.
- Scratch output files at repo root: `dcmap_trace.txt`, `dcmap_detail.txt`, `leaked_lisp.txt`.

---

## 4b. PROGRESS 2026-06-27 (session 2) — config-root cycle CUT; second leak class found

**Done (uncommitted):**
- `UsingCache::m_Usings` → non-owning `TreeItemCPtrArray` (`UsingCache.h/.cpp`); `~UsingCache` now
  actively detaches from its `m_Incoming` (erases `m_Context` from each incoming's `m_Usings` +
  `SetDirty`) so a non-owned used-namespace can't dangle an incoming. **Measured: did NOT change
  `rootRc` (still 2) — `m_Usings` was not the root pinner.** Kept anyway (correct: the tree owns
  namespaces; removes a latent cycle).
- **Root pinner identified by data**: enhanced `DBG_DumpDcDetails` to print `resRc`, `resCache`
  (`res->IsCacheItem()`) and a `RES==ROOT` flag (`g_DBG_ConfigRoot` set in the EnableAutoDelete probe).
  For `reverse.dms`: DC[38] = an `isOld` **SymbDC whose `m_Data` owns the config root** (`reverse`),
  closing `root → owns /test_log → mc_DC → DC graph(m_Args) → DC[38].m_Data → root`.
- **FIX implemented (user-chosen "isOld m_Data non-owning")**: `TreeItemDualRef::m_Data` split into
  a non-owning `WeakPtr<const TreeItem> m_Data` (the universal current-result pointer used by every
  accessor) + an owning `SharedTreeItem m_OwnedData` set ONLY when the DualRef must keep the result
  alive: `isNew` (sole owner of a fresh cache result), `tmp`, or an `isOld` borrow of a **cache** item.
  An `isOld` borrow of a **config** item (`!ti->IsCacheItem()`) is left non-owning — the tree owns it —
  which cuts the root cycle. (`TreeItemDualref.h`, `DataController.cpp` Set/SetTmp/Clear.) `WeakPtr`
  bool-context is fine (precedent: `m_BackRef`); `return m_Data;` binds `SharedPtr(const WeakPtr&)`
  (owning copy). **Build green; `reverse.dms` leak GONE: `rootRc` 2→1, `s_DcMap` 45→0, no
  `leaked_lisp.txt`, clean exit, deterministic over 3 runs.** `subitem.dms` also clean.

**REMAINING (second, independent leak class) — needs a decision:** ran the whole `Unit/Operator`
suite (21 cfgs) in plain Debug. `reverse`/`subitem` clean, but ~14 still leak. Characterized
`subset.dms` (25 DCs): the config **root frees cleanly** (`rootRc=1`→destroyed), but the survivors are
endogenous **cache items** (`resCache=1`, `isNew`: `/test`,`/att`,`/RegionCode` — template/`subset`/
`select` results) in a `cacheItem.mc_DC → DC → DC.m_OwnedData → cacheItem` 2-cycle (DAG-root DC[23]),
**not reachable from the config-tree teardown sweep** (they're parentless cache roots, not under the
config tree). The single `isOld` config-unit borrow there (DC[3], `resCache=0`) is correctly
non-owning now. So the `m_Args` DC-supplier graph is a DAG; the only cycle is the cache-item↔`mc_DC`
link at the DAG root. This was almost certainly pre-existing (my change only removed owning edges →
can't add cycles; no crashes/premature-frees seen, all computations succeeded).

**UPDATE — cache class is the `m_DomainUnit`/`m_ValuesUnit` unit↔attribute cycle, and weakening those
edges FAILS (proven):** added `du`/`vu` cross-refs to the dump — the subset survivors are endogenous
cache attributes owning cache **units** (`du=r7/r17`, `vu=r7/r16`) which own the attributes as
sub-items (parent-owns-child). So the cycle is `attr.m_DomainUnit/m_ValuesUnit (owning) → unit →
(parent-owns-child) → attr`, i.e. handoff item #2, NOT `mc_DC` (user confirmed `mc_DC` is not the
owning back-ptr). Implemented item #2 (m_DomainUnit/m_ValuesUnit → `WeakPtr`, `~AbstrUnit` nulls the
registered domain back-refs, `m_RegisteredDomainOut` flag so `~AbstrDataItem` skips a dangling
`DelDataItemOut`; m_ValuesUnit needs no hook — never dereffed at teardown). Build green; the whole
Operator suite then collapsed to `s_DcMap=0` for ~17/21 — **but two configs regressed**:
`merge_indirect_domainunit.dms` **segfaults** and `centroid_or_mid_complex.dms` **hangs on asserts**
(both were `exit=0`+leak before). cdb stack on the segfault:
`FuncDC::MakeResult → FuncDC_CreateResult → AbstrDataItem::GetDynamicObjClass → AbstrUnit::GetUnitClass`
→ **access violation on a dangling unit** — the attribute's values/domain unit was **freed prematurely
during COMPUTE** because the non-owning ref no longer kept the (computed/indirect) unit alive.
**CONCLUSION: `m_DomainUnit`/`m_ValuesUnit` are load-bearing during compute and CANNOT be weakened.**
The whole item-#2 change was **REVERTED** (all 4 files back to owning `SharedPtr`; reverse.dms still
clean, merge_indirect/centroid no longer crash, the ~14 unit-cycle configs leak again as before).

**Strategic consequence (key):** the principled "weaken the upward owning ref" approach only works for
edges that are NEVER needed for liveness (the `isOld` config-item borrow ✓, `m_Usings` ✓). For edges
needed during compute (`m_DomainUnit`/`m_ValuesUnit`, and likely calculator/DC suppliers), weakening
causes premature-free crashes. Those cycles must instead be broken **at teardown** (option Z / s_DcMap
flush — reset each DC result's cycle members after interest/compute is done), i.e. the deterministic
replacement for the removed auto-delete pin. This reframes the remaining work toward the teardown-flush
the user had earlier deprioritized — now backed by crash evidence.

**Superseded fix options for the cache class (mc_DC framing was wrong):**
- (X, principled) make `mc_DC` **non-owning for cache-result items** (the DC owns the result via
  `m_OwnedData`; the result shouldn't own its DC back). Needs the DC to retain another owner (m_Args /
  registry) and a clearing hook — mirrors the `isOld` cut but on the other edge.
- (Z, pragmatic / "replace the pin's teardown role") at config teardown iterate `s_DcMap` and reset
  each result's `mc_DC` (`SetDC({})`) — breaks item→DC ownership for the parentless cache items the
  config-tree sweep can't reach. Deterministic, teardown-only; would also have fixed reverse on its
  own. User had earlier deprioritized this band-aid in favour of the principled cut.

## 5. Next step — cut the cycle

The user's call (design discussion held): the *immediate* fix is to **weaken the upward owning refs**.

1. **`UsingCache::m_Usings` → non-owning** (raw / `TreeItemCPtrArray`). The cache already maintains
   itself via `TreeItem`'s `OnItemAdded`/`OnItemRemoved` hooks, so it does not need to own the used
   namespaces (the tree owns them). This is the likely root-pinner — do this first and re-measure
   `rootRc` + `s_DcMap` size.
2. **`AbstrDataItem::m_DomainUnit`/`m_ValuesUnit` → weak**, nulled via the existing `DataItemsOut`
   registry (a unit, on destruction, nulls the back-ptr of its registered data items — the
   `ImLosingIt`-shaped pattern, registry already present; only the **domain** unit is registered today).
3. **Calculator/DC suppliers → `InterestPtr`** (keep supplier *data* alive during compute, don't own
   the object) rather than owning `SharedPtr`.

### Strategic question on the table: migrate the TreeItem hierarchy to `std::shared_ptr`/`std::weak_ptr`
Motivation: the in-repo `WeakPtr<T>` is **a raw pointer with NO liveness detection**, so every weak
edge (`m_Parent`, `m_BackRef`, `m_ResultAdi`, `m_DomainUnit`, `m_Usings`, suppliers) needs a *bespoke*
clearing hook — that hand-maintained safety is what keeps leaking. `std::weak_ptr::lock()` gives safe
detection uniformly. The old blocker (`shared_from_this` UB on ref-0-alive objects) is largely cleared
by the owned-from-birth factory work. **Decisive risk: memory+speed on huge data** (`std::shared_ptr`
control block; `make_shared` co-allocates object+control-block so memory can't free while any
`weak_ptr` lives — and there will be many long-lived weak edges). Recommendation: **decouple** — cut
this leak now with the in-repo weakening (#1–#3); pursue the `std::` migration as a separate,
benchmarked branch scoped to the TreeItem hierarchy (interest stays intrusive — `InterestCount` is a
separate counter and does not migrate). Gate go/no-go on a large-config memory+throughput benchmark.

---

## 6. Diagnostic recipes (reusable)

- **Reproduce the leak**: plain Debug `GeoDmsRun.exe /L<log> /S1 /S2 /S3 C:\dev\tst\Unit\Operator\cfg\reverse.dms test_log`
  (Bash: `export MSYS2_ARG_CONV_EXCL='*'` so `/L`,`/S` aren't path-mangled). Read `dcmap_trace.txt`.
- **GUI variant**: `GeoDmsGuiQt.exe /L<log> /T<dmsscript> <config>`; a minimal `.dmsscript` is
  `ActivateItem "/test_log"` / `DefaultView` / `WAIT` / `SEND 1 3 16 0 0` (WM_CLOSE → teardown).
- **Heisenbug note**: never expect this leak under cdb; use the stderr-routing instrumentation instead.
- **Killing stuck abort-dialog procs** (orphaned JIT debug port; `taskkill` reports "no running
  instance" though `tasklist` shows them): attach a fresh `cdb -p <pid> -c ".kill; q"`.
- cdb dumps are UTF-16 — read with PowerShell `Select-String`, not Grep.

See also memory `project_treeitem_ownership_model` (the running session log) and
`feedback_cdb_breakpoint_messagebox`.
