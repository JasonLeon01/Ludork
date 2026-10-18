@echo off
setlocal EnableExtensions DisableDelayedExpansion
for %%I in ("%~dp0.") do set "TOOLS_DIR=%%~fI"
set "SCRIPT_TOOLS=%TOOLS_DIR%\ScriptTools\ScriptTools.exe"
if not exist "%SCRIPT_TOOLS%" set "SCRIPT_TOOLS=%TOOLS_DIR%\..\.tools\ScriptTools\ScriptTools.exe"
if not exist "%SCRIPT_TOOLS%" (
    echo ScriptTools was not found. Run tools\init.bat first.
    exit /b 20
)
"%SCRIPT_TOOLS%" standalone-pack --tools-directory "%TOOLS_DIR%" %*
exit /b %errorlevel%
