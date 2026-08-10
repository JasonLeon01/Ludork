@echo off
setlocal EnableExtensions
cd /d "%~dp0.."

dotnet run --project "%CD%\Ludork.csproj" -- %*
exit /b %errorlevel%
