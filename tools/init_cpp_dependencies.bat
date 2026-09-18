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
set "SFML_DIR=%CPP_DIR%\Engine\ThirdParty\SFML"
set "SOL2_DIR=%CPP_DIR%\Engine\ThirdParty\sol2"
set "LUA_DIR=%CPP_DIR%\Engine\ThirdParty\Lua"
set "INIT_CPP_ROOT=%CD%"
set "INIT_CPP_DIR=%CPP_DIR%"
powershell -NoProfile -Command "$ErrorActionPreference='Stop'; $cpp=$env:INIT_CPP_DIR; $root=$env:INIT_CPP_ROOT; $names=@('luasf','sfml','sol2','lua','lua_cjson','zlib','ffmpeg'); $ps=@(); foreach($name in $names){ $script=Join-Path $root ('tools\cpp_dependencies\' + $name + '.bat'); $psi=New-Object System.Diagnostics.ProcessStartInfo; $psi.FileName=$env:ComSpec; $psi.Arguments=('/c call ' + [char]34 + $script + [char]34 + ' ' + [char]34 + $cpp + [char]34); $psi.WorkingDirectory=$root; $psi.UseShellExecute=$false; $ps+=[Diagnostics.Process]::Start($psi) }; foreach($p in $ps){ $p.WaitForExit() }; $code=0; foreach($p in $ps){ if($p.ExitCode -ne 0){ $code=$p.ExitCode } }; exit $code"
if errorlevel 1 (
    echo A C++ dependency download failed.
    exit /b %errorlevel%
)

set "VALUE_COPY_PATCH=%CD%\patches\luasf-value-copy.patch"
call :resolve_patch_target "%LUASF_DIR%"
echo Applying LuaSF native value copy patch if needed...
call :apply_patch --unidiff-zero --reverse --check "%VALUE_COPY_PATCH%" >nul 2>&1
if errorlevel 1 (
    call :apply_patch --unidiff-zero --check "%VALUE_COPY_PATCH%"
    if errorlevel 1 (
        exit /b 1
    )
    call :apply_patch --unidiff-zero "%VALUE_COPY_PATCH%"
    if errorlevel 1 (
        exit /b 1
    )
) else (
    echo LuaSF native value copy patch is already applied.
)

set "SOL2_PR1606_PATCH=%CD%\patches\sol2-pr1606.patch"
call :resolve_patch_target "%SOL2_DIR%"
echo Applying sol2 PR #1606 patch if needed...
rem The published sol2 headers use CRLF line endings, so this patch has to
rem ignore whitespace to match.
call :apply_patch --ignore-whitespace --reverse --check "%SOL2_PR1606_PATCH%" >nul 2>&1
if errorlevel 1 (
    call :apply_patch --ignore-whitespace --check "%SOL2_PR1606_PATCH%"
    if errorlevel 1 (
        exit /b 1
    )
    call :apply_patch --ignore-whitespace "%SOL2_PR1606_PATCH%"
    if errorlevel 1 (
        exit /b 1
    )
) else (
    echo sol2 PR #1606 patch is already applied.
)

call "%CD%\tools\init_gnu_make.bat"
if errorlevel 1 exit /b %errorlevel%

echo C++ dependencies are ready in %CPP_DIR%
exit /b 0

:resolve_patch_target
set "PATCH_TARGET_DIR=%~f1"
set "PATCH_GIT_ROOT="
set "PATCH_GIT_DIRECTORY="
for /f "usebackq delims=" %%I in (`git -C "%~f1" rev-parse --show-toplevel 2^>nul`) do set "PATCH_GIT_ROOT=%%I"
if defined PATCH_GIT_ROOT for /f "usebackq delims=" %%I in (`git -C "%~f1" rev-parse --show-prefix 2^>nul`) do set "PATCH_GIT_DIRECTORY=%%I"
exit /b 0

:apply_patch
if defined PATCH_GIT_DIRECTORY (
    git -C "%PATCH_GIT_ROOT%" apply --directory="%PATCH_GIT_DIRECTORY%" %*
) else (
    git -C "%PATCH_TARGET_DIR%" apply %*
)
exit /b %errorlevel%
