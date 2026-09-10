@echo off
setlocal EnableExtensions
cd /d "%~dp0"

if exist index.html del /f /q index.html
if exist assets rmdir /s /q assets

cd /d "%~dp0__default__"
call npm run build
if errorlevel 1 exit /b %errorlevel%

if not exist dist\ (
    echo docs\__default__\dist was not produced.
    exit /b 1
)

robocopy dist "%~dp0." /E /NFL /NDL /NJH /NJS /NP
if errorlevel 8 exit /b 1
echo Copied docs\__default__\dist into %~dp0
exit /b 0
