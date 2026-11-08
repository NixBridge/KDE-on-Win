@echo off
call "%~dp0msvc-env.cmd"
if errorlevel 1 exit /b 1
cmake -S "%~dp0..\..\native" -B D:\KDE\native-build -G Ninja "-DCMAKE_PREFIX_PATH=%CRAFT_ROOT%" "-DCMAKE_INSTALL_PREFIX=%CRAFT_ROOT%" -DCMAKE_BUILD_TYPE=Release -DBUILD_VM_PROBES=ON -DBUILD_PLASMA_MODULES=ON
if errorlevel 1 exit /b 1
cmake --build D:\KDE\native-build --target install --parallel 8
exit /b %errorlevel%
