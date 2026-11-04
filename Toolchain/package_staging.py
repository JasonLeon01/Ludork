from __future__ import annotations

import contextlib
import os
import pathlib
import stat
import sys
import tempfile
import threading
from collections.abc import Iterator

from .file_replace import package_lock_candidates, remove_tree, replace_path, wait_until_writable
from .package_process import cancellation_signals


def _directory_identity(path: pathlib.Path) -> tuple[int, int] | None:
    try:
        status = path.lstat()
    except FileNotFoundError:
        return None
    if not stat.S_ISDIR(status.st_mode) or path.is_junction():
        raise OSError(f"Package directory must be a normal directory: {path}")
    return status.st_dev, status.st_ino


def _matches_directory(path: pathlib.Path, identity: tuple[int, int] | None) -> bool:
    if identity is None:
        return False
    try:
        return _directory_identity(path) == identity
    except OSError:
        return False


@contextlib.contextmanager
def package_staging(destination: pathlib.Path) -> Iterator[pathlib.Path]:
    destination = pathlib.Path(os.path.abspath(destination.expanduser()))
    original_identity = _directory_identity(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination = destination.parent.resolve() / destination.name
    signal_scope = (
        cancellation_signals()
        if threading.current_thread() is threading.main_thread()
        else contextlib.nullcontext()
    )
    with signal_scope:
        transaction = pathlib.Path(tempfile.mkdtemp(prefix=f".{destination.name}-package-", dir=destination.parent))
        working = transaction / "working"
        backup = transaction / "original"
        working_identity: tuple[int, int] | None = None
        preserve_transaction = False
        failure: BaseException | None = None
        try:
            yield working
            working_identity = _directory_identity(working)
            if working_identity is None:
                raise RuntimeError(f"Prepared package directory was not found: {working}")
            if _directory_identity(destination) != original_identity:
                raise RuntimeError(f"Package destination changed during preparation: {destination}")
            try:
                if original_identity is not None:
                    wait_until_writable(destination, package_lock_candidates(destination))
                    preserve_transaction = True
                    replace_path(destination, backup)
                    wait_until_writable(backup, package_lock_candidates(backup))
                wait_until_writable(working, package_lock_candidates(working))
                replace_path(working, destination)
                preserve_transaction = False
            except BaseException as error:
                if _matches_directory(destination, working_identity):
                    raise
                if _matches_directory(backup, original_identity):
                    try:
                        replace_path(backup, destination)
                    except BaseException as rollback_error:
                        if not _matches_directory(destination, original_identity):
                            preserve_transaction = True
                            raise RuntimeError(
                                f"Unable to publish package resources: {error}\n"
                                f"Unable to restore {destination}: {rollback_error}\n"
                                f"Recovery files remain at {transaction}"
                            ) from error
                        preserve_transaction = False
                        if isinstance(rollback_error, (KeyboardInterrupt, SystemExit)):
                            raise
                elif original_identity is not None and not _matches_directory(destination, original_identity):
                    preserve_transaction = True
                    raise RuntimeError(
                        f"Unable to publish package resources: {error}\n"
                        f"The original package directory could not be located; recovery files remain at {transaction}"
                    ) from error
                preserve_transaction = False
                raise
        except BaseException as error:
            failure = error
            if _matches_directory(destination, working_identity):
                preserve_transaction = True
                print(
                    f"Package resources were published to {destination}; "
                    f"the operation was interrupted afterwards. Recovery files remain at {transaction}",
                    file=sys.stderr,
                )
            elif preserve_transaction and isinstance(error, (KeyboardInterrupt, SystemExit)):
                print(
                    f"Package recovery was interrupted; recovery files remain at {transaction}",
                    file=sys.stderr,
                )
            raise
        finally:
            if not preserve_transaction:
                try:
                    remove_tree(transaction)
                except BaseException as cleanup_error:
                    published = _matches_directory(destination, working_identity)
                    state = f"Package resources were published to {destination}. " if published else ""
                    if transaction.exists():
                        detail = f"{state}Unable to clean package staging directory {transaction}: {cleanup_error}"
                    else:
                        detail = f"{state}Package staging cleanup completed before the operation was interrupted: {cleanup_error}"
                    if failure is not None:
                        print(detail, file=sys.stderr)
                    elif isinstance(cleanup_error, (KeyboardInterrupt, SystemExit)):
                        print(detail, file=sys.stderr)
                        raise
                    else:
                        raise RuntimeError(detail) from cleanup_error
