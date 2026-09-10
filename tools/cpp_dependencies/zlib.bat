@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0..\.."

if "%~1"=="" (
    echo Usage: tools\cpp_dependencies\zlib.bat ^<cpp-folder^>
    exit /b 1
)

for %%I in ("%~1") do set "CPP_DIR=%%~fI"
for /f "usebackq eol=# tokens=1,2 delims==" %%a in ("%CD%\versions.conf") do set "%%a=%%b"
if not defined ZLIB_VERSION (
    echo ZLIB_VERSION is not set in versions.conf.
    exit /b 1
)
for /f "delims=" %%V in ("!ZLIB_VERSION!") do set "ZLIB_VERSION=%%V"

set "ZLIB_DIR=%CPP_DIR%\Engine\ThirdParty\zlib"
set "INSTALLED_ZLIB_VERSION="
if exist "%ZLIB_DIR%\.ludork-version" set /p INSTALLED_ZLIB_VERSION=<"%ZLIB_DIR%\.ludork-version"
if "!INSTALLED_ZLIB_VERSION!"=="!ZLIB_VERSION!" if exist "%ZLIB_DIR%\CMakeLists.txt" (
    echo Using existing zlib !ZLIB_VERSION!.
    exit /b 0
)

where curl.exe >nul 2>nul
if errorlevel 1 (
    echo curl.exe was not found.
    exit /b 1
)

if not exist "%CPP_DIR%\Engine\ThirdParty" mkdir "%CPP_DIR%\Engine\ThirdParty"
if not exist "%CPP_DIR%\ThirdPartySource" mkdir "%CPP_DIR%\ThirdPartySource"
set "WORK=%CPP_DIR%\ThirdPartySource\ludork-work-zlib"
if exist "%WORK%" rmdir /S /Q "%WORK%"
mkdir "%WORK%\extract"
if not exist "%WORK%\extract" (
    echo Failed to create dependency work directory.
    exit /b 1
)

echo Downloading zlib %ZLIB_VERSION%...
curl.exe -L --fail --show-error -o "%WORK%\zlib.zip" "https://github.com/madler/zlib/archive/refs/tags/v%ZLIB_VERSION%.zip"
if errorlevel 1 exit /b %errorlevel%
powershell -NoProfile -Command "Expand-Archive -Path '%WORK%\zlib.zip' -DestinationPath '%WORK%\extract' -Force"
if errorlevel 1 exit /b %errorlevel%
if not exist "%WORK%\extract\zlib-%ZLIB_VERSION%\CMakeLists.txt" (
    echo zlib source folder was not found after extraction.
    exit /b 1
)
if exist "%ZLIB_DIR%" rmdir /S /Q "%ZLIB_DIR%"
if exist "%ZLIB_DIR%" (
    echo Failed to replace zlib.
    exit /b 1
)
move /Y "%WORK%\extract\zlib-%ZLIB_VERSION%" "%ZLIB_DIR%" >nul
if errorlevel 1 exit /b %errorlevel%
> "%ZLIB_DIR%\.ludork-version" echo %ZLIB_VERSION%
rmdir /S /Q "%WORK%"
exit /b 0
