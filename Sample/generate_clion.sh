#!/usr/bin/env sh
set -eu

PROJECT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ ! -f "$PROJECT_DIR/CMakeLists.txt" ]; then
    echo "CMakeLists.txt was not found: $PROJECT_DIR" >&2
    exit 1
fi
if [ ! -f "$PROJECT_DIR/Main.proj" ]; then
    echo "Main.proj was not found: $PROJECT_DIR" >&2
    exit 1
fi
if [ "$(uname -s)" != "Darwin" ] || [ "$(uname -m)" != "arm64" ]; then
    echo "This CLion configuration tool supports Apple Silicon macOS only." >&2
    exit 1
fi

SCRIPT_TOOLS=
CMAKE_EXE=
NINJA_EXE=
use_tools_dir() {
    candidate_tools_dir=$1
    if [ -x "$candidate_tools_dir/ScriptTools" ]; then
        SCRIPT_TOOLS="$candidate_tools_dir/ScriptTools"
    fi
}

use_clion_app() {
    candidate_clion_app=$1
    if [ ! -d "$candidate_clion_app" ]; then
        return
    fi
    if [ -z "$CMAKE_EXE" ]; then
        CMAKE_EXE=$(find "$candidate_clion_app/Contents/bin" -type f -name cmake -perm -111 2>/dev/null | head -n 1)
    fi
    if [ -z "$NINJA_EXE" ]; then
        NINJA_EXE=$(find "$candidate_clion_app/Contents/bin" -type f -name ninja -perm -111 2>/dev/null | head -n 1)
    fi
}

if [ -n "${LUDORK_TOOLS_DIR:-}" ]; then
    use_tools_dir "$LUDORK_TOOLS_DIR"
    if [ -z "$SCRIPT_TOOLS" ]; then
        echo "LUDORK_TOOLS_DIR does not contain an executable ScriptTools: $LUDORK_TOOLS_DIR" >&2
        exit 1
    fi
else
    DEVELOPMENT_ROOT=$(CDPATH= cd -- "$PROJECT_DIR/.." && pwd)
    if [ -x "$DEVELOPMENT_ROOT/.tools/ScriptTools/ScriptTools" ]; then
        SCRIPT_TOOLS="$DEVELOPMENT_ROOT/.tools/ScriptTools/ScriptTools"
    fi
    if [ -z "$SCRIPT_TOOLS" ]; then
        use_tools_dir "/Applications/Ludork.app/Contents/Resources/tools"
    fi
    if [ -z "$SCRIPT_TOOLS" ]; then
        use_tools_dir "$HOME/Applications/Ludork.app/Contents/Resources/tools"
    fi
    if [ -z "$SCRIPT_TOOLS" ] && command -v mdfind >/dev/null 2>&1; then
        LUDORK_APP=$(mdfind 'kMDItemCFBundleIdentifier == "com.ludork.editor"' | head -n 1)
        if [ -n "$LUDORK_APP" ]; then
            use_tools_dir "$LUDORK_APP/Contents/Resources/tools"
        fi
    fi
fi

if [ -z "$SCRIPT_TOOLS" ]; then
    echo "Ludork ScriptTools was not found." >&2
    echo "Set LUDORK_TOOLS_DIR to the installed Ludork tools directory." >&2
    exit 1
fi

CMAKE_EXE=$(command -v cmake || true)
NINJA_EXE=$(command -v ninja || true)
if [ -z "$CMAKE_EXE" ] && [ -x "/Applications/CMake.app/Contents/bin/cmake" ]; then
    CMAKE_EXE="/Applications/CMake.app/Contents/bin/cmake"
fi
use_clion_app "/Applications/CLion.app"
use_clion_app "$HOME/Applications/CLion.app"
if { [ -z "$CMAKE_EXE" ] || [ -z "$NINJA_EXE" ]; } && command -v mdfind >/dev/null 2>&1; then
    CLION_APP=$(mdfind 'kMDItemCFBundleIdentifier == "com.jetbrains.CLion"' | head -n 1)
    if [ -n "$CLION_APP" ]; then
        use_clion_app "$CLION_APP"
    fi
fi
if [ -z "$CMAKE_EXE" ]; then
    echo "CMake was not found. Install CMake or CLion, or add CMake to PATH." >&2
    exit 1
fi
if [ -z "$NINJA_EXE" ]; then
    echo "Ninja was not found. Install Ninja or CLion, or add Ninja to PATH." >&2
    exit 1
fi

"$SCRIPT_TOOLS" ide-config clion "$PROJECT_DIR" \
    --platform macos \
    --script-tools "$SCRIPT_TOOLS"

(
    cd "$PROJECT_DIR"
    PATH="$(dirname "$NINJA_EXE"):$PATH" \
        "$CMAKE_EXE" --preset ludork-clion-debug
)

if [ ! -f "$PROJECT_DIR/cmake-build-ludork-debug/CMakeCache.txt" ]; then
    echo "CLion project was not configured: $PROJECT_DIR/cmake-build-ludork-debug" >&2
    exit 1
fi

echo "CLion project is configured and ready. Open this directory in CLion: $PROJECT_DIR"
