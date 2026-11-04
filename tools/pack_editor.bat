@echo off
setlocal EnableExtensions DisableDelayedExpansion
chcp 65001>nul
for %%I in ("%~dp0..") do set "ROOT_DIR=%%~fI"
set "EDITOR_TOOL=%ROOT_DIR%\.tools\Toolchain\ludork-editor.exe"
if not exist "%EDITOR_TOOL%" (
    echo Toolchain was not found. Run tools\init.bat first.
    exit /b 1
)
"%EDITOR_TOOL%" package --repository-root "%ROOT_DIR%" %*
exit /b %errorlevel%
