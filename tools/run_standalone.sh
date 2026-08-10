#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"
if [ "$#" -ne 1 ]; then
    echo "Usage: tools/run_standalone.sh <standalone-folder>" >&2
    exit 1
fi
GAME_DIR=$(absolute_path "$1")
if [ ! -x "$GAME_DIR/Main" ]; then
    echo "Standalone game executable was not found: $GAME_DIR/Main" >&2
    exit 1
fi
cd "$GAME_DIR"
exec "$GAME_DIR/Main"
