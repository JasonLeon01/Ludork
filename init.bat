@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0"

if exist "Sample\Engine\pysf" (
  rmdir /S /Q "Sample\Engine\pysf"
)

echo Downloading PySF...
powershell -Command "Invoke-WebRequest -Uri 'https://github.com/JasonLeon01/PySF-AutoGenerator/releases/download/PySF3.0.1.2/pysf-3.0.1.2-Windows-x64.zip' -OutFile 'pysf.zip'"
if errorlevel 1 (
  echo Failed to download PySF.
  exit /b 1
)

echo Extracting PySF...
powershell -Command "Expand-Archive -Path 'pysf.zip' -DestinationPath 'Sample\Engine' -Force"
if errorlevel 1 (
  echo Failed to extract PySF.
  del pysf.zip
  exit /b 1
)

del pysf.zip

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
  goto RUN_APP
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

:RUN_APP
if exist "C_Extensions" (
  cd C_Extensions
  python setup.py
  if errorlevel 1 (
    cd ..
    exit /b 1
  )
  cd ..
)

python main.py
exit /b %errorlevel%
