@echo off
set "CMD=%~1"
if "%CMD%"=="step" goto :step
if "%CMD%"=="log" goto :log
if "%CMD%"=="warn" goto :warn
if "%CMD%"=="err" goto :err
if "%CMD%"=="pip_ensure" goto :pip_ensure
if "%CMD%"=="copytree" goto :copytree
echo [ERROR] Unknown editor packaging lib command: %CMD%
exit /b 1

:step
echo [STEP] %~2
exit /b 0

:log
echo [%date% %time%] %~2
exit /b 0

:warn
echo [WARNING] %~2
exit /b 0

:err
echo [ERROR] %~2
exit /b 1

:pip_ensure
set "PYTHON_EXE=%~2"
set "PACKAGE=%~3"
set "EXTRA_ARGS=%~4"
"%PYTHON_EXE%" -m pip --disable-pip-version-check show "%PACKAGE%" >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [STEP] Installing %PACKAGE%...
    "%PYTHON_EXE%" -m pip --disable-pip-version-check install %EXTRA_ARGS% "%PACKAGE%"
    exit /b %ERRORLEVEL%
)
exit /b 0

:copytree
set "SRC=%~2"
set "DST=%~3"
if not exist "%SRC%\" (
    echo [WARNING] Asset directory not found: %SRC%
    exit /b 0
)
if exist "%DST%\" rmdir /s /q "%DST%"
robocopy "%SRC%" "%DST%" /E /XD "__pycache__" /XF "*.pyc" "*.pyo" >nul
if %ERRORLEVEL% GEQ 8 exit /b %ERRORLEVEL%
exit /b 0
