@echo off
setlocal EnableExtensions DisableDelayedExpansion
rem ===========================================================================
rem session-shell.cmd - session host used as the Windows logon shell
rem
rem This is what the registry value
rem   HKCU\Software\Microsoft\Windows NT\CurrentVersion\Winlogon\Shell
rem points at (via cmd.exe /c) after `deploy-vm.cmd install`.
rem
rem It sets up the environment (plasma-common.cmd), starts the session bus
rem and services, then runs plasmashell in the foreground. Closing
rem plasmashell (or this window) ends the session.
rem
rem Location-independent: package root = this script's folder.
rem ===========================================================================

call "%~dp0plasma-common.cmd" :pc_resolve_root

if not exist "%CRAFT_BIN%\plasmashell.exe" (
    echo session-shell: "%CRAFT_BIN%\plasmashell.exe" not found
    exit /b 1
)

call "%~dp0plasma-common.cmd" :pc_setup_env
call "%~dp0plasma-common.cmd" :pc_mirror_data
call "%~dp0plasma-common.cmd" :pc_write_menu
if errorlevel 1 exit /b 1
call "%~dp0plasma-common.cmd" :pc_rebuild_ksycoca

call "%~dp0plasma-common.cmd" :pc_start_bus
if errorlevel 1 exit /b 1
call "%~dp0plasma-common.cmd" :pc_start_services

rem Tray-resident shell switcher: also acts as the kded6/trayhost watchdog
rem (restarts them if they die while a plasma session is up). Single
rem instance - the HKCU Run key covers the explorer-shell case, this
rem covers the plasma-shell case.
if exist "%CRAFT_BIN%\shellswitch.exe" (
    start "plasma-shellswitch" /D "%CRAFT_BIN%" "%CRAFT_BIN%\shellswitch.exe"
)

rem Run the shell in the foreground; keep this script alive as session host.
"%CRAFT_BIN%\plasmashell.exe"
set "PLASMA_EXIT=%errorlevel%"
if exist "%CRAFT_BIN%\plasma-desktop-tools.exe" "%CRAFT_BIN%\plasma-desktop-tools.exe" --stop-if-idle
exit /b %PLASMA_EXIT%
