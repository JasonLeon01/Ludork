@echo off
setlocal EnableExtensions DisableDelayedExpansion
chcp 65001>nul
set "PYTHONIOENCODING=utf-8"
for %%I in ("%~dp0.") do set "TOOLS_DIR=%%~fI"
for %%I in ("%TOOLS_DIR%\..") do set "ROOT_DIR=%%~fI"
cd /d "%ROOT_DIR%"

set "USE_LUAC=0"
set "ENCRYPT_SHADERS=0"
set "ENCRYPT_DATA=0"
set "ENCRYPT_SAVES=0"
set "USE_LDPAK=0"
:parse_options
if /I "%~1"=="--compile-lua" (
    set "USE_LUAC=1"
    shift
    goto parse_options
)
if /I "%~1"=="--encrypt-shaders" (
    set "ENCRYPT_SHADERS=1"
    shift
    goto parse_options
)
if /I "%~1"=="--encrypt-data" (
    set "ENCRYPT_DATA=1"
    shift
    goto parse_options
)
if /I "%~1"=="--encrypt-saves" (
    set "ENCRYPT_SAVES=1"
    shift
    goto parse_options
)
if /I "%~1"=="--use-ldpak" (
    set "USE_LDPAK=1"
    shift
    goto parse_options
)
if "%~1"=="" goto usage
if not "%~3"=="" goto usage

for %%I in ("%~1") do set "PROJECT_DIR=%%~fI"
if "%~2"=="" (
    set "DIST_ROOT=%PROJECT_DIR%\dist"
) else (
    for %%I in ("%~2") do set "DIST_ROOT=%%~fI"
)

set "PROJECT_FILE=%PROJECT_DIR%\Main.proj"
set "SCRIPT_TOOLS=%TOOLS_DIR%\ScriptTools\ScriptTools.exe"
if not exist "%SCRIPT_TOOLS%" set "SCRIPT_TOOLS=%ROOT_DIR%\.tools\ScriptTools\ScriptTools.exe"
if not exist "%SCRIPT_TOOLS%" (
    echo ScriptTools was not found. Run tools\init.bat first.
    exit /b 1
)
if not exist "%PROJECT_FILE%" (
    echo Main.proj was not found: %PROJECT_FILE%
    exit /b 1
)
"%SCRIPT_TOOLS%" packaging-constants check-app-name "%PROJECT_DIR%"
if errorlevel 1 exit /b %errorlevel%
set "EDITOR_CACHE_DIRECTORY="
for /f "delims=" %%V in ('""%SCRIPT_TOOLS%" packaging-constants list editor-cache-directory --separator space"') do set "EDITOR_CACHE_DIRECTORY=%%V"
if not defined EDITOR_CACHE_DIRECTORY exit /b 1
if "%USE_LDPAK%"=="1" (
    "%SCRIPT_TOOLS%" validate-ldpak-source "%PROJECT_DIR%"
    if errorlevel 1 exit /b 1
)

set "DIST_DIR="
set "NAME_OUTPUT=%TEMP%\ludork-pack-output-%RANDOM%-%RANDOM%.txt"
"%SCRIPT_TOOLS%" packaging-constants prepare-output "%PROJECT_DIR%" "%DIST_ROOT%" > "%NAME_OUTPUT%"
set "PREPARE_EXIT_CODE=%ERRORLEVEL%"
if "%PREPARE_EXIT_CODE%"=="0" for /f "usebackq delims=" %%V in ("%NAME_OUTPUT%") do set "DIST_DIR=%%V"
del /Q "%NAME_OUTPUT%"
if not "%PREPARE_EXIT_CODE%"=="0" exit /b %PREPARE_EXIT_CODE%
if not defined DIST_DIR exit /b 1

findstr /R /C:"\"Cpp\"[ ]*:[ ]*true" "%PROJECT_FILE%" >nul 2>nul
if not errorlevel 1 goto pack_cpp
goto pack_standalone

:pack_standalone
if "%ENCRYPT_SAVES%"=="1" (
    echo The --encrypt-saves packaging option requires a C++ Source project. Standalone projects can set SAVE_AS_LDC = true globally before all require calls in Scripts/Entry.lua.
    exit /b 1
)
if "%USE_LDPAK%"=="1" (
    "%SCRIPT_TOOLS%" validate-ldpak-source "%PROJECT_DIR%"
    if errorlevel 1 exit /b 1
)
robocopy "%PROJECT_DIR%" "%DIST_DIR%" /E /XF *.proj *.pdb *.anim.json *.py *.pyc *.pyo "%PROJECT_DIR%\Binaries\UiPreviewHost.exe" "%PROJECT_DIR%\Binaries\UiPreviewHostRuntime.dll" /XD "%DIST_ROOT%" "%DIST_DIR%" "%PROJECT_DIR%\%EDITOR_CACHE_DIRECTORY%" "%PROJECT_DIR%\Cache" build bin dist dist-luac .venv __pycache__ /NFL /NDL /NJH /NJS /NP
if errorlevel 8 exit /b %errorlevel%

if not exist "%DIST_DIR%\Main.exe" (
    echo Standalone output is missing Main.exe.
    exit /b 1
)

goto complete_pack

:pack_cpp
set "CMAKE_FILE=%PROJECT_DIR%\CMakeLists.txt"
if not exist "%CMAKE_FILE%" (
    echo CMakeLists.txt was not found: %CMAKE_FILE%
    exit /b 1
)

set "LUDORK_VALIDATE_LDPAK_SOURCE=%USE_LDPAK%"
set "LUDORK_SAVE_AS_LDC=%ENCRYPT_SAVES%"
call "%TOOLS_DIR%\build_standalone.bat" "%PROJECT_DIR%" "%%DIST_DIR%%" Release
set "STANDALONE_EXIT_CODE=%ERRORLEVEL%"
set "LUDORK_VALIDATE_LDPAK_SOURCE="
set "LUDORK_SAVE_AS_LDC="
if not "%STANDALONE_EXIT_CODE%"=="0" exit /b %STANDALONE_EXIT_CODE%

if not exist "%DIST_DIR%\Main.exe" (
    echo Pack output is missing Main.exe.
    exit /b 1
)

:complete_pack
call :validate_runtime_layout
if errorlevel 1 exit /b %errorlevel%
call :finalize_package
if errorlevel 1 exit /b %errorlevel%
call :validate_runtime_layout
if errorlevel 1 exit /b %errorlevel%
echo Pack complete: "%DIST_DIR%"
exit /b 0

:finalize_package
set "UI_REGISTRY="
for /f "usebackq delims=" %%R in (`call "%SCRIPT_TOOLS%" ui-preview registry "%PROJECT_DIR%"`) do set "UI_REGISTRY=%%R"
if not defined UI_REGISTRY exit /b 1
set "FINALIZE_OPTIONS="
if "%USE_LUAC%"=="1" set "FINALIZE_OPTIONS=%FINALIZE_OPTIONS% --compile-lua"
if "%ENCRYPT_SHADERS%"=="1" set "FINALIZE_OPTIONS=%FINALIZE_OPTIONS% --encrypt-shaders"
if "%ENCRYPT_DATA%"=="1" set "FINALIZE_OPTIONS=%FINALIZE_OPTIONS% --encrypt-data"
if "%USE_LDPAK%"=="1" set "FINALIZE_OPTIONS=%FINALIZE_OPTIONS% --use-ldpak"
"%SCRIPT_TOOLS%" finalize-package %FINALIZE_OPTIONS% --registry "%UI_REGISTRY%" "%DIST_DIR%"
if errorlevel 1 exit /b %errorlevel%
for %%F in (UiPreviewHost.exe UiPreviewHostRuntime.dll) do if exist "%DIST_DIR%\Binaries\%%F" exit /b 1
if exist "%DIST_DIR%\%EDITOR_CACHE_DIRECTORY%" exit /b 1
exit /b %errorlevel%

:validate_runtime_layout
if not exist "%DIST_DIR%\Main.exe" (
    echo Pack output is missing root Main.exe.
    exit /b 1
)
if not exist "%DIST_DIR%\Binaries\Main.exe" (
    echo Pack output is missing Binaries\Main.exe.
    exit /b 1
)
set "RUNTIME_LIBRARY_FOUND=0"
for %%F in ("%DIST_DIR%\*.dll" "%DIST_DIR%\*.so" "%DIST_DIR%\*.dylib" "%DIST_DIR%\*.so.*") do if exist "%%~fF" if not exist "%%~fF\" (
    echo Runtime library exists outside Binaries: "%%~fF"
    exit /b 1
)
for %%F in ("%DIST_DIR%\Binaries\*.dll") do if exist "%%~fF" if not exist "%%~fF\" set "RUNTIME_LIBRARY_FOUND=1"
if "%RUNTIME_LIBRARY_FOUND%"=="0" (
    echo Pack output contains no runtime DLLs in Binaries.
    exit /b 1
)
exit /b 0

:usage
echo Usage: tools\pack_project.bat [--compile-lua] [--encrypt-shaders] [--encrypt-data] [--encrypt-saves] [--use-ldpak] ^<project-folder^> [dist-folder]
exit /b 1
