@echo on
REM Body of the spawn — sets up VS dev environment so msbuild is on PATH,
REM then runs the interactive BuildSignAndCreateSetup.bat for the .m flavor.
REM Path nested inside this .bat carries no extra quoting (no \" needed).
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
cd /d C:\dev\GeoDMS_2026
call .\BuildSignAndCreateSetup.bat
pause
