@echo off
rem =====================================================================
rem Keep .agents\skills a byte-identical copy of .claude\skills.
rem
rem The skills are edited in ONE place, .claude\skills, and written without naming an agent. Claude
rem Code loads them from there; agents that load skills from .agents\skills read the copy. Two
rem sources edited separately drift apart, as CLAUDE.md and AGENTS.md once did.
rem
rem   batch\SyncSkills.bat          mirror .claude\skills onto .agents\skills, then check
rem   batch\SyncSkills.bat /check   compare only: list every difference and exit 1 when there is one
rem
rem A commit that touches .claude\skills runs this first and stages .agents\skills with it.
rem =====================================================================
setlocal
cd /d "%~dp0.."

if /I not "%~1"=="/check" (
  robocopy ".claude\skills" ".agents\skills" /MIR /XD __pycache__ /NJH /NJS /NP /NFL /NDL >nul
  if errorlevel 8 (
    echo SyncSkills: robocopy failed
    exit /b 1
  )
)

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$src = Join-Path (Get-Location) '.claude\skills'; $dst = Join-Path (Get-Location) '.agents\skills';" ^
  "function Files($root) { if (-not (Test-Path $root)) { return @{} }; $h = @{};" ^
  "  Get-ChildItem $root -Recurse -File | Where-Object { $_.FullName -notmatch '\\__pycache__\\' } |" ^
  "  ForEach-Object { $h[$_.FullName.Substring($root.Length + 1)] = (Get-FileHash $_.FullName -Algorithm SHA256).Hash }; $h }" ^
  "$a = Files $src; $b = Files $dst; $bad = 0;" ^
  "foreach ($k in $a.Keys) { if (-not $b.ContainsKey($k)) { Write-Host ('missing in .agents\skills: ' + $k); $bad++ } elseif ($a[$k] -ne $b[$k]) { Write-Host ('differs: ' + $k); $bad++ } }" ^
  "foreach ($k in $b.Keys) { if (-not $a.ContainsKey($k)) { Write-Host ('only in .agents\skills: ' + $k); $bad++ } }" ^
  "if ($bad) { Write-Host ('SyncSkills: .agents\skills differs from .claude\skills in ' + $bad + ' file(s); run batch\SyncSkills.bat'); exit 1 }" ^
  "Write-Host ('SyncSkills: .agents\skills matches .claude\skills (' + $a.Count + ' files)'); exit 0"
exit /b %ERRORLEVEL%
