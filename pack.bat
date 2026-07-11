@echo off
setlocal
call "%~dp0tools\packaging\pack-editor.bat" %*
set "PACK_RC=%ERRORLEVEL%"
pause
exit /b %PACK_RC%
