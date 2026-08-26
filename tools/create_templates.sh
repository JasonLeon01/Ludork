#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"
. "$PROJECT_ROOT/versions.conf"
: "${FFMPEG_VERSION:?FFMPEG_VERSION is not set in versions.conf}"
CONFIG=${1:-Release}
if [ "$#" -gt 2 ] || { [ "$CONFIG" != "Debug" ] && [ "$CONFIG" != "Release" ]; }; then
    echo "Usage: tools/create_templates.sh [Debug|Release] [output-folder]" >&2
    exit 1
fi

SOURCE_DIR="$PROJECT_ROOT/Sample"
FFMPEG_SOURCE_ARCHIVE="$SOURCE_DIR/ThirdPartySource/ffmpeg-$FFMPEG_VERSION.tar.gz"
if [ "$#" -eq 2 ]; then
    TEMPLATES_DIR=$(absolute_path "$2")
else
    TEMPLATES_DIR="$PROJECT_ROOT/Templates"
fi
CPP_TEMPLATE_DIR="$TEMPLATES_DIR/Cpp"
STANDALONE_TEMPLATE_DIR="$TEMPLATES_DIR/Standalone"
CPP_FFMPEG_TEMPLATE_DIR="$TEMPLATES_DIR/Cpp-ffmpeg"
STANDALONE_FFMPEG_TEMPLATE_DIR="$TEMPLATES_DIR/Standalone-ffmpeg"
SCRIPT_TOOLS="$PROJECT_ROOT/.tools/ScriptTools/ScriptTools"

validate_no_ui_preview_host() {
    template_dir=$1
    forbidden_path=$(find "$template_dir" \
        \( -name 'UiPreviewHost*' -o -name 'UiPreviewCurveResolver*' \) \
        -print -quit)
    if [ -n "$forbidden_path" ]; then
        echo "UI preview host entry was found in a project template: $forbidden_path" >&2
        exit 1
    fi
}

copy_runtime_legal_files() {
    template_dir=$1
    mkdir -p "$template_dir/Licenses"
    rsync -a --delete --exclude '.DS_Store' \
        "$SOURCE_DIR/Licenses/" "$template_dir/Licenses/"
    cp "$SOURCE_DIR/LICENSE.md" "$template_dir/LICENSE.md"
    cp "$SOURCE_DIR/THIRD_PARTY_NOTICES.md" \
        "$template_dir/THIRD_PARTY_NOTICES.md"
    cp "$SOURCE_DIR/THIRD_PARTY_NOTICES_zh_CN.md" \
        "$template_dir/THIRD_PARTY_NOTICES_zh_CN.md"
}

copy_cpp_template() {
    template_dir=$1
    include_ffmpeg=$2
    set -- \
        -a \
        --exclude '.DS_Store' \
        --exclude '.venv/' \
        --exclude 'build/' \
        --exclude 'bin/' \
        --exclude 'Log/' \
        --exclude 'Save/' \
        --exclude '__pycache__/' \
        --exclude '*.anim.json' \
        --exclude '*.py' \
        --exclude '*.pyc' \
        --exclude '*.pyo' \
        --exclude '*.log' \
        --exclude 'Main.ini' \
        --exclude 'Ludork.ini' \
        --exclude 'Ludork-startup-error.log' \
        --exclude '.vs/' \
        --exclude '.idea/' \
        --exclude 'cmake-build-ludork-debug/' \
        --exclude 'CMakeUserPresets.json' \
        --exclude 'generate_vs2022.bat' \
        --exclude 'generate_clion.bat' \
        --exclude 'ThirdPartySource/' \
        --exclude 'UiPreviewHost*' \
        --exclude 'UiPreviewCurveResolver*'
    if [ "$include_ffmpeg" -ne 1 ]; then
        set -- "$@" --exclude 'ffmpeg/'
    fi
    rsync "$@" "$SOURCE_DIR/" "$template_dir/"
    if [ "$include_ffmpeg" -eq 1 ]; then
        mkdir -p "$template_dir/ThirdPartySource"
        cp "$FFMPEG_SOURCE_ARCHIVE" "$template_dir/ThirdPartySource/"
    fi
}

if [ ! -x "$SCRIPT_TOOLS" ]; then
    echo "ScriptTools was not found. Run tools/init.sh first." >&2
    exit 1
fi
if [ ! -f "$SOURCE_DIR/CMakeLists.txt" ] || [ ! -d "$SOURCE_DIR/LuaSF" ] || [ ! -d "$SOURCE_DIR/lua-cjson" ] || [ ! -d "$SOURCE_DIR/zlib" ]; then
    echo "Sample dependencies were not found. Prepare the C++ dependencies before creating templates." >&2
    exit 1
fi
if [ ! -f "$SOURCE_DIR/ffmpeg/configure" ]; then
    echo "FFmpeg source was not found. Run tools/init.sh first." >&2
    exit 1
fi
if [ ! -f "$FFMPEG_SOURCE_ARCHIVE" ]; then
    echo "The distributable FFmpeg source archive was not found. Run tools/init.sh first." >&2
    exit 1
fi

rm -rf "$CPP_TEMPLATE_DIR" "$STANDALONE_TEMPLATE_DIR" \
    "$CPP_FFMPEG_TEMPLATE_DIR" "$STANDALONE_FFMPEG_TEMPLATE_DIR"
mkdir -p "$CPP_TEMPLATE_DIR" "$STANDALONE_TEMPLATE_DIR" \
    "$CPP_FFMPEG_TEMPLATE_DIR" "$STANDALONE_FFMPEG_TEMPLATE_DIR"
copy_cpp_template "$CPP_TEMPLATE_DIR" 0
copy_cpp_template "$CPP_FFMPEG_TEMPLATE_DIR" 1

chmod +x \
    "$CPP_TEMPLATE_DIR/generate_clion.sh" \
    "$CPP_FFMPEG_TEMPLATE_DIR/generate_clion.sh"

"$SCRIPT_TOOLS" configure-project-template "$CPP_TEMPLATE_DIR/Main.proj" true false
"$SCRIPT_TOOLS" configure-project-template "$CPP_FFMPEG_TEMPLATE_DIR/Main.proj" true true
copy_runtime_legal_files "$CPP_TEMPLATE_DIR"
copy_runtime_legal_files "$CPP_FFMPEG_TEMPLATE_DIR"

sh "$TOOLS_DIR/build_standalone.sh" "$CPP_TEMPLATE_DIR" "$STANDALONE_TEMPLATE_DIR" "$CONFIG"
LUDORK_DEPENDENCY_CACHE="$CPP_TEMPLATE_DIR/build/_deps" \
    sh "$TOOLS_DIR/build_standalone.sh" \
    "$CPP_FFMPEG_TEMPLATE_DIR" "$STANDALONE_FFMPEG_TEMPLATE_DIR" "$CONFIG"

copy_standalone_files() {
    source_dir=$1
    target_dir=$2
    mkdir -p "$target_dir/.vscode"
    cp "$source_dir/.vscode/settings.json" "$target_dir/.vscode/settings.json"
    cp "$source_dir/.emmyrc.json" "$target_dir/.emmyrc.json"
    cp "$source_dir/.gitignore" "$target_dir/.gitignore"
}
copy_standalone_files "$CPP_TEMPLATE_DIR" "$STANDALONE_TEMPLATE_DIR"
copy_standalone_files "$CPP_FFMPEG_TEMPLATE_DIR" "$STANDALONE_FFMPEG_TEMPLATE_DIR"
"$SCRIPT_TOOLS" configure-project-template "$STANDALONE_TEMPLATE_DIR/Main.proj" false false
"$SCRIPT_TOOLS" configure-project-template "$STANDALONE_FFMPEG_TEMPLATE_DIR/Main.proj" false true

rm -rf "$CPP_TEMPLATE_DIR/build" "$CPP_TEMPLATE_DIR/bin" \
    "$CPP_FFMPEG_TEMPLATE_DIR/build" "$CPP_FFMPEG_TEMPLATE_DIR/bin"

for template_dir in \
    "$CPP_TEMPLATE_DIR" \
    "$STANDALONE_TEMPLATE_DIR" \
    "$CPP_FFMPEG_TEMPLATE_DIR" \
    "$STANDALONE_FFMPEG_TEMPLATE_DIR"; do
    validate_no_ui_preview_host "$template_dir"
done

echo "C++ source template is ready: $CPP_TEMPLATE_DIR"
echo "Standalone template is ready: $STANDALONE_TEMPLATE_DIR/Main"
echo "C++ FFmpeg source template is ready: $CPP_FFMPEG_TEMPLATE_DIR"
echo "Standalone FFmpeg template is ready: $STANDALONE_FFMPEG_TEMPLATE_DIR/Main"
