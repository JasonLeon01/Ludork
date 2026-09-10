#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"
if [ "$#" -ne 1 ]; then
    echo "Usage: tools/cpp_dependencies/lua_cjson.sh <cpp-folder>" >&2
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
: "${LUA_CJSON_VERSION:?LUA_CJSON_VERSION is not set in versions.conf}"

LUA_CJSON_DIR="$CPP_DIR/Engine/ThirdParty/lua-cjson"
if dependency_ready "$LUA_CJSON_DIR" "$LUA_CJSON_VERSION" "lua_cjson.c"; then
    echo "Using existing lua-cjson $LUA_CJSON_VERSION."
    exit 0
fi

TEMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/ludork-lua-cjson.XXXXXX")
trap 'rm -rf "$TEMP_DIR"' EXIT HUP INT TERM

echo "Downloading lua-cjson $LUA_CJSON_VERSION..."
mkdir -p "$CPP_DIR/Engine/ThirdParty"
archive="$TEMP_DIR/lua-cjson.zip"
curl -L --fail --show-error \
    "https://github.com/openresty/lua-cjson/archive/refs/tags/$LUA_CJSON_VERSION.zip" \
    -o "$archive"
unzip -q "$archive" -d "$TEMP_DIR"
source_dir="$TEMP_DIR/lua-cjson-$LUA_CJSON_VERSION"
if [ ! -f "$source_dir/lua_cjson.c" ]; then
    echo "lua-cjson source folder was not found after extraction." >&2
    exit 1
fi
rm -rf "$LUA_CJSON_DIR"
mv "$source_dir" "$LUA_CJSON_DIR"
printf '%s\n' "$LUA_CJSON_VERSION" > "$LUA_CJSON_DIR/.ludork-version"
