@echo off
setlocal EnableExtensions
chcp 65001>nul
cd /d "%~dp0.."
if not defined CMAKE_BUILD_PARALLEL_LEVEL set "CMAKE_BUILD_PARALLEL_LEVEL=%NUMBER_OF_PROCESSORS%"

if "%~1"=="" goto usage

if /I "%~1"=="Debug" (
    if not "%~2"=="" goto usage
    set "CPP_DIR=%CD%\Sample"
    set "CONFIG=%~1"
) else if /I "%~1"=="Release" (
    if not "%~2"=="" goto usage
    set "CPP_DIR=%CD%\Sample"
    set "CONFIG=%~1"
) else (
    if "%~2"=="" goto usage
    if not "%~3"=="" goto usage
    set "CPP_DIR=%~1"
    set "CONFIG=%~2"
)

if /I not "%CONFIG%"=="Debug" if /I not "%CONFIG%"=="Release" (
    echo Configuration must be Debug or Release.
    goto usage
)

for %%I in ("%CPP_DIR%") do set "CPP_DIR=%%~fI"

if not exist "%CPP_DIR%\CMakeLists.txt" (
    echo CMakeLists.txt was not found: %CPP_DIR%
    exit /b 1
)

set "SCRIPT_TOOLS=%CD%\.tools\ScriptTools\ScriptTools.exe"
set "LUAC_CACHE=%CD%\.tools\Lua\luac.exe"
if not exist "%SCRIPT_TOOLS%" (
    echo ScriptTools was not found. Run tools\init.bat first.
    exit /b 1
)
echo Project: %CPP_DIR%
echo Configuration: %CONFIG%
echo Parallel jobs: %CMAKE_BUILD_PARALLEL_LEVEL%
"%SCRIPT_TOOLS%" ui-assets validate "%CPP_DIR%"
if errorlevel 1 exit /b %errorlevel%

set "GNU_MAKE=%CD%\.tools\gnu-make\gnumake.exe"
findstr /R /C:"\"ffmpeg\"[ ]*:[ ]*true" "%CPP_DIR%\Main.proj" >nul 2>nul
if not errorlevel 1 if not exist "%GNU_MAKE%" (
    echo GNU Make was not found. Run tools\init.bat first.
    exit /b 1
)

set "BUILD_DIR=%CPP_DIR%\build"
cmake -S "%CPP_DIR%" -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=%CONFIG% "-DLUDORK_SCRIPT_TOOLS_EXECUTABLE=%SCRIPT_TOOLS%" "-DLUDORK_LUAC_CACHE_FILE=%LUAC_CACHE%" "-DLUDORK_GNU_MAKE_EXECUTABLE=%GNU_MAKE%"
if errorlevel 1 exit /b %errorlevel%

cmake --build "%BUILD_DIR%" --config "%CONFIG%" --target Main --parallel %CMAKE_BUILD_PARALLEL_LEVEL%
if errorlevel 1 exit /b %errorlevel%

set "OUTPUT_DIR=%CPP_DIR%\bin\%CONFIG%"
if not exist "%OUTPUT_DIR%\Main.exe" (
    echo Build finished without producing %OUTPUT_DIR%\Main.exe
    exit /b 1
)

echo Build complete: %OUTPUT_DIR%\Main.exe
exit /b 0

:usage
echo Usage: tools\build_cpp.bat ^<Debug^|Release^>
echo        tools\build_cpp.bat ^<cpp-folder^> ^<Debug^|Release^>
exit /b 1
