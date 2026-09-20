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
LUASF_VARIANT=${LUASF_VARIANT:-}

# The released source packages are variant-specific: the generated bindings
# follow the SFML the variant was built from, and the SFML-ME forks bind more
# than upstream SFML. Ludork tracks the same variant as its SFML pin.
LUASF_SOURCE_NAME="LuaSF-source"
LUASF_MARKER=$LUASF_VERSION
if [ -n "$LUASF_VARIANT" ]; then
    LUASF_SOURCE_NAME="LuaSF-source-$LUASF_VARIANT"
    LUASF_MARKER="$LUASF_VERSION-$LUASF_VARIANT"
fi

LUASF_DIR="$CPP_DIR/Engine/ThirdParty/LuaSF"
LUAGLUE_DIR="$CPP_DIR/Engine/ThirdParty/LuaGlue"
if [ -z "${LUASF_SOURCE_ARCHIVE:-}" ] && dependency_ready "$LUASF_DIR" "$LUASF_MARKER" "CMakeLists.txt" && dependency_ready "$LUAGLUE_DIR" "$LUASF_MARKER" "CMakeLists.txt"; then
    echo "Using existing LuaSF $LUASF_MARKER."
    exit 0
fi

TEMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/ludork-luasf.XXXXXX")
trap 'rm -rf "$TEMP_DIR"' EXIT HUP INT TERM

echo "Downloading LuaSF $LUASF_MARKER..."
mkdir -p "$TEMP_DIR/extract" "$CPP_DIR/Engine/ThirdParty"
archive="$TEMP_DIR/$LUASF_SOURCE_NAME.tar.gz"
extract_dir="$TEMP_DIR/extract"
if [ -n "${LUASF_SOURCE_ARCHIVE:-}" ]; then
    cp "$LUASF_SOURCE_ARCHIVE" "$archive"
else
    curl -L --fail --show-error \
        "https://github.com/JasonLeon01/LuaSF-AutoGenerator/releases/download/$LUASF_VERSION/$LUASF_SOURCE_NAME.tar.gz" \
        -o "$archive"
fi
tar -xzf "$archive" -C "$extract_dir"
for source_project in LuaSF LuaGlue; do
    if [ ! -f "$extract_dir/$source_project/CMakeLists.txt" ]; then
        echo "The source archive must contain LuaSF/ and LuaGlue/ projects. Set LUASF_SOURCE_ARCHIVE to a current local source archive." >&2
        exit 1
    fi
done
for source_project in LuaSF LuaGlue; do
    destination="$CPP_DIR/Engine/ThirdParty/$source_project"
    rm -rf "$destination"
    mv "$extract_dir/$source_project" "$destination"
    printf '%s\n' "$LUASF_MARKER" > "$destination/.ludork-version"
done
