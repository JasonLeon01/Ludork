@echo off
setlocal EnableExtensions
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
    set "DIST_DIR=%PROJECT_DIR%\dist"
) else (
    for %%I in ("%~2") do set "DIST_DIR=%%~fI"
)

set "PROJECT_FILE=%PROJECT_DIR%\Main.proj"
set "SCRIPT_TOOLS=%TOOLS_DIR%\ScriptTools.exe"
if not exist "%SCRIPT_TOOLS%" set "SCRIPT_TOOLS=%ROOT_DIR%\.tools\ScriptTools\ScriptTools.exe"
if not exist "%SCRIPT_TOOLS%" (
    echo ScriptTools was not found. Run tools\init.bat first.
    exit /b 1
)
if not exist "%PROJECT_FILE%" (
    echo Main.proj was not found: %PROJECT_FILE%
    exit /b 1
)
set "ENTRY_FILE=%PROJECT_DIR%\Scripts\Entry.lua"
if not exist "%ENTRY_FILE%" (
    echo Lua entry script was not found: %ENTRY_FILE%
    exit /b 1
)
findstr /R /C:"^[ ]*local[ ][ ]*APP_NAME[ ]*=[ ]*\"LudorkSample\"[ ]*$" "%ENTRY_FILE%" >nul 2>nul
if not errorlevel 1 (
    echo Change APP_NAME in Scripts/Entry.lua from LudorkSample to a name unique to your game before packaging.
    exit /b 24
)
if "%USE_LDPAK%"=="1" (
    "%SCRIPT_TOOLS%" validate-ldpak-source "%PROJECT_DIR%"
    if errorlevel 1 exit /b 1
)

if exist "%DIST_DIR%" rmdir /S /Q "%DIST_DIR%"
mkdir "%DIST_DIR%"

findstr /R /C:"\"Cpp\"[ ]*:[ ]*true" "%PROJECT_FILE%" >nul 2>nul
if not errorlevel 1 goto pack_cpp
goto pack_standalone

:pack_standalone
if "%ENCRYPT_SAVES%"=="1" (
    echo --encrypt-saves requires a C++ Source project because the setting is compiled into the runtime.
    exit /b 1
)
if "%USE_LDPAK%"=="1" (
    "%SCRIPT_TOOLS%" validate-ldpak-source "%PROJECT_DIR%"
    if errorlevel 1 exit /b 1
)
robocopy "%PROJECT_DIR%" "%DIST_DIR%" /E /XF *.proj *.pdb *.anim.json *.py *.pyc *.pyo UiPreviewHost* UiPreviewCurveResolver* /XD build bin dist dist-luac .venv __pycache__ UiPreviewHost UiPreviewCurveResolver /NFL /NDL /NJH /NJS /NP
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
call "%TOOLS_DIR%\build_standalone.bat" "%PROJECT_DIR%" "%DIST_DIR%" Release
set "STANDALONE_EXIT_CODE=%ERRORLEVEL%"
set "LUDORK_VALIDATE_LDPAK_SOURCE="
set "LUDORK_SAVE_AS_LDC="
if not "%STANDALONE_EXIT_CODE%"=="0" exit /b %STANDALONE_EXIT_CODE%

if not exist "%DIST_DIR%\Main.exe" (
    echo Pack output is missing Main.exe.
    exit /b 1
)

:complete_pack
call :validate_runtime_layout "%DIST_DIR%"
if errorlevel 1 exit /b %errorlevel%
call :finalize_package
if errorlevel 1 exit /b %errorlevel%
call :validate_runtime_layout "%DIST_DIR%"
if errorlevel 1 exit /b %errorlevel%
echo Pack complete: %DIST_DIR%
exit /b 0

:finalize_package
set "FINALIZE_OPTIONS="
if "%USE_LUAC%"=="1" set "FINALIZE_OPTIONS=%FINALIZE_OPTIONS% --compile-lua"
if "%ENCRYPT_SHADERS%"=="1" set "FINALIZE_OPTIONS=%FINALIZE_OPTIONS% --encrypt-shaders"
if "%ENCRYPT_DATA%"=="1" set "FINALIZE_OPTIONS=%FINALIZE_OPTIONS% --encrypt-data"
if "%USE_LDPAK%"=="1" set "FINALIZE_OPTIONS=%FINALIZE_OPTIONS% --use-ldpak"
"%SCRIPT_TOOLS%" finalize-package %FINALIZE_OPTIONS% "%DIST_DIR%"
if errorlevel 1 exit /b %errorlevel%
call :validate_no_ui_preview_host "%DIST_DIR%"
exit /b %errorlevel%

:validate_no_ui_preview_host
for /r "%~1" %%F in (UiPreviewHost* UiPreviewCurveResolver*) do if exist "%%~fF" (
    echo UI preview host entry was found in a game package: %%~fF
    exit /b 1
)
for /d /r "%~1" %%D in (UiPreviewHost* UiPreviewCurveResolver*) do if exist "%%~fD" (
    echo UI preview host entry was found in a game package: %%~fD
    exit /b 1
)
exit /b 0

:validate_runtime_layout
if not exist "%~1\Main.exe" (
    echo Pack output is missing root Main.exe.
    exit /b 1
)
if not exist "%~1\Binaries\Main.exe" (
    echo Pack output is missing Binaries\Main.exe.
    exit /b 1
)
set "RUNTIME_LIBRARY_FOUND=0"
for %%E in (dll so dylib) do for /f "delims=" %%F in ('dir /B /A-D "%~1\*.%%E" 2^>nul') do (
    echo Runtime library exists outside Binaries: %~1\%%F
    exit /b 1
)
for /f "delims=" %%F in ('dir /B /A-D "%~1\*.so.*" 2^>nul') do (
    echo Runtime library exists outside Binaries: %~1\%%F
    exit /b 1
)
for /f "delims=" %%F in ('dir /B /A-D "%~1\Binaries\*.dll" 2^>nul') do set "RUNTIME_LIBRARY_FOUND=1"
if "%RUNTIME_LIBRARY_FOUND%"=="0" (
    echo Pack output contains no runtime DLLs in Binaries.
    exit /b 1
)
exit /b 0

:usage
echo Usage: tools\pack_project.bat [--compile-lua] [--encrypt-shaders] [--encrypt-data] [--encrypt-saves] [--use-ldpak] ^<project-folder^> [dist-folder]
exit /b 1
