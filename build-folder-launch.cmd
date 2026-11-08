@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
set "PATH=D:\KDE\CraftRoot\dev-utils\bin;D:\KDE\CraftRoot\bin;%PATH%"
ninja -C D:\_\13863b6b\build -j 8 folderplugin
exit /b %errorlevel%
