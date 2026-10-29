@echo off
setlocal EnableExtensions DisableDelayedExpansion
chcp 65001>nul
for %%I in ("%~dp0.") do set "TOOLS_DIR=%%~fI"
if "%~1"=="" goto usage
if not "%~3"=="" goto usage
for %%I in ("%~1") do set "PROJECT_DIR=%%~fI"
set "CONFIG=%~2"
if "%CONFIG%"=="" set "CONFIG=Release"
if /I "%CONFIG%"=="Debug" set "CONFIG=Debug"
if /I "%CONFIG%"=="Release" set "CONFIG=Release"
set "SCRIPT_TOOLS=%TOOLS_DIR%\ScriptTools\ScriptTools.exe"
if not exist "%SCRIPT_TOOLS%" set "SCRIPT_TOOLS=%TOOLS_DIR%\..\.tools\ScriptTools\ScriptTools.exe"
if not exist "%SCRIPT_TOOLS%" (
    echo ScriptTools was not found. Prepare the editor tools first.
    exit /b 20
)
"%SCRIPT_TOOLS%" windows-native-build "%PROJECT_DIR%" "%CONFIG%" --target PreviewHost --tools-directory "%TOOLS_DIR%"
if errorlevel 1 exit /b %errorlevel%
"%SCRIPT_TOOLS%" preview validate "%PROJECT_DIR%"
exit /b %errorlevel%

:usage
echo Usage: tools\build_preview_host.bat ^<project-folder^> [Debug^|Release]
exit /b 1
