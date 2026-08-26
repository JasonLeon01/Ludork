#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"

if [ "$#" -eq 1 ] && { [ "$1" = "Debug" ] || [ "$1" = "Release" ]; }; then
    CPP_DIR="$PROJECT_ROOT/Sample"
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
SCRIPT_TOOLS=$(resolve_script_tools)
LUAC_CACHE="$PROJECT_ROOT/.tools/Lua/luac"

CMAKE_BIN=$(find_cmake)
BUILD_JOBS=$(resolve_parallel_jobs)
echo "Project: $CPP_DIR"
echo "Configuration: $CONFIG"
echo "Parallel jobs: $BUILD_JOBS"
"$SCRIPT_TOOLS" ui-assets validate "$CPP_DIR"

set -- \
    -S "$CPP_DIR" \
    -B "$CPP_DIR/build" \
    -DCMAKE_BUILD_TYPE="$CONFIG" \
    -DLUDORK_SCRIPT_TOOLS_EXECUTABLE="$SCRIPT_TOOLS" \
    -DLUDORK_LUAC_CACHE_FILE="$LUAC_CACHE"

DEPENDENCY_NAMES="flac freetype harfbuzz libssh2 mbedtls ogg sheenbidi vorbis"
dependency_cache_is_ready() {
    cache_dir=$1
    for dependency_name in $DEPENDENCY_NAMES; do
        if [ ! -f "$cache_dir/$dependency_name-src/CMakeLists.txt" ]; then
            return 1
        fi
    done
}

dependency_cache=
for cache_candidate in \
    "${LUDORK_DEPENDENCY_CACHE:-}" \
    "$PROJECT_ROOT/Sample/build/_deps" \
    "$PROJECT_ROOT/Sample/build/$CONFIG/_deps"
do
    if [ -n "$cache_candidate" ] && dependency_cache_is_ready "$cache_candidate"; then
        dependency_cache=$cache_candidate
        break
    fi
done
if [ "$CPP_DIR" != "$PROJECT_ROOT/Sample" ] && [ -n "$dependency_cache" ]; then
    mkdir -p "$CPP_DIR/build/_deps"
    for dependency_name in $DEPENDENCY_NAMES; do
        dependency_link="$CPP_DIR/build/_deps/$dependency_name-src"
        if [ ! -e "$dependency_link" ] && [ ! -L "$dependency_link" ]; then
            ln -s "$dependency_cache/$dependency_name-src" "$dependency_link"
        fi
    done
    set -- "$@" -DFETCHCONTENT_FULLY_DISCONNECTED=ON
fi
if [ "$(uname -s)" = "Darwin" ]; then
    if [ "$(uname -m)" != "arm64" ]; then
        echo "This macOS toolchain currently supports Apple Silicon only." >&2
        exit 1
    fi
    set -- "$@" -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=13.3
fi

"$CMAKE_BIN" "$@"
"$CMAKE_BIN" --build "$CPP_DIR/build" --config "$CONFIG" --target Main --parallel "$BUILD_JOBS"

OUTPUT="$CPP_DIR/bin/$CONFIG/Main"
if [ ! -x "$OUTPUT" ]; then
    echo "Build finished without producing $OUTPUT" >&2
    exit 1
fi
echo "Build complete: $OUTPUT"
