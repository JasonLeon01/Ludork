#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"
USE_LUAC=0
ENCRYPT_SHADERS=0
ENCRYPT_DATA=0
while [ "$#" -gt 0 ]; do
    case "$1" in
        --compile-lua)
            USE_LUAC=1
            shift
            ;;
        --encrypt-shaders)
            ENCRYPT_SHADERS=1
            shift
            ;;
        --encrypt-data)
            ENCRYPT_DATA=1
            shift
            ;;
        --*)
            echo "Unknown option: $1" >&2
            exit 1
            ;;
        *)
            break
            ;;
    esac
done
if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    echo "Usage: tools/pack_project.sh [--compile-lua] [--encrypt-shaders] [--encrypt-data] <project-folder> [dist-folder]" >&2
    exit 1
fi
if [ "$(uname -s)" != "Darwin" ]; then
    echo "pack_project.sh currently supports macOS only." >&2
    exit 1
fi

sh "$PROJECT_ROOT/tools/build_script_tools.sh"
SCRIPT_TOOLS=$(resolve_script_tools)
PROJECT_DIR=$(absolute_path "$1")
DEFAULT_DIST="$PROJECT_DIR/dist"
DIST_DIR=$(absolute_path "${2:-$DEFAULT_DIST}")
PROJECT_FILE="$PROJECT_DIR/Main.proj"
if [ ! -f "$PROJECT_FILE" ]; then
    echo "Main.proj was not found: $PROJECT_FILE" >&2
    exit 1
fi
ENTRY_FILE="$PROJECT_DIR/Scripts/Entry.lua"
if [ ! -f "$ENTRY_FILE" ]; then
    echo "Lua entry script was not found: $ENTRY_FILE" >&2
    exit 1
fi
DEFAULT_APP_NAME_PATTERN="^[[:space:]]*local[[:space:]]+APP_NAME[[:space:]]*=[[:space:]]*['\"]LudorkSample['\"][[:space:]]*(--.*)?$"
if grep -Eq "$DEFAULT_APP_NAME_PATTERN" "$ENTRY_FILE"; then
    echo "Change APP_NAME in Scripts/Entry.lua from LudorkSample to a name unique to your game before packaging." >&2
    exit 24
fi

PROJECT_MODE=$("$SCRIPT_TOOLS" project-runtime-mode "$PROJECT_FILE")
TEMPORARY_DIR=$(mktemp -d "${TMPDIR:-/tmp}/ludork-pack.XXXXXX")

cleanup_temporary() {
    rm -rf "$TEMPORARY_DIR"
}

remove_ui_preview_host_entries() {
    package_dir=$1
    find "$package_dir" -depth \
        \( -name 'UiPreviewHost*' -o -name 'UiPreviewCurveResolver*' \) \
        -exec rm -rf {} +
}

validate_no_ui_preview_host() {
    package_dir=$1
    forbidden_path=$(find "$package_dir" \
        \( -name 'UiPreviewHost*' -o -name 'UiPreviewCurveResolver*' \) \
        -print -quit)
    if [ -n "$forbidden_path" ]; then
        echo "UI preview host entry was found in a game package: $forbidden_path" >&2
        exit 1
    fi
}

trap cleanup_temporary EXIT HUP INT TERM

if [ "$PROJECT_MODE" = "standalone" ]; then
    RUNTIME_DIR="$PROJECT_DIR"
else
    if [ ! -f "$PROJECT_DIR/CMakeLists.txt" ]; then
        echo "CMakeLists.txt was not found: $PROJECT_DIR/CMakeLists.txt" >&2
        exit 1
    fi
    sh "$TOOLS_DIR/build_standalone.sh" "$PROJECT_DIR" "$TEMPORARY_DIR/runtime" Release
    RUNTIME_DIR="$TEMPORARY_DIR/runtime"
fi

rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR"
"$SCRIPT_TOOLS" macos-bundle \
    "$PROJECT_DIR" "$RUNTIME_DIR" "$DIST_DIR/Main.app"
remove_ui_preview_host_entries "$DIST_DIR/Main.app"
if [ "$ENCRYPT_SHADERS" -eq 1 ] && [ "$ENCRYPT_DATA" -eq 1 ]; then
    "$SCRIPT_TOOLS" finalize-package --encrypt-shaders --encrypt-data \
        "$DIST_DIR/Main.app/Contents/Resources"
elif [ "$ENCRYPT_SHADERS" -eq 1 ]; then
    "$SCRIPT_TOOLS" finalize-package --encrypt-shaders \
        "$DIST_DIR/Main.app/Contents/Resources"
elif [ "$ENCRYPT_DATA" -eq 1 ]; then
    "$SCRIPT_TOOLS" finalize-package --encrypt-data \
        "$DIST_DIR/Main.app/Contents/Resources"
else
    "$SCRIPT_TOOLS" finalize-package \
        "$DIST_DIR/Main.app/Contents/Resources"
fi
if [ "$USE_LUAC" -eq 1 ]; then
    "$SCRIPT_TOOLS" compile-lua "$DIST_DIR/Main.app/Contents/Resources/Scripts"
fi
validate_no_ui_preview_host "$DIST_DIR/Main.app"
plutil -lint "$DIST_DIR/Main.app/Contents/Info.plist"
echo "Pack complete: $DIST_DIR/Main.app"
