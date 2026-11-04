@echo off
node "%~dp0build_docs_release.mjs" %*
exit /b %errorlevel%
