# Lock-ceiling call-site audit: the static half of the DMS_ENTERS scheme (doc/deadlocks.md 3.7,
# follow-up 5; #1227 section 3, #1233).
#
# A function that opens with
#     DMS_ENTERS(ord_level_type::L, dms_shared_v | dms_exclusive_v);
#     DMS_ENTERS_ITEM(ord_level_type::L, ...);          // outermost acquire is a per-item lock
#     DMS_ENTERS_NOTHING;                                // takes no leveled lock at all
# declares the outermost lock level it will reach, directly or through anything it calls. The Debug
# runtime checks that claim on entry against what the caller holds -- but only on the paths a run
# takes. This pass checks the claims against each other without running anything: for every call
# from a declared function to a declared function, the caller's ceiling must admit the callee's,
# by the rules of level_type::Allow (rtc/dll/src/Parallel.h):
#
#   caller per-item (DMS_ENTERS_ITEM)     admits everything            (rules 3 and 4)
#   caller global (L, m), callee per-item  is refused                  (rule 5)
#   caller global (L, m), callee NOTHING   is admitted                 (EntersNothing is inner to all)
#   caller global (L, m), callee (M, m2)   admitted iff M > L, or M == L and (m2 shared or m exclusive)
#
# The ordinals come from rtc/dll/src/LockLevels.h, so a renumbering changes nothing here.
#
# What it cannot see, and says so: a callee whose name is declared at different levels in
# different bodies (virtual overrides, overloads, unrelated same-named members) is reported once as
# AMBIGUOUS and not checked -- resolving the dynamic type is the job of the runtime check; a call
# made while the caller HOLDS a section taken earlier in the same body is checked against the
# caller's ceiling, not against that section (the runtime does that); and a declaration placed
# after a fast-path check (the rule of 3.7) is treated as covering the whole function, which is
# conservative in the right direction. Generic member names (Add, Del, lock, release, ...) are
# skipped by a short list, because a bare `x.Add(` cannot be tied to the one declared Add.
#
# Exit code: 1 if a FAIL site exists; 0 otherwise. -ShowAmbiguous lists the skipped names,
# -ShowUnattributed the declarations no header could be found for (each of those is a gap in this
# pass, not in the code), and CEIL_TRACE=<function> in the environment prints the body range the
# pass computed for that function.

[CmdletBinding()]
param([switch]$ShowAmbiguous, [switch]$ShowUnattributed)

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$srcDirs = 'rtc','clc','geo','stg','stx','shv','qtgui' |
    ForEach-Object { Join-Path $root $_ } | Where-Object { Test-Path $_ }
$files = Get-ChildItem $srcDirs -Recurse -File -Include *.h,*.cpp,*.ipp,*.hpp,*.inl |
    Where-Object { $_.FullName -notmatch '\\(vcpkg|bin|obj|build|vc_archives|vc_downloads)\\' }

# ---- the ordinal table -----------------------------------------------------------------------------
$levels = @{}
$enumText = [System.IO.File]::ReadAllText((Join-Path $root 'rtc\dll\src\LockLevels.h'))
$enumBody = [regex]::Match($enumText, 'enum class ord_level_type : UInt32\s*\{([\s\S]*?)\};').Groups[1].Value
foreach ($m in [regex]::Matches($enumBody, '(?m)^\s*(\w+)\s*=\s*(0x[0-9A-Fa-f]+|\d+)\s*,')) {
    $v = $m.Groups[2].Value
    $levels[$m.Groups[1].Value] = if ($v -like '0x*') { [uint32]::Parse($v.Substring(2), 'HexNumber') } else { [uint32]$v }
}
if ($levels.Count -lt 10) { Write-Host "cannot read the ordinal table from LockLevels.h"; exit 2 }

# ---- pass 1: declarations, keyed by the enclosing function's simple name ---------------------------
$declRe = [regex]'DMS_ENTERS(_ITEM)?\(ord_level_type::(\w+),\s*(dms_shared_v|dms_exclusive_v)\)|DMS_ENTERS_NOTHING\b'
# a definition header: optional template/extern/linkage/return-type tokens, an optional Class:: path, the
# simple name, a parameter list that may continue on later lines, the usual trailers, an optional '{'
# atomic groups (?>...) keep the token repetition from backtracking on long statement lines; a line with a ';'
# is never a definition header and is not even tried (Test-Header)
$hdrRe  = [regex]'^\s*(?:template\s*<>\s*)?(?:extern\s+"C"\s+)?(?>(?:(?>[\w:<>,\*&~]+)\s+(?!\())*)(?>(?:[\w~]+(?:<>)?::)*)([\w~]+)\s*\((?>[^;()]*)\)?\s*(?:const)?\s*(?:noexcept)?\s*(?:->\s*[\w:<>]+)?\s*(?:override)?\s*(?://.*)?\s*\{?\s*$'
$tplRe  = [regex]'<[^<>()]*>'
function Test-Header([string]$t) {
    if ($t.Length -gt 300 -or $t.IndexOf('(') -lt 0 -or $t.IndexOf(';') -ge 0) { return $null }
    # collapse template argument lists (innermost first) so `IndexedStrings<A, B, C>::f(` and `template <typename T>` read as `<>`
    $u = $t; do { $prev = $u; $u = $tplRe.Replace($u, '<>') } while ($u -ne $prev)
    $hm = $hdrRe.Match($u); if ($hm.Success) { return $hm } else { return $null }
}
# a one-line definition, `T f(...) { body; }`: the header is the part before the first '{'
function Test-HeaderPrefix([string]$t) {
    $b = $t.IndexOf('{'); if ($b -lt 0) { return $null }
    return Test-Header $t.Substring(0, $b)
}
$ctrlRe = [regex]'^\s*(if|for|while|switch|return|else|catch|assert|dms_assert|dbg_assert|MG_CHECK|MGD_CHECKDATA|DBG_START|DBG_TRACE|throw|goto|case|default)\b'
$generic = @('Add','Del','lock','unlock','lock_shared','unlock_shared','try_lock_shared','try_lock_for','release','reset','Set','Init','Report','Commit','Copy','Run','Process','Update','Clear','Describe','GetDescription','GetName','GetFullName','Object','~Object','operator')

function Get-EnclosingFunction([string[]]$lines, [int]$i) {
    # walk back to the nearest line that looks like a function header (column 0, or one tab in a namespace/class body)
    $own = Test-HeaderPrefix $lines[$i]                        # `T f(...) { ... DMS_ENTERS ... }` on one line
    if ($null -ne $own) { return @{ name = $own.Groups[1].Value; line = $i + 1; text = $lines[$i].Trim() } }
    for ($j = $i; $j -ge 0 -and $j -gt $i - 80; $j--) {
        $t = $lines[$j]
        if ($t -match '^\s*$' -or $ctrlRe.IsMatch($t) -or $t -match '^\s*(#|//|/\*|\*|;)') { continue }
        $indent = ($t -replace '^(\s*).*$', '$1')
        if ($indent.Length -le 1 -and $t -match '^\s*\}') { return $null }   # the previous function's end: give up rather than attribute across it
        if ($indent.Length -gt 1) { continue }
        $hm = Test-Header $t
        if ($null -ne $hm) { return @{ name = $hm.Groups[1].Value; line = $j + 1; text = $t.Trim() } }
    }
    return $null
}

$decls = @{}       # name -> list of @{file; line; item; level; shared; nothing}
$perFile = @{}     # file -> list of @{fn; line; decl}   (for pass 2)
$unattributed = @()
foreach ($f in $files) {
    $text = [System.IO.File]::ReadAllText($f.FullName)
    if ($text.IndexOf('DMS_ENTERS') -lt 0) { continue }
    $lines = $text -split "`r?`n"
    $list = @()
    for ($i = 0; $i -lt $lines.Length; $i++) {
        $m = $declRe.Match($lines[$i]); if (-not $m.Success) { continue }
        if ($lines[$i] -match '^\s*(//|#define)') { continue }
        $fn = Get-EnclosingFunction $lines $i
        if ($null -eq $fn) { $unattributed += ("  {0}:{1}: {2}" -f $f.FullName.Substring($root.Length + 1), ($i + 1), $lines[$i].Trim()); continue }
        $d = if ($m.Value -like 'DMS_ENTERS_NOTHING*') { @{ nothing = $true; item = $false; level = [uint32]0xFFFFFFFF; shared = $false } }
             else { @{ nothing = $false; item = [bool]$m.Groups[1].Success; level = $levels[$m.Groups[2].Value]; shared = ($m.Groups[3].Value -eq 'dms_shared_v') } }
        $d.file = $f.FullName.Substring($root.Length + 1); $d.line = $i + 1; $d.fnline = $fn.line; $d.name = $fn.name
        if (-not $decls.ContainsKey($fn.name)) { $decls[$fn.name] = @() }
        $decls[$fn.name] += $d
        $list += $d
    }
    if ($list.Count) { $perFile[$f.FullName] = $list }
}

# a callee is checkable when every declaration under its name agrees; generic names are never checked
function Get-CalleeSpec([string]$name) {
    if ($generic -contains $name) { return $null }
    if (-not $decls.ContainsKey($name)) { return $null }
    $ds = $decls[$name]
    $first = $ds[0]
    foreach ($d in $ds) {
        if ($d.nothing -ne $first.nothing -or $d.item -ne $first.item -or $d.level -ne $first.level -or $d.shared -ne $first.shared) { return 'AMBIGUOUS' }
    }
    return $first
}

function Test-Admits($caller, $callee) {
    if ($caller.item) { return $true }                       # rules 3 and 4
    if ($callee.nothing) { return $true }                    # EntersNothing is inner to everything
    if ($callee.item) { return $false }                      # rule 5
    if ($caller.level -lt $callee.level) { return $true }
    if ($caller.level -gt $callee.level) { return $false }
    return ($callee.shared -or -not $caller.shared)          # the equal-level ceiling rule
}

function Format-Spec($d) {
    if ($d.nothing) { return 'NOTHING' }
    $lv = ($levels.GetEnumerator() | Where-Object { $_.Value -eq $d.level } | Select-Object -First 1).Key
    $mode = if ($d.shared) { 'shared' } else { 'exclusive' }
    if ($d.item) { return "ITEM($lv, $mode)" } else { return "($lv $($d.level), $mode)" }
}

# ---- pass 2: call sites inside declared functions ----------------------------------------------------
$names = @($decls.Keys | Where-Object { $generic -notcontains $_ })
$callRe = [regex]('(?<![\w~:])(' + (($names | ForEach-Object { [regex]::Escape($_) }) -join '|') + ')\s*\(')
$literalOrCommentRe = [regex]'"(?:[^"\\\r\n]|\\.)*"|''(?:[^''\\\r\n]|\\.)*''|//[^\r\n]*|/\*[\s\S]*?\*/'
# blank to spaces but KEEP the newlines, so a block comment does not shift every later line number
$blank = [System.Text.RegularExpressions.MatchEvaluator]{ param($m) [regex]::Replace($m.Value, '[^\r\n]', ' ') }

$fails = @(); $ambiguous = @{}; $checked = 0
foreach ($path in $perFile.Keys) {
    $text = [System.IO.File]::ReadAllText($path)
    $code = $literalOrCommentRe.Replace($text, $blank)
    $lines = $code -split "`r?`n"
    $rel = $path.Substring($root.Length + 1)
    foreach ($d in $perFile[$path]) {
        # the declared function's body: from its header to the next column-0/one-tab function header or end of file
        $start = $d.fnline - 1
        $end = $lines.Length - 1
        for ($k = $d.line; $k -lt $lines.Length; $k++) {
            $t = $lines[$k]
            if ($t -match '^\s*$' -or $ctrlRe.IsMatch($t) -or $t -match '^\s*(#|//|\}|;)') { continue }
            $indent = ($t -replace '^(\s*).*$', '$1')
            if ($indent.Length -le 1 -and $k -gt $d.line -and $null -ne (Test-Header $t)) { $end = $k - 1; break }
        }
        if ($env:CEIL_TRACE -and $d.name -eq $env:CEIL_TRACE) { Write-Host ("trace: {0} in {1}: header {2}, declaration {3}, body end {4}" -f $d.name, $rel, $d.fnline, $d.line, ($end + 1)) }
        for ($k = $d.line; $k -le $end; $k++) {
            foreach ($cm in $callRe.Matches($lines[$k])) {
                $callee = $cm.Groups[1].Value
                if ($callee -eq $d.name) { continue }             # recursion / same-name overload chain
                $spec = Get-CalleeSpec $callee
                if ($null -eq $spec) { continue }
                if ($spec -is [string]) { $ambiguous[$callee] = $true; continue }
                $checked++
                if (-not (Test-Admits $d $spec)) {
                    $fails += ("  {0}:{1}: {2} {3} calls {4} {5}" -f $rel, ($k + 1), $d.name, (Format-Spec $d), $callee, (Format-Spec $spec))
                }
            }
        }
    }
}

$nDecl = ($decls.Values | ForEach-Object { $_ } | Measure-Object).Count
if ($unattributed.Count) {
    Write-Host ("NOTE: {0} declaration(s) could not be tied to a function header and were not checked (-ShowUnattributed lists them)." -f $unattributed.Count) -ForegroundColor Yellow
    if ($ShowUnattributed) { $unattributed | ForEach-Object { Write-Host $_ } }
}
if ($ShowAmbiguous -and $ambiguous.Count) {
    Write-Host ("AMBIGUOUS (declared at different levels under one name; not checked): {0}" -f (($ambiguous.Keys | Sort-Object) -join ', ')) -ForegroundColor Yellow
}
if ($fails.Count -gt 0) {
    Write-Host ("FAIL: {0} call site(s) where a declared ceiling does not admit the callee's ({1} declarations, {2} call sites checked):" -f $fails.Count, $nDecl, $checked) -ForegroundColor Red
    $fails | Sort-Object | ForEach-Object { Write-Host $_ }
    exit 1
}
Write-Host ("OK: every checked call from a declared function admits its callee's ceiling ({0} declarations, {1} call sites checked, {2} ambiguous names skipped)." -f $nDecl, $checked, $ambiguous.Count) -ForegroundColor Green
exit 0
