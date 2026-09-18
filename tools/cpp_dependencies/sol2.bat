@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0..\.."

if "%~1"=="" (
    echo Usage: tools\cpp_dependencies\sol2.bat ^<cpp-folder^>
    exit /b 1
)

for %%I in ("%~1") do set "CPP_DIR=%%~fI"
for /f "usebackq eol=# tokens=1,2 delims==" %%a in ("%CD%\versions.conf") do set "%%a=%%b"
if not defined SOL2_VERSION (
    echo SOL2_VERSION is not set in versions.conf.
    exit /b 1
)
for /f "delims=" %%V in ("!SOL2_VERSION!") do set "SOL2_VERSION=%%V"

set "SOL2_DIR=%CPP_DIR%\Engine\ThirdParty\sol2"
set "INSTALLED_SOL2_VERSION="
if exist "%SOL2_DIR%\.ludork-version" set /p INSTALLED_SOL2_VERSION=<"%SOL2_DIR%\.ludork-version"
if "!INSTALLED_SOL2_VERSION!"=="!SOL2_VERSION!" if exist "%SOL2_DIR%\include\sol2\sol.hpp" (
    echo Using existing sol2 !SOL2_VERSION!.
    exit /b 0
)

where curl.exe >nul 2>nul
if errorlevel 1 (
    echo curl.exe was not found.
    exit /b 1
)

if not exist "%CPP_DIR%\Engine\ThirdParty" mkdir "%CPP_DIR%\Engine\ThirdParty"
if not exist "%CPP_DIR%\ThirdPartySource" mkdir "%CPP_DIR%\ThirdPartySource"
set "WORK=%CPP_DIR%\ThirdPartySource\ludork-work-sol2"
if exist "%WORK%" rmdir /S /Q "%WORK%"
mkdir "%WORK%\sol2\include\sol2"
if not exist "%WORK%\sol2\include\sol2" (
    echo Failed to create dependency work directory.
    exit /b 1
)

echo Downloading sol2 !SOL2_VERSION! headers...
for %%F in (config.hpp forward.hpp sol.hpp) do (
    curl.exe -L --fail --show-error -o "%WORK%\sol2\include\sol2\%%F" "https://github.com/ThePhD/sol2/releases/download/v!SOL2_VERSION!/%%F"
    if errorlevel 1 exit /b %errorlevel%
)

if exist "%SOL2_DIR%" rmdir /S /Q "%SOL2_DIR%"
if exist "%SOL2_DIR%" (
    echo Failed to replace sol2.
    exit /b 1
)
move /Y "%WORK%\sol2" "%SOL2_DIR%" >nul
if errorlevel 1 exit /b %errorlevel%
> "%SOL2_DIR%\.ludork-version" echo !SOL2_VERSION!
rmdir /S /Q "%WORK%"
exit /b 0
