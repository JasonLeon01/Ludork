@echo off
setlocal EnableExtensions DisableDelayedExpansion
if "%~1"=="" exit /b 1
set "CHANNEL="
if "%~2"=="--dev" set "CHANNEL=dev"
if "%~2"=="--release" set "CHANNEL=release"
if not defined CHANNEL (
    echo Specify --dev or --release for editor package layout validation.
    exit /b 1
)
for %%I in ("%~dp0..") do set "ROOT_DIR=%%~fI"
set "EDITOR_TOOL=%ROOT_DIR%\.tools\Toolchain\ludork-editor.exe"
if not exist "%EDITOR_TOOL%" exit /b 1
"%EDITOR_TOOL%" package-validate "%~f1" --platform windows --channel "%CHANNEL%" --phase layout
exit /b %errorlevel%
