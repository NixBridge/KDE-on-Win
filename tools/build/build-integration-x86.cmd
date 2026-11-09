@echo off
set "VSLANG=1033"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat"
if errorlevel 1 exit /b 1
set "PATH=D:\KDE\CraftRoot\dev-utils\bin;%PATH%"
cmake -S "%~dp0..\..\native\integration" -B D:\KDE\integration-x86-build -G Ninja "-DCMAKE_PROJECT_INCLUDE=%~dp0msvc-deps.cmake" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=D:/KDE/CraftRoot
if errorlevel 1 exit /b 1
cmake --build D:\KDE\integration-x86-build --target install --parallel 4
exit /b %errorlevel%
