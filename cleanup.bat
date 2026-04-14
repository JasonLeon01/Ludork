@echo off
setlocal EnableDelayedExpansion
call del_cache.bat
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
rmdir /s /q "C_Extensions\SFML"
echo "Cleaning up pysf..."
rmdir /s /q "pysf"
echo "Cleaning up build..."
rmdir /s /q "build"
echo "Cleaning up .pyi, .pyd, and .so files..."
for /r %%f in (*.pyi) do if exist "%%f" del /f /q "%%f"
for /r %%f in (*.pyd) do if exist "%%f" del /f /q "%%f"
for /r %%f in (*.so) do if exist "%%f" del /f /q "%%f"
endlocal
echo "Cleanup completed"
pause
