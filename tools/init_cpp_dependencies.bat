@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0.."

if "%~1"=="" (
    echo Usage: tools\init_cpp_dependencies.bat ^<cpp-folder^>
    exit /b 1
)

for %%I in ("%~1") do set "CPP_DIR=%%~fI"
for /f "usebackq eol=# tokens=1,2 delims==" %%a in ("%CD%\versions.conf") do set "%%a=%%b"

if not defined LUASF_VERSION (
    echo LUASF_VERSION is not set in versions.conf.
    exit /b 1
)
if not defined LUA_CJSON_VERSION (
    echo LUA_CJSON_VERSION is not set in versions.conf.
    exit /b 1
)
if not defined ZLIB_VERSION (
    echo ZLIB_VERSION is not set in versions.conf.
    exit /b 1
)

for /f "delims=" %%V in ("!LUASF_VERSION!") do set "LUASF_VERSION=%%V"
for /f "delims=" %%V in ("!LUA_CJSON_VERSION!") do set "LUA_CJSON_VERSION=%%V"
for /f "delims=" %%V in ("!ZLIB_VERSION!") do set "ZLIB_VERSION=%%V"

if not exist "%CPP_DIR%\Engine\ThirdParty" mkdir "%CPP_DIR%\Engine\ThirdParty"

set "LUASF_DIR=%CPP_DIR%\Engine\ThirdParty\LuaSF"
set "LUASF_ZIP=%CPP_DIR%\LuaSF-source.zip"
set "LUASF_EXTRACTED_DIR=%CPP_DIR%\Engine\ThirdParty\LuaSF-source"
set "LUA_CJSON_DIR=%CPP_DIR%\Engine\ThirdParty\lua-cjson"
set "LUA_CJSON_ZIP=%CPP_DIR%\lua-cjson.zip"
set "ZLIB_DIR=%CPP_DIR%\Engine\ThirdParty\zlib"
set "ZLIB_ZIP=%CPP_DIR%\zlib.zip"

if exist "%LUA_CJSON_DIR%" rmdir /S /Q "%LUA_CJSON_DIR%"
if exist "%ZLIB_DIR%" rmdir /S /Q "%ZLIB_DIR%"

set "INSTALLED_LUASF_VERSION="
if exist "%LUASF_DIR%\.ludork-version" set /p INSTALLED_LUASF_VERSION=<"%LUASF_DIR%\.ludork-version"
if "%INSTALLED_LUASF_VERSION%"=="%LUASF_VERSION%" if exist "%LUASF_DIR%\CMakeLists.txt" (
    echo Using existing LuaSF %LUASF_VERSION%.
    goto luasf_ready
)

if exist "%LUASF_DIR%" rmdir /S /Q "%LUASF_DIR%"
if exist "%LUASF_EXTRACTED_DIR%" rmdir /S /Q "%LUASF_EXTRACTED_DIR%"
echo Downloading LuaSF %LUASF_VERSION%...
powershell -Command "Invoke-WebRequest -Uri 'https://github.com/JasonLeon01/LuaSF-AutoGenerator/releases/download/%LUASF_VERSION%/LuaSF-source.zip' -OutFile '%LUASF_ZIP%'"
if errorlevel 1 exit /b %errorlevel%
powershell -Command "Expand-Archive -Path '%LUASF_ZIP%' -DestinationPath '%CPP_DIR%\Engine\ThirdParty' -Force"
if errorlevel 1 exit /b %errorlevel%
del "%LUASF_ZIP%"
if not exist "%LUASF_EXTRACTED_DIR%\CMakeLists.txt" (
    echo LuaSF source folder was not found after extraction.
    exit /b 1
)
ren "%LUASF_EXTRACTED_DIR%" "LuaSF"
if errorlevel 1 exit /b %errorlevel%
> "%LUASF_DIR%\.ludork-version" echo %LUASF_VERSION%

:luasf_ready

set "LUASF_GIT_ROOT="
set "LUASF_GIT_DIRECTORY="
for /f "usebackq delims=" %%I in (`git -C "%LUASF_DIR%" rev-parse --show-toplevel 2^>nul`) do set "LUASF_GIT_ROOT=%%I"
if defined LUASF_GIT_ROOT (
    for /f "usebackq delims=" %%I in (`git -C "%LUASF_DIR%" rev-parse --show-prefix 2^>nul`) do set "LUASF_GIT_DIRECTORY=%%I"
)

set "VALUE_COPY_PATCH=%CD%\patches\luasf-value-copy.patch"
echo Applying LuaSF native value copy patch if needed...
call :apply_luasf_patch --reverse --check "%VALUE_COPY_PATCH%" >nul 2>&1
if errorlevel 1 (
    call :apply_luasf_patch --check "%VALUE_COPY_PATCH%"
    if errorlevel 1 (
        exit /b 1
    )
    call :apply_luasf_patch "%VALUE_COPY_PATCH%"
    if errorlevel 1 (
        exit /b 1
    )
) else (
    echo LuaSF native value copy patch is already applied.
)

echo Downloading lua-cjson %LUA_CJSON_VERSION%...
powershell -Command "Invoke-WebRequest -Uri 'https://github.com/openresty/lua-cjson/archive/refs/tags/%LUA_CJSON_VERSION%.zip' -OutFile '%LUA_CJSON_ZIP%'"
if errorlevel 1 exit /b %errorlevel%
powershell -Command "Expand-Archive -Path '%LUA_CJSON_ZIP%' -DestinationPath '%CPP_DIR%\Engine\ThirdParty' -Force"
if errorlevel 1 exit /b %errorlevel%
del "%LUA_CJSON_ZIP%"
if not exist "%CPP_DIR%\Engine\ThirdParty\lua-cjson-%LUA_CJSON_VERSION%" (
    echo lua-cjson source folder was not found after extraction.
    exit /b 1
)
ren "%CPP_DIR%\Engine\ThirdParty\lua-cjson-%LUA_CJSON_VERSION%" "lua-cjson"

echo Downloading zlib %ZLIB_VERSION%...
powershell -Command "Invoke-WebRequest -Uri 'https://github.com/madler/zlib/archive/refs/tags/v%ZLIB_VERSION%.zip' -OutFile '%ZLIB_ZIP%'"
if errorlevel 1 exit /b %errorlevel%
powershell -Command "Expand-Archive -Path '%ZLIB_ZIP%' -DestinationPath '%CPP_DIR%\Engine\ThirdParty' -Force"
if errorlevel 1 exit /b %errorlevel%
del "%ZLIB_ZIP%"
if not exist "%CPP_DIR%\Engine\ThirdParty\zlib-%ZLIB_VERSION%" (
    echo zlib source folder was not found after extraction.
    exit /b 1
)
ren "%CPP_DIR%\Engine\ThirdParty\zlib-%ZLIB_VERSION%" "zlib"

call "%CD%\tools\init_ffmpeg_source.bat" "%CPP_DIR%"
if errorlevel 1 exit /b %errorlevel%

call "%CD%\tools\init_gnu_make.bat"
if errorlevel 1 exit /b %errorlevel%

echo C++ dependencies are ready in %CPP_DIR%
exit /b 0

:apply_luasf_patch
if defined LUASF_GIT_DIRECTORY (
    git -C "%LUASF_GIT_ROOT%" apply --unidiff-zero --directory="%LUASF_GIT_DIRECTORY%" %*
) else (
    git -C "%LUASF_DIR%" apply --unidiff-zero %*
)
exit /b %errorlevel%
