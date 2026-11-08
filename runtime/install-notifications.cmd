@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install-notifications.ps1" %*
pause
