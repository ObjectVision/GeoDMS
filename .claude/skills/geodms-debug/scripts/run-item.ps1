<#
.SYNOPSIS
    Runs one or more items of a .dms configuration through the local GeoDmsRun and reports.

.DESCRIPTION
    Wraps the invocation that is easy to get wrong by hand: /L first and absolute, the status
    flags before the configuration, stdout and stderr captured to files (so PowerShell 5.1
    cannot turn a native stderr line into a bogus exit 255), the elapsed time measured, the
    [E] lines of the log and the stderr text (where a Debug assertion lands) shown afterwards.

    The elapsed time is printed on purpose. A bare item request updates metadata only and
    reports success in milliseconds without computing anything; -Statistics or an
    IntegrityCheck on the item is what forces the data.

.EXAMPLE
    .\run-item.ps1 -Config C:\dev\GeoDMS_2026\scratch\probe.dms -Item /checks -Statistics

.EXAMPLE
    .\run-item.ps1 -Config C:\dev\GeoDMS_2026\testcases\fn_test_connect_matrix.dms -Item /checks -Debug64
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Config,

    # One or more root-relative item paths (the top-level container is the root, so /d not /cfg/d).
    [Parameter(Mandatory = $true)]
    [string[]]$Item,

    # Prepend @statistics so the items are computed and their min/max/sum/nulls printed.
    [switch]$Statistics,

    # Use bin\Debug\x64 instead of bin\Release\x64.
    [switch]$Debug64,

    # Explicit exe; overrides -Debug64.
    [string]$Exe,

    # Status flags, placed before the configuration. Default is what the unit suite uses.
    [string[]]$Flags = @('/S1', '/S2', '/S3'),

    [string]$LogDir,

    # Print the whole log instead of only its [E] lines.
    [switch]$ShowLog
)

$ErrorActionPreference = 'Stop'

# skills\geodms-debug\scripts -> repo root is four levels up
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path

if (-not $Exe) {
    $cfgDir = if ($Debug64) { 'Debug' } else { 'Release' }
    $Exe = Join-Path $repo "bin\$cfgDir\x64\GeoDmsRun.exe"
}
if (-not (Test-Path $Exe)) { throw "GeoDmsRun not found: $Exe (build that configuration first)" }
$Config = (Resolve-Path $Config).Path

if (-not $LogDir) { $LogDir = Join-Path $repo 'scratch\run-item' }
New-Item -ItemType Directory -Force $LogDir | Out-Null

$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$safe = (($Item -join '_') -replace '[\\/:*?"<>|@]', '_').Trim('_')
if ($safe.Length -gt 60) { $safe = $safe.Substring($safe.Length - 60) }
$base = Join-Path $LogDir "$stamp`_$safe"
$log = "$base.log"
$out = "$base.out"
$err = "$base.err"

$args = @("/L$log") + $Flags + @($Config)
if ($Statistics) { $args += '@statistics' }
$args += $Item

Write-Host "exe    : $Exe  (built $((Get-Item $Exe).LastWriteTime.ToString('yyyy-MM-dd HH:mm:ss')))"
Write-Host "config : $Config"
Write-Host "args   : $($args -join ' ')"
Write-Host "log    : $log"
Write-Host ''

$sw = [System.Diagnostics.Stopwatch]::StartNew()
$p = Start-Process -FilePath $Exe -ArgumentList $args -NoNewWindow -Wait -PassThru `
        -RedirectStandardOutput $out -RedirectStandardError $err
$sw.Stop()
$code = $p.ExitCode
$secs = [math]::Round($sw.Elapsed.TotalSeconds, 3)

$verdict = switch ($code) {
    0       { 'ok' }
    1       { 'calculation error, failed IntegrityCheck, or item not found' }
    2       { 'parse/load failure, unknown option, or structured exception caught at main' }
    3       { 'Debug assertion (headless exit)' }
    default { 'unexpected exit code (a crash code is negative or 0xC...)' }
}
Write-Host "exit $code ($verdict) in $secs s"

if ((Test-Path $err) -and (Get-Item $err).Length -gt 0) {
    Write-Host ''
    Write-Host 'stderr:'
    Get-Content $err | Select-Object -First 40 | ForEach-Object { Write-Host "  $_" }
}

if (Test-Path $log) {
    if ($ShowLog) {
        Get-Content $log
    } else {
        $errs = Select-String -Path $log -Pattern '\[E\]'
        if ($errs) {
            Write-Host ''
            Write-Host 'error lines in the log:'
            $errs | Select-Object -First 40 | ForEach-Object { Write-Host "  $($_.Line)" }
            if ($errs.Count -gt 40) { Write-Host "  ... and $($errs.Count - 40) more" }
        }
    }
}

if ($Statistics -and (Test-Path $out)) {
    Write-Host ''
    Write-Host "statistics (from $out), one line per data item:"
    # GeoDmsRun prints one HTML table per data item, a <TD>key</TD><TD>value</TD> row per
    # figure. Fold each table into a single line; a container heading without rows is skipped.
    function Emit-Item([string]$name, [hashtable]$v) {
        if (-not $name -or $v.Count -eq 0) { return }
        Write-Host ('  {0}: min={1} max={2} sum={3} avg={4} nulls={5} count={6} ({7})' -f $name,
            $v['Minimum'], $v['Maximum'], $v['Sum'], $v['Average'], $v['#Nulls'], $v['Count'], $v['ValuesType'])
    }
    $name = $null
    $vals = @{}
    foreach ($raw in Get-Content $out) {
        if ($raw -match 'Statistics for (\S+):') {
            Emit-Item $name $vals
            $name = $Matches[1]
            $vals = @{}
            continue
        }
        if ($raw -match '<TD>([^<]+)</TD>\s*<TD>([^<]*)</TD>') { $vals[$Matches[1].Trim()] = $Matches[2].Trim() }
    }
    Emit-Item $name $vals
}

if ($code -eq 0 -and -not $Statistics -and $secs -lt 0.5) {
    Write-Host ''
    Write-Host "Note: finished in $secs s without @statistics. That proves parse, name resolution and" -ForegroundColor Yellow
    Write-Host 'domain checks, not that anything was computed. Add -Statistics or an IntegrityCheck.' -ForegroundColor Yellow
}

exit $code
