#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"

if [ "$#" -ne 3 ]; then
    echo "Usage: tools/build_standalone.sh <cpp-folder> <standalone-folder> <Debug|Release>" >&2
    exit 1
fi

CPP_DIR=$(absolute_path "$1")
if [ ! -d "$CPP_DIR" ]; then
    echo "C++ project folder was not found: $CPP_DIR" >&2
    exit 1
fi
CPP_DIR=$(CDPATH= cd -P -- "$CPP_DIR" && pwd -P)
standalone_path=$(absolute_path "$2")
if [ -d "$standalone_path" ]; then
    STANDALONE_DIR=$(CDPATH= cd -P -- "$standalone_path" && pwd -P)
else
    standalone_parent=$(CDPATH= cd -P -- "$(dirname -- "$standalone_path")" && pwd -P)
    STANDALONE_DIR="$standalone_parent/$(basename -- "$standalone_path")"
fi
CONFIG=$3
if [ "$CONFIG" != "Debug" ] && [ "$CONFIG" != "Release" ]; then
    echo "Configuration must be Debug or Release." >&2
    exit 1
fi
if [ "$STANDALONE_DIR" = "/" ]; then
    echo "Standalone output must not be the filesystem root." >&2
    exit 1
fi
case "$CPP_DIR" in
    "$STANDALONE_DIR" | "$STANDALONE_DIR"/*)
        echo "Standalone output must not be the C++ project or one of its parent directories: $STANDALONE_DIR" >&2
        exit 1
        ;;
esac
for protected_name in Assets Data Scripts bin build Licenses ThirdPartySource cmake; do
    protected_source="$CPP_DIR/$protected_name"
    case "$STANDALONE_DIR" in
        "$protected_source" | "$protected_source"/*)
            echo "Standalone output overlaps C++ project inputs: $STANDALONE_DIR" >&2
            exit 1
            ;;
    esac
done

sh "$TOOLS_DIR/build_cpp.sh" "$CPP_DIR" "$CONFIG"

for resource in Assets Data Scripts; do
    if [ ! -d "$CPP_DIR/$resource" ]; then
        echo "$resource folder was not found: $CPP_DIR/$resource" >&2
        exit 1
    fi
done
if [ "${LUDORK_VALIDATE_LDPAK_SOURCE:-0}" = "1" ]; then
    SCRIPT_TOOLS=$(resolve_script_tools)
    "$SCRIPT_TOOLS" validate-ldpak-source "$CPP_DIR"
fi

mkdir -p "$STANDALONE_DIR"
rm -rf \
    "$STANDALONE_DIR/Assets" \
    "$STANDALONE_DIR/Data" \
    "$STANDALONE_DIR/Scripts"
rm -f "$STANDALONE_DIR/Scripts.ldpak"
rsync -a --delete --exclude '.DS_Store' "$CPP_DIR/Assets/" "$STANDALONE_DIR/Assets/"
rsync -a --delete --exclude '.DS_Store' --exclude '*.anim.json' "$CPP_DIR/Data/" "$STANDALONE_DIR/Data/"
rsync -a --delete --exclude '.DS_Store' "$CPP_DIR/Scripts/" "$STANDALONE_DIR/Scripts/"
rm -rf "$STANDALONE_DIR/Binaries"
mkdir -p "$STANDALONE_DIR/Binaries"
rsync -a \
    --delete \
    --exclude '.DS_Store' \
    --exclude '*.pdb' \
    --exclude 'UiPreviewHost*' \
    --exclude 'UiPreviewCurveResolver*' \
    "$CPP_DIR/bin/$CONFIG/" "$STANDALONE_DIR/Binaries/"
if [ ! -f "$STANDALONE_DIR/Binaries/Main" ]; then
    echo "Standalone output is missing Binaries/Main." >&2
    exit 1
fi
mv -f "$STANDALONE_DIR/Binaries/Main" "$STANDALONE_DIR/Main"
if [ "$(uname -s)" = "Darwin" ]; then
    if ! command -v install_name_tool >/dev/null 2>&1; then
        echo "install_name_tool was not found." >&2
        exit 1
    fi
    if ! command -v codesign >/dev/null 2>&1; then
        echo "codesign was not found." >&2
        exit 1
    fi
    if otool -l "$STANDALONE_DIR/Main" \
        | grep -Fq 'path @loader_path/../Frameworks '; then
        install_name_tool \
            -delete_rpath '@loader_path/../Frameworks' \
            "$STANDALONE_DIR/Main"
    fi
    if otool -l "$STANDALONE_DIR/Main" \
        | grep -Fq 'path @loader_path '; then
        install_name_tool \
            -delete_rpath '@loader_path' \
            "$STANDALONE_DIR/Main"
    fi
    if ! otool -l "$STANDALONE_DIR/Main" \
        | grep -Fq 'path @loader_path/Binaries '; then
        install_name_tool \
            -add_rpath '@loader_path/Binaries' \
            "$STANDALONE_DIR/Main"
    fi
    codesign --force --sign - "$STANDALONE_DIR/Main"
    codesign --verify --strict "$STANDALONE_DIR/Main"
fi
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

UI_PREVIEW_ENTRY_NAMES="UiPreviewHost UiPreviewCurveResolver"
for entry_name in $UI_PREVIEW_ENTRY_NAMES; do
    find "$STANDALONE_DIR" -depth -name "$entry_name*" -exec rm -rf {} +
done
for entry_name in $UI_PREVIEW_ENTRY_NAMES; do
    forbidden_path=$(find "$STANDALONE_DIR" -name "$entry_name*" -print -quit)
    if [ -n "$forbidden_path" ]; then
        echo "UI preview host entry was found in a standalone build: $forbidden_path" >&2
        exit 1
    fi
done

unexpected_runtime=$(find "$STANDALONE_DIR" \
    -maxdepth 1 \
    \( -type f -o -type l \) \
    \( -name '*.dll' -o -name '*.so' -o -name '*.so.*' -o -name '*.dylib' \) \
    -print \
    -quit)
if [ -n "$unexpected_runtime" ]; then
    echo "Runtime library exists outside Binaries: $unexpected_runtime" >&2
    exit 1
fi
runtime_library=$(find "$STANDALONE_DIR/Binaries" \
    -maxdepth 1 \
    \( -type f -o -type l \) \
    \( -name '*.so' -o -name '*.so.*' -o -name '*.dylib' \) \
    -print \
    -quit)
if [ -z "$runtime_library" ]; then
    echo "Standalone output contains no runtime libraries in Binaries." >&2
    exit 1
fi

if [ ! -f "$STANDALONE_DIR/Main" ]; then
    echo "Standalone output is missing Main." >&2
    exit 1
fi
chmod +x "$STANDALONE_DIR/Main"

echo "Standalone build complete: $STANDALONE_DIR"
