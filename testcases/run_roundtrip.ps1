# Round-trip completeness check for the config-source serializer: for every POSITIVE
# testcase config, dump it back to DMS syntax (@dumpconfig), reload the DUMPED .dms, and
# require its item still computes (exit 0). The dumped config's IntegrityChecks re-verify
# the semantics, so a passing reload means the representation captured everything relevant.
#
# Negatives (_neg / defcheck) are skipped: they intentionally fail to load or compute.
#
# A positive case can also state how its dump must read, in one or more lines
# '// EXPECT-DUMP: <fragment>' of its .dms (#1377). Each fragment is literal text that must occur in
# the dumped .dms, compared case-sensitively; a dump that lacks one fails the case as WRONG-DUMP,
# unless the reload fails as well, which is then the verdict. Comments do not survive a dump, so a
# fragment cannot match its own line.
#
# As in run_testcases.ps1, the cases' %LocalDataDir% is <OutDir>\LocalData, so that the storage
# round trips of runs with different OutDirs do not share their files.
#
# A dump is reloaded from <OutDir>, so its %currDir% is <OutDir>, not testcases\ (#1368). Each run
# therefore first copies into <OutDir> every entry of testcases\ that a case may read through
# %currDir%: the data\ folder and the fixture files, which is everything except the configurations,
# the scripts and the output folders. Taking every such entry rather than a list keeps a new fixture
# covered without upkeep; a fixture that has gone from testcases\ is removed from <OutDir> through the
# list of the previous run in <OutDir>\fixtures.txt. The dumps of the previous run are removed as well,
# so that <OutDir> holds no dump of a case that no longer exists.
#
# Each run is held to the limits of case_guard.ps1, as in run_testcases.ps1: -MaxCommitGB of
# commit (default 8) and -TimeoutSec of wall clock (default 300), 0 switching one off. A run that
# reaches one fails its configuration as LIMIT(commit) or LIMIT(time).
#
# Usage: run_roundtrip.ps1 -Exe <path\to\GeoDmsRun.exe> [-OutDir <folder>] [-MaxCommitGB <GB>] [-TimeoutSec <s>]
# Exit code: 0 if every positive round-trips, 1 otherwise.
param(
    [Parameter(Mandatory)][string]$Exe,
    [string]$OutDir,
    [double]$MaxCommitGB = 8,
    [double]$TimeoutSec = 300
)
$here = $PSScriptRoot
if (-not $here) { $here = Split-Path -Parent $MyInvocation.MyCommand.Definition }
. (Join-Path $here 'case_guard.ps1')
if (-not $OutDir) { $OutDir = Join-Path $here '_out_rt' }
$Exe = (Resolve-Path $Exe).Path
New-Item -ItemType Directory -Force $OutDir | Out-Null
$OutDir = (Resolve-Path $OutDir).Path
if ($OutDir.TrimEnd('\') -eq (Resolve-Path $here).Path.TrimEnd('\')) {
    "ERROR: -OutDir is the testcases folder itself; the dumps and the fixture copies need a folder of their own."
    exit 2
}

# The fixtures that a dump reads through %currDir% (see the header): copy them, and remove what the
# previous run copied but testcases\ no longer has.
$fixtures = @(Get-ChildItem $here | Where-Object {
    if ($_.PSIsContainer) { $_.Name -notlike '_out*' } else { $_.Extension -notin '.dms', '.ps1', '.bat' }
})
$fixtureList = Join-Path $OutDir 'fixtures.txt'
$fixtureNames = @($fixtures | ForEach-Object { $_.Name })
if (Test-Path $fixtureList) {
    foreach ($old in Get-Content $fixtureList) {
        if ($old -and ($fixtureNames -notcontains $old)) { Remove-Item (Join-Path $OutDir $old) -Recurse -Force -ErrorAction SilentlyContinue }
    }
}
foreach ($f in $fixtures) {
    $dst = Join-Path $OutDir $f.Name
    if (Test-Path $dst) {
        if ((Get-Item $dst -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
            "ERROR: $dst is a link; remove it by hand, so that this run does not delete what it points to."
            exit 2
        }
        Remove-Item $dst -Recurse -Force
    }
    Copy-Item $f.FullName $dst -Recurse
}
Set-Content $fixtureList $fixtureNames -Encoding ASCII
Remove-Item (Join-Path $OutDir '*.dms')

# GeoDMS takes the environment variable GEODMS_directories_LocalDataDir before the LocalDataDir setting.
$localDataDir = Join-Path $OutDir 'LocalData'
New-Item -ItemType Directory -Force $localDataDir | Out-Null
$prevLocalDataDir = $env:GEODMS_directories_LocalDataDir
$env:GEODMS_directories_LocalDataDir = $localDataDir -replace '\\', '/'

$map = @{}
$mapFile = Join-Path $here 'fnrun_itemmap.txt'
if (Test-Path $mapFile) {
    foreach ($line in Get-Content $mapFile) {
        $p = ($line.Trim()) -split '\s+'
        if ($p.Count -ge 2) { $map[$p[0]] = @($p[1..($p.Count-1)]) } # the rest of the line, as in run_testcases.ps1
    }
}

$results = @()
try {
    foreach ($cfg in Get-ChildItem (Join-Path $here '*.dms') | Sort-Object Name) {
        $name = $cfg.Name
        $stem = [IO.Path]::GetFileNameWithoutExtension($name)
        if (($stem -match '_neg\d*(_|$)') -or ($stem -eq 'fn_test_defcheck')) { continue } # positives only, by the rule of run_testcases.ps1
        # fn_test_prelude deliberately '#include's the prelude that the engine ALSO auto-imports.
        # A dump materializes the included prelude functions at root (include directives are erased
        # at parse time), and on reload they collide with the auto-imported prelude ("SubItem 'sqr'
        # is already defined"). This config therefore cannot round-trip by construction; the function
        # rendering itself is correct. Documented in doc/function_serializer.md.
        if ($stem -eq 'fn_test_prelude') { continue }
        # typed: a one-element array leaves an if-expression as a bare string, which @-splats per character
        [string[]]$itemArgs = if ($map.ContainsKey($name)) { $map[$name] } else { '/checks' }
        $item = $itemArgs -join ' '
        $dump = Join-Path $OutDir "$stem.dms"
        Remove-Item $dump -ErrorAction SilentlyContinue

        # 1. dump the loaded config back to DMS syntax
        $r = Invoke-GuardedRun $Exe @($cfg.FullName, '@dumpconfig', $dump) (Join-Path $OutDir "$stem.dump.out") $MaxCommitGB $TimeoutSec "$stem.dump"
        if ($r.Limit) {
            "LIMIT: $stem, dump: $($r.Note)"
            $results += [pscustomobject]@{ config = $stem; item = $item; verdict = "LIMIT($($r.Limit))" }
            continue
        }
        if (($r.ExitCode -ne 0) -or -not (Test-Path $dump)) {
            $results += [pscustomobject]@{ config = $stem; item = $item; verdict = 'DUMP-FAIL' }
            continue
        }
        # 1b. the dump must contain every '// EXPECT-DUMP:' fragment of the case (see the header);
        # String.Contains compares ordinally, where PowerShell's own operators would fold case
        $expects = @(Select-String -LiteralPath $cfg.FullName -Pattern '^\s*//\s*EXPECT-DUMP:\s*(.+?)\s*$' -CaseSensitive |
            ForEach-Object { $_.Matches[0].Groups[1].Value })
        $wrongDump = $false
        if ($expects.Count) {
            $dumpText = [IO.File]::ReadAllText($dump)
            foreach ($fragment in $expects) {
                if (-not $dumpText.Contains($fragment)) { "WRONG-DUMP: $stem lacks: $fragment"; $wrongDump = $true }
            }
        }
        # 2. reload the DUMPED config and recompute the item (IntegrityChecks re-verify)
        $r = Invoke-GuardedRun $Exe (@("/L$(Join-Path $OutDir "$stem.reload.log")", $dump) + $itemArgs) (Join-Path $OutDir "$stem.reload.out") $MaxCommitGB $TimeoutSec "$stem.reload"
        if ($r.Limit) { "LIMIT: $stem, reload: $($r.Note)" }
        $verdict = if ($r.Limit) { "LIMIT($($r.Limit))" } elseif ($r.ExitCode -ne 0) { 'RELOAD-FAIL' } elseif ($wrongDump) { 'WRONG-DUMP' } else { 'ok' }
        $results += [pscustomobject]@{ config = $stem; item = $item; verdict = $verdict }
    }
}
finally {
    $env:GEODMS_directories_LocalDataDir = $prevLocalDataDir # leave the caller's session as it was
}
$results | Format-Table -AutoSize | Out-String -Width 120
$bad = @($results | Where-Object { $_.verdict -ne 'ok' }) # @(): one bad case has no .Count in PowerShell 5.1 otherwise
Get-GuardSummary $MaxCommitGB $TimeoutSec
"ROUNDTRIP TOTAL=$($results.Count) BAD=$($bad.Count)"
if ($bad.Count) {
    "FAILURES (inspect $OutDir\<name>.dms and .reload.out):"
    $bad | Format-Table -AutoSize | Out-String -Width 120
    exit 1
}
"ALL POSITIVES ROUND-TRIP"
exit 0
