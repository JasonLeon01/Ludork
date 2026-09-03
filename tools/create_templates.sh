#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"
. "$PROJECT_ROOT/versions.conf"
: "${FFMPEG_VERSION:?FFMPEG_VERSION is not set in versions.conf}"
VARIANT=all
if [ "${1:-}" = "--variant" ]; then
    if [ "$#" -lt 2 ]; then
        echo "Usage: tools/create_templates.sh [--variant all|plain|ffmpeg] [Debug|Release] [output-folder]" >&2
        exit 1
    fi
    VARIANT=$2
    shift 2
fi
CONFIG=${1:-Release}
if [ "$#" -gt 2 ] || { [ "$CONFIG" != "Debug" ] && [ "$CONFIG" != "Release" ]; }; then
    echo "Usage: tools/create_templates.sh [--variant all|plain|ffmpeg] [Debug|Release] [output-folder]" >&2
    exit 1
fi
case "$VARIANT" in
    all | plain | ffmpeg) ;;
    *)
        echo "Usage: tools/create_templates.sh [--variant all|plain|ffmpeg] [Debug|Release] [output-folder]" >&2
        exit 1
        ;;
esac

SOURCE_DIR="$PROJECT_ROOT/Sample"
LICENSES_DIR="$PROJECT_ROOT/Licenses"
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
    include_ffmpeg=$2
    rm -rf "$template_dir/Licenses"
    mkdir -p "$template_dir/Licenses"
    cp "$LICENSES_DIR/README.md" "$template_dir/Licenses/README.md"
    cp "$LICENSES_DIR/README_zh_CN.md" "$template_dir/Licenses/README_zh_CN.md"
    for licence_directory in \
        Lua \
        LuaSF \
        SFML \
        sol2 \
        lua-cjson \
        zlib \
        NativeDependencies; do
        mkdir -p "$template_dir/Licenses/$licence_directory"
        rsync -a --delete --exclude '.DS_Store' \
            "$LICENSES_DIR/$licence_directory/" \
            "$template_dir/Licenses/$licence_directory/"
    done
    if [ "$include_ffmpeg" -eq 1 ]; then
        mkdir -p "$template_dir/Licenses/FFmpeg"
        rsync -a --delete --exclude '.DS_Store' \
            "$LICENSES_DIR/FFmpeg/" "$template_dir/Licenses/FFmpeg/"
    fi
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

copy_standalone_files() {
    source_dir=$1
    target_dir=$2
    mkdir -p "$target_dir/.vscode"
    cp "$source_dir/.vscode/settings.json" "$target_dir/.vscode/settings.json"
    cp "$source_dir/.emmyrc.json" "$target_dir/.emmyrc.json"
    cp "$source_dir/.gitignore" "$target_dir/.gitignore"
}

prepare_template_pair() {
    source_template_dir=$1
    standalone_template_dir=$2
    include_ffmpeg=$3
    rm -rf "$source_template_dir" "$standalone_template_dir"
    mkdir -p "$source_template_dir" "$standalone_template_dir"
    copy_cpp_template "$source_template_dir" "$include_ffmpeg"
    chmod +x "$source_template_dir/generate_clion.sh"
    if [ "$include_ffmpeg" -eq 1 ]; then
        ffmpeg_enabled=true
    else
        ffmpeg_enabled=false
    fi
    "$SCRIPT_TOOLS" configure-project-template \
        "$source_template_dir/Main.proj" true "$ffmpeg_enabled"
    copy_runtime_legal_files "$source_template_dir" "$include_ffmpeg"
}

build_template_pair() {
    source_template_dir=$1
    standalone_template_dir=$2
    include_ffmpeg=$3
    dependency_cache=$4
    if [ -n "$dependency_cache" ]; then
        LUDORK_DEPENDENCY_CACHE="$dependency_cache" \
            sh "$TOOLS_DIR/build_standalone.sh" \
            "$source_template_dir" "$standalone_template_dir" "$CONFIG"
    else
        sh "$TOOLS_DIR/build_standalone.sh" \
            "$source_template_dir" "$standalone_template_dir" "$CONFIG"
    fi
    copy_standalone_files "$source_template_dir" "$standalone_template_dir"
    if [ "$include_ffmpeg" -eq 1 ]; then
        ffmpeg_enabled=true
    else
        ffmpeg_enabled=false
    fi
    "$SCRIPT_TOOLS" configure-project-template \
        "$standalone_template_dir/Main.proj" false "$ffmpeg_enabled"
}

finalize_template_pair() {
    source_template_dir=$1
    standalone_template_dir=$2
    include_ffmpeg=$3
    rm -rf "$source_template_dir/build" "$source_template_dir/bin"
    validate_no_ui_preview_host "$source_template_dir"
    validate_no_ui_preview_host "$standalone_template_dir"
    if [ "$include_ffmpeg" -eq 1 ]; then
        echo "C++ FFmpeg source template is ready: $source_template_dir"
        echo "Standalone FFmpeg template is ready: $standalone_template_dir/Main"
    else
        echo "C++ source template is ready: $source_template_dir"
        echo "Standalone template is ready: $standalone_template_dir/Main"
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
for licence_path in \
    README.md \
    README_zh_CN.md \
    Lua \
    LuaSF \
    SFML \
    sol2 \
    lua-cjson \
    zlib \
    NativeDependencies; do
    if [ ! -e "$LICENSES_DIR/$licence_path" ]; then
        echo "Required runtime licence source was not found: $LICENSES_DIR/$licence_path" >&2
        exit 1
    fi
done
if [ "$VARIANT" != "plain" ]; then
    if [ ! -d "$LICENSES_DIR/FFmpeg" ]; then
        echo "Required FFmpeg licence source was not found: $LICENSES_DIR/FFmpeg" >&2
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
fi

case "$VARIANT" in
    plain)
        prepare_template_pair "$CPP_TEMPLATE_DIR" "$STANDALONE_TEMPLATE_DIR" 0
        build_template_pair "$CPP_TEMPLATE_DIR" "$STANDALONE_TEMPLATE_DIR" 0 ""
        finalize_template_pair "$CPP_TEMPLATE_DIR" "$STANDALONE_TEMPLATE_DIR" 0
        ;;
    ffmpeg)
        prepare_template_pair "$CPP_FFMPEG_TEMPLATE_DIR" "$STANDALONE_FFMPEG_TEMPLATE_DIR" 1
        build_template_pair "$CPP_FFMPEG_TEMPLATE_DIR" "$STANDALONE_FFMPEG_TEMPLATE_DIR" 1 ""
        finalize_template_pair "$CPP_FFMPEG_TEMPLATE_DIR" "$STANDALONE_FFMPEG_TEMPLATE_DIR" 1
        ;;
    all)
        prepare_template_pair "$CPP_TEMPLATE_DIR" "$STANDALONE_TEMPLATE_DIR" 0
        prepare_template_pair "$CPP_FFMPEG_TEMPLATE_DIR" "$STANDALONE_FFMPEG_TEMPLATE_DIR" 1
        build_template_pair "$CPP_TEMPLATE_DIR" "$STANDALONE_TEMPLATE_DIR" 0 ""
        build_template_pair "$CPP_FFMPEG_TEMPLATE_DIR" "$STANDALONE_FFMPEG_TEMPLATE_DIR" 1 \
            "$CPP_TEMPLATE_DIR/build/_deps"
        finalize_template_pair "$CPP_TEMPLATE_DIR" "$STANDALONE_TEMPLATE_DIR" 0
        finalize_template_pair "$CPP_FFMPEG_TEMPLATE_DIR" "$STANDALONE_FFMPEG_TEMPLATE_DIR" 1
        ;;
esac
