#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"
if [ "$#" -ne 1 ]; then
    echo "Usage: tools/init_cpp_dependencies.sh <cpp-folder>" >&2
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
: "${LUASF_VERSION:?LUASF_VERSION is not set in versions.conf}"
: "${LUA_CJSON_VERSION:?LUA_CJSON_VERSION is not set in versions.conf}"
: "${ZLIB_VERSION:?ZLIB_VERSION is not set in versions.conf}"

TEMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/ludork-cpp-dependencies.XXXXXX")

cleanup_temp() {
    rm -rf "$TEMP_DIR"
}

trap cleanup_temp EXIT HUP INT TERM

dependency_ready() {
    dependency_dir=$1
    version=$2
    required_file=$3
    [ -f "$dependency_dir/.ludork-version" ] &&
        [ "$(cat "$dependency_dir/.ludork-version")" = "$version" ] &&
        [ -f "$dependency_dir/$required_file" ]
}

mkdir -p "$CPP_DIR/Engine/ThirdParty"

LUASF_DIR="$CPP_DIR/Engine/ThirdParty/LuaSF"
if dependency_ready "$LUASF_DIR" "$LUASF_VERSION" "CMakeLists.txt"; then
    echo "Using existing LuaSF $LUASF_VERSION."
else
    echo "Downloading LuaSF $LUASF_VERSION..."
    LUASF_ARCHIVE="$TEMP_DIR/LuaSF-source.tar.gz"
    LUASF_EXTRACT_DIR="$TEMP_DIR/LuaSF"
    curl -L --fail --show-error \
        "https://github.com/JasonLeon01/LuaSF-AutoGenerator/releases/download/$LUASF_VERSION/LuaSF-source.tar.gz" \
        -o "$LUASF_ARCHIVE"
    mkdir -p "$LUASF_EXTRACT_DIR"
    tar -xzf "$LUASF_ARCHIVE" -C "$LUASF_EXTRACT_DIR"
    if [ -f "$LUASF_EXTRACT_DIR/CMakeLists.txt" ]; then
        LUASF_SOURCE_DIR="$LUASF_EXTRACT_DIR"
    elif [ -f "$LUASF_EXTRACT_DIR/LuaSF-source/CMakeLists.txt" ]; then
        LUASF_SOURCE_DIR="$LUASF_EXTRACT_DIR/LuaSF-source"
    else
        echo "LuaSF source folder was not found after extraction." >&2
        exit 1
    fi
    rm -rf "$LUASF_DIR"
    mv "$LUASF_SOURCE_DIR" "$LUASF_DIR"
    printf '%s\n' "$LUASF_VERSION" > "$LUASF_DIR/.ludork-version"
fi

LUASF_GIT_ROOT=$(git -C "$LUASF_DIR" rev-parse --show-toplevel 2>/dev/null || true)
LUASF_GIT_DIRECTORY=
if [ -n "$LUASF_GIT_ROOT" ]; then
    case "$LUASF_DIR" in
        "$LUASF_GIT_ROOT"/*)
            LUASF_GIT_DIRECTORY=${LUASF_DIR#"$LUASF_GIT_ROOT"/}
            ;;
    esac
fi
apply_luasf_patch() {
    if [ -n "$LUASF_GIT_DIRECTORY" ]; then
        git -C "$LUASF_GIT_ROOT" apply --unidiff-zero \
            --directory="$LUASF_GIT_DIRECTORY" "$@"
    else
        (cd "$LUASF_DIR" && git apply --unidiff-zero "$@")
    fi
}

VALUE_COPY_PATCH="$PROJECT_ROOT/patches/luasf-value-copy.patch"
echo "Applying LuaSF native value copy patch if needed..."
if apply_luasf_patch --reverse --check "$VALUE_COPY_PATCH" >/dev/null 2>&1; then
    echo "LuaSF native value copy patch is already applied."
else
    apply_luasf_patch --check "$VALUE_COPY_PATCH"
    apply_luasf_patch "$VALUE_COPY_PATCH"
fi

LUA_CJSON_DIR="$CPP_DIR/Engine/ThirdParty/lua-cjson"
if dependency_ready "$LUA_CJSON_DIR" "$LUA_CJSON_VERSION" "lua_cjson.c"; then
    echo "Using existing lua-cjson $LUA_CJSON_VERSION."
else
    echo "Downloading lua-cjson $LUA_CJSON_VERSION..."
    LUA_CJSON_ARCHIVE="$TEMP_DIR/lua-cjson.zip"
    curl -L --fail --show-error \
        "https://github.com/openresty/lua-cjson/archive/refs/tags/$LUA_CJSON_VERSION.zip" \
        -o "$LUA_CJSON_ARCHIVE"
    unzip -q "$LUA_CJSON_ARCHIVE" -d "$TEMP_DIR"
    LUA_CJSON_SOURCE_DIR="$TEMP_DIR/lua-cjson-$LUA_CJSON_VERSION"
    if [ ! -f "$LUA_CJSON_SOURCE_DIR/lua_cjson.c" ]; then
        echo "lua-cjson source folder was not found after extraction." >&2
        exit 1
    fi
    rm -rf "$LUA_CJSON_DIR"
    mv "$LUA_CJSON_SOURCE_DIR" "$LUA_CJSON_DIR"
    printf '%s\n' "$LUA_CJSON_VERSION" > "$LUA_CJSON_DIR/.ludork-version"
fi

ZLIB_DIR="$CPP_DIR/Engine/ThirdParty/zlib"
if dependency_ready "$ZLIB_DIR" "$ZLIB_VERSION" "CMakeLists.txt"; then
    echo "Using existing zlib $ZLIB_VERSION."
else
    echo "Downloading zlib $ZLIB_VERSION..."
    ZLIB_ARCHIVE="$TEMP_DIR/zlib.zip"
    curl -L --fail --show-error \
        "https://github.com/madler/zlib/archive/refs/tags/v$ZLIB_VERSION.zip" \
        -o "$ZLIB_ARCHIVE"
    unzip -q "$ZLIB_ARCHIVE" -d "$TEMP_DIR"
    ZLIB_SOURCE_DIR="$TEMP_DIR/zlib-$ZLIB_VERSION"
    if [ ! -f "$ZLIB_SOURCE_DIR/CMakeLists.txt" ]; then
        echo "zlib source folder was not found after extraction." >&2
        exit 1
    fi
    rm -rf "$ZLIB_DIR"
    mv "$ZLIB_SOURCE_DIR" "$ZLIB_DIR"
    printf '%s\n' "$ZLIB_VERSION" > "$ZLIB_DIR/.ludork-version"
fi

sh "$TOOLS_DIR/init_ffmpeg_source.sh" "$CPP_DIR"
echo "C++ dependencies are ready in $CPP_DIR"
