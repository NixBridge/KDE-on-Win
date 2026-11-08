@echo off
setlocal EnableExtensions
call "%~dp0plasma-common.cmd" :pc_setup_env
"%CRAFT_BIN%\plasma-kde-launcher.exe" process-monitor %*
exit /b %ERRORLEVEL%
