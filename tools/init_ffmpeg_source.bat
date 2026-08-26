@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0.."

if "%~1"=="" (
    echo Usage: tools\init_ffmpeg_source.bat ^<cpp-folder^>
    exit /b 1
)

for %%I in ("%~1") do set "CPP_DIR=%%~fI"
for /f "usebackq eol=# tokens=1,2 delims==" %%a in ("%CD%\versions.conf") do set "%%a=%%b"

if not defined FFMPEG_VERSION (
    echo FFMPEG_VERSION is not set in versions.conf.
    exit /b 1
)
if not defined FFMPEG_COMMIT (
    echo FFMPEG_COMMIT is not set in versions.conf.
    exit /b 1
)

for /f "delims=" %%V in ("!FFMPEG_VERSION!") do set "FFMPEG_VERSION=%%V"
for /f "delims=" %%V in ("!FFMPEG_COMMIT!") do set "FFMPEG_COMMIT=%%V"

set "TAR=%SystemRoot%\System32\tar.exe"
if not exist "%TAR%" (
    echo tar.exe was not found.
    exit /b 1
)

set "SOURCE_DIR=%CPP_DIR%\ffmpeg"
set "EXTRACTED_DIR=%CPP_DIR%\FFmpeg-%FFMPEG_COMMIT%"
set "SOURCE_ARCHIVE_DIR=%CPP_DIR%\ThirdPartySource"
set "SOURCE_ARCHIVE=%SOURCE_ARCHIVE_DIR%\ffmpeg-%FFMPEG_VERSION%.tar.gz"
set "SOURCE_PARTIAL=%SOURCE_ARCHIVE%.part"

if not exist "%SOURCE_ARCHIVE_DIR%" mkdir "%SOURCE_ARCHIVE_DIR%"
if exist "%SOURCE_ARCHIVE%" (
    echo Using existing FFmpeg %FFMPEG_VERSION% source archive.
)
if not exist "%SOURCE_ARCHIVE%" (
    where curl.exe >nul 2>nul
    if errorlevel 1 (
        echo curl.exe was not found.
        exit /b 1
    )
    if exist "%SOURCE_PARTIAL%" del /Q "%SOURCE_PARTIAL%"
    echo Downloading FFmpeg %FFMPEG_VERSION%...
    curl.exe ^
        --location ^
        --fail ^
        --show-error ^
        --retry 5 ^
        --retry-all-errors ^
        --retry-delay 5 ^
        --retry-max-time 900 ^
        --connect-timeout 15 ^
        --max-time 300 ^
        --output "%SOURCE_PARTIAL%" ^
        "https://github.com/FFmpeg/FFmpeg/archive/%FFMPEG_COMMIT%.tar.gz"
    set "CURL_EXIT=!errorlevel!"
    if not "!CURL_EXIT!"=="0" (
        if exist "%SOURCE_PARTIAL%" del /Q "%SOURCE_PARTIAL%"
        exit /b !CURL_EXIT!
    )
    move /Y "%SOURCE_PARTIAL%" "%SOURCE_ARCHIVE%" >nul
    if errorlevel 1 exit /b 1
)
if not exist "%SOURCE_ARCHIVE%" (
    echo FFmpeg source archive was not found.
    exit /b 1
)

if exist "%SOURCE_DIR%" rmdir /S /Q "%SOURCE_DIR%"
if exist "%EXTRACTED_DIR%" rmdir /S /Q "%EXTRACTED_DIR%"
"%TAR%" -xf "%SOURCE_ARCHIVE%" -C "%CPP_DIR%"
if errorlevel 1 exit /b %errorlevel%
if not exist "%EXTRACTED_DIR%\configure" (
    echo FFmpeg source folder was not found after extraction.
    exit /b 1
)
if not exist "%EXTRACTED_DIR%\RELEASE" (
    echo FFmpeg release version file was not found after extraction.
    exit /b 1
)
set "EXTRACTED_FFMPEG_VERSION="
set /p EXTRACTED_FFMPEG_VERSION=<"%EXTRACTED_DIR%\RELEASE"
if not "!EXTRACTED_FFMPEG_VERSION!"=="!FFMPEG_VERSION!" (
    echo FFmpeg release version mismatch: expected !FFMPEG_VERSION!, got !EXTRACTED_FFMPEG_VERSION!.
    exit /b 1
)
ren "%EXTRACTED_DIR%" "ffmpeg"

echo FFmpeg source is ready: %SOURCE_DIR%
exit /b 0
