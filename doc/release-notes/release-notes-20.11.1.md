**Pre-release.** GeoDMS 20.11.1 turns on **free-store drainage**: memory freed by the engine now flows back to the operating system once the machine's RAM use passes `MemoryFlushThreshold`, instead of being retained in the allocator's free stacks for the rest of the run. On the RSopen allocation benchmark this cut peak commit charge from 193.5 GB to about 180 GB, and the peak is now bounded by genuinely live data rather than by retained free memory.

| Flavour | Asset |
|---|---|
| `.m` — Windows, MSBuild | `GeoDms20.11.1.m-Setup-x64.exe` |
| `.c` — Windows, CMake | `GeoDms20.11.1.c-Setup-x64.exe` |
| `.l` — Linux, Ubuntu 24.04 | `GeoDms20.11.1.l-linux-x64.deb`, `.tar.gz` (+ `.sha256`, `.sha256.p7s`) |

## Memory: free-store drainage

Freed object stores below 2 MB used to stay on the allocator's free stacks indefinitely — measured at 112 GB of retained-but-dead pool on a large run. They are now decommitted back to the OS under memory pressure.

- **Two triggers.** Standing RAM use above `MemoryFlushThreshold` — the same signal that already throttles operation activation via `IsLowOnFreeRAM` — needs no scheduler involvement and so works with the admission gate off. Polled at most once a second and only on every 1024th large allocation, so the steady-state cost is a single relaxed load. Independently, when resource-aware scheduling runs in `enforce` mode, the memory ledger's **claim window** drives drainage directly: while a refused claimant waits for budget, the allocator hands cold freed stores back to the OS, so the same signal that defers memory-adding work also shrinks the committed dead pool. This is the measured operating point (drain-F).
- **Array-wide sweep** over all 17 size classes on one shared allocator section, gated on a pending-store counter so a sweep that yields nothing does not immediately re-run.
- **Budgeted**, with a keep-hot reuse band: draining hard for a whole run was measured to re-commit 1.7 TB and cost about 20% wall-clock, so sustained pressure keeps a proportional reuse band while a short scheduler claim window drains hard.
- **Largest classes first**, which was both the fastest configuration measured and the one returning the most memory.
- Controlled by the `MemoryDrainage` setting (on by default) and the `/CF` / `/SF` command-line switches.

Documented in `doc/development/schedule-with-lookahead.md` §8.1.23–8.1.32, including the measurements that rejected two alternative designs (a commit-pressure trigger, which annihilated the pool without moving the peak, and streaming residency).

## Scheduling: lookahead-based activation throttling

The resource-aware admission gate itself is not new in this release, but it gains a direct link to the allocator, and a new signal.

- **The ledger's claim window now drives drainage.** When a task is refused admission because its charge does not fit the budget, it becomes the claimant; while that claim pends, free-stack drainage is switched on, and it is switched off again as soon as the claim resolves or is released. Deferring memory-adding work and returning committed-but-dead memory to the OS are now the same event.
- **A process commit-versus-budget pressure signal** was added — one `GetProcessMemoryInfo` per second rather than per admission — and is deliberately **observability only**. Coupling it into the defer policy and the drain trigger was measured (§8.1.30) as a net loss on the RSopen workload: the free-stack pool was annihilated but the peak did not move, while wall-clock rose about 20% from re-committing 1.7 TB of churn. The trigger was therefore reverted to the claim window, and re-coupling the pressure signal is left pending a coldness-aware design. It still logs pressure transitions under ledger logging.
- The admission-deferral log line now reports which of the two reasons caused a deferral.

Resource-aware scheduling is **on (enforcing) by default as of 20.11.1**; in 20.11.0 and earlier it was off. Switch it off with `/CQ`, or observe without enforcing with `/Sq`.

## Fixes

- **Linux startup crash.** Every `GeoDmsRun` and `GeoDmsGuiQt` invocation segfaulted at startup on Linux — before `main`, so all unit tests failed with exit 139. A static-initialization-order fiasco: on Linux `RTC_GetRegDWord` is served from an ini cache whose backing `std::map` was a namespace-scope static, and callers running during another translation unit's static initialization reached it before its constructor had run. Two such callers existed — the allocator's drainage poll, and `GetTokenID_st` via the case-mixup warning flag, which fires on every static token registration. The ini cache is now a function-local static, constructed on first use, and the allocator no longer consults configuration at all until the process has allocated 8 GB. Windows was never affected, since it reads the real registry.
- **Two GCC-only compile errors** that broke the Linux build of 20.11.0. A local variable in `DiscrAlloc` shadowed an enclosing template parameter, which is ill-formed ([temp.local]/6) — GCC rejects it, MSVC accepts it silently. And appending a sequence element in `Index.cpp` had to move from `push_back` to `emplace_back`, because copy-initialising `SharedStr` from `SA_ConstReference<char>` is ambiguous under GCC while the direct-initialisation `emplace_back` performs is not; `BitVector` gained a forwarding `emplace_back` so generic code compiles against both branches of the element vector.

## Switching the memory features on and off

Both features are controlled the same way: a command-line switch for the session, or a registry / `geodms.ini` setting for the machine. `/S<x>` switches on, `/C<x>` switches off.

| Feature | On | Off | Setting | Default |
|---|---|---|---|---|
| Free-store drainage | `/SF` | `/CF` | `MemoryDrainage` | **1 (on)** |
| Resource-aware scheduling, enforcing | `/SQ` | `/CQ` | `ResourceAwareScheduling` = 2 | **2 (on)** |
| Resource-aware scheduling, observe only | `/Sq` | `/Cq` | `ResourceAwareScheduling` = 1 | |

`ResourceAwareScheduling` takes three values: `0` off, `1` shadow (the ledger computes and logs its decisions but never defers anything), `2` enforce (decisions are acted on). Shadow mode is the safe way to see what the scheduler *would* do on a workload before letting it act.

**Both features share one threshold.** Drainage starts when RAM use passes `MemoryFlushThreshold` (default **80**, a percentage), and the scheduler's admission budget is that same percentage of total allowed physical memory — so by default they engage at the same point, which is the intent. Two things can separate them:

- `SchedulerBudgetMB`, if non-zero, overrides the scheduler's budget with an absolute figure in MB, leaving drainage on the percentage.
- Both are computed against *allowed* physical memory, which `MemoryMaxRAM_GB` clamps — and that defaults to **64**. On a machine with more RAM than that, raise `MemoryMaxRAM_GB` or both features will engage far earlier than the raw percentage suggests.

Beyond the shared threshold the two are also linked directly: while the scheduler is enforcing and a task is waiting for budget, drainage runs regardless of the RAM percentage, so the memory it is waiting for is actively returned to the OS.
