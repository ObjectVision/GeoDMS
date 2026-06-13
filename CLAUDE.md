# GeoDMS — repo notes for Claude

## Running the freshly built GeoDmsGuiQt.exe

An incremental build drops the GUI at:

```
C:\dev\GeoDMS_2026\bin\Release\x64\GeoDmsGuiQt.exe
```

**Always launch it with a specific config `.dms` file as a command-line argument:**

```powershell
& 'C:\dev\GeoDMS_2026\bin\Release\x64\GeoDmsGuiQt.exe' '<path>\some_config.dms'
```

Why the explicit config matters:

- Launched **without** a config argument, the app stops at the splash screen on a protective
  "Reopen last configuration?" confirmation dialog and does **not** show the main window until
  someone clicks Yes/No. Automated/headless drivers get stuck there.
- Passing a config makes it load that file directly and bring up the main window with a
  **recognisable caption** (`<config>.dms (aka ...) in <path> - GeoDms <ver> ...`), which is
  how you confirm you're driving the right instance — important when several `GeoDmsGuiQt.exe`
  processes are around (e.g. an old one still holding the last config).

Tip: kill any stale instances and launch exactly one fresh process so you know you're running
the just-built binary, not an older in-memory one:

```powershell
Get-Process GeoDmsGuiQt -ErrorAction SilentlyContinue | Stop-Process -Force
& 'C:\dev\GeoDMS_2026\bin\Release\x64\GeoDmsGuiQt.exe' '<path>\some_config.dms'
```

For headless feature verification (capture the exact command an action launches, drive
test-script verbs) use the `/L<logfile> /T<scriptfile> <config>` form instead of clicking.
