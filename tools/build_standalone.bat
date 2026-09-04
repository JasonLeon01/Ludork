@echo off
setlocal EnableExtensions
for %%I in ("%~dp0.") do set "TOOLS_DIR=%%~fI"
for %%I in ("%TOOLS_DIR%\..") do set "ROOT_DIR=%%~fI"
cd /d "%ROOT_DIR%"
set "SCRIPT_TOOLS=%TOOLS_DIR%\ScriptTools.exe"
if not exist "%SCRIPT_TOOLS%" set "SCRIPT_TOOLS=%ROOT_DIR%\.tools\ScriptTools\ScriptTools.exe"

if "%~1"=="" goto :usage
if "%~2"=="" goto :usage
if "%~3"=="" goto :usage
if not "%~4"=="" goto :usage

for %%I in ("%~1") do set "CPP_DIR=%%~fI"
for %%I in ("%~2") do set "STANDALONE_DIR=%%~fI"
set "CONFIG=%~3"
if /I not "%CONFIG%"=="Debug" if /I not "%CONFIG%"=="Release" goto :usage

call "%TOOLS_DIR%\build_cpp.bat" "%CPP_DIR%" "%CONFIG%"
if errorlevel 1 exit /b %errorlevel%

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
if "%LUDORK_VALIDATE_PACK_ASSETS%"=="1" (
    if not exist "%SCRIPT_TOOLS%" (
        echo ScriptTools was not found. Run tools\init.bat first.
        exit /b 1
    )
    "%SCRIPT_TOOLS%" validate-asset-pack-source "%CPP_DIR%\Assets"
    if errorlevel 1 exit /b %errorlevel%
)

if not exist "%STANDALONE_DIR%" mkdir "%STANDALONE_DIR%"
robocopy "%CPP_DIR%\Assets" "%STANDALONE_DIR%\Assets" /E /NFL /NDL /NJH /NJS /NP
if errorlevel 8 exit /b %errorlevel%
robocopy "%CPP_DIR%\Data" "%STANDALONE_DIR%\Data" /E /XF *.anim.json /NFL /NDL /NJH /NJS /NP
if errorlevel 8 exit /b %errorlevel%
robocopy "%CPP_DIR%\Scripts" "%STANDALONE_DIR%\Scripts" /E /NFL /NDL /NJH /NJS /NP
if errorlevel 8 exit /b %errorlevel%
robocopy "%CPP_DIR%\bin\%CONFIG%" "%STANDALONE_DIR%" /E /XF *.pdb UiPreviewHost* UiPreviewCurveResolver* /NFL /NDL /NJH /NJS /NP
if errorlevel 8 exit /b %errorlevel%
if exist "%CPP_DIR%\Licenses" (
    robocopy "%CPP_DIR%\Licenses" "%STANDALONE_DIR%\Licenses" /E /NFL /NDL /NJH /NJS /NP
    if errorlevel 8 exit /b %errorlevel%
)
if exist "%CPP_DIR%\ThirdPartySource" (
    robocopy "%CPP_DIR%\ThirdPartySource" "%STANDALONE_DIR%\ThirdPartySource" /E /NFL /NDL /NJH /NJS /NP
    if errorlevel 8 exit /b %errorlevel%
    if exist "%CPP_DIR%\cmake\FFmpeg" (
        robocopy "%CPP_DIR%\cmake\FFmpeg" "%STANDALONE_DIR%\ThirdPartySource\FFmpeg-Build" /E /NFL /NDL /NJH /NJS /NP
        if errorlevel 8 exit /b %errorlevel%
    )
)
for %%F in (LICENSE.md THIRD_PARTY_NOTICES.md THIRD_PARTY_NOTICES_zh_CN.md) do if exist "%CPP_DIR%\%%F" (
    copy /Y "%CPP_DIR%\%%F" "%STANDALONE_DIR%\%%F" >nul
    if errorlevel 1 exit /b %errorlevel%
)

call :remove_ui_preview_host_entries "%STANDALONE_DIR%"
if errorlevel 1 exit /b %errorlevel%

if not exist "%STANDALONE_DIR%\Main.exe" (
    echo Standalone output is missing Main.exe.
    exit /b 1
)

echo Standalone build complete: %STANDALONE_DIR%
exit /b 0

:usage
echo Usage: tools\build_standalone.bat ^<cpp-folder^> ^<standalone-folder^> ^<Debug^|Release^>
exit /b 1

:remove_ui_preview_host_entries
for /r "%~1" %%F in (UiPreviewHost* UiPreviewCurveResolver*) do if exist "%%~fF" (
    del /F /Q "%%~fF"
    if errorlevel 1 exit /b 1
)
for /d /r "%~1" %%D in (UiPreviewHost* UiPreviewCurveResolver*) do if exist "%%~fD" (
    rmdir /S /Q "%%~fD"
    if errorlevel 1 exit /b 1
)
for /r "%~1" %%F in (UiPreviewHost* UiPreviewCurveResolver*) do if exist "%%~fF" exit /b 1
for /d /r "%~1" %%D in (UiPreviewHost* UiPreviewCurveResolver*) do if exist "%%~fD" exit /b 1
exit /b 0
