@echo off
setlocal
cd /d "%~dp0"
where node >nul 2>nul
if errorlevel 1 (
  echo Node.js 24 and npm are required.
  exit /b 1
)
node tools\deploy.mjs %*
exit /b %errorlevel%
