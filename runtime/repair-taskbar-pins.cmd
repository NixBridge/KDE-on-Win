@echo off
setlocal EnableExtensions
call "%~dp0plasma-common.cmd" :pc_setup_env
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0repair-taskbar-pins.ps1" -CraftRoot "%CRAFT_ROOT%" -WaitSeconds 3
set "REPAIR_EXIT=%ERRORLEVEL%"
pause
exit /b %REPAIR_EXIT%
