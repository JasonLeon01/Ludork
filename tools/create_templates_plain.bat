@echo off
call "%~dp0create_templates.bat" --variant plain %*
exit /b %errorlevel%
