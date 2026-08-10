@echo off
setlocal EnableExtensions
cd /d "%~dp0.."

set "VENV_DIR=%CD%\.venv"
set "VENV_PYTHON=%VENV_DIR%\Scripts\python.exe"
set "REQUIREMENTS_FILE=%CD%\requirements.txt"

if not exist "%REQUIREMENTS_FILE%" (
    echo requirements.txt was not found: %REQUIREMENTS_FILE%
    exit /b 1
)

if not exist "%VENV_PYTHON%" (
    echo Creating .venv with Python 3.12...
    py -3.12 -m venv "%VENV_DIR%"
    if errorlevel 1 exit /b %errorlevel%
) else (
    echo Using existing .venv.
)
echo Installing Python requirements into .venv...
"%VENV_PYTHON%" -m pip install -r "%REQUIREMENTS_FILE%"
if errorlevel 1 exit /b %errorlevel%

exit /b 0
