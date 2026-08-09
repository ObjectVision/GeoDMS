<#
.SYNOPSIS
    Report, BEFORE a build starts, how many vcpkg ports would be rebuilt.

.DESCRIPTION
    A port's vcpkg ABI hash covers the compiler, the triplet, the portfile/baseline AND
    vcpkg's own pinned tools (cmake, powershell, ...). Any of those moving invalidates the
    binary cache, and the first sign of it today is a build that silently spends hours
    recompiling boost/gdal/arrow instead of minutes compiling GeoDMS.

    That happened on 2026-08-09: the 2026-06-03 baseline bump moved vcpkg's pinned cmake
    3.31.10 -> 4.3.2, and the .c flavor spent 47 minutes rebuilding 144 already-installed
    ports with no warning beforehand.

    `vcpkg install --dry-run` answers the question in a few seconds without building
    anything. This wrapper runs it and turns the answer into an exit code, so the setup
    scripts can say so up front rather than discovering it an hour later.

    Deliberately advisory: a full rebuild is the CORRECT outcome of a baseline bump or a
    compiler re-pin. The point is to make it a visible, expected cost instead of a surprise.

.OUTPUTS
    Exit 0  - at most -Threshold ports pending (normal).
    Exit 1  - more than -Threshold pending; the caller decides what to do.
    Exit 2  - the query itself could not run (vcpkg missing, etc). Advisory only: callers
              should warn and continue, never fail a build over a diagnostic.
#>
[CmdletBinding()]
param(
    [string] $RepoRoot,
    [string] $Triplet   = 'x64-windows-v145',
    [int]    $Threshold = 5,
    [int]    $ListMax   = 20
)

$ErrorActionPreference = 'Continue'

# Not a param default: $PSScriptRoot is not reliably populated while the param block is
# being bound under Windows PowerShell 5.1 invoked as -File.
if (-not $RepoRoot) { $RepoRoot = Split-Path -Parent $PSScriptRoot }

$vcpkgExe = Join-Path $RepoRoot 'vcpkg\vcpkg.exe'
if (-not (Test-Path $vcpkgExe)) {
    Write-Host "vcpkg-drift-check: $vcpkgExe not found (fresh checkout?) - skipping."
    exit 2
}

# Same roots the build itself uses. VCPKG_BINARY_SOURCES / VCPKG_DOWNLOADS are inherited
# from the calling setup script on purpose: querying a different cache than the build will
# use would answer the wrong question.
# NB: not $args -- that is an automatic variable in PowerShell.
$vcpkgArgs = @(
    'install', '--dry-run',
    '--triplet', $Triplet,
    "--host-triplet=$Triplet",
    '--vcpkg-root', (Join-Path $RepoRoot 'vcpkg'),
    "--x-manifest-root=$RepoRoot",
    "--x-install-root=$(Join-Path $RepoRoot 'vcpkg_installed')"
)

$out = & $vcpkgExe @vcpkgArgs 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "vcpkg-drift-check: dry-run failed (exit $LASTEXITCODE) - skipping the check."
    $out | Select-Object -Last 5 | ForEach-Object { Write-Host "    $_" }
    exit 2
}

# vcpkg prints an indented list under each header; the pending one is absent entirely when
# there is nothing to do. Entries look like "  * name:triplet@version" or "    name:triplet@version"
# - the '*' marks an indirect dependency, NOT a pending build, so it must not be used to
# classify. Section membership is what counts.
$pending = @()
$inPending = $false
foreach ($line in ($out | ForEach-Object { [string]$_ })) {
    if ($line -match 'will be built and installed|will be rebuilt|will be removed') { $inPending = $true; continue }
    if ($line -match 'are already installed|^\s*$')                                  { $inPending = $false; continue }
    if ($line -notmatch '^\s')                                                       { $inPending = $false; continue }
    if ($inPending -and $line -match '^\s+\*?\s*([^\s:]+):') { $pending += $Matches[1] }
}

$n = $pending.Count
if ($n -le $Threshold) {
    if ($n -eq 0) { Write-Host "vcpkg-drift-check: cache is current, no ports to build." }
    else          { Write-Host "vcpkg-drift-check: $n port(s) to build - normal." }
    exit 0
}

Write-Host ''
Write-Host '***********************************************************************'
Write-Host "*** vcpkg would (re)build $n ports before this build can link.      ***"
Write-Host '***********************************************************************'
Write-Host 'This is what an ABI-hash change looks like. The usual causes, in order:'
Write-Host '  - the vcpkg submodule moved (new baseline => new portfiles AND new pinned'
Write-Host '    tool versions; cmake/powershell versions are ABI inputs for every port)'
Write-Host '  - vcpkg-triplets\<triplet>.cmake changed'
Write-Host '  - the MSVC pin (VCToolsVersion in Directory.Build.props) changed'
Write-Host '  - this build resolves a different vcpkg / downloads root / binary cache'
Write-Host '    than the one that populated vc_archives'
Write-Host ''
Write-Host "Ports (first $ListMax):"
$pending | Select-Object -First $ListMax | ForEach-Object { Write-Host "    $_" }
if ($n -gt $ListMax) { Write-Host "    ... and $($n - $ListMax) more" }
Write-Host ''
exit 1
