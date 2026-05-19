**Update — this is cmake-build-specific, msbuild-Release does NOT reproduce.**

Re-running t720 on the same machine, both flavors, side-by-side in the same report:

| Column | Build | t720 status |
|---|---|---|
| 20.0.0.c | cmake-Release (vcpkg toolchain) | **`1` — `Check Failed Error: IsMainThread() || !mayCreate` at DataController.cpp:351, 152× in log** |
| 20.0.0.m | msbuild-Release (MSVC project) | **`OK` — clean run, status_code=0, completes in 387s** |

So `GetOrCreateDataController` is reachable from a worker thread *only* in the cmake build. Likely candidates:

- **Different STL / runtime** — vcpkg's MSVC STL may have different `std::mutex`/`std::thread` semantics or thread-pool scheduling, exposing a latent race.
- **Different optimization** — cmake's `/O2` may inline/reorder differently, changing which code paths a worker thread reaches.
- **Different boost/Qt** — if the worker-spawning library is supplied by vcpkg in the cmake build but by msbuild's vendored copy elsewhere, threading behaviour differs.

Both binaries were built from the same source tree (`refactor_linux_gui` at the same commit); only the build system differs. Worth diffing the imported-DLL lists between `bin/Release/x64/GeoDmsRun.exe` (msbuild) and `build/windows-x64-release/bin/GeoDmsRun.exe` (cmake).

The **TODO** from `766848c9` ("prevent UnifyXXX() to be called from WorkerThreads") still holds as the right structural fix, but the cmake-only manifestation hints at a more concrete starting point: which worker-spawning code is compiled differently between the two pipelines.
