@echo off
setlocal EnableExtensions DisableDelayedExpansion
rem ===========================================================================
rem deploy-vm.cmd - one-shot deployment + session start inside the test VM
rem
rem Location-independent: the CraftRoot runtime must sit in the same folder
rem as this script (extract plasma-vm.zip next to it, or copy the whole
rem package). There is no hard-coded drive/path.
rem
rem Usage:
rem   deploy-vm.cmd             start the session (dbus + kactivitymanagerd
rem                             + kded6 + plasmashell) without touching the
rem                             shell configuration
rem   deploy-vm.cmd install     switch the current user's default shell to
rem                             plasmashell.exe (backup first), then start
rem                             the session
rem   deploy-vm.cmd restore     restore the previous default shell and exit
rem
rem The script:
rem   1. sets up the environment (see plasma-common.cmd)
rem   2. mirrors KDE package data to %LOCALAPPDATA%
rem   3. writes %LOCALAPPDATA%\menus\applications.menu
rem   4. rebuilds the ksycoca service database
rem   5. (optional) switches the user shell to plasmashell
rem   6. starts the session: dbus-daemon + kactivitymanagerd + kded6
rem      + plasmashell
rem
rem Only the current user's Winlogon\Shell value is touched; system
rem binaries and Winlogon itself are never modified. The previous value
rem is saved to %USERPROFILE%\.plasma-windows\shell-backup.txt.
rem ===========================================================================

rem Package root = this script's folder (no hard-coded path).
call "%~dp0plasma-common.cmd" :pc_resolve_root
set "SHELL_KEY=HKCU\Software\Microsoft\Windows NT\CurrentVersion\Winlogon"
set "SHELL_BACKUP=%USERPROFILE%\.plasma-windows\shell-backup.txt"
set "PLASMA_SHELL=%CRAFT_BIN%\plasmashell.exe"
set "SESSION_LAUNCHER=%CRAFT_ROOT%\session-shell.cmd"
set "ACTION=%~1"
if not "%~2"=="" exit /b 2
if not "%ACTION%"=="" if /i not "%ACTION%"=="install" if /i not "%ACTION%"=="restore" exit /b 2

if /i "%ACTION%"=="restore" goto :restore_shell

if not exist "%CRAFT_BIN%\plasmashell.exe" (
    echo ERROR: "%CRAFT_BIN%\plasmashell.exe" not found.
    echo The CraftRoot runtime must sit in the same folder as this script.
    echo Extract plasma-vm.zip next to deploy-vm.cmd first.
    pause
    exit /b 1
)

if /i "%ACTION%"=="install" goto :install_shell

:session
echo === 1/5 Environment ===
call "%~dp0plasma-common.cmd" :pc_setup_env

echo === 2/5 Mirror KDE package data to %%LOCALAPPDATA%% ===
call "%~dp0plasma-common.cmd" :pc_mirror_data

echo === 3/5 applications.menu ===
call "%~dp0plasma-common.cmd" :pc_write_menu
if errorlevel 1 exit /b 1

echo === 4/5 Rebuild ksycoca ===
call "%~dp0plasma-common.cmd" :pc_rebuild_ksycoca

echo === 5/5 Start session (dbus + kactivitymanagerd + kded6 + plasmashell) ===
call "%~dp0plasma-common.cmd" :pc_start_bus
if errorlevel 1 exit /b 1
call "%~dp0plasma-common.cmd" :pc_start_services

echo Starting plasmashell (close this window to end the session)...
"%CRAFT_BIN%\plasmashell.exe"
set "PLASMA_EXIT=%errorlevel%"
if exist "%CRAFT_BIN%\plasma-desktop-tools.exe" "%CRAFT_BIN%\plasma-desktop-tools.exe" --stop-if-idle
echo plasmashell exited with code %PLASMA_EXIT%
endlocal & exit /b %PLASMA_EXIT%

rem ---------------------------------------------------------------------------
rem install: back up the current user shell and switch it to plasmashell.
rem ---------------------------------------------------------------------------
:install_shell
echo === Switching user shell to plasmashell ===
if not exist "%USERPROFILE%\.plasma-windows" mkdir "%USERPROFILE%\.plasma-windows"

set "SHELL_KEY_PS=HKCU:\Software\Microsoft\Windows NT\CurrentVersion\Winlogon"
set "SHELL_VALUE="
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0shell-registry.ps1" -Action install
if errorlevel 1 exit /b 1

echo User shell configured. Native session host is preferred when available.
echo Takes effect at next logon/restart. Starting the session now for testing...
goto :session

rem ---------------------------------------------------------------------------
rem restore: put the backed-up shell value back (explorer.exe if no backup).
rem ---------------------------------------------------------------------------
:restore_shell
echo === Restoring previous user shell ===
set "SHELL_KEY_PS=HKCU:\Software\Microsoft\Windows NT\CurrentVersion\Winlogon"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0shell-registry.ps1" -Action restore
if errorlevel 1 exit /b 1
echo User shell restored. Takes effect at next logon/restart.
endlocal
exit /b 0
