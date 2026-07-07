# Getting Boost.Format out of the universal prelude: migration analysis to std::format

*2026-07-07, branch `refactor_ownership`. Follow-up to
`compile-time-refactor-analysis-2026-07.md` finding 4. This analyses **how the format strings
and the `ser/format.h` machinery must change**; it does not perform the migration.*

## Why

`dbg/Check.h` → `ser/format.h` → `<boost/format.hpp>` puts Boost.Format in the prelude of
**all 294 TUs** (it is the only Boost header in the universal prelude). Measured with the
v145 toolchain:

| probe | preprocessed | front-end parse |
|-------|-------------:|----------------:|
| `<boost/format.hpp>` alone | 90,646 lines / 4.2 MB | 1.19 s |
| `<format>` alone | 61,403 lines / 3.0 MB | 0.80 s |
| full `RtcPCH.h` (with boost/format) | 103,220 lines / 4.8 MB | ~1.6 s |

Much of both probes is shared STL, but Boost.Format's own machinery (boost::optional,
shared_ptr, MPL utilities, the stream-based engine) is prelude weight that `<format>` does
not carry, and `<format>`'s STL content largely overlaps what the prelude needs anyway.
Expect roughly a quarter to a third off the PCH size; with PCH enabled the per-TU win is
gone, but PCH build time, PCH invalidation cost, and the Boost coupling remain. `std::format`
also brings **compile-time format checking**, which Boost.Format cannot do.

## Current architecture

`ser/format.h` defines `mgFormat(msg, args...)` = `boost::format(msg) % arg0 % arg1 ...` and
`mgFormat2string` = `str(...)` + an rvalue-consumption dance (`release_resources`) whose
purpose is to let temporary `TokenStr` locks die immediately after formatting. Everything
else is thin variadic wrappers. Census over rtc…shv+qtgui (test dirs excluded):

| wrapper (defined in) | call sites |
|----------------------|-----------:|
| `reportF`, `reportF_without_cancellation_check` (dbg/Check.h) | 248 |
| `mySSPrintF`, `myFixedBuffer*`, `myArrayPrintF` (utl/mySPrintF.h) | 259 |
| `throwErrorF`, `throwDmsErrF` (dbg/Check.h) | 232 |
| `throwItemErrorF` (throwItemError.h) | 66 |
| `throwSystemError`, `throwLastSystemError` (utl/Environment.h) | 47 |
| `mgFormat2string` / `mgFormat2SharedStr` direct | 50 |
| `TellExtraF` (xct/ErrMsg.h), `throwMsgF` (xct/DmsException.h), `MG_TRACE` (dbg/debug.h) | 20 |
| **total** | **926** |

~780 sites pass a string literal; the ~146 "non-literal" sites are mostly the forwarding
layers themselves plus a handful of genuine runtime selections (ternaries between two
literals, pass-through `msg` parameters). The `boost::format` object is streamed (rather
than `str()`ed) only inside `myFixedBuffer*` — no other user touches the object itself.
`myVSSPrintF` (utl/mySPrintF.cpp) is a separate true-`vsnprintf` path and is unaffected.

Nothing in the codebase catches `boost::io::*` exceptions, and no format literal contains a
literal `{` or `}` — so brace-escaping, the classic std::format migration hazard, is a
non-issue here.

## Directive census (what the format strings actually use)

| directive | count | std::format equivalent |
|-----------|------:|------------------------|
| `%s` | 1,026 | `{}` |
| `%d` | 298 | `{}` |
| `%u` | 124 | `{}` |
| positional `%1%`…`%5%` (9 files, e.g. CheckedCalc.h, stg/DllMain.cpp) | 43 | `{0}`…`{4}` — boost is 1-based, std is 0-based; a string using manual indices must index **all** its fields |
| `%d`/`%u`/`%s` with flags/width/precision (`%04d`, `%02d`, `%.80s`, `%32s`) | 32 | `{:04d}`, `{:02d}`, `{:.80}`, `{:>32}` — printf right-aligns by default, std::format left-aligns strings, so width on `%s` needs an explicit `>` |
| `%f`, `%g`, `%e` (+ `%lg`, `%1.0lf`, `%.6g`) | 25 | `{:f}`, `{:g}`, `{:e}` (+ `{:.1f}`, `{:.6g}`) — keep the type letter: bare `{}` on a double is shortest-round-trip, not printf-fixed-6 |
| `%%` | 12 | literal `%` (no escaping needed) |
| `%x`, `%X`, `%lx` | 11 | `{:x}`, `{:X}` |
| `%c` | 6 | `{}` |
| `%I64u`, `%S` (MS printf-isms) | 4 | `{}` — see "latent bugs" below |

Length modifiers (`l`, `ll`, `I64`) simply disappear: std::format is type-driven.

### Rewrite rules, summarized

1. `%s`, `%d`, `%u`, `%c` → `{}` (bare). Do **not** use `{:d}`/`{:s}` — bare `{}` keeps the
   rewrite type-agnostic (boost never type-checked, so a "%d" may well receive a string
   somewhere; bare `{}` formats whatever arrives).
2. `%f/%g/%e/%x/%X` and anything with flags/width/precision → keep an explicit `{:spec}`,
   mapping printf spec to std spec (drop length modifiers; add `>` for right-aligned strings).
3. `%N%` → `{N-1}`, and convert every other placeholder in that same string to an explicit
   index (std::format forbids mixing automatic and manual indexing). Boost's ability to
   repeat an argument (`%1% … %1%`) maps directly to repeating `{0}`.
4. `%%` → `%`.
5. No brace escaping needed anywhere (verified: zero literal braces in all 780 literals).

All five rules are mechanical; the census script (scratchpad `fmtscan.py`) already parses
every call site and its literal, so it can be extended into the rewriter. Recommended order:
rewrite + compile per module, since the compiler flags every string the rewriter got wrong
only once the wrappers are switched over.

## Latent bugs found by the census (fix these first, independently of migration)

These sites *today* throw `boost::io::format_error` instead of their intended message —
i.e. the real error is masked exactly when it matters:

- [Token.cpp:93](rtc/dll/src/set/Token.cpp) and [Token.cpp:134](rtc/dll/src/set/Token.cpp) —
  `throwErrorF("TOKEN", "%s is not registered as token")` with **no argument**
  (`too_few_args` at throw time).
- [FileMapHandle.cpp:269](rtc/dll/src/ser/FileMapHandle.cpp) —
  `throwSystemError(GetLastError(), "GetFileSize(%S)", handleName)`: `%S` is not a
  Boost.Format directive (`bad_format_string`).
- `HeapSequenceProvider.cpp` / `TriggerOperator.cpp` / `DebugStream.cpp` use `%I64u`
  (MS printf-ism) in throw/report paths — Boost.Format does not document the `I64` length
  modifier; verify or normalize to `%u` now.
- A structural improvement the new wrapper should adopt: catch `std::format_error` inside
  `mgFormat2string` and fall back to emitting the raw format string plus stringified
  arguments, so a malformed format can never again swallow the underlying error report.

## Semantic differences to design around

1. **Argument types.** Boost.Format renders every argument via `operator<<`; std::format
   requires a `std::formatter<T>` specialization. The call sites pass ~340 `.c_str()`s and
   ~1,250 expressions of unaudited types (SharedStr, TokenID/TokenStr, item pointers, enums
   with stream operators, …). Auditing them all is not realistic — the new `mgFormat` must
   keep an **ostream fallback**: constrain on `std::formattable`, and route everything else
   through its existing `operator<<` into a string first.
2. **Floating-point defaults differ**: ostream (and thus boost `%s` on a double) prints 6
   significant digits; std::format `{}` prints shortest-round-trip. To keep output
   byte-identical, the transformer should also route floating-point values through the
   ostream path when the placeholder came from `%s` (in practice: let the fallback cover
   everything that is not integral/char*/string_view, floats included).
3. **Locale**: boost::format uses the stream's (global) locale; std::format is
   locale-independent unless `{:L}`. GeoDMS never imbues, so output is identical and
   becomes deterministic by construction — an improvement, not a risk.
4. **Arity mismatches**: boost throws on both too-few and too-many; std::vformat throws on
   too-few but silently ignores extra arguments. The compile-checked phase (below) turns
   both into compile errors for the ~780 literal sites, which is the real win.
5. **Rvalue/TokenStr lifetime**: the `release_resources` mechanism exists to drop TokenStr
   locks promptly. The new design gets this for free: the transformer **eagerly stringifies**
   non-trivial arguments before `vformat`, so locks die even earlier than today.
6. **Positional base** (1-based → 0-based) and **no mixing** of `{}` with `{0}` — handled by
   rewrite rule 3.

## Target design for ser/format.h

Phase A (drop-in, runtime-checked — enables deleting `<boost/format.hpp>`):

```cpp
#include <format>          // replaces <boost/format.hpp>

template <typename T> concept mg_std_formattable =
    (std::integral<std::remove_cvref_t<T>> && !std::same_as<std::remove_cvref_t<T>, bool>)
    || std::convertible_to<T, std::string_view>;

// everything else (SharedStr, TokenID, doubles, pointers, enums, ...) keeps its
// operator<< rendering — byte-identical output, and TokenStr locks die here:
template <typename T> decltype(auto) mg_fmt_arg(T&& v) {
    if constexpr (mg_std_formattable<T>) return std::forward<T>(v);
    else { std::ostringstream os; os << v; return std::move(os).str(); }
}

template<typename ...Args>
std::string mgFormat2string(CharPtr msg, Args&&... args)
try {
    return std::vformat(msg, std::make_format_args(mg_fmt_arg(std::forward<Args>(args))...));
} catch (std::format_error&) {
    // never let a malformed format mask the actual error: emit raw format + args
    ...
}
```

(Details to settle in implementation: `make_format_args` takes lvalues, so the transformed
pack must be materialized in locals/tuple first; the `ostringstream` include should live in
format.cpp via a small exported stringify helper, keeping `<sstream>` out of the prelude.)

Phase B (compile-checked, per-wrapper flip): change the wrappers' first parameter to
`std::format_string<Args...>` (fully `consteval`-checked) and add `*_runtime` escape-hatch
overloads (or `std::runtime_format`, available in the current MSVC STL under
`/std:c++latest`) for the ~dozen genuinely dynamic sites (ternary literals,
`throwItemErrorF(self, msgVariable)` forwarding). This catches every arity/spec error at
compile time for the 780 literal sites.

`myFixedBuffer*` / `myArrayPrintF` stop streaming a format object and become
`std::format_to_n(buf, size, ...)` — simpler and bounds-safe by construction.

## Staged plan

| stage | content | risk |
|-------|---------|------|
| 0 | Fix the latent-bug sites above (5 files) | none |
| 1 | New `ser/format.h` per Phase A + mechanical rewrite of all format literals (extend fmtscan.py; rewrite + build module-by-module) | low; compiler + fallback catch stragglers |
| 2 | Flip wrappers to `std::format_string` (Phase B); annotate the few runtime-format sites | compile-time only |
| 3 | Delete the boost/format include; re-measure PCH size and build; drop `release_resources` machinery | none |
| 4 | Optional cleanup: migrate `myFixedBuffer*` off `<strstream>` (existing TODO) using `format_to_n` | low |

Verification: the regression test suite compares logged output and error messages broadly;
a full prj_snapshots run (t720, t641) after stage 1 and stage 3 covers both the happy path
and (via IntegrityCheck tests) several throw paths. Grep-level goldens: the 43 positional
sites and the 25 float-spec sites are the ones worth eyeballing in output diffs.
