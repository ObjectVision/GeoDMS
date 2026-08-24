param(
    [Parameter(Mandatory = $true)]
    [string]$OutputDir
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$versionFile = Join-Path $repoRoot 'python\PythonVersions.txt'
$versions = @((Get-Content -LiteralPath $versionFile -Raw).Trim().Split(';') |
    ForEach-Object { $_.Trim() } |
    Where-Object { $_ } |
    Select-Object -Unique)
if (-not $versions.Count -or @($versions | Where-Object { $_ -notmatch '^3\.[0-9]+$' }).Count) {
    throw "$versionFile must contain semicolon-separated CPython minors such as 3.9;3.12;3.13;3.14"
}
$resolvedOutput = (Resolve-Path -LiteralPath $OutputDir).Path
$testScript = Join-Path $repoRoot 'python\tst\UnitTests.py'
$previousPyDir = [Environment]::GetEnvironmentVariable('GEODMS_PYDIR', 'Process')

try {
    $env:GEODMS_PYDIR = $resolvedOutput
    foreach ($version in $versions) {
        $abi = $version.Replace('.', '')
        $root = [Environment]::GetEnvironmentVariable("PYTHON${abi}_ROOT")
        if (-not $root) {
            $includeDir = [Environment]::GetEnvironmentVariable("PYTHON${abi}_INCLUDE_DIR")
            if ($includeDir) {
                $root = Split-Path -Parent $includeDir.TrimEnd('\', '/')
            }
        }

        $pythonExe = if ($root) { Join-Path $root 'python.exe' } else { $null }
        if ($pythonExe -and (Test-Path -LiteralPath $pythonExe -PathType Leaf)) {
            $pythonCommand = $pythonExe
            $pythonArguments = @()
        }
        else {
            $pythonLauncher = Get-Command py.exe -ErrorAction SilentlyContinue
            if (-not $pythonLauncher) {
                throw "CPython $version executable not found. Set PYTHON${abi}_ROOT to a complete CPython $version installation."
            }
            $pythonCommand = $pythonLauncher.Source
            $pythonArguments = @("-$version")
        }

        $versionOutput = & $pythonCommand @pythonArguments -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")'
        $versionExitCode = $LASTEXITCODE
        $actualVersion = (@($versionOutput) -join '').Trim()
        if ($versionExitCode -ne 0 -or $actualVersion -cne $version) {
            throw "Requested CPython $version but $pythonCommand $pythonArguments resolved to '$actualVersion'"
        }

        Write-Host "Testing $resolvedOutput with CPython $version ($pythonCommand $pythonArguments)"
        & $pythonCommand @pythonArguments -u $testScript
        if ($LASTEXITCODE -ne 0) {
            throw "GeoDMS Python binding test failed under CPython $version (exit $LASTEXITCODE)"
        }
    }
}
finally {
    if ($null -eq $previousPyDir) {
        Remove-Item Env:GEODMS_PYDIR -ErrorAction SilentlyContinue
    }
    else {
        $env:GEODMS_PYDIR = $previousPyDir
    }
}

Write-Host "Python binding import matrix passed: $($versions -join ', ')"
