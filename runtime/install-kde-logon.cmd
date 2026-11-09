@echo off
setlocal
set "KDE_LOGON_SCRIPT=%~dp0install-kde-logon.ps1"
powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process powershell.exe -Verb RunAs -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',('\"'+$env:KDE_LOGON_SCRIPT+'\"'),'-Action','Install')"
