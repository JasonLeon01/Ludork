@echo off
setlocal EnableExtensions EnableDelayedExpansion
chcp 65001 >nul

for %%I in ("%~dp0.") do set "PROJECT_DIR=%%~fI"
if not exist "%PROJECT_DIR%\CMakeLists.txt" (
    echo CMakeLists.txt was not found: %PROJECT_DIR%
    exit /b 1
)
if not exist "%PROJECT_DIR%\Main.proj" (
    echo Main.proj was not found: %PROJECT_DIR%
    exit /b 1
)

call :resolve_tools
if errorlevel 1 exit /b 1
call :resolve_cmake
if errorlevel 1 exit /b 1

"%SCRIPT_TOOLS%" ide-config clion "%PROJECT_DIR%" --platform windows --script-tools "%SCRIPT_TOOLS%" --gnu-make "%GNU_MAKE%"
if errorlevel 1 exit /b %errorlevel%

pushd "%PROJECT_DIR%" >nul
"%CMAKE_EXE%" --preset ludork-clion-debug
set "CONFIGURE_EXIT_CODE=!ERRORLEVEL!"
popd
if not "!CONFIGURE_EXIT_CODE!"=="0" exit /b !CONFIGURE_EXIT_CODE!

if not exist "%PROJECT_DIR%\cmake-build-ludork-debug\CMakeCache.txt" (
    echo CLion project was not configured: %PROJECT_DIR%\cmake-build-ludork-debug
    exit /b 1
)

echo CLion project is configured and ready. Open this directory in CLion: %PROJECT_DIR%
exit /b 0

:resolve_tools
set "SCRIPT_TOOLS="
set "GNU_MAKE="
if defined LUDORK_TOOLS_DIR (
    call :use_tools_dir "%LUDORK_TOOLS_DIR%"
    if not defined SCRIPT_TOOLS (
        echo LUDORK_TOOLS_DIR does not contain ScriptTools.exe and gnu-make\gnumake.exe: %LUDORK_TOOLS_DIR%
        exit /b 1
    )
    exit /b 0
)
for %%I in ("%PROJECT_DIR%\..") do set "DEVELOPMENT_ROOT=%%~fI"
if exist "!DEVELOPMENT_ROOT!\.tools\ScriptTools\ScriptTools.exe" if exist "!DEVELOPMENT_ROOT!\.tools\gnu-make\gnumake.exe" (
    set "SCRIPT_TOOLS=!DEVELOPMENT_ROOT!\.tools\ScriptTools\ScriptTools.exe"
    set "GNU_MAKE=!DEVELOPMENT_ROOT!\.tools\gnu-make\gnumake.exe"
)
if not defined SCRIPT_TOOLS call :use_tools_dir "%LOCALAPPDATA%\Ludork\tools"
if not defined SCRIPT_TOOLS (
    for /f "tokens=2,*" %%A in ('reg query "HKCU\Software\Classes\Ludork.Project\shell\open\command" /ve 2^>nul ^| findstr "REG_SZ"') do set "LUDORK_OPEN_COMMAND=%%B"
    if defined LUDORK_OPEN_COMMAND (
        for /f tokens^=2^ delims^=^" %%I in ("!LUDORK_OPEN_COMMAND!") do set "LUDORK_EDITOR_EXE=%%I"
        if defined LUDORK_EDITOR_EXE (
            for %%I in ("!LUDORK_EDITOR_EXE!\..\tools") do call :use_tools_dir "%%~fI"
        )
    )
)
if not defined SCRIPT_TOOLS (
    echo Ludork ScriptTools and GNU Make were not found.
    echo Set LUDORK_TOOLS_DIR to the installed Ludork tools directory.
    exit /b 1
)
exit /b 0

:use_tools_dir
for %%I in ("%~1") do set "CANDIDATE_TOOLS_DIR=%%~fI"
if exist "!CANDIDATE_TOOLS_DIR!\ScriptTools.exe" if exist "!CANDIDATE_TOOLS_DIR!\gnu-make\gnumake.exe" (
    set "SCRIPT_TOOLS=!CANDIDATE_TOOLS_DIR!\ScriptTools.exe"
    set "GNU_MAKE=!CANDIDATE_TOOLS_DIR!\gnu-make\gnumake.exe"
)
exit /b 0

:resolve_cmake
set "CMAKE_EXE="
for /f "delims=" %%I in ('where cmake.exe 2^>nul') do if not defined CMAKE_EXE set "CMAKE_EXE=%%I"
if not defined CMAKE_EXE if exist "%ProgramFiles%\CMake\bin\cmake.exe" set "CMAKE_EXE=%ProgramFiles%\CMake\bin\cmake.exe"
if not defined CMAKE_EXE (
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if exist "!VSWHERE!" (
        for /f "delims=" %%I in ('"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe 2^>nul') do if not defined CMAKE_EXE set "CMAKE_EXE=%%I"
    )
)
if not defined CMAKE_EXE (
    echo CMake was not found. Install VS2022 Desktop development with C++ or add CMake to PATH.
    exit /b 1
)
"%CMAKE_EXE%" --help | findstr /C:"Visual Studio 17 2022" >nul
if errorlevel 1 (
    echo CMake does not provide the Visual Studio 17 2022 generator: %CMAKE_EXE%
    exit /b 1
)
exit /b 0
