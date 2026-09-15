# The two checks of batch\TestShippedDms.bat that are easier in PowerShell than in cmd
# (GeoDMS-Test #24). Run against an OUTPUT folder (bin\<Config>\x64, build\...\bin, bin_GLOBIO\...):
#
#   1. the shipped copy of the battery is current: every testcases\*.dms, the item map and the
#      runners exist under <bin>\examples\testcases and are not older than the source. A stale
#      copy runs a battery that is not the one that ships, and would pass on cases that were
#      since changed. CopyResources (msbuild) and DeployResources (cmake) mirror it on a build;
#      the setup scripts build first, so this only fires on an ad-hoc run.
#
#   2. every shipped .dms is reached from a battery case: starting from the shipped
#      examples\testcases\shipped_*.dms, every #include is followed (a %exeDir% path against
#      <bin>, a bare name against <dir>\<stem>\ then <dir>\, the way the engine resolves it),
#      and the set reached must contain every examples\*.dms and library\**\*.dms that ships.
#      A new shipped file that no case includes fails here, with its name.
#
# Usage: TestShippedDms.ps1 -Bin <output folder> -Source <repo>\testcases
# Exit code: 0 when both hold.
param(
    [Parameter(Mandatory)][string]$Bin,
    [Parameter(Mandatory)][string]$Source
)
$Bin    = (Resolve-Path $Bin).Path.TrimEnd('\')
$Source = (Resolve-Path $Source).Path.TrimEnd('\')
$copy   = Join-Path $Bin 'examples\testcases'
$rc = 0

# --- 1. the shipped copy of the battery is current ---------------------------------------
$stale = @()
foreach ($src in Get-ChildItem $Source -File | Where-Object { $_.Extension -in '.dms', '.txt', '.bat', '.ps1' }) {
    $dst = Join-Path $copy $src.Name
    if (-not (Test-Path $dst)) { $stale += "$($src.Name) (missing)"; continue }
    if ($src.LastWriteTimeUtc -gt (Get-Item $dst).LastWriteTimeUtc) { $stale += "$($src.Name) (source is newer)" }
}
if ($stale.Count) {
    Write-Host "*** the shipped copy of the battery in $copy is not the one in $Source - rebuild so the resource copy mirrors it ***"
    $stale | ForEach-Object { Write-Host "    $_" }
    $rc = 1
} else {
    Write-Host "shipped battery copy is current: $((Get-ChildItem $Source -Filter *.dms).Count) cases"
}

# --- 2. every shipped .dms is reached from a battery case ---------------------------------
$reached = @{}
$queue = New-Object System.Collections.Generic.Queue[string]
foreach ($c in Get-ChildItem (Join-Path $copy 'shipped_*.dms')) { $queue.Enqueue($c.FullName) }
if ($queue.Count -eq 0) {
    Write-Host "*** no shipped_*.dms case in $copy - the battery that covers the shipped content is not there ***"
    $rc = 1
}
while ($queue.Count) {
    $f = $queue.Dequeue()
    $key = $f.ToLower()
    if ($reached.ContainsKey($key)) { continue }
    $reached[$key] = $true
    $text = Get-Content $f -Raw
    foreach ($m in [regex]::Matches($text, '#include\s*[<"]([^>"]+)[>"]')) {
        $inc = $m.Groups[1].Value.Trim() -replace '/', '\'
        if ($inc -match '^%exeDir%\\(.+)$') {
            $p = Join-Path $Bin $Matches[1]
        } elseif ($inc -match '^%') {
            continue   # another placeholder; not a shipped file
        } else {
            $dir  = Split-Path $f -Parent
            $stem = [IO.Path]::GetFileNameWithoutExtension($f)
            $p = Join-Path (Join-Path $dir $stem) $inc
            if (-not (Test-Path $p)) { $p = Join-Path $dir $inc }
        }
        if (Test-Path $p) {
            $queue.Enqueue((Resolve-Path $p).Path)
        } else {
            Write-Host "*** $f includes $inc, which does not exist under $Bin ***"
            $rc = 1
        }
    }
}
$shipped = @(Get-ChildItem (Join-Path $Bin 'examples\*.dms')) + @(Get-ChildItem (Join-Path $Bin 'library') -Recurse -Filter *.dms)
$uncovered = @($shipped | Where-Object { -not $reached.ContainsKey($_.FullName.ToLower()) })
foreach ($u in $uncovered) {
    Write-Host "*** shipped but reached by no battery case: $($u.FullName.Substring($Bin.Length + 1)) - add a testcases\shipped_*.dms that includes it ***"
    $rc = 1
}
Write-Host "shipped .dms files outside the battery: $($shipped.Count), reached from the shipped_*.dms cases: $($shipped.Count - $uncovered.Count)"

exit $rc
