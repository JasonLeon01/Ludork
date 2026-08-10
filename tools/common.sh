#!/usr/bin/env sh
set -eu

TOOLS_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_ROOT=$(CDPATH= cd -- "$TOOLS_DIR/.." && pwd)

find_cmake() {
    if command -v cmake >/dev/null 2>&1; then
        command -v cmake
        return
    fi
    if [ -x "/Applications/CMake.app/Contents/bin/cmake" ]; then
        printf '%s\n' "/Applications/CMake.app/Contents/bin/cmake"
        return
    fi
    echo "CMake was not found." >&2
    exit 1
}

absolute_path() {
    path=$1
    if [ -d "$path" ]; then
        (CDPATH= cd -- "$path" && pwd)
        return
    fi
    parent=$(dirname -- "$path")
    name=$(basename -- "$path")
    parent=$(CDPATH= cd -- "$parent" && pwd)
    printf '%s/%s\n' "$parent" "$name"
}

resolve_script_tools() {
    for candidate in \
        "$TOOLS_DIR/ScriptTools" \
        "$PROJECT_ROOT/.tools/ScriptTools/ScriptTools"
    do
        if [ -x "$candidate" ]; then
            printf '%s\n' "$candidate"
            return
        fi
    done
    echo "ScriptTools was not found. Run tools/init.sh first." >&2
    exit 1
}
