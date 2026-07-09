@echo off
REM ============================================================================
REM  analyze.bat -- run MSVC /analyze (PREfast) native Code Analysis over the
REM  whole solution and collect the warnings.
REM
REM  PREfast is OFF in the normal build props (EnablePREfast=false in
REM  DmsDef.props) so day-to-day builds stay fast. This script forces analysis
REM  ON via command-line overrides for a periodic, dedicated pass -- run it
REM  yourself when you want the static-analysis signal (or wire it into Build.bat).
REM
REM  Usage:   analyze.bat [Debug^|Release]        (default: Debug)
REM
REM  Notes:
REM   * It does a full  -t:Rebuild : /analyze only runs on files that actually
REM     (re)compile, so an incremental build over an up-to-date tree would
REM     analyze NOTHING. Expect roughly one full rebuild + ~1.5-3x (analysis).
REM   * It reuses the chosen config's obj\ / bin\ tree. If you develop (F5) in
REM     Debug and want to keep that incremental state, run  analyze.bat Release .
REM   * All warnings are written to  analyze_<cfg>.warnings.log  (warnings only);
REM     full progress stays on the console. To also keep the full transcript:
REM       ^& cmd /c "analyze.bat 2^>^&1" ^| Tee-Object -FilePath analyze.log
REM ============================================================================
setlocal
cd /d "%~dp0"

set "CFG=%~1"
if "%CFG%"=="" set "CFG=Debug"

set "MSBUILD=C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe"
if not exist "%MSBUILD%" (
  echo ERROR: VS18 MSBuild not found at "%MSBUILD%".
  echo Adjust the MSBUILD path in analyze.bat if your VS18 install differs.
  exit /B 1
)

set "WLOG=analyze_%CFG%.warnings.log"
echo === MSVC Code Analysis ^(/analyze, PREfast^) on all22.sln [%CFG% ^| x64] ===
echo Forcing EnablePREfast=true + RunCodeAnalysis=true ^(so findings surface as warnings^).
echo Warnings -^> %WLOG%   ^(this is a full rebuild; it will take a while^)
echo.

"%MSBUILD%" all22.sln -t:Rebuild -m ^
  -p:Configuration=%CFG% -p:Platform=x64 ^
  -p:EnablePREfast=true -p:RunCodeAnalysis=true ^
  -flp:LogFile=%WLOG%;WarningsOnly;Verbosity=normal ^
  -clp:Summary
set "RC=%ERRORLEVEL%"

echo.
echo === native Code-Analysis warnings ^(C6xxx / C26xxx / C28xxx^) ===
findstr /R /C:"warning C6" /C:"warning C26" /C:"warning C28" "%WLOG%" 2>nul
echo.
echo Full warnings log: %WLOG%   ^(msbuild exit code %RC%^)
endlocal ^& exit /B %RC%
