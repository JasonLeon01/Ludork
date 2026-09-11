#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"
if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    echo "Usage: tools/run_cpp.sh <cpp-folder> [Debug|Release]" >&2
    exit 1
fi
CPP_DIR=$(absolute_path "$1")
CONFIG=${2:-Debug}
GAME="$CPP_DIR/bin/$CONFIG/Main"
if [ ! -x "$GAME" ]; then
    echo "No C++ game executable was found: $GAME" >&2
    exit 1
fi
cd "$CPP_DIR"
exec "$GAME"
