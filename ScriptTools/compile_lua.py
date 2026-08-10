from __future__ import annotations

import argparse
import os
import pathlib
import subprocess
import sys


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
            executable_dir / executable_name,
            executable_dir.parent / "Lua" / executable_name,
            executable_dir.parent.parent / "Lua" / executable_name,
        )
    )
    for candidate in candidates:
        resolved = candidate.resolve()
        if resolved.is_file():
            return resolved
    raise RuntimeError(
        "Host luac was not found. Run tools/init and keep luac next to ScriptTools."
    )


def lua_source_paths(scripts_dir: pathlib.Path) -> list[pathlib.Path]:
    return sorted(
        path
        for path in scripts_dir.rglob("*.lua")
        if path.is_file() and not path.name.endswith(".d.lua")
    )


def compile_scripts(scripts_dir: pathlib.Path, luac: pathlib.Path) -> int:
    scripts = lua_source_paths(scripts_dir)
    if not scripts:
        raise RuntimeError(f"No Lua scripts were found: {scripts_dir}")
    destinations = [script.with_suffix(".luac") for script in scripts]
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
            temporary.unlink()
    for script, temporary in zip(scripts, temporaries, strict=True):
        result = subprocess.run(
            [str(luac), "-s", "-o", str(temporary), str(script)],
            check=False,
        )
        if result.returncode != 0:
            for pending in temporaries:
                if pending.exists():
                    pending.unlink()
            raise RuntimeError(
                f"luac failed with exit code {result.returncode}: {script}"
            )
        if temporary.read_bytes()[:4] != b"\x1bLua":
            for pending in temporaries:
                if pending.exists():
                    pending.unlink()
            raise RuntimeError(f"luac did not produce Lua bytecode: {script}")
    for script, destination, temporary in zip(
        scripts, destinations, temporaries, strict=True
    ):
        os.replace(temporary, destination)
        script.unlink()
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
