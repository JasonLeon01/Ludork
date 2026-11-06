#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"
if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    echo "Usage: tools/build_preview_host.sh <project-folder> [Debug|Release]" >&2
    exit 1
fi
CPP_DIR=$(absolute_path "$1")
CONFIG=${2:-Release}
if [ "$CONFIG" != Debug ] && [ "$CONFIG" != Release ]; then
    echo "Configuration must be Debug or Release." >&2
    exit 1
fi
if [ ! -f "$CPP_DIR/CMakeLists.txt" ]; then
    echo "CMakeLists.txt was not found: $CPP_DIR" >&2
    exit 1
fi
TOOLCHAIN_DIRECTORY=$(resolve_toolchain_directory)
exec "$TOOLCHAIN_DIRECTORY/ludork-build" native-build "$CPP_DIR" "$CONFIG" \
    --target PreviewHost --tools-directory "$TOOLS_DIR"
