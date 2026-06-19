#!/usr/bin/env bash
set -euo pipefail

PYTHON_EXE="${1:?python executable required}"
shift
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$PYTHON_EXE" "${SCRIPT_DIR}/pack_game.py" "$@"
