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

for /f "delims=" %%V in ("!FFMPEG_VERSION!") do set "FFMPEG_VERSION=%%V"

set "TAR=%SystemRoot%\System32\tar.exe"
if not exist "%TAR%" (
    echo tar.exe was not found.
    exit /b 1
)

set "SOURCE_DIR=%CPP_DIR%\ffmpeg"
set "EXTRACTED_DIR=%CPP_DIR%\ffmpeg-%FFMPEG_VERSION%"
set "SOURCE_ARCHIVE_DIR=%CPP_DIR%\ThirdPartySource"
set "SOURCE_ARCHIVE=%SOURCE_ARCHIVE_DIR%\ffmpeg-%FFMPEG_VERSION%.tar.xz"

if not exist "%SOURCE_ARCHIVE_DIR%" mkdir "%SOURCE_ARCHIVE_DIR%"
if not exist "%SOURCE_ARCHIVE%" (
    echo Downloading FFmpeg %FFMPEG_VERSION%...
    powershell -NoProfile -Command "Invoke-WebRequest -Uri ('https://ffmpeg.org/releases/ffmpeg-' + $env:FFMPEG_VERSION.Trim() + '.tar.xz') -OutFile $env:SOURCE_ARCHIVE.Trim()"
    if errorlevel 1 exit /b %errorlevel%
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
ren "%EXTRACTED_DIR%" "ffmpeg"

echo FFmpeg source is ready: %SOURCE_DIR%
exit /b 0
