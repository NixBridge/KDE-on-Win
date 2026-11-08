@echo off
call "%~dp0msvc-env.cmd"
if errorlevel 1 exit /b 1
ninja -C D:\_\a5c7712e\build -j 8 PlasmaQuick plasmaextracomponentsplugin
exit /b %errorlevel%
