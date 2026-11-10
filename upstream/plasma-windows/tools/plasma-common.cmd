@echo off
rem ===========================================================================
rem plasma-common.cmd - shared session bootstrap logic (called via call)
rem
rem Provides labels that all session scripts (start-plasma-session.cmd,
rem session-shell.cmd, plasma-shell.cmd, deploy-vm.cmd) use so environment
rem setup, data mirroring, menu generation, dbus and ksycoca handling are
rem implemented exactly once. ASCII only (no localized text) - callers own
rem their own output.
rem
rem Callers must set CRAFT_ROOT before calling (or call :pc_resolve_root).
rem All labels return via exit /b 0 on success.
rem
rem NOTE: no setlocal here - environment changes (PATH, QT_PLUGIN_PATH, ...)
rem must survive back into the caller.
rem
rem Usage from another batch file:
rem   call "%~dp0plasma-common.cmd" :pc_setup_env
rem   call "%~dp0plasma-common.cmd" :pc_mirror_data
rem ===========================================================================

goto %~1 2>nul
goto :eof

rem ---------------------------------------------------------------------------
rem :pc_resolve_root - set CRAFT_ROOT to this script's folder
rem ---------------------------------------------------------------------------
:pc_resolve_root
rem A packaged runtime wins over stale inherited developer environment variables.
if exist "%~dp0bin\plasmashell.exe" set "CRAFT_ROOT=%~dp0"
if not defined CRAFT_ROOT set "CRAFT_ROOT=%~dp0"
for %%P in ("%CRAFT_ROOT%\.") do set "CRAFT_ROOT=%%~fP"
set "CRAFT_BIN=%CRAFT_ROOT%\bin"
set "CRAFT_DATA=%CRAFT_ROOT%\bin\data"
exit /b 0

rem ---------------------------------------------------------------------------
rem :pc_setup_env - PATH, Qt plugins, DBus address, software backend
rem ---------------------------------------------------------------------------
:pc_setup_env
call :pc_resolve_root
set "PATH=%CRAFT_BIN%;%PATH%"
set "QT_PLUGIN_PATH=%CRAFT_ROOT%\plugins"
set "QML_IMPORT_PATH=%CRAFT_ROOT%\qml"
set "QML2_IMPORT_PATH=%CRAFT_ROOT%\qml"
rem NOTE: no XDG_* overrides here on purpose. Qt's QStandardPaths on
rem Windows already resolves GenericDataLocation/GenericConfigLocation to
rem %LOCALAPPDATA% (verified with qtpaths6), so KDE data and configs land
rem there without any environment help. Setting XDG_DATA_HOME/XDG_CONFIG_HOME
rem session-wide would re-route every XDG-aware application (opencode, ...)
rem into %LOCALAPPDATA% as well, breaking data locations and making it
rem impossible to switch between explorer and plasma without restarting apps.
set "DBUS_SESSION_BUS_ADDRESS=tcp:host=127.0.0.1,port=12443"
set "QT_QUICK_BACKEND=software"
exit /b 0

rem ---------------------------------------------------------------------------
rem :pc_mirror_data - mirror KDE package data to %LOCALAPPDATA%
rem (QStandardPaths on Windows resolves GenericDataLocation to LOCALAPPDATA)
rem ---------------------------------------------------------------------------
:pc_mirror_data
call :pc_resolve_root
for %%S in (plasmoids shells wallpapers layout-templates desktoptheme look-and-feel) do (
    if exist "%CRAFT_DATA%\plasma\%%S" (
        if not exist "%LOCALAPPDATA%\plasma\%%S" mkdir "%LOCALAPPDATA%\plasma\%%S"
        for /d %%D in ("%CRAFT_DATA%\plasma\%%S\*") do (
            xcopy /e /i /y "%%D" "%LOCALAPPDATA%\plasma\%%S\%%~nxD" >nul
        )
    )
)
if not exist "%LOCALAPPDATA%\applications" mkdir "%LOCALAPPDATA%\applications"
copy /y "%CRAFT_DATA%\applications\*.desktop" "%LOCALAPPDATA%\applications\" >nul 2>&1
exit /b 0

rem ---------------------------------------------------------------------------
rem :pc_write_menu - write %LOCALAPPDATA%\menus\applications.menu
rem ---------------------------------------------------------------------------
:pc_write_menu
call :pc_resolve_root
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0write-session-menu.ps1"
exit /b %errorlevel%

rem ---------------------------------------------------------------------------
rem :pc_rebuild_ksycoca - rebuild the KService database
rem ---------------------------------------------------------------------------
:pc_rebuild_ksycoca
call :pc_resolve_root
if exist "%~dp0build-startmenu-desktops.ps1" (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-startmenu-desktops.ps1" -Quiet >nul 2>&1
    if errorlevel 1 echo WARNING: Windows Start Menu indexing failed.
)
"%CRAFT_BIN%\kbuildsycoca6.exe" --noincremental >nul 2>&1
exit /b 0

rem ---------------------------------------------------------------------------
rem :pc_start_bus - start the session bus if not already listening (port 12443)
rem ---------------------------------------------------------------------------
:pc_start_bus
call :pc_resolve_root
set "DBUS_CONF=%~dp0dbus-session-plasma.conf"
powershell -NoProfile -Command "if (Get-NetTCPConnection -LocalPort 12443 -State Listen -ErrorAction SilentlyContinue) { exit 0 } else { exit 1 }"
if not errorlevel 1 exit /b 0
if exist "%DBUS_CONF%" (
    call :pc_spawn "%CRAFT_BIN%\dbus-daemon.exe" --config-file="%DBUS_CONF%" --nofork
) else (
    call :pc_spawn "%CRAFT_BIN%\dbus-daemon.exe" --session --nofork
)
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$i = 0; while ($i -lt 50 -and -not (Get-NetTCPConnection -LocalPort 12443 -State Listen -ErrorAction SilentlyContinue)) { Start-Sleep -Milliseconds 200; $i++ }; if (Get-NetTCPConnection -LocalPort 12443 -State Listen -ErrorAction SilentlyContinue) { exit 0 } else { exit 1 }"
exit /b %errorlevel%

rem ---------------------------------------------------------------------------
rem :pc_start_services - kactivitymanagerd + kded6 + trayhost in the background
rem ---------------------------------------------------------------------------
:pc_start_services
call :pc_resolve_root
if exist "%CRAFT_BIN%\plasma-desktop-tools.exe" call :pc_spawn "%CRAFT_BIN%\plasma-desktop-tools.exe" --daemon
if exist "%CRAFT_ROOT%\install-file-associations.ps1" (
    call :pc_spawn powershell.exe -NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File "%CRAFT_ROOT%\install-file-associations.ps1" -CraftRoot "%CRAFT_ROOT%" -IfNeeded
)
if exist "%CRAFT_BIN%\plasma-personalization.exe" (
    call :pc_spawn "%CRAFT_BIN%\plasma-personalization.exe"
)
if exist "%CRAFT_BIN%\plasma-window-decorations.exe" (
    call :pc_spawn "%CRAFT_BIN%\plasma-window-decorations.exe"
)
if exist "%CRAFT_ROOT%\start-notifications.ps1" (
    call :pc_spawn powershell.exe -NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File "%CRAFT_ROOT%\start-notifications.ps1"
)
if exist "%CRAFT_ROOT%\repair-taskbar-pins.ps1" (
    call :pc_spawn powershell.exe -NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File "%CRAFT_ROOT%\repair-taskbar-pins.ps1" -CraftRoot "%CRAFT_ROOT%"
)
call :pc_spawn "%CRAFT_BIN%\kactivitymanagerd.exe"
call :pc_spawn "%CRAFT_BIN%\kded6.exe"
rem Windows tray host (SNI bridge into the Plasma system tray); its log
rem lands next to the exe (working dir), i.e. %CRAFT_BIN%\trayhost.log.
if exist "%CRAFT_BIN%\trayhost.exe" (
    pushd "%CRAFT_BIN%"
    call :pc_spawn "%CRAFT_BIN%\trayhost.exe"
    popd
)
timeout /t 3 /nobreak >nul
exit /b 0

rem Native background spawning: no console windows; stdout/stderr go to user logs.
:pc_spawn
if not exist "%CRAFT_BIN%\kde-session-host.exe" goto :pc_spawn_legacy
"%CRAFT_BIN%\kde-session-host.exe" --spawn-hidden %*
exit /b %errorlevel%
:pc_spawn_legacy
start "" %*
exit /b 0

rem Foreground desktop output is otherwise absent from hidden-service logs.
:pc_run_plasma
set "PLASMA_LOG_DIR=%LOCALAPPDATA%\plasma-win-session\logs"
if defined KDE_ON_WIN_SETTINGS for %%P in ("%KDE_ON_WIN_SETTINGS%") do set "PLASMA_LOG_DIR=%%~dpPlogs"
if not exist "%PLASMA_LOG_DIR%" mkdir "%PLASMA_LOG_DIR%" 2>nul
set "PLASMA_LOG=%PLASMA_LOG_DIR%\plasmashell.exe.log"
if not exist "%PLASMA_LOG_DIR%" goto :pc_run_plasma_console
type nul >>"%PLASMA_LOG%" 2>nul || goto :pc_run_plasma_console
echo Plasma output: "%PLASMA_LOG%"
"%CRAFT_BIN%\plasmashell.exe" >>"%PLASMA_LOG%" 2>&1
set "PLASMA_RUN_EXIT=%errorlevel%"
if "%PLASMA_RUN_EXIT%"=="0" exit /b 0
echo ERROR: plasmashell exited with code %PLASMA_RUN_EXIT%.
type "%PLASMA_LOG%"
exit /b %PLASMA_RUN_EXIT%
:pc_run_plasma_console
echo WARNING: Cannot write Plasma log. Keeping output in this console.
"%CRAFT_BIN%\plasmashell.exe"
exit /b %errorlevel%
