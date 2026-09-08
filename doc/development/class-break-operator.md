# The class-break computation as an operator application (#1248)

Follow-up of #587, whose stage S3 left this as the last item writer.

## 0. Summary

`shv/dll/src/Theme.cpp` created an `OperationContext` item writer whose payload computed the class
breaks of a map view's generated classification. It was the last caller of
`OperationContext::CreateItemWriter`; storage reads had become operator applications in #587.

After this change the break attribute is an ordinary calculated item — an application of
`ClassifyNonzeroJenksFisher` — and the map view demands it like any other item. Two new pieces of
language surface carry it:

- `weeded_counts(A)`, the value-count table as a result of its own;
- a ternary signature on the eight existing `Classify*` groups, taking that table instead of
  scanning the attribute itself.

`OperationContext` keeps one kind of task. What that actually removes is smaller than the issue
assumed; §4 says why.

## 1. What it was

`Theme::Create(aNr, thematicAttr, featureLayerInfo, dv, doThrow)` found
`LayerInfo::ClassificationMissing` and called `CreateBreakAttr`, which created two plain mutable
desktop items: a `PaletteDomain` unit of `DEFAULT_MAX_NR_BREAKS` (= 8) classes and a `ClassBreaks`
attribute on it. Nothing calculated them. `Theme::Create` then posted a main-thread oper that
collected two futures by hand — the thematic attribute with its values units, and its domain unit
— and called `CreateItemWriter` with a lambda, parking the context in `Theme::m_ClassTask`.

The lambda (`CreateNonzeroJenksFisherBreakAttr`) scanned the attribute
(`GetWeededCounts`, at most `MAX_PAIR_COUNT` = 4096 pairs), classified into
`min(#pairs, 8)` breaks, and then, back on the GUI thread, called `paletteDomain->SetCount(nrBreaks)`,
filled the break attribute and rebuilt the palettes from the computed breaks.

Two of those steps are the reason the shape was what it was:

- **`SetCount`.** The number of classes is decided by the data. An operator may not resize an
  argument unit, and with `k` = 8 over fewer than 8 distinct values `ClassifyUniqueValues` pads the
  array with repeats of the last value — eight legend classes where three belong.
- **the palettes.** `CreateSystemColorPalette` uses the break values to find `bothSigns` and
  `hasZero`, which is what anchors a diverging ramp on zero (#1146).

## 2. Why the obvious shapes do not work

- **Fixed palette domain, existing binary operator.** Simplest, one scan, and wrong: it is exactly
  the padding case above.
- **Let the operator own its result domain**, as `unique` does (`clc/dll/src/Unique.cpp:372`).
  `CreatePaletteData` asserts `!domain->IsCacheItem()`, because the colour and label palettes are
  written as desktop items on that domain. The palette domain must stay a real tree item.
- **Give the palette domain a calculation rule** that sizes it (`range(uint8, 0b, uint8(min_elem(
  nrofrows(counts), 8)))`; every operator in it exists). This composes, but its count is then
  unknown when `Theme::Create` runs, and that function must hand back a theme *with* a palette:
  four callers, two of them with `doThrow=true`. It would turn layer creation asynchronous for a
  number the view can perfectly well set itself.

## 3. What it is now

Three desktop items, of which two are calculated:

```
ClassBreakCounts := weeded_counts(<thematic attr>)                              // a unit
PaletteDomain                                                                    // plain, 8 classes provisionally
PaletteDomain/ClassBreaks := ClassifyNonzeroJenksFisher(
        ClassBreakCounts/Values, ClassBreakCounts/Count, PaletteDomain)
```

built by `CreateNonzeroJenksFisherBreakItems` (`shv/dll/src/ShvDesktopData.cpp`). The rules are
text expressions naming the items by full name, the idiom shv already uses for generated items
(`PaletteControl.cpp`'s `pcount({})`, `TableControl.cpp`'s `id({})`).

Naming the counts table as an item, rather than repeating the `weeded_counts` subexpression in two
rules, is deliberate: the sharing is then a fact of the tree that can be inspected, not a property
of two `LispRef`s hashing alike.

`Theme::Create` is otherwise unchanged: the palette domain still has a usable count at creation
time, so the palette is still created synchronously in the `PaletteMissing` branch, and no call
site sees a different contract. All it does now is remember the counts unit and the view.

### The settle step

What the view still owes the classification happens once, in
`Theme::settleGeneratedClassification`, called from `Theme::PrepareThemeData` — which already
returns `AVS_SuspendedOrFailed` and is how a theme tells the view to come back later:

1. prepare the counts unit; not ready → suspend, the view retries;
2. `SetCount(min(#counts, 8))` on the palette domain under an `ItemWriteLock`, and `MarkTS`, so
   that anything derived from the provisional size is recomputed. This happens **before** the
   break attribute is demanded, so the classification runs once, against the settled count;
3. prepare the break attribute, read it with `GetValuesAsFloat64Array`, and rebuild the palettes
   from those breaks — preserving #1146's zero-anchored ramp;
4. clear `Theme::m_ClassCounts`, which is both the interest that kept the counts table resident and
   the "still to settle" flag — the analogue of the old `m_ClassTask.Clear()` on completion. The
   `ClassBreaks` rule names the table by full name, so the desktop item stays and its data is
   recomputed on demand if it is ever wanted again.

Sizing a plain desktop unit from the view is what the old continuation did too. The difference is
that it is no longer a payload racing the GUI thread: it is a step in the view's own prepare walk.

### How the resize reaches the legend

Worth writing down, because it is not the path it used to be. `AbstrUnit::SetCount` ->
`OnDomainChange` -> `AbstrDataItem::OnDomainUnitRangeChange` is guarded by
`GetCalculatorMember() ? IsDataBlock() : bool(m_DataObject)`. For the old *written* `ClassBreaks`
that held, and the data was copied into the resized array. A calculated `ClassBreaks` makes it a
**no-op**: the item is not resized in place, it is invalidated and recomputed.

What drives the legend is the timestamp instead: `MarkTS` on the palette domain -> `ClassBreaks` is
a `FuncDC` holding that domain as an argument -> `Theme::VisitSuppliers` visits `m_Classification`
and sees the newer TS -> `Theme::DoInvalidate` -> the layer invalidates -> the next pass rebuilds
the palette control at the new count. Observed: `ClassifyNonzeroJenksFisher ... n=8` and its
`area`/`count` columns, then the re-range, then `n=7`, and a seven-row legend.

The provisional eight rows still appear first, for the same reason as before: the palette control
is built from `LayerControl.cpp`, which needs only `GetPaletteDomain()`, available at 8 with no
classification computed.

### The operators

`weeded_counts(A: D->V) -> unit U { Values: U->V ; Count: U->UInt64 }`, shaped like `unique`:
`CreateResultUnit`, two members, count set in `mustCalc`, `DescribeSignature` describing a
complete member set. The body is the existing
`GetWeededCounts<ClassBreakValueType, typelists::num_objects, CountType>(adi, MAX_PAIR_COUNT)`.
`Values` is written with `SetValuesAsFloat64Array` through the values-unit range data — the same
round trip `FillBreakAttrFromArray` performs for a break attribute.

`MAX_PAIR_COUNT` is a property of the operator, not an argument: it is what the map view has
always applied, and making it settable would make the result depend on a number nobody sets.

`ClassifyCountsOperator` is the ternary sibling of `ClassifyFixedOperator`, instantiated for all
eight groups from the same `ClassBreakOperators<V>` struct. It rebuilds the
`ValueCountPairContainer` from the two arguments and calls the same `ClassBreakFunc`.

The two signatures agree below `MAX_PAIR_COUNT` distinct values and may differ above it: the
binary form counts every value (`GetCounts`), `weeded_counts` merges adjacent pairs at the cap.

## 4. What the rtc cleanup really removes

The issue expected the item writer to own `m_TaskFunc`, the `task_func_type` constructor and the
`m_KeptArgInterests` / `m_KeptArgItems` / `m_KeptArgUnits` retention added in `df732ab7`. Reading
the code, only the first two are its own:

- **Removed**: `OperationContext::CreateItemWriter` and the `OperationContext(task_func_type)`
  constructor. Every `OperationContext` is now FuncDC-bound, which is what
  `RefreshEstimateForAdmission`'s `if (!funcDC ...)` early return existed for; it is now an assert.
- **Kept: `m_TaskFunc`.** `ScheduleCalcResult` assigns an `OC_CalcResultFunc` to it
  (`OperationContext.cpp`, in the `ScheduleCalcResult` section). It is the payload holder of both
  paths, not the item writer's.
- **Kept: the `m_KeptArg*` retention.** It is filled in `Schedule(TreeItem*, allArgInterest, ...)`,
  and `ScheduleCalcResult` passes a non-empty `allArgInterests` through the same call. The comment
  claiming it was item-writer-only was wrong, and now says so.
- **Kept: `m_WriteLock`.** The calc path takes it in that same shared `Schedule`. Anyone reading
  #1248 as "remove the write lock with the item writer" would break every operator.
- **Kept: `connectArgs`' lookup-by-item branch.** A calc-path argument list can hold a plain item
  future too (`m_FuncDC->m_OtherSuppliers`), so it is not dead.

The scheduler simplification is therefore real but modest: one factory, one constructor, one
early return, and comments that no longer describe a second kind of task.

## 5. Verification

- `testcases/oper_classify_counts.dms` covers the operators offline: the counts table, and the
  ternary form against the binary form on the same data, once through a float values unit and once
  through an integral one with a non-zero lower bound — the branch where a lossy `Float64` round
  trip would show.
- Nothing in `testcases/` reaches `Theme::Create`, so the shv half is provable only by driving
  `GeoDmsGuiQt` on a config whose layer has no classification: open it, check that the legend has
  the number of classes the data supports rather than eight, and that a signed attribute still
  gets a ramp centred on zero.

## 6. Left open, deliberately

- **The settle is one-shot**, as the item-writer task was: `m_ClassCounts` is cleared and nothing
  re-derives the class count if the thematic data later changes. The old code was then stale in
  both the breaks and the count; this is stale in the count only, since `ClassBreaks` recomputes
  itself — which surfaces as padded duplicate breaks rather than as old values. Making it
  re-settle would mean keeping a non-interest handle to the counts unit and resetting on
  `DoInvalidate`.
- **The relayout rides on a following pass.** `settleGeneratedClassification` resizes the palette
  domain inside a draw visit (`GraphDrawer::DoLayer` -> `PrepareThemeSetData`) and returns
  `AVS_Ready`, so the invalidation it causes is picked up by whatever pass comes next, rather than
  by one it requests. The old code did this from `PostGuiOper`, outside any visit. It converged in
  every run measured here; it has not been proved to converge when the counts arrive late. The fix,
  if wanted, is to invalidate the `act` the visitor already passes.
- **The classification runs twice** on the first view: once against the provisional eight classes,
  once against the settled count. `weeded_counts` is shared, so the second run is Jenks-Fisher over
  at most `MAX_PAIR_COUNT` pairs, but it is work the old code did not do.

## 7. Documentation

`weeded_counts` and the ternary `Classify*` form are user-visible language surface and belong on
the wiki, on the `Classify*` page with a page of its own for `weeded_counts`. The item-writer
removal is internal and belongs in the issue debrief instead.
