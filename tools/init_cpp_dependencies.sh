#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"
TOOL=$(resolve_toolchain_tool build)
exec "$TOOL" dependencies "$@" --repository-root "$PROJECT_ROOT"
