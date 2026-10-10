@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0run_device.ps1" %*
exit /b %ERRORLEVEL%
