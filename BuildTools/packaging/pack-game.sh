#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
. "$SCRIPT_DIR/_lib.sh"

PROJ_PATH=""
DIST_PATH=""
PYTHON_EXE="$PYTHON"
INCLUDE_PYAV=""
PLATFORM=""
ENTRY_NAME="Entry"

while [ $# -gt 0 ]; do
    case "$1" in
        --proj-path)
            PROJ_PATH="$2"
            shift 2
            ;;
        --dist-path)
            DIST_PATH="$2"
            shift 2
            ;;
        --python)
            PYTHON_EXE="$2"
            shift 2
            ;;
        --platform)
            PLATFORM="$2"
            shift 2
            ;;
        --include-pyav)
            INCLUDE_PYAV="1"
            shift
            ;;
        *)
            err "Unknown argument: $1"
            ;;
    esac
done

[ -n "$PROJ_PATH" ] || err "Missing --proj-path"
[ -n "$DIST_PATH" ] || err "Missing --dist-path"
[ -n "$PYTHON_EXE" ] || err "Missing --python and no LudorkEnv Python was found"
[ -f "$PROJ_PATH/Entry.py" ] || err "Entry.py not found in $PROJ_PATH"

PROJ_PATH="$(cd "$PROJ_PATH" && pwd)"
mkdir -p "$DIST_PATH"
DIST_PATH="$(cd "$DIST_PATH" && pwd)"

if [ -z "$PLATFORM" ]; then
    case "$("$PYTHON_EXE" -c 'import sys; print(sys.platform)')" in
        win32) PLATFORM="win32" ;;
        darwin) PLATFORM="macos_arm" ;;
        *) err "Unsupported platform. Pass --platform win32 or macos_arm." ;;
    esac
fi

case "$PLATFORM" in
    win32|macos_arm) ;;
    *) err "Unsupported platform: $PLATFORM" ;;
esac

step "Cleaning previous Nuitka artifacts..."
for suffix in .build .dist .onefile-build; do
    rm -rf "$DIST_PATH/$ENTRY_NAME$suffix"
done
rm -f "$PROJ_PATH/nuitka-crash-report.xml"

pip_ensure_with_exe "$PYTHON_EXE" nuitka "-U"
if [ -n "$INCLUDE_PYAV" ]; then
    pip_ensure_with_exe "$PYTHON_EXE" av "-U"
fi

NUITKA_FLAGS=(
    --remove-output
    --assume-yes-for-downloads
    "--output-dir=$DIST_PATH"
    --output-filename=Main
    --include-data-dir=Assets=Assets
    --include-data-dir=Data=Data
    --include-package=pysf
)

[ -f "$PROJ_PATH/Main.ini" ] && NUITKA_FLAGS+=(--include-data-file=Main.ini=Main.ini)
[ -n "$INCLUDE_PYAV" ] && NUITKA_FLAGS+=(--include-module=av)

if [ "$PLATFORM" = "win32" ]; then
    NUITKA_FLAGS+=(--standalone --windows-console-mode=disable)
    [ -f "$PROJ_PATH/Assets/System/icon.ico" ] && NUITKA_FLAGS+=("--windows-icon-from-ico=$PROJ_PATH/Assets/System/icon.ico")
else
    NUITKA_FLAGS+=(--mode=app --macos-app-name=Main)
    [ -f "$PROJ_PATH/Assets/System/icon.icns" ] && NUITKA_FLAGS+=("--macos-app-icon=$PROJ_PATH/Assets/System/icon.icns")
fi

step "Running Nuitka build for game..."
printf 'Running Nuitka:'
printf ' %q' "$PYTHON_EXE" -u -m nuitka "${NUITKA_FLAGS[@]}" "$ENTRY_NAME.py"
printf '\n'
(cd "$PROJ_PATH" && "$PYTHON_EXE" -u -m nuitka "${NUITKA_FLAGS[@]}" "$ENTRY_NAME.py")

if [ "$PLATFORM" = "win32" ]; then
    log "Game packaging complete. Output: $DIST_PATH/$ENTRY_NAME.dist"
else
    log "Game packaging complete. Output: $DIST_PATH/Main.app"
fi
