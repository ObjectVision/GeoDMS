@echo off
setlocal

set geodms_rootdir=C:\dev\GeoDMS_2026
set TstDir=C:\dev\tst
set LocalDataDir=C:\Users\MaartenHilferink\Objectvision\Object Vision - General\LocalData
set ResultDir=%LocalDataDir%\GeoDMSTestResults
set ResultFileName=%ResultDir%\unit\gui_test_run.txt

set version=%1
if "%version%"=="" set version=CD64

if "%version%"=="CD64" set GeoDmsPath=%geodms_rootdir%\build\windows-x64-debug\bin
if "%version%"=="CR64" set GeoDmsPath=%geodms_rootdir%\build\windows-x64-release\bin

set GeoDmsQtCmdBase="%GeoDmsPath%\GeoDmsGuiQt.exe"
set GeoDmsRunCmdBase="%GeoDmsPath%\GeoDmsRun.exe"

if not exist "%ResultDir%\unit\gui" mkdir "%ResultDir%\unit\gui"
del "%ResultFileName%" 2>nul
echo GUI Test Run for %version% > "%ResultFileName%"

echo ===============================================
echo Running DPGeneral_missing_file_error (GUI part)
echo ===============================================
set ResultFolder=%LocalDataDir%\GeoDMSTestResults\Unit\GUI
if not exist "%ResultFolder%" mkdir "%ResultFolder%"
del "%ResultFolder%\DPGeneral_missing_file_error.tmp" 2>nul

set command=%GeoDmsQtCmdBase% /T%TstDir%\dmsscript\DPGeneral_missing_file_error.dmsscript /S1 /S2 /S3 %TstDir%\Unit\GUI\cfg\DPGeneral_missing_file_error.dms test_log
echo Command: %command%
%command%
echo GUI exit code: %ERRORLEVEL%

echo ===============================================
echo Running DPGeneral_missing_file_error (compare)
echo ===============================================
set cmd2=%GeoDmsRunCmdBase% /S1 /S2 /S3 "%TstDir%\Unit\GUI\cfg\DPGeneral_missing_file_error.dms" test_log
echo Command: %cmd2%
%cmd2%
if %ERRORLEVEL% EQU 0 (
    echo PASS: DPGeneral_missing_file_error
    echo PASS: DPGeneral_missing_file_error >> "%ResultFileName%"
    if exist "%ResultDir%\unit\gui\DPGeneral_MF_error.txt" (
        FOR /F "usebackq tokens=* delims=" %%x in ("%ResultDir%\unit\gui\DPGeneral_MF_error.txt") DO echo %%x
    )
) else (
    echo FAIL: DPGeneral_missing_file_error
    echo FAIL: DPGeneral_missing_file_error >> "%ResultFileName%"
)

echo.
echo ===============================================
echo Running DPGeneral_explicit_supplier_error (GUI)
echo ===============================================
del "%ResultFolder%\DPGeneral_explicit_supplier_error.tmp" 2>nul

set command=%GeoDmsQtCmdBase% /T%TstDir%\dmsscript\DPGeneral_explicit_supplier_error.dmsscript /S1 /S2 /S3 %TstDir%\Unit\GUI\cfg\DPGeneral_explicit_supplier_error.dms test_log %ResultDir%\unit\gui\DPGeneral_ES_error.txt
echo Command: %command%
%command%
echo GUI exit code: %ERRORLEVEL%

echo ===============================================
echo Running DPGeneral_explicit_supplier_error (compare)
echo ===============================================
set cmd2=%GeoDmsRunCmdBase% /S1 /S2 /S3 "%TstDir%\Unit\GUI\cfg\DPGeneral_explicit_supplier_error.dms" test_log
echo Command: %cmd2%
%cmd2%
if %ERRORLEVEL% EQU 0 (
    echo PASS: DPGeneral_explicit_supplier_error
    echo PASS: DPGeneral_explicit_supplier_error >> "%ResultFileName%"
    if exist "%ResultDir%\unit\gui\DPGeneral_ES_error.txt" (
        FOR /F "usebackq tokens=* delims=" %%x in ("%ResultDir%\unit\gui\DPGeneral_ES_error.txt") DO echo %%x
    )
) else (
    echo FAIL: DPGeneral_explicit_supplier_error
    echo FAIL: DPGeneral_explicit_supplier_error >> "%ResultFileName%"
)

echo.
echo === Done ===
type "%ResultFileName%"
endlocal
