@echo off
setlocal EnableExtensions EnableDelayedExpansion

if "%~6"=="" exit /b 2
for %%I in ("%~1") do set "SOURCE_DIR=%%~fI"
for %%I in ("%~2") do set "BUILD_DIR=%%~fI"
for %%I in ("%~3") do set "INSTALL_DIR=%%~fI"
for %%I in ("%~4") do set "GNU_MAKE=%%~fI"
set "TOOLCHAIN=%~5"
for %%I in ("%~6") do set "COMPILER=%%~fI"
if not "%TOOLCHAIN%"=="msvc" if not "%TOOLCHAIN%"=="mingw" exit /b 2
for %%I in ("%BUILD_DIR%\..") do if /I "%BUILD_DIR%"=="%%~fI" (
    echo The FFmpeg build directory must not be a filesystem root.
    exit /b 2
)
for %%I in ("%BUILD_DIR%" "%INSTALL_DIR%") do if /I "%%~fI"=="%SOURCE_DIR%" (
    echo FFmpeg output directories must not replace the source directory.
    exit /b 2
)
for %%I in ("%BUILD_DIR%\..\install") do if /I not "%%~fI"=="%INSTALL_DIR%" (
    echo The FFmpeg install directory must be the build directory's sibling named install.
    exit /b 2
)
set "FFMPEG_REVISION="
if exist "%SOURCE_DIR%\RELEASE" set /p "FFMPEG_REVISION="<"%SOURCE_DIR%\RELEASE"
if not defined FFMPEG_REVISION (
    echo FFmpeg release version could not be read: %SOURCE_DIR%\RELEASE
    exit /b 1
)
set "WORK_SOURCE_DIR=%BUILD_DIR%\src"
set "BUILD_JOBS=%LUDORK_FFMPEG_BUILD_JOBS%"
if not defined BUILD_JOBS set "BUILD_JOBS=%NUMBER_OF_PROCESSORS%"
if not defined BUILD_JOBS set "BUILD_JOBS=1"
cd /d "%~dp0"

set "BASH=%ProgramFiles%\Git\bin\bash.exe"
if not exist "%BASH%" set "BASH="
if not defined BASH for /f "tokens=*" %%I in ('where bash.exe 2^>nul') do if not defined BASH set "BASH=%%I"
if not exist "%BASH%" (
    echo Git Bash was not found.
    exit /b 1
)
for %%I in ("%BASH%") do set "GIT_USR_BIN=%%~dpI..\usr\bin"
if not exist "%GIT_USR_BIN%\sh.exe" for %%I in ("%BASH%") do set "GIT_USR_BIN=%%~dpI"
for %%I in ("%GIT_USR_BIN%") do set "GIT_USR_BIN=%%~fI"
if not exist "%GIT_USR_BIN%\sh.exe" (
    echo The Git Bash shell utilities were not found.
    exit /b 1
)
set "PATH=%GIT_USR_BIN%;%PATH%"

if "%TOOLCHAIN%"=="mingw" (
    if not exist "%COMPILER%" (
        echo The MinGW compiler was not found: %COMPILER%
        exit /b 1
    )
    for %%I in ("%COMPILER%") do (
        set "COMPILER_DIRECTORY=%%~dpI"
        set "FFMPEG_CC=%%~nxI"
    )
    for %%T in (ar nm ranlib strip windres dlltool) do if not exist "!COMPILER_DIRECTORY!%%T.exe" (
        echo The MinGW tool was not found: !COMPILER_DIRECTORY!%%T.exe
        exit /b 1
    )
    set "PATH=!COMPILER_DIRECTORY!;!PATH!"
    set "FFMPEG_TOOLCHAIN_ARGS=--target-os=mingw32 --ar=ar.exe --nm=nm.exe --ranlib=ranlib.exe --strip=strip.exe --windres=windres.exe --host-cc=!FFMPEG_CC! --host-ld=!FFMPEG_CC!"
    set "FFMPEG_LINK_FLAGS=-Wl,--no-insert-timestamp"
) else (
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if not exist "!VSWHERE!" (
        echo Visual Studio Installer vswhere.exe was not found.
        exit /b 1
    )
    for /f "usebackq tokens=*" %%I in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS_PATH=%%I"
    if not defined VS_PATH (
        echo Visual Studio C++ build tools were not found.
        exit /b 1
    )
    set "PATCH=%GIT_USR_BIN%\patch.exe"
    if not exist "!PATCH!" (
        echo patch.exe was not found in the Git installation.
        exit /b 1
    )
    call "!VS_PATH!\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
    if errorlevel 1 exit /b !errorlevel!
    set "CL=/experimental:deterministic "/pathmap:%WORK_SOURCE_DIR%=." !CL!"
    set "FFMPEG_CC=cl.exe"
    if defined LUDORK_FFMPEG_CC_LAUNCHER set "FFMPEG_CC=!LUDORK_FFMPEG_CC_LAUNCHER! cl.exe"
    set "FFMPEG_TOOLCHAIN_ARGS=--toolchain=msvc --target-os=win32"
    set "FFMPEG_LINK_FLAGS=-Brepro -INCREMENTAL:NO"
)

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
if "%TOOLCHAIN%"=="msvc" (
    "%PATCH%" -d "%WORK_SOURCE_DIR%" -p1 -i "%~dp0msvc-localized-output.patch"
    if errorlevel 1 exit /b !errorlevel!
)

pushd "%BUILD_DIR%"
if errorlevel 1 exit /b %errorlevel%
"%BASH%" "%WORK_SOURCE_DIR%\configure" ^
    --prefix=../install ^
    %FFMPEG_TOOLCHAIN_ARGS% ^
    "--cc=%FFMPEG_CC%" ^
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
    "--extra-ldflags=%FFMPEG_LINK_FLAGS%" ^
    "--extra-cflags=-D_WIN32_WINNT=0x0602 -DWINVER=0x0602"
if errorlevel 1 (
    set "BUILD_RESULT=!ERRORLEVEL!"
    popd
    exit /b !BUILD_RESULT!
)
"%GNU_MAKE%" -j%BUILD_JOBS% "SHELL=%BASH%" "REVISION=%FFMPEG_REVISION%" CCDEP= CXXDEP= ASDEP= HOSTCCDEP= install
set "BUILD_RESULT=!ERRORLEVEL!"
popd
exit /b !BUILD_RESULT!
