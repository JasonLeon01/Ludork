#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"
SCRIPT_TOOLS=$(resolve_script_tools)
exec "$SCRIPT_TOOLS" ios-pack "$@"
