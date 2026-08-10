@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0.."

for /f "usebackq eol=# tokens=1,2 delims==" %%a in ("%CD%\versions.conf") do set "%%a=%%b"
if not defined GNU_MAKE_VERSION (
    echo GNU_MAKE_VERSION is not set in versions.conf.
    exit /b 1
)

for /f "delims=" %%V in ("!GNU_MAKE_VERSION!") do set "GNU_MAKE_VERSION=%%V"

set "OUTPUT_DIR=%CD%\.tools\gnu-make"
set "OUTPUT=%OUTPUT_DIR%\gnumake.exe"
if exist "%OUTPUT%" (
    "%OUTPUT%" --version >nul 2>nul
    if not errorlevel 1 exit /b 0
)

set "SOURCE_ARCHIVE_DIR=%CD%\.tools\sources"
set "SOURCE_ARCHIVE=%SOURCE_ARCHIVE_DIR%\make-%GNU_MAKE_VERSION%.tar.gz"
set "BUILD_ROOT=%CD%\.tools\build"
set "SOURCE_DIR=%BUILD_ROOT%\make-%GNU_MAKE_VERSION%"
if not exist "%SOURCE_ARCHIVE_DIR%" mkdir "%SOURCE_ARCHIVE_DIR%"
if not exist "%SOURCE_ARCHIVE%" (
    echo Downloading GNU Make %GNU_MAKE_VERSION%...
    powershell -NoProfile -Command "Invoke-WebRequest -Uri ('https://ftp.gnu.org/gnu/make/make-' + $env:GNU_MAKE_VERSION.Trim() + '.tar.gz') -OutFile $env:SOURCE_ARCHIVE.Trim()"
    if errorlevel 1 exit /b %errorlevel%
)
if not exist "%SOURCE_ARCHIVE%" (
    echo GNU Make source archive was not found.
    exit /b 1
)

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

if exist "%SOURCE_DIR%" rmdir /S /Q "%SOURCE_DIR%"
if not exist "%BUILD_ROOT%" mkdir "%BUILD_ROOT%"
"%SystemRoot%\System32\tar.exe" -xf "%SOURCE_ARCHIVE%" -C "%BUILD_ROOT%"
if errorlevel 1 exit /b %errorlevel%
if not exist "%SOURCE_DIR%\build_w32.bat" (
    echo GNU Make source folder was not found after extraction.
    exit /b 1
)

call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
pushd "%SOURCE_DIR%"
call build_w32.bat
set "BUILD_RESULT=%ERRORLEVEL%"
popd
if not "%BUILD_RESULT%"=="0" exit /b %BUILD_RESULT%
if not exist "%SOURCE_DIR%\WinRel\gnumake.exe" (
    echo GNU Make build did not produce gnumake.exe.
    exit /b 1
)

if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"
copy /Y "%SOURCE_DIR%\WinRel\gnumake.exe" "%OUTPUT%" >nul
if errorlevel 1 exit /b %errorlevel%
echo GNU Make is ready: %OUTPUT%
exit /b 0
