@echo off
setlocal EnableExtensions
set "PATH=%~dp0bin;%PATH%"
set "QT_PLUGIN_PATH=%~dp0plugins"
set "QML_IMPORT_PATH=%~dp0qml"
set "QML2_IMPORT_PATH=%~dp0qml"
"%~dp0bin\qml-import-probe.exe" -platform offscreen > "%~dp0qml-import-result.txt" 2>&1
set "PROBE_EXIT=%ERRORLEVEL%"
type "%~dp0qml-import-result.txt"
echo.
pause
exit /b %PROBE_EXIT%
