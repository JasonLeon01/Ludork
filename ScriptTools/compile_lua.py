from __future__ import annotations

import argparse
import os
import pathlib
import subprocess
import sys

from .file_replace import remove_file, replace_path
from .resource_constants import LUA_SOURCE_EXTENSION, LUA_COMPILED_EXTENSION


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


def compile_scripts(scripts_dir: pathlib.Path, luac: pathlib.Path) -> int:
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
    for script, temporary in zip(scripts, temporaries, strict=True):
        result = subprocess.run(
            [str(luac), "-s", "-o", str(temporary), str(script)],
            check=False,
        )
        if result.returncode != 0:
            for pending in temporaries:
                if pending.exists():
                    remove_file(pending, missing_ok=True)
            raise RuntimeError(
                f"luac failed with exit code {result.returncode}: {script}"
            )
        if temporary.read_bytes()[:4] != b"\x1bLua":
            for pending in temporaries:
                if pending.exists():
                    remove_file(pending, missing_ok=True)
            raise RuntimeError(f"luac did not produce Lua bytecode: {script}")
    for script, destination, temporary in zip(
        scripts, destinations, temporaries, strict=True
    ):
        replace_path(temporary, destination)
        remove_file(script)
    return len(scripts)


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="ScriptTools compile-lua")
    parser.add_argument("--luac")
    parser.add_argument("scripts_directory", type=pathlib.Path)
    parsed = parser.parse_args(arguments)
    scripts_dir = parsed.scripts_directory.resolve()
    if not scripts_dir.is_dir():
        parser.error(f"Scripts directory was not found: {scripts_dir}")
    luac = resolve_luac(parsed.luac)
    count = compile_scripts(scripts_dir, luac)
    print(f"Compiled and renamed {count} Lua scripts with {luac}")
    return 0
