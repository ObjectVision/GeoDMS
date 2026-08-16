# Collapsing the Unit&lt;V&gt; and TileFunctor&lt;V&gt; intermediate hierarchies

Branch `lookahead-scheduling` @ `b34b1267`, 2026-08-16. `file:line` anchors valid at that commit;
prefer named symbols when lines drift. Companion analysis:
[config-cache-separation.md](config-cache-separation.md) (separable effort; nothing here depends on
it).

## 1. Why these layers exist, and why they can go

The chains `AbstrUnit → … → Unit<V>` (up to **10** template layers, all in `tic/Unit.h`) and
`AbstrDataObject → … → TileFunctor<V>` (up to 4 layers, `tic/DataArray.h`) were built to express,
by *inheritance splicing*, what pre-C++17 templates could not say in one class: conditional data
members, conditionally-present member functions, and per-type behaviour forks. The splicing tables
are `unit_traits<V>` (`Unit.h:332-356`) and `data_array_traits<V>` (`DataArray.h:266-295`).

C++23 is available (`CMakeLists.txt:42`) and the modern idiom is already used *side by side* with
the layers it obsoletes: `if constexpr` bodies (`Unit.h:60`, `DataArray.h:148-165,210-211`),
`[[no_unique_address]]` conditional members (`Unit.h:62`, `DataArray.h:147`), concepts
(`vt/ElemTraits.h:133,153,197,200`) used as `template <fixed_elem V>` (`TileArrayImpl.h:320-363`).
The intermediate classes' own `static_assert`s re-test the very traits the layering encodes
(`has_var_range_field_v` at `Unit.h:131,140`; `has_simple_range_v` at `Unit.cpp:787,860,886`).

**Removability is established by census** (repo-wide, excluding vcpkg/obj/bin):

- No Unit-side intermediate (`UnitBase`, `RangedUnit`, `FloatUnit`, `CountableUnitBase`,
  `NumRangeUnitAdapterBase`, `Var/FixedNumRangeUnitAdapter`, `GeoUnitAdapter[I]`, `TileAdapter`,
  `IndexableUnitAdapter`, `CountableUnit`, `OrderedUnit`, `OrdinalUnit`, `BitUnit[Base]`,
  `VoidUnit[Base]`) is referenced as a type outside `rtc/dll/src/tic`. The single non-comment
  external-ish use, file-local `GetRangeOrVoid(const RangedUnit<F>*)` (`DataArray.cpp:642`), is
  itself dead (zero callers) and is deleted in U1.
- External code reaches members through `Unit<V>*` (statically typed): `GetRange`/`SetRange` — 106
  occurrences in 38 files; `Get[Curr]SegmInfo` — 6 sites (`clc/OperUnit.cpp:710,728,1267`,
  `geo/OperPolygon.cpp:2201,2220`, `geo/GridDist.cpp:550`); `Set[Ir]RegularTileRange` — 3 sites
  (`clc/OperUnit.cpp:731,1209,1270`); `GetValueAtIndex` — `clc/ReadData.cpp:439`. Name lookup on the
  most-derived class survives any collapse.
- DataArray side: one external **derivation** — `ConstTileFunctor : GeneratedTileFunctor<V>`
  (`clc/ConstOper.h:20`) — and one external **specialization** — the
  `DataArrayBase<null_wrap<T>>` poison (`clc/AggrUniStructNum.h:91`). Everything else is typedef
  reads (`iterator`, `locked_cseq_t`, … ~15 sites in clc/stg) that survive via a compat alias.
- **The DMS runtime Class graph is flat with respect to the layers**: `UnitClass`'s base is
  hard-wired `AbstrUnit::GetStaticClass()` (`UnitClass.cpp:63`), `DataItemClass`'s is
  `AbstrDataObject::GetStaticClass()` (`DataItemClass.cpp:32`); no intermediate has a Class object.
  Collapsing changes serialization, operator signatures, `IsDerivedFrom`, XML round-trip and the
  C API by exactly nothing.
- `DataArray<V>` is already only an alias: `using DataArray = TileFunctor<V>;
  // TODO G8: SUBSTITUTE AWAY` (`TicBase.h:84`; 644 spellings in 106 files — kept for now, see §6).

The collapse also deletes the explicit-instantiation maintenance tails that exist *only because*
exported members live on intermediate templates (`Unit.cpp:1353-1430` including the GCC
`TileAdapter` block; `DataArray.cpp:1007-1025`).

## 2. Per-class verdicts

| class (Unit.h) | verdict |
|---|---|
| `CountableUnit<V>` :244, `BitUnit<N>` :325, `VoidUnit` :328 | empty naming aliases — **delete (U2)** |
| `GeoUnitAdapterI<U>` :176 | one function (`GetTileRangeAsIRect`) — **fold into GeoUnitAdapter (U2)** |
| `OrdinalUnit<V>` :262 | one function (`SetCount`) — **fold into OrderedUnit (U2)** |
| `TileAdapter<Base>` :217 | availability gate, no data/virtuals; both members re-`static_assert` the gating trait — **fold into CountableUnitBase with `requires tileable_value` (U3)** |
| `OrderedUnit<V>` :248 | three functions — **fold into CountableUnitBase (U3)** |
| `NumRangeUnitAdapterBase`/`Var`/`FixedNumRangeUnitAdapter` :121/128/137 | SFINAE-era conditional-member split — **fold; `if constexpr`/`requires` (U3/U5)** |
| `IndexableUnitAdapter<U>` :229 | duck-typed mixin; `GetDimSize` still uses TYPEID tag-dispatch (`Unit.cpp:49-66,1241`) — **fold; `if constexpr` on dimension (U5)** |
| `GeoUnitAdapter<U>` :147 | carries `m_Projection` — **fold; `[[no_unique_address]]` conditional member (U5)** |
| `UnitBase` :52 / `RangedUnit` :65 / `FloatUnit` :109 / `CountableUnitBase` :184 | real storage/behaviour layers — **merge into single `Unit<V>` (U5)**; the Float-vs-Countable `SetRange` fork becomes `if constexpr (has_simple_range_v<V>)` (its bodies already assert exactly that) |
| `BitUnitBase<N>` :274 / `VoidUnitBase` :303 | real leaf behaviour, divergent SegmInfo protocol — **unify protocol (U3), merge (U5)** |
| `Unit<const V>` poison :376 | keep |

| class (DataArray.h) | verdict |
|---|---|
| `AdditiveArray<V>` :222 (1 fn), `GeoArrayAdapter` :233 (1 fn), `PointArrayAdapter` :244 (2), `SeqArrayAdapter` :256 (1) | **fold (U4)** |
| `NumericArray<V>` :176 (22 numeric conversions, already partly `if constexpr`) | **fold (U4)** |
| `DataArrayBase<V>` :43 (storage + pure `GetTile`/`GetFutureTile` + `future_tile`) | **merge into TileFunctor (U4)**; keep name as compat alias |
| `data_array_traits<V>` :266 (with the empty `<Void>` hole :281 patched by `TileFunctor<Void>` :324) | **delete (U4)**; keep the `TileFunctor<Void>` specialization |
| `TileFunctor<V>` :297 | **keep** — the `DECL_RTTI(TIC_CALL, DataItemClass)` anchor and the merge target |
| `GeneratedTileFunctor` :330, `DelayedTileFunctor`/`FutureTileFunctor`/`LazyTileFunctor` (TileFunctorImpl.h), concrete arrays (TileArrayImpl.h) | **keep** — genuine behavioural forks (see [../tile-data-retainment.md](../tile-data-retainment.md)); one external subclass |

## 3. Target shape

### 3.1 Named concepts (successors of the traits tables; define once, reuse in clauses)

All expressible in existing traits (`ElemTraits.h`, `TiledRangeData.h:112,365`):

| concept | definition | covers |
|---|---|---|
| `ranged_value<V>` | `has_var_range_v<V>` | ints, floats, all 6 points |
| `fixed_range_value<V>` | `is_bitvalue_v<V> \|\| is_void_v<V>` | Bool/UInt2/UInt4, Void |
| `countable_value<V>` | `ranged_value<V> && !has_simple_range_v<V>` | ints + int-points |
| `indexable_value<V>` | `is_integral_v<scalar_of_t<V>>` | countable ∪ fixed_range — today's IndexableUnitAdapter placements |
| `tileable_value<V>` | `countable_value<V> && !has_small_range_v<V>` | ≡ `typelists::tiled_domain_elements` |
| `geo_value<V>` | `dimension_of_v<V> == 2` | the 6 point types |
| `numeric_range_value<V>` | `dimension_of_v<V>==1 && (is_numeric_v<V> \|\| fixed_range_value<V>)` | today's NumRangeUnitAdapterBase placements |

### 3.2 One `Unit<V> : AbstrUnit`

- **Storage**, all `[[no_unique_address]]` conditional (the pattern already used for
  `m_RangeDataPtr`): `range_data_ptr_or_void<V> m_RangeDataPtr` (public — `get_range_ptr_of_valuesunit`);
  `conditional_t<has_var_range_v<V>, SharedPtr<const UnitMetric>, Void> m_Metric`;
  `conditional_t<geo_value<V>, SharedPtr<const UnitProjection>, Void> m_Projection`.
- **Non-virtual typed API** carries `requires`-clauses (`TIC_CALL`, defined in Unit.cpp; the
  out-of-line definition must repeat the clause token-identically — always spell clauses via the
  named concepts, never raw trait expressions):
  `GetRange`/`GetPreparedRange`/`SetRange`×2/`ValidateRange` (`ranged_value`, plus constant-range
  `GetRange` for `fixed_range_value`), `Get[Curr]SegmInfo` (unified raw-pointer protocol, §3.3),
  `GetTileRange`/`GetValueAtIndex`/`GetIndexForValue` (`indexable_value`),
  `Set[Ir]RegularTileRange` (`tileable_value`), `GetTileFirstValue`/`GetTileValue`
  (1-D `countable_value`).
- **Virtual overrides are declared unconditionally** — a virtual function cannot carry a
  requires-clause ([class.virtual]) — with `if constexpr` bodies whose else-branch calls the
  AbstrUnit default **non-virtually** (base defaults verified: `SetMetric`/`SetProjection` no-op,
  `GetMetric`/`GetProjection` → nullptr, `GetCount` → 0, `GetTiledRangeData` → `{}`,
  `SetMaxRange` no-op, the conversion/indexing members → `throwIllegalAbstract`;
  `AbstrUnit.cpp:157,577,704,725-731,813-1107`). Example pattern:

  ```cpp
  void SetCount(SizeT c) override {
      if constexpr (countable_value<V> && dimension_of_v<V> == 1 && std::is_unsigned_v<V>)
          SetRange(range_t(0, ThrowingConvert<V>(c)));
      else
          AbstrUnit::SetCount(c);   // base throw — bit-for-bit today's behaviour
  }
  ```

- **Gate-fidelity rule (the one dangerous mistake class):** each member's gate is *the exact set of
  V that override the member today* — derived from the traits-table placements — **not** "the
  storage exists". Two documented traps: `GetValuesRangeCount`/`IsFirstValueZero`
  (`DataArray.h:210-211`) must keep `AbstrDataObject`'s UNDEFINED/false for polygon arrays even
  though `has_var_range_field_v<SPolygon>` is true; SharedStr units must keep
  `HasTiledRangeData() == true` (`Unit.h:60` else-branch) and base-throwing `GetRangeAsStr`.
- **Devirtualized intra-chain virtuals** (only ever dispatched within the chain or through
  `Unit<V>*`): the `SetRange` pures (`Unit.h:78-79`) and `Load/StoreRangeImpl` (`Unit.h:102-103`,
  absorbed into the blob-stream overrides). External vtable contracts (AbstrUnit's) are untouched.
- `unit_traits` deleted; the dead `Float80` entry (`Unit.h:343-345`; `CC_LONGDOUBLE_80` is
  Borland-only, `cpc/CompChar.h:66,132,151`) dies with it. `Unit<const V>` poison stays.
- `range_t` for `fixed_range_value` maps to `Range<UInt32>`-style as today; it must only be *named*,
  never completed, for SharedStr (members using it are constrained out).

### 3.3 SegmInfo protocol unification (U3)

`RangedUnit::Get[Curr]SegmInfo` returns raw `const range_data_t*` (`Unit.h:73`);
`BitUnitBase`'s returns `SharedPtr<const range_data_t>` (`Unit.h:296-300`) — not an override, a
name-based divergence. **Unify on the raw pointer**: all 6 external sites bind `auto` or feed the
raw-pointer consumer `CutTileSpec(const TiledRangeData<T>*, …)` (`clc/OperUnit.cpp:640`), so zero
external edits. `BitUnitBase` keeps its function-local everlasting singleton and returns
`.get_ptr()`; `GetTiledRangeData` re-wraps (AddRef of an everlasting object — safe); Void gets the
symmetric `FixedRange<0>` singleton (pattern at `Unit.h:311-314`). Lifetime contract unchanged.

### 3.4 DataArray side: one `TileFunctor<V> : AbstrDataObject`

Merge per §2; compat alias `template <typename V> using DataArrayBase = TileFunctor<V>;` carries the
typedef-read sites. Poisons must be *ported*, not aliased (aliases cannot be specialized):
`DataArrayBase<bool>` (`DataArray.h:170`) → declared-undefined `TileFunctor<bool>`;
`clc/AggrUniStructNum.h:91` → same trick on `TileFunctor<null_wrap<T>>` (one-line clc edit). Delete
`s_DAB` (`DataArray.cpp:1001`) — `DataArrayBase` has no own `DECL_RTTI`, so it registered the
inherited abstract `AbstrDataObject` Class ~30× (only `s_TFR` matters). Gates for the folded
members: numeric block + `GetValuesRangeCount`/`IsFirstValueZero` on
`is_numeric_v<V> || is_bitvalue_v<V> ||` countable-point; `GetSumAsFloat64` numeric-or-bit only;
`GetActualRangeAsDRect` on `dimension_of_v<field_of_t<V>> == 2`; `Get/SetValueAsDPoint` on
`geo && has_fixed_elem_size_v<V>` (SPolygon is 2-D too — the fixed_elem term is load-bearing);
`GetValueAsDPoints` on `geo && !has_fixed_elem_size_v<V>`; non-virtual `FindPos` gets a
requires-clause.

## 4. Instantiation/export mechanics

- **MSVC**: dllexport-on-class covers members; per-member `TIC_CALL` stays on out-of-line members.
- **GCC/Linux**: `template class Unit<T>` does **not** instantiate base-class template members —
  the root cause of both hand-written tails (`Unit.cpp:1363-1366` comment, `:1412-1413` TileAdapter
  block; `DataArray.cpp:1012-1014` per-member block, which was additionally blocked from
  `template class` by dead uninstantiable code, `:1013`). After the merges there *are* no base
  template members, so a uniform `template class Unit<X>;` / `template class TileFunctor<X>;` block
  (via `INSTANTIATE_FLD_ELEM`/`INSTANTIATE_VOID`, `utl/Instantiate.h`) replaces everything, on both
  compilers. C++20 [temp.explicit]/10 makes explicit class instantiation skip constraint-unsatisfied
  members — the load-bearing mechanism; U1/U4 exercise it on the DataArray side before U5 bets the
  Unit side on it.
- `TiledUnitInstantiator` (`Unit.cpp:1333-1350`) is subsumed by the class instantiations in U5
  (its pointer members never instantiated anything; the ODR-use of the two tile members is covered);
  do not delete it earlier.
- `RangeProp<T> : PropDef<Unit<T>, Unit<T>::range_t>` (`UnitClassReg.h:27-74`) resolves
  `range_t`/`GetRange`/`SetRange`/`GetTSF` on `Unit<T>` by normal lookup; the registration statics
  (`Unit.cpp:1347-1348`) are the acceptance probe — they fail to compile if a gate is wrong.

## 5. Commit sequence

Each commit: build `all22.sln` (VS18 msbuild, serial, Debug x64 on the laptop; judge by link lines /
bin timestamps, not exit code). Per the owner's verification policy (2026-08-16): no per-step test
battery — run `.\batch\TestDebugUnit.bat` once **after the last step** of a series (assert
coverage); CMake/WSL builds and `full.py` run on OVSRV10, not the laptop. **GCC-only regions
changed in U1-U3 need that OVSRV10 Linux build before any merge toward main** (the laptop has no
GCC).

- **U1 — DataArray dead code + instantiation groundwork** *(implemented 2026-08-16)*. Delete dead
  `GetRangeOrVoid` overload set (`DataArray.cpp:640-658`; zero callers) and
  `SetIndexedValueArray`/`GetIndexedValueArray` (`DataArray.h:85,90`, `DataArray.ipp:35-63`; zero
  callers, and `SetIndexedValueArray` calls `GetLockedDataWrite()` argument-less against a 2-param
  no-default declaration — the exact uninstantiable dead code named at `DataArray.cpp:1013`). In the
  GCC-only block, replace the per-member list with per-class instantiation
  (`template class DataArrayBase<T>;` + the TileFunctor RTTI members + `CreateHeapTileArrayV<T>`),
  flagged for OVSRV10 confirmation.
- **U2 — empty naming + one-function Unit layers** *(implemented 2026-08-16)*. Delete
  `CountableUnit`/`BitUnit`/`VoidUnit`; fold `GeoUnitAdapterI` → `GeoUnitAdapter` (if-constexpr
  override, F/DPoint keep base throw) and `OrdinalUnit` → `OrderedUnit` (SetCount if-constexpr
  unsigned, signed keep base throw); update `unit_traits` entries and the GCC block spellings;
  remove the stale `// , mc_RangeDataPtr;` comment (`Unit.h:62`).
- **U3 — SegmInfo unification + availability adapters.** §3.3; `TileAdapter` members →
  `CountableUnitBase` with `requires tileable_value`; delete the GCC TileAdapter block; fold
  `OrderedUnit` into `CountableUnitBase`; duplicate `NumRangeUnitAdapterBase`'s one member into its
  two children (temporary, dies in U5).
- **U4 — DataArray merge** *(implemented 2026-08-16)*. Merged per §3.4, but into **`DataArrayBase<V>`**
  rather than up into `TileFunctor<V>`, which now derives directly from it. That target keeps
  `DataArrayBase` a real class template, so both poison specializations (`DataArrayBase<bool>` and
  clc's `DataArrayBase<null_wrap<T>>`) and every external `DataArrayBase<V>::iterator`-style typedef
  read keep working untouched — no compat alias needed. `data_array_traits` and the five adapter
  layers are gone; the gates live in named predicates (`numeric_elem_v`, `countable_point_elem_v`,
  `numeric_array_api_v`, `geo_elem_v`, `point_elem_v`, `polygon_elem_v`).
  **Lesson that constrains U5:** only the non-virtual `FindPos` could take a requires-clause (a
  virtual override may not, [class.virtual]/6), and MSVC then rejects naming that constrained member
  in an explicit *function* instantiation (C3190) — so constrained members must be emitted through
  `template class X<T>;`, never per-member. See the guarded instantiation at the end of DataArray.cpp.
- **U5 — Unit spine merge** (the big one). §3.2 in full; delete `unit_traits`,
  `TiledUnitInstantiator`, all instantiation tails → uniform `template class Unit<X>` on both
  compilers. Debug spot-run of a tiled + a bit-domain + a string-values config (SetRange asserts are
  Debug-only). Post-U5 grep invariant: no live references to any deleted class name.
- **U6 — optional follow-ups, separate efforts**: `[[msvc::no_unique_address]]` portability macro
  (plain `[[no_unique_address]]` is a no-op on MSVC — pre-existing, affects only bytes-per-unit);
  the 644-site `DataArray` → `TileFunctor` substitution (`TicBase.h:84` TODO) and eventual
  `DataArrayBase` alias retirement; the DataArray.h interface/impl split recommended by
  [compile-time-refactor-analysis-2026-07.md](compile-time-refactor-analysis-2026-07.md) (easier
  once there is one class to split).

## 6. Risks

1. **[temp.explicit]/10 reliance** (constraint-skipping class instantiation) — canaried in U1/U4;
   fallback: per-concept `if constexpr` body + internal `static_assert` (worse diagnostics, same
   schedule).
2. **Token-identical out-of-line requires-clauses** — mitigated by only ever spelling clauses via
   the named concepts.
3. **ODR/ABI**: vtable layouts and object sizes of `Unit<V>`/`TileFunctor<V>` change per commit —
   safe only because everything rebuilds from `all22.sln` and externals use the extern-"C" API +
   AbstrUnit/AbstrDataObject virtuals (untouched). Never mix old/new DLLs; `dumpbin /exports` diff
   on DmTic.dll at U1 and U5.
4. **Gate fidelity** (§3.2 rule) — review every folded override against the traits-table placement,
   not against storage availability.
5. **PCH**: `DataArray.h` sits in ClcPCH + GeoPCH — every commit is a full clc+geo rebuild
   (accepted, batched); the end state shrinks both hot headers.
6. **Laptop has no GCC**: all edits inside `#if !defined(_MSC_VER)` regions are compile-unverified
   until the OVSRV10 build — keep them mechanical (same types, new spellings), and gate merges on
   that build.
