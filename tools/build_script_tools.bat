@echo off
setlocal EnableExtensions
cd /d "%~dp0.."

set "PYTHON=%CD%\.venv\Scripts\python.exe"
if not exist "%PYTHON%" (
    echo Python environment was not found. Run tools\setup_python.bat first.
    exit /b 1
)
"%PYTHON%" "%CD%\tools\build_script_tools.py"
exit /b %errorlevel%
