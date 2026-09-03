# Run the typed higher-order-function testcase suite through GeoDmsRun and classify
# each case as pass/fail.
#
#   Positives (no '_neg' in the name)     must exit 0.
#   Negatives ('_neg' in the name, or the
#   'defcheck' definition-check case)      must exit nonzero (they assert a rejection).
#   Exit code 3 (a Debug assertion)        is ALWAYS a failure.
#
# Per-config item overrides live in fnrun_itemmap.txt (one 'file.dms /item/path' per
# line); the default item is /checks (each positive config carries a /checks container
# whose IntegrityChecks fail the run if a computed value is wrong).
#
# Configs run SORTED BY FILE NAME, one GeoDmsRun invocation each. Cases are otherwise
# independent, but a round trip over a storage cannot be: the reading domain unit takes
# its size from the file while its meta info is determined, which is before any supplier
# of it can be updated, so the write cannot be ordered before the read inside one config.
# Such a pair is split over two configs whose names order them -- see
# stor_shp_ringclose_1_write.dms / stor_shp_ringclose_2_read.dms. Do not rename one half
# without the other.
#
# Usage: run_testcases.ps1 -Exe <path\to\GeoDmsRun.exe> [-OutDir <logfolder>]
# Exit code: 0 if every case matched its expected outcome, 1 otherwise.
param(
    [Parameter(Mandatory)][string]$Exe,
    [string]$OutDir
)
$here = $PSScriptRoot
if (-not $here) { $here = Split-Path -Parent $MyInvocation.MyCommand.Definition }
if (-not $OutDir) { $OutDir = Join-Path $here '_out' }
$Exe = (Resolve-Path $Exe).Path
try { New-Item -ItemType Directory -Force $OutDir -ErrorAction Stop | Out-Null }
catch {
    # installed under Program Files: the suite dir is not writable - log to TEMP
    $OutDir = Join-Path $env:TEMP 'GeoDmsTestcases_out'
    New-Item -ItemType Directory -Force $OutDir | Out-Null
    "NOTE: logs go to $OutDir (suite folder not writable)"
}
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
    $item = if ($map.ContainsKey($name)) { $map[$name] } else { '/checks' }
    $log  = Join-Path $OutDir "$stem.log"
    & $Exe "/L$log" $cfg.FullName $item *> (Join-Path $OutDir "$stem.out")
    $code = $LASTEXITCODE
    $isNeg = ($stem -match '_neg') -or ($stem -eq 'fn_test_defcheck')
    $verdict = if ($code -eq 3) { 'ASSERT' }
               elseif ($isNeg -and $code -ne 0) { 'ok(neg)' }
               elseif (-not $isNeg -and $code -eq 0) { 'ok' }
               else { 'UNEXPECTED' }
    $results += [pscustomobject]@{ config = $stem; item = $item; exit = $code; verdict = $verdict }
}
$results | Format-Table -AutoSize | Out-String -Width 120
$bad = $results | Where-Object { $_.verdict -in 'ASSERT','UNEXPECTED' }
"TOTAL=$($results.Count) BAD=$($bad.Count)"
if ($bad) {
    "FAILED CASES:"
    $bad | Format-Table -AutoSize | Out-String -Width 120
    exit 1
}
"ALL TESTCASES PASSED"
exit 0
