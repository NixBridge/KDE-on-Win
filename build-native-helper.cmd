@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
call "%~dp0upstream\plasma-windows\tools\build-shellswitch.cmd"
exit /b %errorlevel%
