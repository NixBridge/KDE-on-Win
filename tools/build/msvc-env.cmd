@echo off
if not defined CRAFT_ROOT set "CRAFT_ROOT=D:\KDE\CraftRoot"
if not defined VCVARS64 set "VCVARS64=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
call "%VCVARS64%"
if errorlevel 1 exit /b 1
set "PATH=%CRAFT_ROOT%\dev-utils\bin;%CRAFT_ROOT%\bin;%PATH%"
exit /b 0
