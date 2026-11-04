from __future__ import annotations

import pathlib
import re
import shlex


def read_versions(path: pathlib.Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for number, line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), 1):
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        key, separator, value = line.partition("=")
        key = key.strip()
        if not separator or re.fullmatch(r"[A-Z][A-Z0-9_]*", key) is None:
            raise ValueError(f"Invalid version assignment at {path}:{number}")
        if key in values:
            raise ValueError(f"Duplicate {key} at {path}:{number}")
        try:
            tokens = shlex.split(value, comments=True, posix=True)
        except ValueError as error:
            raise ValueError(f"Invalid {key} at {path}:{number}: {error}") from error
        if len(tokens) > 1:
            raise ValueError(f"Expected one literal value for {key} at {path}:{number}")
        values[key] = tokens[0] if tokens else ""
    return values
