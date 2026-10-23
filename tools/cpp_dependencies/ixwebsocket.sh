#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"
if [ "$#" -ne 1 ]; then
    echo "Usage: tools/cpp_dependencies/ixwebsocket.sh <cpp-folder>" >&2
    exit 1
fi

CPP_DIR=$(absolute_path "$1")
if [ ! -f "$CPP_DIR/CMakeLists.txt" ]; then
    echo "CMakeLists.txt was not found: $CPP_DIR" >&2
    exit 1
fi
for tool in curl tar; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "$tool was not found." >&2
        exit 1
    fi
done

. "$PROJECT_ROOT/versions.conf"
: "${IXWEBSOCKET_VERSION:?IXWEBSOCKET_VERSION is not set in versions.conf}"
: "${IXWEBSOCKET_COMMIT:?IXWEBSOCKET_COMMIT is not set in versions.conf}"

IXWEBSOCKET_DIR="$CPP_DIR/Engine/ThirdParty/IXWebSocket"
marker="$IXWEBSOCKET_VERSION-$IXWEBSOCKET_COMMIT"
if dependency_ready "$IXWEBSOCKET_DIR" "$marker" "CMakeLists.txt"; then
    echo "Using existing IXWebSocket $IXWEBSOCKET_VERSION."
    exit 0
fi

TEMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/ludork-ixwebsocket.XXXXXX")
trap 'rm -rf "$TEMP_DIR"' EXIT HUP INT TERM

echo "Downloading IXWebSocket $IXWEBSOCKET_VERSION ($IXWEBSOCKET_COMMIT)..."
mkdir -p "$CPP_DIR/Engine/ThirdParty"
archive="$TEMP_DIR/ixwebsocket.tar.gz"
download_dependency \
    "https://github.com/machinezone/IXWebSocket/archive/$IXWEBSOCKET_COMMIT.tar.gz" \
    "$archive"
tar -xzf "$archive" -C "$TEMP_DIR"
source_dir="$TEMP_DIR/IXWebSocket-$IXWEBSOCKET_COMMIT"
if [ ! -f "$source_dir/CMakeLists.txt" ] || [ ! -f "$source_dir/ixwebsocket/IXWebSocket.h" ]; then
    echo "IXWebSocket source folder was not found after extraction." >&2
    exit 1
fi
rm -rf "$IXWEBSOCKET_DIR"
mv "$source_dir" "$IXWEBSOCKET_DIR"
printf '%s\n' "$marker" > "$IXWEBSOCKET_DIR/.ludork-version"
