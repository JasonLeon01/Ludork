@echo off
setlocal EnableDelayedExpansion

cd /d "%~dp0.."

set "PATCH_FILE=%CD%\patches\NodeGraphQt-patch-issue-411.diff"
set "TARGET_DIR=%CD%\LudorkEnv\Lib\site-packages\NodeGraphQt"

if not exist "%TARGET_DIR%\qgraphics\node_base.py" (
  echo NodeGraphQt is not installed in LudorkEnv, skipping patch.
  exit /b 0
)

if not exist "%PATCH_FILE%" (
  echo Patch file not found: %PATCH_FILE%
  exit /b 1
)

where git >nul 2>&1
if errorlevel 1 (
  echo git is required to apply NodeGraphQt patches.
  exit /b 1
)

set "PATCH_WORKDIR=%TEMP%\ludork-nodegraphqt-patch-%RANDOM%"
mkdir "%PATCH_WORKDIR%" >nul 2>&1
pushd "%PATCH_WORKDIR%"
git init -q
if errorlevel 1 (
  echo Failed to initialize temporary git workspace for NodeGraphQt patch.
  popd
  rmdir /S /Q "%PATCH_WORKDIR%" >nul 2>&1
  exit /b 1
)

set "GIT_DIR=%PATCH_WORKDIR%\.git"
pushd "%TARGET_DIR%"

git apply --reverse --check "%PATCH_FILE%" >nul 2>&1
if not errorlevel 1 (
  echo NodeGraphQt patch issue-411 is already applied.
  popd
  popd
  rmdir /S /Q "%PATCH_WORKDIR%" >nul 2>&1
  set "GIT_DIR="
  exit /b 0
)

git apply --check "%PATCH_FILE%"
if errorlevel 1 (
  echo Failed to validate NodeGraphQt patch issue-411. Installed NodeGraphQt version may differ from the patch target ^(0.6.44^).
  popd
  popd
  rmdir /S /Q "%PATCH_WORKDIR%" >nul 2>&1
  set "GIT_DIR="
  exit /b 1
)

git apply "%PATCH_FILE%"
if errorlevel 1 (
  echo Failed to apply NodeGraphQt patch issue-411.
  popd
  popd
  rmdir /S /Q "%PATCH_WORKDIR%" >nul 2>&1
  set "GIT_DIR="
  exit /b 1
)

popd
popd
rmdir /S /Q "%PATCH_WORKDIR%" >nul 2>&1
set "GIT_DIR="
echo Applied NodeGraphQt patch issue-411.
