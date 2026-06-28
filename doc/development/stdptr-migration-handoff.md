# std-pointer ownership migration — session handoff

Branch: `refactor_ownership`. This continues the migration of the TreeItem family off the intrusive
`SharedPtr<T>`/`WeakPtr<T>` onto std ownership. Read this together with
`doc/development/std-ptr-migration-plan.md` (§4 = the `DcRef` variant) and `CLAUDE.md` (build rules).

## Goal & invariants

The point of the operation is to **harden ownership**: no dangling raw pointers, no transient
liveness checks used in `if`s.

- `TreeItem` derives from **`Actor`** (NOT `SharedActor`) — it has **no intrusive refcount**. Family =
  `TreeItem`, `AbstrUnit`, `AbstrDataItem`, `AbstrParam`, `Unit<…>` and subclasses.
- `TreeItemDualRef` / `DataController` **intentionally stay `SharedActor`** (intrusive). They are NOT
  TreeItem-family. Other non-family types that stay intrusive: `AbstrDataObject`, `DataArray<…>`,
  `TileFunctor<…>`, `AbstrCalculator`, `AbstrStorageManager`, `SharedActor`.

## Wrapper types (`rtc/dll/src/ptr/SharedTreePtr.h`)

- `shared_tree_ptr<T>` — **owning**. `.get()/.get_ptr()/operator->/explicit operator bool/operator==(raw)`;
  tag ctors `newly_obj`/`existing_obj`/`no_zombies`. **No implicit raw ctor and no implicit `operator T*`**
  (both were tried and removed on purpose — every raw-needing site must be classified explicitly).
- `weak_tree_ptr<T>` — **non-owning**, drop-in for the old `WeakPtr`: raw-ptr ctor,
  `operator=(raw|nullptr|shared)`, `.get()/.get_ptr()/operator->/explicit operator bool/operator==(raw)/.reset()`
  — all via a momentary `lock()` (null if expired).
- **`ICanChoosePtr` was a temporary placeholder and has been DELETED. Never reintroduce it.**

## `DcRef` — `TreeItemDualRef::m_Data` (`tic/dll/src/TreeItemDualref.h`)

Replaces the old `(m_Data WeakPtr + m_OwnedData SharedTreeItem)` split. One member, a variant whose
`index()` is the state:

```
std::variant<
  std::monostate,                  // 0 empty
  std::shared_ptr<TreeItem>,       // 1 IsNew      — root cache result; DualRef is primary owner
  std::shared_ptr<const TreeItem>, // 2 IsOld-cache— owned ("like new")
  std::weak_ptr<const TreeItem>,   // 3 IsOld-config— tree owns it; lock to use
  std::weak_ptr<TreeItem>          // 4 IsTmp      — instantiation borrow; lock to use
>
```

- `get()` returns an **owning** `shared_tree_ptr<const TreeItem>` (weak arms lock into a held snapshot →
  no dangling). Callers MUST hold it: `if (auto p = m_Data.get()) p->…`.
- State = `kind()` (variant index). `clear()`, `use_count()` (owning arms only).
- The transient `has_ptr/is_null/get_ptr/operator bool/operator->` were **removed** from `DcRef`.
- `TreeItemDualRef` keeps `GetCurr()` (owning), `GetNew()/GetOld()/operator->` (raw borrows — valid only
  while an owner outlives the use; the next hardening candidate), and `IsNew/IsOld/IsTmp` from `kind()`.

## Why `m_Data` is a variant and not a plain `std::weak_ptr` (FAQ)

A pure `weak_ptr<const TreeItem>` works for **IsTmp** (the call site owns the tmp during use) but NOT for
**IsNew**: a freshly `make_shared`'d, parentless cache result has no other owner; the DualRef IS its
primary owner, and its lifetime (= the DataController = the config item's `mc_DC`) outlives the call site,
so ownership cannot be pushed there. Hence owning arms for IsNew/IsOld-cache, weak arms for
IsOld-config/IsTmp.

## Already done (whole repo, outside what's noted below)

- All intrusive `SharedPtr/WeakPtr<TreeItem-family>` and all `ICanChoosePtr` eliminated.
- Interest subsystem: `SupplInterest.h` → `InterestPtr<std::weak_ptr<const Actor>>`; `Actor::weak_from_actor()`
  virtual (TreeItem overrides via `weak_from_this`); DataControllers are NOT retained in the supplier list
  (their interest rides the ownership chain — `mc_DC` via `TreeItem::StartInterest`, arg DCs via
  `FuncDC::StartInterest`, plus the result-TreeItem entries). `InterestRetainContextBase::Add` and
  `UpdateMarker::ChangeSourceLock` take `const Actor*` now.
- Units `m_DomainUnit/m_ValuesUnit` → `weak_tree_ptr` (lock-at-use); `UsingCache` `m_Usings`/`m_SortedItemCache`
  → `vector<weak_ptr>` with a lock-aware comparator; `TileFunctorImpl::m_ResultAdi` → `weak_tree_ptr`.
- `TreeItem.h` members `m_Parent`/`m_BackRef`/`mc_RefItem`/`mc_OrgItem` → `weak_tree_ptr<const TreeItem>`.
- `ExtLockMgr` external `AddRef/Release` → owning `std::multiset<SharedTreeItem>` pin registry.

## Compile-cascade fix rules (from the architect)

- **C2440/C2664** (smart-ptr/`DcRef`→raw): BORROWED (passed/transient/compared) → append `.get()`;
  HELD (stored) → change the holder's type to `weak_tree_ptr<X>`. Never store a raw from a smart ptr.
- **C2679/C2678** (`=`/op from raw): **reroute the source** — make the producing local/function hand over a
  `shared_tree_ptr`/`weak_tree_ptr` from construction, not a raw rewrapped via `existing_obj`.
- **`GetRefCount()` on a TreeItem**: gone → `p->weak_from_this().use_count()` in debug/assert, or relax.
- **C2665 on `std::set/std::map<std::shared_ptr<const TreeItem/AbstrUnit>>`** keyed by raw → wrap the key
  `shared_tree_ptr<const X>(rawkey, existing_obj{})`.

## Immediate next steps

1. The last verifying Debug build was **314 errors in DmTic** (`build_debug_stdptr4.log`, the borrowed/held
   audit after the implicit-raw shortcut was removed). Background subagents were applying the rules; their
   edits may be partially landed — **re-grep / rebuild; don't trust prior in-flight state.**
2. Adapt the ~40 `m_Data.get()`/`m_Data->`/`bool(m_Data)` sites in `DataController.cpp` /
   `MoreDataControllers.cpp` to the owning `DcRef::get()` (e.g. `MakeResult`'s `return m_Data;` →
   `return m_Data.get();`).
3. Rebuild and iterate to green:
   `MSBuild.exe all22.sln -p:Configuration=Debug -p:Platform=x64 -m -nr:false` (VS18 msbuild only — never a
   standalone `.vcxproj`; it scatters vcpkg caches). Fix each error per the rules.
4. Smoke-test `reverse.dms` under cdb (`bu USER32!MessageBoxW`, `bu ucrtbased!wassert`, `.kill`), then run
   `batch/TestDebugUnit.bat` to green.

## Constraints

Build only via VS18 msbuild on `all22.sln` (Debug). **Never `git push`.** Edit in place (no worktrees).
Compiler pinned MSVC 14.50 (14.51 miscompiles geos). Update memory `project_stdptr_cascade_triage.md`.

## Delegation guidance

The cascade is large but rule-driven, so fan out to subagents — but agents can't run the canonical build,
so the loop is: **you build → bucket errors by file → delegate file-clusters with the rules → you rebuild
to verify.** Keep the intricate, cross-cutting pieces (DcRef/SupplInterest/interest accounting,
`DataController`/`MoreDataControllers` `m_Data`) yourself; delegate the mechanical borrowed/held `.get()` vs
`weak_tree_ptr` edits in leaf operator/storage/gui files. Tell agents: read `build_debug_stdptr<N>.log`,
grep for their files, apply the rules, do NOT build, mark anything ambiguous `// TODO ownership:` + `.get()`
stopgap, and never touch `*InterestPtr` typedefs or non-family pointers.
