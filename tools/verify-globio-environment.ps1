[CmdletBinding()]
param(
    [string]$GlobioRoot = $env:GLOBIO_ENV_ROOT,
    [string]$LockFile
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $LockFile) {
    $LockFile = Join-Path $repoRoot 'vcpkg-globio\environment.yml'
}
if (-not $GlobioRoot) {
    throw 'GLOBIO_ENV_ROOT is not set. Point it at the conda environment created from vcpkg-globio\environment.yml.'
}

$resolvedRoot = (Resolve-Path -LiteralPath $GlobioRoot).Path
$resolvedLock = (Resolve-Path -LiteralPath $LockFile).Path
$metadataDir = Join-Path $resolvedRoot 'conda-meta'
if (-not (Test-Path -LiteralPath $metadataDir -PathType Container)) {
    throw "$resolvedRoot is not a conda environment (conda-meta is missing)."
}

$locked = @{}
foreach ($line in Get-Content -LiteralPath $resolvedLock) {
    if ($line -match '^\s*-\s+([^=\s]+)=([^=\s]+)=([^\s#]+)\s*$') {
        $locked[$Matches[1].ToLowerInvariant()] = [pscustomobject]@{
            Name = $Matches[1]
            Version = $Matches[2]
            Build = $Matches[3]
        }
    }
}
if (-not $locked.Count) {
    throw "No exact name=version=build package locks found in $resolvedLock."
}

$installed = @{}
foreach ($metadataFile in Get-ChildItem -LiteralPath $metadataDir -Filter '*.json' -File) {
    $metadata = Get-Content -LiteralPath $metadataFile.FullName -Raw | ConvertFrom-Json
    if ($metadata.name) {
        $installed[$metadata.name.ToString().ToLowerInvariant()] = $metadata
    }
}

$mismatches = @()
foreach ($entry in $locked.Values) {
    $key = $entry.Name.ToLowerInvariant()
    if (-not $installed.ContainsKey($key)) {
        $mismatches += "$($entry.Name): missing (expected $($entry.Version)=$($entry.Build))"
        continue
    }
    $actual = $installed[$key]
    if ($actual.version -cne $entry.Version -or $actual.build -cne $entry.Build) {
        $mismatches += "$($entry.Name): $($actual.version)=$($actual.build), expected $($entry.Version)=$($entry.Build)"
    }
}
if ($mismatches.Count) {
    throw "GLOBIO environment differs from $resolvedLock`n  $($mismatches -join "`n  ")"
}

$requiredFiles = @(
    'python.exe',
    'include\Python.h',
    'libs\python39.lib',
    'Library\include\gdal.h',
    'Library\include\proj.h',
    'Library\include\geos\version.h',
    'Library\include\tiff.h',
    'Library\lib\gdal_i.lib',
    'Library\lib\proj.lib',
    'Library\lib\geos.lib',
    'Library\lib\tiff.lib',
    'Library\bin\gdal301.dll',
    'Library\bin\proj_8_0.dll',
    'Library\bin\geos.dll',
    'Library\bin\geos_c.dll',
    'Library\bin\tiff.dll',
    'Library\share\gdal',
    'Library\share\proj'
)
$missingFiles = @($requiredFiles | Where-Object { -not (Test-Path -LiteralPath (Join-Path $resolvedRoot $_)) })
if ($missingFiles.Count) {
    throw "GLOBIO development/runtime files are missing from $resolvedRoot`n  $($missingFiles -join "`n  ")"
}

$version = & (Join-Path $resolvedRoot 'python.exe') -c 'import sys; print(str(sys.version_info.major)+chr(46)+str(sys.version_info.minor))'
if ($LASTEXITCODE -ne 0 -or (@($version) -join '').Trim() -cne '3.9') {
    throw "The GLOBIO interpreter must be CPython 3.9; got '$((@($version) -join '').Trim())'."
}

Write-Host "GLOBIO environment verified: $resolvedRoot"
Write-Host '  CPython 3.9.18; GDAL 3.1.4; PROJ 8.0.1; GEOS 3.9.1'
