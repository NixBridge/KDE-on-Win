@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
set "PATH=D:\KDE\CraftRoot\dev-utils\bin;D:\KDE\CraftRoot\bin;%PATH%"
ninja -C D:\_\ed4c8194\build -j 8 kio_desktop desktopnotifier
if errorlevel 1 exit /b 1
cmake -DCMAKE_INSTALL_PREFIX=D:/KDE/CraftRoot -DCMAKE_INSTALL_CONFIG_NAME=RelWithDebInfo -P D:\_\ed4c8194\build\kioworkers\desktop\cmake_install.cmake
exit /b %errorlevel%
