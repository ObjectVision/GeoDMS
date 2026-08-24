[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$OutputDir,

    [string]$GlobioRoot = $env:GLOBIO_ENV_ROOT,

    [string]$CopiedFilesLog
)

$ErrorActionPreference = 'Stop'

if (-not $GlobioRoot) {
    throw 'GLOBIO_ENV_ROOT is not set.'
}
$resolvedOutput = (Resolve-Path -LiteralPath $OutputDir).Path
$sourceBin = Join-Path (Resolve-Path -LiteralPath $GlobioRoot).Path 'Library\bin'

$dumpbin = Get-Command dumpbin.exe -ErrorAction SilentlyContinue
if (-not $dumpbin) {
    $vsRoot = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC'
    $dumpbin = Get-ChildItem -LiteralPath $vsRoot -Filter dumpbin.exe -File -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '\\bin\\Hostx64\\x64\\dumpbin\.exe$' } |
        Sort-Object { [version]$_.Directory.Parent.Parent.Parent.Name } -Descending |
        Select-Object -First 1
}
if (-not $dumpbin) {
    throw 'VS18 x64 dumpbin.exe was not found; cannot resolve the GLOBIO DLL closure.'
}
$dumpbinPath = if ($dumpbin.Source) { $dumpbin.Source } else { $dumpbin.FullName }

# GeoDMS is built with the current v145 runtime. Never replace it with the older
# runtime carried by the conda prefix, even when a conda DLL imports the same ABI.
$neverCopy = '^(?:api-ms-|ucrtbase\.dll$|msvcp140.*\.dll$|vcruntime140.*\.dll$|concrt140\.dll$|vccorlib140\.dll$|vcomp140\.dll$|vcamp140\.dll$)'
$seen = @{}
$queue = [Collections.Generic.Queue[string]]::new()
Get-ChildItem -LiteralPath $resolvedOutput -File |
    Where-Object { $_.Extension -in '.exe', '.dll', '.pyd' } |
    ForEach-Object { $queue.Enqueue($_.FullName) }

$copied = [Collections.Generic.List[string]]::new()
$deployed = [Collections.Generic.List[string]]::new()
while ($queue.Count) {
    $binary = $queue.Dequeue()
    $key = [IO.Path]::GetFullPath($binary).ToLowerInvariant()
    if ($seen.ContainsKey($key)) { continue }
    $seen[$key] = $true

    $dependents = & $dumpbinPath /nologo /dependents $binary 2>$null |
        Where-Object { $_ -match '^    ([^ ].*\.dll)\s*$' } |
        ForEach-Object { $Matches[1] }
    foreach ($dependency in $dependents) {
        if ($dependency -match $neverCopy) { continue }
        $source = Join-Path $sourceBin $dependency
        $destination = Join-Path $resolvedOutput $dependency
        if (Test-Path -LiteralPath $source -PathType Leaf) {
            $mustCopy = -not (Test-Path -LiteralPath $destination -PathType Leaf)
            if (-not $mustCopy) {
                $sourceHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
                $destinationHash = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
                $mustCopy = $sourceHash -cne $destinationHash
            }
            if ($mustCopy) {
                Copy-Item -LiteralPath $source -Destination $destination -Force
                $copied.Add($destination)
                Write-Verbose "GLOBIO runtime: $dependency"
            }
            $deployed.Add($destination)
            $queue.Enqueue($destination)
        }
        elseif (Test-Path -LiteralPath $destination -PathType Leaf) {
            $queue.Enqueue($destination)
        }
    }
}

if (-not (Test-Path -LiteralPath (Join-Path $resolvedOutput 'gdal301.dll'))) {
    throw "The deployed closure in $resolvedOutput does not contain gdal301.dll."
}
if (Test-Path -LiteralPath (Join-Path $resolvedOutput 'gdal.dll')) {
    throw "The G output contains the regular-build gdal.dll; its dependency trees have been mixed."
}

if ($CopiedFilesLog) {
    # Record the complete deployed set, including unchanged files. MSBuild uses
    # this as FileWrites input so IncrementalClean does not treat an unchanged
    # conda DLL as an orphan on the next build.
    $deployed | Sort-Object -Unique | Set-Content -LiteralPath $CopiedFilesLog -Encoding UTF8
}
Write-Host "GLOBIO runtime closure deployed to $resolvedOutput ($($copied.Count) file copies/updates)."
