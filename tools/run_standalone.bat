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

pushd "%GAME_DIR%"
"%GAME_EXE%"
set "EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %EXIT_CODE%
