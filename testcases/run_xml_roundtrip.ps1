# XML round-trip battery (#1261): does a configuration survive being written as XML and read back?
#
# Not to be confused with run_roundtrip.ps1 beside it, which asks a different question of the DMS
# writer: dump a configuration, RELOAD the dump and recompute its item. This one never reloads a
# .dms; it compares two DMS dumps of what should be the same tree, one of them taken after a
# detour through the XML notation.
#
# For every testcases\*.dms that loads, three GeoDmsRun invocations:
#     1. @dumpconfig <stem>.direct.dms   the configuration as the DMS writer renders it
#     2. @dumpconfig <stem>.xml          the same configuration in the XML notation
#     3. @dumpconfig <stem>.viaxml.dms   the tree READ BACK from that .xml, rendered again
# and then 1 against 3. They must be identical: both are the same writer over what should be
# the same tree, so any difference is something the XML notation lost on the way out or on the
# way back in. That is how the four defects of #1261 were found (a function came back as a
# template, a value array was dropped, a multi-line rule was cut at its first line, and every
# run of white space in element text was collapsed).
#
# Configurations whose round trip is known to differ are listed in xml_roundtrip_known_diff.txt with
# the reason. A listed configuration that suddenly MATCHES fails the run as well: the list is
# meant to shrink deliberately, not to rot.
#
# '_neg' configurations are skipped: they deliberately fail to load, so their dump is of a
# half-built tree and says nothing about the notation.
#
# Cost: three runs per configuration, so about twice the plain testcases battery, and offline.
#
# Usage: run_xml_roundtrip.ps1 -Exe <path\to\GeoDmsRun.exe> [-OutDir <logfolder>]
# Exit code: 0 if every configuration matched or is listed with the same verdict, 1 otherwise.
param(
    [Parameter(Mandatory)][string]$Exe,
    [string]$OutDir
)
$here = $PSScriptRoot
if (-not $here) { $here = Split-Path -Parent $MyInvocation.MyCommand.Definition }
if (-not $OutDir) { $OutDir = Join-Path $here '_out_xml_roundtrip' }
$Exe = (Resolve-Path $Exe).Path
try { New-Item -ItemType Directory -Force $OutDir -ErrorAction Stop | Out-Null }
catch {
    # installed under Program Files: the suite dir is not writable - work in TEMP
    $OutDir = Join-Path $env:TEMP 'GeoDmsXmlRoundtrip_out'
    New-Item -ItemType Directory -Force $OutDir | Out-Null
    "NOTE: output goes to $OutDir (suite folder not writable)"
}
$OutDir = (Resolve-Path $OutDir).Path

$known = @{}
$knownFile = Join-Path $here 'xml_roundtrip_known_diff.txt'
if (Test-Path $knownFile) {
    foreach ($line in Get-Content $knownFile) {
        $t = $line.Trim()
        if (-not $t -or $t.StartsWith('#')) { continue }
        $p = $t -split '\s+', 2
        $known[$p[0]] = if ($p.Count -gt 1) { $p[1] } else { '(no reason given)' }
    }
}

$results = @()
foreach ($cfg in Get-ChildItem (Join-Path $here '*.dms') | Sort-Object Name) {
    $stem = [IO.Path]::GetFileNameWithoutExtension($cfg.Name)
    if ($stem -match '_neg') { continue }
    $a = Join-Path $OutDir "$stem.direct.dms"
    $x = Join-Path $OutDir "$stem.xml"
    $b = Join-Path $OutDir "$stem.viaxml.dms"
    foreach ($f in $a, $x, $b) { if (Test-Path $f) { Remove-Item -LiteralPath $f } }

    & $Exe $cfg.FullName '@dumpconfig' $a *> (Join-Path $OutDir "$stem.1.out"); $c1 = $LASTEXITCODE
    & $Exe $cfg.FullName '@dumpconfig' $x *> (Join-Path $OutDir "$stem.2.out"); $c2 = $LASTEXITCODE
    if ($c1 -ne 0 -or $c2 -ne 0 -or -not (Test-Path $a) -or -not (Test-Path $x)) {
        $results += [pscustomobject]@{ config = $stem; verdict = 'DUMPFAIL'; delta = "$c1/$c2" }
        continue
    }
    & $Exe $x '@dumpconfig' $b *> (Join-Path $OutDir "$stem.3.out"); $c3 = $LASTEXITCODE
    if ($c3 -ne 0 -or -not (Test-Path $b)) {
        $results += [pscustomobject]@{ config = $stem; verdict = 'READFAIL'; delta = "$c3" }
        continue
    }
    $d = @(Compare-Object (Get-Content $a) (Get-Content $b))
    $isKnown = $known.ContainsKey($stem)
    $verdict = if ($d.Count -eq 0 -and -not $isKnown) { 'ok' }
               elseif ($d.Count -ne 0 -and $isKnown)  { 'ok(known)' }
               elseif ($d.Count -eq 0 -and $isKnown)  { 'NOW-MATCHES' }
               else                                   { 'DIFFERS' }
    $results += [pscustomobject]@{ config = $stem; verdict = $verdict; delta = $d.Count }
}
$results | Where-Object { $_.verdict -ne 'ok' } | Format-Table -AutoSize | Out-String -Width 120
$bad = @($results | Where-Object { $_.verdict -in 'DIFFERS','NOW-MATCHES','READFAIL','DUMPFAIL' })
"TOTAL=$($results.Count) OK=$(@($results | Where-Object verdict -eq 'ok').Count) KNOWN=$(@($results | Where-Object verdict -eq 'ok(known)').Count) BAD=$($bad.Count)"
if ($bad.Count) {
    "FAILED CASES:"
    $bad | Format-Table -AutoSize | Out-String -Width 120
    "A DIFFERS is a fidelity loss: compare <config>.direct.dms with <config>.viaxml.dms in $OutDir."
    "A NOW-MATCHES means xml_roundtrip_known_diff.txt lists a configuration that no longer differs; remove its line."
    exit 1
}
"ALL ROUNDTRIPS AS EXPECTED"
exit 0
