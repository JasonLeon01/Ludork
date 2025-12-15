#!/usr/bin/env bash

cd "$(dirname "$0")"

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
      python main.py
      exit $?
      ;;
    *)
      deactivate 2>/dev/null || true
      rm -rf "$ENV_DIR"
      ;;
  esac
fi

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

python main.py
exit $?
