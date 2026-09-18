@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0..\.."

if "%~1"=="" (
    echo Usage: tools\cpp_dependencies\luasf.bat ^<cpp-folder^>
    exit /b 1
)

for %%I in ("%~1") do set "CPP_DIR=%%~fI"
for /f "usebackq eol=# tokens=1,2 delims==" %%a in ("%CD%\versions.conf") do set "%%a=%%b"
if not defined LUASF_VERSION (
    echo LUASF_VERSION is not set in versions.conf.
    exit /b 1
)
for /f "delims=" %%V in ("!LUASF_VERSION!") do set "LUASF_VERSION=%%V"
if not defined LUASF_VARIANT set "LUASF_VARIANT="
for /f "delims=" %%V in ("!LUASF_VARIANT!") do set "LUASF_VARIANT=%%V"

rem The released source packages are variant-specific: the generated bindings
rem follow the SFML the variant was built from, and the SFML-ME forks bind more
rem than upstream SFML. Ludork tracks the same variant as its SFML pin.
set "LUASF_SOURCE_NAME=LuaSF-source"
set "LUASF_MARKER=!LUASF_VERSION!"
if not "!LUASF_VARIANT!"=="" (
    set "LUASF_SOURCE_NAME=LuaSF-source-!LUASF_VARIANT!"
    set "LUASF_MARKER=!LUASF_VERSION!-!LUASF_VARIANT!"
)

set "LUASF_DIR=%CPP_DIR%\Engine\ThirdParty\LuaSF"
set "INSTALLED_LUASF_VERSION="
if exist "%LUASF_DIR%\.ludork-version" set /p INSTALLED_LUASF_VERSION=<"%LUASF_DIR%\.ludork-version"
if "!INSTALLED_LUASF_VERSION!"=="!LUASF_MARKER!" if exist "%LUASF_DIR%\CMakeLists.txt" (
    echo Using existing LuaSF !LUASF_MARKER!.
    exit /b 0
)

where curl.exe >nul 2>nul
if errorlevel 1 (
    echo curl.exe was not found.
    exit /b 1
)

if not exist "%CPP_DIR%\Engine\ThirdParty" mkdir "%CPP_DIR%\Engine\ThirdParty"
if not exist "%CPP_DIR%\ThirdPartySource" mkdir "%CPP_DIR%\ThirdPartySource"
set "WORK=%CPP_DIR%\ThirdPartySource\ludork-work-luasf"
if exist "%WORK%" rmdir /S /Q "%WORK%"
mkdir "%WORK%\extract"
if not exist "%WORK%\extract" (
    echo Failed to create dependency work directory.
    exit /b 1
)

echo Downloading LuaSF %LUASF_MARKER%...
curl.exe -L --fail --show-error -o "%WORK%\!LUASF_SOURCE_NAME!.zip" "https://github.com/JasonLeon01/LuaSF-AutoGenerator/releases/download/%LUASF_VERSION%/!LUASF_SOURCE_NAME!.zip"
if errorlevel 1 exit /b %errorlevel%
powershell -NoProfile -Command "Expand-Archive -Path '%WORK%\!LUASF_SOURCE_NAME!.zip' -DestinationPath '%WORK%\extract' -Force"
if errorlevel 1 exit /b %errorlevel%
set "LUASF_SOURCE=%WORK%\extract"
if exist "%WORK%\extract\!LUASF_SOURCE_NAME!\CMakeLists.txt" set "LUASF_SOURCE=%WORK%\extract\!LUASF_SOURCE_NAME!"
if not exist "%LUASF_SOURCE%\CMakeLists.txt" (
    echo LuaSF source folder was not found after extraction.
    exit /b 1
)
if exist "%LUASF_DIR%" rmdir /S /Q "%LUASF_DIR%"
if exist "%LUASF_DIR%" (
    echo Failed to replace LuaSF.
    exit /b 1
)
move /Y "%LUASF_SOURCE%" "%LUASF_DIR%" >nul
if errorlevel 1 exit /b %errorlevel%
> "%LUASF_DIR%\.ludork-version" echo !LUASF_MARKER!
rmdir /S /Q "%WORK%"
exit /b 0
