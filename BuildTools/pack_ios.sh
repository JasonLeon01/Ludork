#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LUDORK_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

PROJ_PATH=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --proj-path)
            PROJ_PATH="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1" >&2
            exit 1
            ;;
    esac
done

if [ -z "${PROJ_PATH}" ]; then
    echo "Usage: $0 --proj-path <game_project_root>" >&2
    exit 1
fi

PROJ_PATH="$(cd "${PROJ_PATH}" && pwd -P)"
PROJECT_NAME="$(basename "${PROJ_PATH}")"
SCRIPTS_DIR="${PROJ_PATH}"
IOS_PYTHON_DIR="${LUDORK_ROOT}/ios_python"
RESOURCE_DIR="${PROJ_PATH}/Assets/System"
GENERATE_SCRIPT="${SCRIPT_DIR}/generateiOSApp.sh"

if [ ! -f "${GENERATE_SCRIPT}" ]; then
    echo "generateiOSApp.sh not found: ${GENERATE_SCRIPT}" >&2
    exit 1
fi

cmd=(bash "${GENERATE_SCRIPT}" "${PROJECT_NAME}" -g "${PROJ_PATH}" "${SCRIPTS_DIR}" "${IOS_PYTHON_DIR}")
if [ -d "${RESOURCE_DIR}" ]; then
    cmd+=(-r "${RESOURCE_DIR}")
fi

echo "Running: ${cmd[*]}"
exec "${cmd[@]}"
