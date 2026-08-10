#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"
CPP_DIR=$(absolute_path "${1:-"$PROJECT_ROOT/Sample"}")
SAMPLE_DIR=$(absolute_path "$PROJECT_ROOT/Sample")

sh "$TOOLS_DIR/setup_python.sh"
sh "$TOOLS_DIR/build_script_tools.sh"
sh "$TOOLS_DIR/init_cpp_dependencies.sh" "$CPP_DIR"
if [ "$CPP_DIR" = "$SAMPLE_DIR" ]; then
    sh "$TOOLS_DIR/build_ui_preview_host.sh" Release
    echo "ScriptTools, C++ dependencies, and UiPreviewHost are ready: $CPP_DIR"
else
    echo "ScriptTools and C++ dependencies are ready: $CPP_DIR"
    echo "UiPreviewHost was not built because this init targets a custom C++ project. Run tools/init.sh without a project path to prepare the editor tool."
fi
