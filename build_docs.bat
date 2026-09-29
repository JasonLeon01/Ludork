@echo off
node "%~dp0build_docs.mjs" %*
exit /b %errorlevel%
