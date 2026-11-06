#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"

if [ "$#" -ne 2 ]; then
    echo "Usage: tools/build_cpp.sh <cpp-folder> <Debug|Release>" >&2
    exit 1
fi

CPP_DIR=$(absolute_path "$1")
CONFIG=$2
if [ "$CONFIG" != "Debug" ] && [ "$CONFIG" != "Release" ]; then
    echo "Configuration must be Debug or Release." >&2
    exit 1
fi
if [ ! -f "$CPP_DIR/CMakeLists.txt" ]; then
    echo "CMakeLists.txt was not found: $CPP_DIR" >&2
    exit 1
fi
if [ ! -f "$CPP_DIR/Main.proj" ]; then
    echo "Main.proj was not found: $CPP_DIR" >&2
    exit 1
fi

require_macos_arm64
TOOLCHAIN_DIRECTORY=$(resolve_toolchain_directory)
exec "$TOOLCHAIN_DIRECTORY/ludork-build" native-build "$CPP_DIR" "$CONFIG" \
    --tools-directory "$TOOLS_DIR"
