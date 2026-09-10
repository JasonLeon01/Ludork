#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"
if [ "$#" -ne 1 ]; then
    echo "Usage: tools/cpp_dependencies/luasf.sh <cpp-folder>" >&2
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
: "${LUASF_VERSION:?LUASF_VERSION is not set in versions.conf}"

LUASF_DIR="$CPP_DIR/Engine/ThirdParty/LuaSF"
if dependency_ready "$LUASF_DIR" "$LUASF_VERSION" "CMakeLists.txt"; then
    echo "Using existing LuaSF $LUASF_VERSION."
    exit 0
fi

TEMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/ludork-luasf.XXXXXX")
trap 'rm -rf "$TEMP_DIR"' EXIT HUP INT TERM

echo "Downloading LuaSF $LUASF_VERSION..."
mkdir -p "$TEMP_DIR/extract" "$CPP_DIR/Engine/ThirdParty"
archive="$TEMP_DIR/LuaSF-source.tar.gz"
extract_dir="$TEMP_DIR/extract"
curl -L --fail --show-error \
    "https://github.com/JasonLeon01/LuaSF-AutoGenerator/releases/download/$LUASF_VERSION/LuaSF-source.tar.gz" \
    -o "$archive"
tar -xzf "$archive" -C "$extract_dir"
if [ -f "$extract_dir/CMakeLists.txt" ]; then
    source_dir="$extract_dir"
elif [ -f "$extract_dir/LuaSF-source/CMakeLists.txt" ]; then
    source_dir="$extract_dir/LuaSF-source"
else
    echo "LuaSF source folder was not found after extraction." >&2
    exit 1
fi
rm -rf "$LUASF_DIR"
mv "$source_dir" "$LUASF_DIR"
printf '%s\n' "$LUASF_VERSION" > "$LUASF_DIR/.ludork-version"
