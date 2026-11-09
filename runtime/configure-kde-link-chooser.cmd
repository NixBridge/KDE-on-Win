@echo off
call "%~dp0plasma-common.cmd" :pc_setup_env
start "KDE link chooser" "%~dp0bin\plasma-open-with.exe" --configure-defaults
