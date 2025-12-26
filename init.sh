#!/usr/bin/env bash

cd "$(dirname "$0")"

if [ -d "Sample/Engine/pysf" ]; then
  rm -rf "Sample/Engine/pysf"
fi

echo "Downloading PySF..."
curl -L -o pysf.zip "https://github.com/JasonLeon01/PySF-AutoGenerator/releases/download/PySF3.0.1.2/pysf-3.0.1.2-macOS-ARM64.zip"
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
  python -m pip install -r requirements.txt
  if [ $? -ne 0 ]; then
    rm -rf "$ENV_DIR"
    exit 1
  fi
fi

if [ -d "C_Extentions" ]; then
  cd C_Extentions
  for d in */; do
    if [ -d "$d" ]; then
      echo "Building extension in $d..."
      cd "$d"
      python setup.py build
      if [ $? -ne 0 ]; then
        echo "Failed to build extension in $d."
        cd ..
        cd ..
        exit 1
      fi
      python move.py
      if [ $? -ne 0 ]; then
        echo "Failed to move extension in $d."
        cd ..
        cd ..
        exit 1
      fi
      cd ..
    fi
  done
  cd ..
fi

python main.py
exit $?
