@echo off
REM Removes an installed GeoDMS with its own uninstaller, waits until that is done, and says
REM whether the install folder is gone. The .m, .c and .g setup scripts call it before they
REM install (to remove a previous installation of the same version) and when the
REM post-install tests fail (to remove the build they just installed).
REM
REM   %1 = the install folder, such as C:\Program Files\ObjectVision\GeoDms20.23.1.g
REM
REM Exit code: 0 the folder is gone, or was not there; 1 it is still there, and the message
REM lists what it holds.
REM
REM The uninstaller runs from a copy in %TEMP%, not from the folder it removes (#1296).
REM Without _?= an NSIS uninstaller copies itself to TEMP, starts that copy and returns at
REM once, so a caller cannot wait for it; with _?= it runs where it is started. Started from
REM the install folder it cannot delete its own uninstaller.exe, so the folder stays, and the
REM rmdir the setup scripts ran after it failed with "Access is denied" in a console without
REM administrator rights: the uninstaller elevates itself through UAC, the console does not.
REM Run from a copy, the elevated uninstaller deletes uninstaller.exe and the folder itself,
REM and this script deletes only its own copy in TEMP. The uninstaller's exit code is 0 in
REM both cases, so the folder is checked instead.
REM
REM The copy runs elevated from a folder the user can write to, as NSIS's own copy did before
REM NSIS 3.09 (CVE-2023-37378): a process of the same user could replace it between the copy
REM and the UAC prompt. The setup in distr\ is in the same position, but it is signed, so its
REM UAC prompt names the publisher; the uninstaller is not signed.

setlocal
if "%~1"=="" (
    echo *** UninstallGeoDms.bat: no install folder given ***
    exit /b 1
)
set "INSTALL_DIR=%~1"
set "UNINST_COPY=%TEMP%\%~nx1-uninstaller.exe"
if not exist "%INSTALL_DIR%" exit /b 0
if not exist "%INSTALL_DIR%\uninstaller.exe" goto :check

echo --- uninstalling %INSTALL_DIR% ---
copy /y "%INSTALL_DIR%\uninstaller.exe" "%UNINST_COPY%" >nul
if errorlevel 1 (
    echo *** could not copy the uninstaller to %UNINST_COPY% ***
    goto :check
)
REM _?= must be the last argument, unquoted even though the path holds a space.
"%UNINST_COPY%" /S _?=%INSTALL_DIR%
del /q "%UNINST_COPY%" 2>nul

:check
if not exist "%INSTALL_DIR%" (
    echo Removed %INSTALL_DIR%
    exit /b 0
)
echo *** %INSTALL_DIR% is still there, holding: ***
dir /b /s "%INSTALL_DIR%"
echo *** Only a console with administrator rights can remove it: rmdir /s /q "%INSTALL_DIR%" ***
exit /b 1
