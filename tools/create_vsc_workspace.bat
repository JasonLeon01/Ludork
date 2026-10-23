@echo off
setlocal EnableExtensions
set "PYTHONIOENCODING=utf-8"
set "SCRIPT=%~dp0create_vsc_workspace.py"

if defined PYTHON goto run_python

where python >nul 2>&1
if not errorlevel 1 (
    set "PYTHON=python"
    goto run_python
)

where py >nul 2>&1
if not errorlevel 1 (
    py -3 "%SCRIPT%"
    exit /b %errorlevel%
)

echo Python 3 was not found. Install it or set PYTHON to its executable. 1>&2
exit /b 1

:run_python
"%PYTHON%" "%SCRIPT%"
exit /b %errorlevel%
