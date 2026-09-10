#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"
if [ "$#" -ne 1 ]; then
    echo "Usage: tools/cpp_dependencies/zlib.sh <cpp-folder>" >&2
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
if ! command -v unzip >/dev/null 2>&1; then
    echo "unzip was not found." >&2
    exit 1
fi

. "$PROJECT_ROOT/versions.conf"
: "${ZLIB_VERSION:?ZLIB_VERSION is not set in versions.conf}"

ZLIB_DIR="$CPP_DIR/Engine/ThirdParty/zlib"
if dependency_ready "$ZLIB_DIR" "$ZLIB_VERSION" "CMakeLists.txt"; then
    echo "Using existing zlib $ZLIB_VERSION."
    exit 0
fi

TEMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/ludork-zlib.XXXXXX")
trap 'rm -rf "$TEMP_DIR"' EXIT HUP INT TERM

echo "Downloading zlib $ZLIB_VERSION..."
mkdir -p "$CPP_DIR/Engine/ThirdParty"
archive="$TEMP_DIR/zlib.zip"
curl -L --fail --show-error \
    "https://github.com/madler/zlib/archive/refs/tags/v$ZLIB_VERSION.zip" \
    -o "$archive"
unzip -q "$archive" -d "$TEMP_DIR"
source_dir="$TEMP_DIR/zlib-$ZLIB_VERSION"
if [ ! -f "$source_dir/CMakeLists.txt" ]; then
    echo "zlib source folder was not found after extraction." >&2
    exit 1
fi
rm -rf "$ZLIB_DIR"
mv "$source_dir" "$ZLIB_DIR"
printf '%s\n' "$ZLIB_VERSION" > "$ZLIB_DIR/.ludork-version"
