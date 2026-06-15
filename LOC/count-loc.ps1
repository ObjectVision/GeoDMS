<#
.SYNOPSIS
  Count lines of code in the GeoDMS workspace, grouped by file extension, and write a
  dated Markdown report into this LOC folder (one file per run, so numbers can be
  tracked over time).

.DESCRIPTION
  Two metrics per extension:
    LOC  = total physical lines in the file.
    SLOC = "source" lines: non-blank AND not pure-comment lines. Comment syntax is
           recognized per language (see $cfg below). SLOC is a heuristic - it does not
           parse strings, so a comment marker inside a string literal (e.g. "http://")
           can occasionally be miscounted. Good enough for trend tracking.

  Third-party / generated / build-artifact trees are excluded (vcpkg*, bin, obj, build,
  .vs, .git, and this LOC folder itself).

.EXAMPLE
  powershell -File <repo-root>\LOC\count-loc.ps1
  # writes LOC\loc-<yyyy-MM-dd>.md and prints the table.

.PARAMETER Root
  Workspace root to scan. Defaults to the parent of this script's folder.
#>
[CmdletBinding()]
param(
    [string]$Root = (Split-Path -Parent $PSScriptRoot)
)

# Directories excluded from the count.
$excludeRegex = '\\(vcpkg|vcpkg_installed|vc_archives|vc_downloads|vcpkg-triplets|\.vs|\.git|build|LOC)\\|\\(bin|obj)\\'

# Comment configuration per extension: line token + block open/close (null = none).
$cfg = @{}
$cStyle = '.cpp','.h','.hpp','.c','.cc','.cxx','.hxx','.inl','.ipp','.cs','.rc','.def','.idl'
foreach ($e in $cStyle)                                   { $cfg[$e] = @{ line='//';  bo='/*';   bc='*/'  } }
foreach ($e in '.py','.sh','.cmake','.yml','.yaml')       { $cfg[$e] = @{ line='#';   bo=$null;  bc=$null } }
foreach ($e in '.ps1','.psm1')                            { $cfg[$e] = @{ line='#';   bo='<#';   bc='#>'  } }
foreach ($e in '.bat','.cmd')                             { $cfg[$e] = @{ line='REM'; bo=$null;  bc=$null } }  # also '::'
foreach ($e in '.xml','.props','.targets','.vcxproj','.filters') { $cfg[$e] = @{ line=$null; bo='<!--'; bc='-->' } }
foreach ($e in '.json','.sln','.md','.txt')               { $cfg[$e] = @{ line=$null; bo=$null;  bc=$null } }

$codeExt = $cfg.Keys

function Get-FileCounts {
    param([string]$Path, [hashtable]$C)
    $loc = 0; $sloc = 0; $inBlock = $false
    try { $lines = [System.IO.File]::ReadAllLines($Path) } catch { return @{ LOC = 0; SLOC = 0 } }
    foreach ($raw in $lines) {
        $loc++
        $t = $raw.Trim()
        if ($t.Length -eq 0) { continue }                 # blank line

        # Batch: whole-line comments only (REM ... or :: ...).
        if ($C.line -eq 'REM') {
            if ($t -notmatch '^(rem\b|::)') { $sloc++ }
            continue
        }

        $code = $false; $i = 0; $n = $t.Length
        while ($i -lt $n) {
            if ($inBlock) {
                if ($C.bc) {
                    $end = $t.IndexOf($C.bc, $i)
                    if ($end -lt 0) { $i = $n } else { $inBlock = $false; $i = $end + $C.bc.Length }
                } else { $i = $n }
            } else {
                $lpos = if ($C.line) { $t.IndexOf($C.line, $i) } else { -1 }
                $bpos = if ($C.bo)   { $t.IndexOf($C.bo,   $i) } else { -1 }
                if ($lpos -ge 0 -and ($bpos -lt 0 -or $lpos -le $bpos)) {
                    if ($t.Substring($i, $lpos - $i).Trim().Length -gt 0) { $code = $true }
                    $i = $n
                } elseif ($bpos -ge 0) {
                    if ($t.Substring($i, $bpos - $i).Trim().Length -gt 0) { $code = $true }
                    $inBlock = $true; $i = $bpos + $C.bo.Length
                } else {
                    if ($t.Substring($i).Trim().Length -gt 0) { $code = $true }
                    $i = $n
                }
            }
        }
        if ($code) { $sloc++ }
    }
    return @{ LOC = $loc; SLOC = $sloc }
}

$files = Get-ChildItem $Root -Recurse -File -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -notmatch $excludeRegex -and $codeExt -contains $_.Extension.ToLower() }

$agg = @{}
foreach ($f in $files) {
    $ext = $f.Extension.ToLower()
    $c = Get-FileCounts -Path $f.FullName -C $cfg[$ext]
    if (-not $agg.ContainsKey($ext)) { $agg[$ext] = [pscustomobject]@{ Ext = $ext; Files = 0; LOC = 0; SLOC = 0 } }
    $agg[$ext].Files++; $agg[$ext].LOC += $c.LOC; $agg[$ext].SLOC += $c.SLOC
}

$rows = $agg.Values | Sort-Object LOC -Descending
$tFiles = [int]($rows | Measure-Object Files -Sum).Sum
$tLOC   = [int]($rows | Measure-Object LOC   -Sum).Sum
$tSLOC  = [int]($rows | Measure-Object SLOC  -Sum).Sum

# "C++ core" = .cpp + .h + .hpp + .ipp
$cppExt   = '.cpp','.h','.hpp','.ipp'
$cppRows  = $rows | Where-Object { $cppExt -contains $_.Ext }
$cppFiles = [int]($cppRows | Measure-Object Files -Sum).Sum
$cppLOC   = [int]($cppRows | Measure-Object LOC   -Sum).Sum
$cppSLOC  = [int]($cppRows | Measure-Object SLOC  -Sum).Sum

# ---- Build Markdown ----
$date = Get-Date -Format 'yyyy-MM-dd'
$nl = [Environment]::NewLine
$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("# $(Split-Path -Leaf $Root) - Lines of Code")
$lines.Add("")
$lines.Add("_Generated: $date_")
$lines.Add("_Workspace: $Root (excludes vcpkg trees, bin/obj/build, .vs, .git, LOC)_")
$lines.Add("")
$lines.Add("| Extension | Files | LOC | SLOC |")
$lines.Add("|---|---:|---:|---:|")
foreach ($r in $rows) {
    $lines.Add(('| `{0}` | {1:N0} | {2:N0} | {3:N0} |' -f $r.Ext, $r.Files, $r.LOC, $r.SLOC))
}
$lines.Add(('| **TOTAL** | **{0:N0}** | **{1:N0}** | **{2:N0}** |' -f $tFiles, $tLOC, $tSLOC))
$lines.Add("")
$lines.Add(("**C++ core** (.cpp + .h + .hpp + .ipp): {0:N0} files, {1:N0} LOC, {2:N0} SLOC." -f $cppFiles, $cppLOC, $cppSLOC))
$lines.Add("")
$lines.Add("**LOC** = total physical lines. **SLOC** = non-blank, non-comment lines (comment syntax recognized per language; heuristic - does not parse string literals).")

$outFile = Join-Path $PSScriptRoot "loc-$date.md"
Set-Content -Path $outFile -Value ($lines -join $nl) -Encoding utf8

# ---- Append/refresh history.csv (one row per date; re-running on the same date replaces that row) ----
$histFile = Join-Path $PSScriptRoot 'history.csv'
$header = 'Date,Files,LOC,SLOC,LOC_Cpp,SLOC_Cpp'
$record = '{0},{1},{2},{3},{4},{5}' -f $date, $tFiles, $tLOC, $tSLOC, $cppLOC, $cppSLOC
$hist = New-Object System.Collections.Generic.List[string]
$hist.Add($header)
if (Test-Path $histFile) {
    foreach ($l in (Get-Content $histFile | Select-Object -Skip 1)) {
        if ($l.Trim().Length -eq 0) { continue }
        if ($l.Split(',')[0] -ne $date) { $hist.Add($l) }   # keep prior dates, drop today's old row
    }
}
$hist.Add($record)
Set-Content -Path $histFile -Value ($hist -join $nl) -Encoding utf8

# Console summary
$rows | Format-Table @{N='Ext';E={$_.Ext}}, @{N='Files';E={'{0:N0}' -f $_.Files}}, @{N='LOC';E={'{0:N0}' -f $_.LOC}}, @{N='SLOC';E={'{0:N0}' -f $_.SLOC}} -AutoSize
"TOTAL:    {0:N0} LOC / {1:N0} SLOC across {2:N0} files" -f $tLOC, $tSLOC, $tFiles
"C++ core: {0:N0} LOC / {1:N0} SLOC across {2:N0} files" -f $cppLOC, $cppSLOC, $cppFiles
"Report written to: $outFile"
"History updated:   $histFile"
