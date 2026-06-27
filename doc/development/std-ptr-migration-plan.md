# TreeItem ownership migration: intrusive `SharedPtr` → `std::shared_ptr` / `std::weak_ptr`

Branch `MapView_Tilting`, 2026-06-27. Design plan (user-directed). Companion to
`teardown-leak-and-ownership-cycles.md` (the leak hunt that motivated this).

---

## 1. Why migrate (what the session proved)

Every teardown leak this session is a **retain cycle among intrusive owning back-edges**. The in-repo
`WeakPtr<T>` (`rtc/dll/src/ptr/WeakPtr.h`) is just `ptr_base<T,copyable>` — **a raw non-owning pointer
with NO liveness detection and NO control block**. So every weak edge needs a *bespoke* clearing hook,
and that hand-maintained safety is itself the recurring bug source:

- `m_BackRef` — nulled in `SetReferredItem`/`~TreeItem`.
- `m_ResultAdi` — `ImLosingIt()` virtual.
- `UsingCache::m_Usings` — `~UsingCache` detaches from `m_Incoming`.
- `AbstrUnit::DataItemsOut` registry — `~AbstrDataItem` deregisters; would need a `~AbstrUnit` reset.

Two failure modes were demonstrated directly:

1. **`isOld m_Data` weak (config item)** — works at teardown (fixed reverse.dms) but **can dangle on a
   config edit**: a raw weak ref with no way to detect the config item was freed.
2. **`m_DomainUnit`/`m_ValuesUnit` weak** — **CRASHED**: access violation *during compute*
   (`FuncDC::MakeResult → FuncDC_CreateResult → AbstrDataItem::GetDynamicObjClass →
   AbstrUnit::GetUnitClass`). The computed/indirect unit was kept alive *only* by the (now-weak) back-ref;
   nothing else owned the transient unit, so it was freed mid-compute.

`std::weak_ptr::lock()` gives **uniform, automatic liveness detection**, and `std::shared_ptr` sub-item
ownership keeps every tree node (including units) alive so weak refs can `.lock()` them for the duration
of a use. That removes **all** the bespoke clearing hooks above and fixes both failure modes:

- (1) the weak config-item ref `.lock()`s; a vanished item returns null instead of dangling.
- (2) the unit is owned by the tree (its parent's `shared_ptr` sub-item link) and/or its producing DC's
  result holder, and compute `.lock()`s the weak ref into a **temp `shared_ptr`** held across the op —
  so it cannot be freed mid-compute.

---

## 2. The ownership rule

- **Owning = `std::shared_ptr`, DOWNWARD only — sub-item relations.** `single_linked_shared_tree<T>`:
  `m_FirstSub` and `m_Next` are `std::shared_ptr<T>`. A parent transitively owns its whole subtree
  (including the sibling chain via `m_Next`).
- **Weak = `std::weak_ptr` — everything else (all non-sub-item relations).** `m_Parent`, `m_BackRef`,
  `m_DomainUnit`, `m_ValuesUnit`, `UsingCache::m_Usings`, `mc_RefItem`/`mc_OrgItem` (config↔cache
  cross-ref), calculator named-suppliers, DC→TreeItem back-refs. **Dereferenced only via `.lock()`**,
  which yields a temp `shared_ptr` held for the duration of the use.
- **Exceptions — owning `std::shared_ptr` that are NOT sub-item relations:**
  - `SessionData::m_ConfigRoot` — the single owning root holder (the tree has no parent above it).
  - **Temp locked pointers** — `InterestPtr` / `DataReadLock` / compute-time result holders: the
    `shared_ptr` obtained from `.lock()`, held across an operation to guarantee liveness during use.
  - `TreeItemDualRef::m_Data` IsNew + IsOld-cache-subitem arms (own the result — see §4).
- **Interest stays INTRUSIVE.** `Actor::m_InterestCount` is a *separate* counter governing data
  residency (`StartInterest`/`StopInterest`, free heavy data at 0), independent of object lifetime. It
  does **not** migrate. Object lifetime = `std::shared_ptr` refcount; interest = intrusive counter. Two
  orthogonal systems, as today.

---

## 3. Per-member ownership map (TreeItem hierarchy)

| member | file | now | after | note |
|---|---|---|---|---|
| `single_linked_tree::m_FirstSub`, `m_Next` | `rtc .../single_linked_tree.h` | raw | `shared_ptr<TreeItem>` | **the** owning edges; becomes `single_linked_shared_tree<T>` |
| `TreeItem::m_Parent` | `tic TreeItem.h` | `WeakPtr<const TreeItem>` | `weak_ptr<const TreeItem>` | upward, weak |
| `TreeItem::m_BackRef` | `tic TreeItem.h` | `WeakPtr<const TreeItem>` | `weak_ptr` | cache→config; weak (already weak-typed) |
| `TreeItem::mc_RefItem` / `mc_OrgItem` | `tic TreeItem.h` | strong | `weak_ptr` | config↔cache instantiation cross-ref; the cache root is owned by its DC's `m_Data` |
| `TreeItem::mc_DC` | `tic TreeItem.h` | `DataControllerRef` (strong) | **keep owning** | config item owns its DC; DC is a *separate* hierarchy (not a TreeItem). The TreeItem boundary is `DC::m_Data` (→ variant, §4) |
| `AbstrDataItem::m_DomainUnit`, `m_ValuesUnit` | `tic AbstrDataItem.h:157` | `SharedPtr<const AbstrUnit>` | `weak_ptr<const AbstrUnit>` | **the unit↔attribute cycle**; lock-at-use; see §5 |
| `AbstrDataItem::m_DataObject` | `tic AbstrDataItem.h:160` | strong | **keep owning** | the item owns its data object; the functor self-cycle is already cut (`m_ResultAdi` weak + `ImLosingIt`) — under std:: that becomes a plain `weak_ptr` |
| `UsingCache::m_Usings` | `tic UsingCache.h` | now raw (this session) | `vector<weak_ptr<const TreeItem>>` | lock-at-use; `m_Incoming` stays raw observer |
| `AbstrCalculator` named-suppliers / `m_Holder` | `tic AbstrCalculator.*` | strong | `weak_ptr` | supplier→ancestor; lock-at-use |
| `SessionData::m_ConfigRoot` | `tic SessionData.h:90` | `SharedPtr<const TreeItem>` | `shared_ptr` (exception) | the owning root holder |
| `SessionData::m_ConfigSettings` | `tic SessionData.h:90` | `SharedPtr` | `weak_ptr` | it is a *child* of the root → tree owns it |

DC graph (parallel, non-TreeItem hierarchy — `DataController`/`FuncDC`): `m_Args`/`m_OtherSuppliers`
ownership is internal to the DC graph and **out of scope** for the TreeItem migration except at the
`DC::m_Data` boundary (the variant). The `s_DcMap` `DuplRef`-CAS weak-registry pattern
(`MakeSharedFromWeakPtrInsideSync`) is replaced by `std::weak_ptr` + `lock()` once DCs are
`shared_ptr`-managed (a follow-on; can stay intrusive initially).

---

## 4. `TreeItemDualRef::m_Data` → `std::variant` (the IsNew/IsOld/IsTmp distinction)

The current `(DCF_IsOld, DCF_IsTmp)` state flags + single pointer become one variant whose `index()`
*is* the state:

```cpp
using DcRef = std::variant<
    std::monostate,                    // empty
    std::shared_ptr<TreeItem>,         // IsNew  : root cache result — DualRef is the primary owner
    std::shared_ptr<const TreeItem>,   // IsOld  : sub-item of another root-cache-item — owned, "like new"
    std::weak_ptr<const TreeItem>,     // IsOld  : config item — the tree owns it; .lock() to use
    std::weak_ptr<TreeItem>            // IsTmp  : instantiation borrow at the calling site; lock at use
>;
mutable DcRef m_Data;
```

- `IsNew()/IsOld()/IsTmp()` ⇐ `m_Data.index()`.
- `GetOld()/GetNew()/operator->` dispatch on the active arm; the **weak arms return `.lock()`**, so a
  vanished config/tmp item is detected (null) rather than dereferenced blind — this is precisely the
  dangling-`IsOld` fix that the in-repo `WeakPtr` cannot give.
- The two owning arms (IsNew, IsOld-cache-subitem) keep results alive exactly as the current
  `m_OwnedData` does. This session's `isOld` split (config→non-owning, cache→owning) maps **directly**
  onto the variant — the diagnosis carries over; the variant just makes the weak arm *safe*.

---

## 5. Units: weak refs + compute-time liveness (the thing that crashed)

`m_DomainUnit`/`m_ValuesUnit` become `weak_ptr<const AbstrUnit>`. The cycle (attribute owns unit; unit
owns attribute as a sub-item) is broken because the back-ref no longer owns. Liveness during compute is
guaranteed by:

1. **Tree ownership** — a unit that lives in a (cache or config) tree is owned by its parent's
   `shared_ptr` sub-item link; it survives as long as the tree root (held by the producing DC's
   `m_Data` / `SessionData::m_ConfigRoot`) is alive.
2. **Lock-at-use** — `GetAbstrDomainUnit()/GetAbstrValuesUnit()` (and every compute path that touches a
   unit, e.g. `GetDynamicObjClass → GetUnitClass`) take a temp `shared_ptr` via `.lock()` and hold it
   across the use. This is what was missing in today's crashing attempt — the weak ref alone, with the
   transient unit owned by nothing else, was freed mid-compute.

**`DataItemsOut` registry — keep, but only for its functional role.** It has two purposes:
- (a) *clear the back-ref on unit death* — **obviated** by `weak_ptr` (auto-detection; no manual reset,
  no `~AbstrUnit` hook, no values-unit parallel registry needed). This is exactly why we do **not**
  build the values-unit clearing registry — the migration removes the need.
- (b) *range-change notification* — `AbstrUnit::OnDomainChange`, `Unit.cpp:732`
  (`if (self->GetNrDataItemsOut())`). **Functional; must be preserved.** The registry stays for this;
  registration/deregistration (`AddDataItemOut`/`DelDataItemOut`, `DataLocks.cpp:349`,
  `~AbstrDataItem`) remains, minus any back-ref-reset responsibility.

---

## 6. Three hard problems & how they're handled

1. **`shared_from_this` during construction (the old blocker).** Taking a `shared_ptr`-to-`this`
   inside a constructor is UB. Rule: objects are created by `std::make_shared` (or a factory returning
   `shared_ptr`) and only *then* wired in (`AddItem(childShared)` sets `parent->m_FirstSub/m_Next =
   childShared` and `child->m_Parent = weak_ptr(parentShared)`). No `AddSub`/parent-wiring from inside a
   ctor. The owned-from-birth factory work already done this session (`CreateConfigRoot`/
   `CreateCacheRoot` → `SharedMutableTreeItem`, `XmlTreeParser::ReadTree`, factory contract →
   `SharedPtr<SharedActor>`) is the scaffolding; finish threading it so every creation path yields a
   `shared_ptr` before wiring.
2. **Recursive `shared_ptr` teardown → stack overflow.** Destroying a node releases `m_FirstSub` and
   `m_Next` `shared_ptr`s recursively; a long `m_Next` sibling chain or a deep tree can blow the stack on
   big configs. `~TreeItem` (and the `single_linked_shared_tree` node dtor) must tear down
   **iteratively**: detach the sibling/child chain into a local and unlink in a loop, not via recursive
   `shared_ptr` destruction.
3. **Perf/memory on huge data (the go/no-go gate).** External control block (+16 bytes, atomic
   inc/dec), and `make_shared` co-allocates object+control-block so object memory cannot be freed while
   *any* `weak_ptr` survives — and there will be many long-lived weak edges (`m_Parent`, units, usings).
   **Gate the merge on a large-config memory + throughput benchmark** (e.g. a big RSOpen/2BURP-class run),
   comparing peak commit + wall-clock against the intrusive baseline. Keep the intrusive code path as the
   fallback until the benchmark passes.

---

## 7. Phased execution (in-place on `MapView_Tilting`; no worktrees, no push)

0. **Design lock** (this doc).
1. **Foundational pointer layer.** Introduce `shared_ptr`/`weak_ptr` aliases for the TreeItem family
   (`SharedTreeItem` etc. → `std::shared_ptr<…>`), `enable_shared_from_this` on `TreeItem`/`AbstrUnit`/
   `AbstrDataItem`. Retire the intrusive *ownership* ops (`AdoptRef`/`DuplRef`/`IsOwned`/`GetRefCount`/
   `Abandon`); **keep `Actor::m_InterestCount` intrusive**. Provide shims so `newly_obj`/`existing_obj`/
   `no_zombies` call-sites compile during transition, then remove.
2. **`single_linked_shared_tree<T>`** — `m_FirstSub`/`m_Next` `shared_ptr`, `m_Parent` `weak_ptr`;
   rewrite `AddItem`/`RemoveItem`/`~TreeItem` (iterative teardown, §6.2). Delete the AutoDelete
   machinery (the per-node pin) — `shared_ptr` refcount replaces it.
3. **Back/side refs → `weak_ptr`** with lock-at-use: `m_BackRef`, `m_DomainUnit`/`m_ValuesUnit`,
   `m_Usings`, `mc_RefItem`/`mc_OrgItem`, calculator suppliers.
4. **`m_Data` variant** (§4) + `SessionData::m_ConfigRoot` `shared_ptr` / temp-lock `shared_ptr`
   exceptions.
5. **Cleanup + verify.** Remove temp instrumentation (see leak-doc §4/§4b inventory) and the obsolete
   bespoke hooks; convert the `s_DcMap` `DuplRef` pattern to `weak_ptr` if DCs migrate. Full regression
   (Operator suite must reach `s_DcMap=0` with no crashes; then prj_snapshots) + the §6.3 benchmark.

---

## 8. Carried over from this session

**Keep (encode the correct distinctions; fold into the migration):**
- `isOld m_Data` non-owning split (config→non-owning, cache-subitem→owning) → becomes the variant arms (§4).
- `UsingCache::m_Usings` non-owning + `~UsingCache` incoming-detach → becomes `weak_ptr<…>` + lock (the
  detach hook then disappears).
- Fix-B (session termination cancels+drains workers), the `m_ResultAdi` weak + `ImLosingIt`, the
  factory-contract → owned-from-birth work.

**Remove once green:** all temp instrumentation (`DBG_DumpDc*`, `g_DBG_ConfigRoot`, `resRc`/`resCache`/
`du`/`vu` dump fields, the `rootRc` probe, `_set_error_mode` in `~LispCaches`, scratch `dcmap_*`/
`leaked_lisp.txt`), the AutoDelete pin, and the per-member clearing hooks the migration obsoletes.

**Reverted (do not reintroduce as an interim):** the in-repo-`WeakPtr` `m_DomainUnit`/`m_ValuesUnit`
weakening — it premature-frees during compute. It is only safe under §5 (tree `shared_ptr` ownership +
lock-at-use), i.e. after the migration.

---

## 9. Class-hierarchy restructuring (feasibility crux)

Current layering (verified):

```
Object                      (mci; polymorphic, NON-refcounted; Class singletons derive from it, static)
  └ PersistentObject        (rtc)
      └ Actor               (act; carries m_InterestCount — the INTEREST counter)
SharedBase                  (ptr; intrusive m_RefCount + AdoptRef/IncRef/DecRef/DuplRef/IsOwned/Abandon)
SharedObjWrap<VBase> : VBase, SharedBase          (combines a polymorphic base with intrusive refcount)
SharedActor = SharedObjWrap<Actor>                (= Actor + SharedBase)

TreeItem        : SharedActor, ItemTree           (ItemTree = single_linked_tree<TreeItem>)
TreeItemDualRef : SharedActor                      (m_Data lives here)
DataController  : TreeItemDualRef                  (so DCs are SharedActors too)
```

**Both `TreeItem` and `DataController` are `SharedActor`s** — they share *two* intrusive subsystems:
`Actor::m_InterestCount` (interest) and `SharedBase::m_RefCount` (object lifetime). The migration must
**split object-lifetime management while keeping interest shared**:

- **`Actor` stays** the common base carrying `m_InterestCount` (interest is intrusive, unchanged).
- **`TreeItem` drops `SharedActor` → derives `Actor` directly + `std::enable_shared_from_this<TreeItem>`**;
  its lifetime is the `std::shared_ptr` control block. `SharedBase` is no longer in TreeItem's bases.
  `ItemTree` → `single_linked_shared_tree<TreeItem>`.
- **`DataController`/`TreeItemDualRef` keep `SharedActor`** (intrusive) — **confirmed by the back-ref
  audit in §14**: nothing holds a `std::weak_ptr` to a DC, so DCs never need a control block.
  `DataControllerRef` stays intrusive `SharedPtr<DataController>`; `mc_DC` (TreeItem→DC, intrusive) stays
  owning. The only TreeItem-boundary in the DC graph is `DC::m_Data` → the §4 variant of std:: pointers.
- (`Actor` already had `SharedObj` factored out of it — TreeItem deriving `Actor` directly is the clean
  continuation of that.)

**Cross-boundary type — `SharedActorInterestPtr` must split.** `using SharedActorInterestPtr =
InterestPtr<SharedPtr<const SharedActor>>` (`Actor.h:73`) and `Actor::GetInterestPtrOrNull()` couple the
*interest* counter with intrusive *object* ownership over `SharedActor`. After TreeItem stops being a
`SharedActor`, an interest-holder on a TreeItem can no longer be `SharedPtr<const SharedActor>`. Resolve
by parameterising the holder on its ownership pointer: TreeItem interest → `InterestPtr<std::shared_ptr<
const TreeItem>>` (a temp-locked `shared_ptr`, §2), DC interest → `InterestPtr<SharedPtr<const
DataController>>` (intrusive). `GetInterestPtrOrNull` likewise splits or templatises. Used at
`OperationContext::m_ResKeeper`, `SupplInterest`, `ItemSchemaView::m_AllItems`, storage
`interest_holders_container` — all must follow the split. This is Phase-1 work, **not** a reason to
migrate DCs.

> **Follow-up cleanup (write-down, do later):** once stable, *flatten* `DataController` into
> `TreeItemDualRef` and **drop `Actor` as a DC base** — a DC does no timestamping, no invalidation, and
> doesn't implement `PersistentObject::GetID()/GetParent()`; it only needs interest + the result handle.
> Removing the `Actor`/`SharedActor` base from DCs would let the interest holder for DCs shrink too.
- **`Object`/`Class`** (and other `SharedObj = SharedObjWrap<Object>` users that are NOT TreeItems) are
  unaffected — `Class` singletons are static and were never `SharedBase`-managed for lifetime in a way
  that conflicts. Audit `SharedObjWrap<Object>` users (`rtc Class.h`, `persistent.cpp`) to confirm none
  are TreeItems needing migration.

**Factory contract** (changed this session to return `SharedPtr<SharedActor>`): for the TreeItem family
it returns `std::shared_ptr<TreeItem>` (the products are all TreeItems); `MetaClass::CreateFromXml`'s
return type splits or templatizes accordingly. `Class`/`Object` returns stay raw.

**`InterestPtr`** (`InterestPtr<SharedPtr<const SharedActor>>`) currently couples an owning `SharedPtr`
with an interest inc/dec. For TreeItems it becomes `InterestPtr<std::shared_ptr<const TreeItem>>` (owning
shared_ptr + interest). This is one of the **temp-locked-`shared_ptr`** exceptions (§2): it legitimately
holds a `shared_ptr` to keep data alive across a compute window. For DCs it stays intrusive.

---

## 10. Intrusive-API → `std::` call-site taxonomy

Every in-repo smart-pointer API and its replacement (drives the mechanical sweep; the count of each
class sizes the work):

| intrusive (now) | meaning | std:: replacement |
|---|---|---|
| `SharedPtr<T>(p, newly_obj{})` / `MakeSharedForNewlyCreatedObject` | adopt a fresh refcount-0 object | `std::shared_ptr<T>(p)` from `make_shared`/factory; **at the creation site**, not later |
| `SharedPtr<T>(p, existing_obj{})` / `MakeSharedFromBorrowedObjectPtr` | +1 on an already-owned object | copy an existing `shared_ptr`, or `shared_from_this()` |
| `SharedPtr<T>(p, no_zombies{})` / `MakeSharedFromWeakPtrInsideSync` (`DuplRef` CAS) | safe weak→strong upgrade under lock | `std::weak_ptr<T>::lock()` (the whole reason for the migration) |
| `AdoptRef()` | take ownership of refcount-0 object | gone — ownership is the `shared_ptr` itself |
| `IncRef()/DecRef()/Release()` | manual refcount | gone — managed by control block |
| `GetRefCount()` | debug/assert/heuristics | `use_count()` (debug only) — audit non-debug uses (e.g. `DataController.cpp:366` assert, `DBG_*`) |
| `IsOwned()` | refcount>0 assert | gone / `bool(shared_ptr)` |
| `Abandon()` | mark for the `no_zombies` path | gone — `weak_ptr` handles it |
| `WeakPtr<T>` (raw, no detection) | non-owning back-ref | `std::weak_ptr<T>` (+ `.lock()` at every use) |
| `AutoDeletePtr` / AutoDelete pin / `EnableAutoDelete` | teardown force-free of pinned subtree | **deleted** — `shared_ptr` refcount + iterative `~TreeItem` replace it |
| `SharedMutableTreeItem`, `SharedTreeItem`, `DataItemRefContainer` element, etc. typedefs | TreeItem-family handles | re-typedef to `std::shared_ptr<…>` (most call-sites unchanged) |

`get()/get_ptr()/operator->/operator bool/is_null()/has_ptr()` exist on both, so most *read* sites are
source-compatible after the typedef switch; the churn is concentrated in the **construction/ownership**
verbs above and in adding `.lock()` at weak-deref sites.

> **Follow-up cleanup (write-down, do later):** if, after the migration, **no** `SharedObj`/`SharedBase`
> object needs to support back-/weak-pointers any more (GraphicObjects already use
> `enable_shared_from_this_base<GraphicObject>`; DCs keep only intrusive forward/registry refs), then the
> whole in-repo intrusive smart-pointer toolkit (`SharedBase`, `SharedPtr`/`WeakPtr`, `newly_obj`/
> `existing_obj`/`no_zombies`, `DuplRef`) — and this taxonomy — can be retired entirely. **First get
> things working; this removal is a separate later pass.**

---

## 11. Code sketches (the load-bearing rewrites)

**Tree links integrated directly into `TreeItem`** (not a separate `single_linked_shared_tree<T>` base —
per review, move the members into `TreeItem` and merge the link member-functions in):
```cpp
struct TreeItem : Actor, std::enable_shared_from_this<TreeItem> /*, …*/ {
    // ... (was ItemTree = single_linked_tree<TreeItem>) now inline:
    std::shared_ptr<TreeItem> m_FirstSub;   // owns first child
    std::shared_ptr<TreeItem> m_Next;       // owns next sibling
    std::weak_ptr<const TreeItem> m_Parent; // non-owning up-ref (lock at use)
    // AddSub/DelSub/_GetFirstSubItem/GetNextItem/etc. fold into TreeItem methods.
};
```
NOTE: `single_linked_tree` was generic (also used by shv `dataview.cpp` view objects via `AddSub`/`DelSub`
on `m_ParentView`) — so the *generic* template stays for those non-TreeItem users; only **TreeItem's** use
is inlined and switched to `shared_ptr`. Audit the `AddSub`/`DelSub` call-sites to keep the view-object
path on the old generic template.

**`AddItem` (post-construction wiring only):**
```cpp
void TreeItem::AddItem(const std::shared_ptr<TreeItem>& child) {
    assert(child && child->m_Parent.expired());
    child->m_Parent = weak_from_this();
    // append to the owned sibling chain (no recursion)
    std::shared_ptr<TreeItem>* tail = &m_FirstSub;
    while (*tail) tail = &(*tail)->m_Next;
    *tail = child;                    // the tree now owns `child`
    if (m_UsingCache) m_UsingCache->OnItemAdded(child.get());
}
```

**`~TreeItem` / RemoveItem — ITERATIVE teardown** (avoids recursive `shared_ptr` destruction stack
overflow on long `m_Next` chains, §6.2):
```cpp
TreeItem::~TreeItem() {
    // unlink children into a worklist and drop them iteratively
    std::shared_ptr<TreeItem> child = std::move(m_FirstSub);
    while (child) {
        auto next = std::move(child->m_Next);   // detach sibling before child dies
        child->m_FirstSub.reset();               // (its own subtree torn down by its dtor, also iterative)
        child = std::move(next);                 // loop, not recurse
    }
}
```

**`GetTreeParent`** → `m_Parent.lock()` (returns null if the parent vanished — replaces the `no_zombies`
`DuplRef` dance). All upward walks (`GetTreeParent` chains, 132 sites) become `.lock()`-guarded.

**`TreeItemDualRef` accessors over the §4 variant:**
```cpp
const TreeItem* GetOld() const {
    return std::visit(overload{
        [](std::monostate)                                  -> const TreeItem* { return nullptr; },
        [](const std::shared_ptr<TreeItem>& p)              { return p.get(); },
        [](const std::shared_ptr<const TreeItem>& p)        { return p.get(); },
        [](const std::weak_ptr<const TreeItem>& w)          { return w.lock().get(); }, // temp lock
        [](const std::weak_ptr<TreeItem>& w)                { return (const TreeItem*)w.lock().get(); },
    }, m_Data);
}
```
(For sustained use the caller holds the `lock()`ed `shared_ptr`, not the bare `get()`.)

---

## 12. Transition strategy (keep each phase compilable)

The build will be red *within* a phase but green *between* phases. Tactics:
- **Typedef indirection first.** Switch `SharedTreeItem`/`SharedMutableTreeItem`/… typedefs to
  `std::shared_ptr<…>` and provide a thin `WeakPtr`-compatible `std::weak_ptr` alias so the bulk of
  read-only call-sites compile unchanged; fix the ownership-verb sites (table §10) class by class.
- **Shim the retired verbs** during transition: free functions `AdoptRef(shared_ptr&)` (no-op),
  `MakeSharedFromBorrowedObjectPtr(p)` → `shared_ptr` via `shared_from_this`, so sites compile before
  they're rewritten; delete the shims at end of phase.
- **One subsystem per commit** on `refactor_ownership` (user-sanctioned intermediate commits): (1) types,
  (2) tree, (3) weak back-refs, (4) variant, (5) cleanup. Each commit builds Release + Debug and runs the
  Operator suite.

---

## 13. Test plan & risk register

**Gates (per phase):** Release+Debug build green; Operator suite (`/S1 /S2 /S3 … test_log`) → no crash and
`s_DcMap=0` at teardown (the instrumentation stays until the final cleanup phase); then prj_snapshots
(t720/2BURP, t641/RSOpen) for concurrency/stack-pressure; then the **perf benchmark vs the recent
reference results** (peak commit + wall-clock).

**Risks:**
- *Stack overflow on teardown* — mitigated by iterative `~TreeItem` (§11); test with a deep/wide config.
- *Perf/memory regression* — the go/no-go gate; `make_shared` co-alloc + control-block atomics on millions
  of nodes. Fallback: keep intrusive path until benchmark passes.
- *`shared_from_this` in ctor* — forbidden; enforced by the make_shared-then-wire protocol (§6.1).
- *Hidden refcount-0-alive assumptions* — code that today creates a raw object and relies on the
  AutoDelete pin / `existing_obj` borrow before an owner exists. The factory work cleared the known ones
  (parser use-after-free); the sweep must catch the rest (they surface as `bad_weak_ptr`/null `lock()`).
- *Const-correctness* — `shared_ptr<const TreeItem>` ↔ `shared_ptr<TreeItem>` needs `const_pointer_cast`
  at the mutate boundaries (today's `const_cast<TreeItem*>` sites).
- *DC/TreeItem boundary* — DCs stay intrusive; verify no remaining `SharedActor`-as-TreeItem assumption
  (e.g. `SharedActorInterestPtr` used on TreeItems must switch to the std:: InterestPtr).

---

## 14. Back-ref audit of the DataController family (justifies keeping DCs intrusive)

Goal: confirm **nothing holds a `std::weak_ptr` to a DataController/`FuncDC`/`SymbDC`/… (or its ancestors/
descendants)** — because a weak edge is the only thing that would force a DC to be `std::shared_ptr`-
managed. Every back/side reference found is managed, transient, or an intrusive self-registry:

| ref | where | kind | verdict |
|---|---|---|---|
| `OperationContext::m_FuncDC` | `OperationContext.h:311` (`WeakPtr<const FuncDC>`) | back-ref OC→FuncDC | **managed**: `FuncDC` owns the OC (`m_OperContext` is `shared_ptr<OperationContext>`, `MoreDataControllers.h:153`); the link is reset from **both** sides — `FuncDC::resetOperContextImpl` does `operContext->m_FuncDC.reset()` (`MoreDataControllers.cpp:204`), called by `~FuncDC → CancelOperContext` (`:170-172`) and by the OC's own end (`OperationContext.cpp:1698-1701`, `assert(!m_FuncDC)`). So `m_FuncDC` is nulled before the FuncDC dies even if a worker still holds the OC. Stays in-repo `WeakPtr`. |
| `DataControllerContextHandle::m_DC` | `DataController.h:104` (`const DataController*`) | raw, RAII handle | stack-scoped; DC outlives the handle. Raw is fine. |
| `OperatorContextHandle::m_FuncDC` | `OperationContext.cpp:541` (`const FuncDC*`) | raw, RAII handle | stack-scoped. Fine. |
| `s_DcMap` | `DataController.cpp:344` (`map<key, const DataController*>`) | non-owning registry | intrusive self-managed: `~DataController` erases itself; weak→strong upgrade is the `DuplRef`/`MakeSharedFromWeakPtrInsideSync` CAS. Stays intrusive (DCs stay intrusive). |
| `FuncDC::m_Args` (`DcRefListElem::m_DC`), `m_OtherSuppliers` | `MoreDataControllers.h` | owning **forward** ref (`DataControllerRef`) | not a back-ref; intrusive forward ownership. Fine. |
| `TreeItem::m_Producer`, `WaiterSet`, `ItemWriteLock` ocb | `TreeItem.h:157`, `OperationContext.h:110`, `ItemLocks.h:71` | `std::weak_ptr<OperationContext>` | weak to the **OperationContext** (already `std::shared_ptr`-managed), **not** to a DC. Fine as-is. |

**Conclusion:** no `weak_ptr`-to-DC anywhere → DataController/FuncDC/SymbDC/NumbDC/StringDC/UI64DC and their
ancestors (`TreeItemDualRef`/`SharedActor`/`Actor`) **stay intrusive**. The migration boundary is exactly
`DC::m_Data` (→ §4 variant) plus the `SharedActorInterestPtr` split (§9).

---

## 15. Consolidated follow-up cleanups (after the migration is green — do NOT do now)

1. Flatten `DataController` into `TreeItemDualRef`; drop the `Actor`/`SharedActor` base from DCs (no
   timestamp/invalidation/`GetID`/`GetParent` needed). (§9)
2. If nothing left needs intrusive back-/weak-ptrs, retire the whole in-repo intrusive smart-pointer
   toolkit (`SharedBase`, `SharedPtr`/`WeakPtr`, `newly_obj`/`existing_obj`/`no_zombies`, `DuplRef`) and
   the §10 taxonomy. (§10)
3. Remove all temp leak-hunt instrumentation (§8 / leak-doc inventory).
4. Remove GraphicObject serialisation/persistence onto a config-item tree (`GraphicObject::Sync`). That
   is the *only* consumer of `TreeItem::Reorder` (via shv `GraphicContainer::SaveOrder`) — the call that
   forced the `TIC_CALL` export of `Reorder` in Phase 1a. With `Sync` gone, `Reorder` (and its export)
   become dead and can be deleted too.

