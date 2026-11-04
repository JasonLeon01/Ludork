@echo off
setlocal EnableExtensions
cd /d "%~dp0.."
set "TOOL=%CD%\.tools\Toolchain\ludork-build.exe"
if not exist "%TOOL%" (
    echo Ludork build tools were not found. Run tools\init.bat first.
    exit /b 1
)
"%TOOL%" dependencies %* --repository-root "%CD%"
exit /b %errorlevel%
