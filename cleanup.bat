@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0"
echo "Cleaning up venv..."
if exist "LudorkEnv" (
  rmdir /S /Q "LudorkEnv"
)
echo "Cleaning up lib..."
if exist "lib" (
  rmdir /S /Q "lib"
)
echo "Cleaning up SFML..."
for /d /r %%d in (SFML) do (
  if exist "%%d\NUL" rmdir /s /q "%%d"
)
echo "Cleaning up pysf..."
for /d /r %%d in (pysf) do (
  if exist "%%d\NUL" rmdir /s /q "%%d"
)
echo "Cleaning up build..."
for /d /r %%d in (build) do (
  if exist "%%d\NUL" rmdir /s /q "%%d"
)
echo "Cleaning up .pyi, .pyd, and .so files..."
for /r %%f in (*.pyi) do if exist "%%f" del /f /q "%%f"
for /r %%f in (*.pyd) do if exist "%%f" del /f /q "%%f"
for /r %%f in (*.so) do if exist "%%f" del /f /q "%%f"
endlocal
echo "Cleanup completed"
pause
