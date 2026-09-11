@echo off
setlocal EnableExtensions
cd /d "%~dp0"

cd /d "%~dp0__default__"
call npm run build
if errorlevel 1 exit /b %errorlevel%

for %%F in (index.html docs\index.html about\index.html favicon.svg .nojekyll) do (
    if not exist "dist\%%F" (
        echo Missing website output: %%F
        exit /b 1
    )
)
if not exist dist\assets\ (
    echo Website assets were not produced.
    exit /b 1
)

if exist "%~dp0index.html" del /f /q "%~dp0index.html"
for %%D in (assets docs about) do (
    if exist "%~dp0%%D" rmdir /s /q "%~dp0%%D"
    if exist "%~dp0%%D" exit /b 1
)
robocopy dist "%~dp0." /E /NFL /NDL /NJH /NJS /NP
if errorlevel 8 exit /b 1
echo Copied docs\__default__\dist into %~dp0
exit /b 0
