@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
set "PATH=D:\KDE\CraftRoot\dev-utils\bin;D:\KDE\CraftRoot\bin;%PATH%"
ninja -C D:\_\ed4c8194\build -j 8 -k 0
exit /b %errorlevel%
