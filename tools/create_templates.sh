#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"
exec "$(resolve_toolchain_tool pack)" create-templates --repository-root "$PROJECT_ROOT" "$@"
