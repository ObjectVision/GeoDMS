# Format-argument audit: the arguments of every format call with a literal format must match its
# placeholders (doc/development/boost-format-to-std-format-migration.md stage 2; INF-A03, PLN-A04;
# doc/continuations-2026-10-06.md A3).
#
# The format sinks -- throwErrorF, throwDmsErrF, reportF, mgFormat2string, mySSPrintF and the rest
# listed below -- forward a runtime CharPtr to std::vformat (utl/MgFormat.h, mgFormat2string). Unlike
# boost::format, which they replaced, vformat says nothing when the arguments do not fit:
#
#   more arguments than placeholders   the surplus is dropped silently. The worst case is a call in
#                                      the convention of the wrong sink: throwErrorF takes
#                                      (context, format, args...), throwDmsErrF takes
#                                      (format, args...), so throwDmsErrF("voronoi", "the second
#                                      argument must ...{}", range) reports just "voronoi".
#   fewer arguments than placeholders  vformat throws; mgFormat2string catches that and emits the raw
#                                      format followed by the arguments, braces and all.
#   a printf directive (%d, %s, ...)   is literal text to std::format; its argument is dropped.
#
# A std::format_string parameter would make each of these a compile error (stage 2 of the migration
# document). Until the sinks take one, this pass checks every call whose format argument is made of
# string literals only; a format held in a variable or built at run time is not seen.
#
# Placeholders follow std::format: {{ and }} are literal braces, {} and {:spec} take the next argument,
# {N} and {N:spec} argument N.
#
# Exit code: 1 if a FAIL site exists; 0 otherwise. -ShowCount prints how many calls were checked.

[CmdletBinding()]
param([switch]$ShowCount)

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$srcDirs = 'rtc','clc','geo','stg','stx','shv','qtgui','python','run' |
    ForEach-Object { Join-Path $root $_ } | Where-Object { Test-Path $_ }

$files = Get-ChildItem $srcDirs -Recurse -File -Include *.h,*.cpp,*.ipp,*.hpp,*.inl |
    Where-Object { $_.FullName -notmatch '\\(vcpkg|bin|obj|build|vc_archives|vc_downloads)\\' }

# sink name -> index of its format argument; -1: the first argument when that is a literal, else the
# second (reportF and its variant take an optional MsgCategory before the severity, throwItemErrorF an
# optional self before the format)
$formatIndex = @{
    'throwDmsErrF' = 0; 'throwOperErrorF' = 0; 'throwMsgF' = 0; 'TellExtraF' = 0; 'MG_TRACE' = 0;
    'mgFormat2string' = 0; 'mgFormat2SharedStr' = 0; 'mgFormat' = 0; 'mySSPrintF' = 0;
    'throwLastSystemError' = 0; 'myArrayPrintF' = 0; 'DBG_TRACE' = 0; 'DMS_TRACE' = 0;
    'throwErrorF' = 1; 'throwInternalErrorF' = 1; 'throwSystemError' = 1;
    'myFixedBufferAsCString' = 2; 'myFixedBufferWrite' = 2; 'myFixedBufferAsCharPtrRange' = 2;
    'reportF' = -2; 'reportF_without_cancellation_check' = -2;
    'throwItemErrorF' = -1
}
# the name, optional template arguments (mgFormat2string<Args...>, myArrayPrintF<64>), an optional
# variable name (myArrayPrintF<64> buf(...)), and the opening parenthesis
$sinkRe = [regex]('\b(' + ($formatIndex.Keys -join '|') + ')\s*(?:<[^<>;()]*>)?\s*(?:\w+\s*)?\(')

# one pass over literals and comments: a literal is kept verbatim, a comment is blanked to spaces but
# keeps its line ends, so every offset and line number stays the same
$literalOrCommentRe = [regex]'"(?:[^"\\\r\n]|\\.)*"|''(?:[^''\\\r\n]|\\.)*''|//[^\r\n]*|/\*[\s\S]*?\*/'
$notLineEndRe = [regex]'[^\r\n]'
$blankComments = [System.Text.RegularExpressions.MatchEvaluator]{
    param($m)
    if ($m.Value[0] -eq '"' -or $m.Value[0] -eq "'") { $m.Value } else { $notLineEndRe.Replace($m.Value, ' ') }
}
# a format argument made of string literals only, adjacent ones concatenated by the compiler
$literalsOnlyRe = [regex]'^\s*(?:(?:u8|L|u|U)?"(?:[^"\\\r\n]|\\.)*"\s*)+$'
$literalRe      = [regex]'"((?:[^"\\\r\n]|\\.)*)"'
# a printf conversion; a space flag is left out so that "50% of" does not read as one
$printfRe       = [regex]'%[-+#0]*(?:\d+|\*)?(?:\.\d+)?(?:hh|h|ll|l|z|j|t|I64)?[diuxXfFgGsc]'

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

# the top-level arguments of an argument list without its outer parentheses
function Split-Args([string]$s) {
    $parts = New-Object System.Collections.Generic.List[string]
    $depth = 0; $start = 0; $j = 0; $n = $s.Length
    while ($j -lt $n) {
        $c = $s[$j]
        if ($c -eq '"' -or $c -eq "'") {
            $q = $c; $j++
            while ($j -lt $n -and $s[$j] -ne $q) { if ($s[$j] -eq '\') { $j++ }; $j++ }
        }
        elseif ($c -eq '(' -or $c -eq '[' -or $c -eq '{') { $depth++ }
        elseif ($c -eq ')' -or $c -eq ']' -or $c -eq '}') { $depth-- }
        elseif ($c -eq ',' -and $depth -eq 0) { $parts.Add($s.Substring($start, $j - $start)); $start = $j + 1 }
        $j++
    }
    if ($s.Substring($start).Trim().Length -gt 0 -or $parts.Count -gt 0) { $parts.Add($s.Substring($start)) }
    return ,$parts
}

# the number of arguments a std::format string takes, or a string that says why it is malformed
function Get-ArgCount([string]$fmt) {
    $auto = 0; $maxManual = -1; $j = 0; $n = $fmt.Length
    while ($j -lt $n) {
        $c = $fmt[$j]
        if ($c -eq '{') {
            if ($j + 1 -lt $n -and $fmt[$j + 1] -eq '{') { $j += 2; continue }
            $close = $fmt.IndexOf('}', $j)
            if ($close -lt 0) { return "an unclosed '{'" }
            $id = $fmt.Substring($j + 1, $close - $j - 1).Split(':')[0].Trim()
            if ($id -eq '') { $auto++ }
            elseif ($id -match '^\d+$') { $maxManual = [Math]::Max($maxManual, [int]$id) }
            else { return "a placeholder {$id} that std::format cannot read" }
            $j = $close + 1; continue
        }
        if ($c -eq '}') {
            if ($j + 1 -lt $n -and $fmt[$j + 1] -eq '}') { $j += 2; continue }
            return "a lone '}' (write '}}')"
        }
        $j++
    }
    if ($auto -gt 0 -and $maxManual -ge 0) { return 'automatic and numbered placeholders mixed' }
    if ($maxManual -ge 0) { return $maxManual + 1 }
    return $auto
}

$fails = @()
$checked = 0
foreach ($f in $files) {
    $text = [System.IO.File]::ReadAllText($f.FullName)
    if (-not $sinkRe.IsMatch($text)) { continue }
    $code = $literalOrCommentRe.Replace($text, $blankComments)   # same offsets, comments blanked
    foreach ($m in $sinkRe.Matches($code)) {
        $name = $m.Groups[1].Value
        # a #define line forwards a parameter, not a literal
        $lineStart = $code.LastIndexOf("`n", [Math]::Max($m.Index - 1, 0)) + 1
        if ($code.Substring($lineStart, $m.Index - $lineStart) -match '^\s*#') { continue }
        $open = $m.Index + $m.Length - 1
        $end = Get-BalancedEnd $code $open
        $inner = $code.Substring($open + 1, [Math]::Max($end - $open - 2, 0))
        if ($name -eq 'DBG_TRACE' -or $name -eq 'DMS_TRACE') {
            # DBG_TRACE(("format", args)): the macro argument is the parenthesised argument list of MG_TRACE
            $t = $inner.Trim()
            if (-not ($t.StartsWith('(') -and $t.EndsWith(')'))) { continue }
            $inner = $t.Substring(1, $t.Length - 2)
        }
        $parts = Split-Args $inner
        if ($parts.Count -eq 0) { continue }

        $fi = $formatIndex[$name]
        if ($fi -eq -1) { $fi = if ($literalsOnlyRe.IsMatch($parts[0])) { 0 } else { 1 } }
        elseif ($fi -eq -2) { $fi = if ($parts[0] -match '\bMsgCategory\b|\bMC_\w+') { 2 } else { 1 } }
        if ($parts.Count -le $fi) { continue }
        if (-not $literalsOnlyRe.IsMatch($parts[$fi])) { continue }   # a format held in a variable: not seen

        $fmt = -join ($literalRe.Matches($parts[$fi]) | ForEach-Object { $_.Groups[1].Value })
        $given = $parts.Count - $fi - 1
        $checked++

        $why = $null
        $need = Get-ArgCount $fmt
        $pm = $printfRe.Match($fmt)
        if ($need -is [string])    { $why = "the format has $need" }
        elseif ($pm.Success)       { $why = "the format has a printf directive '$($pm.Value)', which std::format prints as text" }
        elseif ($given -gt $need)  { $why = "$given argument(s) for $need placeholder(s): vformat drops the surplus silently" }
        elseif ($given -lt $need)  { $why = "$given argument(s) for $need placeholder(s): the format fails and the raw text is reported" }
        if (-not $why) { continue }

        $line = ($code.Substring(0, $m.Index) -split "`n").Count
        $snippet = ($code.Substring($m.Index, $end - $m.Index) -replace '\s+', ' ')
        if ($snippet.Length -gt 220) { $snippet = $snippet.Substring(0, 220) + ' ...' }
        $rel = $f.FullName.Substring($root.Length + 1)
        $fails += ("  {0}:{1}: {2}: {3}`n      {4}" -f $rel, $line, $name, $why, $snippet)
    }
}

if ($ShowCount) { Write-Host "$checked format call(s) with a literal format checked." }
if ($fails.Count -gt 0) {
    Write-Host ("FAIL: {0} format call(s) whose arguments do not match the placeholders of their literal format:" -f $fails.Count) -ForegroundColor Red
    $fails | ForEach-Object { Write-Host $_ }
    exit 1
}
Write-Host "OK: every format call with a literal format has one argument per placeholder ($checked checked)." -ForegroundColor Green
exit 0
