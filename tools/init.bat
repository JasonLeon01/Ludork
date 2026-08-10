@echo off
setlocal EnableExtensions
cd /d "%~dp0.."

set "CPP_DIR=%~1"
if "%CPP_DIR%"=="" set "CPP_DIR=%CD%\Sample"
for %%I in ("%CPP_DIR%") do set "CPP_DIR=%%~fI"
set "SAMPLE_DIR=%CD%\Sample"

call "%CD%\tools\setup_python.bat"
if errorlevel 1 exit /b %errorlevel%

call "%CD%\tools\build_script_tools.bat"
if errorlevel 1 exit /b %errorlevel%

call "%CD%\tools\init_cpp_dependencies.bat" "%CPP_DIR%"
if errorlevel 1 exit /b %errorlevel%

if /I not "%CPP_DIR%"=="%SAMPLE_DIR%" goto custom_project_ready

call "%CD%\tools\build_ui_preview_host.bat" Release
if errorlevel 1 exit /b %errorlevel%

echo ScriptTools, C++ dependencies, and UiPreviewHost are ready: %CPP_DIR%
exit /b 0

:custom_project_ready
echo ScriptTools and C++ dependencies are ready: %CPP_DIR%
echo UiPreviewHost was not built because this init targets a custom C++ project. Run tools\init.bat without a project path to prepare the editor tool.
exit /b 0
