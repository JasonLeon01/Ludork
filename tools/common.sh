#!/usr/bin/env sh
set -eu

TOOLS_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ "$(basename -- "$TOOLS_DIR")" = "editor_runtime" ] \
    && [ -f "$TOOLS_DIR/../common.sh" ]; then
    PROJECT_ROOT=$(CDPATH= cd -- "$TOOLS_DIR/../.." && pwd)
else
    PROJECT_ROOT=$(CDPATH= cd -- "$TOOLS_DIR/.." && pwd)
fi

find_cmake() {
    configured_cmake=${LUDORK_CMAKE:-}
    if [ -n "$configured_cmake" ]; then
        set -- "$configured_cmake"
    else
        set -- \
            cmake \
            /opt/homebrew/bin/cmake \
            /usr/local/bin/cmake \
            /Applications/CMake.app/Contents/bin/cmake
    fi

    for candidate in "$@"; do
        if [ "${candidate#/}" != "$candidate" ]; then
            candidate_path=$candidate
        else
            candidate_path=$(command -v "$candidate" 2>/dev/null || true)
        fi
        if [ -z "$candidate_path" ] || [ ! -x "$candidate_path" ]; then
            continue
        fi
        if "$candidate_path" --version >/dev/null 2>&1; then
            printf '%s\n' "$candidate_path"
            return
        fi
    done

    echo "CMake was not found. Set LUDORK_CMAKE to its executable path." >&2
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

require_macos_arm64() {
    if [ "$(uname -s)" != "Darwin" ]; then
        echo "This tool currently supports macOS only." >&2
        exit 1
    fi
    if [ "$(uname -m)" != "arm64" ]; then
        echo "This macOS toolchain currently supports Apple Silicon only." >&2
        exit 1
    fi
}
