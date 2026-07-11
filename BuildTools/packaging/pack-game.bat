@echo off
setlocal EnableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "ROOT=%%~fI"
set "VENV=%ROOT%\LudorkEnv"
set "PYTHON=%VENV%\Scripts\python.exe"
set "LIB=%SCRIPT_DIR%_lib.bat"

set "PROJ_PATH="
set "DIST_PATH="
set "PYTHON_EXE="
set "PLATFORM=win32"
set "INCLUDE_PYAV="
set "ENTRY_NAME=Entry"

:parse_args
if "%~1"=="" goto args_done
if "%~1"=="--proj-path" (
    set "PROJ_PATH=%~2"
    shift
    shift
    goto parse_args
)
if "%~1"=="--dist-path" (
    set "DIST_PATH=%~2"
    shift
    shift
    goto parse_args
)
if "%~1"=="--python" (
    set "PYTHON_EXE=%~2"
    shift
    shift
    goto parse_args
)
if "%~1"=="--platform" (
    set "PLATFORM=%~2"
    shift
    shift
    goto parse_args
)
if "%~1"=="--include-pyav" (
    set "INCLUDE_PYAV=1"
    shift
    goto parse_args
)
call "%LIB%" err "Unknown argument: %~1"
exit /b 1

:args_done
if "%PROJ_PATH%"=="" (
    call "%LIB%" err "Missing --proj-path"
    exit /b 1
)
if "%DIST_PATH%"=="" (
    call "%LIB%" err "Missing --dist-path"
    exit /b 1
)
if "%PYTHON_EXE%"=="" set "PYTHON_EXE=%PYTHON%"
if not exist "%PYTHON_EXE%" (
    call "%LIB%" err "Python executable not found: %PYTHON_EXE%"
    exit /b 1
)
if /I not "%PLATFORM%"=="win32" (
    call "%LIB%" err "Unsupported platform for pack-game.bat: %PLATFORM%"
    exit /b 1
)
if not exist "%PROJ_PATH%\Entry.py" (
    call "%LIB%" err "Entry.py not found in %PROJ_PATH%"
    exit /b 1
)

if not exist "%DIST_PATH%" mkdir "%DIST_PATH%"

call "%LIB%" step "Using Python: %PYTHON_EXE%"
call "%LIB%" step "Project path: %PROJ_PATH%"
call "%LIB%" step "Dist path: %DIST_PATH%"
call "%LIB%" step "Cleaning previous Nuitka artifacts..."
for %%S in (.build .dist .onefile-build) do (
    if exist "%DIST_PATH%\%ENTRY_NAME%%%S" rmdir /s /q "%DIST_PATH%\%ENTRY_NAME%%%S" 2>nul
)
if exist "%PROJ_PATH%\nuitka-crash-report.xml" del /q "%PROJ_PATH%\nuitka-crash-report.xml" >nul 2>&1

call "%LIB%" pip_ensure "%PYTHON_EXE%" nuitka "-U"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
if "%INCLUDE_PYAV%"=="1" (
    call "%LIB%" pip_ensure "%PYTHON_EXE%" av "-U"
    if !ERRORLEVEL! neq 0 exit /b !ERRORLEVEL!
)

set "FLAGS=--remove-output"
set "FLAGS=%FLAGS% --assume-yes-for-downloads"
set "FLAGS=%FLAGS% "--output-dir=%DIST_PATH%""
set "FLAGS=%FLAGS% --output-filename=Main"
set "FLAGS=%FLAGS% --include-data-dir=Assets=Assets"
set "FLAGS=%FLAGS% --include-data-dir=Data=Data"
set "FLAGS=%FLAGS% --include-package=pysf"
if exist "%PROJ_PATH%\Main.ini" set "FLAGS=%FLAGS% --include-data-file=Main.ini=Main.ini"
if "%INCLUDE_PYAV%"=="1" set "FLAGS=%FLAGS% --include-module=av"
set "FLAGS=%FLAGS% --standalone --windows-console-mode=disable"
if exist "%PROJ_PATH%\Assets\System\icon.ico" set "FLAGS=%FLAGS% "--windows-icon-from-ico=%PROJ_PATH%\Assets\System\icon.ico""

call "%LIB%" step "Running Nuitka build for game..."
pushd "%PROJ_PATH%"
echo Running Nuitka: "%PYTHON_EXE%" -u -m nuitka %FLAGS% %ENTRY_NAME%.py
"%PYTHON_EXE%" -u -m nuitka %FLAGS% %ENTRY_NAME%.py
set "PACK_RC=%ERRORLEVEL%"
popd

if "%PACK_RC%"=="0" (
    call "%LIB%" log "Game packaging complete. Output: %DIST_PATH%\%ENTRY_NAME%.dist"
)
exit /b %PACK_RC%
