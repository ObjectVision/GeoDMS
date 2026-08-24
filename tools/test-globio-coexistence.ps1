[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$OutputDir,

    [string]$GlobioRoot = $env:GLOBIO_ENV_ROOT
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
& (Join-Path $PSScriptRoot 'verify-globio-environment.ps1') -GlobioRoot $GlobioRoot

$resolvedOutput = (Resolve-Path -LiteralPath $OutputDir).Path
$python = Join-Path $GlobioRoot 'python.exe'
$test = Join-Path $repoRoot 'python\tst\GlobioCompatibility.py'
$previousPath = $env:PATH
try {
    $env:PATH = "$(Join-Path $GlobioRoot 'Library\bin');$GlobioRoot;$(Join-Path $GlobioRoot 'Scripts');$previousPath"
    foreach ($order in 'osgeo-first', 'geodms-first') {
        & $python -u $test $order $resolvedOutput
        if ($LASTEXITCODE -ne 0) {
            throw "GLOBIO coexistence test '$order' failed with exit $LASTEXITCODE."
        }
    }
}
finally {
    $env:PATH = $previousPath
}
Write-Host 'GLOBIO/GeoDMS coexistence passed in both import orders.'
