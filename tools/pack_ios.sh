#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"
TOOLCHAIN_DIRECTORY=$(resolve_toolchain_directory)
exec "$TOOLCHAIN_DIRECTORY/ludork-pack" ios-pack "$@"
