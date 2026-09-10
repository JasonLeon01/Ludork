@echo off
setlocal EnableExtensions
cd /d "%~dp0.."

set "CPP_DIR=%~1"
if "%CPP_DIR%"=="" set "CPP_DIR=%CD%\Game"
for %%I in ("%CPP_DIR%") do set "CPP_DIR=%%~fI"

call "%CD%\tools\setup_python.bat"
if errorlevel 1 exit /b %errorlevel%

call "%CD%\tools\build_script_tools.bat"
if errorlevel 1 exit /b %errorlevel%

call "%CD%\tools\init_cpp_dependencies.bat" "%CPP_DIR%"
if errorlevel 1 exit /b %errorlevel%

echo ScriptTools and C++ dependencies are ready: %CPP_DIR%
exit /b 0
