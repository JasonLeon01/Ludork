@echo off
setlocal EnableExtensions
cd /d "%~dp0.."

if "%~1"=="" (
    echo Usage: tools\run_cpp.bat ^<cpp-folder^> [Debug^|Release]
    exit /b 1
)

for %%I in ("%~1") do set "CPP_DIR=%%~fI"
set "CONFIG=%~2"
if "%CONFIG%"=="" set "CONFIG=Debug"

if /I not "%CONFIG%"=="Debug" if /I not "%CONFIG%"=="Release" (
    echo Configuration must be Debug or Release.
    echo Usage: tools\run_cpp.bat ^<cpp-folder^> [Debug^|Release]
    exit /b 1
)

set "OUTPUT_DIR=%CPP_DIR%\bin\%CONFIG%"
set "GAME_EXE=%OUTPUT_DIR%\Main.exe"

if not exist "%GAME_EXE%" (
    for %%I in ("%OUTPUT_DIR%\*.exe") do if not defined FOUND_GAME_EXE set "FOUND_GAME_EXE=%%~fI"
    if defined FOUND_GAME_EXE set "GAME_EXE=%FOUND_GAME_EXE%"
)

if not exist "%GAME_EXE%" (
    echo No C++ game executable was found in %OUTPUT_DIR%
    exit /b 1
)

set "SCRIPT_TOOLS=%~dp0ScriptTools.exe"
if not exist "%SCRIPT_TOOLS%" set "SCRIPT_TOOLS=%CD%\.tools\ScriptTools\ScriptTools.exe"
if not exist "%SCRIPT_TOOLS%" (
    echo ScriptTools was not found. Run tools\init.bat first.
    exit /b 1
)
"%SCRIPT_TOOLS%" ui-assets generate "%CPP_DIR%"
if errorlevel 1 exit /b %errorlevel%

pushd "%CPP_DIR%"
"%GAME_EXE%"
set "EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %EXIT_CODE%
