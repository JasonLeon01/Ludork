#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"
if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    echo "Usage: tools/build_ui_preview_host.sh <project-folder> [Debug|Release]" >&2
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
SCRIPT_TOOLS=$(resolve_script_tools)
CMAKE_BIN=$(find_cmake)
BUILD_JOBS=$(resolve_parallel_jobs)
set -- -S "$CPP_DIR" -B "$CPP_DIR/build" -DCMAKE_BUILD_TYPE="$CONFIG" \
    -DLUDORK_SCRIPT_TOOLS_EXECUTABLE="$SCRIPT_TOOLS" -DLUDORK_BUILD_UI_PREVIEW_HOST=ON
if [ "$(uname -s)" = Darwin ]; then
    set -- "$@" -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=13.3
fi
"$CMAKE_BIN" "$@"
"$CMAKE_BIN" --build "$CPP_DIR/build" --config "$CONFIG" \
    --target UiPreviewHost --parallel "$BUILD_JOBS"
"$SCRIPT_TOOLS" ui-preview validate "$CPP_DIR"
