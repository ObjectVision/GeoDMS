## Summary

When a worker thread evaluates a domain attribute whose expression is a **late-bound string-constructed expression** (using the `:= =` template syntax), the engine reaches `GetOrCreateDataController` and the assertion at `DataController.cpp:351` fires:

```
Check Failed Error: IsMainThread() || !mayCreate
C:\dev\GeoDMS_2026\tic\dll\src\DataController.cpp(351):
This seems to be a GeoDms internal error
```

In one of our regression runs (t720, 2BURP project) this fires **152 times**, all from worker thread `33`, on dynamically-constructed domain expressions.

The relaxation in commit `766848c9` (`#361 fixed by replacing UnifyDomain()->GetOrCreateDC() -> MakeResult() by UnifyDomain()->GetExistingDC() -> MakeResult()`) explicitly notes it as incomplete:

> TODO: prevent UnifyXXX() to be called from WorkerThreads.

This issue is the realization of that TODO â€” there is at least one other code path that still routes through `GetOrCreateDataController` from a worker thread.

## Repro

Run `python full.py -version local` (cmake-Release) or `local-msbuild-release` against the in-tree `2BURP` regression test:

```
F:\SourceData\RegressionTests\prj_snapshots\2BURP\cfg\main\Analysis\Future.dms
```

Lines 10-18 build several `_domain` attributes via `:= '...'` (late-bound string templates substituting `ModelParameters/StartYear`):

```dms
attribute<BuiltUpKm2>        BuiltUp_Residential_Area_domain (domain) := ='SourceData/BuiltUp/Residential/'+ModelParameters/StartYear+'[km2] / 1[km2] * 1[BuiltUpKm2]';
attribute<Person>            Population_domain               (domain) := ='SourceData/Population/PerYear/'+ModelParameters/StartYear+'[Person]';
attribute<Person_BuiltUpKm2> Population_inRes_Density_domain (domain) := Population_domain / BuiltUp_Residential_Area_domain;
```

The first two reach the assertion when evaluated by worker thread 33.

## First error log lines

```
2026-05-08 11:31:52[33][[/Analysis/Future/InitialState/Impl/Population_domain]] Check Failed Error: IsMainThread() || !mayCreate
2026-05-08 11:31:52[33]C:\dev\GeoDMS_2026\tic\dll\src\DataController.cpp(351):
2026-05-08 11:31:52[33]This seems to be a GeoDms internal error...
```

A preceding line shows a tokenizer case-mix warning that hints at a substitution path being triggered:

```
2026-05-08 11:31:52[1]Depreciated mix-up of cases, tokenized 'Region_rel' as token 4754 and then seen 'region_rel' [[/Analysis/Future/InitialState/Impl]]
```

## Likely culprits â€” to grep

The function only has two callers in the public API:

```
DataControllerRef GetOrCreateDataController(LispPtr keyExpr) { return GetDataControllerImpl(keyExpr, /*mayCreate=*/true); }
DataControllerRef GetExistingDataController(LispPtr keyExpr) { return GetDataControllerImpl(keyExpr, /*mayCreate=*/false); }
```

Anywhere a worker-thread codepath can reach `GetOrCreateDataController(...)` is a candidate. Particularly:

- `tic/dll/src/AbstrCalculator.cpp:170,171,480,948,1058,1381` â€” substitution / meta-info paths
- `tic/dll/src/AbstrUnit.cpp:292`
- `tic/dll/src/MoreDataControllers.cpp:162`
- `tic/dll/src/TreeItem.cpp:1092,2350,2419,2429,2452`

## Environment

- Branch: `refactor_linux_gui` @ `3f13e1e5` and prior parents
- Windows 11 Pro 26200; reproduces in cmake-Release and msbuild-Release
- /S1 /S2 /S3 multitasking flags

## Priority hint

The same engine path is exercised by any cfg that builds domains via `:= ='...'` string templates with metric casts. Likely impacts more tests than just t720 once we enable parallel evaluation more broadly.
