#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"
if [ "$#" -ne 1 ]; then
    echo "Usage: tools/init_cpp_dependencies.sh <cpp-folder>" >&2
    exit 1
fi

CPP_DIR=$(absolute_path "$1")
if [ ! -f "$CPP_DIR/CMakeLists.txt" ]; then
    echo "CMakeLists.txt was not found: $CPP_DIR" >&2
    exit 1
fi

DEPS_DIR="$TOOLS_DIR/cpp_dependencies"
CHILD_PIDS=""

cleanup_children() {
    for pid in $CHILD_PIDS; do
        kill "$pid" 2>/dev/null || true
    done
    for pid in $CHILD_PIDS; do
        wait "$pid" 2>/dev/null || true
    done
}

trap 'cleanup_children; exit 130' HUP INT TERM

mkdir -p "$CPP_DIR/Engine/ThirdParty"

sh "$DEPS_DIR/luasf.sh" "$CPP_DIR" &
CHILD_PIDS="$CHILD_PIDS $!"
sh "$DEPS_DIR/sfml.sh" "$CPP_DIR" &
CHILD_PIDS="$CHILD_PIDS $!"
sh "$DEPS_DIR/sol2.sh" "$CPP_DIR" &
CHILD_PIDS="$CHILD_PIDS $!"
sh "$DEPS_DIR/lua.sh" "$CPP_DIR" &
CHILD_PIDS="$CHILD_PIDS $!"
sh "$DEPS_DIR/lua_cjson.sh" "$CPP_DIR" &
CHILD_PIDS="$CHILD_PIDS $!"
sh "$DEPS_DIR/zlib.sh" "$CPP_DIR" &
CHILD_PIDS="$CHILD_PIDS $!"
sh "$DEPS_DIR/ffmpeg.sh" "$CPP_DIR" &
CHILD_PIDS="$CHILD_PIDS $!"

status=0
for pid in $CHILD_PIDS; do
    wait "$pid" || status=1
done
CHILD_PIDS=""
if [ "$status" -ne 0 ]; then
    echo "A C++ dependency download failed." >&2
    exit 1
fi

apply_patch() {
    patch_file=$1
    target_dir=$2
    shift 2

    git_root=$(git -C "$target_dir" rev-parse --show-toplevel 2>/dev/null || true)
    if [ -n "$git_root" ]; then
        case "$target_dir" in
            "$git_root"/*)
                git -C "$git_root" apply --directory="${target_dir#"$git_root"/}" "$@" "$patch_file"
                return
                ;;
        esac
    fi
    (cd "$target_dir" && git apply "$@" "$patch_file")
}

apply_patch_if_needed() {
    label=$1
    patch_file=$2
    target_dir=$3
    shift 3

    echo "Applying $label patch if needed..."
    if apply_patch "$patch_file" "$target_dir" --reverse --check "$@" >/dev/null 2>&1; then
        echo "$label patch is already applied."
        return
    fi
    apply_patch "$patch_file" "$target_dir" --check "$@"
    apply_patch "$patch_file" "$target_dir" "$@"
}

apply_patch_if_needed \
    "LuaSF native value copy" \
    "$PROJECT_ROOT/patches/luasf-value-copy.patch" \
    "$CPP_DIR/Engine/ThirdParty/LuaSF" \
    --unidiff-zero

# The published sol2 headers use CRLF line endings, so this patch has to
# ignore whitespace to match.
apply_patch_if_needed \
    "sol2 PR #1606" \
    "$PROJECT_ROOT/patches/sol2-pr1606.patch" \
    "$CPP_DIR/Engine/ThirdParty/sol2" \
    --ignore-whitespace

echo "C++ dependencies are ready in $CPP_DIR"
