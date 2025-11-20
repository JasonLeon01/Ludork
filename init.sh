#!/usr/bin/env bash

# Ensure working directory is the script's directory
cd "$(dirname "$0")"

ENV_DIR="LudorkEnv"
PY_CMD="python3.10"

# Detect Python 3.10
if ! command -v "$PY_CMD" >/dev/null 2>&1; then
  echo "Python 3.10 not found. Please install Python 3.10 and try again."
  exit 1
fi

# If venv exists, activate and check version
if [ -f "$ENV_DIR/bin/activate" ]; then
  . "$ENV_DIR/bin/activate"
  PV=$(python --version 2>&1 | awk '{print $2}')
  case "$PV" in
    3.10*)
      python main.py
      exit $?
      ;;
    *)
      deactivate 2>/dev/null || true
      rm -rf "$ENV_DIR"
      ;;
  esac
fi

# Create venv with Python 3.10
"$PY_CMD" -m venv "$ENV_DIR"
if [ $? -ne 0 ]; then
  rm -rf "$ENV_DIR"
  exit 1
fi

# Activate venv and install requirements
. "$ENV_DIR/bin/activate"
python -m pip install -r requirements.txt
if [ $? -ne 0 ]; then
  rm -rf "$ENV_DIR"
  exit 1
fi

# Run main.py
python main.py
exit $?