# -*- encoding: utf-8 -*-

from __future__ import annotations

import os
import subprocess


def RunTerminal(command: str, cwd: str, timeout: int = 120) -> str:
    if os.name == "nt":
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
