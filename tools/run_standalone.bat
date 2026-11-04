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

set "TOOLCHAIN_DIRECTORY=%~dp0Toolchain"
if not exist "%TOOLCHAIN_DIRECTORY%" set "TOOLCHAIN_DIRECTORY=%~dp0..\.tools\Toolchain"
if not exist "%TOOLCHAIN_DIRECTORY%" (
    echo Toolchain was not found. Run tools\init.bat first.
    exit /b 1
)
"%TOOLCHAIN_DIRECTORY%\ludork-assets.exe" material-assets validate "%GAME_DIR%"
if errorlevel 1 exit /b %errorlevel%

pushd "%GAME_DIR%"
"%GAME_EXE%"
set "EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %EXIT_CODE%
