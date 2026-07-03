# Pointer-safety review — unguarded / dangling dereferences after the std-ptr ownership migration

Date: 2026-07-02. Branch: `refactor_ownership` (HEAD `4cf1238e`). Scope: rtc, tic, stx, stg, clc, geo,
shv, qtgui, exe. Method: pattern sweep (`.lock().get()`, `.lock()->`, rogue `std::shared_ptr<family>(raw)`,
`GetNew()/GetOld()`, `no_zombies`, `lock_or_cancel`, temporary-`.get()` escapes) + four parallel deep reviews
(tic core; shv GUI; stg/geo/clc; TreeItemDualRef/DcRef).

**STATUS 2026-07-02 (same day, follow-up commit): FIXED.** All 4 HIGHs, all MEDIUMs, and the actionable
LOWs below were applied across 35 files (see the commit that adds this paragraph). Verified: full Debug
`all22.sln` build green + `batch\TestDebugUnit.bat` green (matches the 61-config baseline of `cc60a08f`).
Intentionally NOT fixed: Explain.cpp raw ultimate-unit snapshots (prior architect decision: acceptable,
kept alive by m_DataItem interest); debug-only sites (UsingCache TestOrder, MG_DEBUG locals, assert-only
`.lock().get()` comparisons); AbstrBoundingBoxCache raw keys (pre-existing, callers hold locks); the
policy decisions in "Highest-leverage fixes" item 8 (null contracts documentation, DataLock null rejection)
remain open as design choices. Key structural additions: `GetCurr()` snapshots now used in
FuncDC/SymbDC/DataController/Checker/OperationContext; `TreeItemDualRef::GetOwnedSnapshot()` +
`OperationContext::m_KeptResultUnits` close the invalidate/cancel unit-liveness window; `InterestPtr`
inc/dec now hold the weak lock across the count mutation; `GetBackRefStr()` is self-guarding.

**FOLLOW-UP 2026-07-03 (second commit): the open design decisions were RESOLVED and implemented**
(build + TestDebugUnit green again, result file identical to the baseline):
- item 6 → `AbstrDataItem::GetDomainUnitOrThrow()/GetValuesUnitOrThrow()` (guaranteed non-null owning
  `SharedUnit` or item error); adopted at the flagged compute-path sites (ValueGetter, DataArray,
  TreeItem::SetReferredItem/UpdateMetaInfoImpl, XmlTreeOut null-skip, OperAttr{Uni,Bin,Ter}.h,
  CastedUnaryAttrOper.h, RegCount, DataWriteLock::Commit).
- item 7 → `DataReadLock/DataWriteLock::GetRefObjOrThrow()/GetItemOrThrow()` (empty lock fails at use,
  not at deref); adopted at RegCount VisitImpl, OperPropValue, Commit MG_CHECKs.
- item 8 → `GetNew()` is MG_CHECK(!IsOld())-enforced in release and const_cast-free: `DcRef::NewResult`
  now stores the root as `std::shared_ptr<TreeItem>` (`m_Root`) + `m_KeptUnits`. (Strict IsNew() was NOT
  used: null-on-monostate and the IsTmp borrow are legitimate GetNew states — the create-parentless-root
  pattern and template instantiation rely on them.)
- item 9 → Explain.cpp `m_UltimateDomainUnit/m_UltimateValuesUnit` + queue-entry units are
  `std::weak_ptr<const AbstrUnit>`, lock-at-use, soft-fail in render paths.
- quick wins: expiry-tolerant `UsingCache::TestOrder`; `DiscrAlloc::GetPartitioningUnit()` returns owning
  `SharedUnit`; DataWriteLock-ctor configItem re-own via `no_zombies` (graceful teardown degrade);
  `TreeItem::GetBackRef()` returns owning `SharedTreeItem` (loop callers hold it);
  `tools/check-ptr-discipline.ps1` = the standing audit (rogue raw-ctor grep FAILs, check-then-lock WARNs).

## VERIFICATION ROUND 2026-07-03 (third commit) — "are all derefs now known valid?"

Four independent read-only verifiers re-audited HEAD (residual `.lock().get()` 42 sites; the ~290-site
`GetAbstr*Unit()->` population; adversarial review of the two fix commits; `GetOld()`/empty-getter
residuals ~75 sites). Verdict: every audited class verifies site-by-site or rests on a NAMED, PROVEN
invariant. The named invariants (documented here as the validity basis):
1. Same-full-expression pinning: a `.lock()` temporary pins its target to the end of the statement.
2. Parent-owns-child: a live item implies live ancestors; destruction is top-down on the meta thread.
3. Kind-1 owning arm: `resultHolder->`/`GetNew()` derefs after `SetNew` cannot expire; `Clear()` empties
   the kind (so `if (!resultHolder)` re-arms), it never leaves a dangling arm.
4. Fresh-arm-per-call: `CanResultToConfigItem` operators re-arm from live args each call (verified per
   operator) or never deref the arm in mustCalc.
5. OC unit snapshots: `m_KeptResultUnits` AND (new this round) `m_KeptArgUnits` co-own the kind-1
   kept-alive units of the result and of every ARG DC for the OC's whole run — closing the confirmed
   meta-thread `DoInvalidate->Clear()` mid-compute window for args (the invalidation path is not
   consumer-aware: DetermineState->InvalidateAt->DoInvalidate->Clear has no guard against running OCs).
6. Cancel-on-expiry: worker-side dead weak = `throwTaskCanceled()` / `Fail(...)`, never a silent skip.

Fixes this round (all verifier findings):
- FuncDC::MakeResult/CallCalcResult now `Fail("result item no longer exists")` before returning empty
  (callers rely on "null => WasFailed or DidSuspend"; SymbDC already did this).
- SetReadLocks/connectArgs dead-supplier paths `throwTaskCanceled()` instead of success-shaped
  empty-return/skip (which would have run the operator without read locks / waiter ordering).
- `HandleFail(null)` transitions the OC to exception (was: unconditional deref).
- `TreeItem::DoWriteItem` null-checks the `CalledCalcHandle` result (suspend path).
- `m_KeptArgUnits` (OperationContext) — the LEG-2 arg-unit snapshot, symmetric to `m_KeptResultUnits`.
- Guarded the two nullable-`GetUsing()` callers (DedicatedAttrs.cpp, TreeItemProps.cpp); UsingCache
  OnItemAdded all-usings-expired now overwrites instead of silently dropping the new child.
- Poly2GridOper vpi-ctor unit fetch via OrThrow; dormant MG_DEBUG_OPERATIONS TOCTOU fixed.

Remaining known-and-accepted (NOT migration debt):
- C-API (`attr_Interface.cpp`) has no InTemplate guard — a null-deref on API misuse; pre-existing.
- `GetNew()` kind()-then-`std::get` under a concurrent meta-thread `Clear()` is a (pre-existing-class)
  data race; the new code fails safer (bad_variant_access into the OC boundary vs a dangling raw).
- Debug-only asserts that deref momentary locks (compiled out in release).

Verified: Debug all22.sln green; TestDebugUnit result file identical to the green baseline (3rd time).

Severity legend: **HIGH** = reachable null/dangling deref in normal operation; **MEDIUM** = reachable under
teardown / cancellation / concurrent destruction (exactly the windows the recent drain-hang work exercised);
**LOW** = assert-only, debug-only, or theoretical.

Positive results first:
- The rogue-control-block audit grep is **clean** (0 sites).
- No `.lock()->` single-token derefs remain; ~90% of the 329 shv lock sites use the guarded
  `auto p = w.lock(); if (!p) ...` idiom.
- The ~110-site `resultHolder.GetNew()`-as-parent-during-CreateResult class is **safe by construction**
  (verified: before first assignment `GetNew()` is null and `Create*Unit/CreateCacheDataItem` accept a null
  context as "parentless cache root"; after `SetNew` the arm is kind-1 owning for the rest of CreateResult).
- qtgui is clean (`m_root`/`m_current_item` owning; other `.get()`s are Qt widget unique_ptr plumbing).
- `lock_or_cancel` users (DiscrAlloc compute phase, Overlay, RegCount:129) are the exemplary pattern.

## Cross-cutting root causes (fixing these closes most findings)

1. **Check-then-lock TOCTOU** (~25 sites, mostly `tic/dll/src/TreeItem.cpp`): `if (!w.expired())` in one
   statement, `w.lock()->…` in a later one. Nothing pins the target between check and re-lock. Note also:
   a parent's `weak_from_this()` expires at *destructor entry*, so during teardown children observably have
   expired `m_Parent` while still alive. Mechanical fix: `if (auto p = w.lock()) { use p everywhere }`.
   (Same-statement multi-locks are safe: the first lock's temporary pins the object to the end of the full
   expression.)
2. **`DcRef`/`TreeItemDualRef`: kind checks masquerading as liveness checks.** `operator bool` / `IsOld()` /
   `HasBackRef()` report which variant arm is set, not whether a weak arm (kind 3 IsOld-config / kind 4 IsTmp)
   is still live. Code writes `if (m_Data) m_Data.get()->X()` believing it guarded. The safe owning accessor
   `GetCurr()` has **zero call sites** in the whole repo. Fix pattern: hoist `auto curr = GetCurr(); if (!curr) …`
   at function top and use it throughout.
3. **Null-capable raw-returning accessors that callers deref unchecked**: `GetAbstrDomainUnit()` /
   `GetAbstrValuesUnit()` (null on worker thread + expired weak — no FindUnit fallback off the meta thread),
   `AbstrCalculator::GetHolder()`, `TreeItemDualRef::GetOld()/operator->`, `GetBackRefStr()`,
   `InterestPtr<weak>::get_ptr()` (RtcBase.h:352), and `lock_or_cancel(...).get()` returned as raw
   (DiscrAlloc.cpp:635). Decide + document each null contract; add checked variants for hot callers.
4. **`DataReadLock`/`DataWriteLock` silently accept a null item** (DataLocks.cpp:281), converting an unchecked
   `weak.lock().get()` at a lock site into a *deferred* null deref at first use of the lock — much harder to
   diagnose than an immediate `lock_or_cancel` throw.

## HIGH

- **tic/dll/src/TreeItem.cpp:3080 — `GetNextVisibleItem()`**: `m_Parent.lock()->mc_RefItem.lock().get()`
  guarded only by debug `dms_assert(!m_Parent.expired())` (3075). Public C API
  (`DMS_TreeItem_GetNextVisibleItem`, TicInterface.cpp:395), used in iteration loops (DedicatedAttrs.cpp:68,
  AbstrStoragemanager.cpp:1085/1092, XmlTreeOut.cpp:1174, shv GraphicContainer.cpp:88). Any call on a
  parentless item null-derefs in Release. Fix: `auto parent = m_Parent.lock(); if (!parent) return nullptr;`.
- **shv/dll/src/IndexCollector.cpp:87-92, 108-109**: `auto res = m_DC->CallCalcResult();` then
  `AsDataItem(res->GetOld())` — `CallCalcResult` returns an **empty** FutureData on failure/suspension
  (MoreDataControllers.cpp:367/390/401/451); `InterestPtr::operator->` is assert-only, so Release calls
  `GetOld()` on null `this`. GUI table-draw path; a plain config error in the indexed attr crashes instead of
  reporting. Fix: null-check `res`, then `auto item = res->GetCurr(); MG_CHECK(item);` and hold it.
- **shv/dll/src/LayerControl.cpp:286, 293-309 — `LayerControlBaseDragger::Exec`**: `srcOwner` / `dstOwner`
  from `GetOwner().lock()` never null-checked, then deref'd (`MoveEntry`, `NrEntries`). Layer sets mutate
  between drag events (the guards at 279-280 prove mid-drag death is expected). Fix: bail on null after each lock.
- **shv/dll/src/TableHeaderControl.cpp:179-187 — `ColumnHeaderDragger::Exec`**: if the dragged header expired
  mid-drag (column removed by `DoUpdateView:323`) while `m_HooverObj` is non-null,
  `debug_cast<…>(GetTargetObject().lock().get())->GetDic()` derefs null; `dstLayer` (null until `SetDic`) and
  `dstOwner` (185→187) likewise unchecked. Fix: lock+check target at top; null-check `dstLayer`/`dstOwner`.

## MEDIUM — tic core

- **`mc_RefItem` expired()-then-relock family** (fix all with one named-lock rewrite):
  TreeItem.cpp:386-387 (`ResetAllKeepInterest` — runs during EnableAutoDelete teardown, worst-timed),
  1278-1282 (`SetReferredItem` swap: mid-statement race makes clause 3 deref null), 1367-1368, 1380-1381,
  1619-1622, 1640-1643, 2367-2382 (eight separate re-locks; use the owning snapshot taken at 2386),
  2602-2605, 3067-3069 (noexcept), 3210-3212 + 3292-3295 (`VisitSuppliers`: null flows into `visitor(...)`,
  which requires non-null — use the null-safe `.Visit()` as at 3288), 4648-4649 (`StopInterest`, **noexcept**
  → a race is std::terminate).
- **`m_Parent` same pattern**: TreeItem.cpp:741-742, 875-876, 1115-1116, 4781-4800 (config-file name/line/col
  — error-reporting path, runs on worker threads).
- **tic/dll/src/TreeItemDualref.h:172 — `GetBackRefStr()`** (found independently by two reviewers): unguarded
  `p->m_BackRef.lock()->GetSourceName()` — `p` null on expired weak arm, and `m_BackRef` is reset by the meta
  thread (TreeItem.cpp:1279) while this runs in worker-thread error/trace reporting (`getContext` fires on
  every ≥MajorTrace report; callers: OperationContext.cpp:2659, Dijkstra.cpp:618, Connect.cpp:327/692/986,
  BoostPolygon.cpp:186/863/1696, GridDist.cpp:275, OperPolygon.cpp:1919). The `HasBackRef()` pre-check is a
  separate-statement TOCTOU. Fix: make the member self-guarding (null → empty SharedStr).
- **FuncDC/SymbDC unguarded `m_Data.get()->…` with possible expired kind-3 arm**:
  MoreDataControllers.cpp:330, 395-397, 417, 438-445, 454, 763, 801, 810-811, 818; SymbDC::CallCalcResult
  1010-1020 (SymbDC *always* holds arm 3 for config items). Trigger: config item destroyed (config edit,
  template drop, teardown) without the DC invalidated first. Fix: hoist `GetCurr()` snapshot per function.
- **tic/dll/src/DataController.cpp:244, 251 — `Inc/DecDataInterestCount`** deref `m_Data.get()` unguarded;
  the three callers gate on kind only. `StopInterest` (276) is `noexcept` → null deref = std::terminate during
  interest teardown. `Set` (158) same exposure. Fix: `if (auto p = m_Data.get()) p->Inc/DecInterestCount();`.
- **tic/dll/src/OperationContext.cpp:1814-1816, 1845** — worker-thread supplier derefs: `GetOld()` then
  assert-only, `GetCurrRangeItem().get()` unguarded (1845 fully unguarded). Sibling 1443-1446 shows the
  correct guarded form. Also **1229/1413 → 1986, 2051**: `m_Result` can be constructed null (expired weak arm)
  and is deref'd behind asserts. Fix: guard like 1443; `MG_CHECK(m_Result)` fail the OC gracefully.
- **Invalidate/cancel unit-liveness race**: `TreeItemDualRef::Clear()` (DataController.cpp:218) drops the
  kind-1 `m_Owned` kept-alive **units** while a worker OperationContext still computes with raw borrows —
  `OperationContext::m_Result` owns only the cache root, not the units. Worker's `GetAbstrValuesUnit()` then
  locks null (e.g. OperAttrUni.h:76). This violates the "DcRef owns unit-liveness" invariant across the cancel
  window. Fix: OC snapshots the whole `NewResult` owned vector at schedule time.
- **rtc/dll/src/ptr/InterestHolders.h:149, 189 (+ RtcBase.h:352)** — weak-arm InterestPtr Inc/Dec run on a raw
  after the momentary lock inside `get_ptr()` is released (UAF window; race-only since both sides are usually
  meta-thread). Expired targets ARE correctly skipped (dtor decrement on dead supplier is a no-op — verified).
  Fix: `if (auto p = m_Item.lock()) p->Inc/DecInterestCount();` (hold the lock across the call).
- **clc/dll/src/Checker.cpp:53-67** — worker-thread `GetOld()`/`operator->` on an arm-2/3 holder
  (`resultHolder = arg1` makes it weak for config items) with no liveness guard. Fix: `GetCurr()` + MG_CHECK
  at top of the mustCalc block.
- **AbstrDataItem.cpp:119-134 — `GetAbstrDomainUnit()/GetAbstrValuesUnit()` null contract**: worker thread +
  expired weak → returns null (no FindUnit re-resolution off the meta thread); meta thread `InTemplate()` →
  FindUnit returns null instead of throwing. Callers tree-wide deref unchecked (ValueGetter.h:66,
  DataArray.cpp:874/881/931, TreeItem.cpp:1248/2350, XmlTreeOut.cpp:800/827, …). Fix: checked variants +
  document the contract at the declaration.
- **AbstrCalculator.h:150 — `GetHolder()`** returns `m_Holder.lock().get()`; deref'd unchecked at
  AbstrCalculator.cpp:877, 904-923, 1252, 1413 — precisely the error/failure paths, on calculators that can
  outlive their holder (that's why m_Holder is weak). Fix: `lock_or_cancel`-style guard at top of those.
- **Explain.cpp:65-66, 73-74, 536-543** — raw `const AbstrUnit*` ultimate-unit snapshots (+ `m_Queue` raws)
  stored in `CalcExplImpl`, which persists across GUI renders (`Init` early-returns when studyObject+TS
  unchanged); liveness chain (m_DataItem → mc_DC → DcRef KeepAlive) breaks on invalidation between renders
  and `ProcessQueue` compares/derefs before re-Init. Fix: do the TODO — `std::weak_ptr<const AbstrUnit>`
  members, lock+check per use, fail soft.
- **UsingCache.cpp:60-66, 507-519** — `CompareLtWeakItemId` derefs `lock_raw(...)` unguarded in
  sort/lower_bound/set_union over caches that legitimately hold expired entries (file's own header comment
  says so; subtree destruction does NOT call `OnItemRemoved` for sibling caches). Also `UpdateCache:427-437`
  unpins previous `refItem`s mid-chain. Fix: expiry-tolerant comparator (expired → ID 0 / filter first);
  guard 512/519 like 462.
- **Empty `GetCurrUltimateItem()/GetUltimateItem()/GetCurrRangeItem()` results deref'd unchecked**
  (helpers correctly return empty on mid-destruction items): AbstrDataItem.cpp:167, 175, 708, 737;
  AbstrUnit.cpp:656 (raw additionally stored in `UnitProjection`), 798, 851; OperationContext.cpp:1445;
  TreeItem.cpp:3995, 4230-4231, 4263. Fix: named local + null-check.
- **XmlTreeOut.cpp:673, 819, 839** — loop-carried raw `di = AsDataItem(di->GetReferredItem()).get();`
  (owning temporary dies at the semicolon). Meta-thread-only today; fix with an owning loop variable
  (the pattern UsingCache.cpp:412 already uses).

## MEDIUM — stg / geo / clc

- **geo/dll/src/OperDistrict.cpp:156 (`DiversityOperator::CreateResult`)** — `GetCurrRefObj().get()` fetched
  **before** `DataReadLock arg1Lock` (159); nothing pins the data object in the gap; `inputGrid->GetDataRead()`
  (166) derefs. Sibling `DistrictOperator` (72-73) has the correct order. Fix: lock first. (HIGH-leaning.)
- **geo/dll/src/RegCount.cpp:273** — `DataWriteLock(ri.m_Result.lock().get(), …)`: null tolerated by the ctor,
  deferred crash at VisitImpl:127 / Commit:287 if the result subtree is dropped between CreateResultCaller and
  CalcResult (cancel/teardown). Fix: `lock_or_cancel(ri.m_Result)` held across the lock (as line 129 does).
- **gdal two-step weak re-own UAF**: stg/dll/src/gdal/gdal_vect.cpp:2196, 2558, gdal_base.cpp:1022 —
  `make_shared_tree(m_DataHolder.get_ptr(), no_zombies{})`: `get_ptr()` = `weak.lock().get()` (momentary),
  then `weak_from_this()` runs on the raw **after** the momentary owner died. Fix: lock once
  (`std::weak_ptr<…> w = holder; auto adi = w.lock();`) or add `InterestPtr::get_shared()`.
- **gdal_vect.cpp:2196, 2558 — `adi_n` not null-checked** (gdal_base.cpp:1022 shows the guarded form):
  2196 → empty DataReadLock in vector → null `operator->` at 2209; 2558 → `WriteGeometryElement(adi_n.get())`
  derefs immediately. `LayerIsReadyForWriting` (2633) is a check-then-use gap across the whole layer write.
  Fix: null-check, `throwTaskCanceled()` on expired.

## MEDIUM — shv (teardown / disconnect / view-close races)

- **SelCaret.cpp:52** — `GetOwner().lock()->GetDataView().lock()->XOrSelCaret(diff)`: two unguarded derefs.
- **ViewPort.cpp:1231** — `GetDataView().lock()->m_ControllerID` unguarded (command-enable query can race close).
- **GraphicRect.cpp:105-107** — `GetTargetVP().lock()` has an explicit empty path (no MapControl/overview);
  the two sibling users guard, this one derefs.
- **GraphicLayer.cpp:125-126 (`SetActive`)** — `dv->OnCaptionChanged()` unguarded; invoked from layer-set
  shuffle/remove and theme changes on possibly-disconnected layers.
- **DataItemColumn.cpp:525-530, 1342-1349** — the only 2 of 38 `GetDataView().lock()` sites in the file
  without guards.
- **ScrollPort.cpp:290-292 (`SetScrollY`)** — unguarded; same file guards at 90, 246, 391, 401, 489.
- **EditPalette.cpp:118-119** — `m_PaletteDomain.lock()->GetDisplayName()` right after `make_weak_tree(newDomain)`,
  which returns an EMPTY weak for null *or non-std-owned* input (parentless cache units are a known class).
- **GraphVisitor.cpp:728** — `dic->GetTableControl().lock().get()->GetIndexAttr()` unguarded; the base-class
  override of the same method (line 284) explicitly guards + returns GVS_Continue, i.e. the code's own
  contract says null is possible.
- **ViewPort.cpp:271, 1393-1397** — unguarded derefs of weak registry map entries (`gc.second.lock()->…`)
  while adjacent loops over the same maps guard (273-277, 283-285); GridCoord dtor erases by key so a same-key
  successor can drop/duplicate entries.
- **GraphVisitor.cpp:481 (`GraphObjLocator::Locate`) + LayerControl.cpp:246, TableHeaderControl.cpp:141** —
  returns `m_TheOne.lock().get()`; both callers immediately `->shared_from_this()`. Null currently
  accidentally unreachable. Fix: return the shared_ptr; null-check in callers.

## LOW (selection)

- `GetNew()`'s `dms_assert(!IsOld())` (TreeItemDualref.h:140) — Release hands out a *mutable* pointer to a
  shared cache/config item on kind 2/3 (const violation, not a null issue).
- `FuncDC::GetArgs` (MoreDataControllers.cpp:578-583) can push a null ArgRef if MakeResult nulls without Fail;
  operators deref args behind asserts. Falls away when the FuncDC MEDIUM is fixed.
- `DataController::IsCalculating` (DataController.cpp:559) — kind-checked, not liveness-checked.
- ValueGetter.h:49/61/66 — weak `m_Adi` locked unguarded in `Create()`; synchronous with args pinned, but the
  weak buys nothing (IndexGetterCreator's `m_Adi` is shared — copy that).
- Poly2GridOper.cpp:524/552/651/668 — `m_PolyAttr.lock()` raws used across whole tile loops; backed implicitly
  by the operator-arg frame + arg1Lock; `lock_or_cancel` at scope top would make it explicit.
- DiscrAlloc.cpp:635 — `lock_or_cancel(...).get()` returned as raw (shape defeats the helper's purpose).
- AbstrBoundingBoxCache (g_BB_Register raw keys + raw `m_FeatureData`, pre-existing): cache can outlive the
  read lock that pinned the data object; ABA false-hit possible. Current callers hold locks; hardening only.
- GraphicObject.cpp:847-849 `IsOwnerOf` derefs unchecked null-capable arg; ActivationInfo.h:55 + DataView::Activate
  assert-only; GraphVisitor.cpp:900; LayerControl.cpp:331-338; ItemSchemaView.cpp:119-121 (assert-then-relock);
  IndexCollector.cpp:54-55 (`existing_obj` can throw bad_weak_ptr into the draw's catch — acceptable, noted);
  PaletteControl.cpp:75-76; DataLocks.cpp:292 + Explain.cpp:485 (`existing_obj` on a mid-destruction item
  throws bad_weak_ptr — `no_zombies` + check would degrade gracefully); MemoryMappedDataStorageManager.cpp:102-103;
  UsingCache.cpp:157; TreeItem.cpp:2675 (`GetBackRef` momentary raw return); OperationContext.cpp:1892-1894
  (MG_DEBUG only); AbstrCalculator.cpp:566-582/627-634 + OperGroups.cpp:496-497 (assert-only GetOld chains).

## Highest-leverage fixes, in order

1. Sweep the `expired()`-then-relock TOCTOU family (~25 sites, mostly TreeItem.cpp) into
   `if (auto p = w.lock())` named locks. Mechanical, closes the biggest MEDIUM class.
2. Make `GetCurr()` the actual access path in FuncDC/SymbDC/DataController interest functions and Checker
   (hoisted owning snapshot + null-handle per function); make `GetBackRefStr()` self-guarding.
   Closes the DcRef kind-vs-liveness class (M1-M5, M8).
3. Fix the 4 HIGHs (TreeItem.cpp:3080, IndexCollector, the two drag `Exec` handlers) — small local guards.
4. InterestPtr weak Inc/Dec: hold the lock across the count mutation (InterestHolders.h).
5. OperationContext: snapshot the kind-1 `m_Owned` unit vector at schedule time (cancel-window unit liveness).
6. gdal: single-lock accessor for `m_DataHolder` + null-checks at 2196/2558; OperDistrict.cpp:156 lock order;
   RegCount.cpp:273 `lock_or_cancel`.
7. shv stragglers: mirror the guards the surrounding files already use (12 sites listed above).
8. Policy decisions: null contract of `GetAbstrDomainUnit/GetAbstrValuesUnit/GetHolder`; whether
   `DataReadLock/DataWriteLock` should reject null items in operator code; Explain.cpp weak members (the TODO).
