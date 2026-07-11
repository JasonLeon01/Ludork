@echo off
setlocal EnableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "ROOT=%%~fI"
set "VENV=%ROOT%\LudorkEnv"
set "PYTHON=%VENV%\Scripts\python.exe"
set "LIB=%SCRIPT_DIR%_lib.bat"

if not exist "%PYTHON%" (
    call "%LIB%" err "Python executable not found: %PYTHON%"
    exit /b 1
)

call "%LIB%" pip_ensure "%PYTHON%" nuitka "-U"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

set "OUTDIR=%ROOT%\build"
set "QML_SOURCE=%ROOT%\EditorGlobal\Qml"
set "QML_PACK_DIR=%OUTDIR%\editor_qml_pack\EditorGlobal\Qml"
set "LOCALE_JSON=%ROOT%\Locale\locale.json"
set "LOCALE_BAK=%ROOT%\locale.json.bak"
set "PROJ=%ROOT%\Sample\Main.proj"
set "PROJ_BAK=%ROOT%\Main.proj.bak"
set "PACK_RC=0"
set "PROJ_REPLACED=0"
set "PACK_PROFILE_ARG=%~1"

for /f "tokens=1,* delims==" %%A in ('"%PYTHON%" "%ROOT%\tools\packaging\pack_profile.py" %PACK_PROFILE_ARG%') do (
    if /I "%%A"=="PROFILE" set "PACK_PROFILE=%%B"
    if /I "%%A"=="JOBS" set "PACK_JOBS=%%B"
    if /I "%%A"=="LTO" set "PACK_LTO=%%B"
    if /I "%%A"=="JOBS_LABEL" set "PACK_JOBS_LABEL=%%B"
)
if not defined PACK_PROFILE set "PACK_PROFILE=low"
if not defined PACK_LTO set "PACK_LTO=no"
if not defined PACK_JOBS_LABEL (
    if defined PACK_JOBS (set "PACK_JOBS_LABEL=%PACK_JOBS%") else (set "PACK_JOBS_LABEL=all")
)

if not exist "%LOCALE_JSON%" (
    call "%LIB%" err "Locale JSON file not found: %LOCALE_JSON%"
    exit /b 1
)

call "%LIB%" step "Generating locale pickles..."
"%PYTHON%" "%ROOT%\tools\localeTransfer.py" "%LOCALE_JSON%"
if %ERRORLEVEL% neq 0 (
    set "PACK_RC=%ERRORLEVEL%"
    goto cleanup
)

call "%LIB%" step "Preparing Qt QML tools..."
"%PYTHON%" "%ROOT%\tools\packaging\ensure_qml_tools.py"
if %ERRORLEVEL% neq 0 (
    set "PACK_RC=%ERRORLEVEL%"
    goto cleanup
)

call "%LIB%" step "Preparing editor QML resources..."
"%PYTHON%" "%ROOT%\tools\packaging\prepare_editor_qml.py" "%QML_SOURCE%" "%QML_PACK_DIR%"
if %ERRORLEVEL% neq 0 (
    set "PACK_RC=%ERRORLEVEL%"
    goto cleanup
)

if exist "%LOCALE_JSON%" move /Y "%LOCALE_JSON%" "%LOCALE_BAK%" >nul
if exist "%PROJ%" move /Y "%PROJ%" "%PROJ_BAK%" >nul
> "%PROJ%" echo {}
set "PROJ_REPLACED=1"

if exist "%ROOT%\BuildTools\Qt515" (
    call "%LIB%" step "Removing legacy Qt tool cache from BuildTools..."
    rmdir /S /Q "%ROOT%\BuildTools\Qt515"
)

set "NUITKA_FLAGS=--remove-output"
set "NUITKA_FLAGS=%NUITKA_FLAGS% "--output-dir=%OUTDIR%""
set "NUITKA_FLAGS=%NUITKA_FLAGS% "--company-name=Metempsychosis Game Studio""
set "NUITKA_FLAGS=%NUITKA_FLAGS% "--product-name=Ludork""
set "NUITKA_FLAGS=%NUITKA_FLAGS% "--file-version=1.0.0.0""
set "NUITKA_FLAGS=%NUITKA_FLAGS% "--product-version=1.0.0.0""
set "NUITKA_FLAGS=%NUITKA_FLAGS% "--file-description=Ludork Editor""
set "NUITKA_FLAGS=%NUITKA_FLAGS% "--copyright=Copyright (c) 2026 Ludork""
set "NUITKA_FLAGS=%NUITKA_FLAGS% --enable-plugin=pyqt5"
set "NUITKA_FLAGS=%NUITKA_FLAGS% --include-qt-plugins=platforms,styles,iconengines,imageformats,qml"
set "NUITKA_FLAGS=%NUITKA_FLAGS% --include-package-data=qt_material"
set "NUITKA_FLAGS=%NUITKA_FLAGS% --include-module=EditorGlobal"
set "NUITKA_FLAGS=%NUITKA_FLAGS% --include-package=Widgets"
set "NUITKA_FLAGS=%NUITKA_FLAGS% --include-package=Utils"
set "NUITKA_FLAGS=%NUITKA_FLAGS% "--include-data-dir=%ROOT%\Resource=Resource""
set "NUITKA_FLAGS=%NUITKA_FLAGS% "--include-data-dir=%ROOT%\Locale=Locale""
set "NUITKA_FLAGS=%NUITKA_FLAGS% "--include-data-dir=%ROOT%\Styles=Styles""
set "NUITKA_FLAGS=%NUITKA_FLAGS% "--include-data-dir=%QML_PACK_DIR%=EditorGlobal\Qml""
set "NUITKA_FLAGS=%NUITKA_FLAGS% "--include-data-dir=%ROOT%\BuildTools=BuildTools""
set "NUITKA_FLAGS=%NUITKA_FLAGS% --include-module=NodeGraphQt"
set "NUITKA_FLAGS=%NUITKA_FLAGS% --include-package=debugpy"
set "NUITKA_FLAGS=%NUITKA_FLAGS% --include-module=asyncio"
set "NUITKA_FLAGS=%NUITKA_FLAGS% --include-module=psutil"
set "NUITKA_FLAGS=%NUITKA_FLAGS% --include-module=pympler.asizeof"
set "NUITKA_FLAGS=%NUITKA_FLAGS% --include-module=av"
set "NUITKA_FLAGS=%NUITKA_FLAGS% --include-module=openpyxl"
set "NUITKA_FLAGS=%NUITKA_FLAGS% --include-package=agent"
set "NUITKA_FLAGS=%NUITKA_FLAGS% "--include-data-dir=%ROOT%\agent=agent""
set "NUITKA_FLAGS=%NUITKA_FLAGS% --include-module=quickjs"
set "NUITKA_FLAGS=%NUITKA_FLAGS% --include-module=_quickjs"
set "NUITKA_FLAGS=%NUITKA_FLAGS% --include-package=openai"
set "NUITKA_FLAGS=%NUITKA_FLAGS% --include-module=PyQt5.QtSvg"
set "NUITKA_FLAGS=%NUITKA_FLAGS% --nofollow-import-to=Engine"
set "NUITKA_FLAGS=%NUITKA_FLAGS% --nofollow-import-to=Global"
set "NUITKA_FLAGS=%NUITKA_FLAGS% --nofollow-import-to=Source"
if defined PACK_JOBS set "NUITKA_FLAGS=%NUITKA_FLAGS% --jobs=%PACK_JOBS%"
set "NUITKA_FLAGS=%NUITKA_FLAGS% --lto=%PACK_LTO%"
if exist "%ROOT%\Resource\icon.ico" set "NUITKA_FLAGS=%NUITKA_FLAGS% "--windows-icon-from-ico=%ROOT%\Resource\icon.ico""
set "NUITKA_FLAGS=%NUITKA_FLAGS% --windows-console-mode=disable --standalone"

call "%LIB%" step "Running Nuitka build (profile: %PACK_PROFILE%, jobs: %PACK_JOBS_LABEL%, lto: %PACK_LTO%)..."
pushd "%ROOT%"
"%PYTHON%" -m nuitka %NUITKA_FLAGS% "%ROOT%\main.py"
set "PACK_RC=%ERRORLEVEL%"
popd
if not "%PACK_RC%"=="0" goto cleanup

set "RUNTIME=%OUTDIR%\main.dist"
call "%LIB%" step "Copying Sample directory..."
call "%LIB%" copytree "%ROOT%\Sample" "%RUNTIME%\Sample"
if %ERRORLEVEL% neq 0 (
    set "PACK_RC=%ERRORLEVEL%"
    goto cleanup
)
call "%LIB%" step "Copying Plugins directory..."
call "%LIB%" copytree "%ROOT%\Plugins" "%RUNTIME%\Plugins"
if %ERRORLEVEL% neq 0 set "PACK_RC=%ERRORLEVEL%"

:cleanup
call "%LIB%" step "Cleaning up packaging state..."
if exist "%LOCALE_BAK%" (
    if exist "%LOCALE_JSON%" del /q "%LOCALE_JSON%" >nul 2>&1
    move /Y "%LOCALE_BAK%" "%LOCALE_JSON%" >nul
)
if "%PROJ_REPLACED%"=="1" if exist "%PROJ%" del /q "%PROJ%" >nul 2>&1
if exist "%PROJ_BAK%" move /Y "%PROJ_BAK%" "%PROJ%" >nul

if "%PACK_RC%"=="0" (
    call "%LIB%" log "Packaging complete. Output: %RUNTIME%"
)
exit /b %PACK_RC%
