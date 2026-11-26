from __future__ import annotations

import argparse
import contextlib
import os
import pathlib
import shutil
import sys
import tempfile
import threading
from collections.abc import Callable, Iterator
from concurrent.futures import FIRST_COMPLETED, CancelledError, ThreadPoolExecutor, wait
from itertools import islice

from .file_replace import remove_file, remove_tree, replace_path
from .file_workers import process_worker_count
from .resource_constants import LUA_SOURCE_EXTENSION, LUA_COMPILED_EXTENSION
from .tool_process import cancellation_signals, run


def resolve_luac(configured: str | None = None) -> pathlib.Path:
    candidates: list[pathlib.Path] = []
    if configured:
        candidates.append(pathlib.Path(configured).expanduser())
    environment_luac = os.environ.get("LUDORK_LUAC", "").strip()
    if environment_luac:
        candidates.append(pathlib.Path(environment_luac).expanduser())
    executable_dir = pathlib.Path(sys.argv[0]).resolve().parent
    executable_name = "luac.exe" if os.name == "nt" else "luac"
    candidates.extend(
        (
            executable_dir.parent / executable_name,
            executable_dir.parent / "Lua" / executable_name,
        )
    )
    for candidate in candidates:
        resolved = candidate.resolve()
        if resolved.is_file():
            return resolved
    raise RuntimeError(
        "Host luac was not found. Run tools/init and keep luac in the editor tools directory."
    )


def lua_source_paths(scripts_dir: pathlib.Path) -> list[pathlib.Path]:
    return sorted(
        path
        for path in scripts_dir.rglob("*" + LUA_SOURCE_EXTENSION)
        if path.is_file() and not path.name.endswith(".d.lua")
    )


def _compile_script(luac: pathlib.Path, script: pathlib.Path, temporary: pathlib.Path,
                    check_cancelled: Callable[[], None]) -> None:
    result = run([luac, "-s", "-o", temporary, script], cancel_check=check_cancelled)
    if result.returncode:
        raise RuntimeError(f"luac failed with exit code {result.returncode}: {script}")
    with temporary.open("rb") as stream:
        if stream.read(4) != b"\x1bLua":
            raise RuntimeError(f"luac did not produce Lua bytecode: {script}")


@contextlib.contextmanager
def _protect_sources(root: pathlib.Path, files: list[tuple[pathlib.Path, pathlib.Path, pathlib.Path]]) -> Iterator[None]:
    backup_root = pathlib.Path(tempfile.mkdtemp(prefix=".luac-sources-", dir=root))
    keep_backups = False
    try:
        for script, _, _ in files:
            backup = backup_root / script.relative_to(root)
            backup.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(script, backup, follow_symlinks=False)
        try:
            yield
        except BaseException as error:
            try:
                for script, destination, temporary in files:
                    if not os.path.lexists(script):
                        shutil.copy2(backup_root / script.relative_to(root), script, follow_symlinks=False)
                    if not temporary.exists():
                        remove_file(destination, missing_ok=True)
            except BaseException as rollback_error:
                keep_backups = True
                raise RuntimeError(f"Lua bytecode publication failed: {error}\n"
                                   f"Unable to restore sources: {rollback_error}\n"
                                   f"Original scripts remain at {backup_root}") from rollback_error
            raise
    finally:
        if not keep_backups:
            remove_tree(backup_root)


def compile_scripts(scripts_dir: pathlib.Path, luac: pathlib.Path, *, protect_sources: bool = False) -> int:
    scripts = lua_source_paths(scripts_dir)
    if not scripts:
        raise RuntimeError(f"No Lua scripts were found: {scripts_dir}")
    destinations = [script.with_suffix(LUA_COMPILED_EXTENSION) for script in scripts]
    for destination in destinations:
        if destination.exists():
            raise RuntimeError(
                f"Lua bytecode destination already exists: {destination}"
            )
    temporaries = [
        destination.with_name(f".{destination.name}.tmp")
        for destination in destinations
    ]
    for temporary in temporaries:
        if temporary.exists():
            remove_file(temporary)
    stopped = threading.Event()

    def check_cancelled() -> None:
        if stopped.is_set():
            raise CancelledError

    worker_count = min(len(scripts), process_worker_count())
    jobs = zip(scripts, temporaries, strict=True)
    try:
        with ThreadPoolExecutor(max_workers=worker_count) as executor:
            try:
                pending = set()
                while True:
                    for script, temporary in islice(jobs, worker_count - len(pending)):
                        pending.add(executor.submit(_compile_script, luac, script, temporary, check_cancelled))
                    if not pending:
                        break
                    completed, pending = wait(pending, timeout=0.2, return_when=FIRST_COMPLETED)
                    for future in completed:
                        future.result()
            except BaseException:
                stopped.set()
                raise
        files = list(zip(scripts, destinations, temporaries, strict=True))
        with _protect_sources(scripts_dir, files) if protect_sources else contextlib.nullcontext():
            for script, destination, temporary in files:
                replace_path(temporary, destination)
                remove_file(script)
    except BaseException:
        for temporary in temporaries:
            remove_file(temporary, missing_ok=True)
        raise
    return len(scripts)


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="ludork-assets compile-lua")
    parser.add_argument("--luac")
    parser.add_argument("scripts_directory", type=pathlib.Path)
    parsed = parser.parse_args(arguments)
    scripts_dir = parsed.scripts_directory.resolve()
    if not scripts_dir.is_dir():
        parser.error(f"Scripts directory was not found: {scripts_dir}")
    luac = resolve_luac(parsed.luac)
    try:
        with cancellation_signals():
            count = compile_scripts(scripts_dir, luac, protect_sources=True)
    except KeyboardInterrupt:
        print("Lua compilation cancelled.", file=sys.stderr)
        return 130
    print(f"Compiled and renamed {count} Lua scripts with {luac}")
    return 0
