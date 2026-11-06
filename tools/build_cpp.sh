#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"

if [ "$#" -eq 1 ] && { [ "$1" = "Debug" ] || [ "$1" = "Release" ]; }; then
    CPP_DIR="$PROJECT_ROOT/Game"
    CONFIG=$1
elif [ "$#" -eq 2 ]; then
    CPP_DIR=$(absolute_path "$1")
    CONFIG=$2
else
    echo "Usage: tools/build_cpp.sh <Debug|Release>" >&2
    echo "       tools/build_cpp.sh <cpp-folder> <Debug|Release>" >&2
    exit 1
fi

if [ "$CONFIG" != "Debug" ] && [ "$CONFIG" != "Release" ]; then
    echo "Configuration must be Debug or Release." >&2
    exit 1
fi
if [ ! -f "$CPP_DIR/CMakeLists.txt" ]; then
    echo "CMakeLists.txt was not found: $CPP_DIR" >&2
    exit 1
fi
TOOLCHAIN_DIRECTORY=$(resolve_toolchain_directory)
exec "$TOOLCHAIN_DIRECTORY/ludork-build" native-build "$CPP_DIR" "$CONFIG" \
    --tools-directory "$TOOLS_DIR" --repository-root "$PROJECT_ROOT" \
    --luac-cache-file "$PROJECT_ROOT/.tools/Lua/luac"
