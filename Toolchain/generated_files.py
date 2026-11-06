from __future__ import annotations

import os
import pathlib
import stat
import tempfile

from .file_replace import remove_file, replace_path


class GeneratedFileOwnershipError(ValueError):
    pass


def has_generated_marker(data: bytes, marker: bytes, *, exact_line: bool = False) -> bool:
    contents = data.removeprefix(b"\xef\xbb\xbf")
    if exact_line:
        return contents.splitlines()[:1] == [marker]
    return contents.startswith(marker)


def prepare_generated_file(
    path: pathlib.Path, data: bytes, *, mode_from: pathlib.Path | None = None
) -> None:
    path.write_bytes(data)
    if mode_from is not None and mode_from.exists():
        path.chmod(stat.S_IMODE(mode_from.stat().st_mode))


def write_if_different(path: pathlib.Path, data: bytes) -> bool:
    target = path.resolve()
    if target.exists() and target.read_bytes() == data:
        return False
    target.parent.mkdir(parents=True, exist_ok=True)
    previous_mode = stat.S_IMODE(target.stat().st_mode) if target.exists() else None
    with tempfile.TemporaryDirectory(prefix=".ludork-generated-", dir=target.parent) as directory:
        prepared = pathlib.Path(directory) / "output"
        prepare_generated_file(prepared, data, mode_from=target)
        try:
            replace_path(prepared, target)
        except OSError as error:
            if os.name == "nt" and previous_mode is not None and target.exists():
                try:
                    target.chmod(previous_mode)
                except OSError as restore_error:
                    error.add_note(f"Could not restore permissions for {target}: {restore_error}")
            raise
    return True


def write_generated_file(
    path: pathlib.Path, data: bytes, *, marker: bytes,
    exact_line: bool = False, description: str = "generated file",
) -> bool:
    if path.exists() and not has_generated_marker(path.read_bytes(), marker, exact_line=exact_line):
        raise GeneratedFileOwnershipError(f"refusing to overwrite hand-written {description}: {path}")
    return write_if_different(path, data)


def remove_generated_file(
    path: pathlib.Path, *, marker: bytes, exact_line: bool = False,
    missing_ok: bool = False, description: str = "generated file",
) -> bool:
    try:
        contents = path.read_bytes()
    except FileNotFoundError:
        if missing_ok:
            return False
        raise
    if not has_generated_marker(contents, marker, exact_line=exact_line):
        raise GeneratedFileOwnershipError(f"refusing to remove hand-written stale {description}: {path}")
    remove_file(path, missing_ok=missing_ok)
    return True
