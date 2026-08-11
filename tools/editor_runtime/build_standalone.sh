#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"

if [ "$#" -ne 3 ]; then
    echo "Usage: tools/build_standalone.sh <cpp-folder> <standalone-folder> <Debug|Release>" >&2
    exit 1
fi

CPP_DIR=$(absolute_path "$1")
STANDALONE_DIR=$(absolute_path "$2")
CONFIG=$3
if [ "$CONFIG" != "Debug" ] && [ "$CONFIG" != "Release" ]; then
    echo "Configuration must be Debug or Release." >&2
    exit 1
fi

sh "$TOOLS_DIR/build_cpp.sh" "$CPP_DIR" "$CONFIG"

for resource in Assets Data Scripts; do
    if [ ! -d "$CPP_DIR/$resource" ]; then
        echo "$resource folder was not found: $CPP_DIR/$resource" >&2
        exit 1
    fi
done

mkdir -p "$STANDALONE_DIR"
rsync -a --delete --exclude '.DS_Store' "$CPP_DIR/Assets/" "$STANDALONE_DIR/Assets/"
rsync -a --delete --exclude '.DS_Store' --exclude '*.anim.json' "$CPP_DIR/Data/" "$STANDALONE_DIR/Data/"
rsync -a --delete --exclude '.DS_Store' "$CPP_DIR/Scripts/" "$STANDALONE_DIR/Scripts/"
rsync -a \
    --exclude '.DS_Store' \
    --exclude '*.pdb' \
    --exclude 'UiPreviewHost*' \
    --exclude 'UiPreviewCurveResolver*' \
    "$CPP_DIR/bin/$CONFIG/" "$STANDALONE_DIR/"
if [ -d "$CPP_DIR/Licenses" ]; then
    rsync -a --delete --exclude '.DS_Store' "$CPP_DIR/Licenses/" "$STANDALONE_DIR/Licenses/"
fi
if [ -d "$CPP_DIR/ThirdPartySource" ]; then
    rsync -a --delete --exclude '.DS_Store' "$CPP_DIR/ThirdPartySource/" "$STANDALONE_DIR/ThirdPartySource/"
    if [ -d "$CPP_DIR/cmake/FFmpeg" ]; then
        rsync -a --delete --exclude '.DS_Store' "$CPP_DIR/cmake/FFmpeg/" "$STANDALONE_DIR/ThirdPartySource/FFmpeg-Build/"
    fi
fi
for legal_name in LICENSE.md THIRD_PARTY_NOTICES.md THIRD_PARTY_NOTICES_zh_CN.md; do
    if [ -f "$CPP_DIR/$legal_name" ]; then
        cp "$CPP_DIR/$legal_name" "$STANDALONE_DIR/$legal_name"
    fi
done
find "$STANDALONE_DIR" -depth \
    \( -name 'UiPreviewHost*' -o -name 'UiPreviewCurveResolver*' \) \
    -exec rm -rf {} +

if [ ! -x "$STANDALONE_DIR/Main" ]; then
    echo "Standalone output is missing Main." >&2
    exit 1
fi

echo "Standalone build complete: $STANDALONE_DIR"
