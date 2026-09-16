from __future__ import annotations

import os
import pathlib
import shutil
import stat
import time
from collections.abc import Callable

_RETRY_SECONDS = 10
_INITIAL_DELAY = 0.05
_MAXIMUM_DELAY = 0.4
_WINDOWS_RETRY_ERRORS = (5, 32, 33)
_PACKAGE_LOCK_SUFFIXES = {".dll", ".dylib", ".exe", ".so"}


def retry_file_operation(operation: Callable[[], object]) -> None:
    deadline = time.monotonic() + _RETRY_SECONDS
    delay = _INITIAL_DELAY
    while True:
        try:
            operation()
            return
        except OSError as error:
            if (
                os.name != "nt"
                or (
                    not isinstance(error, PermissionError)
                    and getattr(error, "winerror", None) not in _WINDOWS_RETRY_ERRORS
                )
                or time.monotonic() >= deadline
            ):
                raise
            time.sleep(min(delay, max(0, deadline - time.monotonic())))
            delay = min(delay * 2, _MAXIMUM_DELAY)


def replace_path(source: pathlib.Path, destination: pathlib.Path) -> None:
    def replace() -> None:
        _clear_readonly_file(destination)
        source.replace(destination)

    retry_file_operation(replace)


def remove_file(path: pathlib.Path, *, missing_ok: bool = False) -> None:
    def remove() -> None:
        _clear_readonly_file(path)
        path.unlink(missing_ok=missing_ok)

    retry_file_operation(remove)


def remove_tree(path: pathlib.Path, *, ignore_errors: bool = False) -> None:
    def remove() -> None:
        shutil.rmtree(path, onexc=_clear_readonly_entry)

    if ignore_errors:
        try:
            retry_file_operation(remove)
        except OSError:
            shutil.rmtree(path, ignore_errors=True)
        return
    retry_file_operation(remove)


def wait_until_writable(
    directory: pathlib.Path, files: list[pathlib.Path] | None = None
) -> None:
    if os.name != "nt":
        return

    def check_files() -> None:
        for path in files if files is not None else directory.rglob("*"):
            if path.is_file() and not _is_link(path):
                # Mapped binaries can survive a directory rename but cannot be opened for writing.
                with path.open("r+b"):
                    pass

    retry_file_operation(check_files)


def package_lock_candidates(root: pathlib.Path) -> list[pathlib.Path]:
    return [
        path
        for path in root.rglob("*")
        if path.is_file()
        and not _is_link(path)
        and path.suffix.lower() in _PACKAGE_LOCK_SUFFIXES
    ]


def copy_file(source: str, destination: str) -> None:
    retry_file_operation(lambda: shutil.copy2(source, destination))


def _is_link(path: pathlib.Path) -> bool:
    if path.is_symlink():
        return True
    is_junction = getattr(path, "is_junction", None)
    return bool(is_junction is not None and is_junction())


def _clear_readonly_file(path: pathlib.Path) -> None:
    if os.name != "nt" or _is_link(path) or not path.is_file():
        return
    mode = path.stat().st_mode
    if not mode & stat.S_IWRITE:
        path.chmod(mode | stat.S_IWRITE)


def _clear_readonly_entry(
    function: Callable[..., object], path: str, exception: BaseException
) -> None:
    if os.name != "nt" or (
        not isinstance(exception, PermissionError)
        and getattr(exception, "winerror", None) not in _WINDOWS_RETRY_ERRORS
    ):
        raise exception
    target = pathlib.Path(path)
    try:
        if not _is_link(target):
            target.chmod(stat.S_IWRITE)
    except OSError:
        pass
    function(path)
