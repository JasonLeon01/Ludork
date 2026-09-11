#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"
cd "$PROJECT_ROOT"

PYTHON="$PROJECT_ROOT/.venv/bin/python"
if [ ! -x "$PYTHON" ]; then
    echo "Python environment was not found. Run tools/setup_python.sh first." >&2
    exit 1
fi
exec "$PYTHON" "$PROJECT_ROOT/tools/build_script_tools.py"
