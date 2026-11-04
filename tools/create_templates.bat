@echo off
setlocal EnableExtensions
cd /d "%~dp0.."
"%CD%\.tools\Toolchain\ludork-pack.exe" create-templates --repository-root "%CD%" %*
exit /b %errorlevel%
