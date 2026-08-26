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
SCRIPT_TOOLS=$(resolve_script_tools)
CMAKE_BIN=$(find_cmake)
BUILD_JOBS=$(resolve_parallel_jobs)

echo "Project: $CPP_DIR"
echo "Configuration: $CONFIG"
echo "Parallel jobs: $BUILD_JOBS"
"$SCRIPT_TOOLS" ui-assets validate "$CPP_DIR"
"$CMAKE_BIN" \
    -S "$CPP_DIR" \
    -B "$CPP_DIR/build" \
    -DCMAKE_BUILD_TYPE="$CONFIG" \
    -DLUDORK_SCRIPT_TOOLS_EXECUTABLE="$SCRIPT_TOOLS" \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=13.3
"$CMAKE_BIN" --build "$CPP_DIR/build" --config "$CONFIG" --target Main --parallel "$BUILD_JOBS"

OUTPUT="$CPP_DIR/bin/$CONFIG/Main"
if [ ! -x "$OUTPUT" ]; then
    echo "Build finished without producing $OUTPUT" >&2
    exit 1
fi

echo "Build complete: $OUTPUT"
