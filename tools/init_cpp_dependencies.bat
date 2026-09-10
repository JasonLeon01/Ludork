@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0.."

if "%~1"=="" (
    echo Usage: tools\init_cpp_dependencies.bat ^<cpp-folder^>
    exit /b 1
)

for %%I in ("%~1") do set "CPP_DIR=%%~fI"
if not exist "%CPP_DIR%\Engine\ThirdParty" mkdir "%CPP_DIR%\Engine\ThirdParty"

set "LUASF_DIR=%CPP_DIR%\Engine\ThirdParty\LuaSF"
set "INIT_CPP_ROOT=%CD%"
set "INIT_CPP_DIR=%CPP_DIR%"
powershell -NoProfile -Command "$ErrorActionPreference='Stop'; $cpp=$env:INIT_CPP_DIR; $root=$env:INIT_CPP_ROOT; $names=@('luasf','lua_cjson','zlib','ffmpeg'); $ps=@(); foreach($name in $names){ $script=Join-Path $root ('tools\cpp_dependencies\' + $name + '.bat'); $psi=New-Object System.Diagnostics.ProcessStartInfo; $psi.FileName=$env:ComSpec; $psi.Arguments=('/c call ' + [char]34 + $script + [char]34 + ' ' + [char]34 + $cpp + [char]34); $psi.WorkingDirectory=$root; $psi.UseShellExecute=$false; $ps+=[Diagnostics.Process]::Start($psi) }; foreach($p in $ps){ $p.WaitForExit() }; $code=0; foreach($p in $ps){ if($p.ExitCode -ne 0){ $code=$p.ExitCode } }; exit $code"
if errorlevel 1 (
    echo A C++ dependency download failed.
    exit /b %errorlevel%
)

set "LUASF_GIT_ROOT="
set "LUASF_GIT_DIRECTORY="
for /f "usebackq delims=" %%I in (`git -C "%LUASF_DIR%" rev-parse --show-toplevel 2^>nul`) do set "LUASF_GIT_ROOT=%%I"
if defined LUASF_GIT_ROOT (
    for /f "usebackq delims=" %%I in (`git -C "%LUASF_DIR%" rev-parse --show-prefix 2^>nul`) do set "LUASF_GIT_DIRECTORY=%%I"
)

set "VALUE_COPY_PATCH=%CD%\patches\luasf-value-copy.patch"
echo Applying LuaSF native value copy patch if needed...
call :apply_luasf_patch --reverse --check "%VALUE_COPY_PATCH%" >nul 2>&1
if errorlevel 1 (
    call :apply_luasf_patch --check "%VALUE_COPY_PATCH%"
    if errorlevel 1 (
        exit /b 1
    )
    call :apply_luasf_patch "%VALUE_COPY_PATCH%"
    if errorlevel 1 (
        exit /b 1
    )
) else (
    echo LuaSF native value copy patch is already applied.
)

call "%CD%\tools\init_gnu_make.bat"
if errorlevel 1 exit /b %errorlevel%

echo C++ dependencies are ready in %CPP_DIR%
exit /b 0

:apply_luasf_patch
if defined LUASF_GIT_DIRECTORY (
    git -C "%LUASF_GIT_ROOT%" apply --unidiff-zero --directory="%LUASF_GIT_DIRECTORY%" %*
) else (
    git -C "%LUASF_DIR%" apply --unidiff-zero %*
)
exit /b %errorlevel%
