#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"
if [ "$#" -ne 1 ]; then
    echo "Usage: tools/cpp_dependencies/lua.sh <cpp-folder>" >&2
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
if ! command -v tar >/dev/null 2>&1; then
    echo "tar was not found." >&2
    exit 1
fi

. "$PROJECT_ROOT/versions.conf"
: "${LUA_VERSION:?LUA_VERSION is not set in versions.conf}"
: "${LUA_SHA256:?LUA_SHA256 is not set in versions.conf}"

LUA_DIR="$CPP_DIR/Engine/ThirdParty/Lua"
if dependency_ready "$LUA_DIR" "$LUA_VERSION" "src/lua.h"; then
    echo "Using existing Lua $LUA_VERSION."
    exit 0
fi

TEMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/ludork-lua.XXXXXX")
trap 'rm -rf "$TEMP_DIR"' EXIT HUP INT TERM

echo "Downloading Lua $LUA_VERSION..."
mkdir -p "$CPP_DIR/Engine/ThirdParty"
archive="$TEMP_DIR/lua.tar.gz"
curl -L --fail --show-error \
    "https://www.lua.org/ftp/lua-$LUA_VERSION.tar.gz" \
    -o "$archive"

echo "Verifying Lua SHA-256..."
if command -v sha256sum >/dev/null 2>&1; then
    printf '%s  %s\n' "$LUA_SHA256" "$archive" | sha256sum --check -
elif command -v shasum >/dev/null 2>&1; then
    actual_sha256=$(shasum -a 256 "$archive" | awk '{print $1}')
    if [ "$actual_sha256" != "$LUA_SHA256" ]; then
        echo "SHA-256 mismatch: $actual_sha256" >&2
        exit 1
    fi
else
    echo "Missing sha256sum or shasum; cannot verify $archive" >&2
    exit 1
fi

tar -xzf "$archive" -C "$TEMP_DIR"
source_dir="$TEMP_DIR/lua-$LUA_VERSION"
if [ ! -f "$source_dir/src/lua.h" ]; then
    echo "Lua source folder was not found after extraction." >&2
    exit 1
fi
rm -rf "$LUA_DIR"
mv "$source_dir" "$LUA_DIR"
printf '%s\n' "$LUA_VERSION" > "$LUA_DIR/.ludork-version"
