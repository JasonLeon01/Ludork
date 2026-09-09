@echo off
setlocal EnableExtensions
chcp 65001>nul
for %%I in ("%~dp0.") do set "TOOLS_DIR=%%~fI"
for %%I in ("%TOOLS_DIR%\..") do set "ROOT_DIR=%%~fI"
if "%~1"=="" goto usage
if not "%~3"=="" goto usage
for %%I in ("%~1") do set "PROJECT_DIR=%%~fI"
set "CONFIG=%~2"
if "%CONFIG%"=="" set "CONFIG=Release"
if /I not "%CONFIG%"=="Debug" if /I not "%CONFIG%"=="Release" goto usage
if not exist "%PROJECT_DIR%\CMakeLists.txt" (
    echo CMakeLists.txt was not found: %PROJECT_DIR%
    exit /b 1
)
set "SCRIPT_TOOLS=%TOOLS_DIR%\ScriptTools.exe"
if not exist "%SCRIPT_TOOLS%" set "SCRIPT_TOOLS=%ROOT_DIR%\.tools\ScriptTools\ScriptTools.exe"
set "GNU_MAKE=%TOOLS_DIR%\gnu-make\gnumake.exe"
if not exist "%GNU_MAKE%" set "GNU_MAKE=%ROOT_DIR%\.tools\gnu-make\gnumake.exe"
if not exist "%SCRIPT_TOOLS%" (
    echo ScriptTools was not found. Prepare the editor tools first.
    exit /b 1
)
if not defined CMAKE_BUILD_PARALLEL_LEVEL set "CMAKE_BUILD_PARALLEL_LEVEL=%NUMBER_OF_PROCESSORS%"
cmake -S "%PROJECT_DIR%" -B "%PROJECT_DIR%\build" -DCMAKE_BUILD_TYPE=%CONFIG% "-DLUDORK_SCRIPT_TOOLS_EXECUTABLE=%SCRIPT_TOOLS%" "-DLUDORK_GNU_MAKE_EXECUTABLE=%GNU_MAKE%" -DLUDORK_BUILD_UI_PREVIEW_HOST=ON
if errorlevel 1 exit /b %errorlevel%
cmake --build "%PROJECT_DIR%\build" --config "%CONFIG%" --target UiPreviewHost --parallel %CMAKE_BUILD_PARALLEL_LEVEL%
if errorlevel 1 exit /b %errorlevel%
"%SCRIPT_TOOLS%" ui-preview validate "%PROJECT_DIR%"
exit /b %errorlevel%

:usage
echo Usage: tools\build_ui_preview_host.bat ^<project-folder^> [Debug^|Release]
exit /b 1
