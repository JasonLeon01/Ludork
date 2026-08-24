#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"

CONFIG=${1:-Release}
if [ "$#" -gt 1 ] || { [ "$CONFIG" != "Debug" ] && [ "$CONFIG" != "Release" ]; }; then
    echo "Usage: tools/build_ui_preview_host.sh [Debug|Release]" >&2
    exit 1
fi

PROJECT_DIR="$PROJECT_ROOT/UiPreviewHost"
BUILD_DIR="$PROJECT_ROOT/.tools/UiPreviewHost/build"
SCRIPT_TOOLS="$PROJECT_ROOT/.tools/ScriptTools/ScriptTools"
GNU_MAKE="$PROJECT_ROOT/.tools/gnu-make/gnumake"

if [ ! -x "$SCRIPT_TOOLS" ]; then
    echo "ScriptTools was not found. Run tools/init.sh first." >&2
    exit 1
fi

CMAKE=$(find_cmake)
set -- \
    -S "$PROJECT_DIR" \
    -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$CONFIG" \
    -DLUDORK_SCRIPT_TOOLS_EXECUTABLE="$SCRIPT_TOOLS"
if [ -x "$GNU_MAKE" ]; then
    set -- "$@" -DLUDORK_GNU_MAKE_EXECUTABLE="$GNU_MAKE"
fi
"$CMAKE" "$@"
"$CMAKE" --build "$BUILD_DIR" --config "$CONFIG" \
    --target UiPreviewHost --parallel

OUTPUT="$PROJECT_ROOT/.tools/UiPreviewHost/bin/$CONFIG/UiPreviewHost"
if [ ! -x "$OUTPUT" ]; then
    echo "Build finished without producing $OUTPUT" >&2
    exit 1
fi
RUNTIME=$(find \
    "$PROJECT_ROOT/.tools/UiPreviewHost/bin/$CONFIG" \
    -maxdepth 1 \
    -type f \
    -name 'UiPreviewHostRuntime.*' \
    -print \
    -quit)
if [ -z "$RUNTIME" ]; then
    echo "Build finished without producing UiPreviewHostRuntime" >&2
    exit 1
fi
echo "UI preview host ready: $OUTPUT"
