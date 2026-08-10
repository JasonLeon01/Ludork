#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"
cd "$PROJECT_ROOT"
exec dotnet run --project "$PROJECT_ROOT/Ludork.csproj" -- "$@"
