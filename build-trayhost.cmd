@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
set "PATH=D:\KDE\CraftRoot\dev-utils\bin;D:\KDE\CraftRoot\bin;%PATH%"
cmake -S "%~dp0native" -B D:\KDE\native-build -G Ninja -DCMAKE_PREFIX_PATH=D:/KDE/CraftRoot -DCMAKE_INSTALL_PREFIX=D:/KDE/CraftRoot -DCMAKE_BUILD_TYPE=Release -DBUILD_VM_PROBES=ON -DBUILD_PLASMA_MODULES=ON
if errorlevel 1 exit /b 1
cmake --build D:\KDE\native-build --target install --parallel 8
exit /b %errorlevel%
