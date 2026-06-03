<#
.SYNOPSIS
  Provision the in-repo vcpkg tool (./vcpkg submodule) on a fresh checkout.

.DESCRIPTION
  On a fresh `git clone` the ./vcpkg submodule is registered but empty, and even
  after `git submodule update --init` there is no vcpkg.exe until the tool is
  bootstrapped. The MSBuild/CMake vcpkg integration then fails with
  "vcpkg.exe is not recognized ... exited with code 9009".

  This script makes both steps automatic so a fresh checkout builds with no
  documented manual steps:
    1. If ./vcpkg is empty, check the submodule out (git submodule update --init).
    2. If ./vcpkg/vcpkg.exe is missing, run bootstrap-vcpkg.bat.

  It is idempotent (exits immediately once vcpkg.exe exists) and safe under
  parallel builds (msbuild /m, VS F5): a Global named mutex serializes the
  one-time bootstrap across the concurrent MSBuild nodes.
#>
$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$vcpkgDir = Join-Path $repoRoot 'vcpkg'
$vcpkgExe = Join-Path $vcpkgDir 'vcpkg.exe'

# Fast path: already provisioned.
if (Test-Path $vcpkgExe) { exit 0 }

# Serialize the bootstrap across concurrent MSBuild nodes / project builds.
$mutex = New-Object System.Threading.Mutex($false, 'Global\GeoDMS_vcpkg_bootstrap')
[void]$mutex.WaitOne()
try {
    # Another node may have finished while we waited for the mutex.
    if (Test-Path $vcpkgExe) { exit 0 }

    # Fresh clone: the submodule is registered but its working tree is empty.
    if (-not (Test-Path (Join-Path $vcpkgDir 'bootstrap-vcpkg.bat'))) {
        Write-Host "ensure-vcpkg: checking out ./vcpkg submodule..."
        git -C $repoRoot submodule update --init vcpkg
        if ($LASTEXITCODE -ne 0) { throw "git submodule update --init vcpkg failed ($LASTEXITCODE)" }
    }

    Write-Host "ensure-vcpkg: bootstrapping vcpkg.exe..."
    & (Join-Path $vcpkgDir 'bootstrap-vcpkg.bat') -disableMetrics
    if ($LASTEXITCODE -ne 0) { throw "vcpkg bootstrap failed ($LASTEXITCODE)" }

    if (-not (Test-Path $vcpkgExe)) { throw "bootstrap completed but $vcpkgExe is still missing" }
}
finally {
    $mutex.ReleaseMutex()
    $mutex.Dispose()
}
