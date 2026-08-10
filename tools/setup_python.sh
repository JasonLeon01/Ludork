#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"
cd "$PROJECT_ROOT"

VENV_PYTHON="$PROJECT_ROOT/.venv/bin/python"
if [ ! -f "$PROJECT_ROOT/requirements.txt" ]; then
    echo "requirements.txt was not found: $PROJECT_ROOT/requirements.txt" >&2
    exit 1
fi

if [ ! -x "$VENV_PYTHON" ]; then
    PYTHON_BIN=${PYTHON:-python3.12}
    if ! command -v "$PYTHON_BIN" >/dev/null 2>&1; then
        echo "Python 3.12 was not found." >&2
        exit 1
    fi
    echo "Creating .venv with Python 3.12..."
    "$PYTHON_BIN" -m venv "$PROJECT_ROOT/.venv"
else
    echo "Using existing .venv."
fi
echo "Installing Python requirements into .venv..."
"$VENV_PYTHON" -m pip install -r "$PROJECT_ROOT/requirements.txt"
