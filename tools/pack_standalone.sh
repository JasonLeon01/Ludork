#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"

require_macos_arm64
TOOLCHAIN_DIRECTORY=$(resolve_toolchain_directory)
exec "$TOOLCHAIN_DIRECTORY/ludork-pack" standalone-pack --tools-directory "$TOOLS_DIR" "$@"
