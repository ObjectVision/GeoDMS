# Round-trip completeness check for the config-source serializer: for every POSITIVE
# testcase config, dump it back to DMS syntax (@dumpconfig), reload the DUMPED .dms, and
# require its item still computes (exit 0). The dumped config's IntegrityChecks re-verify
# the semantics, so a passing reload means the representation captured everything relevant.
#
# Negatives (_neg / defcheck) are skipped: they intentionally fail to load or compute.
#
# Usage: run_roundtrip.ps1 -Exe <path\to\GeoDmsRun.exe> [-OutDir <folder>]
# Exit code: 0 if every positive round-trips, 1 otherwise.
param(
    [Parameter(Mandatory)][string]$Exe,
    [string]$OutDir
)
$here = $PSScriptRoot
if (-not $here) { $here = Split-Path -Parent $MyInvocation.MyCommand.Definition }
if (-not $OutDir) { $OutDir = Join-Path $here '_out_rt' }
$Exe = (Resolve-Path $Exe).Path
New-Item -ItemType Directory -Force $OutDir | Out-Null
$OutDir = (Resolve-Path $OutDir).Path

$map = @{}
$mapFile = Join-Path $here 'fnrun_itemmap.txt'
if (Test-Path $mapFile) {
    foreach ($line in Get-Content $mapFile) {
        $p = ($line.Trim()) -split '\s+'
        if ($p.Count -eq 2) { $map[$p[0]] = $p[1] }
    }
}

$results = @()
foreach ($cfg in Get-ChildItem (Join-Path $here '*.dms') | Sort-Object Name) {
    $name = $cfg.Name
    $stem = [IO.Path]::GetFileNameWithoutExtension($name)
    if (($stem -match '_neg') -or ($stem -eq 'fn_test_defcheck')) { continue } # positives only
    # fn_test_prelude deliberately '#include's the prelude that the engine ALSO auto-imports.
    # A dump materializes the included prelude functions at root (include directives are erased
    # at parse time), and on reload they collide with the auto-imported prelude ("SubItem 'sqr'
    # is already defined"). This config therefore cannot round-trip by construction; the function
    # rendering itself is correct. Documented in doc/function_serializer.md.
    if ($stem -eq 'fn_test_prelude') { continue }
    $item = if ($map.ContainsKey($name)) { $map[$name] } else { '/checks' }
    $dump = Join-Path $OutDir "$stem.dms"
    Remove-Item $dump -ErrorAction SilentlyContinue

    # 1. dump the loaded config back to DMS syntax
    & $Exe $cfg.FullName '@dumpconfig' $dump *> (Join-Path $OutDir "$stem.dump.out")
    if (($LASTEXITCODE -ne 0) -or -not (Test-Path $dump)) {
        $results += [pscustomobject]@{ config = $stem; item = $item; verdict = 'DUMP-FAIL' }
        continue
    }
    # 2. reload the DUMPED config and recompute the item (IntegrityChecks re-verify)
    & $Exe "/L$(Join-Path $OutDir "$stem.reload.log")" $dump $item *> (Join-Path $OutDir "$stem.reload.out")
    $verdict = if ($LASTEXITCODE -eq 0) { 'ok' } else { 'RELOAD-FAIL' }
    $results += [pscustomobject]@{ config = $stem; item = $item; verdict = $verdict }
}
$results | Format-Table -AutoSize | Out-String -Width 120
$bad = $results | Where-Object { $_.verdict -ne 'ok' }
"ROUNDTRIP TOTAL=$($results.Count) BAD=$($bad.Count)"
if ($bad) {
    "FAILURES (inspect $OutDir\<name>.dms and .reload.out):"
    $bad | Format-Table -AutoSize | Out-String -Width 120
    exit 1
}
"ALL POSITIVES ROUND-TRIP"
exit 0
