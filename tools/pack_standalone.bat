@echo off
setlocal EnableExtensions DisableDelayedExpansion
for %%I in ("%~dp0.") do set "TOOLS_DIR=%%~fI"
set "TOOLCHAIN_DIRECTORY=%TOOLS_DIR%\Toolchain"
if not exist "%TOOLCHAIN_DIRECTORY%" set "TOOLCHAIN_DIRECTORY=%TOOLS_DIR%\..\.tools\Toolchain"
if not exist "%TOOLCHAIN_DIRECTORY%" (
    echo Toolchain was not found. Run tools\init.bat first.
    exit /b 20
)
"%TOOLCHAIN_DIRECTORY%\ludork-pack.exe" standalone-pack --tools-directory "%TOOLS_DIR%" %*
exit /b %errorlevel%
