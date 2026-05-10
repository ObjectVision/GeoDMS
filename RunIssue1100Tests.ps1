# RunIssue1100Tests.ps1
# Iteration harness for ObjectVision/GeoDMS#1100 (Linux/Qt GUI regressions surfaced by Shv.dll cross-platform port).
#
# For each scripted scenario it:
#   1. starts a GUI exe with /T<dmsscript>,
#   2. waits for the main window to appear,
#   3. snaps a PNG of the window after the view has had time to render,
#   4. lets the script's terminal SEND-WM_CLOSE shut the GUI down,
#   5. drains the clipboard (text or bitmap) into a file.
#
# It runs each scenario once with the 19.2.0 reference exe and once with the local build,
# writing into C:\dev\tst\Issue1100\ref\ and C:\dev\tst\Issue1100\cur\.
# At the end it diffs text outputs and reports image-byte equality.
#
# Usage:
#   pwsh -File RunIssue1100Tests.ps1                         # default: cur = bin\Release\x64
#   pwsh -File RunIssue1100Tests.ps1 -CurFlavor Debug        # cur from bin\Debug\x64
#   pwsh -File RunIssue1100Tests.ps1 -RefOnly                # only refresh ref outputs
#   pwsh -File RunIssue1100Tests.ps1 -CurOnly                # only refresh cur outputs

[CmdletBinding()]
param(
    [ValidateSet('Debug','Release')]
    [string]$CurFlavor = 'Release',
    [string]$RefExe = 'C:\Program Files\ObjectVision\GeoDms19.2.0\GeoDmsGuiQt.exe',
    [string]$Cfg    = 'C:\dev\tst\Operator\cfg\MicroTst.dms',
    [string]$ScriptDir = 'C:\dev\tst\dmsscript',
    [string]$OutRoot   = 'C:\dev\tst\Issue1100',
    [int]   $RenderWaitMs = 7000,
    [switch]$RefOnly,
    [switch]$CurOnly
)

$ErrorActionPreference = 'Stop'

$CurExe = "C:\dev\GeoDMS26\bin\$CurFlavor\x64\GeoDmsGuiQt.exe"

# Commands that exist in the current source but NOT in the 19.2.0 parser.
# When running against a legacy exe we strip these from the dmsscript before launch
# (19.2.0's TestScript.cpp falls through with uninitialised COPYDATASTRUCT on unknown
# keywords and may dispatch garbage as a CommandCode).
$LegacyIncompatibleCommands = @('BringToFront')

if (-not (Test-Path $RefExe)) { throw "Reference exe not found: $RefExe" }
if (-not (Test-Path $CurExe)) { throw "Current exe not found:   $CurExe" }
if (-not (Test-Path $Cfg))    { throw "Config not found: $Cfg" }
if (-not (Test-Path $ScriptDir)) { throw "Script dir not found: $ScriptDir" }

$Scenarios = @(
    [pscustomobject]@{
        Name         = 'tableview_district'
        Script       = Join-Path $ScriptDir 'Issue1100_tableview_district.dmsscript'
        ClipboardKind= 'Text'
        ClipExt      = '.csv'
    }
    [pscustomobject]@{
        Name         = 'mapview_district_hoek'
        Script       = Join-Path $ScriptDir 'Issue1100_mapview_district_hoek.dmsscript'
        ClipboardKind= 'Image'
        ClipExt      = '.png'
    }
)

# --- Win32 P/Invoke ----------------------------------------------------------------
# We capture via CopyFromScreen (the actual on-screen pixels) rather than PrintWindow,
# so the GUI must be visible and on top of the Z-order. The script-level BringToFront
# command (added in TestScript.cpp) handles current builds; BringWindowToTop here is
# the equivalent fallback for legacy exes (19.2.0) where that command does not exist.
if (-not ('Issue1100.Win32' -as [type])) {
Add-Type -Namespace Issue1100 -Name Win32 -MemberDefinition @'
    [System.Runtime.InteropServices.DllImport("user32.dll")]
    public static extern bool GetWindowRect(System.IntPtr hWnd, out RECT lpRect);

    [System.Runtime.InteropServices.DllImport("user32.dll")]
    public static extern bool IsWindowVisible(System.IntPtr hWnd);

    [System.Runtime.InteropServices.DllImport("user32.dll")]
    public static extern bool BringWindowToTop(System.IntPtr hWnd);

    [System.Runtime.InteropServices.DllImport("user32.dll")]
    public static extern bool SetWindowPos(System.IntPtr hWnd, System.IntPtr hWndInsertAfter,
        int X, int Y, int cx, int cy, uint uFlags);

    [System.Runtime.InteropServices.StructLayout(System.Runtime.InteropServices.LayoutKind.Sequential)]
    public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
'@
}

# Make this PowerShell process per-monitor DPI-aware so PrintWindow gives us native
# pixels on a 125%-scaled monitor instead of a downscaled bitmap. This must run before
# any HDC is created. Best-effort: fails silently on older systems / if already set.
try {
    if (-not ('Issue1100.Dpi' -as [type])) {
        Add-Type -Namespace Issue1100 -Name Dpi -MemberDefinition @'
            [System.Runtime.InteropServices.DllImport("user32.dll")]
            public static extern bool SetProcessDpiAwarenessContext(System.IntPtr value);
'@
    }
    # DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 = -4
    [void][Issue1100.Dpi]::SetProcessDpiAwarenessContext([IntPtr]::new(-4))
} catch { }

Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms

function Wait-MainWindow {
    param([System.Diagnostics.Process]$Proc, [int]$TimeoutMs = 20000)
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($sw.ElapsedMilliseconds -lt $TimeoutMs) {
        if ($Proc.HasExited) { return [IntPtr]::Zero }
        $Proc.Refresh()
        $h = $Proc.MainWindowHandle
        if ($h -ne [IntPtr]::Zero -and [Issue1100.Win32]::IsWindowVisible($h)) { return $h }
        Start-Sleep -Milliseconds 100
    }
    return [IntPtr]::Zero
}

function Raise-Window {
    param([IntPtr]$Hwnd)
    # Raise + show without stealing keyboard focus. SWP_NOACTIVATE keeps the caller calm,
    # SWP_SHOWWINDOW makes sure the window is not still hidden/minimized.
    $HWND_TOP = [IntPtr]::Zero
    $SWP_NOMOVE = 0x0002
    $SWP_NOSIZE = 0x0001
    $SWP_NOACTIVATE = 0x0010
    $SWP_SHOWWINDOW = 0x0040
    [void][Issue1100.Win32]::SetWindowPos($Hwnd, $HWND_TOP, 0, 0, 0, 0,
        ($SWP_NOMOVE -bor $SWP_NOSIZE -bor $SWP_NOACTIVATE -bor $SWP_SHOWWINDOW))
    [void][Issue1100.Win32]::BringWindowToTop($Hwnd)
}

function Save-WindowScreenshot {
    param([IntPtr]$Hwnd, [string]$OutPath)
    $r = New-Object Issue1100.Win32+RECT
    if (-not [Issue1100.Win32]::GetWindowRect($Hwnd, [ref]$r)) { throw "GetWindowRect failed" }
    $w = [Math]::Max(1, $r.Right - $r.Left)
    $h = [Math]::Max(1, $r.Bottom - $r.Top)
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    try {
        # CopyFromScreen reads actual displayed pixels, so the window must be on top.
        # Z-order is handled by BringToFront (in the dmsscript) plus Raise-Window from PS.
        $g.CopyFromScreen($r.Left, $r.Top, 0, 0, (New-Object System.Drawing.Size $w, $h))
        $bmp.Save($OutPath, [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $g.Dispose(); $bmp.Dispose()
    }
}

function Save-Clipboard {
    param([string]$Kind, [string]$OutPath)
    # System.Windows.Forms.Clipboard requires STA; PowerShell 7 default is MTA.
    # Run in a fresh STA thread.
    $script = {
        param($Kind, $OutPath)
        Add-Type -AssemblyName System.Windows.Forms
        Add-Type -AssemblyName System.Drawing
        if ($Kind -eq 'Text') {
            if ([System.Windows.Forms.Clipboard]::ContainsText()) {
                [System.IO.File]::WriteAllText($OutPath, [System.Windows.Forms.Clipboard]::GetText(), [System.Text.Encoding]::UTF8)
                return 'OK'
            } else { return 'EMPTY' }
        } else {
            if ([System.Windows.Forms.Clipboard]::ContainsImage()) {
                $img = [System.Windows.Forms.Clipboard]::GetImage()
                $img.Save($OutPath, [System.Drawing.Imaging.ImageFormat]::Png)
                $img.Dispose()
                return 'OK'
            } else { return 'EMPTY' }
        }
    }
    $rs = [runspacefactory]::CreateRunspace()
    $rs.ApartmentState = 'STA'
    $rs.ThreadOptions  = 'UseNewThread'
    $rs.Open()
    try {
        $ps = [PowerShell]::Create().AddScript($script).AddArgument($Kind).AddArgument($OutPath)
        $ps.Runspace = $rs
        $r = $ps.Invoke()
        return ($r | Select-Object -Last 1)
    } finally {
        $rs.Close(); $rs.Dispose()
    }
}

function Get-CompatibleScript {
    param([string]$Path, [bool]$IsLegacy)
    if (-not $IsLegacy) { return $Path }
    # Strip lines that invoke commands the legacy parser does not know about.
    # Keep line numbers and surrounding text identical so any error messages remain
    # human-readable, by replacing the keyword with a // comment marker.
    $src = Get-Content -LiteralPath $Path
    $needsRewrite = $false
    $rewritten = foreach ($line in $src) {
        $modified = $line
        foreach ($cmd in $LegacyIncompatibleCommands) {
            if ($modified -match "^\s*$([regex]::Escape($cmd))\b") {
                $modified = "// (stripped for legacy exe) $modified"
                $needsRewrite = $true
                break
            }
        }
        $modified
    }
    if (-not $needsRewrite) { return $Path }
    $tmp = [System.IO.Path]::Combine([System.IO.Path]::GetTempPath(),
        ('Issue1100_legacy_' + [System.IO.Path]::GetFileName($Path)))
    Set-Content -LiteralPath $tmp -Value $rewritten -Encoding UTF8
    return $tmp
}

function Run-Scenario {
    param(
        [string]$Exe,
        [pscustomobject]$Scenario,
        [string]$OutDir,
        [bool]  $IsLegacy
    )
    New-Item -Path $OutDir -ItemType Directory -Force | Out-Null
    $shotPath = Join-Path $OutDir ($Scenario.Name + '_window.png')
    $clipPath = Join-Path $OutDir ($Scenario.Name + '_clipboard' + $Scenario.ClipExt)
    Remove-Item $shotPath, $clipPath -ErrorAction SilentlyContinue

    $effectiveScript = Get-CompatibleScript -Path $Scenario.Script -IsLegacy $IsLegacy
    if ($effectiveScript -ne $Scenario.Script) {
        Write-Host "  [$($Scenario.Name)] using rewritten script: $effectiveScript"
    }

    Write-Host "  [$($Scenario.Name)] launching $Exe"
    $argList = @("/T$effectiveScript", '/S1', '/S2', '/S3', $Cfg, 'test_log')
    $proc = Start-Process -FilePath $Exe -ArgumentList $argList -PassThru
    $hwnd = Wait-MainWindow -Proc $proc
    if ($hwnd -eq [IntPtr]::Zero) {
        Write-Warning "    no main window appeared"
    } else {
        # Belt-and-suspenders: raise the window from PS as well. For legacy exes this
        # is the only Z-order push; for current builds the dmsscript also calls
        # BringToFront, but a duplicate raise is harmless.
        Raise-Window -Hwnd $hwnd
        Start-Sleep -Milliseconds $RenderWaitMs
        try {
            # Re-raise immediately before capture in case Qt restacked between our
            # raise and the end of the wait (e.g. modal popups, splash repaints).
            Raise-Window -Hwnd $hwnd
            Start-Sleep -Milliseconds 250
            Save-WindowScreenshot -Hwnd $hwnd -OutPath $shotPath
            Write-Host "    screenshot -> $shotPath"
        } catch {
            Write-Warning "    screenshot failed: $_"
        }
    }
    if (-not $proc.WaitForExit(30000)) {
        Write-Warning "    GUI did not exit within 30s; killing"
        $proc.Kill()
    }

    $clipResult = Save-Clipboard -Kind $Scenario.ClipboardKind -OutPath $clipPath
    if ($clipResult -eq 'OK') {
        Write-Host "    clipboard ($($Scenario.ClipboardKind)) -> $clipPath"
    } else {
        Write-Warning "    clipboard $($Scenario.ClipboardKind) was empty"
    }
}

function Compare-Outputs {
    param([pscustomobject]$Scenario, [string]$RefDir, [string]$CurDir)
    $shot = $Scenario.Name + '_window.png'
    $clip = $Scenario.Name + '_clipboard' + $Scenario.ClipExt
    $rs = Join-Path $RefDir $shot;  $cs = Join-Path $CurDir $shot
    $rc = Join-Path $RefDir $clip;  $cc = Join-Path $CurDir $clip

    Write-Host "[$($Scenario.Name)] diff:"

    foreach ($pair in @(@{kind='screenshot'; ref=$rs; cur=$cs}, @{kind='clipboard';  ref=$rc; cur=$cc})) {
        if (-not (Test-Path $pair.ref)) { Write-Host "    $($pair.kind): MISSING ref"; continue }
        if (-not (Test-Path $pair.cur)) { Write-Host "    $($pair.kind): MISSING cur"; continue }
        $rh = (Get-FileHash $pair.ref -Algorithm SHA256).Hash
        $ch = (Get-FileHash $pair.cur -Algorithm SHA256).Hash
        if ($rh -eq $ch) {
            Write-Host "    $($pair.kind): identical"
        } else {
            $rl = (Get-Item $pair.ref).Length
            $cl = (Get-Item $pair.cur).Length
            Write-Host "    $($pair.kind): DIFFER (ref $rl B, cur $cl B)"
            if ($Scenario.ClipboardKind -eq 'Text' -and $pair.kind -eq 'clipboard') {
                # show first ~6 differing lines
                $rt = Get-Content $pair.ref
                $ct = Get-Content $pair.cur
                $diff = Compare-Object $rt $ct -SyncWindow 0 | Select-Object -First 12
                $diff | ForEach-Object { "        $($_.SideIndicator) $($_.InputObject)" } | Write-Host
            }
        }
    }
}

# --- main ------------------------------------------------------------------------
foreach ($s in $Scenarios) {
    if (-not (Test-Path $s.Script)) { throw "Missing script: $($s.Script)" }
}

$refDir = Join-Path $OutRoot 'ref'
$curDir = Join-Path $OutRoot 'cur'

if (-not $CurOnly) {
    Write-Host "=== Reference run ($RefExe) ==="
    foreach ($s in $Scenarios) { Run-Scenario -Exe $RefExe -Scenario $s -OutDir $refDir -IsLegacy $true }
}
if (-not $RefOnly) {
    Write-Host "=== Current run ($CurExe) ==="
    foreach ($s in $Scenarios) { Run-Scenario -Exe $CurExe -Scenario $s -OutDir $curDir -IsLegacy $false }
}

if (-not $RefOnly -and -not $CurOnly) {
    Write-Host "`n=== Comparison ==="
    foreach ($s in $Scenarios) { Compare-Outputs -Scenario $s -RefDir $refDir -CurDir $curDir }
}

Write-Host "`nDone. Outputs in $OutRoot"
