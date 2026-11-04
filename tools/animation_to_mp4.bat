@echo off
setlocal EnableExtensions
chcp 65001>nul
set "PYTHONIOENCODING=utf-8"
cd /d "%~dp0.."
set "TOOLCHAIN_DIRECTORY=%CD%\.tools\Toolchain"
if not exist "%TOOLCHAIN_DIRECTORY%" (
    echo Toolchain was not found. Run tools\setup_python.bat and tools\build_toolchain_bundle.bat first.
    exit /b 1
)
"%TOOLCHAIN_DIRECTORY%\ludork-assets.exe" animation-mp4 %*
exit /b %errorlevel%
