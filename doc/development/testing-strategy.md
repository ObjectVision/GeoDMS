# Testing strategy

## Unit tests are insufficient for thread / stack / dependency-graph bugs

The unit-test suite (`unit.bat` / `Test{,CMake}{Debug,Release}Unit.bat` /
`run_unit.bat`) is small, single-threaded, and has shallow dependency
graphs — useful for catching basic regressions but it **never**:

- Triggers `TreeItem::UpdateMetaInfo`'s `RemainingStackSpace() <= 327680`
  stack-overflow guard, which is what hands the meta-thread baton to a
  worker via `std::async`.
- Builds deeply-recursive `Substitute` / `VisitSuppliers` chains that
  exercise late-bound `:= '…'` template substitution at scale.
- Surfaces cmake-vs-msbuild divergences (different vcpkg-supplied MSVC
  STL versions schedule `std::async` differently).

For thread-safety, stack-pressure, and meta-thread-baton bugs, run the
**full project regression suite** at `tst/batch/full.py`:

```sh
cd C:\dev\tst\batch
python full.py -version local            # cmake-release flavor
python full.py -version local-msbuild-release
```

The 2BURP cfg (`t720_2BURP_indicator_results/result`) in particular has
so many layered `attribute<X> := '…'` domain definitions that the
meta-info chain fills the stack within the first second of the run, the
baton transfers to a worker thread, and any path with
`assert(IsMainThread())` instead of `assert(IsMetaThread())` fires
immediately. Bug
[#1102](https://github.com/ObjectVision/GeoDMS/issues/1102) was found
this way after >18 unit-test passes had been green.

For fast bug repros: a `cmake-Debug` build + the `t720` target reproduces
#1102-class assertions within ~1 s (152 asserts in 8 s).

## Whenever changing an `IsMainThread` assert

Search for sibling `IsMainThread()` checks on the same call chain
(`AbstrCalculator::*`, `TreeItem::*`, `DataController::*`, `Actor::*`).
Most are already migrated to `IsMetaThread()` (commit `0165494c`,
related to #1102), but stragglers may exist in newer code. See
[doc/linux/qt-hwnd-gotchas.md](../linux/qt-hwnd-gotchas.md) for the
adjacent rule that Qt-rendering paths in `shv/`/`qtgui/` must keep
`IsMainThread()` (Qt event loop runs on the *literal* main thread).

## Regression run logistics

- Result HTMLs land under
  `C:/dev/tst/Regression/GeoDMSTestResults/<version>_<flavor>_<arch>_…/`.
- Each test caches its output in a `local_…__<test_name>.bin` file in the
  same dir; **delete those `.bin`s before re-running** if you've changed
  binaries — otherwise the runner reports the *previous* run's pass/fail
  with a "results are reused" note.
- `python full.py` runs all ~26 tests; there is no test filter — to run a
  single test fast, write a focused runner that reuses `full.py`'s
  `_load_local_settings` / `get_geodms_paths` and adds a single
  `regression.add_exp` entry.
