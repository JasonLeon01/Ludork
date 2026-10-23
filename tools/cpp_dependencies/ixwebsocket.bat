@echo off
setlocal EnableExtensions
cd /d "%~dp0..\.."
if "%~1"=="" (
    echo Usage: tools\cpp_dependencies\ixwebsocket.bat ^<cpp-folder^>
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0ixwebsocket.ps1" -CppFolder "%~1"
exit /b %errorlevel%
