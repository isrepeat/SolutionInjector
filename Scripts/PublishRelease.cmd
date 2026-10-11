@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0PowerShell\PublishRelease.ps1" %*
set "PublishExitCode=%ERRORLEVEL%"
pause
exit /b %PublishExitCode%