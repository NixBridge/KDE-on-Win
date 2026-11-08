@echo off
setlocal EnableExtensions
set "PATH=%~dp0bin;%PATH%"
set "QT_PLUGIN_PATH=%~dp0plugins"
set "QML_IMPORT_PATH=%~dp0qml"
set "QML2_IMPORT_PATH=%~dp0qml"
set "DBUS_SESSION_BUS_ADDRESS=tcp:host=127.0.0.1,port=12443"
"%~dp0bin\qml-import-probe.exe" -platform offscreen > "%~dp0runtime-checks.txt" 2>&1
set "QML_EXIT=%ERRORLEVEL%"
"%~dp0bin\desktop-protocol-probe.exe" >> "%~dp0runtime-checks.txt" 2>&1
set "DESKTOP_EXIT=%ERRORLEVEL%"
"%~dp0bin\breeze-mapping-probe.exe" -platform offscreen >> "%~dp0runtime-checks.txt" 2>&1
set "ICON_EXIT=%ERRORLEVEL%"
"%~dp0bin\taskbar-icon-probe.exe" -platform offscreen >> "%~dp0runtime-checks.txt" 2>&1
set "TASKBAR_EXIT=%ERRORLEVEL%"
"%~dp0bin\notification-relay-probe.exe" >> "%~dp0runtime-checks.txt" 2>&1
set "NOTIFY_EXIT=%ERRORLEVEL%"
"%~dp0bin\plasma-window-decorations.exe" --self-test -platform offscreen >> "%~dp0runtime-checks.txt" 2>&1
set "DECORATION_EXIT=%ERRORLEVEL%"
"%~dp0bin\plasma-text-editor.exe" --self-test -platform offscreen >> "%~dp0runtime-checks.txt" 2>&1
set "EDITOR_EXIT=%ERRORLEVEL%"
"%~dp0bin\context-menu-probe.exe" >> "%~dp0runtime-checks.txt" 2>&1
set "MENU_EXIT=%ERRORLEVEL%"
type "%~dp0runtime-checks.txt"
echo.
pause
if not "%QML_EXIT%"=="0" exit /b %QML_EXIT%
if not "%DESKTOP_EXIT%"=="0" exit /b %DESKTOP_EXIT%
if not "%ICON_EXIT%"=="0" exit /b %ICON_EXIT%
if not "%TASKBAR_EXIT%"=="0" exit /b %TASKBAR_EXIT%
if not "%NOTIFY_EXIT%"=="0" exit /b %NOTIFY_EXIT%
if not "%DECORATION_EXIT%"=="0" exit /b %DECORATION_EXIT%
if not "%EDITOR_EXIT%"=="0" exit /b %EDITOR_EXIT%
exit /b %MENU_EXIT%
