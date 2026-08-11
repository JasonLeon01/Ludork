#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"

if [ "$(uname -s)" != "Darwin" ]; then
    echo "HarmonyOS packaging is only supported on macOS." >&2
    exit 20
fi

SCRIPT_TOOLS=$(resolve_script_tools)
exec "$SCRIPT_TOOLS" harmony-pack "$@"
