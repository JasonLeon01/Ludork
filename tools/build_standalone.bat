@echo off
setlocal EnableExtensions DisableDelayedExpansion
for %%I in ("%~dp0.") do set "TOOLS_DIR=%%~fI"
for %%I in ("%TOOLS_DIR%\..") do set "ROOT_DIR=%%~fI"
cd /d "%ROOT_DIR%"
set "SCRIPT_TOOLS=%TOOLS_DIR%\ScriptTools\ScriptTools.exe"
if not exist "%SCRIPT_TOOLS%" set "SCRIPT_TOOLS=%ROOT_DIR%\.tools\ScriptTools\ScriptTools.exe"

set "USE_CURRENT_BUILD=0"
if /I "%~1"=="--use-current-build" (
    set "USE_CURRENT_BUILD=1"
    shift
)
if "%~1"=="" goto :usage
if "%~2"=="" goto :usage
if "%~3"=="" goto :usage
if not "%~4"=="" goto :usage

for %%I in ("%~1") do set "CPP_DIR=%%~fI"
for %%I in ("%~2") do set "STANDALONE_DIR=%%~fI"
set "CONFIG=%~3"
if /I not "%CONFIG%"=="Debug" if /I not "%CONFIG%"=="Release" goto :usage
set "EDITOR_CACHE_DIRECTORY="
for /f "delims=" %%V in ('""%SCRIPT_TOOLS%" packaging-constants list editor-cache-directory --separator space"') do set "EDITOR_CACHE_DIRECTORY=%%V"
if not defined EDITOR_CACHE_DIRECTORY exit /b 1
set "RESOURCE_GROUPS="
for /f "delims=" %%V in ('""%SCRIPT_TOOLS%" packaging-constants list resource-groups --separator space"') do set "RESOURCE_GROUPS=%%V"
if not defined RESOURCE_GROUPS exit /b 1
set "RUNTIME_LEGAL_FILES="
for /f "delims=" %%V in ('""%SCRIPT_TOOLS%" packaging-constants list runtime-legal-files --separator space"') do set "RUNTIME_LEGAL_FILES=%%V"
if not defined RUNTIME_LEGAL_FILES exit /b 1
set "LUDORK_STANDALONE_SOURCE_PATH=%CPP_DIR%"
set "LUDORK_STANDALONE_TARGET_PATH=%STANDALONE_DIR%"
powershell -NoProfile -Command "function Test-ReparseAncestor([string] $value) { $current = $value; while (-not [string]::IsNullOrEmpty($current)) { if (Test-Path -LiteralPath $current) { $item = Get-Item -Force -LiteralPath $current; while ($null -ne $item) { if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { return $true }; $item = $item.Parent }; return $false }; $parent = [IO.Path]::GetDirectoryName($current); if ($parent -eq $current) { return $false }; $current = $parent }; return $false }; $source = [IO.Path]::GetFullPath($env:LUDORK_STANDALONE_SOURCE_PATH).TrimEnd('\') + '\'; $target = [IO.Path]::GetFullPath($env:LUDORK_STANDALONE_TARGET_PATH).TrimEnd('\') + '\'; if ((Test-ReparseAncestor $source) -or (Test-ReparseAncestor $target) -or $source.StartsWith($target, [StringComparison]::OrdinalIgnoreCase)) { exit 1 }; foreach ($name in @('Assets', 'Data', 'Scripts', 'Binaries', $env:EDITOR_CACHE_DIRECTORY, 'Cache', 'bin', 'build', 'Licenses', 'ThirdPartySource', 'Engine', 'Intermediate')) { $protected = [IO.Path]::Combine($source, $name).TrimEnd('\') + '\'; if ($target.StartsWith($protected, [StringComparison]::OrdinalIgnoreCase)) { exit 1 } }"
set "LUDORK_STANDALONE_SOURCE_PATH="
set "LUDORK_STANDALONE_TARGET_PATH="
if errorlevel 1 (
    echo Standalone output must not be the C++ project or one of its parent directories: %STANDALONE_DIR%
    exit /b 1
)

if "%USE_CURRENT_BUILD%"=="1" (
    if not exist "%SCRIPT_TOOLS%" (
        echo ScriptTools was not found. Run tools\init.bat first.
        exit /b 1
    )
    "%SCRIPT_TOOLS%" ui-assets validate "%CPP_DIR%"
    if errorlevel 1 exit /b 1
) else (
    call "%TOOLS_DIR%\build_cpp.bat" "%CPP_DIR%" "%CONFIG%"
    if errorlevel 1 exit /b 1
    cmake --build "%CPP_DIR%\build" --config "%CONFIG%" --target LudorkLauncher
    if errorlevel 1 exit /b 1
)

if not exist "%CPP_DIR%\Assets" (
    echo Assets folder was not found: %CPP_DIR%\Assets
    exit /b 1
)
if not exist "%CPP_DIR%\Data" (
    echo Data folder was not found: %CPP_DIR%\Data
    exit /b 1
)
if not exist "%CPP_DIR%\Scripts" (
    echo Scripts folder was not found: %CPP_DIR%\Scripts
    exit /b 1
)
if "%LUDORK_VALIDATE_LDPAK_SOURCE%"=="1" (
    if not exist "%SCRIPT_TOOLS%" (
        echo ScriptTools was not found. Run tools\init.bat first.
        exit /b 1
    )
    "%SCRIPT_TOOLS%" validate-ldpak-source "%CPP_DIR%"
    if errorlevel 1 exit /b 1
)

if not exist "%STANDALONE_DIR%" mkdir "%STANDALONE_DIR%"
if exist "%STANDALONE_DIR%\Assets" rmdir /S /Q "%STANDALONE_DIR%\Assets"
if exist "%STANDALONE_DIR%\Assets" exit /b 1
if exist "%STANDALONE_DIR%\Data" rmdir /S /Q "%STANDALONE_DIR%\Data"
if exist "%STANDALONE_DIR%\Data" exit /b 1
if exist "%STANDALONE_DIR%\Scripts" rmdir /S /Q "%STANDALONE_DIR%\Scripts"
if exist "%STANDALONE_DIR%\Scripts" exit /b 1
for %%R in (%RESOURCE_GROUPS%) do (
    if exist "%STANDALONE_DIR%\%%R.ldpak" del /Q "%STANDALONE_DIR%\%%R.ldpak"
    if exist "%STANDALONE_DIR%\%%R.ldpak" exit /b 1
)
robocopy "%CPP_DIR%\Assets" "%STANDALONE_DIR%\Assets" /E /NFL /NDL /NJH /NJS /NP
if errorlevel 8 exit /b %errorlevel%
robocopy "%CPP_DIR%\Data" "%STANDALONE_DIR%\Data" /E /XF *.anim.json /NFL /NDL /NJH /NJS /NP
if errorlevel 8 exit /b %errorlevel%
robocopy "%CPP_DIR%\Scripts" "%STANDALONE_DIR%\Scripts" /E /NFL /NDL /NJH /NJS /NP
if errorlevel 8 exit /b %errorlevel%
if exist "%STANDALONE_DIR%\Binaries" rmdir /S /Q "%STANDALONE_DIR%\Binaries"
mkdir "%STANDALONE_DIR%\Binaries"
if errorlevel 1 exit /b %errorlevel%
robocopy "%CPP_DIR%\bin\%CONFIG%" "%STANDALONE_DIR%\Binaries" /E /XF *.pdb /NFL /NDL /NJH /NJS /NP
if errorlevel 8 exit /b %errorlevel%
if not exist "%STANDALONE_DIR%\Binaries\Main.exe" (
    echo Standalone output is missing Binaries\Main.exe.
    exit /b 1
)
set "LAUNCHER=%CPP_DIR%\build\launcher\%CONFIG%\Main.exe"
if not exist "%LAUNCHER%" (
    echo Standalone launcher was not found: %LAUNCHER%
    exit /b 1
)
copy /Y "%LAUNCHER%" "%STANDALONE_DIR%\Main.exe" >nul
if errorlevel 1 exit /b %errorlevel%
if exist "%CPP_DIR%\Licenses" (
    robocopy "%CPP_DIR%\Licenses" "%STANDALONE_DIR%\Licenses" /E /NFL /NDL /NJH /NJS /NP
    if errorlevel 8 exit /b %errorlevel%
)
if exist "%CPP_DIR%\ThirdPartySource" (
    robocopy "%CPP_DIR%\ThirdPartySource" "%STANDALONE_DIR%\ThirdPartySource" /E /NFL /NDL /NJH /NJS /NP
    if errorlevel 8 exit /b %errorlevel%
    if exist "%CPP_DIR%\Engine\cmake\FFmpeg" (
        robocopy "%CPP_DIR%\Engine\cmake\FFmpeg" "%STANDALONE_DIR%\ThirdPartySource\FFmpeg-Build" /E /NFL /NDL /NJH /NJS /NP
        if errorlevel 8 exit /b %errorlevel%
    )
)
for %%F in (%RUNTIME_LEGAL_FILES%) do if exist "%CPP_DIR%\%%F" (
    copy /Y "%CPP_DIR%\%%F" "%STANDALONE_DIR%\%%F" >nul
    if errorlevel 1 exit /b %errorlevel%
)

if exist "%STANDALONE_DIR%\%EDITOR_CACHE_DIRECTORY%" rmdir /S /Q "%STANDALONE_DIR%\%EDITOR_CACHE_DIRECTORY%"
if exist "%STANDALONE_DIR%\%EDITOR_CACHE_DIRECTORY%" exit /b 1
"%SCRIPT_TOOLS%" ui-preview copy "%CPP_DIR%" "%STANDALONE_DIR%"
if errorlevel 1 exit /b %errorlevel%
if exist "%STANDALONE_DIR%\Cache" rmdir /S /Q "%STANDALONE_DIR%\Cache"
if exist "%STANDALONE_DIR%\Cache" exit /b 1
call :validate_runtime_layout
if errorlevel 1 exit /b %errorlevel%

if not exist "%STANDALONE_DIR%\Main.exe" (
    echo Standalone output is missing Main.exe.
    exit /b 1
)

echo Standalone build complete: "%STANDALONE_DIR%"
exit /b 0

:usage
echo Usage: tools\build_standalone.bat [--use-current-build] ^<cpp-folder^> ^<standalone-folder^> ^<Debug^|Release^>
exit /b 1

:validate_runtime_layout
if not exist "%STANDALONE_DIR%\Binaries\Main.exe" (
    echo Standalone runtime executable is missing: "%STANDALONE_DIR%\Binaries\Main.exe"
    exit /b 1
)
set "RUNTIME_LIBRARY_FOUND=0"
for /f "delims=" %%F in ('dir /b /a-d "%STANDALONE_DIR%\*.dll" "%STANDALONE_DIR%\*.so" "%STANDALONE_DIR%\*.dylib" "%STANDALONE_DIR%\*.so.*" 2^>nul') do (
    echo Runtime library exists outside Binaries: "%%F"
    exit /b 1
)
for /f "delims=" %%F in ('dir /b /a-d "%STANDALONE_DIR%\Binaries\*.dll" 2^>nul') do set "RUNTIME_LIBRARY_FOUND=1"
if "%RUNTIME_LIBRARY_FOUND%"=="0" (
    echo Standalone output contains no runtime libraries in Binaries.
    exit /b 1
)
exit /b 0
