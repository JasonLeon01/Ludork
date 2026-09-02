@echo off
call "%~dp0create_templates.bat" --variant ffmpeg %*
exit /b %errorlevel%
