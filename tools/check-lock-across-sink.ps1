# Registry-share-across-sink audit (doc/deadlocks.md B6 and R1; Parallel.h, "Two things it
# deliberately does not do").
#
# A TokenStr / TokenStrRange -- what TokenID::GetStrLock(), AsStrRangeLock(), GetTokenStrLock(),
# Object::GetNameLock() and friends return -- holds a shared usage of the token registry
# (IndexedString, 90) for its lifetime. As an unnamed argument temporary it lives to the end of
# the full expression, so in
#     reportF(st, "{}", item->GetNameLock().c_str());
# the usage spans the whole report: mgFormat2string, reportD, the DebugOutStream section and the
# posting of the message. The report funnel declares DMS_ENTERS(IndexedString, shared) and admits
# it today; this check keeps the pattern out anyway, because the lock-level checker is SCOPE-shaped
# and cannot see a LIFETIME that spans a call, and because the fix is free and cheaper: pass the
# TokenID itself,
#     reportF(st, "{}", item->GetNameID());
# mgFormatArg renders it through mgFormatArgOf (sym/Token.h) into an SSO-sized std::string, and the
# usage ends inside the format call.
#
# FAIL: a format sink -- reportF/reportD and their _without_cancellation_check variants, DBG_TRACE,
#   ProgressMsg, SetStatusText, SendStatusText, throwErrorF, throwDmsErrF, throwItemErrorF,
#   throwOperErrorF, throwErrorD, throwDmsErrD, throwItemErrorD, throwCheckFailed,
#   throwPreconditionFailed, mySSPrintF, mgFormat2SharedStr -- whose argument text contains a
#   ...Lock( accessor that is not a data/item/tile lock. At a throw-family sink the span is
#   harmless (after the format nothing runs in that frame but the throw, and unwinding destroys
#   the temporary first); it is flagged all the same, the TokenID form being shorter and cheaper.
#
# Not covered: a NAMED TokenStr local used by a later sink statement (needs dataflow; the #1227
# ...Lock() renames make that greppable by hand). Comments and string literals are blanked
# before the accessor search, so neither a // inside "https://..." nor an accessor named in a
# message text counts; the parenthesis walk still honours literals.
#
# Exit code: 1 if a FAIL site exists; 0 otherwise.

[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$srcDirs = 'rtc','clc','geo','stg','stx','shv','qtgui','python','run' |
    ForEach-Object { Join-Path $root $_ } | Where-Object { Test-Path $_ }

$files = Get-ChildItem $srcDirs -Recurse -File -Include *.h,*.cpp,*.ipp,*.hpp,*.inl |
    Where-Object { $_.FullName -notmatch '\\(vcpkg|bin|obj|build|vc_archives|vc_downloads)\\' }

$sinks = 'reportF','reportD','reportF_without_cancellation_check','reportD_without_cancellation_check',
         'DBG_TRACE','DBG_TraceStr','ProgressMsg','SetStatusText','SendStatusText',
         'throwErrorF','throwDmsErrF','throwItemErrorF','throwOperErrorF','throwErrorD','throwDmsErrD',
         'throwItemErrorD','throwCheckFailed','throwPreconditionFailed','mySSPrintF','mgFormat2SharedStr'
$sinkRe = [regex]('\b(' + ($sinks -join '|') + ')\s*\(')
$lockRe = [regex]'\b\w*Lock\s*\('
# accessors that end in Lock( but are not registry usages: data, item and tile locks, and the
# lock-taking primitives themselves
$notRegistryRe = [regex]'^(DataReadLock|DataWriteLock|ItemReadLock|ItemWriteLock|ScopedLock|ScopedTryLock|ReadableTileLock|WritableTileLock|TileLock|StaticLock|GetReadLock|GetDataItemReadLock|Lock|TryLock|CanLock|IsLock|\w*_lock)\s*\($'
# one pass over literals and comments: a literal is kept verbatim (the // in "https://..." is not a
# comment), a comment is blanked to spaces, so every offset and line number stays the same
$literalOrCommentRe = [regex]'"(?:[^"\\\r\n]|\\.)*"|''(?:[^''\\\r\n]|\\.)*''|//[^\r\n]*|/\*[\s\S]*?\*/'
$blankComments = [System.Text.RegularExpressions.MatchEvaluator]{
    param($m)
    if ($m.Value[0] -eq '"' -or $m.Value[0] -eq "'") { $m.Value } else { ' ' * $m.Value.Length }
}
$blankAll = [System.Text.RegularExpressions.MatchEvaluator]{ param($m) ' ' * $m.Value.Length }

# index just past the ')' that closes the '(' at $i, skipping string and char literals
function Get-BalancedEnd([string]$s, [int]$i) {
    $depth = 0; $j = $i; $n = $s.Length
    while ($j -lt $n) {
        $c = $s[$j]
        if ($c -eq '"' -or $c -eq "'") {
            $q = $c; $j++
            while ($j -lt $n -and $s[$j] -ne $q) { if ($s[$j] -eq '\') { $j++ }; $j++ }
        }
        elseif ($c -eq '(') { $depth++ }
        elseif ($c -eq ')') { $depth--; if ($depth -eq 0) { return $j + 1 } }
        $j++
    }
    return $n
}

$fails = @()
foreach ($f in $files) {
    $text = [System.IO.File]::ReadAllText($f.FullName)
    if ($text.IndexOf('Lock') -lt 0) { continue }
    $code = $literalOrCommentRe.Replace($text, $blankComments)   # same offsets, comments blanked
    foreach ($m in $sinkRe.Matches($code)) {
        $open = $m.Index + $m.Length - 1
        $end = Get-BalancedEnd $code $open
        $args = $code.Substring($open, $end - $open)
        $argsNoLiterals = $literalOrCommentRe.Replace($args, $blankAll)
        $locks = @($lockRe.Matches($argsNoLiterals) | ForEach-Object { $_.Value } |
                   Where-Object { -not $notRegistryRe.IsMatch($_) } | Sort-Object -Unique)
        if ($locks.Count -eq 0) { continue }
        $line = ($code.Substring(0, $m.Index) -split "`n").Count
        $snippet = ($code.Substring($m.Index, $end - $m.Index) -replace '\s+', ' ')
        if ($snippet.Length -gt 200) { $snippet = $snippet.Substring(0, 200) + ' ...' }
        $rel = $f.FullName.Substring($root.Length + 1)
        $fails += ("  {0}:{1}: {2} <- {3}`n      {4}" -f $rel, $line, $m.Groups[1].Value, ($locks -join ', '), $snippet)
    }
}

if ($fails.Count -gt 0) {
    Write-Host ("FAIL: {0} format sink(s) hold a token-registry usage across the call; pass the TokenID instead of ...Lock():" -f $fails.Count) -ForegroundColor Red
    $fails | ForEach-Object { Write-Host $_ }
    exit 1
}
Write-Host "OK: no format sink holds a token-registry usage across the call." -ForegroundColor Green
exit 0
