@echo off
setlocal EnableExtensions
chcp 65001>nul
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0installer\build_installer.ps1" -Format exe %*
exit /b %ERRORLEVEL%
