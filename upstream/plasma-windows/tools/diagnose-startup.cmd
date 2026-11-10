@echo off
setlocal EnableExtensions DisableDelayedExpansion
rem SPDX-License-Identifier: GPL-3.0-or-later
rem No install/restore arguments: diagnose normal startup only.
if not "%~1"=="" if /i not "%~1"=="--no-pause" exit /b 2
if not "%~2"=="" exit /b 2
set "ERRORLEVEL="
set "STARTUP_LOG=%~dp0startup-diagnostic.log"
type nul >"%STARTUP_LOG%" 2>nul || goto :temporary_log
goto :run
:temporary_log
set "STARTUP_LOG=%TEMP%\kde-startup-diagnostic.log"
type nul >"%STARTUP_LOG%" 2>nul || goto :no_log
:run
>"%STARTUP_LOG%" echo KDE-on-Win bootstrap diagnostic
>>"%STARTUP_LOG%" echo Runtime: "%~dp0"
if not exist "%~dp0deploy-vm.cmd" goto :missing
rem Child cmd isolates a batch syntax error or an accidental plain EXIT.
set "KDE_DEPLOY_CAPTURED=1"
pushd "%~dp0"
if errorlevel 1 goto :bad_directory
"%ComSpec%" /d /v:off /c deploy-vm.cmd >>"%STARTUP_LOG%" 2>&1
set "STARTUP_EXIT=%errorlevel%"
popd
goto :report
:bad_directory
>>"%STARTUP_LOG%" echo ERROR: Cannot enter the runtime directory.
set "STARTUP_EXIT=1"
goto :report
:missing
>>"%STARTUP_LOG%" echo ERROR: deploy-vm.cmd is missing beside this diagnostic.
set "STARTUP_EXIT=2"
:report
>>"%STARTUP_LOG%" echo Bootstrap exit code: %STARTUP_EXIT%
type "%STARTUP_LOG%"
echo.
echo Please share this log: "%STARTUP_LOG%"
if not "%STARTUP_EXIT%"=="0" if not defined KDE_BOOTSTRAP_NO_PAUSE if /i not "%~1"=="--no-pause" pause
exit /b %STARTUP_EXIT%
:no_log
echo ERROR: Neither the runtime folder nor TEMP can store the diagnostic log.
if /i not "%~1"=="--no-pause" pause
exit /b 1
