@echo off
setlocal
set "PATH=%~dp0bin;%PATH%"
set "QT_PLUGIN_PATH=%~dp0plugins"
"%~dp0bin\kwindowsystem-probe.exe"
if errorlevel 1 exit /b 1
if not exist "%~dp0bin\kws-probe-result.txt" exit /b 1
type "%~dp0bin\kws-probe-result.txt"
findstr /B /C:"FAIL:" "%~dp0bin\kws-probe-result.txt" >nul
if not errorlevel 1 exit /b 1
exit /b 0
