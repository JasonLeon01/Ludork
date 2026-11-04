#!/usr/bin/env sh
set -eu
. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"
cd "$PROJECT_ROOT"
TOOLCHAIN_DIRECTORY=$(resolve_toolchain_directory)
exec "$TOOLCHAIN_DIRECTORY/ludork-assets" animation-mp4 "$@"
