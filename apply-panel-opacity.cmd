@echo off
setlocal EnableExtensions
call "%~dp0plasma-common.cmd" :pc_setup_env
"%CRAFT_BIN%\qdbus.exe" org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.evaluateScript "var list = panels(); for (var i = 0; i < list.length; ++i) { list[i].opacity = 'opaque'; }"
if errorlevel 1 (
    echo Failed: run deploy-vm.cmd first, then run this script while Plasma is running.
    pause
    exit /b 1
)
echo Panel opacity saved. Explorer has not been stopped.
pause
exit /b 0
