param(
    [Parameter(Mandatory = $true)]
    [string]$OutputDir,

    # Primarily for a focused verifier self-test. Release scripts omit this and
    # therefore always verify the complete PythonVersions.txt matrix.
    [string[]]$ExpectedVersions,

    [string]$VersionsFile
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$versionFile = if ($VersionsFile) { $VersionsFile } else { Join-Path $repoRoot 'python\PythonVersions.txt' }
if (-not $ExpectedVersions) {
    $ExpectedVersions = @((Get-Content -LiteralPath $versionFile -Raw).Trim().Split(';'))
}
$ExpectedVersions = @($ExpectedVersions | ForEach-Object { $_.Trim() } | Where-Object { $_ } | Select-Object -Unique)
if (-not $ExpectedVersions.Count -or @($ExpectedVersions | Where-Object { $_ -notmatch '^3\.[0-9]+$' }).Count) {
    throw "$versionFile must contain semicolon-separated CPython minors such as 3.9;3.12;3.13;3.14"
}

$resolvedOutput = (Resolve-Path -LiteralPath $OutputDir).Path
$expectedNames = @($ExpectedVersions | ForEach-Object {
    "geodms.cp$($_.Replace('.', ''))-win_amd64.pyd"
} | Sort-Object)

$bindings = @(Get-ChildItem -LiteralPath $resolvedOutput -Filter 'geodms*.pyd' -File | Sort-Object Name)
$foundNames = @($bindings.Name)
if (Compare-Object -ReferenceObject $expectedNames -DifferenceObject $foundNames -CaseSensitive) {
    $expected = $expectedNames -join ', '
    $found = if ($foundNames.Count) { ($foundNames -join ', ') } else { '<none>' }
    throw "Expected only $expected in $resolvedOutput; found: $found"
}

# PE import names are stored as ASCII strings. Reading those strings directly
# keeps this check independent of a particular Visual Studio/dumpbin location.
foreach ($pythonVersion in $ExpectedVersions) {
    $pythonAbi = $pythonVersion.Replace('.', '')
    $expectedName = "geodms.cp$pythonAbi-win_amd64.pyd"
    $expectedPath = Join-Path $resolvedOutput $expectedName
    $imageText = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($expectedPath))
    $pythonImports = @(
        [regex]::Matches($imageText, '(?i)python[0-9]{1,3}(?:_d)?\.dll') |
            ForEach-Object { $_.Value.ToLowerInvariant() } |
            Sort-Object -Unique
    )
    $expectedImport = "python$pythonAbi.dll"
    if ($pythonImports.Count -ne 1 -or $pythonImports[0] -cne $expectedImport) {
        $found = if ($pythonImports.Count) { ($pythonImports -join ', ') } else { '<none>' }
        throw "$expectedName must import only $expectedImport; found: $found"
    }
    Write-Host "Python binding verified: $expectedName -> $expectedImport"
}

# Policy for #1105: the binding is an extension for an already-running matching
# CPython, not an embedded interpreter. Shipping an unrelated vcpkg Python DLL
# beside it is both unnecessary and actively misleading.
$runtimeDlls = @(Get-ChildItem -LiteralPath $resolvedOutput -Filter 'python*.dll' -File)
if ($runtimeDlls.Count) {
    throw "Do not deploy Python runtimes beside the GeoDMS bindings; found: $($runtimeDlls.Name -join ', ')"
}

Write-Host "Python ABI matrix verified; runtimes are supplied by their matching CPython interpreters"
