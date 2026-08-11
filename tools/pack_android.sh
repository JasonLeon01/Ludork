#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"

if [ "$(uname -s)" != "Darwin" ] || [ "$(uname -m)" != "arm64" ]; then
    echo "Android packaging requires Apple Silicon macOS." >&2
    exit 20
fi

SCRIPT_TOOLS=$(resolve_script_tools)
exec "$SCRIPT_TOOLS" android-pack "$@"
