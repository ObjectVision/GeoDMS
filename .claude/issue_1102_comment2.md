**Stack analysis (cmake-Debug, attached via VS).**

The asserting worker thread is spawned by `TreeItem::UpdateMetaInfo:2640` via `std::async`/PPL:

```
TreeItem::UpdateMetaInfo  (main thread)
  ├─ line 2640: std::async(lambda { UpdateMetaInfoImpl2(); })
  └─ line 2644: future<void>::get()                       ← main blocks here

                                   worker (Windows TP / PPL):
                                     UpdateMetaInfoImpl2:2570
                                     → Actor::DetermineState:872
                                     → DetermineLastSupplierChange:674
                                     → AbstrCalculator::VisitSuppliers:650
                                       assert(IsMainThread())  ← fails
```

Notable observations:
- Main thread is *blocked* in `future::get()`, so the async is **not gaining any concurrency** — it just acts as an exception-isolation wrapper.
- Both threads run the same chain (`DetermineState → DetermineLastSupplierChange → VisitSuppliers → GetMetaInfo → SubstituteExpr → SubstituteArgs ↺ → slSupplierExprImpl:908 → UpdateMetaInfo`); the recursion is deep (dozens of frames in the main-thread stack).
- This explains the cmake-vs-msbuild divergence: vcpkg's MSVC STL `14.50.35717` schedules the lambda on a real worker thread, while msbuild's vendored STL likely inlines under `std::launch::async|deferred` semantics or differs in the threadpool dispatch policy.

**Fix candidates (least to most invasive):**

1. **Drop the async** — replace `std::async(...lambda...) → future.get()` at TreeItem.cpp:2640-2644 with `try { UpdateMetaInfoImpl2(); } catch (...) { /* same propagation */ }`. If the only purpose of `std::async` here is exception forwarding (it certainly looks like it — main thread blocks immediately), this is functionally identical and trivially eliminates the worker re-entry.
2. **Force deferred launch policy** — `std::async(std::launch::deferred, lambda)` guarantees execution on the calling thread when `.get()` is invoked.
3. **Make `VisitSuppliers` / `DetermineState` thread-safe** — relax `assert(IsMainThread())` at AbstrCalculator.cpp:650. Higher risk; the precondition is probably load-bearing for some shared mutable state.

For #1102's reported repro, option (1) or (2) should make it disappear without altering observable program behavior.
