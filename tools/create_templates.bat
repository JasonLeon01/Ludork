@echo off
setlocal EnableExtensions
cd /d "%~dp0.."

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Release"
if not "%~3"=="" goto usage
if /I not "%CONFIG%"=="Debug" if /I not "%CONFIG%"=="Release" (
    goto usage
)

set "SOURCE_DIR=%CD%\Sample"
if "%~2"=="" (
    set "TEMPLATES_DIR=%CD%\Templates"
) else (
    for %%I in ("%~2") do set "TEMPLATES_DIR=%%~fI"
)
set "CPP_TEMPLATE_DIR=%TEMPLATES_DIR%\Cpp"
set "STANDALONE_TEMPLATE_DIR=%TEMPLATES_DIR%\Standalone"
set "CPP_FFMPEG_TEMPLATE_DIR=%TEMPLATES_DIR%\Cpp-ffmpeg"
set "STANDALONE_FFMPEG_TEMPLATE_DIR=%TEMPLATES_DIR%\Standalone-ffmpeg"
set "SCRIPT_TOOLS=%CD%\.tools\ScriptTools\ScriptTools.exe"

if not exist "%SOURCE_DIR%\CMakeLists.txt" (
    echo Sample C++ project was not found: %SOURCE_DIR%
    exit /b 1
)
if not exist "%SCRIPT_TOOLS%" (
    echo ScriptTools was not found. Run tools\init.bat first.
    exit /b 1
)
if not exist "%SOURCE_DIR%\LuaSF" set "MISSING_DEPENDENCIES=1"
if not exist "%SOURCE_DIR%\lua-cjson" set "MISSING_DEPENDENCIES=1"
if not exist "%SOURCE_DIR%\zlib" set "MISSING_DEPENDENCIES=1"
if defined MISSING_DEPENDENCIES (
    echo Sample dependencies were not found. Run tools\init.bat first.
    exit /b 1
)
if not exist "%SOURCE_DIR%\ffmpeg\configure" (
    echo FFmpeg source was not found. Run tools\init.bat first.
    exit /b 1
)
if not exist "%SOURCE_DIR%\ThirdPartySource\ffmpeg-*.tar.xz" (
    echo The distributable FFmpeg source archive was not found. Run tools\init.bat first.
    exit /b 1
)
if not exist "%CD%\.tools\gnu-make\gnumake.exe" (
    echo GNU Make was not found. Run tools\init.bat first.
    exit /b 1
)

if exist "%CPP_TEMPLATE_DIR%" rmdir /S /Q "%CPP_TEMPLATE_DIR%"
if exist "%STANDALONE_TEMPLATE_DIR%" rmdir /S /Q "%STANDALONE_TEMPLATE_DIR%"
if exist "%CPP_FFMPEG_TEMPLATE_DIR%" rmdir /S /Q "%CPP_FFMPEG_TEMPLATE_DIR%"
if exist "%STANDALONE_FFMPEG_TEMPLATE_DIR%" rmdir /S /Q "%STANDALONE_FFMPEG_TEMPLATE_DIR%"
mkdir "%CPP_TEMPLATE_DIR%"
mkdir "%STANDALONE_TEMPLATE_DIR%"
mkdir "%CPP_FFMPEG_TEMPLATE_DIR%"
mkdir "%STANDALONE_FFMPEG_TEMPLATE_DIR%"

robocopy "%SOURCE_DIR%" "%CPP_TEMPLATE_DIR%" /E /XD "%SOURCE_DIR%\.venv" "%SOURCE_DIR%\build" "%SOURCE_DIR%\bin" "%SOURCE_DIR%\ffmpeg" "%SOURCE_DIR%\ThirdPartySource" "%SOURCE_DIR%\Log" "%SOURCE_DIR%\Save" "%SOURCE_DIR%\.vs" "%SOURCE_DIR%\.idea" "%SOURCE_DIR%\cmake-build-ludork-debug" __pycache__ UiPreviewHost UiPreviewCurveResolver /XF *.anim.json *.py *.pyc *.pyo *.log Main.ini Ludork.ini CMakeUserPresets.json generate_clion.sh UiPreviewHost* UiPreviewCurveResolver* /NFL /NDL /NJH /NJS /NP
if errorlevel 8 exit /b %errorlevel%
robocopy "%SOURCE_DIR%" "%CPP_FFMPEG_TEMPLATE_DIR%" /E /XD "%SOURCE_DIR%\.venv" "%SOURCE_DIR%\build" "%SOURCE_DIR%\bin" "%SOURCE_DIR%\Log" "%SOURCE_DIR%\Save" "%SOURCE_DIR%\.vs" "%SOURCE_DIR%\.idea" "%SOURCE_DIR%\cmake-build-ludork-debug" __pycache__ UiPreviewHost UiPreviewCurveResolver /XF *.anim.json *.py *.pyc *.pyo *.log Main.ini Ludork.ini CMakeUserPresets.json generate_clion.sh UiPreviewHost* UiPreviewCurveResolver* /NFL /NDL /NJH /NJS /NP
if errorlevel 8 exit /b %errorlevel%

"%SCRIPT_TOOLS%" configure-project-template "%CPP_TEMPLATE_DIR%\Main.proj" true false
if errorlevel 1 exit /b %errorlevel%
"%SCRIPT_TOOLS%" configure-project-template "%CPP_FFMPEG_TEMPLATE_DIR%\Main.proj" true true
if errorlevel 1 exit /b %errorlevel%

call :copy_release_legal_files "%CPP_TEMPLATE_DIR%"
if errorlevel 1 exit /b %errorlevel%
call :copy_release_legal_files "%CPP_FFMPEG_TEMPLATE_DIR%"
if errorlevel 1 exit /b %errorlevel%

call "%CD%\tools\build_standalone.bat" "%CPP_TEMPLATE_DIR%" "%STANDALONE_TEMPLATE_DIR%" "%CONFIG%"
if errorlevel 1 exit /b %errorlevel%
call "%CD%\tools\build_standalone.bat" "%CPP_FFMPEG_TEMPLATE_DIR%" "%STANDALONE_FFMPEG_TEMPLATE_DIR%" "%CONFIG%"
if errorlevel 1 exit /b %errorlevel%

call :copy_standalone_files "%CPP_TEMPLATE_DIR%" "%STANDALONE_TEMPLATE_DIR%"
if errorlevel 1 exit /b %errorlevel%
call :copy_standalone_files "%CPP_FFMPEG_TEMPLATE_DIR%" "%STANDALONE_FFMPEG_TEMPLATE_DIR%"
if errorlevel 1 exit /b %errorlevel%
"%SCRIPT_TOOLS%" configure-project-template "%STANDALONE_TEMPLATE_DIR%\Main.proj" false false
if errorlevel 1 exit /b %errorlevel%
"%SCRIPT_TOOLS%" configure-project-template "%STANDALONE_FFMPEG_TEMPLATE_DIR%\Main.proj" false true
if errorlevel 1 exit /b %errorlevel%

if exist "%CPP_TEMPLATE_DIR%\build" rmdir /S /Q "%CPP_TEMPLATE_DIR%\build"
if exist "%CPP_TEMPLATE_DIR%\bin" rmdir /S /Q "%CPP_TEMPLATE_DIR%\bin"
if exist "%CPP_FFMPEG_TEMPLATE_DIR%\build" rmdir /S /Q "%CPP_FFMPEG_TEMPLATE_DIR%\build"
if exist "%CPP_FFMPEG_TEMPLATE_DIR%\bin" rmdir /S /Q "%CPP_FFMPEG_TEMPLATE_DIR%\bin"

for %%T in (
    "%CPP_TEMPLATE_DIR%"
    "%STANDALONE_TEMPLATE_DIR%"
    "%CPP_FFMPEG_TEMPLATE_DIR%"
    "%STANDALONE_FFMPEG_TEMPLATE_DIR%"
) do (
    call :validate_no_ui_preview_host "%%~T"
    if errorlevel 1 exit /b 1
)

echo C++ source template is ready: %CPP_TEMPLATE_DIR%
echo Standalone template is ready: %STANDALONE_TEMPLATE_DIR%\Main.exe
echo C++ FFmpeg source template is ready: %CPP_FFMPEG_TEMPLATE_DIR%
echo Standalone FFmpeg template is ready: %STANDALONE_FFMPEG_TEMPLATE_DIR%\Main.exe
exit /b 0

:usage
echo Usage: tools\create_templates.bat [Debug^|Release] [output-folder]
exit /b 1

:copy_standalone_files
set "COPY_SOURCE=%~1"
set "COPY_TARGET=%~2"
if not exist "%COPY_TARGET%\.vscode" mkdir "%COPY_TARGET%\.vscode"
copy /Y "%COPY_SOURCE%\.vscode\settings.json" "%COPY_TARGET%\.vscode\settings.json" >nul
if errorlevel 1 exit /b %errorlevel%
copy /Y "%COPY_SOURCE%\.emmyrc.json" "%COPY_TARGET%\.emmyrc.json" >nul
if errorlevel 1 exit /b %errorlevel%
copy /Y "%COPY_SOURCE%\.gitignore" "%COPY_TARGET%\.gitignore" >nul
exit /b %errorlevel%

:copy_release_legal_files
set "LEGAL_TARGET=%~1"
robocopy "%CD%\Licenses" "%LEGAL_TARGET%\Licenses" /E /NFL /NDL /NJH /NJS /NP
if errorlevel 8 exit /b %errorlevel%
copy /Y "%CD%\LICENSE.md" "%LEGAL_TARGET%\LICENSE.md" >nul
if errorlevel 1 exit /b %errorlevel%
copy /Y "%CD%\THIRD_PARTY_NOTICES.md" "%LEGAL_TARGET%\THIRD_PARTY_NOTICES.md" >nul
if errorlevel 1 exit /b %errorlevel%
copy /Y "%CD%\THIRD_PARTY_NOTICES_zh_CN.md" "%LEGAL_TARGET%\THIRD_PARTY_NOTICES_zh_CN.md" >nul
exit /b %errorlevel%

:validate_no_ui_preview_host
for /r "%~1" %%F in (UiPreviewHost* UiPreviewCurveResolver*) do if exist "%%~fF" (
    echo UI preview host entry was found in a project template: %%~fF
    exit /b 1
)
for /d /r "%~1" %%D in (UiPreviewHost* UiPreviewCurveResolver*) do if exist "%%~fD" (
    echo UI preview host entry was found in a project template: %%~fD
    exit /b 1
)
exit /b 0
