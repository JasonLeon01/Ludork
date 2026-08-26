@echo off
setlocal EnableExtensions
chcp 65001>nul
cd /d "%~dp0.."
if not defined CMAKE_BUILD_PARALLEL_LEVEL set "CMAKE_BUILD_PARALLEL_LEVEL=%NUMBER_OF_PROCESSORS%"

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Release"
if not "%~2"=="" goto usage
if /I not "%CONFIG%"=="Debug" if /I not "%CONFIG%"=="Release" goto usage

set "PROJECT_DIR=%CD%\UiPreviewHost"
set "BUILD_DIR=%CD%\.tools\UiPreviewHost\build"
set "SCRIPT_TOOLS=%CD%\.tools\ScriptTools\ScriptTools.exe"
set "GNU_MAKE=%CD%\.tools\gnu-make\gnumake.exe"

if not exist "%SCRIPT_TOOLS%" (
    echo ScriptTools was not found. Run tools\init.bat first.
    exit /b 1
)
if not exist "%GNU_MAKE%" (
    echo GNU Make was not found. Run tools\init.bat first.
    exit /b 1
)

cmake -S "%PROJECT_DIR%" -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=%CONFIG% "-DLUDORK_SCRIPT_TOOLS_EXECUTABLE=%SCRIPT_TOOLS%" "-DLUDORK_GNU_MAKE_EXECUTABLE=%GNU_MAKE%"
if errorlevel 1 exit /b %errorlevel%

echo Parallel jobs: %CMAKE_BUILD_PARALLEL_LEVEL%
cmake --build "%BUILD_DIR%" --config "%CONFIG%" --target UiPreviewHost --parallel %CMAKE_BUILD_PARALLEL_LEVEL%
if errorlevel 1 exit /b %errorlevel%

set "OUTPUT=%CD%\.tools\UiPreviewHost\bin\%CONFIG%\UiPreviewHost.exe"
if not exist "%OUTPUT%" (
    echo Build finished without producing %OUTPUT%
    exit /b 1
)
set "RUNTIME=%CD%\.tools\UiPreviewHost\bin\%CONFIG%\UiPreviewHostRuntime.dll"
if not exist "%RUNTIME%" (
    echo Build finished without producing %RUNTIME%
    exit /b 1
)

echo UI preview host ready: %OUTPUT%
exit /b 0

:usage
echo Usage: tools\build_ui_preview_host.bat [Debug^|Release]
exit /b 1
