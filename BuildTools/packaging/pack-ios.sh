#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
. "$SCRIPT_DIR/_lib.sh"

PROJ_PATH=""
while [ $# -gt 0 ]; do
    case "$1" in
        --proj-path)
            PROJ_PATH="$2"
            shift 2
            ;;
        *)
            err "Unknown argument: $1"
            ;;
    esac
done

[ -n "$PROJ_PATH" ] || err "Missing --proj-path"
[ -f "$PROJ_PATH/Entry.py" ] || err "Entry.py not found in $PROJ_PATH"

PROJ_PATH="$(cd "$PROJ_PATH" && pwd -P)"
PROJECT_NAME="$(basename "$PROJ_PATH")"
SCRIPTS_DIR="$PROJ_PATH"
IOS_PYTHON_DIR="$ROOT/ios_python"
RESOURCE_DIR="$PROJ_PATH/Assets/System"
GENERATE_SCRIPT="$ROOT/BuildTools/generateiOSApp.sh"

[ -f "$GENERATE_SCRIPT" ] || err "generateiOSApp.sh not found: $GENERATE_SCRIPT"

cmd=(bash "$GENERATE_SCRIPT" "$PROJECT_NAME" -g "$PROJ_PATH" "$SCRIPTS_DIR" "$IOS_PYTHON_DIR")
if [ -d "$RESOURCE_DIR" ]; then
    cmd+=(-r "$RESOURCE_DIR")
fi

step "Generating iOS Xcode project..."
log "Running: ${cmd[*]}"
exec "${cmd[@]}"
