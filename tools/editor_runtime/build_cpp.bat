@echo off
setlocal EnableExtensions EnableDelayedExpansion
chcp 65001>nul
if not defined CMAKE_BUILD_PARALLEL_LEVEL set "CMAKE_BUILD_PARALLEL_LEVEL=2"

if "%~1"=="" goto usage
if "%~2"=="" goto usage
if not "%~3"=="" goto usage

for %%I in ("%~dp0..") do set "ROOT_DIR=%%~fI"
for %%I in ("%~1") do set "CPP_DIR=%%~fI"
set "CONFIG=%~2"

if /I not "%CONFIG%"=="Debug" if /I not "%CONFIG%"=="Release" goto usage
if not exist "%CPP_DIR%\CMakeLists.txt" (
    echo CMakeLists.txt was not found: %CPP_DIR%
    exit /b 1
)
if not exist "%CPP_DIR%\Main.proj" (
    echo Main.proj was not found: %CPP_DIR%
    exit /b 1
)

set "SCRIPT_TOOLS=%~dp0ScriptTools.exe"
if not exist "%SCRIPT_TOOLS%" if exist "%ROOT_DIR%\.tools\ScriptTools\ScriptTools.exe" (
    set "SCRIPT_TOOLS=%ROOT_DIR%\.tools\ScriptTools\ScriptTools.exe"
)
if not exist "%SCRIPT_TOOLS%" (
    echo ScriptTools was not found. Run tools\init.bat first.
    exit /b 1
)

set "GNU_MAKE="
if exist "%ROOT_DIR%\tools\gnu-make\gnumake.exe" (
    set "GNU_MAKE=%ROOT_DIR%\tools\gnu-make\gnumake.exe"
)
if not defined GNU_MAKE if exist "%ROOT_DIR%\.tools\gnu-make\gnumake.exe" (
    set "GNU_MAKE=%ROOT_DIR%\.tools\gnu-make\gnumake.exe"
)
if not defined GNU_MAKE (
    for /f "delims=" %%I in ('where gnumake.exe 2^>nul') do (
        if not defined GNU_MAKE set "GNU_MAKE=%%I"
    )
)
if not defined GNU_MAKE (
    for /f "delims=" %%I in ('where make.exe 2^>nul') do (
        if not defined GNU_MAKE set "GNU_MAKE=%%I"
    )
)

findstr /R /C:"\"ffmpeg\"[ ]*:[ ]*true" "%CPP_DIR%\Main.proj" >nul 2>nul
if not errorlevel 1 if not defined GNU_MAKE (
    echo GNU Make was not found in tools\gnu-make or PATH.
    exit /b 1
)

echo Project: %CPP_DIR%
echo Configuration: %CONFIG%
"%SCRIPT_TOOLS%" ui-assets validate "%CPP_DIR%"
if errorlevel 1 exit /b %errorlevel%
set "BUILD_DIR=%CPP_DIR%\build"
if defined GNU_MAKE (
    cmake -S "%CPP_DIR%" -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=%CONFIG% "-DLUDORK_SCRIPT_TOOLS_EXECUTABLE=%SCRIPT_TOOLS%" "-DLUDORK_GNU_MAKE_EXECUTABLE=%GNU_MAKE%"
) else (
    cmake -S "%CPP_DIR%" -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=%CONFIG% "-DLUDORK_SCRIPT_TOOLS_EXECUTABLE=%SCRIPT_TOOLS%"
)
if errorlevel 1 exit /b %errorlevel%

cmake --build "%BUILD_DIR%" --config "%CONFIG%" --target Main --parallel
if errorlevel 1 exit /b %errorlevel%

set "OUTPUT_DIR=%CPP_DIR%\bin\%CONFIG%"
if not exist "%OUTPUT_DIR%\Main.exe" (
    echo Build finished without producing %OUTPUT_DIR%\Main.exe
    exit /b 1
)

echo Build complete: %OUTPUT_DIR%\Main.exe
exit /b 0

:usage
echo Usage: tools\build_cpp.bat ^<cpp-folder^> ^<Debug^|Release^>
exit /b 1
