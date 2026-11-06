from __future__ import annotations

import os
import pathlib
import shutil
import subprocess

from .tool_process import run


def find_cmake() -> pathlib.Path:
    configured = os.environ.get("LUDORK_CMAKE", "")
    candidates = [configured] if configured else [
        "cmake", "/opt/homebrew/bin/cmake", "/usr/local/bin/cmake", "/Applications/CMake.app/Contents/bin/cmake",
    ]
    for candidate in candidates:
        path = pathlib.Path(candidate) if os.path.isabs(candidate) else pathlib.Path(shutil.which(candidate) or "")
        if path.is_file() and os.access(path, os.X_OK):
            if run([path, "--version"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode == 0:
                return path.absolute()
    raise ValueError("CMake was not found. Set LUDORK_CMAKE to its executable path.")


def parallel_jobs() -> str:
    value = os.environ.get("CMAKE_BUILD_PARALLEL_LEVEL", "") or str(os.cpu_count() or 1)
    if not value.isascii() or not value.isdigit() or int(value) == 0:
        raise ValueError("CMAKE_BUILD_PARALLEL_LEVEL must be a positive integer.")
    return value
