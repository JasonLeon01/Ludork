@echo off
setlocal EnableExtensions DisableDelayedExpansion

if "%~1"=="" exit /b 1
if not "%~2"=="--dev" if not "%~2"=="--release" (
    echo Specify --dev or --release for editor package layout validation.
    exit /b 1
)
set "PACKAGE_DIR=%~f1"
if "%~2"=="--release" (
    for %%F in (Plugins plugins.json) do if exist "%PACKAGE_DIR%\%%F" (
        echo Unexpected plugin content at the release package root: %PACKAGE_DIR%\%%F
        exit /b 1
    )
) else (
    if exist "%PACKAGE_DIR%\tools\OfficialPlugins" (
        echo Unexpected installer plugin payload in a development package: %PACKAGE_DIR%\tools\OfficialPlugins
        exit /b 1
    )
)
for %%F in (
    Ludork.exe
    Binaries\Ludork.exe
    Binaries\Ludork.dll
    Binaries\Ludork.deps.json
    Binaries\Ludork.runtimeconfig.json
    Binaries\coreclr.dll
    Binaries\hostfxr.dll
    Binaries\hostpolicy.dll
    Binaries\System.Private.CoreLib.dll
) do (
    if not exist "%PACKAGE_DIR%\%%F" (
        echo Required editor package file was not found: %PACKAGE_DIR%\%%F
        exit /b 1
    )
)

for %%F in (
    "%PACKAGE_DIR%\Server"
    "%PACKAGE_DIR%\LudorkServer*.tar.gz"
    "%PACKAGE_DIR%\docs"
    "%PACKAGE_DIR%\About_*.md"
    "%PACKAGE_DIR%\THIRD_PARTY_NOTICES*.md"
    "%PACKAGE_DIR%\*.dll"
    "%PACKAGE_DIR%\*.so"
    "%PACKAGE_DIR%\*.dylib"
    "%PACKAGE_DIR%\*.deps.json"
    "%PACKAGE_DIR%\*.runtimeconfig*.json"
    "%PACKAGE_DIR%\Ludork.ini"
    "%PACKAGE_DIR%\Ludork.plist"
    "%PACKAGE_DIR%\*-signing.json"
    "%PACKAGE_DIR%\.localserver"
    "%PACKAGE_DIR%\Log"
    "%PACKAGE_DIR%\WebView2"
    "%PACKAGE_DIR%\tools\installer"
    "%PACKAGE_DIR%\tools\pack_editor_msi.bat"
    "%PACKAGE_DIR%\tools\pack_editor_exe.bat"
) do if exist "%%~fF" (
    echo Unexpected file at the editor package root: %%~fF
    exit /b 1
)
for %%F in ("%PACKAGE_DIR%\*.exe") do if exist "%%~fF" (
    if /I not "%%~nxF"=="Ludork.exe" (
        echo Unexpected executable at the editor package root: %%~fF
        exit /b 1
    )
)
for %%F in (
    "%PACKAGE_DIR%\Binaries\Locale"
    "%PACKAGE_DIR%\Binaries\Templates"
    "%PACKAGE_DIR%\Binaries\Server"
    "%PACKAGE_DIR%\Binaries\LudorkServer*.tar.gz"
    "%PACKAGE_DIR%\Binaries\tools"
    "%PACKAGE_DIR%\Binaries\Plugins"
    "%PACKAGE_DIR%\Binaries\plugins.json"
    "%PACKAGE_DIR%\Binaries\docs"
    "%PACKAGE_DIR%\Binaries\Licenses"
    "%PACKAGE_DIR%\Binaries\About_*.md"
    "%PACKAGE_DIR%\Binaries\LICENSE.md"
    "%PACKAGE_DIR%\Binaries\README*.md"
    "%PACKAGE_DIR%\Binaries\THIRD_PARTY_NOTICES*.md"
    "%PACKAGE_DIR%\Binaries\Ludork.ini"
    "%PACKAGE_DIR%\Binaries\Ludork.plist"
    "%PACKAGE_DIR%\Binaries\*-signing.json"
    "%PACKAGE_DIR%\Binaries\.localserver"
    "%PACKAGE_DIR%\Binaries\Log"
    "%PACKAGE_DIR%\Binaries\WebView2"
    "%PACKAGE_DIR%\Binaries\.ludork-development"
) do if exist "%%~fF" (
    echo Non-binary editor content exists in Binaries: %%~fF
    exit /b 1
)
exit /b 0
