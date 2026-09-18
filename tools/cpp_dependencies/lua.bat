@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0..\.."

if "%~1"=="" (
    echo Usage: tools\cpp_dependencies\lua.bat ^<cpp-folder^>
    exit /b 1
)

for %%I in ("%~1") do set "CPP_DIR=%%~fI"
for /f "usebackq eol=# tokens=1,2 delims==" %%a in ("%CD%\versions.conf") do set "%%a=%%b"
if not defined LUA_VERSION (
    echo LUA_VERSION is not set in versions.conf.
    exit /b 1
)
if not defined LUA_SHA256 (
    echo LUA_SHA256 is not set in versions.conf.
    exit /b 1
)
for /f "delims=" %%V in ("!LUA_VERSION!") do set "LUA_VERSION=%%V"
for /f "delims=" %%V in ("!LUA_SHA256!") do set "LUA_SHA256=%%V"

set "LUA_DIR=%CPP_DIR%\Engine\ThirdParty\Lua"
set "INSTALLED_LUA_VERSION="
if exist "%LUA_DIR%\.ludork-version" set /p INSTALLED_LUA_VERSION=<"%LUA_DIR%\.ludork-version"
if "!INSTALLED_LUA_VERSION!"=="!LUA_VERSION!" if exist "%LUA_DIR%\src\lua.h" (
    echo Using existing Lua !LUA_VERSION!.
    exit /b 0
)

where curl.exe >nul 2>nul
if errorlevel 1 (
    echo curl.exe was not found.
    exit /b 1
)

if not exist "%CPP_DIR%\Engine\ThirdParty" mkdir "%CPP_DIR%\Engine\ThirdParty"
if not exist "%CPP_DIR%\ThirdPartySource" mkdir "%CPP_DIR%\ThirdPartySource"
set "WORK=%CPP_DIR%\ThirdPartySource\ludork-work-lua"
if exist "%WORK%" rmdir /S /Q "%WORK%"
mkdir "%WORK%\extract"
if not exist "%WORK%\extract" (
    echo Failed to create dependency work directory.
    exit /b 1
)

echo Downloading Lua !LUA_VERSION!...
curl.exe -L --fail --show-error -o "%WORK%\lua.tar.gz" "https://www.lua.org/ftp/lua-!LUA_VERSION!.tar.gz"
if errorlevel 1 exit /b %errorlevel%

echo Verifying Lua SHA-256...
set "ACTUAL_SHA256="
for /f "skip=1 delims=" %%H in ('certutil -hashfile "%WORK%\lua.tar.gz" SHA256') do (
    if not defined ACTUAL_SHA256 set "ACTUAL_SHA256=%%H"
)
set "ACTUAL_SHA256=!ACTUAL_SHA256: =!"
if /I not "!ACTUAL_SHA256!"=="!LUA_SHA256!" (
    echo SHA-256 mismatch: !ACTUAL_SHA256!
    exit /b 1
)

tar -xzf "%WORK%\lua.tar.gz" -C "%WORK%\extract"
if errorlevel 1 exit /b %errorlevel%
if not exist "%WORK%\extract\lua-!LUA_VERSION!\src\lua.h" (
    echo Lua source folder was not found after extraction.
    exit /b 1
)
if exist "%LUA_DIR%" rmdir /S /Q "%LUA_DIR%"
if exist "%LUA_DIR%" (
    echo Failed to replace Lua.
    exit /b 1
)
move /Y "%WORK%\extract\lua-!LUA_VERSION!" "%LUA_DIR%" >nul
if errorlevel 1 exit /b %errorlevel%
> "%LUA_DIR%\.ludork-version" echo !LUA_VERSION!
rmdir /S /Q "%WORK%"
exit /b 0
