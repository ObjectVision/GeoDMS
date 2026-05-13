## Staged metric-cast patches (apply after current t641 run finishes)

### 1. lus_demo_2023 (t611) — `Distance_decay/LandUse/Km*` & `PolicyMaps/Km1`

File: `F:\SourceData\RegressionTests\prj_snapshots\lus_demo_2023\cfg\demo\Current_situation\distance_decay.dms`

For lines 28, 38, 48, 58 inside the four `for_each_nedvld(...)` templates, change the inner string from:

```
'rescale( potential(current_landuse/'+ /Classifications/ggModel/Name + ', /Geography/Distmatrices/Impl_'+ModelParameters/ModelResolution+'/pot<RANGE>m/potrange/RelWeightSqrt_demo))'
```

to (append `[Potentiaal]` cast inside the string):

```
'rescale( potential(current_landuse/'+ /Classifications/ggModel/Name + ', /Geography/Distmatrices/Impl_'+ModelParameters/ModelResolution+'/pot<RANGE>m/potrange/RelWeightSqrt_demo))[Potentiaal]'
```

For lines 69-70 (`PolicyMaps/Km1/birdhabitat`, `net_EHS03`), change:

```dms
attribute<Potentiaal> birdhabitat (domain) := ='rescale( potential(policy_maps/NatureLandscape/birdHabitat [float32], ...RelWeightSqrt_demo))';
attribute<Potentiaal> net_EHS03   (domain) := ='rescale( potential(policy_maps/NatureLandscape/net_EHS03   [float32], ...RelWeightSqrt_demo))';
```

to:

```dms
attribute<Potentiaal> birdhabitat (domain) := ='rescale( potential(policy_maps/NatureLandscape/birdHabitat [float32], ...RelWeightSqrt_demo))[Potentiaal]';
attribute<Potentiaal> net_EHS03   (domain) := ='rescale( potential(policy_maps/NatureLandscape/net_EHS03   [float32], ...RelWeightSqrt_demo))[Potentiaal]';
```

### 2. RSopen Intensiveren.dms (t641_2)

File: `F:\SourceData\RegressionTests\prj_snapshots\RSopen_RegressieTest_v2025H2_wLB\cfg\main\VariantParameters\Ontwikkelpakketten\Intensiveren.dms`

Lines 42-43 — replace unit-stripping `[float32]` with target-unit cast:

```dms
// before
attribute<FSI> FSINettoBuurt := Elements/Text[value(uInt32(id(.))* nrAttr + 11, Elements)][float32];
attribute<GSI> GSINettoBuurt := Elements/Text[value(uInt32(id(.))* nrAttr + 12, Elements)][float32];

// after — chain: parse string→float32, then cast to named unit
attribute<FSI> FSINettoBuurt := Elements/Text[value(uInt32(id(.))* nrAttr + 11, Elements)][float32][FSI];
attribute<GSI> GSINettoBuurt := Elements/Text[value(uInt32(id(.))* nrAttr + 12, Elements)][float32][GSI];
```

(Test: if `[float32][FSI]` rejects, try `[FSI]` directly; the string-parse may auto-route via the unit's value type.)

### Apply procedure

After the current t641 run notification arrives:

1. Apply both edits via Edit tool on OVSRV05 paths.
2. Run robocopy OVSRV05→F:.
3. Wipe `t611` and `t641_2`/`t641_3` bins.
4. Re-run `python full.py -version local`.
