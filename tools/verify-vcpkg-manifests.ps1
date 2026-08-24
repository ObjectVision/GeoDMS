[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$regular = Get-Content -LiteralPath (Join-Path $repoRoot 'vcpkg.json') -Raw | ConvertFrom-Json
$globio = Get-Content -LiteralPath (Join-Path $repoRoot 'vcpkg-globio\vcpkg.json') -Raw | ConvertFrom-Json
$regularConfiguration = Get-Content -LiteralPath (Join-Path $repoRoot 'vcpkg-configuration.json') -Raw | ConvertFrom-Json
$globioConfiguration = Get-Content -LiteralPath (Join-Path $repoRoot 'vcpkg-globio\vcpkg-configuration.json') -Raw | ConvertFrom-Json

function Get-DependencyMap($manifest) {
    $map = @{}
    foreach ($dependency in $manifest.dependencies) {
        $name = if ($dependency -is [string]) { $dependency } else { $dependency.name }
        $map[$name] = $dependency | ConvertTo-Json -Compress -Depth 10
    }
    return $map
}

$regularMap = Get-DependencyMap $regular
$globioMap = Get-DependencyMap $globio
$spatial = @('gdal', 'geos', 'sqlite3')
$expectedGlobioNames = @($regularMap.Keys | Where-Object { $_ -notin $spatial } | Sort-Object)
$actualGlobioNames = @($globioMap.Keys | Sort-Object)
$nameDiff = Compare-Object $expectedGlobioNames $actualGlobioNames
if ($nameDiff) {
    throw "vcpkg-globio/vcpkg.json must equal the regular dependency list minus $($spatial -join ', '). Differences: $($nameDiff | Out-String)"
}
foreach ($name in $expectedGlobioNames) {
    if ($regularMap[$name] -cne $globioMap[$name]) {
        throw "Dependency '$name' differs between vcpkg.json and vcpkg-globio/vcpkg.json."
    }
}

foreach ($property in 'default-registry', 'registries') {
    $regularValue = $regularConfiguration.$property | ConvertTo-Json -Compress -Depth 10
    $globioValue = $globioConfiguration.$property | ConvertTo-Json -Compress -Depth 10
    if ($regularValue -cne $globioValue) {
        throw "vcpkg configuration '$property' differs between the regular and G manifests."
    }
}
if (@($regularConfiguration.'overlay-triplets') -join ';' -cne 'vcpkg-triplets' -or
    @($globioConfiguration.'overlay-triplets') -join ';' -cne '../vcpkg-triplets') {
    throw 'The regular and G configurations must resolve the shared vcpkg-triplets directory from their respective manifest roots.'
}

Write-Host 'vcpkg manifests verified: common dependencies and registry configuration match; the G spatial stack is conda-owned.'
