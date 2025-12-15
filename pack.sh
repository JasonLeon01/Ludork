#!/usr/bin/env bash

cd "$(dirname "$0")"

ENV_DIR="LudorkEnv"

if [ -f "$ENV_DIR/bin/activate" ]; then
  . "$ENV_DIR/bin/activate"
else
  echo "Warning: virtual environment '$ENV_DIR' not found. Continuing without activation."
fi

if command -v python >/dev/null 2>&1; then
  PY="python"
else
  PY="python3"
fi

$PY pack.py
