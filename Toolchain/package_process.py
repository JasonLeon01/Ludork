from __future__ import annotations

import pathlib
import subprocess
from collections.abc import Mapping, Sequence

from .pack_error import PackError
from . import tool_process


def run(command: Sequence[str | pathlib.Path] | str, *, cwd: pathlib.Path | None = None,
        env: Mapping[str, str] | None = None, input: bytes | None = None,
        capture: bool = False, quiet: bool = False, check: bool = True,
        timeout: float | None = None) -> subprocess.CompletedProcess[bytes]:
    arguments = command if isinstance(command, str) else [str(value) for value in command]
    result = tool_process.run(
        arguments, cwd=cwd, env=env, input=input,
        stdout=subprocess.PIPE if capture else subprocess.DEVNULL if quiet else None,
        stderr=subprocess.PIPE if capture else None, timeout=timeout,
    )
    result.stdout = result.stdout or b""
    result.stderr = result.stderr or b""
    if check and result.returncode:
        detail = (result.stderr or result.stdout).decode("utf-8", errors="replace").strip()
        program = "Command" if isinstance(arguments, str) else arguments[0]
        raise PackError(f"{program} failed with exit code {result.returncode}."
                        + (f"\n{detail}" if detail else ""),
                        result.returncode if result.returncode > 0 else 130)
    return result
