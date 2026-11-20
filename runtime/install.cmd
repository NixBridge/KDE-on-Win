@echo off
setlocal EnableExtensions DisableDelayedExpansion
set "ERRORLEVEL="
if not "%~1"=="" exit /b 2
call "%~dp0deploy-vm.cmd" install-only
set "INSTALL_EXIT=%ERRORLEVEL%"
if not "%INSTALL_EXIT%"=="0" (
    echo ERROR: Installation failed. Windows will not restart.
    pause
    exit /b %INSTALL_EXIT%
)
echo Plasma will start at the next sign-in for this Windows user.
"%SystemRoot%\System32\choice.exe" /c RN /n /t 30 /d R /m "Restart in 30 seconds. Press R to restart now or N to restart later."
if not "%ERRORLEVEL%"=="1" exit /b 0
"%SystemRoot%\System32\shutdown.exe" /r /t 0
if errorlevel 1 (
    echo Installation succeeded. Please restart Windows manually.
    pause
    exit /b 1
)
exit /b 0
