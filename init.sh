#!/usr/bin/env bash

cd "$(dirname "$0")"

mkdir -p "C_Extensions"
if [ -d "C_Extensions/SFML" ]; then
  rm -rf "C_Extensions/SFML"
fi

echo "Downloading SFML..."
curl -L -o sfml.zip "https://github.com/SFML/SFML/archive/refs/tags/3.0.1.zip"
if [ $? -ne 0 ]; then
  echo "Failed to download SFML."
  exit 1
fi

echo "Extracting SFML..."
unzip -q sfml.zip -d "C_Extensions"
if [ $? -ne 0 ]; then
  echo "Failed to extract SFML."
  rm sfml.zip
  exit 1
fi

rm sfml.zip

if [ -d "C_Extensions/SFML-3.0.1" ]; then
  mv "C_Extensions/SFML-3.0.1" "C_Extensions/SFML"
else
  echo "SFML source folder not found."
  exit 1
fi

if [ -d "Sample/Engine/pysf" ]; then
  rm -rf "Sample/Engine/pysf"
fi

echo "Downloading PySF..."
curl -L -o pysf.zip "https://github.com/JasonLeon01/PySF-AutoGenerator/releases/download/PySF3.0.1.4/pysf-3.0.1.4-macOS-ARM64.zip"
if [ $? -ne 0 ]; then
  echo "Failed to download PySF."
  exit 1
fi

echo "Extracting PySF..."
unzip -q pysf.zip -d "Sample/Engine"
if [ $? -ne 0 ]; then
  echo "Failed to extract PySF."
  rm pysf.zip
  exit 1
fi

rm pysf.zip

ENV_DIR="LudorkEnv"
PY_CMD="python3.10"

if ! command -v "$PY_CMD" >/dev/null 2>&1; then
  echo "Python 3.10 not found. Please install Python 3.10 and try again."
  exit 1
fi

if [ -f "$ENV_DIR/bin/activate" ]; then
  . "$ENV_DIR/bin/activate"
  PV=$(python --version 2>&1 | awk '{print $2}')
  case "$PV" in
    3.10*)
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
  "$PY_CMD" setup.py
  if [ $? -ne 0 ]; then
    cd ..
    exit 1
  fi
  cd ..
fi

"$PY_CMD" main.py
exit $?
