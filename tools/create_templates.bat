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
set "FFMPEG_VERSION="
for /f "usebackq eol=# tokens=1,2 delims==" %%A in ("%CD%\versions.conf") do if /I "%%A"=="FFMPEG_VERSION" set "FFMPEG_VERSION=%%B"
if not defined FFMPEG_VERSION (
    echo FFMPEG_VERSION is not set in versions.conf.
    exit /b 1
)
set "FFMPEG_SOURCE_ARCHIVE=%SOURCE_DIR%\ThirdPartySource\ffmpeg-%FFMPEG_VERSION%.tar.gz"

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
if not exist "%FFMPEG_SOURCE_ARCHIVE%" (
    echo The distributable FFmpeg source archive was not found. Run tools\init.bat first.
    exit /b 1
)
if not exist "%CD%\.tools\gnu-make\gnumake.exe" (
    echo GNU Make was not found. Run tools\init.bat first.
    exit /b 1
)

for %%T in (
    "%CPP_TEMPLATE_DIR%"
    "%STANDALONE_TEMPLATE_DIR%"
    "%CPP_FFMPEG_TEMPLATE_DIR%"
    "%STANDALONE_FFMPEG_TEMPLATE_DIR%"
) do (
    if exist "%%~T" rmdir /S /Q "%%~T"
    mkdir "%%~T"
)

call :copy_cpp_template "%CPP_TEMPLATE_DIR%" 0
if errorlevel 1 exit /b %errorlevel%
call :copy_cpp_template "%CPP_FFMPEG_TEMPLATE_DIR%" 1
if errorlevel 1 exit /b %errorlevel%

"%SCRIPT_TOOLS%" configure-project-template "%CPP_TEMPLATE_DIR%\Main.proj" true false
if errorlevel 1 exit /b %errorlevel%
"%SCRIPT_TOOLS%" configure-project-template "%CPP_FFMPEG_TEMPLATE_DIR%\Main.proj" true true
if errorlevel 1 exit /b %errorlevel%

call :copy_runtime_legal_files "%CPP_TEMPLATE_DIR%"
if errorlevel 1 exit /b %errorlevel%
call :copy_runtime_legal_files "%CPP_FFMPEG_TEMPLATE_DIR%"
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

for %%T in ("%CPP_TEMPLATE_DIR%" "%CPP_FFMPEG_TEMPLATE_DIR%") do (
    if exist "%%~T\build" rmdir /S /Q "%%~T\build"
    if exist "%%~T\bin" rmdir /S /Q "%%~T\bin"
)

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

:copy_cpp_template
set COPY_TEMPLATE_EXCLUDED_DIRECTORIES="%SOURCE_DIR%\.venv" "%SOURCE_DIR%\build" "%SOURCE_DIR%\bin" "%SOURCE_DIR%\Log" "%SOURCE_DIR%\Save" "%SOURCE_DIR%\.vs" "%SOURCE_DIR%\.idea" "%SOURCE_DIR%\cmake-build-ludork-debug" "%SOURCE_DIR%\ThirdPartySource" __pycache__ UiPreviewHost UiPreviewCurveResolver
if "%~2"=="0" set COPY_TEMPLATE_EXCLUDED_DIRECTORIES=%COPY_TEMPLATE_EXCLUDED_DIRECTORIES% "%SOURCE_DIR%\ffmpeg"
robocopy "%SOURCE_DIR%" "%~1" /E /XD %COPY_TEMPLATE_EXCLUDED_DIRECTORIES% /XF *.anim.json *.py *.pyc *.pyo *.log Main.ini Ludork.ini CMakeUserPresets.json generate_clion.sh UiPreviewHost* UiPreviewCurveResolver* /NFL /NDL /NJH /NJS /NP
if errorlevel 8 exit /b %errorlevel%
if "%~2"=="1" (
    if not exist "%~1\ThirdPartySource" mkdir "%~1\ThirdPartySource"
    copy /Y "%FFMPEG_SOURCE_ARCHIVE%" "%~1\ThirdPartySource\ffmpeg-%FFMPEG_VERSION%.tar.gz" >nul
    if errorlevel 1 exit /b 1
)
exit /b 0

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

:copy_runtime_legal_files
set "LEGAL_TARGET=%~1"
robocopy "%SOURCE_DIR%\Licenses" "%LEGAL_TARGET%\Licenses" /E /NFL /NDL /NJH /NJS /NP
if errorlevel 8 exit /b %errorlevel%
copy /Y "%SOURCE_DIR%\LICENSE.md" "%LEGAL_TARGET%\LICENSE.md" >nul
if errorlevel 1 exit /b %errorlevel%
copy /Y "%SOURCE_DIR%\THIRD_PARTY_NOTICES.md" "%LEGAL_TARGET%\THIRD_PARTY_NOTICES.md" >nul
if errorlevel 1 exit /b %errorlevel%
copy /Y "%SOURCE_DIR%\THIRD_PARTY_NOTICES_zh_CN.md" "%LEGAL_TARGET%\THIRD_PARTY_NOTICES_zh_CN.md" >nul
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
