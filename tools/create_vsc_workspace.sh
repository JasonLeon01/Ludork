#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"
cd "$PROJECT_ROOT"

PYTHON_BIN=${PYTHON:-python3}
if ! command -v "$PYTHON_BIN" >/dev/null 2>&1; then
    echo "Python 3 was not found. Install it or set PYTHON to its executable." >&2
    exit 1
fi

exec "$PYTHON_BIN" "$PROJECT_ROOT/tools/create_vsc_workspace.py"
