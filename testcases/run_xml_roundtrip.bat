@echo off
rem =====================================================================
rem Run the XML round-trip battery over testcases\*.dms: dump each configuration in DMS
rem syntax and in XML, read the XML back, dump that in DMS syntax too, and compare.
rem
rem Usage:  run_xml_roundtrip.bat [path\to\GeoDmsRun.exe]
rem Default exe: ..\bin\Release\x64\GeoDmsRun.exe (relative to this script).
rem Exit code: 0 if every configuration matched or is listed in xml_roundtrip_known_diff.txt.
rem =====================================================================
setlocal
set "EXE=%~1"
REM Default exe: repo layout (testcases\ beside bin\), else the INSTALLED layout
REM (this suite ships as <install>\examples\testcases, two levels below GeoDmsRun.exe).
if "%EXE%"=="" set "EXE=%~dp0..\bin\Release\x64\GeoDmsRun.exe"
if not exist "%EXE%" if exist "%~dp0..\..\GeoDmsRun.exe" set "EXE=%~dp0..\..\GeoDmsRun.exe"
if not exist "%EXE%" (
  echo ERROR: GeoDmsRun.exe not found at "%EXE%".
  echo Build the Release configuration first, or pass the exe path as the first argument.
  exit /b 2
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0run_xml_roundtrip.ps1" -Exe "%EXE%"
exit /b %ERRORLEVEL%
