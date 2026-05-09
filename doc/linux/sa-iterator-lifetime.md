# `SA_Iterator` temporary-lifetime pitfall

A C++ lifetime trap that first surfaced via Linux GCC strict-mode
diagnostics during the Linux port (and was eventually pinned with a GDB
crash trace). Generic C++, not Linux-specific in principle — but Linux
GCC's stricter lifetime rules (and ASan/UBSan integration) are what made
it visible.

## The bug pattern

```cpp
auto& x = *(some_SA_Iterator + offset);   // <-- DANGLING REFERENCE
```

`SA_Iterator<T>::operator*()` returns `SA_Reference<T>&` — i.e. a
reference into the iterator itself. When the iterator is a *temporary*
(produced by `operator+`), C++ does **not** extend its lifetime through
the function call: the temporary is destroyed at the end of the full
expression, leaving `x` pointing into freed stack memory.

## Symptom in the wild

`FastConnectOperator::CreateResult` (`shv/dll/src/Connect.cpp` line 1279,
at the time of the bug) crashed with `sequence_obj<FPoint>::size()`
failing `IsLocked()` because the underlying `m_Container` was stale
stack data from the destroyed temporary iterator.

## The fix

```cpp
auto xIter = some_SA_Iterator + offset;   // named, lives for the whole scope
auto& x = *xIter;                          // reference now valid
// … use x …
```

Naming the iterator extends its lifetime to the surrounding scope, and
the reference returned by `operator*` stays valid.

## Where to look for similar patterns

Any code that takes a reference through a chained iterator expression
(`*(iter + n)`, `*(iter - n)`, `*++iter`, …) and binds it to `auto&`
or a typed reference is a candidate. The signal is `auto&` (or a typed
reference) on the LHS of an expression that ultimately dereferences a
function return value.

`SA_Iterator` and `SA_Reference` are defined in `rtc/dll/src/seq/`; this
pattern only matters where iterators return references-into-self rather
than values.
