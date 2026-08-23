@echo off
setlocal EnableExtensions

rem Cleanup old Microsoft symbol-cache entries.
rem Cache layout assumed:
rem   C:\dev\SymbolCache\<name>.pdb\<GUID+Age>\<name>.pdb
rem
rem For every *.pdb directory that contains multiple version directories,
rem keep the version directory with the newest LastWriteTimeUtc and delete
rem the remaining version directories.
rem
rem Usage:
rem   CleanupOldSymbols.bat
rem   CleanupOldSymbols.bat /dryrun

set "SYMBOL_CACHE=C:\dev\SymbolCache"
set "DRYRUN=0"

if /I "%~1"=="/dryrun" set "DRYRUN=1"

if not exist "%SYMBOL_CACHE%\" (
    echo Symbol cache not found: "%SYMBOL_CACHE%"
    exit /b 1
)

echo Symbol cache: "%SYMBOL_CACHE%"
if "%DRYRUN%"=="1" (
    echo DRY RUN - nothing will be deleted.
) else (
    echo Deleting older cached PDB versions...
)
echo.

powershell.exe -NoProfile -ExecutionPolicy Bypass -Command ^
  "$ErrorActionPreference = 'Stop';" ^
  "$root = $env:SYMBOL_CACHE;" ^
  "$dryRun = ($env:DRYRUN -eq '1');" ^
  "$deletedDirs = 0;" ^
  "$freedBytes = [int64]0;" ^
  "Get-ChildItem -LiteralPath $root -Directory | Where-Object { $_.Name -like '*.pdb' } | ForEach-Object {" ^
  "  $pdbDir = $_;" ^
  "  $versions = @(Get-ChildItem -LiteralPath $pdbDir.FullName -Directory | Sort-Object LastWriteTimeUtc -Descending);" ^
  "  if ($versions.Count -gt 1) {" ^
  "    $keep = $versions[0];" ^
  "    Write-Host ($pdbDir.Name + ':');" ^
  "    Write-Host ('  keep   ' + $keep.Name);" ^
  "    foreach ($old in ($versions | Select-Object -Skip 1)) {" ^
  "      $bytes = [int64](Get-ChildItem -LiteralPath $old.FullName -File -Recurse -Force -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum;" ^
  "      if ($dryRun) {" ^
  "        Write-Host ('  would delete ' + $old.Name);" ^
  "      } else {" ^
  "        Write-Host ('  delete ' + $old.Name);" ^
  "        Remove-Item -LiteralPath $old.FullName -Recurse -Force;" ^
  "      }" ^
  "      $deletedDirs++;" ^
  "      $freedBytes += $bytes;" ^
  "    }" ^
  "  }" ^
  "};" ^
  "$mb = [math]::Round($freedBytes / 1MB, 1);" ^
  "if ($dryRun) {" ^
  "  Write-Host ''; Write-Host ('Would delete {0} version directories, about {1} MB.' -f $deletedDirs, $mb);" ^
  "} else {" ^
  "  Write-Host ''; Write-Host ('Deleted {0} version directories, about {1} MB.' -f $deletedDirs, $mb);" ^
  "}"

if errorlevel 1 (
    echo.
    echo Cleanup failed.
    exit /b 1
)

exit /b 0
