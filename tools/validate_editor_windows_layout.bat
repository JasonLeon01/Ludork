@echo off
setlocal EnableExtensions DisableDelayedExpansion

if "%~1"=="" exit /b 1
set "PACKAGE_DIR=%~f1"
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
    "%PACKAGE_DIR%\*.dll"
    "%PACKAGE_DIR%\*.so"
    "%PACKAGE_DIR%\*.dylib"
    "%PACKAGE_DIR%\*.deps.json"
    "%PACKAGE_DIR%\*.runtimeconfig*.json"
) do if exist "%%~fF" (
    echo Editor runtime file exists outside Binaries: %%~fF
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
    "%PACKAGE_DIR%\Binaries\.ludork-development"
) do if exist "%%~fF" (
    echo Non-binary editor content exists in Binaries: %%~fF
    exit /b 1
)
exit /b 0
