@echo off
setlocal EnableExtensions EnableDelayedExpansion
chcp 65001>nul

for %%I in ("%~dp0..") do set "ROOT_DIR=%%~fI"
set "PREBUILT_TEMPLATES_DIR="
set "USE_CURRENT_UI_PREVIEW_HOST=0"

:parse_arguments
if "%~1"=="" goto arguments_ready
if /I "%~1"=="--templates" (
    if "%~2"=="" goto usage
    for %%I in ("%~2") do set "PREBUILT_TEMPLATES_DIR=%%~fI"
    shift
    shift
    goto parse_arguments
)
if /I "%~1"=="--use-current-ui-preview-host" (
    set "USE_CURRENT_UI_PREVIEW_HOST=1"
    shift
    goto parse_arguments
)
goto usage

:arguments_ready
set "PROJECT_FILE=%ROOT_DIR%\Ludork.csproj"
set "WORK_DIR=%ROOT_DIR%\obj\editor-package"
set "STAGE_DIR=%WORK_DIR%\dist"
set "FINAL_DIR=%ROOT_DIR%\dist"
set "BACKUP_DIR=%WORK_DIR%\previous-dist"
set "DIST_BACKED_UP=0"

if not exist "%PROJECT_FILE%" (
    echo Ludork.csproj was not found: %PROJECT_FILE%
    exit /b 1
)
if /I not "%PROCESSOR_ARCHITECTURE%"=="AMD64" if /I not "%PROCESSOR_ARCHITEW6432%"=="AMD64" (
    echo Windows x64 is required to package the editor.
    exit /b 1
)

where dotnet.exe >nul 2>nul
if errorlevel 1 (
    echo The .NET SDK was not found.
    exit /b 1
)
where cmake.exe >nul 2>nul
if errorlevel 1 (
    echo CMake was not found.
    exit /b 1
)

set "GNU_MAKE_VERSION="
set "FFMPEG_VERSION="
for /f "usebackq eol=# tokens=1,2 delims==" %%A in ("%ROOT_DIR%\versions.conf") do (
    if /I "%%A"=="GNU_MAKE_VERSION" set "GNU_MAKE_VERSION=%%B"
    if /I "%%A"=="FFMPEG_VERSION" set "FFMPEG_VERSION=%%B"
)
if not defined GNU_MAKE_VERSION (
    echo GNU_MAKE_VERSION is not set in versions.conf.
    exit /b 1
)
if not defined FFMPEG_VERSION (
    echo FFMPEG_VERSION is not set in versions.conf.
    exit /b 1
)

set "GNU_MAKE_EXE=%ROOT_DIR%\.tools\gnu-make\gnumake.exe"
set "GNU_MAKE_SOURCE=%ROOT_DIR%\.tools\sources\make-%GNU_MAKE_VERSION%.tar.gz"
set "GNU_MAKE_LICENSE=%ROOT_DIR%\.tools\build\make-%GNU_MAKE_VERSION%\COPYING"
set "SCRIPT_TOOLS=%ROOT_DIR%\.tools\ScriptTools\ScriptTools.exe"
set "SCRIPT_TOOLS_VERSION_REPORT=%ROOT_DIR%\.tools\ScriptTools\runtime-versions.txt"
set "FFMPEG_SOURCE_ARCHIVE=%ROOT_DIR%\Sample\ThirdPartySource\ffmpeg-%FFMPEG_VERSION%.tar.gz"

for %%F in (
    "%ROOT_DIR%\tools\create_templates.bat"
    "%ROOT_DIR%\tools\build_ui_preview_host.bat"
    "%ROOT_DIR%\tools\editor_runtime\build_cpp.bat"
    "%ROOT_DIR%\tools\build_standalone.bat"
    "%ROOT_DIR%\tools\pack_project.bat"
    "%SCRIPT_TOOLS%"
    "%SCRIPT_TOOLS_VERSION_REPORT%"
    "%ROOT_DIR%\Sample\CMakeLists.txt"
) do (
    call :require_file "%%~F"
    if errorlevel 1 exit /b 1
)
for %%D in (
    "%ROOT_DIR%\Sample\LuaSF"
    "%ROOT_DIR%\Sample\lua-cjson"
    "%ROOT_DIR%\Sample\zlib"
) do (
    call :require_directory "%%~D"
    if errorlevel 1 exit /b 1
)
for %%F in (
    "%ROOT_DIR%\Sample\ffmpeg\configure"
    "%FFMPEG_SOURCE_ARCHIVE%"
    "%GNU_MAKE_EXE%"
    "%GNU_MAKE_SOURCE%"
    "%ROOT_DIR%\Locale\locale.json"
    "%ROOT_DIR%\LICENSE.md"
    "%ROOT_DIR%\README.md"
    "%ROOT_DIR%\README_zh_CN.md"
    "%ROOT_DIR%\THIRD_PARTY_NOTICES.md"
    "%ROOT_DIR%\THIRD_PARTY_NOTICES_zh_CN.md"
    "%ROOT_DIR%\About_en_GB.md"
    "%ROOT_DIR%\About_zh_CN.md"
) do (
    call :require_file "%%~F"
    if errorlevel 1 exit /b 1
)
for %%D in (
    "%ROOT_DIR%\docs\_images"
    "%ROOT_DIR%\docs\en_GB"
    "%ROOT_DIR%\docs\zh_CN"
    "%ROOT_DIR%\Licenses"
) do (
    call :require_directory "%%~D"
    if errorlevel 1 exit /b 1
)

if "%USE_CURRENT_UI_PREVIEW_HOST%"=="1" (
    echo Using current native UI preview host...
) else (
    echo Building native UI preview host...
    call "%ROOT_DIR%\tools\build_ui_preview_host.bat" Release
    if errorlevel 1 exit /b 1
)
call :resolve_vc_runtime
if errorlevel 1 exit /b 1

if exist "%WORK_DIR%" rmdir /S /Q "%WORK_DIR%"
mkdir "%STAGE_DIR%"
if errorlevel 1 goto failed

echo Publishing Windows x64 editor...
dotnet publish "%PROJECT_FILE%" -c Release -r win-x64 --self-contained true -o "%STAGE_DIR%" -p:PublishSingleFile=false -p:PublishTrimmed=false -p:PublishAot=false -p:DebugSymbols=false -p:DebugType=None
if errorlevel 1 goto failed

"%SCRIPT_TOOLS%" prune-editor-windows-publish "%STAGE_DIR%"
if errorlevel 1 goto failed

echo Compiling packaged locale data...
pushd "%STAGE_DIR%"
"%STAGE_DIR%\Ludork.exe" --compile-locale
set "PACK_RESULT=!ERRORLEVEL!"
popd
if not "!PACK_RESULT!"=="0" goto failed
if exist "%STAGE_DIR%\Locale\locale.json" del /Q "%STAGE_DIR%\Locale\locale.json"

if defined PREBUILT_TEMPLATES_DIR (
    echo Copying prepared editor project templates...
    call :copy_directory "%PREBUILT_TEMPLATES_DIR%" "%STAGE_DIR%\Templates"
    if errorlevel 1 goto failed
) else (
    echo Generating editor project templates...
    call "%ROOT_DIR%\tools\create_templates.bat" Release "%STAGE_DIR%\Templates"
    if errorlevel 1 goto failed
)

echo Copying editor resources...
if exist "%STAGE_DIR%\docs" rmdir /S /Q "%STAGE_DIR%\docs"
if exist "%STAGE_DIR%\Page" rmdir /S /Q "%STAGE_DIR%\Page"
call :copy_directory "%ROOT_DIR%\docs\_images" "%STAGE_DIR%\docs\_images"
if errorlevel 1 goto failed
call :copy_directory "%ROOT_DIR%\docs\en_GB" "%STAGE_DIR%\docs\en_GB"
if errorlevel 1 goto failed
call :copy_directory "%ROOT_DIR%\docs\zh_CN" "%STAGE_DIR%\docs\zh_CN"
if errorlevel 1 goto failed
call :copy_directory "%ROOT_DIR%\Licenses" "%STAGE_DIR%\Licenses"
if errorlevel 1 goto failed
for %%F in (
    LICENSE.md
    README.md
    README_zh_CN.md
    THIRD_PARTY_NOTICES.md
    THIRD_PARTY_NOTICES_zh_CN.md
) do (
    copy /Y "%ROOT_DIR%\%%F" "%STAGE_DIR%\%%F" >nul
    if errorlevel 1 goto failed
)
for %%F in ("%ROOT_DIR%\About_*.md") do (
    copy /Y "%%~fF" "%STAGE_DIR%\%%~nxF" >nul
    if errorlevel 1 goto failed
)

echo Preparing official editor plugins...
"%SCRIPT_TOOLS%" editor-official-plugins prepare "%ROOT_DIR%\Plugins" "%STAGE_DIR%"
if errorlevel 1 goto failed

mkdir "%STAGE_DIR%\tools" >nul 2>nul
copy /Y "%ROOT_DIR%\tools\editor_runtime\build_cpp.bat" "%STAGE_DIR%\tools\build_cpp.bat" >nul
if errorlevel 1 goto failed
for %%F in (build_standalone.bat pack_project.bat) do (
    copy /Y "%ROOT_DIR%\tools\%%F" "%STAGE_DIR%\tools\%%F" >nul
    if errorlevel 1 goto failed
)
call :require_file "%ROOT_DIR%\.tools\Lua\luac.exe"
if errorlevel 1 goto failed
copy /Y "%ROOT_DIR%\.tools\ScriptTools\ScriptTools.exe" "%STAGE_DIR%\tools\ScriptTools.exe" >nul
if errorlevel 1 goto failed
copy /Y "%SCRIPT_TOOLS_VERSION_REPORT%" "%STAGE_DIR%\tools\ScriptTools-runtime-versions.txt" >nul
if errorlevel 1 goto failed
copy /Y "%ROOT_DIR%\.tools\Lua\luac.exe" "%STAGE_DIR%\tools\luac.exe" >nul
if errorlevel 1 goto failed
call :copy_ui_preview_host "%ROOT_DIR%\.tools\UiPreviewHost\bin\Release" "%STAGE_DIR%\tools\UiPreviewHost"
if errorlevel 1 goto failed
call :copy_vc_runtime "%VC_RUNTIME_DIR%" "%STAGE_DIR%\tools\UiPreviewHost"
if errorlevel 1 goto failed

mkdir "%STAGE_DIR%\tools\gnu-make" >nul 2>nul
copy /Y "%GNU_MAKE_EXE%" "%STAGE_DIR%\tools\gnu-make\gnumake.exe" >nul
if errorlevel 1 goto failed
copy /Y "%GNU_MAKE_SOURCE%" "%STAGE_DIR%\tools\gnu-make\make-%GNU_MAKE_VERSION%.tar.gz" >nul
if errorlevel 1 goto failed
if exist "%GNU_MAKE_LICENSE%" (
    copy /Y "%GNU_MAKE_LICENSE%" "%STAGE_DIR%\tools\gnu-make\COPYING" >nul
) else (
    "%SystemRoot%\System32\tar.exe" -xOf "%GNU_MAKE_SOURCE%" "make-%GNU_MAKE_VERSION%/COPYING" > "%STAGE_DIR%\tools\gnu-make\COPYING"
)
if errorlevel 1 goto failed

call :purge_python_cache "%STAGE_DIR%"
if errorlevel 1 goto failed
call :purge_debug_symbols "%STAGE_DIR%"
if errorlevel 1 goto failed
call :purge_template_runtime_state "%STAGE_DIR%\Templates"
if errorlevel 1 goto failed
call :validate_package "%STAGE_DIR%"
if errorlevel 1 goto failed

if not exist "%FINAL_DIR%" goto promote_stage
move "%FINAL_DIR%" "%BACKUP_DIR%" >nul
if errorlevel 1 goto failed
set "DIST_BACKED_UP=1"

:promote_stage
move "%STAGE_DIR%" "%FINAL_DIR%" >nul
if errorlevel 1 goto restore_previous
set "DIST_BACKED_UP=0"
if exist "%BACKUP_DIR%" rmdir /S /Q "%BACKUP_DIR%"
if exist "%WORK_DIR%" rmdir /S /Q "%WORK_DIR%"

echo Editor package complete: %FINAL_DIR%
exit /b 0

:restore_previous
if "%DIST_BACKED_UP%"=="1" move "%BACKUP_DIR%" "%FINAL_DIR%" >nul
set "DIST_BACKED_UP=0"
goto failed

:require_file
if exist "%~1" exit /b 0
echo Required file was not found: %~1
exit /b 1

:require_directory
if exist "%~1\" exit /b 0
echo Required directory was not found: %~1
exit /b 1

:resolve_vc_runtime
set "VC_VS_PATH="
set "CMAKE_CACHE=%ROOT_DIR%\.tools\UiPreviewHost\build\CMakeCache.txt"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%CMAKE_CACHE%" (
    for /f "usebackq tokens=1,* delims==" %%A in ("%CMAKE_CACHE%") do (
        if /I "%%A"=="CMAKE_GENERATOR_INSTANCE:INTERNAL" set "VC_VS_PATH=%%B"
    )
)
if not defined VC_VS_PATH (
    call :require_file "%VSWHERE%"
    if errorlevel 1 exit /b 1
    for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Redist.14.Latest -property installationPath`) do set "VC_VS_PATH=%%I"
)
if not defined VC_VS_PATH (
    echo Visual Studio C++ redistributable tools were not found.
    exit /b 1
)
set "VC_REDIST_VERSION_FILE=%VC_VS_PATH%\VC\Auxiliary\Build\Microsoft.VCRedistVersion.default.txt"
call :require_file "%VC_REDIST_VERSION_FILE%"
if errorlevel 1 exit /b 1
set "VC_REDIST_VERSION="
set /p VC_REDIST_VERSION=<"%VC_REDIST_VERSION_FILE%"
if not defined VC_REDIST_VERSION (
    echo Visual C++ redistributable version file was empty: %VC_REDIST_VERSION_FILE%
    exit /b 1
)
set "VC_RUNTIME_DIR=%VC_VS_PATH%\VC\Redist\MSVC\%VC_REDIST_VERSION%\x64\Microsoft.VC143.CRT"
call :require_directory "%VC_RUNTIME_DIR%"
if errorlevel 1 exit /b 1
exit /b 0

:copy_directory
robocopy "%~1" "%~2" /E /NFL /NDL /NJH /NJS /NP
if errorlevel 8 exit /b 1
exit /b 0

:copy_ui_preview_host
set "PREVIEW_SOURCE=%~1"
set "PREVIEW_TARGET=%~2"
if not exist "%PREVIEW_TARGET%" mkdir "%PREVIEW_TARGET%"
for %%F in (
    UiPreviewHost.exe
    UiPreviewHostRuntime.dll
    LudorkStandard.dll
    LuaSF.dll
    lua.dll
    sfml-system-3.dll
    sfml-window-3.dll
    sfml-graphics-3.dll
    sfml-audio-3.dll
    sfml-network-3.dll
) do (
    call :require_file "%PREVIEW_SOURCE%\%%F"
    if errorlevel 1 exit /b 1
    copy /Y "%PREVIEW_SOURCE%\%%F" "%PREVIEW_TARGET%\%%F" >nul
    if errorlevel 1 exit /b 1
)
exit /b 0

:copy_vc_runtime
set "VC_RUNTIME_SOURCE=%~1"
set "VC_RUNTIME_TARGET=%~2"
for %%F in (
    MSVCP140.dll
    MSVCP140_ATOMIC_WAIT.dll
    VCRUNTIME140.dll
    VCRUNTIME140_1.dll
) do (
    call :require_file "%VC_RUNTIME_SOURCE%\%%F"
    if errorlevel 1 exit /b 1
    copy /Y "%VC_RUNTIME_SOURCE%\%%F" "%VC_RUNTIME_TARGET%\%%F" >nul
    if errorlevel 1 exit /b 1
)
exit /b 0

:purge_python_cache
for /d /r "%~1" %%D in (__pycache__) do (
    if exist "%%~fD" rmdir /S /Q "%%~fD"
)
del /S /Q "%~1\*.pyc" >nul 2>nul
del /S /Q "%~1\*.pyo" >nul 2>nul
exit /b 0

:purge_debug_symbols
del /S /Q "%~1\*.pdb" >nul 2>nul
exit /b 0

:purge_template_runtime_state
for %%T in (Cpp Cpp-ffmpeg) do (
    if exist "%~1\%%T\Log" rmdir /S /Q "%~1\%%T\Log"
    if exist "%~1\%%T\Save" rmdir /S /Q "%~1\%%T\Save"
    if exist "%~1\%%T\Main.ini" del /Q "%~1\%%T\Main.ini"
    if exist "%~1\%%T\Ludork.ini" del /Q "%~1\%%T\Ludork.ini"
    if exist "%~1\%%T\Ludork-startup-error.log" del /Q "%~1\%%T\Ludork-startup-error.log"
)
exit /b 0

:validate_package
set "PACKAGE_DIR=%~1"
call :require_file "%PACKAGE_DIR%\Ludork.exe"
if errorlevel 1 exit /b 1
call :require_file "%PACKAGE_DIR%\Locale\en_GB"
if errorlevel 1 exit /b 1
call :require_file "%PACKAGE_DIR%\Locale\zh_CN"
if errorlevel 1 exit /b 1
call :require_file "%PACKAGE_DIR%\Templates\Cpp\Main.proj"
if errorlevel 1 exit /b 1
call :require_file "%PACKAGE_DIR%\Templates\Cpp-ffmpeg\Main.proj"
if errorlevel 1 exit /b 1
call :validate_standalone_runtime_layout "%PACKAGE_DIR%\Templates\Standalone"
if errorlevel 1 exit /b 1
call :validate_standalone_runtime_layout "%PACKAGE_DIR%\Templates\Standalone-ffmpeg"
if errorlevel 1 exit /b 1
for %%T in (Cpp Cpp-ffmpeg Standalone Standalone-ffmpeg) do (
    "%SCRIPT_TOOLS%" validate-ldpak-source "%PACKAGE_DIR%\Templates\%%T"
    if errorlevel 1 exit /b 1
)
call :require_file "%PACKAGE_DIR%\tools\build_cpp.bat"
if errorlevel 1 exit /b 1
call :require_file "%PACKAGE_DIR%\tools\build_standalone.bat"
if errorlevel 1 exit /b 1
call :require_file "%PACKAGE_DIR%\tools\pack_project.bat"
if errorlevel 1 exit /b 1
call :require_file "%PACKAGE_DIR%\tools\ScriptTools.exe"
if errorlevel 1 exit /b 1
call :require_file "%PACKAGE_DIR%\tools\ScriptTools-runtime-versions.txt"
if errorlevel 1 exit /b 1
call :require_file "%PACKAGE_DIR%\tools\luac.exe"
if errorlevel 1 exit /b 1
call :require_file "%PACKAGE_DIR%\tools\UiPreviewHost\UiPreviewHost.exe"
if errorlevel 1 exit /b 1
call :require_file "%PACKAGE_DIR%\tools\UiPreviewHost\UiPreviewHostRuntime.dll"
if errorlevel 1 exit /b 1
call :validate_ui_preview_host_ownership "%PACKAGE_DIR%"
if errorlevel 1 exit /b 1
for %%F in (
    MSVCP140.dll
    MSVCP140_ATOMIC_WAIT.dll
    VCRUNTIME140.dll
    VCRUNTIME140_1.dll
) do (
    call :require_file "%PACKAGE_DIR%\tools\UiPreviewHost\%%F"
    if errorlevel 1 exit /b 1
)
call :require_file "%PACKAGE_DIR%\tools\gnu-make\gnumake.exe"
if errorlevel 1 exit /b 1
call :require_file "%PACKAGE_DIR%\tools\gnu-make\make-%GNU_MAKE_VERSION%.tar.gz"
if errorlevel 1 exit /b 1
call :require_file "%PACKAGE_DIR%\tools\gnu-make\COPYING"
if errorlevel 1 exit /b 1
call :require_file "%PACKAGE_DIR%\LICENSE.md"
if errorlevel 1 exit /b 1
call :require_file "%PACKAGE_DIR%\README.md"
if errorlevel 1 exit /b 1
call :require_file "%PACKAGE_DIR%\README_zh_CN.md"
if errorlevel 1 exit /b 1
call :require_file "%PACKAGE_DIR%\THIRD_PARTY_NOTICES.md"
if errorlevel 1 exit /b 1
call :require_file "%PACKAGE_DIR%\THIRD_PARTY_NOTICES_zh_CN.md"
if errorlevel 1 exit /b 1
call :require_file "%PACKAGE_DIR%\About_en_GB.md"
if errorlevel 1 exit /b 1
call :require_file "%PACKAGE_DIR%\About_zh_CN.md"
if errorlevel 1 exit /b 1
for %%F in ("%ROOT_DIR%\About_*.md") do (
    call :require_file "%PACKAGE_DIR%\%%~nxF"
    if errorlevel 1 exit /b 1
)
call :require_directory "%PACKAGE_DIR%\docs\_images"
if errorlevel 1 exit /b 1
call :require_directory "%PACKAGE_DIR%\docs\en_GB"
if errorlevel 1 exit /b 1
call :require_directory "%PACKAGE_DIR%\docs\zh_CN"
if errorlevel 1 exit /b 1
call :require_directory "%PACKAGE_DIR%\Licenses"
if errorlevel 1 exit /b 1
"%SCRIPT_TOOLS%" editor-official-plugins validate "%PACKAGE_DIR%"
if errorlevel 1 exit /b 1
for %%F in (
    "DotNet\LICENSE.txt"
    "DotNet\THIRD-PARTY-NOTICES.txt"
    "DotNetPackages\Microsoft.Extensions-8.0.0-LICENSE.txt"
    "DotNetPackages\Microsoft.Extensions-8.0.0-THIRD-PARTY-NOTICES.txt"
    "DotNetPackages\Microsoft.Win32.Registry-and-System.Security-4.7.0-LICENSE.txt"
    "DotNetPackages\Microsoft.Win32.Registry-and-System.Security-4.7.0-THIRD-PARTY-NOTICES.txt"
    "DotNetPackages\System.Collections.Immutable-and-System.Reflection.Metadata-10.0.1-LICENSE.txt"
    "DotNetPackages\System.Collections.Immutable-and-System.Reflection.Metadata-10.0.1-THIRD-PARTY-NOTICES.txt"
    "DotNetPackages\System.IO.Pipelines-8.0.0-LICENSE.txt"
    "DotNetPackages\System.IO.Pipelines-8.0.0-THIRD-PARTY-NOTICES.txt"
    "DotNetPackages\System.Memory-4.5.3-LICENSE.txt"
    "DotNetPackages\System.Memory-4.5.3-THIRD-PARTY-NOTICES.txt"
    "DotNetPackages\System.ValueTuple-4.5.0-LICENSE.txt"
    "DotNetPackages\System.ValueTuple-4.5.0-THIRD-PARTY-NOTICES.txt"
    "README.md"
    "README_zh_CN.md"
    "Avalonia\ANGLE-LICENSE.txt"
    "Avalonia\LICENSE.txt"
    "Avalonia\Inter-OFL-1.1.txt"
    "EditorPackages\AvaloniaEdit-LICENSE.txt"
    "EditorPackages\CommunityToolkit.Mvvm-LICENSE.md"
    "EditorPackages\Material.Avalonia-LICENSE.txt"
    "EditorPackages\CommunityToolkit.Mvvm-THIRD-PARTY-NOTICES.txt"
    "EditorPackages\HarfBuzzSharp-LICENSE.txt"
    "EditorPackages\MoonSharp-LICENSE.txt"
    "EditorPackages\NAudio-LICENSE.txt"
    "EditorPackages\NAudio.Vorbis-LICENSE.txt"
    "EditorPackages\NVorbis-LICENSE.txt"
    "EditorPackages\NodifyM.Avalonia-LICENSE.txt"
    "EditorPackages\Roslyn-LICENSE.txt"
    "EditorPackages\Roslyn-THIRD-PARTY-NOTICES.rtf"
    "EditorPackages\SkiaSharp-LICENSE.txt"
    "EditorPackages\System.Reactive-LICENSE.txt"
    "EditorPackages\MicroCom.Runtime-LICENSE.txt"
    "EditorPackages\Microsoft.IO.RecyclableMemoryStream-LICENSE.txt"
    "EditorPackages\SkiaSharp-and-HarfBuzzSharp-NativeAssets-THIRD-PARTY-NOTICES.txt"
    "EditorPackages\Tmds.DBus.Protocol-LICENSE.txt"
    "LuaSF\LICENSE.txt"
    "Lua\LICENSE.txt"
    "SFML\LICENSE.txt"
    "sol2\LICENSE.txt"
    "lua-cjson\LICENSE.txt"
    "zlib\LICENSE.txt"
    "FFmpeg\COPYING.GPLv2.txt"
    "FFmpeg\COPYING.GPLv3.txt"
    "FFmpeg\COPYING.LGPLv2.1.txt"
    "FFmpeg\COPYING.LGPLv3.txt"
    "FFmpeg\README.md"
    "FFmpeg\UPSTREAM-LICENSE.md"
    "GNUMake\COPYING.txt"
    "MicrosoftVisualCppRuntime\README.md"
    "MicrosoftVisualCppRuntime\Visual-C-Runtime-2015-2022-License.docx"
    "NativeDependencies\FreeType-FTL.txt"
    "NativeDependencies\FreeType-LICENSE.txt"
    "NativeDependencies\Glad-CC0-1.0.txt"
    "NativeDependencies\HarfBuzz-COPYING.txt"
    "NativeDependencies\SheenBidi-LICENSE.txt"
    "NativeDependencies\Ogg-COPYING.txt"
    "NativeDependencies\Vorbis-COPYING.txt"
    "NativeDependencies\Wine-DInput-LGPLv2.1.txt"
    "NativeDependencies\FLAC-COPYING.Xiph.txt"
    "NativeDependencies\MbedTLS-LICENSE.txt"
    "NativeDependencies\libssh2-COPYING.txt"
    "NativeDependencies\SFML-THIRD-PARTY.md"
    "HarmonyOSSans\LICENSE.txt"
    "SampleMusic\NOTICE.md"
    "ScriptTools\Nuitka-4.1.3-AGPL-3.0.txt"
    "ScriptTools\Nuitka-4.1.3-NOTICE.txt"
    "ScriptTools\Nuitka-4.1.3-RUNTIME-EXCEPTION.txt"
    "ScriptTools\Python-3.12-LICENSES-AND-ACKNOWLEDGEMENTS.rst.txt"
    "ScriptTools\Zstandard-1.4.7-LICENSE.txt"
) do (
    call :require_file "%PACKAGE_DIR%\Licenses\%%~F"
    if errorlevel 1 exit /b 1
)

for %%F in (
    Avalonia.FreeDesktop.AtSpi.dll
    Avalonia.FreeDesktop.dll
    Avalonia.Native.dll
    Avalonia.X11.dll
    Tmds.DBus.Protocol.dll
) do (
    if exist "%PACKAGE_DIR%\%%F" (
        echo Foreign platform assembly was found: %PACKAGE_DIR%\%%F
        exit /b 1
    )
)
call :require_file "%PACKAGE_DIR%\Avalonia.Metal.dll"
if errorlevel 1 exit /b 1
call :require_file "%PACKAGE_DIR%\Ludork.deps.json"
if errorlevel 1 exit /b 1
findstr /I /C:"Avalonia.Metal" "%PACKAGE_DIR%\Ludork.deps.json" >nul
if errorlevel 1 (
    echo Required Avalonia.Metal dependency was not found in Ludork.deps.json.
    exit /b 1
)
for %%P in (
    Avalonia.FreeDesktop
    Avalonia.FreeDesktop.AtSpi
    Avalonia.Native
    Avalonia.X11
    Tmds.DBus.Protocol
) do (
    findstr /I /C:"%%P" "%PACKAGE_DIR%\Ludork.deps.json" >nul
    if not errorlevel 1 (
        echo Foreign platform dependency was found in Ludork.deps.json: %%P
        exit /b 1
    )
)
for /f "delims=" %%D in ('dir /B /A "%PACKAGE_DIR%\docs"') do (
    if /I not "%%D"=="_images" if /I not "%%D"=="en_GB" if /I not "%%D"=="zh_CN" (
        echo Non-public documentation was found in the editor package: %PACKAGE_DIR%\docs\%%D
        exit /b 1
    )
)

for %%P in (
    "%PACKAGE_DIR%\Sample"
    "%PACKAGE_DIR%\.ludork-development"
    "%PACKAGE_DIR%\requirements.txt"
    "%PACKAGE_DIR%\versions.conf"
    "%PACKAGE_DIR%\.venv"
    "%PACKAGE_DIR%\.tools"
    "%PACKAGE_DIR%\Page"
    "%PACKAGE_DIR%\Ludork.ini"
    "%PACKAGE_DIR%\Locale\locale.json"
    "%PACKAGE_DIR%\tools\pack_editor.bat"
    "%PACKAGE_DIR%\tools\pack_editor_msi.bat"
    "%PACKAGE_DIR%\tools\installer"
    "%PACKAGE_DIR%\tools\create_templates.bat"
    "%PACKAGE_DIR%\tools\run_editor.bat"
) do (
    if exist "%%~P" (
        echo Forbidden package entry was found: %%~P
        exit /b 1
    )
)

for /d /r "%PACKAGE_DIR%" %%D in (__pycache__) do (
    if exist "%%~fD" (
        echo Python cache directory was found: %%~fD
        exit /b 1
    )
)
for /r "%PACKAGE_DIR%" %%F in (*.py *.pyc *.pyo) do (
    if exist "%%~fF" (
        echo Python cache file was found: %%~fF
        exit /b 1
    )
)
for /r "%PACKAGE_DIR%" %%F in (*.resources.dll) do (
    if exist "%%~fF" (
        echo Dependency satellite resource was found: %%~fF
        exit /b 1
    )
)
for /r "%PACKAGE_DIR%" %%F in (*.pdb) do (
    if exist "%%~fF" (
        echo Debug symbol file was found: %%~fF
        exit /b 1
    )
)
for /r "%PACKAGE_DIR%\tools" %%F in (*.sh) do (
    if exist "%%~fF" (
        echo Non-Windows tool was found: %%~fF
        exit /b 1
    )
)
for %%T in (Cpp Cpp-ffmpeg Standalone Standalone-ffmpeg) do (
    call :require_file "%PACKAGE_DIR%\Templates\%%T\LICENSE.md"
    if errorlevel 1 exit /b 1
    call :require_file "%PACKAGE_DIR%\Templates\%%T\THIRD_PARTY_NOTICES.md"
    if errorlevel 1 exit /b 1
    call :require_file "%PACKAGE_DIR%\Templates\%%T\THIRD_PARTY_NOTICES_zh_CN.md"
    if errorlevel 1 exit /b 1
    for %%F in (
        "README.md"
        "README_zh_CN.md"
        "Lua\LICENSE.txt"
        "LuaSF\LICENSE.txt"
        "SFML\LICENSE.txt"
        "sol2\LICENSE.txt"
        "lua-cjson\LICENSE.txt"
        "zlib\LICENSE.txt"
        "NativeDependencies\FLAC-COPYING.Xiph.txt"
        "NativeDependencies\FreeType-FTL.txt"
        "NativeDependencies\FreeType-LICENSE.txt"
        "NativeDependencies\Glad-CC0-1.0.txt"
        "NativeDependencies\HarfBuzz-COPYING.txt"
        "NativeDependencies\libssh2-COPYING.txt"
        "NativeDependencies\MbedTLS-LICENSE.txt"
        "NativeDependencies\Ogg-COPYING.txt"
        "NativeDependencies\SFML-THIRD-PARTY.md"
        "NativeDependencies\SheenBidi-LICENSE.txt"
        "NativeDependencies\Vorbis-COPYING.txt"
        "NativeDependencies\Wine-DInput-LGPLv2.1.txt"
    ) do (
        call :require_file "%PACKAGE_DIR%\Templates\%%T\Licenses\%%~F"
        if errorlevel 1 exit /b 1
    )
    call :require_file "%PACKAGE_DIR%\Templates\%%T\Assets\Fonts\LICENSE.txt"
    if errorlevel 1 exit /b 1
    call :require_file "%PACKAGE_DIR%\Templates\%%T\Assets\Musics\LICENSE.md"
    if errorlevel 1 exit /b 1
    for %%L in (Avalonia DotNet DotNetPackages EditorPackages GNUMake HarmonyOSSans MicrosoftVisualCppRuntime SampleMusic ScriptTools) do if exist "%PACKAGE_DIR%\Templates\%%T\Licenses\%%L" (
        echo Non-runtime licence directory was found in a project template: %PACKAGE_DIR%\Templates\%%T\Licenses\%%L
        exit /b 1
    )
    call :validate_no_ui_preview_host "%PACKAGE_DIR%\Templates\%%T"
    if errorlevel 1 exit /b 1
)
for %%T in (Cpp Standalone) do if exist "%PACKAGE_DIR%\Templates\%%T\Licenses\FFmpeg" (
    echo FFmpeg licence material was found in a non-FFmpeg template: %PACKAGE_DIR%\Templates\%%T\Licenses\FFmpeg
    exit /b 1
)
for %%T in (Cpp-ffmpeg Standalone-ffmpeg) do for %%F in (
    COPYING.GPLv2.txt
    COPYING.GPLv3.txt
    COPYING.LGPLv2.1.txt
    COPYING.LGPLv3.txt
    README.md
    UPSTREAM-LICENSE.md
) do (
    call :require_file "%PACKAGE_DIR%\Templates\%%T\Licenses\FFmpeg\%%F"
    if errorlevel 1 exit /b 1
)
for %%T in (Cpp Cpp-ffmpeg) do (
    call :require_file "%PACKAGE_DIR%\Templates\%%T\generate_vs2022.bat"
    if errorlevel 1 exit /b 1
    call :require_file "%PACKAGE_DIR%\Templates\%%T\generate_clion.bat"
    if errorlevel 1 exit /b 1
    if exist "%PACKAGE_DIR%\Templates\%%T\generate_clion.sh" (
        echo Non-Windows IDE tool was found: %PACKAGE_DIR%\Templates\%%T\generate_clion.sh
        exit /b 1
    )
    if exist "%PACKAGE_DIR%\Templates\%%T\Log" (
        echo Template runtime log directory was found: %PACKAGE_DIR%\Templates\%%T\Log
        exit /b 1
    )
    if exist "%PACKAGE_DIR%\Templates\%%T\Save" (
        echo Template runtime save directory was found: %PACKAGE_DIR%\Templates\%%T\Save
        exit /b 1
    )
    if exist "%PACKAGE_DIR%\Templates\%%T\Main.ini" (
        echo Template runtime configuration was found: %PACKAGE_DIR%\Templates\%%T\Main.ini
        exit /b 1
    )
    if exist "%PACKAGE_DIR%\Templates\%%T\Ludork-startup-error.log" (
        echo Template startup log was found: %PACKAGE_DIR%\Templates\%%T\Ludork-startup-error.log
        exit /b 1
    )
    for %%D in (.vs .idea cmake-build-ludork-debug) do if exist "%PACKAGE_DIR%\Templates\%%T\%%D" (
        echo Generated IDE directory was found in a source template: %PACKAGE_DIR%\Templates\%%T\%%D
        exit /b 1
    )
    if exist "%PACKAGE_DIR%\Templates\%%T\CMakeUserPresets.json" (
        echo Generated CMake user presets were found in a source template: %PACKAGE_DIR%\Templates\%%T\CMakeUserPresets.json
        exit /b 1
    )
)
for %%T in (Standalone Standalone-ffmpeg) do for %%F in (
    generate_vs2022.bat
    generate_clion.bat
    generate_clion.sh
) do if exist "%PACKAGE_DIR%\Templates\%%T\%%F" (
    echo Source-only IDE tool was found in a Standalone template: %PACKAGE_DIR%\Templates\%%T\%%F
    exit /b 1
)
exit /b 0

:validate_standalone_runtime_layout
call :require_file "%~1\Main.exe"
if errorlevel 1 exit /b 1
call :require_file "%~1\Binaries\Main.exe"
if errorlevel 1 exit /b 1
set "STANDALONE_RUNTIME_LIBRARY_FOUND=0"
for %%E in (dll so dylib) do for /f "delims=" %%F in ('dir /B /A-D "%~1\*.%%E" 2^>nul') do (
    echo Standalone runtime library exists outside Binaries: %~1\%%F
    exit /b 1
)
for /f "delims=" %%F in ('dir /B /A-D "%~1\*.so.*" 2^>nul') do (
    echo Standalone runtime library exists outside Binaries: %~1\%%F
    exit /b 1
)
for /f "delims=" %%F in ('dir /B /A-D "%~1\Binaries\*.dll" 2^>nul') do set "STANDALONE_RUNTIME_LIBRARY_FOUND=1"
if "%STANDALONE_RUNTIME_LIBRARY_FOUND%"=="0" (
    echo Standalone template contains no runtime libraries in Binaries: %~1
    exit /b 1
)
exit /b 0

:validate_ui_preview_host_ownership
set "PREVIEW_PACKAGE_ROOT=%~f1"
set "PREVIEW_CANONICAL_DIRECTORY=%~f1\tools\UiPreviewHost"
set "PREVIEW_CANONICAL_EXECUTABLE=%~f1\tools\UiPreviewHost\UiPreviewHost.exe"
set "PREVIEW_CANONICAL_RUNTIME=%~f1\tools\UiPreviewHost\UiPreviewHostRuntime.dll"
for /r "%PREVIEW_PACKAGE_ROOT%" %%F in (UiPreviewHost*) do if exist "%%~fF" (
    if /I not "%%~fF"=="%PREVIEW_CANONICAL_EXECUTABLE%" (
        if /I not "%%~fF"=="%PREVIEW_CANONICAL_RUNTIME%" (
            echo UI preview host exists outside its canonical editor tool path: %%~fF
            exit /b 1
        )
    )
)
for /d /r "%PREVIEW_PACKAGE_ROOT%" %%D in (UiPreviewHost*) do if exist "%%~fD" (
    if /I not "%%~fD"=="%PREVIEW_CANONICAL_DIRECTORY%" (
        echo UI preview host exists outside its canonical editor tool path: %%~fD
        exit /b 1
    )
)
call :validate_absent_ui_preview_curve_resolver "%PREVIEW_PACKAGE_ROOT%"
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

:validate_absent_ui_preview_curve_resolver
for /r "%~1" %%F in (UiPreviewCurveResolver*) do if exist "%%~fF" (
    echo UI preview host source entry was found in the editor package: %%~fF
    exit /b 1
)
for /d /r "%~1" %%D in (UiPreviewCurveResolver*) do if exist "%%~fD" (
    echo UI preview host source entry was found in the editor package: %%~fD
    exit /b 1
)
exit /b 0

:usage
echo Usage: tools\pack_editor.bat [--templates ^<folder^>] [--use-current-ui-preview-host]
exit /b 1

:failed
set "PACK_RESULT=%ERRORLEVEL%"
if "%PACK_RESULT%"=="0" set "PACK_RESULT=1"
if exist "%WORK_DIR%" rmdir /S /Q "%WORK_DIR%"
echo Editor packaging failed.
exit /b %PACK_RESULT%
