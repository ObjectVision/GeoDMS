# Development environment notes

Generic developer-experience tips for working in this repo, lifted from
session memory so they travel with the code rather than living only in any
one developer's tooling. If you find yourself repeatedly tripping over the
same workspace quirk, add it here.

| File | What it covers |
|---|---|
| [build-tips.md](build-tips.md) | Build sequencing rule (don't run multiple full builds in parallel), Windows cmake/vcpkg paths from VS18, MSVC heap-space gotcha for `RLookup.cpp`. |
| [dev-environment-gotchas.md](dev-environment-gotchas.md) | `NoDefaultCurrentDirectoryInExePath` blocking `Test*.bat` scripts, git case-sensitivity for `shv/dll/src/dataview.cpp` and friends, repository search scope. |
| [testing-strategy.md](testing-strategy.md) | When unit tests are insufficient — concurrency / stack / meta-thread bugs only surface in the full `prj_snapshots` regression suite. |

See also `doc/linux/` for porting-specific notes and `PORTING_STATUS.md` at
the repo root for the Linux-port progress index.
