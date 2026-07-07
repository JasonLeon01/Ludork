@echo off
setlocal EnableDelayedExpansion
if "%~1"=="" (
  echo Usage: pack_game.bat ^<python_exe^> [options]
  exit /b 1
)
set "PYTHON_EXE=%~1"
set "SCRIPT_DIR=%~dp0"
shift
set "ARGS="
:parse_args
if "%~1"=="" goto run
set "ARGS=!ARGS! "%~1""
shift
goto parse_args
:run
call "%SCRIPT_DIR%packaging\pack-game.bat" --python "%PYTHON_EXE%" !ARGS!
exit /b %ERRORLEVEL%
