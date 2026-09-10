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

set "BUILD_DIR=%PROJECT_DIR%\.vs\CMake"
set "SOLUTION_FILE=%PROJECT_DIR%\Main.sln"
if exist "%BUILD_DIR%\CMakeCache.txt" (
    findstr /B /C:"CMAKE_GENERATOR:INTERNAL=Visual Studio 17 2022" "%BUILD_DIR%\CMakeCache.txt" >nul
    if errorlevel 1 (
        echo Existing VS2022 CMake cache uses a different generator.
        echo Move or remove %BUILD_DIR% before generating the VS2022 solution.
        exit /b 1
    )
    findstr /B /C:"CMAKE_GENERATOR_PLATFORM:INTERNAL=x64" "%BUILD_DIR%\CMakeCache.txt" >nul
    if errorlevel 1 (
        echo Existing VS2022 CMake cache does not target x64.
        echo Move or remove %BUILD_DIR% before generating the VS2022 solution.
        exit /b 1
    )
)

"%CMAKE_EXE%" -S "%PROJECT_DIR%" -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64 "-DLUDORK_SCRIPT_TOOLS_EXECUTABLE:FILEPATH=%SCRIPT_TOOLS%" "-DLUDORK_GNU_MAKE_EXECUTABLE:FILEPATH=%GNU_MAKE%" "-DLUDORK_BUILD_CPP_SCRIPT:FILEPATH=%BUILD_CPP%"
if errorlevel 1 exit /b %errorlevel%

if not exist "%BUILD_DIR%\Main.sln" (
    echo VS2022 solution was not generated: %BUILD_DIR%\Main.sln
    exit /b 1
)

"%CMAKE_EXE%" "-DLUDORK_VS_SOURCE_SOLUTION=%BUILD_DIR%\Main.sln" "-DLUDORK_VS_OUTPUT_SOLUTION=%SOLUTION_FILE%" "-DLUDORK_VS_BINARY_DIR=%BUILD_DIR%" -P "%PROJECT_DIR%\Engine\cmake\ExportVisualStudioSolution.cmake"
if errorlevel 1 exit /b %errorlevel%

if not exist "%SOLUTION_FILE%" (
    echo VS2022 solution was not exported: %SOLUTION_FILE%
    exit /b 1
)

echo VS2022 solution is ready: %SOLUTION_FILE%
exit /b 0

:resolve_tools
set "SCRIPT_TOOLS="
set "GNU_MAKE="
set "BUILD_CPP="
if defined LUDORK_TOOLS_DIR (
    call :use_tools_dir "%LUDORK_TOOLS_DIR%"
    if not defined SCRIPT_TOOLS (
        echo LUDORK_TOOLS_DIR does not contain ScriptTools.exe, build_cpp.bat, and gnu-make\gnumake.exe: %LUDORK_TOOLS_DIR%
        exit /b 1
    )
    exit /b 0
)
for %%I in ("%PROJECT_DIR%\..") do set "DEVELOPMENT_ROOT=%%~fI"
if exist "!DEVELOPMENT_ROOT!\.tools\ScriptTools\ScriptTools.exe" if exist "!DEVELOPMENT_ROOT!\.tools\gnu-make\gnumake.exe" if exist "!DEVELOPMENT_ROOT!\tools\build_cpp.bat" (
    set "SCRIPT_TOOLS=!DEVELOPMENT_ROOT!\.tools\ScriptTools\ScriptTools.exe"
    set "GNU_MAKE=!DEVELOPMENT_ROOT!\.tools\gnu-make\gnumake.exe"
    set "BUILD_CPP=!DEVELOPMENT_ROOT!\tools\build_cpp.bat"
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
    echo Ludork ScriptTools, C++ build tool, and GNU Make were not found.
    echo Set LUDORK_TOOLS_DIR to the installed Ludork tools directory.
    exit /b 1
)
exit /b 0

:use_tools_dir
for %%I in ("%~1") do set "CANDIDATE_TOOLS_DIR=%%~fI"
if exist "!CANDIDATE_TOOLS_DIR!\ScriptTools.exe" if exist "!CANDIDATE_TOOLS_DIR!\gnu-make\gnumake.exe" if exist "!CANDIDATE_TOOLS_DIR!\build_cpp.bat" (
    set "SCRIPT_TOOLS=!CANDIDATE_TOOLS_DIR!\ScriptTools.exe"
    set "GNU_MAKE=!CANDIDATE_TOOLS_DIR!\gnu-make\gnumake.exe"
    set "BUILD_CPP=!CANDIDATE_TOOLS_DIR!\build_cpp.bat"
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
