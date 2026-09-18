#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"
if [ "$#" -ne 1 ]; then
    echo "Usage: tools/cpp_dependencies/sfml.sh <cpp-folder>" >&2
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
: "${SFML_REPOSITORY:?SFML_REPOSITORY is not set in versions.conf}"
: "${SFML_TAG:?SFML_TAG is not set in versions.conf}"

SFML_DIR="$CPP_DIR/Engine/ThirdParty/SFML"
if dependency_ready "$SFML_DIR" "$SFML_TAG" "CMakeLists.txt"; then
    echo "Using existing SFML $SFML_TAG."
    exit 0
fi

TEMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/ludork-sfml.XXXXXX")
trap 'rm -rf "$TEMP_DIR"' EXIT HUP INT TERM

echo "Downloading SFML $SFML_TAG from $SFML_REPOSITORY..."
mkdir -p "$CPP_DIR/Engine/ThirdParty"
archive="$TEMP_DIR/sfml.tar.gz"
curl -L --fail --show-error \
    "https://github.com/$SFML_REPOSITORY/archive/refs/tags/$SFML_TAG.tar.gz" \
    -o "$archive"
tar -xzf "$archive" -C "$TEMP_DIR"
source_dir="$TEMP_DIR/${SFML_REPOSITORY##*/}-$SFML_TAG"
if [ ! -f "$source_dir/CMakeLists.txt" ]; then
    echo "SFML source folder was not found after extraction." >&2
    exit 1
fi
rm -rf "$SFML_DIR"
mv "$source_dir" "$SFML_DIR"
printf '%s\n' "$SFML_TAG" > "$SFML_DIR/.ludork-version"
