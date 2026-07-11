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

set "PACK_PROFILE=low"
set "PACK_JOBS="
set "PACK_LTO=no"
set "PACK_JOBS_LABEL=all"
set "REMOVABLE_FILES="
for /f "tokens=1,* delims==" %%A in ('call "%PYTHON_EXE%" "%ROOT%\tools\packaging\pack_profile.py"') do (
    if /I "%%A"=="PROFILE" set "PACK_PROFILE=%%B"
    if /I "%%A"=="JOBS" set "PACK_JOBS=%%B"
    if /I "%%A"=="LTO" set "PACK_LTO=%%B"
    if /I "%%A"=="JOBS_LABEL" set "PACK_JOBS_LABEL=%%B"
)

set "FLAGS=--remove-output"
set "FLAGS=%FLAGS% --assume-yes-for-downloads"
set "FLAGS=%FLAGS% "--output-dir=%DIST_PATH%""
set "FLAGS=%FLAGS% --output-filename=Main"
set "FLAGS=%FLAGS% --include-data-dir=Assets=Assets"
set "FLAGS=%FLAGS% --include-data-dir=Data=Data"
set "FLAGS=%FLAGS% --include-package=pysf"
if "%INCLUDE_PYAV%"=="1" set "FLAGS=%FLAGS% --include-module=av"
for /f "tokens=1,* delims==" %%A in ('call "%PYTHON_EXE%" "%ROOT%\tools\packaging\game_nuitka_slim.py" --platform "%PLATFORM%"') do (
    if /I "%%A"=="NOFOLLOW" set "FLAGS=!FLAGS! --nofollow-import-to=%%B"
    if /I "%%A"=="NOINCLUDE_DATA" set "FLAGS=!FLAGS! --noinclude-data-files=%%B"
    if /I "%%A"=="NOINCLUDE_DLL" set "FLAGS=!FLAGS! --noinclude-dlls=%%B"
    if /I "%%A"=="REMOVE_FILE" set "REMOVABLE_FILES=!REMOVABLE_FILES! %%B"
)
if defined PACK_JOBS set "FLAGS=%FLAGS% --jobs=%PACK_JOBS%"
set "FLAGS=%FLAGS% --lto=%PACK_LTO%"
set "FLAGS=%FLAGS% --standalone --windows-console-mode=disable"
if exist "%PROJ_PATH%\Assets\System\icon.ico" set "FLAGS=%FLAGS% "--windows-icon-from-ico=%PROJ_PATH%\Assets\System\icon.ico""

call "%LIB%" step "Running Nuitka build for game (profile: %PACK_PROFILE%, jobs: %PACK_JOBS_LABEL%, lto: %PACK_LTO%)..."
pushd "%PROJ_PATH%"
echo Running Nuitka: "%PYTHON_EXE%" -u -m nuitka %FLAGS% %ENTRY_NAME%.py
"%PYTHON_EXE%" -u -m nuitka %FLAGS% %ENTRY_NAME%.py
set "PACK_RC=%ERRORLEVEL%"
popd

if "%PACK_RC%"=="0" (
    for %%F in (!REMOVABLE_FILES!) do (
        if exist "%DIST_PATH%\%ENTRY_NAME%.dist\%%F" del /q "%DIST_PATH%\%ENTRY_NAME%.dist\%%F"
    )
    call "%LIB%" step "Pruning dev-only data files from package..."
    "%PYTHON_EXE%" "%ROOT%\tools\packaging\game_nuitka_slim.py" --prune-dist "%DIST_PATH%\%ENTRY_NAME%.dist"
    call "%LIB%" log "Game packaging complete. Output: %DIST_PATH%\%ENTRY_NAME%.dist"
)
exit /b %PACK_RC%
