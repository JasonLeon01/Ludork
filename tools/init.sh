#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"
CPP_DIR=$(absolute_path "${1:-"$PROJECT_ROOT/Game"}")

sh "$TOOLS_DIR/setup_python.sh"
sh "$TOOLS_DIR/build_script_tools.sh"
sh "$TOOLS_DIR/init_cpp_dependencies.sh" "$CPP_DIR"
echo "ScriptTools and C++ dependencies are ready: $CPP_DIR"
