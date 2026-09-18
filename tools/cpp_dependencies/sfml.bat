@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0..\.."

if "%~1"=="" (
    echo Usage: tools\cpp_dependencies\sfml.bat ^<cpp-folder^>
    exit /b 1
)

for %%I in ("%~1") do set "CPP_DIR=%%~fI"
for /f "usebackq eol=# tokens=1,2 delims==" %%a in ("%CD%\versions.conf") do set "%%a=%%b"
if not defined SFML_REPOSITORY (
    echo SFML_REPOSITORY is not set in versions.conf.
    exit /b 1
)
if not defined SFML_TAG (
    echo SFML_TAG is not set in versions.conf.
    exit /b 1
)
for /f "delims=" %%V in ("!SFML_REPOSITORY!") do set "SFML_REPOSITORY=%%V"
for /f "delims=" %%V in ("!SFML_TAG!") do set "SFML_TAG=%%V"

set "SFML_DIR=%CPP_DIR%\Engine\ThirdParty\SFML"
set "INSTALLED_SFML_VERSION="
if exist "%SFML_DIR%\.ludork-version" set /p INSTALLED_SFML_VERSION=<"%SFML_DIR%\.ludork-version"
if "!INSTALLED_SFML_VERSION!"=="!SFML_TAG!" if exist "%SFML_DIR%\CMakeLists.txt" (
    echo Using existing SFML !SFML_TAG!.
    exit /b 0
)

where curl.exe >nul 2>nul
if errorlevel 1 (
    echo curl.exe was not found.
    exit /b 1
)

set "TAR=%SystemRoot%\System32\tar.exe"
if not exist "%TAR%" (
    echo tar.exe was not found.
    exit /b 1
)

set "SFML_REPOSITORY_NAME=%SFML_REPOSITORY%"
for /f "tokens=2 delims=/" %%R in ("!SFML_REPOSITORY!") do set "SFML_REPOSITORY_NAME=%%R"

if not exist "%CPP_DIR%\Engine\ThirdParty" mkdir "%CPP_DIR%\Engine\ThirdParty"
if not exist "%CPP_DIR%\ThirdPartySource" mkdir "%CPP_DIR%\ThirdPartySource"
set "WORK=%CPP_DIR%\ThirdPartySource\ludork-work-sfml"
if exist "%WORK%" rmdir /S /Q "%WORK%"
mkdir "%WORK%\extract"
if not exist "%WORK%\extract" (
    echo Failed to create dependency work directory.
    exit /b 1
)

echo Downloading SFML !SFML_TAG! from !SFML_REPOSITORY!...
curl.exe -L --fail --show-error -o "%WORK%\sfml.tar.gz" "https://github.com/!SFML_REPOSITORY!/archive/refs/tags/!SFML_TAG!.tar.gz"
if errorlevel 1 exit /b %errorlevel%
"%TAR%" -xzf "%WORK%\sfml.tar.gz" -C "%WORK%\extract"
if errorlevel 1 exit /b %errorlevel%
set "SFML_SOURCE=%WORK%\extract\!SFML_REPOSITORY_NAME!-!SFML_TAG!"
if not exist "!SFML_SOURCE!\CMakeLists.txt" (
    echo SFML source folder was not found after extraction.
    exit /b 1
)
if exist "%SFML_DIR%" rmdir /S /Q "%SFML_DIR%"
if exist "%SFML_DIR%" (
    echo Failed to replace SFML.
    exit /b 1
)
move /Y "!SFML_SOURCE!" "%SFML_DIR%" >nul
if errorlevel 1 exit /b %errorlevel%
> "%SFML_DIR%\.ludork-version" echo !SFML_TAG!
rmdir /S /Q "%WORK%"
exit /b 0
