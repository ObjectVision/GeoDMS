# The two checks of batch\TestShippedDms.bat that are easier in PowerShell than in cmd
# (GeoDMS-Test #24). Run against an OUTPUT folder (bin\<Config>\x64, build\...\bin, bin_GLOBIO\...):
#
#   1. the shipped copy of the battery is current: every testcases\*.dms, the item map and the
#      runners exist under <bin>\examples\testcases with the content of the source. A stale
#      copy runs a battery that is not the one that ships, and would pass on cases that were
#      since changed. CopyResources (msbuild) and DeployResources (cmake) mirror it on a build;
#      the setup scripts build first, so this only fires on an ad-hoc run. The content is
#      compared, not the time: an NSIS install stores file times to 2 seconds, so an installed
#      copy is up to 2 s older than its source; compared by time, it failed the 20.23.0.g setup,
#      whose launcher runs this on the installed copy.
#
#   2. every shipped .dms is reached from a battery case: starting from the shipped
#      examples\testcases\shipped_*.dms, every #include is followed (a %exeDir% path against
#      <bin>, a bare name against <dir>\<stem>\ then <dir>\, the way the engine resolves it),
#      and the set reached must contain every examples\*.dms and library\**\*.dms that ships.
#      A new shipped file that no case includes fails here, with its name. Every name in an
#      include is compared in case: Windows finds a file named in the wrong case, but the same
#      files ship in the Linux setup, where that include fails (#1359).
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
    if ((Get-FileHash $src.FullName).Hash -ne (Get-FileHash $dst).Hash) { $stale += "$($src.Name) (differs from the source)" }
}
if ($stale.Count) {
    Write-Host "*** the shipped copy of the battery in $copy is not the one in $Source - rebuild so the resource copy mirrors it ***"
    $stale | ForEach-Object { Write-Host "    $_" }
    $rc = 1
} else {
    Write-Host "shipped battery copy is current: $((Get-ChildItem $Source -Filter *.dms).Count) cases"
}

# --- 2. every shipped .dms is reached from a battery case ---------------------------------

# Resolve $rel under $base one name at a time against the names in each folder, and return the
# path with the names as they are on disk, or $null. Test-Path ignores case, so it cannot tell
# an include that Linux finds from one that it does not; -IgnoreCase gives the file meant.
function Resolve-Names([string]$base, [string]$rel, [switch]$IgnoreCase) {
    $cur = $base
    foreach ($part in ($rel -split '[\\/]')) {
        if ($part -eq '' -or $part -eq '.') { continue }
        if ($part -eq '..') { $cur = Split-Path $cur -Parent; continue }
        $hit = Get-ChildItem -LiteralPath $cur -Force -ErrorAction SilentlyContinue |
            Where-Object { if ($IgnoreCase) { $_.Name -eq $part } else { $_.Name -ceq $part } } |
            Select-Object -First 1
        if (-not $hit) { return $null }
        $cur = $hit.FullName
    }
    if (Test-Path -LiteralPath $cur -PathType Leaf) { return $cur }
    return $null
}

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
            $rel = $Matches[1]; $bases = @($Bin)
        } elseif ($inc -match '^%') {
            continue   # another placeholder; not a shipped file
        } else {
            $dir  = Split-Path $f -Parent
            $stem = [IO.Path]::GetFileNameWithoutExtension($f)
            $rel = $inc; $bases = @((Join-Path $dir $stem), $dir)
        }
        $p = $null
        foreach ($b in $bases) { if (-not $p) { $p = Resolve-Names $b $rel } }
        if ($p) {
            $queue.Enqueue($p)
            continue
        }
        foreach ($b in $bases) { if (-not $p) { $p = Resolve-Names $b $rel -IgnoreCase } }
        if ($p) {
            Write-Host "*** $f includes $inc, but the file is $($p.Substring($Bin.Length + 1)): the Linux setup does not find it ***"
            $queue.Enqueue($p)   # still follow it, so that what it includes is counted as reached
        } else {
            Write-Host "*** $f includes $inc, which does not exist under $Bin ***"
        }
        $rc = 1
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
