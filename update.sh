#!/usr/bin/env bash

cd "$(dirname "$0")"

mkdir -p "C_Extensions"

if [ -d "Sample/pysf" ]; then
  rm -rf "Sample/pysf"
fi

if [ -d "pysf" ]; then
  rm -rf "pysf"
fi

echo "Downloading PySF..."
curl -L -o pysf.zip "https://github.com/JasonLeon01/PySF-AutoGenerator/releases/download/PySF3.1.0.0/pysf-macOS-ARM64.zip"
if [ $? -ne 0 ]; then
  echo "Failed to download PySF."
  exit 1
fi

echo "Extracting PySF..."
unzip -q pysf.zip -d "Sample"
if [ $? -ne 0 ]; then
  echo "Failed to extract PySF to Sample."
  rm pysf.zip
  exit 1
fi

unzip -q pysf.zip -d "."
if [ $? -ne 0 ]; then
  echo "Failed to extract PySF to current folder."
  rm pysf.zip
  exit 1
fi

rm pysf.zip

ENV_DIR="LudorkEnv"
PY_CMD="python3.12"

if ! command -v "$PY_CMD" >/dev/null 2>&1; then
  echo "Python 3.12 not found. Please install Python 3.12 and try again."
  exit 1
fi

if [ -f "$ENV_DIR/bin/activate" ]; then
  . "$ENV_DIR/bin/activate"
  PV=$(python --version 2>&1 | awk '{print $2}')
  case "$PV" in
    3.12*)
      ;;
    *)
      deactivate 2>/dev/null || true
      rm -rf "$ENV_DIR"
      ;;
  esac
fi

if [ ! -f "$ENV_DIR/bin/activate" ]; then
  "$PY_CMD" -m venv "$ENV_DIR"
  if [ $? -ne 0 ]; then
    rm -rf "$ENV_DIR"
    exit 1
  fi

  . "$ENV_DIR/bin/activate"
  "$PY_CMD" -m pip install -r requirements.txt
  if [ $? -ne 0 ]; then
    rm -rf "$ENV_DIR"
    exit 1
  fi
fi

if [ -d "C_Extensions" ]; then
  cd C_Extensions
  "$PY_CMD" setup.py --no-clean
  if [ $? -ne 0 ]; then
    cd ..
    exit 1
  fi
  cd ..
fi

"$PY_CMD" main.py
exit $?
