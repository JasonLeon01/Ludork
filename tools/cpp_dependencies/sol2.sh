#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"
if [ "$#" -ne 1 ]; then
    echo "Usage: tools/cpp_dependencies/sol2.sh <cpp-folder>" >&2
    exit 1
fi

CPP_DIR=$(absolute_path "$1")
if [ ! -f "$CPP_DIR/CMakeLists.txt" ]; then
    echo "CMakeLists.txt was not found: $CPP_DIR" >&2
    exit 1
fi
if ! command -v curl >/dev/null 2>&1; then
    echo "curl was not found." >&2
    exit 1
fi

. "$PROJECT_ROOT/versions.conf"
: "${SOL2_VERSION:?SOL2_VERSION is not set in versions.conf}"

SOL2_DIR="$CPP_DIR/Engine/ThirdParty/sol2"
if dependency_ready "$SOL2_DIR" "$SOL2_VERSION" "include/sol2/sol.hpp"; then
    echo "Using existing sol2 $SOL2_VERSION."
    exit 0
fi

TEMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/ludork-sol2.XXXXXX")
trap 'rm -rf "$TEMP_DIR"' EXIT HUP INT TERM

echo "Downloading sol2 $SOL2_VERSION headers..."
mkdir -p "$TEMP_DIR/sol2/include/sol2" "$CPP_DIR/Engine/ThirdParty"
for file in config.hpp forward.hpp sol.hpp; do
    curl -L --fail --show-error \
        "https://github.com/ThePhD/sol2/releases/download/v$SOL2_VERSION/$file" \
        -o "$TEMP_DIR/sol2/include/sol2/$file"
done
rm -rf "$SOL2_DIR"
mv "$TEMP_DIR/sol2" "$SOL2_DIR"
printf '%s\n' "$SOL2_VERSION" > "$SOL2_DIR/.ludork-version"
