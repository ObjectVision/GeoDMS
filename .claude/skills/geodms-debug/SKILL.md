---
name: geodms-debug
description: Reproducing and diagnosing GeoDMS engine and GUI behaviour from the just-built binaries in bin\<Config>\x64. Covers the GeoDmsRun command line and its exit codes, why a bare item request computes nothing and @statistics or an IntegrityCheck does, Debug-build assertions (headless exit 3 versus the modal abort dialog), getting a stack with cdb by attaching, driving GeoDmsGuiQt headlessly with a /T script or interactively, writing a probe .dms, and the sandbox traps of the agent's own shell (virtualised registry and user profile, shared working directory). Use when asked to reproduce an issue, verify a fix on data, capture an assertion, or test a GUI feature.
---

# Reproducing and diagnosing on the local build

The binaries are `C:\dev\GeoDMS_2026\bin\Release\x64` and `bin\Debug\x64` (see geodms-build
for making them current). Everything below assumes the tree is quiet; a probe that runs
during someone else's link reports nonsense.

## A headless run

```
GeoDmsRun.exe [/L<LogFile>] [/S1 /S2 /S3 /SP /SW ...] <Config.dms> [<Item>|@<verb> ...]
```

- `/L` must be the first argument and the path absolute. A relative `/L` writes no log and the
  run just hangs to its timeout. Build the argument as one string, `"/LC:\...\x.log"`.
- Status flags come before the config. `/S1 /S2 /S3` are the multithreading levels the unit
  suite runs with; use them so a probe sees the same scheduling. `/SP` writes performance
  and memory diagnostics to the log.
- Item paths are relative to the desktop root, and the config's top-level container is that
  root: for `container foo { unit d { ... } }` the item is `/d`, not `/foo/d`. A wrong
  prefix reports `the specified item '/foo/d' was not found` and exit 1.
- A bare item request (the default `@commit` verb) updates the item to metadata level and
  reports success in about zero seconds without running the operator. Exit 0 there proves
  parse, name resolution and domain checks, nothing about the data. Two things force the
  data: `@statistics /item` (prints min, max, sum, average, variance, nulls and count for
  the item and its whole subtree, as HTML on stdout) and an IntegrityCheck on a parameter
  that depends on it. Look at the elapsed time before drawing any conclusion.
- Other verbs: `@dumpconfig <out.dms>`, `@sourcedescr <item>`, `@checkfunctions`,
  `@file <out>` to redirect the statistics output. `@statistics` is its own argument; glued
  to the item path it is looked up as an item and fails as "not found".
- Exit codes: 0 ok; 1 a calculation error, a failed IntegrityCheck or an item not found;
  2 a parse or load failure, an unknown option, or a structured exception caught at main;
  3 a Debug-build assertion in a headless run. Also grep the log for `[E]`: some storage
  errors are logged and the run still ends 0 with an empty result.

Invoke it through `cmd /c` from PowerShell 5.1, or from Bash with `MSYS_NO_PATHCONV=1`:

```powershell
& cmd /c "C:\dev\GeoDMS_2026\bin\Release\x64\GeoDmsRun.exe /LC:\dev\GeoDMS_2026\scratch\p.log /S1 /S2 /S3 C:\dev\GeoDMS_2026\scratch\probe.dms @statistics /checks > C:\dev\GeoDMS_2026\scratch\p.out 2>&1"
```

PowerShell that pipes a native exe's stderr reports exit 255 on success; Bash rewrites
`/checks` into `C:/Program Files/Git/checks`. The script beside this skill wraps all of it:

```powershell
.\.claude\skills\geodms-debug\scripts\run-item.ps1 -Config C:\dev\GeoDMS_2026\scratch\probe.dms -Item /checks -Statistics
```

It times the run, prints the exit code, the `[E]` lines, anything on stderr (that is where
an assertion lands) and warns when a run without `@statistics` finished suspiciously fast.
Logs go to `scratch\run-item\`. `scratch\` is gitignored and is where every probe, log and
dump belongs; never the repo root.

Log files and cdb dumps can be UTF-16. If a Bash `grep` finds nothing in a file that clearly
has content, read it with PowerShell `Select-String` or the Read tool instead.

## Debug-build assertions

A failed `assert` or `MG_CHECK` ends in the CRT `abort()`. In `GeoDmsRun` with no debugger
attached, `run/exe/src/MainRun.cpp` routes the text to stderr and exits 3, so a headless run
does not stall; the line `Assertion failed: <cond>, file ..., line N` is in the captured
stderr. Under a debugger, and in `GeoDmsGuiQt`, the modal Retry/Ignore dialog still appears,
drawn through `USER32!MessageBoxW`, and the process parks there holding its DLLs: the next
link fails with `LNK1168`/`LNK1104`, or is skipped silently. Before rebuilding, kill every
`GeoDmsRun`, `GeoDmsGuiQt`, `cdb` and `WerFault` of yours (`Stop-Process -Force`; if a
debug port keeps one alive, `cdb -p <pid> -c ".kill;q"`). Kill only your own: check the
command line of a process before touching it, another session's GUI is theirs.

## A stack for an assertion or a crash

cdb cannot usefully launch the Debug binary: the multithreaded process crawls under the
debugger and never reaches the fault. Attach instead.

1. `$env:_NT_SYMBOL_PATH = 'C:\dev\GeoDMS_2026\bin\Debug\x64'` (local PDBs; the public
   symbol server makes the walk hang).
2. Run the binary plain in the background; on an assertion it parks in the dialog (GUI, or
   GeoDmsRun under a debugger) or exits 3 (headless GeoDmsRun, so for a stack you attach
   before the assert or run under the VS debugger).
3. Attach and dump: `cdb.exe -p <pid> -c '~*kn 50; q'` from
   `C:\Program Files (x86)\Windows Kits\10\Debuggers\x64`. The faulting thread reads
   `ucrtbased!abort` under `wassert` under `throwCheckFailed` under your frames. A full
   `~*kn` over the Tic/Clc PDBs takes minutes; give it a long timeout.
4. When you do drive the target under cdb, put `bu USER32!MessageBoxW` (and
   `bu ucrtbased!wassert`) in a `-cf <scriptfile>` so it breaks before the dialog; inspect,
   then `.kill; q`. Never `qd`, which lets it continue into the dialog. Use `bu`, not `bp`:
   the modules are not loaded yet at launch.
5. A caught structured exception (`Caught at Main: OS Structured Exception 0xC0000005`,
   exit 2, no dialog) leaves nothing to attach to: attach early with `sxe av` then `g`, or
   use the VS debugger.

Some bugs vanish under a debugger (teardown races did). Then run plain, and suppress the
dialog in temporary Debug-only code: `_set_error_mode(_OUT_TO_STDERR);
_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);` before the failing path.

`doc/deadlocks.md` is the lock inventory; a computation at 0% CPU with no error line is a
parked wait, and that document says where the known ones were.

## Verifying a GUI feature

Headless first. The real code path runs from a script, and the log carries what happened:

```
GeoDmsGuiQt.exe /L<abs log> /T<abs scriptfile> <config.dms>
```

The verbs are in `qtgui/exe/src/TestScript.cpp` and on the wiki page `Gui-scripting`; the
test scripts in `C:\dev\tst\dmsscript` are worked examples. The app does not quit by itself,
so end the script with the `WM_CLOSE` send; a modal box (a failed export, for instance)
swallows it, and the log tells you which. `SEND` sub-object paths are 1-based and a 0 ends
the path; the log lines `Activate <class>`, `Execute <caption>` and `exit at end of menu`
say which object and menu item you actually hit. Enumerate a menu without executing anything
by ending the path with 99.

For interactive driving: launch exactly one instance, with the config on the command line,
from a tool call with `dangerouslyDisableSandbox: true`; a sandboxed launch is input- and
screenshot-isolated. Raise it with `SwitchToThisWindow(hwnd, true)` (`SetForegroundWindow`
returns true and does nothing), verify with `GetForegroundWindow`, then send keys with
`WScript.Shell.SendKeys` and capture the window rect with `System.Drawing.CopyFromScreen`.
That capture is not covered by the Claude window, which in computer-use screenshots sits as
a black rectangle over the GeoDMS window and its dialogs; an `EnumWindows` listing with
`vis=True` and a sane rect is the truth, not the screenshot. `open_application` spawns a
new bare instance every call; click the taskbar button instead. The clipboard is a free
assertion: `Get-Clipboard -Raw` and `-Format Image`.

## The agent's shell is not the machine

- Registry writes and reads from the PowerShell or Bash tool are virtualised: a `reg add`
  looks applied and is invisible to every real process, and real values can be absent in
  the sandboxed view. GeoDMS reads `HKCU\Software\ObjectVision\<COMPUTERNAME>\GeoDMS` first,
  then `...\DMS`. To read or change it for real, run `reg` from a scheduled task, or pass
  `dangerouslyDisableSandbox: true`.
- The user-profile file system has the same overlay: a Python site-packages that imports
  fine in the tool shell may not exist on the real machine. Ground truth comes from a
  scheduled task or the user's own console.
- Bash and PowerShell share one working directory. Use absolute paths in every redirect and
  every long-running call; a background task whose redirect failed still reports exit 0.
- A background job started with `Start-Process` is still a child of the Claude app and dies
  when the app self-updates (observed twice in one day). Anything that must outlive the turn
  starts from a scheduled task.

## Writing a probe config

- One file in `scratch\`, self-contained units, a `container checks` of
  `parameter<bool> x := ..., IntegrityCheck = "x == True";` lines, and always one check you
  know must fail, so you have seen exit 1 work before trusting exit 0. Compare an old and a
  new operator as `parameter<float32> d := max(abs(a - b));` and read `d` with
  `@statistics`.
- Identifiers are case-insensitive: `unit<spoint> W3` beside `attribute<float32> w3` is
  "already defined".
- A domain named `Arc` collides with the `arc` composition keyword: `(Arc)` after an
  attribute parses as the composition and the item silently becomes a sequence with no
  domain. Name arc domains `Road` and write the composition `(arc)` in lower case.
- A `Range` property is a point-stream literal, `[xy(0; 300000), xy(280000; 625000))`, with
  semicolons inside `xy()`; it is not an expression.
- A probe worth keeping becomes `testcases\fn_test_<topic>.dms` or
  `testcases\oper_<name>.dms` with a `/checks` container, offline and cheap; it needs no
  packaging edit. Anything that downloads or reads real data goes to
  `batch\TestShippedContent.bat` instead.
- Old configs under `C:\dev\tst` may use removed syntax such as `point()`; write a fresh one.

## When a probe is not enough

Threading, stack-pressure and meta-thread bugs do not show in small configs. The `t720`
(2BURP) project in the `tst` regression fills the stack within a second and hands the
meta-thread baton to a worker; `doc/development/testing-strategy.md` says how to run a
single project. A full `full.py` round needs the user's consent (geodms-build, tier 3).
