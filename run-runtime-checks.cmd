@echo off
setlocal EnableExtensions
set "PATH=%~dp0bin;%PATH%"
set "QT_PLUGIN_PATH=%~dp0plugins"
set "QML_IMPORT_PATH=%~dp0qml"
set "QML2_IMPORT_PATH=%~dp0qml"
"%~dp0bin\qml-import-probe.exe" -platform offscreen > "%~dp0runtime-checks.txt" 2>&1
set "QML_EXIT=%ERRORLEVEL%"
"%~dp0bin\desktop-protocol-probe.exe" >> "%~dp0runtime-checks.txt" 2>&1
set "DESKTOP_EXIT=%ERRORLEVEL%"
type "%~dp0runtime-checks.txt"
echo.
pause
if not "%QML_EXIT%"=="0" exit /b %QML_EXIT%
exit /b %DESKTOP_EXIT%
