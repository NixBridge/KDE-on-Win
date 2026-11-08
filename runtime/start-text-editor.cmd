@echo off
call "%~dp0plasma-common.cmd" :pc_setup_env
"%~dp0bin\plasma-kde-launcher.exe" text-editor %*
