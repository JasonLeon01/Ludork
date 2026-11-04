from __future__ import annotations

import os
from pathlib import Path
import shutil
import sys


TOOL_ROLES = ("build", "codegen", "assets", "pack", "editor", "check")


def executable_name(role: str) -> str:
    if role not in TOOL_ROLES:
        raise ValueError(f"Unknown toolchain role: {role}")
    return f"ludork-{role}" + (".exe" if os.name == "nt" else "")


def resolve_tool(directory: Path, role: str) -> Path:
    tool = directory.expanduser().resolve() / executable_name(role)
    if not tool.is_file() or not os.access(tool, os.X_OK):
        raise RuntimeError(f"Toolchain executable was not found: {tool}")
    return tool


def resolve_toolchain_directory(directory: Path | None = None) -> Path:
    if directory is None:
        configured = os.environ.get("LUDORK_TOOLCHAIN_DIRECTORY", "").strip()
        if configured:
            directory = Path(configured)
        else:
            invocation = sys.argv[0]
            if not Path(invocation).is_absolute():
                invocation = shutil.which(invocation) or os.path.abspath(invocation)
            directory = Path(invocation).parent
    root = directory.expanduser().resolve()
    for role in TOOL_ROLES:
        resolve_tool(root, role)
    return root
