@echo off
REM Launches run_setup_m.bat in a new visible cmd window so the user can
REM answer the CHOICE prompts and the SafeNet PIN dialog interactively.
REM Avoids nested-quote escapes by delegating all logic to run_setup_m.bat.
start "" cmd /k C:\dev\GeoDMS_2026\.claude\run_setup_m.bat
