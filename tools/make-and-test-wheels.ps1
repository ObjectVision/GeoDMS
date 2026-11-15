# Builds and tests the PyPI wheels of the geodms module (#1355); batch\BuildSignAndCreateSetup.bat
# runs it on bin\Release\x64 before NSIS.
#
# 1. Makes a venv with the packages of python\wheel-tools.txt, kept in
#    %LOCALAPPDATA%\ObjectVision\GeoDMS\wheel-tools and made again when that file changes. Making it
#    downloads the packages from PyPI; a venv that is up to date needs no network.
# 2. First removes the wheels of the same version that an earlier run left in -Dist, then has
#    tools\make-wheels.py write one wheel per Python version listed in python\PythonVersions.txt
#    from -OutputDir into -Dist.
# 3. twine check --strict on those wheels: the metadata checks that PyPI applies to an upload.
# 4. Each wheel is installed in a fresh venv of its own CPython, found as tools\test-python-bindings.ps1
#    finds it (PYTHON<abi>_ROOT, else the py launcher), and python\tst\UnitTests.py --installed must
#    pass there with no GeoDMS folder on PATH, so that only the wheel can supply the engine.
#
# Stops with exit code 1 at the first failure.
param(
    [Parameter(Mandatory = $true)]
    [string]$OutputDir,

    [Parameter(Mandatory = $true)]
    [string]$Dist,

    [Parameter(Mandatory = $true)]
    [string]$Version
)

$ErrorActionPreference = 'Stop'

# The CPython that runs delvewheel, wheel and twine; the versions in python\wheel-tools.txt were
# tested under it.
$toolPythonVersion = '3.13'

$repoRoot = Split-Path -Parent $PSScriptRoot
$toolsFile = Join-Path $repoRoot 'python\wheel-tools.txt'
$makeWheels = Join-Path $repoRoot 'tools\make-wheels.py'
$testScript = Join-Path $repoRoot 'python\tst\UnitTests.py'
$toolVenv = Join-Path $env:LOCALAPPDATA 'ObjectVision\GeoDMS\wheel-tools'
$started = Get-Date

# Returns the command that runs CPython <minor>: the exe, or the py launcher and its -<minor> switch.
# PowerShell unrolls a returned list of one element into a string, so callers wrap it in @().
function Find-Python([string]$minor) {
    $abi = $minor.Replace('.', '')
    $root = [Environment]::GetEnvironmentVariable("PYTHON${abi}_ROOT")
    if (-not $root) {
        $includeDir = [Environment]::GetEnvironmentVariable("PYTHON${abi}_INCLUDE_DIR")
        if ($includeDir) { $root = Split-Path -Parent $includeDir.TrimEnd('\', '/') }
    }
    $exe = if ($root) { Join-Path $root 'python.exe' } else { $null }
    if ($exe -and (Test-Path -LiteralPath $exe -PathType Leaf)) { return @($exe) }
    $launcher = Get-Command py.exe -ErrorAction SilentlyContinue
    if (-not $launcher) { throw "CPython $minor not found. Set PYTHON${abi}_ROOT to a complete CPython $minor installation." }
    return @($launcher.Source, "-$minor")
}

function Invoke-Checked([string]$what, [string[]]$command) {
    $exe = $command[0]
    $arguments = @($command | Select-Object -Skip 1)
    Write-Host "> $($command -join ' ')"
    & $exe @arguments
    if ($LASTEXITCODE -ne 0) { throw "$what failed (exit $LASTEXITCODE)" }
}

$testRoot = Join-Path ([IO.Path]::GetTempPath()) "geodms-wheel-test-$PID"
try {
    $resolvedOutput = (Resolve-Path -LiteralPath $OutputDir).Path
    New-Item -ItemType Directory -Force -Path $Dist | Out-Null
    $resolvedDist = (Resolve-Path -LiteralPath $Dist).Path
    $minors = @((Get-Content -LiteralPath (Join-Path $repoRoot 'python\PythonVersions.txt') -Raw).Trim().Split(';') |
        ForEach-Object { $_.Trim() } | Where-Object { $_ })

    # 1. the tool venv
    $toolPython = Join-Path $toolVenv 'Scripts\python.exe'
    $installedTools = Join-Path $toolVenv 'wheel-tools.txt'
    $upToDate = (Test-Path -LiteralPath $toolPython) -and (Test-Path -LiteralPath $installedTools) -and
        ((Get-Content -LiteralPath $installedTools -Raw) -ceq (Get-Content -LiteralPath $toolsFile -Raw))
    if (-not $upToDate) {
        if (Test-Path -LiteralPath $toolVenv) { Remove-Item -Recurse -Force -LiteralPath $toolVenv }
        Invoke-Checked "creating the tool venv" (@(Find-Python $toolPythonVersion) + @('-m', 'venv', $toolVenv))
        Invoke-Checked "installing python\wheel-tools.txt" @($toolPython, '-m', 'pip', 'install', '--quiet', '-r', $toolsFile)
        Copy-Item -LiteralPath $toolsFile -Destination $installedTools
    }

    # 2. the wheels
    Get-ChildItem -LiteralPath $resolvedDist -Filter "geodms-$Version-*.whl" | Remove-Item -Force
    Invoke-Checked "make-wheels.py" @($toolPython, $makeWheels, '--bin', $resolvedOutput, '--out', $resolvedDist, '--version', $Version)
    $wheels = @()
    foreach ($minor in $minors) {
        $abi = 'cp' + $minor.Replace('.', '')
        $wheel = Join-Path $resolvedDist "geodms-$Version-$abi-$abi-win_amd64.whl"
        if (-not (Test-Path -LiteralPath $wheel)) { throw "make-wheels.py wrote no $wheel" }
        $wheels += $wheel
    }

    # 3. the metadata
    Invoke-Checked "twine check" (@($toolPython, '-m', 'twine', 'check', '--strict') + $wheels)

    # 4. each wheel in a fresh venv, with no GeoDMS folder on PATH
    $env:PATH = ($env:PATH -split ';' | Where-Object { $_ -and $_ -notmatch 'ObjectVision|GeoDMS' }) -join ';'
    Remove-Item Env:GEODMS_PYDIR -ErrorAction SilentlyContinue
    for ($i = 0; $i -lt $minors.Count; $i++) {
        $venv = Join-Path $testRoot ('venv' + $minors[$i].Replace('.', ''))
        Invoke-Checked "creating a venv for CPython $($minors[$i])" (@(Find-Python $minors[$i]) + @('-m', 'venv', $venv))
        $venvPython = Join-Path $venv 'Scripts\python.exe'
        Invoke-Checked "installing $($wheels[$i])" @($venvPython, '-m', 'pip', 'install', '--quiet', '--no-deps', '--no-index', $wheels[$i])
        Invoke-Checked "UnitTests.py --installed under CPython $($minors[$i])" @($venvPython, '-I', '-u', $testScript, '--installed')
    }

    $seconds = [int]((Get-Date) - $started).TotalSeconds
    Write-Host "=== wheels of $Version built and tested in $seconds s: $($wheels -join ', ') ==="
    exit 0
}
catch {
    Write-Host "*** WHEELS FAILED: $($_.Exception.Message) ***"
    exit 1
}
finally {
    if (Test-Path -LiteralPath $testRoot) { Remove-Item -Recurse -Force -LiteralPath $testRoot -ErrorAction SilentlyContinue }
}
