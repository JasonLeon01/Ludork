@echo off
setlocal EnableExtensions EnableDelayedExpansion
chcp 65001>nul

for %%I in ("%~dp0..") do set "ROOT_DIR=%%~fI"
set "PROJECT_FILE=%ROOT_DIR%\Ludork.csproj"
set "DIST_DIR=%ROOT_DIR%\dist"
set "WIX_SOURCE=%ROOT_DIR%\tools\installer\Ludork.wxs"
set "PAYLOAD_GENERATOR=%ROOT_DIR%\tools\installer\generate_installer_payload.ps1"
set "PROJECT_ICON=%ROOT_DIR%\Assets\project-icon.ico"
set "WIX_DIR=%ROOT_DIR%\.tools\wix"
set "WIX_EXE=%WIX_DIR%\wix.exe"
set "WORK_DIR=%ROOT_DIR%\obj\editor-msi"
set "WORK_MSI=%WORK_DIR%\Ludork.msi"
set "PAYLOAD_SOURCE=%WORK_DIR%\EditorPayload.wxs"

if not exist "%PROJECT_FILE%" (
    echo Ludork.csproj was not found: %PROJECT_FILE%
    exit /b 1
)
if not exist "%WIX_SOURCE%" (
    echo WiX source was not found: %WIX_SOURCE%
    exit /b 1
)
if not exist "%PAYLOAD_GENERATOR%" (
    echo Installer payload generator was not found: %PAYLOAD_GENERATOR%
    exit /b 1
)
if not exist "%PROJECT_ICON%" (
    echo Project file icon was not found: %PROJECT_ICON%
    exit /b 1
)
if not exist "%DIST_DIR%\Ludork.exe" (
    echo Packaged editor was not found. Run tools\pack_editor.bat first.
    exit /b 1
)
call "%ROOT_DIR%\tools\validate_editor_windows_layout.bat" "%DIST_DIR%"
if errorlevel 1 exit /b 1
if /I not "%PROCESSOR_ARCHITECTURE%"=="AMD64" if /I not "%PROCESSOR_ARCHITEW6432%"=="AMD64" (
    echo Windows x64 is required to package the editor MSI.
    exit /b 1
)

where dotnet.exe >nul 2>nul
if errorlevel 1 (
    echo The .NET SDK was not found.
    exit /b 1
)

set "WIX_VERSION="
for /f "usebackq eol=# tokens=1,2 delims==" %%A in ("%ROOT_DIR%\versions.conf") do (
    if /I "%%A"=="WIX_VERSION" set "WIX_VERSION=%%B"
)
if not defined WIX_VERSION (
    echo WIX_VERSION is not set in versions.conf.
    exit /b 1
)

set "PRODUCT_VERSION="
for /f "usebackq delims=" %%V in (`dotnet msbuild "%PROJECT_FILE%" -nologo -getProperty:Version`) do (
    if not defined PRODUCT_VERSION set "PRODUCT_VERSION=%%V"
)
if not defined PRODUCT_VERSION (
    echo The Ludork product version could not be read.
    exit /b 1
)

if "%~1"=="" (
    set "OUTPUT_MSI=%ROOT_DIR%\Ludork-%PRODUCT_VERSION%-win-x64.msi"
) else (
    for %%I in ("%~1") do set "OUTPUT_MSI=%%~fI"
)
for %%I in ("%OUTPUT_MSI%") do (
    if /I not "%%~xI"==".msi" (
        echo The output path must use the .msi extension: %OUTPUT_MSI%
        exit /b 1
    )
    set "OUTPUT_DIR=%%~dpI"
)

if exist "%WIX_EXE%" (
    "%WIX_EXE%" --version | findstr /B /C:"%WIX_VERSION%" >nul
    if errorlevel 1 (
        echo Replacing the cached WiX Toolset with version %WIX_VERSION%...
        dotnet tool uninstall wix --tool-path "%WIX_DIR%"
        if errorlevel 1 exit /b 1
    )
)
if not exist "%WIX_EXE%" (
    echo Installing WiX Toolset %WIX_VERSION% into the repository tool cache...
    dotnet tool install wix --tool-path "%WIX_DIR%" --version "%WIX_VERSION%"
    if errorlevel 1 exit /b 1
)
"%WIX_EXE%" --version | findstr /B /C:"%WIX_VERSION%" >nul
if errorlevel 1 (
    echo The cached WiX version does not match WIX_VERSION=%WIX_VERSION%.
    exit /b 1
)

for /r "%DIST_DIR%" %%F in (*.pdb *.resources.dll) do (
    if exist "%%~fF" (
        echo Forbidden editor package file was found: %%~fF
        exit /b 1
    )
)
if exist "%DIST_DIR%\tools\pack_editor_msi.bat" (
    echo The MSI packaging tool must not be present in dist.
    exit /b 1
)
if exist "%DIST_DIR%\tools\installer\" (
    echo The MSI installer sources must not be present in dist.
    exit /b 1
)

if exist "%WORK_DIR%" rmdir /S /Q "%WORK_DIR%"
mkdir "%WORK_DIR%"
if errorlevel 1 goto failed

echo Generating the editor MSI payload manifest...
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%PAYLOAD_GENERATOR%" -DistDir "%DIST_DIR%" -OutputPath "%PAYLOAD_SOURCE%" -ProductVersion "%PRODUCT_VERSION%"
if errorlevel 1 goto failed

echo Building Ludork %PRODUCT_VERSION% Windows x64 MSI...
"%WIX_EXE%" build "%WIX_SOURCE%" "%PAYLOAD_SOURCE%" -arch x64 -define ProductVersion="%PRODUCT_VERSION%" -bindpath dist="%DIST_DIR%" -bindpath assets="%ROOT_DIR%\Assets" -intermediateFolder "%WORK_DIR%\intermediate" -pdbtype none -out "%WORK_MSI%"
if errorlevel 1 goto failed

echo Validating MSI database...
"%WIX_EXE%" msi validate "%WORK_MSI%" -sice ICE60 -sice ICE91
if errorlevel 1 goto failed

if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"
if errorlevel 1 goto failed
copy /Y "%WORK_MSI%" "%OUTPUT_MSI%" >nul
if errorlevel 1 goto failed

if exist "%WORK_DIR%" rmdir /S /Q "%WORK_DIR%"
echo Editor MSI complete: %OUTPUT_MSI%
exit /b 0

:failed
set "PACK_RESULT=%ERRORLEVEL%"
if "%PACK_RESULT%"=="0" set "PACK_RESULT=1"
if exist "%WORK_DIR%" rmdir /S /Q "%WORK_DIR%"
echo Editor MSI packaging failed.
exit /b %PACK_RESULT%
