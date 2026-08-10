#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"
if [ "$#" -ne 1 ]; then
    echo "Usage: tools/init_ffmpeg_source.sh <cpp-folder>" >&2
    exit 1
fi

CPP_DIR=$(absolute_path "$1")
. "$PROJECT_ROOT/versions.conf"
: "${FFMPEG_VERSION:?FFMPEG_VERSION is not set in versions.conf}"

SOURCE_DIR="$CPP_DIR/ffmpeg"
EXTRACTED_DIR="$CPP_DIR/ffmpeg-$FFMPEG_VERSION"
SOURCE_ARCHIVE_DIR="$CPP_DIR/ThirdPartySource"
SOURCE_ARCHIVE="$SOURCE_ARCHIVE_DIR/ffmpeg-$FFMPEG_VERSION.tar.xz"
case "$SOURCE_DIR" in
    "$CPP_DIR"/ffmpeg) ;;
    *) echo "Invalid FFmpeg source target: $SOURCE_DIR" >&2; exit 1 ;;
esac

mkdir -p "$SOURCE_ARCHIVE_DIR"
if [ ! -f "$SOURCE_ARCHIVE" ]; then
    echo "Downloading FFmpeg $FFMPEG_VERSION..."
    curl -L --fail --show-error \
        "https://ffmpeg.org/releases/ffmpeg-$FFMPEG_VERSION.tar.xz" \
        -o "$SOURCE_ARCHIVE"
fi
if [ ! -f "$SOURCE_ARCHIVE" ]; then
    echo "FFmpeg source archive was not found." >&2
    exit 1
fi

rm -rf "$SOURCE_DIR" "$EXTRACTED_DIR"
tar -xf "$SOURCE_ARCHIVE" -C "$CPP_DIR"
if [ ! -f "$EXTRACTED_DIR/configure" ]; then
    echo "FFmpeg source folder was not found after extraction." >&2
    exit 1
fi
mv "$EXTRACTED_DIR" "$SOURCE_DIR"
echo "FFmpeg source is ready: $SOURCE_DIR"
