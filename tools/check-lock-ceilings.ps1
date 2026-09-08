# Lock-ceiling call-site audit: the static half of the DMS_ENTERS scheme (doc/deadlocks.md 3.9,
# follow-up 5; #1227 section 3, #1233).
#
# A function that opens with
#     DMS_ENTERS(ord_level_type::L, dms_shared_v | dms_exclusive_v);
#     DMS_ENTERS_ITEM(ord_level_type::L, ...);          // outermost acquire is a per-item lock
#     DMS_ENTERS_NOTHING;                                // takes no leveled lock at all
# declares the outermost lock level it will reach, directly or through anything it calls. The Debug
# runtime checks that claim on entry against what the caller holds -- but only on the paths a run
# takes. This pass checks the claims against each other without running anything: for every call
# from a declared function to a declared function -- directly, or through any chain of UNDECLARED
# functions in between -- the caller's ceiling must admit the callee's, by the rules of
# level_type::Allow (rtc/dll/src/Parallel.h):
#
#   caller per-item (DMS_ENTERS_ITEM)     admits everything            (rules 3 and 4)
#   caller global (L, m), callee per-item  is refused                  (rule 5)
#   caller global (L, m), callee NOTHING   is admitted                 (EntersNothing is inner to all)
#   caller global (L, m), callee (M, m2)   admitted iff M > L, or M == L and (m2 shared or m exclusive)
#
# The ordinals come from rtc/dll/src/LockLevels.h, so a renumbering changes nothing here.
#
# Resolution is by simple name, and the pass says what that cannot do: a name DECLARED at different
# levels in different bodies (virtual overrides, overloads, unrelated same-named members) is reported
# as AMBIGUOUS and not checked -- resolving the dynamic type is the runtime's job; an undeclared name
# is followed only when it has exactly one definition in the tree (an overload set is not followed);
# a short list of generic member names (Add, Del, lock, release, ...) is never tied to a callee; a
# call made while the caller HOLDS a section taken earlier in the same body is checked against the
# caller's ceiling, not against that section (the runtime does that); and a declaration placed after
# a fast-path check (the rule of 3.7) is treated as covering the whole function, which is
# conservative in the right direction.
#
# Exit code: 1 if a FAIL site exists; 0 otherwise. -ShowAmbiguous lists the skipped names,
# -ShowUnattributed the declarations no definition could be found for (each of those is a gap in this
# pass, not in the code), and CEIL_TRACE=<function> in the environment prints the body range the
# pass computed for that function.

[CmdletBinding()]
param([switch]$ShowAmbiguous, [switch]$ShowUnattributed, [switch]$ListDeclared, [int]$MaxDepth = 8)

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

# ---- text handling -----------------------------------------------------------------------------------
# a definition header: optional template/extern/linkage/return-type tokens, an optional Class:: path, the
# simple name, a parameter list that may continue on later lines, the usual trailers, an optional '{'.
# Atomic groups (?>...) keep the token repetition from backtracking on long statement lines; the lookahead
# (?!\() keeps the token group from swallowing the name when a space precedes the parameter list.
$hdrRe  = [regex]'^\s*(?:template\s*<>\s*)?(?:extern\s+"C"\s+)?(?>(?:(?>[\w:<>,\*&~]+)\s+(?!\())*)(?>((?:[\w~]+(?:<>)?::)*))([\w~]+)\s*\((?>[^;()]*)\)?\s*(?:const)?\s*(?:noexcept)?\s*(?:->\s*[\w:<>]+)?\s*(?:override)?\s*(?://.*)?\s*\{?\s*$'
# groups: 1 = the Class:: path (empty for a free function or an in-class inline member), 2 = the simple name
$tplRe  = [regex]'<[^<>()]*>'
$ctrlRe = [regex]'^\s*(if|for|while|switch|return|else|catch|assert|dms_assert|dbg_assert|MG_CHECK|MGD_CHECKDATA|DBG_START|DBG_TRACE|throw|goto|case|default|using|typedef|struct|class|enum|namespace)\b'
function Test-Header([string]$t) {
    if ($t.Length -gt 300 -or $t.IndexOf('(') -lt 0 -or $t.IndexOf(';') -ge 0) { return $null }
    # collapse template argument lists (innermost first) so `IndexedStrings<A, B, C>::f(` and `template <typename T>` read as `<>`
    $u = $t; do { $prev = $u; $u = $tplRe.Replace($u, '<>') } while ($u -ne $prev)
    $hm = $hdrRe.Match($u); if (-not $hm.Success) { return $null }
    # a parameter list that continues on the next line is accepted only when a return type or a Class:: path precedes the
    # name: a bare `foo(a,` is the first line of a call, not of a definition
    if ($u.IndexOf(')') -lt 0 -and $u.Substring(0, $hm.Groups[2].Index).Trim() -eq '') { return $null }
    return $hm
}
# a one-line definition, `T f(...) { body; }`: the header is the part before the first '{', and its parameter list must be
# closed there -- `foo(x, [=] { ... })` is a call with a lambda argument, not a definition
function Test-HeaderPrefix([string]$t) {
    $b = $t.IndexOf('{'); if ($b -lt 0) { return $null }
    $pre = $t.Substring(0, $b)
    if ($pre.TrimEnd() -notmatch '\)\s*(?:const)?\s*(?:noexcept)?\s*(?:override)?\s*(?:->\s*[\w:<>]+)?$') { return $null }
    return Test-Header $pre
}
$literalOrCommentRe = [regex]'"(?:[^"\\\r\n]|\\.)*"|''(?:[^''\\\r\n]|\\.)*''|//[^\r\n]*|/\*[\s\S]*?\*/'
# blank to spaces but KEEP the newlines, so a block comment does not shift every later line number
$blank = [System.Text.RegularExpressions.MatchEvaluator]{ param($m) [regex]::Replace($m.Value, '[^\r\n]', ' ') }
$declRe = [regex]'DMS_ENTERS(_ITEM)?\(ord_level_type::(\w+),\s*(dms_shared_v|dms_exclusive_v)\)|DMS_ENTERS_NOTHING\b'
$generic = @('Add','Del','lock','unlock','lock_shared','unlock_shared','try_lock_shared','try_lock_for','release','reset','Set','Init','Report','Commit','Copy','Run','Process','Update','Clear','Describe','GetDescription','GetName','GetFullName','Object','~Object','operator','get','size','empty','begin','end','push_back','insert','erase','find','swap','assign','clear','count','max','min','apply','call','invoke','Get','Create','Open','Close','Read','Write','Load','Save','Do','Check','Visit','Handle','Start','Stop','Fail','Throw','Notify','Post','Send')

# ---- pass 0: every function definition in the tree, per file, with its body range ------------------
# $defs: name -> list of @{file; start; end; lines(ref)}   (start/end are 0-based line indices of the blanked text)
$defs = @{}; $fileLines = @{}
foreach ($f in $files) {
    $text = [System.IO.File]::ReadAllText($f.FullName)
    if ($text.IndexOf('(') -lt 0) { continue }
    $code = $literalOrCommentRe.Replace($text, $blank)
    $lines = $code -split "`r?`n"
    $fileLines[$f.FullName] = $lines
    $heads = @()
    for ($i = 0; $i -lt $lines.Length; $i++) {
        $t = $lines[$i]
        if ($t.Length -lt 3 -or $t.IndexOf('(') -lt 0) { continue }
        $indent = 0; while ($indent -lt $t.Length -and ($t[$indent] -eq "`t" -or $t[$indent] -eq ' ')) { $indent++ }
        if ($indent -gt 1) { continue }
        if ($ctrlRe.IsMatch($t)) { continue }
        $hm = Test-Header $t
        if ($null -eq $hm) { $hm = Test-HeaderPrefix $t }
        if ($null -ne $hm) { $heads += @{ name = $hm.Groups[2].Value; line = $i; qualified = ($hm.Groups[1].Value -ne '') } }
    }
    for ($h = 0; $h -lt $heads.Count; $h++) {
        $start = $heads[$h].line
        $end = if ($h + 1 -lt $heads.Count) { $heads[$h + 1].line - 1 } else { $lines.Length - 1 }
        $d = @{ name = $heads[$h].name; file = $f.FullName; start = $start; end = $end; qualified = $heads[$h].qualified }
        if (-not $defs.ContainsKey($d.name)) { $defs[$d.name] = @() }
        $defs[$d.name] += $d
    }
}

# ---- pass 1: declarations, attributed to the definition whose range contains them -------------------
$decls = @{}          # name -> list of specs
$declaredDefs = @()   # definitions that carry a declaration: @{def; spec}
$unattributed = @()
foreach ($path in $fileLines.Keys) {
    $lines = $fileLines[$path]
    if (($lines -join "`n").IndexOf('DMS_ENTERS') -lt 0) { continue }
    $rel = $path.Substring($root.Length + 1)
    $inFile = @($defs.Values | ForEach-Object { $_ } | Where-Object { $_.file -eq $path })
    for ($i = 0; $i -lt $lines.Length; $i++) {
        $m = $declRe.Match($lines[$i]); if (-not $m.Success) { continue }
        if ($lines[$i] -match '^\s*#define') { continue }
        $owner = $inFile | Where-Object { $_.start -le $i -and $i -le $_.end } | Select-Object -First 1
        if ($null -eq $owner) { $unattributed += ("  {0}:{1}: {2}" -f $rel, ($i + 1), $lines[$i].Trim()); continue }
        $spec = if ($m.Value -like 'DMS_ENTERS_NOTHING*') { @{ nothing = $true; item = $false; level = [uint32]0xFFFFFFFF; shared = $false } }
                else { @{ nothing = $false; item = [bool]$m.Groups[1].Success; level = $levels[$m.Groups[2].Value]; shared = ($m.Groups[3].Value -eq 'dms_shared_v') } }
        $spec.name = $owner.name; $spec.file = $rel; $spec.line = $i + 1
        if (-not $decls.ContainsKey($owner.name)) { $decls[$owner.name] = @() }
        $decls[$owner.name] += $spec
        $declaredDefs += @{ def = $owner; spec = $spec }
    }
}

# ---- pass 1b: the DMS_CALLEE_ENTERS contracts on function-pointer typedefs, and the variables of those types ----
# `typedef R (CONV *TName DMS_CALLEE_ENTERS(ord_level_type::L, mode))(...)` gives every call through a variable
# declared as `TName v` the ceiling (L, mode): the contract IS the declaration of whatever gets plugged in.
$ptrTypeSpecs = @{}; $ptrVarSpecs = @{}
$typedefRe = [regex]'typedef\s+[^;(]*\(\s*(?:\w+\s+)?\*\s*(\w+)\s+DMS_CALLEE_ENTERS(?:_NOTHING\b|\(\s*ord_level_type::(\w+)\s*,\s*(dms_shared_v|dms_exclusive_v)\s*\))'
foreach ($path in $fileLines.Keys) {
    if (-not $path.EndsWith('.h')) { continue }
    foreach ($m in $typedefRe.Matches(($fileLines[$path] -join "`n"))) {
        $ptrTypeSpecs[$m.Groups[1].Value] = if ($m.Groups[2].Success) { @{ nothing = $false; item = $false; level = $levels[$m.Groups[2].Value]; shared = ($m.Groups[3].Value -eq 'dms_shared_v'); name = $m.Groups[1].Value } }
                                           else { @{ nothing = $true; item = $false; level = [uint32]0xFFFFFFFF; shared = $false; name = $m.Groups[1].Value } }
    }
}
if ($ptrTypeSpecs.Count) {
    $varRe = [regex]('\b(' + (($ptrTypeSpecs.Keys | ForEach-Object { [regex]::Escape($_) }) -join '|') + ')\s+(\w+)\s*(?:[;=,)]|$)')
    foreach ($path in $fileLines.Keys) {
        foreach ($l in $fileLines[$path]) {
            if ($l.IndexOf('T') -lt 0 -and $l.IndexOf('Func') -lt 0) { continue }
            foreach ($m in $varRe.Matches($l)) { $ptrVarSpecs[$m.Groups[2].Value] = $ptrTypeSpecs[$m.Groups[1].Value] }
        }
    }
}

function Get-CalleeSpec([string]$name) {   # a declared, unambiguous callee's spec; 'AMBIGUOUS'; or $null when undeclared
    if ($ptrVarSpecs.ContainsKey($name) -and -not $decls.ContainsKey($name)) { return $ptrVarSpecs[$name] }
    if (-not $decls.ContainsKey($name)) { return $null }
    $ds = $decls[$name]; $first = $ds[0]
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

# ---- pass 2: what each body reaches --------------------------------------------------------------------
# a call by simple name: `f(`, `A::B::f(` (the last identifier), `x.f(`, `p->f(`, and `(*fp)(` for a function pointer
$anyCallRe = [regex]'(?<![\w~])([A-Za-z_]\w*)\s*\(|\(\s*\*\s*([A-Za-z_]\w*)\s*\)\s*\('
# a member/function DECLARATION inside a class body or header: a type, a name, a parameter list and ';' --
# it is not a call, and in a header it sits inside the textual range of the previous inline definition
$declLineRe = [regex]'^\s*(?:(?:virtual|static|inline|explicit|constexpr|friend|RTC_CALL|TIC_CALL|CLC_CALL|GEO_CALL|SHV_CALL|STG_CALL|STX_CALL|extern)\s+)*[\w:<>,\*&~]+\s+[\w~]+\s*\([^;{}]*\)\s*(?:const)?\s*(?:noexcept)?\s*(?:override)?\s*(?:=\s*0)?\s*(?:=\s*default)?\s*;'
$ambiguous = @{}
$reachMemo = @{}   # undeclared name -> list of @{spec; via}   (declared specs reachable through undeclared bodies)

# the names called in a body: name -> @{line; qualified}. qualified = reached through '.' or '->' (a call on some
# other object), which is followed into a DECLARED callee by name but never into an undeclared body.
function Get-CallNames($def) {
    $lines = $fileLines[$def.file]; $names = @{}
    for ($k = $def.start + 1; $k -le $def.end; $k++) {
        $t = $lines[$k]
        if ($t.IndexOf('(') -lt 0 -or $declLineRe.IsMatch($t)) { continue }
        foreach ($cm in $anyCallRe.Matches($t)) {
            $n = if ($cm.Groups[1].Success) { $cm.Groups[1].Value } else { $cm.Groups[2].Value }
            $q = $cm.Index -gt 0 -and ($t[$cm.Index - 1] -eq '.' -or ($cm.Index -gt 1 -and $t.Substring($cm.Index - 2, 2) -eq '->'))
            if (-not $names.ContainsKey($n)) { $names[$n] = @{ line = $k + 1; qualified = $q } }
            elseif (-not $q) { $names[$n].qualified = $false }
        }
    }
    return $names
}

# the declared specs reachable from an UNDECLARED function, following unambiguous undeclared callees
$script:reachCut = $false   # set when a walk is cut short by the cycle guard or the depth limit: such a result is not memoized
function Get-Reach([string]$name, [int]$depth, [System.Collections.Generic.HashSet[string]]$onPath) {
    if ($reachMemo.ContainsKey($name)) { return $reachMemo[$name] }
    $out = @()
    if ($depth -gt $MaxDepth) { $script:reachCut = $true; return $out }
    if (-not $defs.ContainsKey($name)) { return $out }
    # one definition, or an overload set of UNQUALIFIED definitions (free functions, in-class inline members) in at most
    # two files (a header of inline forwarders plus the .cpp, as reportD is): followed as a union. Same-named Class::
    # definitions, or a name defined in more files than that, are different functions of different classes: not followed.
    $bodies = $defs[$name]
    if ($bodies.Count -gt 1) {
        if (@($bodies | Where-Object { $_.qualified }).Count -gt 0) { return $out }
        if (@($bodies | ForEach-Object { $_.file } | Sort-Object -Unique).Count -gt 2) { return $out }
    }
    if (-not $onPath.Add($name)) { $script:reachCut = $true; return $out }
    $cutBefore = $script:reachCut; $script:reachCut = $false
    foreach ($def in $bodies) {
        $calls = Get-CallNames $def
        if ($env:CEIL_TRACE_REACH -and $name -eq $env:CEIL_TRACE_REACH) {
            Write-Host ("trace: body of {0} at {1}:{2}-{3} calls: {4}" -f $name, $def.file.Substring($root.Length + 1), ($def.start + 1), ($def.end + 1), (($calls.Keys | Sort-Object) -join ', '))
        }
        foreach ($n in $calls.Keys) {
            if ($n -eq $name -or $generic -contains $n) { continue }
            $spec = Get-CalleeSpec $n
            if ($spec -is [string]) { $ambiguous[$n] = $true; continue }
            if ($null -ne $spec) { $out += @{ spec = $spec; via = @($n) }; continue }
            if ($calls[$n].qualified) { continue }
            foreach ($r in (Get-Reach $n ($depth + 1) $onPath)) { $out += @{ spec = $r.spec; via = @($n) + $r.via } }
        }
    }
    [void]$onPath.Remove($name)
    if ($env:CEIL_TRACE_REACH -and $name -eq $env:CEIL_TRACE_REACH) {
        Write-Host ("trace: reach of {0} ({1} bodies): {2}{3}" -f $name, $bodies.Count, (($out | ForEach-Object { ($_.via -join '->') + ' ' + (Format-Spec $_.spec) }) -join '; '), $(if ($script:reachCut) { ' [cut]' } else { '' }))
    }
    # a result computed while a cycle or the depth limit cut a branch is incomplete for THIS path only: do not memoize it
    if (-not $script:reachCut) { $reachMemo[$name] = $out }
    $script:reachCut = $cutBefore -or $script:reachCut
    return $out
}

$fails = @(); $checked = 0
foreach ($dd in $declaredDefs) {
    $def = $dd.def; $d = $dd.spec
    $rel = $def.file.Substring($root.Length + 1)
    if ($env:CEIL_TRACE -and $def.name -eq $env:CEIL_TRACE) { Write-Host ("trace: {0} in {1}: header {2}, declaration {3}, body end {4}" -f $def.name, $rel, ($def.start + 1), $d.line, ($def.end + 1)) }
    $calls = Get-CallNames $def
    foreach ($n in $calls.Keys) {
        if ($n -eq $def.name -or $generic -contains $n) { continue }
        $spec = Get-CalleeSpec $n
        if ($spec -is [string]) { $ambiguous[$n] = $true; continue }
        if ($null -ne $spec) {
            $checked++
            if (-not (Test-Admits $d $spec)) { $fails += ("  {0}:{1}: {2} {3} calls {4} {5}" -f $rel, $calls[$n].line, $def.name, (Format-Spec $d), $n, (Format-Spec $spec)) }
            continue
        }
        if ($calls[$n].qualified) { continue }
        $onPath = New-Object 'System.Collections.Generic.HashSet[string]'; [void]$onPath.Add($def.name)
        foreach ($r in (Get-Reach $n 1 $onPath)) {
            $checked++
            if (-not (Test-Admits $d $r.spec)) {
                $via = @($n) + $r.via
                $fails += ("  {0}:{1}: {2} {3} reaches {4} {5} via {6}" -f $rel, $calls[$n].line, $def.name, (Format-Spec $d), $via[-1], (Format-Spec $r.spec), ($via -join ' -> '))
            }
        }
    }
}

$nDecl = ($decls.Values | ForEach-Object { $_ } | Measure-Object).Count
if ($ListDeclared) {
    foreach ($k in ($decls.Keys | Sort-Object)) { Write-Host ("  {0,-48} {1}" -f $k, (($decls[$k] | ForEach-Object { (Format-Spec $_) + ' @' + $_.file + ':' + $_.line }) -join ' | ')) }
}
if ($unattributed.Count) {
    Write-Host ("NOTE: {0} declaration(s) could not be tied to a function definition and were not checked (-ShowUnattributed lists them)." -f $unattributed.Count) -ForegroundColor Yellow
    if ($ShowUnattributed) { $unattributed | ForEach-Object { Write-Host $_ } }
}
if ($ShowAmbiguous -and $ambiguous.Count) {
    Write-Host ("AMBIGUOUS (declared at different levels under one name; not checked): {0}" -f (($ambiguous.Keys | Sort-Object) -join ', ')) -ForegroundColor Yellow
}
$uniqueFails = @($fails | Sort-Object -Unique)
if ($uniqueFails.Count -gt 0) {
    Write-Host ("FAIL: {0} call site(s) where a declared ceiling does not admit what the call reaches ({1} declarations, {2} definitions, {3} reaches checked):" -f $uniqueFails.Count, $nDecl, $defs.Keys.Count, $checked) -ForegroundColor Red
    $uniqueFails | ForEach-Object { Write-Host $_ }
    exit 1
}
Write-Host ("OK: every checked call from a declared function admits what it reaches ({0} declarations, {1} definitions indexed, {2} reaches checked, {3} ambiguous names skipped)." -f $nDecl, $defs.Keys.Count, $checked, $ambiguous.Count) -ForegroundColor Green
exit 0
