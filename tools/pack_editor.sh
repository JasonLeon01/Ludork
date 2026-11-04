#!/usr/bin/env sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
EDITOR_TOOL="$ROOT_DIR/.tools/Toolchain/ludork-editor"
if [ ! -x "$EDITOR_TOOL" ]; then
    echo "Toolchain was not found. Run tools/init.sh first." >&2
    exit 1
fi
exec "$EDITOR_TOOL" package --repository-root "$ROOT_DIR" "$@"
