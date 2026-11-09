@echo off
if /i "%~1"=="restore" goto restore
start "" "%~dp0bin\kde-style-host.exe"
if exist "%~dp0bin\x86\kde-style-host.exe" start "" "%~dp0bin\x86\kde-style-host.exe"
exit /b 0
:restore
if exist "%~dp0bin\x86\kde-style-host.exe" start /wait "" "%~dp0bin\x86\kde-style-host.exe" --stop
start /wait "" "%~dp0bin\kde-style-host.exe" --stop
