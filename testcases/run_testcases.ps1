# Run the typed higher-order-function testcase suite through GeoDmsRun and classify
# each case as pass/fail.
#
#   Positives (no '_neg' part in the name) must exit 0.
#   Negatives ('_neg' as a part of the name,
#   followed by digits, '_' or the end, or
#   the 'defcheck' definition-check case)  must exit nonzero (they assert a rejection),
#                                          with the message they expect (below).
#   So oper_negate_unsigned is a positive, and fn_test_p2_neg1 and oper_modus_range_neg_part_set
#   are negatives.
#   Exit code 3 (a Debug assertion)        is ALWAYS a failure.
#
# A negative case states the message it is written to fail with, in one or more lines of its .dms
# that consist of a comment of the form
#     // EXPECT-FAILURE: <fragment>
# Each fragment must occur, as literal text (not a pattern) and in the same case, in a message of
# the case's output: a line that starts with 'Failure:', 'Last ErrMsg:' or '[E]', or one of the
# lines that continue it, up to the next line that starts with a timestamp ('['), with 'Context:'
# or with 'Update [[', or that starts another message. In the output of a Debug build, where the
# CRT copy of each line comes first, the console's copy behind it is read (ConvertFrom-DebugLine
# below). A fragment cannot span lines, and one line need not hold them all. Take a fragment from
# the first line of the message where it can; use a later line only for what only that line says,
# such as the list of signatures that follows
# 'Cannot find operator for these arguments:', or the item that a storage error names. A
# negative case whose fragments are all found passes as ok(neg); one that fails without one of them
# fails the run as WRONG-FAILURE, as it failed for a reason other than the one it was written for:
# a misspelled item, a syntax error in the configuration, an operator that the build under test
# does not have (#1369).
#
# expect_failure_missing.txt lists the negative cases that have no such line yet, each with the
# reason. A listed case is still judged on its exit code only, ok(neg,unchecked). A negative case
# that has no EXPECT-FAILURE line and is not listed fails the run as NO-EXPECT, so a new negative
# case comes with its expected message; a listed case that has one fails as LISTED-HAS-EXPECT, and
# a listed name that is no negative case as LISTED-UNKNOWN, so that the list only shrinks. An empty
# fragment, or an EXPECT-FAILURE line in a positive case, fails as BAD-EXPECT.
#
# GeoDmsRun reports an error that left one of its own calls of the DMS API through its exception
# translator, reportMsg in run/exe/src/MainRun.cpp, which writes it to stderr behind 'Caught at Main:'
# and not to the /L log of the case. A case whose output has such a line fails as CAUGHT-AT-MAIN,
# a positive as well as a negative, when it would otherwise pass. Until #1384 that was how the Linux
# engine reported an error caught inside a nested call, so this keeps that fixed; no case of the
# battery writes such a line on either platform.
#
# Per-config item overrides live in fnrun_itemmap.txt (one 'file.dms /item/path' per
# line); the default item is /checks (each positive config carries a /checks container
# whose IntegrityChecks fail the run if a computed value is wrong). Everything after the
# file name is passed to GeoDmsRun as it stands, so a line can name a command as well:
# 'file.dms @statistics /item' (#1270 asserts the exit code of such a request).
#
# Configs run SORTED BY FILE NAME, one GeoDmsRun invocation each. Cases are otherwise
# independent, but a round trip over a storage cannot be: the reading domain unit takes
# its size from the file while its meta info is determined, which is before any supplier
# of it can be updated, so the write cannot be ordered before the read inside one config.
# Such a pair is split over two configs whose names order them -- see
# stor_shp_ringclose_1_write.dms / stor_shp_ringclose_2_read.dms. Do not rename one half
# without the other.
#
# The cases' %LocalDataDir%, where those round trips keep their files (GeoDmsTestcases), is
# <OutDir>\LocalData, so that batteries run with different OutDirs at the same time neither read
# nor overwrite each other's files.
#
# Each case runs under the limits of case_guard.ps1: at most -MaxCommitGB of commit (default 8)
# and -TimeoutSec of wall clock (default 300); 0 switches a limit off. A case that reaches one is
# a failure, LIMIT(commit) or LIMIT(time), whatever its exit code and whether or not it is a
# negative: a runaway on an older exe (bisecting, a kept build) then ends at the limit instead of
# taking the machine's memory. A case's .out file holds its stdout and stderr as GeoDmsRun wrote
# them, plus a closing line naming the limit when one stopped it.
#
# Usage: run_testcases.ps1 -Exe <path\to\GeoDmsRun.exe> [-OutDir <logfolder>] [-MaxCommitGB <GB>] [-TimeoutSec <s>]
# Exit code: 0 if every case matched its expected outcome, 1 otherwise.
param(
    [Parameter(Mandatory)][string]$Exe,
    [string]$OutDir,
    [double]$MaxCommitGB = 8,
    [double]$TimeoutSec = 300
)
$here = $PSScriptRoot
if (-not $here) { $here = Split-Path -Parent $MyInvocation.MyCommand.Definition }
. (Join-Path $here 'case_guard.ps1')
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
        if ($p.Count -ge 2) { $map[$p[0]] = @($p[1..($p.Count-1)]) }
    }
}

# The negative cases without an EXPECT-FAILURE line, with the reason.
$uncheckedName = 'expect_failure_missing.txt'
$unchecked = @{}
$uncheckedFile = Join-Path $here $uncheckedName
if (Test-Path $uncheckedFile) {
    foreach ($line in Get-Content $uncheckedFile) {
        $t = $line.Trim()
        if (-not $t -or $t.StartsWith('#')) { continue }
        $p = $t -split '\s+', 2
        $unchecked[$p[0]] = if ($p.Count -gt 1) { $p[1] } else { '(no reason given)' }
    }
}
$expectPattern = '^\s*//\s*EXPECT-FAILURE:(.*)$'        # a line of a case's .dms, in this case
$errorPattern  = '^(Failure:|Last ErrMsg:|\[E\])'       # a line of its output that starts a message
$msgEndPattern = '^(\[|\s*Context:|Update \[\[)'         # a line that ends the lines continuing a message
$caughtMarker  = 'Caught at Main:'                       # what GeoDmsRun's exception translator writes before an error
$utf8 = New-Object System.Text.UTF8Encoding $false

# A Debug build also writes each line of a message to the CRT output, and in the .out that copy comes
# first, on the same line as the console's copy: '[other]Error: <text>[E][<time>][1][other]<text>' for
# the first line of a message, 'Error: <text>   <text>' for a line that continues one. The console's
# copy is the line that a Release build writes, so that is the line the patterns above are applied to.
$consolePrefixPattern = '\[[^\]]\]\[\d{4}-\d\d-\d\d \d\d:\d\d:\d\d\]\[\d+\]\['
$crtPrefixPattern     = '^(\[[a-z]+\])?[A-Za-z]+: '
function ConvertFrom-DebugLine([string]$line) {
    $m = [regex]::Match($line, $consolePrefixPattern)
    if ($m.Success -and $m.Index -gt 0) { return $line.Substring($m.Index) }
    $c = [regex]::Match($line, $crtPrefixPattern)
    if ($c.Success) {
        $rest = $line.Substring($c.Length)
        if ($rest.Length -ge 3 -and (($rest.Length - 3) % 2) -eq 0) {
            $k = ($rest.Length - 3) / 2
            if ($rest.Substring($k, 3) -ceq '   ' -and $rest.Substring(0, $k) -ceq $rest.Substring($k + 3)) { return '   ' + $rest.Substring($k + 3) }
        }
    }
    return $line
}

$results = @()
$negSeen = @{}
try {
    foreach ($cfg in Get-ChildItem (Join-Path $here '*.dms') | Sort-Object Name) {
        $name = $cfg.Name
        $stem = [IO.Path]::GetFileNameWithoutExtension($name)
        # typed: a one-element array leaves an if-expression as a bare string, which @-splats per character
        [string[]]$itemArgs = if ($map.ContainsKey($name)) { $map[$name] } else { '/checks' }
        $item = $itemArgs -join ' '
        $log  = Join-Path $OutDir "$stem.log"
        $out  = Join-Path $OutDir "$stem.out"
        $expects = @()
        foreach ($l in [IO.File]::ReadAllLines($cfg.FullName, $utf8)) {
            if ($l -cmatch $expectPattern) { $expects += $Matches[1].Trim() }
        }
        $r = Invoke-GuardedRun $Exe (@("/L$log", $cfg.FullName) + $itemArgs) $out $MaxCommitGB $TimeoutSec $stem
        $code = $r.ExitCode
        $isNeg = ($stem -match '_neg\d*(_|$)') -or ($stem -eq 'fn_test_defcheck')
        if ($isNeg) { $negSeen[$stem] = $true }
        if ($r.Limit) { "LIMIT: ${stem}: $($r.Note)" } # straight away, so that a run which seemed to stall says why
        $detail = ''
        if ($r.Limit)                               { $verdict = "LIMIT($($r.Limit))" }
        elseif ($code -eq 3)                        { $verdict = 'ASSERT' }
        elseif ($isNeg -ne ($code -ne 0))           { $verdict = 'UNEXPECTED' }
        elseif (-not $isNeg) {
            if ($expects.Count)                     { $verdict = 'BAD-EXPECT'; $detail = 'a positive case with an EXPECT-FAILURE line' }
            else                                    { $verdict = 'ok' }
        }
        elseif ($expects -contains '')              { $verdict = 'BAD-EXPECT'; $detail = 'an EXPECT-FAILURE line without a fragment' }
        elseif ($unchecked.ContainsKey($stem)) {
            if ($expects.Count)                     { $verdict = 'LISTED-HAS-EXPECT'; $detail = "listed in $uncheckedName" }
            else                                    { $verdict = 'ok(neg,unchecked)' }
        }
        elseif (-not $expects.Count)                { $verdict = 'NO-EXPECT' }
        else {
            # the lines of every message: its first line, and the lines that continue it
            $errorLines = New-Object System.Collections.Generic.List[string]
            $inMessage = $false
            foreach ($raw in [IO.File]::ReadAllLines($out, $utf8)) {
                $l = ConvertFrom-DebugLine $raw
                if ($l -cmatch $errorPattern) { $errorLines.Add($l); $inMessage = $true; continue }
                if ($inMessage -and ($l -cmatch $msgEndPattern)) { $inMessage = $false }
                if ($inMessage) { $errorLines.Add($l) }
            }
            $missing = @()
            foreach ($f in $expects) {
                $found = $false
                foreach ($l in $errorLines) { if ($l.Contains($f)) { $found = $true; break } }
                if (-not $found) { $missing += $f }
            }
            if ($missing.Count) {
                $verdict = 'WRONG-FAILURE'
                $first = if ($errorLines.Count) { $errorLines[0] } else { '(none)' }
                if ($first.Length -gt 200) { $first = $first.Substring(0, 200) + '...' }
                $detail = "not found: '$($missing[0])'; first error line: $first"
            }
            else { $verdict = 'ok(neg)' }
        }
        if ($verdict -in 'ok', 'ok(neg)', 'ok(neg,unchecked)') {
            foreach ($l in [IO.File]::ReadAllLines($out, $utf8)) {
                $at = $l.IndexOf($caughtMarker, [StringComparison]::Ordinal)
                if ($at -lt 0) { continue }
                $msg = $l.Substring($at + $caughtMarker.Length).Trim()
                if ($msg.Length -gt 200) { $msg = $msg.Substring(0, 200) + '...' }
                $verdict = 'CAUGHT-AT-MAIN'
                $detail = "stderr has '$caughtMarker $msg'"
                break
            }
        }
        $results +=[pscustomobject]@{ config = $stem; verdict = $verdict; exit = $code; item = $item; detail = $detail }
    }
}
finally {
    $env:GEODMS_directories_LocalDataDir = $prevLocalDataDir # leave the caller's session as it was
}
foreach ($stem in $unchecked.Keys | Sort-Object) {
    if (-not $negSeen.ContainsKey($stem)) {
        $results += [pscustomobject]@{ config = $stem; verdict = 'LISTED-UNKNOWN'; exit = ''; item = ''; detail = "listed in $uncheckedName, but no negative case has this name" }
    }
}
$results | Format-Table config, verdict, exit, item -AutoSize | Out-String -Width 120
# @(): a single [pscustomobject] has no .Count in Windows PowerShell 5.1, so one bad case printed 'BAD='
$bad = @($results | Where-Object { $_.verdict -notin 'ok', 'ok(neg)', 'ok(neg,unchecked)' })
Get-GuardSummary $MaxCommitGB $TimeoutSec
$negs = @($results | Where-Object { $negSeen.ContainsKey($_.config) })
"NEGATIVES=$($negs.Count): $(@($negs | Where-Object verdict -eq 'ok(neg)').Count) failed with their EXPECT-FAILURE message," +
" $(@($negs | Where-Object verdict -eq 'ok(neg,unchecked)').Count) listed in $uncheckedName and judged on the exit code only"
"TOTAL=$($results.Count) BAD=$($bad.Count)"
if ($bad.Count) {
    "FAILED CASES:"
    $bad | Format-Table config, verdict, exit, item -AutoSize | Out-String -Width 120
    foreach ($b in $bad | Where-Object detail) { "$($b.verdict) $($b.config): $($b.detail)" }
    $seen = @($bad | ForEach-Object verdict)
    if ($seen -contains 'WRONG-FAILURE')     { "A WRONG-FAILURE failed, but not with the message that its EXPECT-FAILURE lines expect: compare them with <config>.out in $OutDir." }
    if ($seen -contains 'NO-EXPECT')         { "A NO-EXPECT is a negative case without an EXPECT-FAILURE line: add one with a fragment of the message it is written to fail with." }
    if ($seen -contains 'LISTED-HAS-EXPECT') { "A LISTED-HAS-EXPECT has an EXPECT-FAILURE line and is still listed in ${uncheckedName}: remove its line there." }
    if ($seen -contains 'LISTED-UNKNOWN')    { "A LISTED-UNKNOWN is a name in $uncheckedName that is no negative case (any more): remove its line there." }
    if ($seen -contains 'CAUGHT-AT-MAIN')    { "A CAUGHT-AT-MAIN reported an error through GeoDmsRun's exception translator, which writes it to stderr behind '$caughtMarker' and not to the /L log: find the API call that let it through (#1384)." }
    exit 1
}
"ALL TESTCASES PASSED"
exit 0
