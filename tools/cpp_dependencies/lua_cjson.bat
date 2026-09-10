@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0..\.."

if "%~1"=="" (
    echo Usage: tools\cpp_dependencies\lua_cjson.bat ^<cpp-folder^>
    exit /b 1
)

for %%I in ("%~1") do set "CPP_DIR=%%~fI"
for /f "usebackq eol=# tokens=1,2 delims==" %%a in ("%CD%\versions.conf") do set "%%a=%%b"
if not defined LUA_CJSON_VERSION (
    echo LUA_CJSON_VERSION is not set in versions.conf.
    exit /b 1
)
for /f "delims=" %%V in ("!LUA_CJSON_VERSION!") do set "LUA_CJSON_VERSION=%%V"

set "LUA_CJSON_DIR=%CPP_DIR%\Engine\ThirdParty\lua-cjson"
set "INSTALLED_LUA_CJSON_VERSION="
if exist "%LUA_CJSON_DIR%\.ludork-version" set /p INSTALLED_LUA_CJSON_VERSION=<"%LUA_CJSON_DIR%\.ludork-version"
if "!INSTALLED_LUA_CJSON_VERSION!"=="!LUA_CJSON_VERSION!" if exist "%LUA_CJSON_DIR%\lua_cjson.c" (
    echo Using existing lua-cjson !LUA_CJSON_VERSION!.
    exit /b 0
)

where curl.exe >nul 2>nul
if errorlevel 1 (
    echo curl.exe was not found.
    exit /b 1
)

if not exist "%CPP_DIR%\Engine\ThirdParty" mkdir "%CPP_DIR%\Engine\ThirdParty"
if not exist "%CPP_DIR%\ThirdPartySource" mkdir "%CPP_DIR%\ThirdPartySource"
set "WORK=%CPP_DIR%\ThirdPartySource\ludork-work-lua-cjson"
if exist "%WORK%" rmdir /S /Q "%WORK%"
mkdir "%WORK%\extract"
if not exist "%WORK%\extract" (
    echo Failed to create dependency work directory.
    exit /b 1
)

echo Downloading lua-cjson %LUA_CJSON_VERSION%...
curl.exe -L --fail --show-error -o "%WORK%\lua-cjson.zip" "https://github.com/openresty/lua-cjson/archive/refs/tags/%LUA_CJSON_VERSION%.zip"
if errorlevel 1 exit /b %errorlevel%
powershell -NoProfile -Command "Expand-Archive -Path '%WORK%\lua-cjson.zip' -DestinationPath '%WORK%\extract' -Force"
if errorlevel 1 exit /b %errorlevel%
if not exist "%WORK%\extract\lua-cjson-%LUA_CJSON_VERSION%\lua_cjson.c" (
    echo lua-cjson source folder was not found after extraction.
    exit /b 1
)
if exist "%LUA_CJSON_DIR%" rmdir /S /Q "%LUA_CJSON_DIR%"
if exist "%LUA_CJSON_DIR%" (
    echo Failed to replace lua-cjson.
    exit /b 1
)
move /Y "%WORK%\extract\lua-cjson-%LUA_CJSON_VERSION%" "%LUA_CJSON_DIR%" >nul
if errorlevel 1 exit /b %errorlevel%
> "%LUA_CJSON_DIR%\.ludork-version" echo %LUA_CJSON_VERSION%
rmdir /S /Q "%WORK%"
exit /b 0
