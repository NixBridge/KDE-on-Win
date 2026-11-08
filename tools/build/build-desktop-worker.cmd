@echo off
call "%~dp0msvc-env.cmd"
if errorlevel 1 exit /b 1
ninja -C D:\_\ed4c8194\build -j 8 kio_desktop desktopnotifier
if errorlevel 1 exit /b 1
cmake -DCMAKE_INSTALL_PREFIX=D:/KDE/CraftRoot -DCMAKE_INSTALL_CONFIG_NAME=RelWithDebInfo -P D:\_\ed4c8194\build\kioworkers\desktop\cmake_install.cmake
exit /b %errorlevel%
