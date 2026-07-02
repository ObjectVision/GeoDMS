# Std-ptr ownership discipline audit (see doc/development/ptr-safety-review-2026-07-02.md and
# doc/development/stdptr-migration-handoff.md "Guardrail is now convention-only").
#
# Check 1 (FAIL): rogue control block -- a std::shared_ptr for a TreeItem-family object constructed
#   directly from a raw pointer. This double-manages a tree-owned object (second control block with a
#   delete deleter -> double-free/UAF at teardown). Every raw must be classified via
#   make_shared_tree(p, newly_obj{}|existing_obj{}|no_zombies{}) instead. The wrapper's "=delete" raw
#   ctor used to catch this at compile time; since the wrappers were removed this grep is the guard.
#
# Check 2 (WARN): check-then-lock race -- "if (!w.expired())" followed by a separate-statement
#   "w.lock()" deref. Nothing pins the target between the check and the re-lock; use one named lock:
#   "if (auto p = w.lock()) { ... p ... }". Heuristic (same member name within 4 lines), so warn-only.
#
# Exit code: 1 if Check 1 finds any site; 0 otherwise.

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$srcDirs = 'rtc','tic','stx','stg','clc','geo','shv','sym','qtgui','python','exe','run' |
    ForEach-Object { Join-Path $root $_ } | Where-Object { Test-Path $_ }

$files = Get-ChildItem $srcDirs -Recurse -File -Include *.h,*.cpp,*.ipp

$fail = 0

# ---- Check 1: rogue std::shared_ptr<family>(raw) ----
$roguePattern = 'std::shared_ptr<(const )?(TreeItem|Abstr\w+|Unit<)[^>]*>\s*\(\s*[a-z_]\w*\s*\)'
$rogue = $files | Select-String -Pattern $roguePattern |
    Where-Object { $_.Line -notmatch 'newly_obj|existing_obj|no_zombies|nullptr' }
if ($rogue) {
    Write-Host "FAIL: rogue std::shared_ptr<TreeItem-family>(raw) construction (use make_shared_tree with a tag):" -ForegroundColor Red
    $rogue | ForEach-Object { Write-Host ("  {0}:{1}: {2}" -f $_.Path.Substring($root.Length + 1), $_.LineNumber, $_.Line.Trim()) }
    $fail = 1
}

# ---- Check 2: expired() guard + separate-statement lock() on the same member ----
$warnCount = 0
foreach ($f in $files) {
    $lines = [System.IO.File]::ReadAllLines($f.FullName)
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $m = [regex]::Match($lines[$i], 'if\s*\(\s*!\s*((?:\w|\.|->)+)\.expired\(\)\s*\)')
        if (-not $m.Success) { continue }
        $member = [regex]::Escape($m.Groups[1].Value)
        $end = [Math]::Min($i + 4, $lines.Count - 1)
        for ($j = $i + 1; $j -le $end; $j++) {
            if ($lines[$j] -match "$member\.lock\(\)") {
                Write-Host ("WARN: check-then-lock race candidate: {0}:{1} (expired() at line {2})" -f `
                    $f.FullName.Substring($root.Length + 1), ($j + 1), ($i + 1)) -ForegroundColor Yellow
                $warnCount++
                break
            }
        }
    }
}

if ($fail -eq 0 -and $warnCount -eq 0) {
    Write-Host "OK: no rogue shared_ptr raw constructions, no check-then-lock candidates." -ForegroundColor Green
} elseif ($fail -eq 0) {
    Write-Host ("OK (with {0} warning(s) to review)." -f $warnCount) -ForegroundColor Yellow
}
exit $fail
