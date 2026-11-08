@echo off
call "%~dp0msvc-env.cmd"
if errorlevel 1 exit /b 1
ninja -C D:\_\ed4c8194\build -j 8 plasmashell
exit /b %errorlevel%
