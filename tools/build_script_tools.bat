@echo off
setlocal EnableExtensions
cd /d "%~dp0.."

set "PYTHON=%CD%\.venv\Scripts\python.exe"
set "SOURCE_DIR=%CD%\ScriptTools"
set "OUTPUT_DIR=%CD%\.tools\ScriptTools"
set "OUTPUT=%OUTPUT_DIR%\ScriptTools.exe"
set "STAMP=%OUTPUT_DIR%\source.sha256"
set "VERSION_REPORT=%OUTPUT_DIR%\runtime-versions.txt"

if not exist "%PYTHON%" (
    echo Python environment was not found. Run tools\setup_python.bat first.
    exit /b 1
)
if not exist "%SOURCE_DIR%\__main__.py" (
    echo ScriptTools source was not found: %SOURCE_DIR%
    exit /b 1
)
"%PYTHON%" -c "import sys; raise SystemExit(0 if sys.version_info[:2] == (3, 12) else 1)"
if errorlevel 1 (
    echo ScriptTools requires a Python 3.12 virtual environment. Run tools\setup_python.bat again.
    exit /b 1
)

if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"
set "PYTHON_VERSION="
set "NUITKA_VERSION="
set "OPENSSL_VERSION="
set "PYTHON_ZSTANDARD_VERSION="
for /f "usebackq delims=" %%V in (`call "%PYTHON%" -c "import platform; print(platform.python_version())"`) do set "PYTHON_VERSION=%%V"
for /f "usebackq delims=" %%V in (`call "%PYTHON%" -c "import importlib.metadata; print(importlib.metadata.version('Nuitka'))"`) do set "NUITKA_VERSION=%%V"
for /f "usebackq delims=" %%V in (`call "%PYTHON%" -c "import ssl; print(ssl.OPENSSL_VERSION)"`) do set "OPENSSL_VERSION=%%V"
for /f "usebackq delims=" %%V in (`call "%PYTHON%" -c "import importlib.metadata; print(importlib.metadata.version('zstandard'))"`) do set "PYTHON_ZSTANDARD_VERSION=%%V"
if not defined PYTHON_VERSION (
    echo Failed to determine the CPython version.
    exit /b 1
)
if not defined NUITKA_VERSION (
    echo Failed to determine the Nuitka version.
    exit /b 1
)
if not defined OPENSSL_VERSION (
    echo Failed to determine the OpenSSL version.
    exit /b 1
)
if not defined PYTHON_ZSTANDARD_VERSION (
    echo Failed to determine the python-zstandard version.
    exit /b 1
)
set "NUITKA_ZSTD_VERSION=1.4.7"
set "SOURCE_HASH="
for /f "delims=" %%H in ('powershell -NoProfile -ExecutionPolicy Bypass -File "%CD%\tools\script_tools_hash.ps1" "%SOURCE_DIR%"') do set "SOURCE_HASH=%%H"
if not defined SOURCE_HASH (
    echo Failed to calculate the ScriptTools source hash.
    exit /b 1
)
set "SOURCE_HASH=%SOURCE_HASH%:Windows-x64:%PYTHON_VERSION%:%NUITKA_VERSION%:%OPENSSL_VERSION%:%PYTHON_ZSTANDARD_VERSION%:%NUITKA_ZSTD_VERSION%"
> "%VERSION_REPORT%" (
    echo Platform: Windows-x64
    echo CPython: %PYTHON_VERSION%
    echo OpenSSL: %OPENSSL_VERSION%
    echo Nuitka: %NUITKA_VERSION%
    echo Nuitka onefile Zstandard: %NUITKA_ZSTD_VERSION%
    echo python-zstandard build compressor: %PYTHON_ZSTANDARD_VERSION%
)
if errorlevel 1 exit /b %errorlevel%
set "PREVIOUS_HASH="
if exist "%STAMP%" set /p PREVIOUS_HASH=<"%STAMP%"
if exist "%OUTPUT%" if "%SOURCE_HASH%"=="%PREVIOUS_HASH%" (
    echo Using current ScriptTools: %OUTPUT%
    exit /b 0
)

echo Building standalone ScriptTools...
"%PYTHON%" -m nuitka ^
    --mode=onefile ^
    --assume-yes-for-downloads ^
    --include-package=ScriptTools ^
    --output-dir="%OUTPUT_DIR%" ^
    --output-filename=ScriptTools.exe ^
    "%SOURCE_DIR%\__main__.py"
if errorlevel 1 exit /b %errorlevel%
if not exist "%OUTPUT%" (
    echo Nuitka did not produce %OUTPUT%
    exit /b 1
)
powershell -NoProfile -Command "Set-Content -LiteralPath '%STAMP%' -Value '%SOURCE_HASH%' -NoNewline"
if errorlevel 1 exit /b %errorlevel%
echo ScriptTools ready: %OUTPUT%
exit /b 0
