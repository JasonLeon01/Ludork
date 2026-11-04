#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"

if [ "$(uname -s)" != "Darwin" ] || [ "$(uname -m)" != "arm64" ]; then
    echo "Android packaging requires Apple Silicon macOS." >&2
    exit 20
fi

TOOLCHAIN_DIRECTORY=$(resolve_toolchain_directory)
exec "$TOOLCHAIN_DIRECTORY/ludork-pack" android-pack "$@"
