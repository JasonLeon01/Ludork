@echo off
setlocal EnableExtensions
cd /d "%~dp0.."

if "%~1"=="" (
    echo Usage: tools\run_standalone.bat ^<standalone-folder^>
    exit /b 1
)

for %%I in ("%~1") do set "GAME_DIR=%%~fI"
set "GAME_EXE=%GAME_DIR%\Main.exe"

if not exist "%GAME_EXE%" (
    echo Standalone game executable was not found: %GAME_EXE%
    exit /b 1
)

set "SCRIPT_TOOLS=%~dp0ScriptTools\ScriptTools.exe"
if not exist "%SCRIPT_TOOLS%" set "SCRIPT_TOOLS=%~dp0..\.tools\ScriptTools\ScriptTools.exe"
if not exist "%SCRIPT_TOOLS%" (
    echo ScriptTools was not found. Run tools\init.bat first.
    exit /b 1
)
"%SCRIPT_TOOLS%" material-assets validate "%GAME_DIR%"
if errorlevel 1 exit /b %errorlevel%

pushd "%GAME_DIR%"
"%GAME_EXE%"
set "EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %EXIT_CODE%
