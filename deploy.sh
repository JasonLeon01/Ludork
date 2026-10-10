#!/usr/bin/env sh
set -eu
cd -- "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
if ! command -v node >/dev/null 2>&1; then
  printf '%s\n' 'Node.js 24 and npm are required.' >&2
  exit 1
fi
exec node tools/deploy.mjs "$@"
