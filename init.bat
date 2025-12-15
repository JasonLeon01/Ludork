@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0"
set ENV_DIR=LudorkEnv
set "PY_CMD=py -3.10"

call %PY_CMD% -V >nul 2>&1
if errorlevel 1 (
  set "PY_CMD=python3.10"
  call %PY_CMD% -V >nul 2>&1
  if errorlevel 1 (
    set "PY_CMD=%LocalAppData%\Programs\Python\Python310\python.exe"
    if not exist "%PY_CMD%" (
      echo Python 3.10 not found. Please install Python 3.10 and try again.
      exit /b 1
    )
  )
)

if exist "%ENV_DIR%\Scripts\activate.bat" (
  call "%ENV_DIR%\Scripts\activate.bat"
  for /f "tokens=2 delims= " %%v in ('python --version 2^>^&1') do set PV=%%v
  if /I not "!PV:~0,4!"=="3.10" (
    call deactivate
    rmdir /S /Q "%ENV_DIR%"
    goto CREATE_ENV
  )
  python main.py
  exit /b %errorlevel%
)

:CREATE_ENV
call %PY_CMD% -m venv "%ENV_DIR%"
if errorlevel 1 (
  rmdir /S /Q "%ENV_DIR%"
  exit /b 1
)

call "%ENV_DIR%\Scripts\activate.bat"
python -m pip install -r requirements.txt
if errorlevel 1 (
  rmdir /S /Q "%ENV_DIR%"
  exit /b 1
)

python main.py
exit /b %errorlevel%
