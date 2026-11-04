#!/usr/bin/env sh
set -eu

TOOLS_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ -f "$TOOLS_DIR/../common.sh" ]; then
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

resolve_parallel_jobs() {
    parallel_jobs=${CMAKE_BUILD_PARALLEL_LEVEL:-}
    if [ -z "$parallel_jobs" ]; then
        if [ "$(uname -s)" = "Darwin" ]; then
            parallel_jobs=$(sysctl -n hw.logicalcpu)
        else
            parallel_jobs=$(getconf _NPROCESSORS_ONLN)
        fi
    fi

    case "$parallel_jobs" in
        '' | *[!0-9]* | 0)
            echo "CMAKE_BUILD_PARALLEL_LEVEL must be a positive integer." >&2
            exit 1
            ;;
    esac
    printf '%s\n' "$parallel_jobs"
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

resolve_toolchain_directory() {
    if [ -n "${LUDORK_TOOLCHAIN_DIRECTORY:-}" ]; then
        if [ ! -x "$LUDORK_TOOLCHAIN_DIRECTORY/ludork-build" ]; then
            echo "LUDORK_TOOLCHAIN_DIRECTORY does not contain ludork-build: $LUDORK_TOOLCHAIN_DIRECTORY" >&2
            return 1
        fi
        (CDPATH= cd -- "$LUDORK_TOOLCHAIN_DIRECTORY" && pwd)
        return
    fi
    for candidate in \
        "$TOOLS_DIR/Toolchain" \
        "$PROJECT_ROOT/.tools/Toolchain"
    do
        if [ -d "$candidate" ] && [ -x "$candidate/ludork-build" ]; then
            printf '%s\n' "$candidate"
            return
        fi
    done
    echo "Toolchain was not found. Run tools/init.sh first." >&2
    exit 1
}

resolve_toolchain_tool() {
    case "$1" in
        build|codegen|assets|pack|editor|check) ;;
        *) echo "Unknown toolchain role: $1" >&2; return 1 ;;
    esac
    toolchain_directory=$(resolve_toolchain_directory)
    tool="$toolchain_directory/ludork-$1"
    if [ ! -x "$tool" ]; then
        echo "Toolchain executable was not found: $tool" >&2
        return 1
    fi
    printf '%s\n' "$tool"
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

read_signing_secret() {
    signing_secret=
    if ! IFS= read -r signing_secret || [ -z "$signing_secret" ]; then
        echo "A non-empty signing password is required on standard input." >&2
        exit 1
    fi
    printf '%s\n' "$signing_secret"
}
