@echo off
setlocal EnableExtensions EnableDelayedExpansion

if "%~4"=="" exit /b 2
for %%I in ("%~1") do set "SOURCE_DIR=%%~fI"
for %%I in ("%~2") do set "BUILD_DIR=%%~fI"
for %%I in ("%~3") do set "INSTALL_DIR=%%~fI"
for %%I in ("%~4") do set "GNU_MAKE=%%~fI"
set "WORK_SOURCE_DIR=%BUILD_DIR%\src"
set "BUILD_JOBS=%LUDORK_FFMPEG_BUILD_JOBS%"
if not defined BUILD_JOBS set "BUILD_JOBS=1"
cd /d "%~dp0"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo Visual Studio Installer vswhere.exe was not found.
    exit /b 1
)
for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS_PATH=%%I"
if not defined VS_PATH (
    echo Visual Studio C++ build tools were not found.
    exit /b 1
)

set "BASH=%ProgramFiles%\Git\bin\bash.exe"
if not exist "%BASH%" set "BASH="
if not defined BASH for /f "tokens=*" %%I in ('where bash.exe 2^>nul') do if not defined BASH set "BASH=%%I"
if not exist "%BASH%" (
    echo Git Bash was not found.
    exit /b 1
)
for %%I in ("%BASH%") do set "GIT_ROOT=%%~dpI.."
for %%I in ("%GIT_ROOT%") do set "GIT_ROOT=%%~fI"
set "PATCH=%GIT_ROOT%\usr\bin\patch.exe"
if not exist "%PATCH%" (
    echo patch.exe was not found in the Git installation.
    exit /b 1
)
set "PATH=%GIT_ROOT%\usr\bin;%PATH%"

call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
set "CL=/experimental:deterministic /pathmap:%WORK_SOURCE_DIR%=. %CL%"

if exist "%BUILD_DIR%" rmdir /S /Q "%BUILD_DIR%"
if exist "%BUILD_DIR%" (
    echo FFmpeg build directory could not be cleaned: %BUILD_DIR%
    exit /b 1
)
if exist "%INSTALL_DIR%" rmdir /S /Q "%INSTALL_DIR%"
if exist "%INSTALL_DIR%" (
    echo FFmpeg install directory could not be cleaned: %INSTALL_DIR%
    exit /b 1
)
mkdir "%WORK_SOURCE_DIR%"
if errorlevel 1 exit /b %errorlevel%
mkdir "%INSTALL_DIR%"
if errorlevel 1 exit /b %errorlevel%
robocopy "%SOURCE_DIR%" "%WORK_SOURCE_DIR%" /E /NFL /NDL /NJH /NJS /NP
if errorlevel 8 exit /b %errorlevel%
"%PATCH%" -d "%WORK_SOURCE_DIR%" -p1 -i "%~dp0msvc-localized-output.patch"
if errorlevel 1 exit /b %errorlevel%

pushd "%BUILD_DIR%"
"%BASH%" "%WORK_SOURCE_DIR%\configure" ^
    --prefix=../install ^
    --toolchain=msvc ^
    --target-os=win32 ^
    --arch=x86_64 ^
    --enable-shared ^
    --disable-static ^
    --disable-gpl ^
    --disable-version3 ^
    --disable-nonfree ^
    --disable-everything ^
    --disable-autodetect ^
    --disable-programs ^
    --disable-doc ^
    --disable-network ^
    --disable-avdevice ^
    --disable-avfilter ^
    --disable-hwaccels ^
    --disable-x86asm ^
    --disable-runtime-cpudetect ^
    --disable-debug ^
    --enable-small ^
    --enable-pic ^
    --enable-protocol=file ^
    --enable-demuxer=mov ^
    --enable-decoder=h264,aac ^
    --enable-parser=h264,aac ^
    --enable-swscale ^
    --enable-swresample ^
    "--extra-cflags=-D_WIN32_WINNT=0x0602 -DWINVER=0x0602"
if errorlevel 1 (
    set "BUILD_RESULT=!ERRORLEVEL!"
    popd
    exit /b !BUILD_RESULT!
)
"%GNU_MAKE%" -j%BUILD_JOBS% "SHELL=%BASH%" CCDEP= CXXDEP= ASDEP= HOSTCCDEP= install
set "BUILD_RESULT=!ERRORLEVEL!"
popd
exit /b !BUILD_RESULT!
