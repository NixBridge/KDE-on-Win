@echo off
call "%~dp0msvc-env.cmd"
if errorlevel 1 exit /b 1
ninja -C D:\_\13863b6b\build -j 8 folderplugin
exit /b %errorlevel%
