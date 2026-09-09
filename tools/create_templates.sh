#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"
. "$PROJECT_ROOT/versions.conf"
: "${FFMPEG_VERSION:?FFMPEG_VERSION is not set in versions.conf}"
VARIANT=all
NATIVE_CACHE=
CONFIG=Release
OUTPUT_FOLDER=
POSITIONAL_COUNT=0
usage() {
    echo "Usage: tools/create_templates.sh [--variant all|plain|ffmpeg] [--native-cache <folder>] [Debug|Release] [output-folder]" >&2
    exit 1
}
while [ "$#" -gt 0 ]; do
    case "$1" in
        --variant | --native-cache)
            [ "$#" -ge 2 ] && [ -n "$2" ] || usage
            case "$2" in --*) usage ;; esac
            if [ "$1" = "--variant" ]; then VARIANT=$2; else NATIVE_CACHE=$2; fi
            shift 2
            ;;
        --*) usage ;;
        *)
            case "$POSITIONAL_COUNT" in
                0) CONFIG=$1 ;;
                1) OUTPUT_FOLDER=$1 ;;
                *) usage ;;
            esac
            POSITIONAL_COUNT=$((POSITIONAL_COUNT + 1))
            shift
            ;;
    esac
done
if [ "$CONFIG" != "Debug" ] && [ "$CONFIG" != "Release" ]; then usage; fi
case "$VARIANT" in
    all | plain | ffmpeg) ;;
    *) usage ;;
esac

SOURCE_DIR="$PROJECT_ROOT/Sample"
LICENSES_DIR="$PROJECT_ROOT/Licenses"
FFMPEG_SOURCE_ARCHIVE="$SOURCE_DIR/ThirdPartySource/ffmpeg-$FFMPEG_VERSION.tar.gz"
if [ -n "$OUTPUT_FOLDER" ]; then
    TEMPLATES_DIR=$(absolute_path "$OUTPUT_FOLDER")
else
    TEMPLATES_DIR="$PROJECT_ROOT/Templates"
fi
CPP_TEMPLATE_DIR="$TEMPLATES_DIR/Cpp"
STANDALONE_TEMPLATE_DIR="$TEMPLATES_DIR/Standalone"
CPP_FFMPEG_TEMPLATE_DIR="$TEMPLATES_DIR/Cpp-ffmpeg"
STANDALONE_FFMPEG_TEMPLATE_DIR="$TEMPLATES_DIR/Standalone-ffmpeg"
SCRIPT_TOOLS="$PROJECT_ROOT/.tools/ScriptTools/ScriptTools"
# CMake publishes these seven files; all other Scripts content comes from Sample.
GENERATED_SCRIPTS="stub/Engine.d.lua stub/GlobalCore.d.lua stub/GlobalFunctions.d.lua stub/LuaSF.d.lua Engine_meta.lua GlobalCore_meta.lua GlobalFunctions_meta.lua"

physical_path() (
    if [ -d "$1" ]; then
        CDPATH= cd -P -- "$1"
        pwd -P
    else
        path_parent=$(physical_path "$(dirname -- "$1")")
        case "$(basename -- "$1")" in
            .) printf '%s\n' "$path_parent" ;;
            ..) dirname -- "$path_parent" ;;
            *) printf '%s/%s\n' "${path_parent%/}" "$(basename -- "$1")" ;;
        esac
    fi
)

if [ -n "$NATIVE_CACHE" ]; then
    NATIVE_CACHE=$(physical_path "$NATIVE_CACHE")
    for protected_dir in "$TEMPLATES_DIR" "$SOURCE_DIR"; do
        protected_dir=$(physical_path "$protected_dir")
        case "${NATIVE_CACHE%/}/" in "${protected_dir%/}/"*)
            echo "Native cache overlaps templates or Sample: $NATIVE_CACHE" >&2
            exit 1 ;;
        esac
        case "${protected_dir%/}/" in "${NATIVE_CACHE%/}/"*)
            echo "Native cache overlaps templates or Sample: $NATIVE_CACHE" >&2
            exit 1 ;;
        esac
    done
    for cache_variant in plain ffmpeg; do
        if [ -L "$NATIVE_CACHE/$cache_variant" ] || [ -L "$NATIVE_CACHE/$cache_variant/$CONFIG" ]; then
            echo "Native cache entries must not be symbolic links: $NATIVE_CACHE/$cache_variant/$CONFIG" >&2
            exit 1
        fi
    done
fi

native_cache_entry() {
    if [ "$1" -eq 1 ]; then
        printf '%s/ffmpeg/%s\n' "$NATIVE_CACHE" "$CONFIG"
    else
        printf '%s/plain/%s\n' "$NATIVE_CACHE" "$CONFIG"
    fi
}

validate_native_cache() (
    cache_entry=$1
    if [ ! -s "$cache_entry/bin/$CONFIG/Main" ]; then
        echo "Incomplete native cache: $cache_entry/bin/$CONFIG/Main" >&2
        exit 1
    fi
    for generated_script in $GENERATED_SCRIPTS; do
        if [ ! -s "$cache_entry/Scripts/$generated_script" ]; then
            echo "Incomplete native cache: $cache_entry/Scripts/$generated_script" >&2
            exit 1
        fi
    done
    runtime_library=$(find "$cache_entry/bin/$CONFIG" -maxdepth 1 \
        \( -type f -o -type l \) \
        \( -name '*.so' -o -name '*.so.*' -o -name '*.dylib' \) -print -quit)
    if [ -z "$runtime_library" ]; then
        echo "Native cache contains no runtime libraries: $cache_entry" >&2
        exit 1
    fi
    "$SCRIPT_TOOLS" ui-preview validate "$cache_entry"
)

copy_native_outputs() (
    native_source=$1
    native_target=$2
    mkdir -p "$native_target/bin/$CONFIG" "$native_target/Scripts/stub"
    rsync -a "$native_source/bin/$CONFIG/" "$native_target/bin/$CONFIG/"
    for generated_script in $GENERATED_SCRIPTS; do
        cp -p "$native_source/Scripts/$generated_script" "$native_target/Scripts/$generated_script"
    done
    "$SCRIPT_TOOLS" ui-preview copy --runtime-directory "bin/$CONFIG" "$native_source" "$native_target"
)

if [ -n "$NATIVE_CACHE" ]; then
    for cache_variant in plain ffmpeg; do
        if [ "$VARIANT" = all ] || [ "$VARIANT" = "$cache_variant" ]; then
            cache_entry="$NATIVE_CACHE/$cache_variant/$CONFIG"
            if [ -e "$cache_entry" ]; then validate_native_cache "$cache_entry"; fi
        fi
    done
fi

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
        --exclude 'Intermediate/' \
        --exclude '/Temp/' \
        --exclude '/Cache/' \
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
        --exclude '/Binaries/'
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
    set -- "$source_template_dir" "$standalone_template_dir" "$CONFIG"
    if [ -n "$NATIVE_CACHE" ]; then
        cache_entry=$(native_cache_entry "$include_ffmpeg")
        if [ -e "$cache_entry" ] || [ -L "$cache_entry" ]; then
            copy_native_outputs "$cache_entry" "$source_template_dir"
            set -- --use-current-build "$@"
            echo "Reusing native cache: $cache_entry"
        fi
    fi
    if [ -n "$dependency_cache" ]; then
        LUDORK_DEPENDENCY_CACHE="$dependency_cache" \
            sh "$TOOLS_DIR/build_standalone.sh" "$@"
    else
        sh "$TOOLS_DIR/build_standalone.sh" "$@"
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
    if [ -n "$NATIVE_CACHE" ]; then
        cache_entry=$(native_cache_entry "$include_ffmpeg")
        if [ ! -e "$cache_entry" ]; then
            copy_native_outputs "$source_template_dir" "$cache_entry"
            validate_native_cache "$cache_entry"
            echo "Saved native cache: $cache_entry"
        fi
    fi
    rm -rf "$source_template_dir/build" "$source_template_dir/bin" \
        "$source_template_dir/Intermediate" "$source_template_dir/Temp" "$source_template_dir/Cache"
    rm -rf "$source_template_dir/Binaries"
    "$SCRIPT_TOOLS" ui-preview validate "$standalone_template_dir"
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
if [ ! -f "$SOURCE_DIR/CMakeLists.txt" ] || [ ! -d "$SOURCE_DIR/Engine/ThirdParty/LuaSF" ] || [ ! -d "$SOURCE_DIR/Engine/ThirdParty/lua-cjson" ] || [ ! -d "$SOURCE_DIR/Engine/ThirdParty/zlib" ]; then
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
    if [ ! -f "$SOURCE_DIR/Engine/ThirdParty/ffmpeg/configure" ]; then
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
