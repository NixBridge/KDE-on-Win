@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
set "PATH=D:\KDE\CraftRoot\dev-utils\bin;D:\KDE\CraftRoot\bin;%PATH%"
cmake -S "%~dp0interfaces" -B D:\KDE\interface-build -G Ninja -DCMAKE_PREFIX_PATH=D:/KDE/CraftRoot -DCMAKE_INSTALL_PREFIX=D:/KDE/CraftRoot -DKWIN_SOURCE=D:/KDE/sources/extra/kwin-6.7.4 -DLOCKER_SOURCE=D:/KDE/sources/extra/kscreenlocker-6.7.4 -DSTATUS_NOTIFIER_SOURCE=D:/KDE/sources/extra/kstatusnotifieritem-6.28.0
if errorlevel 1 exit /b 1
cmake --build D:\KDE\interface-build --target install
exit /b %errorlevel%
