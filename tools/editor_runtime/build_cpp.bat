@echo off
setlocal EnableExtensions DisableDelayedExpansion
chcp 65001>nul
for %%I in ("%~dp0.") do set "TOOLS_DIR=%%~fI"
if "%~1"=="" goto usage
if "%~2"=="" goto usage
if not "%~3"=="" goto usage
for %%I in ("%~1") do set "CPP_DIR=%%~fI"
set "CONFIG=%~2"
if /I "%CONFIG%"=="Debug" set "CONFIG=Debug"
if /I "%CONFIG%"=="Release" set "CONFIG=Release"
set "TOOLCHAIN_DIRECTORY=%TOOLS_DIR%\Toolchain"
if not exist "%TOOLCHAIN_DIRECTORY%" set "TOOLCHAIN_DIRECTORY=%TOOLS_DIR%\..\.tools\Toolchain"
if not exist "%TOOLCHAIN_DIRECTORY%" (
    echo Toolchain was not found. Prepare the editor tools first.
    exit /b 20
)
"%TOOLCHAIN_DIRECTORY%\ludork-build.exe" windows-native-build "%CPP_DIR%" "%CONFIG%" --tools-directory "%TOOLS_DIR%"
exit /b %errorlevel%

:usage
echo Usage: tools\build_cpp.bat ^<cpp-folder^> ^<Debug^|Release^>
exit /b 1
