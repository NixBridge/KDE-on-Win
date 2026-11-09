@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install-file-associations.ps1" -CraftRoot "%~dp0" %*
if errorlevel 1 pause
