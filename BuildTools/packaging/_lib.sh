#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
VENV="$ROOT/LudorkEnv"

if [ -x "$VENV/Scripts/python.exe" ]; then
    PYTHON="$VENV/Scripts/python.exe"
elif [ -x "$VENV/bin/python3" ]; then
    PYTHON="$VENV/bin/python3"
elif [ -x "$VENV/bin/python" ]; then
    PYTHON="$VENV/bin/python"
else
    PYTHON=""
fi

log() {
    echo "[$(date '+%H:%M:%S')] $*"
}

step() {
    echo "[STEP] $*"
}

warn() {
    echo "[WARNING] $*" >&2
}

err() {
    echo "[ERROR] $*" >&2
    exit 1
}

venv_activate() {
    if [ -f "$VENV/Scripts/activate" ]; then
        . "$VENV/Scripts/activate"
    elif [ -f "$VENV/bin/activate" ]; then
        . "$VENV/bin/activate"
    else
        err "Python venv not found: $VENV"
    fi

    [ -n "$PYTHON" ] || err "Python executable not found in venv: $VENV"
}

pip_ensure_with_exe() {
    local python_exe="$1"
    local package="$2"
    local extra_args="${3:-}"

    if [ -n "$extra_args" ]; then
        step "Updating $package..."
        # shellcheck disable=SC2086
        "$python_exe" -m pip install $extra_args "$package"
        return
    fi

    if ! "$python_exe" -m pip show "$package" >/dev/null 2>&1; then
        step "Installing $package..."
        # shellcheck disable=SC2086
        "$python_exe" -m pip install $extra_args "$package"
    fi
}

pip_ensure() {
    pip_ensure_with_exe "$PYTHON" "$1" "${2:-}"
}
