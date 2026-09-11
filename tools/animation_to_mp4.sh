#!/usr/bin/env sh
set -eu
. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"
cd "$PROJECT_ROOT"
SCRIPT_TOOLS=$(resolve_script_tools)
exec "$SCRIPT_TOOLS" animation-mp4 "$@"
