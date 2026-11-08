@echo off
call "%~dp0plasma-common.cmd" :pc_setup_env
start "plasma-window-decorations" "%~dp0bin\plasma-window-decorations.exe"
