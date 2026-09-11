@echo off
setlocal EnableExtensions
cd /d "%~dp0.."

set "VARIANT=all"
set "NATIVE_CACHE="
set "CONFIG="
set "OUTPUT_FOLDER="
:parse_args
if "%~1"=="" goto args_parsed
set "ARG=%~1"
set "OPTION_VALUE=%~2"
if /I "%~1"=="--variant" (
    if "%~2"=="" goto usage
    if "%OPTION_VALUE:~0,2%"=="--" goto usage
    set "VARIANT=%~2"
    shift
    shift
    goto parse_args
)
if /I "%~1"=="--native-cache" (
    if "%~2"=="" goto usage
    if "%OPTION_VALUE:~0,2%"=="--" goto usage
    for %%I in ("%~2") do set "NATIVE_CACHE=%%~fI"
    shift
    shift
    goto parse_args
)
if "%ARG:~0,2%"=="--" goto usage
if not defined CONFIG (
    set "CONFIG=%~1"
) else if not defined OUTPUT_FOLDER (
    set "OUTPUT_FOLDER=%~1"
) else (
    goto usage
)
shift
goto parse_args
:args_parsed
if not defined CONFIG set "CONFIG=Release"
if /I not "%CONFIG%"=="Debug" if /I not "%CONFIG%"=="Release" goto usage
if /I "%CONFIG%"=="Debug" set "CONFIG=Debug"
if /I "%CONFIG%"=="Release" set "CONFIG=Release"
if /I not "%VARIANT%"=="all" if /I not "%VARIANT%"=="plain" if /I not "%VARIANT%"=="ffmpeg" goto usage

set "SOURCE_DIR=%CD%\Game"
set "LICENSES_DIR=%CD%\Licenses"
if not defined OUTPUT_FOLDER (
    set "TEMPLATES_DIR=%CD%\Templates"
) else (
    for %%I in ("%OUTPUT_FOLDER%") do set "TEMPLATES_DIR=%%~fI"
)
set "SCRIPT_TOOLS=%CD%\.tools\ScriptTools\ScriptTools.exe"
set "EDITOR_CACHE_DIRECTORY="
for /f "delims=" %%V in ('""%SCRIPT_TOOLS%" packaging-constants list editor-cache-directory --separator space"') do set "EDITOR_CACHE_DIRECTORY=%%V"
if not defined EDITOR_CACHE_DIRECTORY exit /b 1
set "GENERATED_SCRIPTS="
for /f "delims=" %%V in ('""%SCRIPT_TOOLS%" packaging-constants list native-lua-files --separator space --windows"') do set "GENERATED_SCRIPTS=%%V"
if not defined GENERATED_SCRIPTS exit /b 1
set "RUNTIME_LEGAL_FILES="
for /f "delims=" %%V in ('""%SCRIPT_TOOLS%" packaging-constants list runtime-legal-files --separator space"') do set "RUNTIME_LEGAL_FILES=%%V"
if not defined RUNTIME_LEGAL_FILES exit /b 1
set "TEMPLATE_NAMES="
for /f "delims=" %%V in ('""%SCRIPT_TOOLS%" packaging-constants list template-names --separator space"') do set "TEMPLATE_NAMES=%%V"
if not defined TEMPLATE_NAMES exit /b 1
for /f "tokens=1-4" %%A in ("%TEMPLATE_NAMES%") do (
    set "CPP_TEMPLATE_DIR=%TEMPLATES_DIR%\%%A"
    set "CPP_FFMPEG_TEMPLATE_DIR=%TEMPLATES_DIR%\%%B"
    set "STANDALONE_TEMPLATE_DIR=%TEMPLATES_DIR%\%%C"
    set "STANDALONE_FFMPEG_TEMPLATE_DIR=%TEMPLATES_DIR%\%%D"
)
if defined NATIVE_CACHE (
    call :validate_native_cache_paths
    if errorlevel 1 exit /b 1
    for %%V in (plain ffmpeg) do if /I "%VARIANT%"=="all" (
        call :check_native_cache "%NATIVE_CACHE%\%%V\%CONFIG%"
        if errorlevel 1 exit /b 1
    )
    if /I not "%VARIANT%"=="all" (
        call :check_native_cache "%NATIVE_CACHE%\%VARIANT%\%CONFIG%"
        if errorlevel 1 exit /b 1
    )
)
set "FFMPEG_VERSION="
for /f "usebackq eol=# tokens=1,2 delims==" %%A in ("%CD%\versions.conf") do if /I "%%A"=="FFMPEG_VERSION" set "FFMPEG_VERSION=%%B"
if not defined FFMPEG_VERSION (
    echo FFMPEG_VERSION is not set in versions.conf.
    exit /b 1
)
set "FFMPEG_SOURCE_ARCHIVE=%SOURCE_DIR%\ThirdPartySource\ffmpeg-%FFMPEG_VERSION%.tar.gz"

if not exist "%SOURCE_DIR%\CMakeLists.txt" (
    echo Game C++ project was not found: %SOURCE_DIR%
    exit /b 1
)
if not exist "%SCRIPT_TOOLS%" (
    echo ScriptTools was not found. Run tools\init.bat first.
    exit /b 1
)
if not exist "%SOURCE_DIR%\Engine\ThirdParty\LuaSF" set "MISSING_DEPENDENCIES=1"
if not exist "%SOURCE_DIR%\Engine\ThirdParty\lua-cjson" set "MISSING_DEPENDENCIES=1"
if not exist "%SOURCE_DIR%\Engine\ThirdParty\zlib" set "MISSING_DEPENDENCIES=1"
if defined MISSING_DEPENDENCIES (
    echo Game dependencies were not found. Run tools\init.bat first.
    exit /b 1
)
if /I not "%VARIANT%"=="plain" (
    if not exist "%SOURCE_DIR%\Engine\ThirdParty\ffmpeg\configure" (
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
)

if /I "%VARIANT%"=="plain" (
    call :create_template_pair "%CPP_TEMPLATE_DIR%" "%STANDALONE_TEMPLATE_DIR%" 0
    if errorlevel 1 exit /b 1
    exit /b 0
)
if /I "%VARIANT%"=="ffmpeg" (
    call :create_template_pair "%CPP_FFMPEG_TEMPLATE_DIR%" "%STANDALONE_FFMPEG_TEMPLATE_DIR%" 1
    if errorlevel 1 exit /b 1
    exit /b 0
)
call :create_template_pair "%CPP_TEMPLATE_DIR%" "%STANDALONE_TEMPLATE_DIR%" 0
if errorlevel 1 exit /b 1
call :create_template_pair "%CPP_FFMPEG_TEMPLATE_DIR%" "%STANDALONE_FFMPEG_TEMPLATE_DIR%" 1
if errorlevel 1 exit /b 1
exit /b 0

:usage
echo Usage: tools\create_templates.bat [--variant all^|plain^|ffmpeg] [--native-cache ^<folder^>] [Debug^|Release] [output-folder]
exit /b 1

:create_template_pair
set "CPP_TARGET=%~1"
set "STANDALONE_TARGET=%~2"
set "INCLUDE_FFMPEG=%~3"
set "FFMPEG_ENABLED=false"
if "%INCLUDE_FFMPEG%"=="1" set "FFMPEG_ENABLED=true"
for %%T in ("%CPP_TARGET%" "%STANDALONE_TARGET%") do (
    if exist "%%~T" rmdir /S /Q "%%~T"
    mkdir "%%~T"
    if errorlevel 1 exit /b 1
)
call :copy_cpp_template "%CPP_TARGET%" "%INCLUDE_FFMPEG%"
if errorlevel 1 exit /b %errorlevel%
"%SCRIPT_TOOLS%" configure-project-template "%CPP_TARGET%\Main.proj" true %FFMPEG_ENABLED%
if errorlevel 1 exit /b %errorlevel%
call :copy_runtime_legal_files "%CPP_TARGET%" "%INCLUDE_FFMPEG%"
if errorlevel 1 exit /b %errorlevel%
set "CURRENT_BUILD_OPTION="
set "CACHE_ENTRY="
if defined NATIVE_CACHE (
    call :restore_native_cache
    if errorlevel 1 exit /b 1
)
call "%CD%\tools\build_standalone.bat" %CURRENT_BUILD_OPTION% "%CPP_TARGET%" "%STANDALONE_TARGET%" "%CONFIG%"
if errorlevel 1 exit /b %errorlevel%
call :copy_standalone_files "%CPP_TARGET%" "%STANDALONE_TARGET%"
if errorlevel 1 exit /b %errorlevel%
"%SCRIPT_TOOLS%" configure-project-template "%STANDALONE_TARGET%\Main.proj" false %FFMPEG_ENABLED%
if errorlevel 1 exit /b %errorlevel%
if defined CACHE_ENTRY if not defined CURRENT_BUILD_OPTION (
    call :copy_native_outputs "%CPP_TARGET%" "%CACHE_ENTRY%"
    if errorlevel 1 exit /b 1
    call :validate_native_cache "%CACHE_ENTRY%"
    if errorlevel 1 exit /b 1
    echo Saved native cache: %CACHE_ENTRY%
)
if exist "%CPP_TARGET%\build" rmdir /S /Q "%CPP_TARGET%\build"
if exist "%CPP_TARGET%\bin" rmdir /S /Q "%CPP_TARGET%\bin"
if exist "%CPP_TARGET%\Intermediate" rmdir /S /Q "%CPP_TARGET%\Intermediate"
if exist "%CPP_TARGET%\%EDITOR_CACHE_DIRECTORY%" rmdir /S /Q "%CPP_TARGET%\%EDITOR_CACHE_DIRECTORY%"
if exist "%CPP_TARGET%\%EDITOR_CACHE_DIRECTORY%" exit /b 1
if exist "%CPP_TARGET%\Cache" rmdir /S /Q "%CPP_TARGET%\Cache"
if exist "%CPP_TARGET%\Cache" exit /b 1
if exist "%CPP_TARGET%\Binaries" rmdir /S /Q "%CPP_TARGET%\Binaries"
if exist "%CPP_TARGET%\Binaries" exit /b 1
"%SCRIPT_TOOLS%" ui-preview validate "%STANDALONE_TARGET%"
if errorlevel 1 exit /b 1
if "%INCLUDE_FFMPEG%"=="1" (
    echo C++ FFmpeg source template is ready: %CPP_TARGET%
    echo Standalone FFmpeg template is ready: %STANDALONE_TARGET%\Main.exe
) else (
    echo C++ source template is ready: %CPP_TARGET%
    echo Standalone template is ready: %STANDALONE_TARGET%\Main.exe
)
exit /b 0

:validate_native_cache_paths
powershell -NoProfile -Command ^
    "$ErrorActionPreference = 'Stop';" ^
    "function Check-Path($path) { $path = [IO.Path]::GetFullPath($path); $current = $path; while ($current) { if (Test-Path -LiteralPath $current) { $item = Get-Item -Force -LiteralPath $current; if (-not $item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw ('Unsafe cache/template path: ' + $current) } }; $current = [IO.Path]::GetDirectoryName($current.TrimEnd('\')) }; return $path.TrimEnd('\') + '\' };" ^
    "$cache = Check-Path $env:NATIVE_CACHE;" ^
    "foreach ($variant in @('plain', 'ffmpeg')) { [void](Check-Path ($cache + $variant + '\' + $env:CONFIG)) };" ^
    "foreach ($path in @($env:TEMPLATES_DIR, $env:SOURCE_DIR)) { $protected = Check-Path $path; if ($cache.StartsWith($protected, [StringComparison]::OrdinalIgnoreCase) -or $protected.StartsWith($cache, [StringComparison]::OrdinalIgnoreCase)) { throw ('Native cache overlaps templates or Game: ' + $cache) } }"
exit /b %errorlevel%

:check_native_cache
if not exist "%~1" exit /b 0
call :validate_native_cache "%~1"
exit /b %errorlevel%

:restore_native_cache
set "CACHE_VARIANT=plain"
if "%INCLUDE_FFMPEG%"=="1" set "CACHE_VARIANT=ffmpeg"
set "CACHE_ENTRY=%NATIVE_CACHE%\%CACHE_VARIANT%\%CONFIG%"
if not exist "%CACHE_ENTRY%" exit /b 0
call :copy_native_outputs "%CACHE_ENTRY%" "%CPP_TARGET%"
if errorlevel 1 exit /b 1
set "CURRENT_BUILD_OPTION=--use-current-build"
echo Reusing native cache: %CACHE_ENTRY%
exit /b 0

:copy_native_outputs
for %%D in (bin\%CONFIG% build\launcher\%CONFIG%) do (
    robocopy "%~1\%%D" "%~2\%%D" /E /NFL /NDL /NJH /NJS /NP
    if errorlevel 8 exit /b 1
)
if not exist "%~2\Scripts\stub" (
    mkdir "%~2\Scripts\stub"
    if errorlevel 1 exit /b 1
)
for %%F in (%GENERATED_SCRIPTS%) do (
    copy /Y "%~1\Scripts\%%F" "%~2\Scripts\%%F" >nul
    if errorlevel 1 exit /b 1
)
"%SCRIPT_TOOLS%" ui-preview copy --runtime-directory "bin/%CONFIG%" "%~1" "%~2"
exit /b %errorlevel%

:validate_native_cache
for %%F in (Main.exe Engine.dll GlobalCore.dll GlobalFunctions.dll LuaSF.dll lua.dll) do (
    call :require_native_file "%~1\bin\%CONFIG%\%%F"
    if errorlevel 1 exit /b 1
)
call :require_native_file "%~1\build\launcher\%CONFIG%\Main.exe"
if errorlevel 1 exit /b 1
for %%F in (%GENERATED_SCRIPTS%) do (
    call :require_native_file "%~1\Scripts\%%F"
    if errorlevel 1 exit /b 1
)
"%SCRIPT_TOOLS%" ui-preview validate "%~1"
exit /b %errorlevel%

:require_native_file
if not exist "%~1" (
    echo Required native cache file was not found: %~1
    exit /b 1
)
for %%F in ("%~1") do if "%%~zF"=="0" (
    echo Required native cache file is empty: %~1
    exit /b 1
)
exit /b 0

:copy_cpp_template
set COPY_TEMPLATE_EXCLUDED_DIRECTORIES="%SOURCE_DIR%\Binaries" "%SOURCE_DIR%\.venv" "%SOURCE_DIR%\build" "%SOURCE_DIR%\Intermediate" "%SOURCE_DIR%\%EDITOR_CACHE_DIRECTORY%" "%SOURCE_DIR%\Cache" "%SOURCE_DIR%\bin" "%SOURCE_DIR%\Log" "%SOURCE_DIR%\Save" "%SOURCE_DIR%\.vs" "%SOURCE_DIR%\.idea" "%SOURCE_DIR%\cmake-build-ludork-debug" "%SOURCE_DIR%\ThirdPartySource" __pycache__
set COPY_TEMPLATE_EXCLUDED_DIRECTORIES=%COPY_TEMPLATE_EXCLUDED_DIRECTORIES% "%SOURCE_DIR%\Scripts\Source\UI" "%SOURCE_DIR%\Scripts\stub\Source\UI" "%SOURCE_DIR%\Scripts\stub\Source\UIWindows" "%SOURCE_DIR%\Scripts\Source\Locale"
if "%~2"=="0" set COPY_TEMPLATE_EXCLUDED_DIRECTORIES=%COPY_TEMPLATE_EXCLUDED_DIRECTORIES% "%SOURCE_DIR%\Engine\ThirdParty\ffmpeg"
robocopy "%SOURCE_DIR%" "%~1" /E /XD %COPY_TEMPLATE_EXCLUDED_DIRECTORIES% /XF *.anim.json *.py *.pyc *.pyo *.log Main.ini Ludork.ini CMakeUserPresets.json generate_clion.sh /NFL /NDL /NJH /NJS /NP
if errorlevel 8 exit /b %errorlevel%
if exist "%SOURCE_DIR%\Scripts\Source\Locale\Core.lua" (
    if not exist "%~1\Scripts\Source\Locale" mkdir "%~1\Scripts\Source\Locale"
    if errorlevel 1 exit /b 1
    copy /Y "%SOURCE_DIR%\Scripts\Source\Locale\Core.lua" "%~1\Scripts\Source\Locale\Core.lua" >nul
    if errorlevel 1 exit /b 1
)
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
set "COPY_FFMPEG_LICENSES=%~2"
if exist "%LEGAL_TARGET%\Licenses" rmdir /S /Q "%LEGAL_TARGET%\Licenses"
mkdir "%LEGAL_TARGET%\Licenses"
if errorlevel 1 exit /b 1
for %%F in (README.md README_zh_CN.md) do (
    copy /Y "%LICENSES_DIR%\%%F" "%LEGAL_TARGET%\Licenses\%%F" >nul
    if errorlevel 1 exit /b 1
)
for %%D in (Lua LuaSF SFML sol2 lua-cjson zlib NativeDependencies) do (
    robocopy "%LICENSES_DIR%\%%D" "%LEGAL_TARGET%\Licenses\%%D" /E /NFL /NDL /NJH /NJS /NP
    if errorlevel 8 exit /b 1
)
if "%COPY_FFMPEG_LICENSES%"=="1" (
    robocopy "%LICENSES_DIR%\FFmpeg" "%LEGAL_TARGET%\Licenses\FFmpeg" /E /NFL /NDL /NJH /NJS /NP
    if errorlevel 8 exit /b 1
)
for %%F in (%RUNTIME_LEGAL_FILES%) do (
    copy /Y "%SOURCE_DIR%\%%F" "%LEGAL_TARGET%\%%F" >nul
    if errorlevel 1 exit /b 1
)
exit /b %errorlevel%
