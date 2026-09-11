@echo off
setlocal EnableExtensions
chcp 65001>nul
set "PYTHONIOENCODING=utf-8"
cd /d "%~dp0.."
set "SCRIPT_TOOLS=%CD%\.tools\ScriptTools\ScriptTools.exe"
if not exist "%SCRIPT_TOOLS%" (
    echo ScriptTools was not found. Run tools\setup_python.bat and tools\build_script_tools.bat first.
    exit /b 1
)
"%SCRIPT_TOOLS%" animation-mp4 %*
exit /b %errorlevel%
