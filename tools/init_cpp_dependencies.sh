#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"
if [ "$#" -ne 1 ]; then
    echo "Usage: tools/init_cpp_dependencies.sh <cpp-folder>" >&2
    exit 1
fi

CPP_DIR=$(absolute_path "$1")
if [ ! -f "$CPP_DIR/CMakeLists.txt" ]; then
    echo "CMakeLists.txt was not found: $CPP_DIR" >&2
    exit 1
fi

DEPS_DIR="$TOOLS_DIR/cpp_dependencies"
CHILD_PIDS=""

cleanup_children() {
    for pid in $CHILD_PIDS; do
        kill "$pid" 2>/dev/null || true
    done
    for pid in $CHILD_PIDS; do
        wait "$pid" 2>/dev/null || true
    done
}

trap 'cleanup_children; exit 130' HUP INT TERM

mkdir -p "$CPP_DIR/Engine/ThirdParty"

sh "$DEPS_DIR/luasf.sh" "$CPP_DIR" &
CHILD_PIDS="$CHILD_PIDS $!"
sh "$DEPS_DIR/sfml.sh" "$CPP_DIR" &
CHILD_PIDS="$CHILD_PIDS $!"
sh "$DEPS_DIR/lua.sh" "$CPP_DIR" &
CHILD_PIDS="$CHILD_PIDS $!"
sh "$DEPS_DIR/lua_cjson.sh" "$CPP_DIR" &
CHILD_PIDS="$CHILD_PIDS $!"
sh "$DEPS_DIR/zlib.sh" "$CPP_DIR" &
CHILD_PIDS="$CHILD_PIDS $!"
sh "$DEPS_DIR/ffmpeg.sh" "$CPP_DIR" &
CHILD_PIDS="$CHILD_PIDS $!"

status=0
for pid in $CHILD_PIDS; do
    wait "$pid" || status=1
done
CHILD_PIDS=""
if [ "$status" -ne 0 ]; then
    echo "A C++ dependency download failed." >&2
    exit 1
fi

echo "C++ dependencies are ready in $CPP_DIR"
