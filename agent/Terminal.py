# -*- encoding: utf-8 -*-

from __future__ import annotations

import os
import re
import subprocess
import sys


_pythonCCommandPattern = re.compile(
    r"^python(?:\.exe)?\s+-c\s+(.+)$",
    re.IGNORECASE | re.DOTALL,
)


def _extractPythonCCode(command: str) -> str | None:
    match = _pythonCCommandPattern.match(command.strip())
    if match is None:
        return None
    code = match.group(1).strip()
    if len(code) >= 2 and code[0] == code[-1] and code[0] in ("'", '"'):
        return code[1:-1]
    return code


def RunTerminal(command: str, cwd: str, timeout: int = 120) -> str:
    command = command.strip()
    pythonCode = _extractPythonCCode(command)
    if pythonCode is not None:
        shellCmd: list[str] = [sys.executable, "-c", pythonCode]
    elif os.name == "nt":
        shellCmd = ["cmd", "/c", command]
    else:
        shellCmd = ["bash", "-c", command]
    try:
        proc = subprocess.run(
            shellCmd,
            cwd=cwd,
            capture_output=True,
            text=True,
            timeout=timeout,
        )
        output = proc.stdout
        if proc.stderr:
            output += "\n" + proc.stderr
        if not output.strip():
            output = f"(exit code: {proc.returncode})"
        return output.strip()
    except subprocess.TimeoutExpired:
        return f"Command timed out after {timeout}s"
    except FileNotFoundError:
        return "Error: Shell not found"
    except Exception as e:
        return f"Error: {e}"
