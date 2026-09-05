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
: "${FFMPEG_COMMIT:?FFMPEG_COMMIT is not set in versions.conf}"

SOURCE_DIR="$CPP_DIR/Engine/ThirdParty/ffmpeg"
EXTRACTED_DIR="$CPP_DIR/Engine/ThirdParty/FFmpeg-$FFMPEG_COMMIT"
SOURCE_ARCHIVE_DIR="$CPP_DIR/ThirdPartySource"
SOURCE_ARCHIVE="$SOURCE_ARCHIVE_DIR/ffmpeg-$FFMPEG_VERSION.tar.gz"
SOURCE_PARTIAL="$SOURCE_ARCHIVE.part"
case "$SOURCE_DIR" in
    "$CPP_DIR"/Engine/ThirdParty/ffmpeg) ;;
    *) echo "Invalid FFmpeg source target: $SOURCE_DIR" >&2; exit 1 ;;
esac

mkdir -p "$SOURCE_ARCHIVE_DIR" "$CPP_DIR/Engine/ThirdParty"
if [ -f "$SOURCE_ARCHIVE" ]; then
    echo "Using existing FFmpeg $FFMPEG_VERSION source archive."
fi
if [ ! -f "$SOURCE_ARCHIVE" ]; then
    if ! command -v curl >/dev/null 2>&1; then
        echo "curl was not found." >&2
        exit 1
    fi
    rm -f "$SOURCE_PARTIAL"
    echo "Downloading FFmpeg $FFMPEG_VERSION..."
    if curl \
        --location \
        --fail \
        --show-error \
        --retry 5 \
        --retry-all-errors \
        --retry-delay 5 \
        --retry-max-time 900 \
        --connect-timeout 15 \
        --max-time 300 \
        "https://github.com/FFmpeg/FFmpeg/archive/$FFMPEG_COMMIT.tar.gz" \
        --output "$SOURCE_PARTIAL"; then
        :
    else
        download_exit=$?
        rm -f "$SOURCE_PARTIAL"
        exit "$download_exit"
    fi
    mv "$SOURCE_PARTIAL" "$SOURCE_ARCHIVE"
fi
if [ ! -f "$SOURCE_ARCHIVE" ]; then
    echo "FFmpeg source archive was not found." >&2
    exit 1
fi

rm -rf "$SOURCE_DIR" "$EXTRACTED_DIR"
tar -xf "$SOURCE_ARCHIVE" -C "$CPP_DIR/Engine/ThirdParty"
if [ ! -f "$EXTRACTED_DIR/configure" ]; then
    echo "FFmpeg source folder was not found after extraction." >&2
    exit 1
fi
if [ ! -f "$EXTRACTED_DIR/RELEASE" ]; then
    echo "FFmpeg release version file was not found after extraction." >&2
    exit 1
fi
EXTRACTED_FFMPEG_VERSION=$(cat "$EXTRACTED_DIR/RELEASE")
if [ "$EXTRACTED_FFMPEG_VERSION" != "$FFMPEG_VERSION" ]; then
    echo "FFmpeg release version mismatch: expected $FFMPEG_VERSION, got $EXTRACTED_FFMPEG_VERSION." >&2
    exit 1
fi
mv "$EXTRACTED_DIR" "$SOURCE_DIR"
echo "FFmpeg source is ready: $SOURCE_DIR"
