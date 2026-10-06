# TreeItem ownership: who owns whom

*Status (2026-10-06): reference for the ownership model at HEAD 0563aa5a0. The TreeItem family moved
from the intrusive `SharedPtr`/`WeakPtr` to `std::shared_ptr`/`std::weak_ptr` on 2026-06-28
(3f17a26e5 to ae5fecde5), the transitional wrappers went on 2026-07-02 (70c3f9422), and the
pointer-safety review was fixed in 36aa718b8, 55a3968ba and 3d5d95f96. This page replaces four
documents, now history in `doc/archive/`:
[std-ptr-migration-plan.md](../archive/std-ptr-migration-plan.md) (the design),
[stdptr-migration-handoff.md](../archive/stdptr-migration-handoff.md) (the work log),
[ptr-safety-review-2026-07-02.md](../archive/ptr-safety-review-2026-07-02.md) (the audit and its
fixes) and [teardown-leak-and-ownership-cycles.md](../archive/teardown-leak-and-ownership-cycles.md)
(the leak hunt that started it). Open work is in section 8.*

## 1. Three kinds of count

- **Lifetime** of a TreeItem-family object (`TreeItem`, `AbstrUnit`, `Unit<V>`, `AbstrDataItem`;
  `AbstrParam` is an alias of `AbstrDataItem`) is a `std::shared_ptr` control block:
  `struct TreeItem : Actor, std::enable_shared_from_this<TreeItem>`. A TreeItem has no intrusive
  reference count.
- **Interest** (should the data be kept resident) is the intrusive `Actor::m_InterestCount`,
  independent of lifetime. An item can be destroyed while consumers still hold interest in it; its
  destructor therefore undoes its own supplier interest (`StopSupplInterest`).
- **Usage locks** are counts of a third kind: `TreeItem::m_ItemLockCount` (item read/write locks,
  `ItemLocks.cpp`) and `AbstrDataItem::m_DataLockCount` (data locks). `DataReadLock` declares its
  owning `m_KeepItemAlive` first, so that it is destroyed last and no count-bearing lock is ever the
  item's last owner.

Everything else that is reference counted stays intrusive (`SharedBase`, `SharedPtr<T>`): see section 5.

## 2. The tree and its side edges

Ownership in the tree points down. Every other edge between TreeItems is weak, with a few
deliberate owning exceptions.

| edge | type | kind |
|---|---|---|
| `TreeItem::m_FirstSub`, `m_Next` | `std::shared_ptr<TreeItem>` | owning: a parent owns its first child, each child owns its next sibling; `~TreeItem` tears the sibling chain down iteratively |
| `TreeItem::m_Parent` | `std::weak_ptr<const TreeItem>` | weak |
| `TreeItem::m_BackRef` | `std::weak_ptr<const TreeItem>` | weak: cache root to the config item that refers to it |
| `TreeItem::mc_RefItem`, `mc_OrgItem` | `std::weak_ptr<const TreeItem>` | weak |
| `AbstrDataItem::m_DomainUnit`, `m_ValuesUnit` | `WeakUnit` (`std::weak_ptr<const AbstrUnit>`) | weak; lock at use, or `GetDomainUnitOrThrow()`/`GetValuesUnitOrThrow()` |
| `UsingCache::m_Usings` | `std::vector<std::weak_ptr<const TreeItem>>` | weak; a namespace detaches itself from its incoming caches |
| `AbstrCalculator::m_Holder` | `std::weak_ptr<const TreeItem>` | weak |
| tile functor `m_ResultAdi` (`TileFunctorImpl.h`) | `std::weak_ptr<AbstrDataItem>` | weak, cleared by `ImLosingIt()`; since 0fc692c63 (TIC-A10) `OperAttrBin`'s apply function also captures its result weakly |
| `SessionData::m_ConfigRoot`, `m_ConfigSettings` | `SharedTreeItem` | owning: the session owns the configuration tree |
| `AbstrCalculator::m_SearchContext` | `SharedTreeItem` | owning, may point up the tree (to the config root as fallback) |
| `SupplCache` (explicit suppliers) | `std::shared_ptr<const TreeItem>[]` | owning |
| `ConfigProperties::mc_CheckGuardians` (#1218) | `std::shared_ptr<const TreeItemCheckGuardians>` | owning, may hold cross-branch suppliers |
| `s_ExtPins` (`TicItemSupport.cpp`) | `std::multiset<SharedTreeItem>` | owning: the pins of the C API `DMS_TreeItem_AddRef`/`Release` |

The upward owning edges of calculators, supplier caches and check guardians form cycles with the
tree. They are cut at teardown, not by weak pointers: `TreeItem::EnableAutoDelete` on the config root
marks the session cancelling, waits a bounded time for the session usages to drain (#1191),
resets interest, calls `ResetSubTreeConfigData` (calculators, integrity checkers, check guardians,
storage managers) and then `SessionData::ReleaseIt`, which drops the last owner.

Two consequences of `std::enable_shared_from_this` that code must respect: `weak_from_this()` of an
object expires when its destructor is entered, so during teardown a child can see an expired
`m_Parent` while it is still alive; and `shared_from_this()` on such an object throws
`std::bad_weak_ptr`.

## 3. Classifying a raw pointer

A raw pointer to a TreeItem-family object never becomes owning by itself. `ptr/SharedTreePtr.h`
provides the helpers:

- `make_shared_tree(p, newly_obj{})`: adopt a freshly created object as its first owner. It checks
  (`MG_CHECK2`) that the object has no control block yet.
- `make_shared_tree(p, existing_obj{})`: become an additional owner of an object that is already
  owned (`shared_from_this`; throws `bad_weak_ptr` if it is not).
- `make_shared_tree(p, no_zombies{})`: the safe weak-to-strong step; null if the object is expiring
  or not owned.
- `make_weak_tree(p)`: a weak reference from a raw pointer (empty when not owned).
- `lock_or_cancel(w)`: lock a stored weak pointer for a worker-side operation, and throw the task
  cancellation when it has expired (the configuration is being torn down).

Never write `std::shared_ptr<T>(rawPtr)` for a family object: that makes a second control block with
a delete deleter, and the object is freed under its real owners. The transitional wrapper made this
a compile error; since 70c3f9422 it is a convention, and `tools/check-ptr-discipline.ps1` enforces it
(section 6).

The policy: a pointer to a family object that is stored beyond a stack frame is a `std::weak_ptr`
(or one of the owning exceptions above); a stack-local borrow may stay raw. The validity of a raw
borrow rests on one of these rules, established by the verification round of 2026-07-03:
a `.lock()` temporary pins its target to the end of the full expression; a live item implies live
ancestors (parent owns child, and destruction runs top-down on the meta thread); a kind-1 result
holder cannot expire while it is set (section 4); an `OperationContext` co-owns the units of its
result and its arguments for its whole run; and a worker that finds an expired weak pointer cancels
or fails, it never skips silently. Check-then-lock (`if (!w.expired())` and a later `w.lock()->`)
is not a guard: lock once and keep the result.

## 4. DataControllers and the result holder

The DataController family (`TreeItemDualRef`, `DataController`, `FuncDC`, `SymbDC`, ...) stays
intrusive: `TreeItemDualRef : SharedActor`. A config item owns its DC through `mc_DC`
(`DataControllerRef`, an intrusive `SharedPtr<const DataController>`); a `FuncDC` owns its argument
DCs (`m_Args`, `m_OtherSuppliers`) and its `OperationContext` (`std::shared_ptr`). The links back
are not owning: `OperationContext::m_FuncDC` is an in-repo `WeakPtr`, reset from both sides before
the `FuncDC` dies, and `s_DcMap` is a registry that `~DataController` erases itself from. Nothing
holds a `std::weak_ptr` to a DC, which is why DCs were left intrusive (plan section 14).

The boundary between the two systems is `TreeItemDualRef::m_Data`, a `DcRef`
(`tic/TreeItemDualRef.h`): one `std::variant` whose active alternative is the state.

| arm | state | holds |
|---|---|---|
| 0 | empty | nothing |
| 1 | IsNew | `NewResult`: owns the fresh cache root result, plus `m_KeptUnits`, owning refs to the domain and values units of the result's cache items, which have no other owner |
| 2 | IsOld, cache | `OldCacheSubItem`: owns the cache root of a sub-item, and borrows the sub-item itself |
| 3 | IsOld, config | `std::weak_ptr<const TreeItem>`: the tree owns the config item |
| 4 | IsTmp | `std::weak_ptr<TreeItem>`: an instantiation borrow at the calling site |

Accessors: `GetCurr()` returns an owning snapshot and is the safe way to read and hold the result.
`GetNew()` returns a mutable raw pointer and is `MG_CHECK`ed against the IsOld arms in Release too.
`GetOld()`, `operator->` and `GetCurrUlt()` return raw borrows (section 8). `operator bool`,
`IsNew()`, `IsOld()` and `IsTmp()` report which arm is set, not whether a weak arm is still alive.
Units that an operator creates through a unit creator are handed to the kind-1 holder with
`KeepAlive`; `CaptureResultUnits` collects the rest after `CreateResultCaller`; and
`GetOwnedSnapshot()` lets an `OperationContext` co-own them (`m_KeptResultUnits`, `m_KeptArgUnits`),
so a meta-thread `DoInvalidate` cannot free them under a running worker.

Supplier interest crosses the boundary the same way: `SupplInterestListElem` holds a TreeItem
supplier through a weak `InterestPtr<std::weak_ptr<const Actor>>`, and a DataController supplier
through an owning `SharedActorInterestPtr` (`m_DcValue`).

## 5. What stays intrusive

`SharedPtr<T>` (202 lines in the sources, `doc/code-audit-2026-09-27.md` 3.2) still owns the
DataController family, `AbstrDataObject` and the tile functors (`AbstrDataItem::m_DataObject`),
`AbstrCalculator`, `AbstrStorageManager`, `LispObj` (through `LispRef`) and the unit metrics and
projections (`UnitMetric`, `UnitProjection`). None of them is referred to by a `std::weak_ptr`, and
their ownership runs forward, so a control block would add memory and atomics without a liveness
gain. The in-repo `WeakPtr<T>` (92 lines) is a raw pointer with no liveness check (PLN-A05).

## 6. The guard

`tools/check-ptr-discipline.ps1` fails on a rogue `std::shared_ptr<TreeItem|Abstr*|Unit<...>>(raw)`
construction and warns on check-then-lock candidates. `analyze.bat` runs it since 6d2db64c8
(PLN-A06), and since 0563aa5a0 `batch\run_source_checks.bat` runs it with the other syntactic
checks from the `Test*Unit.bat` launchers and the setup scripts.

## 7. Known and accepted

What the pointer-safety review left alone on purpose, as of its last round (2026-07-03):

- Debug-only code and asserts that dereference a momentary lock; they are compiled out in Release.
- `AbstrBoundingBoxCache`: `g_BB_Register` is keyed by a raw `const AbstrDataObject*` and the cache
  keeps a raw `m_FeatureData`. The callers hold read locks, so this is hardening only, although a
  false hit after an address is reused remains possible.
- The C API in `AttrInterface.cpp` (then `attr_Interface.cpp`) has no in-template guard, so misuse
  dereferences null.
- `GetNew()` reads `kind()` and then the arm; a concurrent meta-thread `Clear()` in between is a data
  race, which now fails as `bad_variant_access` at the operation boundary instead of with a
  dangling pointer.

## 8. Open

- **DcRef kind 1**: split `NewResult` into 1a (single value), 1b (domain plus default values unit),
  1c (attribute with its two units) and 1d (general), so the common cases avoid the vector
  allocation (the TODO on `DcRef::NewResult`).
- **DcRef kind 2**: own the DataController that produced the cache root instead of the root
  object, so that a kind-2 holder also reaches the root's kept units. Deferred in the handoff of
  2026-06-29; nothing in the code marks it.
- **Two `// TODO ownership` stopgaps** of the six of July: `UsingCache.cpp` (`UpdateCache`, the
  `refItem` chain) and `stg/dll/src/AbstrStreamManager.cpp` (the current range item).
- **Raw borrows from temporaries**: `GetOld()` is `m_Data.get().get()`, a raw pointer taken from a
  temporary owning snapshot, valid only while another owner holds the item. On 2026-10-06 there are
  56 lines that call it besides its definition, 59 lines with `resultHolder->` (which is `GetOld()`)
  and 8 lines that call `GetCurrUlt()`, which takes its raw pointer from a temporary
  `GetCurrUltimateItem()`. Each rests on one of the rules of section 3; moving them to `GetCurr()`
  makes that explicit.
- **PLN-A05**: rename `WeakPtr<T>` to an observer type that does not promise a liveness check (or
  use `T*`), and record that DCs, data objects and Lisp objects stay intrusive.
- **Plan section 15** (follow-ups after the migration). Done: 15.3, the temporary instrumentation
  (bae0848ce), and 15.5, the wrappers (70c3f9422). Open: 15.4, retiring `GraphicObject::Sync`, the
  only reason `TreeItem::Reorder` is exported (`GraphicContainer::SaveOrder` still calls it), and
  15.6, a keyed sub-item map. Three items rest on premises that no longer hold:
  - 15.1, flatten `DataController` into `TreeItemDualRef` and drop its `Actor` base, assumes a DC
    does no invalidation; `DataController` overrides `DoInvalidate` and `DoUpdate`, and `FuncDC`
    overrides `DoInvalidate` again. Not a cleanup; do not pick it up.
  - 15.7, stop deriving `UnitClass` and `DataItemClass` from `TreeItemClass`: they do not.
    `UnitClass`, `DataItemClass` and `TreeItemClass` each derive from `Class` directly
    (`UnitClass.h`, `DataItemClass.h`, `TreeItemClass.h`). Do not pick it up either.
  - 15.2, retire the whole intrusive toolkit, contradicts section 14 of the same plan, which keeps
    the DC family intrusive; the other types of section 5 above use it as well, and the
    `newly_obj`/`existing_obj`/`no_zombies` tags that `make_shared_tree` takes live in
    `ptr/SharedPtr.h`. Only `WeakPtr<T>` can go (PLN-A05).
