from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import sys
import tempfile

from . import tool_process
from .cli.catalogue import COMMANDS
from .tool_paths import TOOL_ROLES, executable_name


BUILD_MODE = "standalone"
BUNDLE_LAYOUT = "independent-entrypoints"
MANIFEST_NAME = "runtime-files.json"
SOURCE_STAMP_NAME = "source.sha256"
VERSION_REPORT_NAME = "runtime-versions.txt"


def smoke_bundle(directory: pathlib.Path, working_directory: pathlib.Path) -> None:
    environment = {key: value for key, value in os.environ.items() if key.upper() not in {"PYTHONHOME", "PYTHONPATH"}}
    with tempfile.TemporaryDirectory(prefix="toolchain smoke ", dir=working_directory) as temporary:
        project = pathlib.Path(temporary) / "Main.proj"
        project.write_text('{"Cpp":true}\n', encoding="utf-8")
        for role in TOOL_ROLES:
            probes = [(["--help"], f"ludork-{role}")]
            for command in COMMANDS[role]:
                if command == "project-runtime-mode":
                    probes.append(([command, str(project)], "cpp"))
                else:
                    probes.append(([command, "--help"], ""))
            for arguments, expected in probes:
                result = tool_process.run(
                    [str(directory / executable_name(role)), *arguments],
                    cwd=temporary, env=environment, capture_output=True, text=True, check=False,
                )
                if result.returncode != 0 or expected not in result.stdout:
                    raise RuntimeError(
                        f"Toolchain {role} {' '.join(arguments)} smoke check failed: {result.stdout}{result.stderr}"
                    )


def _file_states(root: pathlib.Path) -> dict[str, dict[str, object]]:
    states: dict[str, dict[str, object]] = {}
    for path in sorted(root.rglob("*"), key=lambda path: path.relative_to(root).as_posix().encode("utf-8")):
        relative = path.relative_to(root).as_posix()
        if relative == MANIFEST_NAME:
            continue
        if path.name == "__pycache__" or path.name.endswith((".build", ".onefile-build", ".dist")):
            raise ValueError(f"Build output is not part of a runtime bundle: {path}")
        if path.suffix.lower() in {".py", ".pyc", ".pyo", ".c", ".cpp", ".h", ".o", ".obj", ".pdb"}:
            raise ValueError(f"Source or compiler output is not part of a runtime bundle: {path}")
        if path.is_symlink():
            target = path.readlink()
            if target.is_absolute() or not path.resolve().is_relative_to(root) or not path.exists():
                raise ValueError(f"Runtime bundle link leaves its directory or is broken: {path}")
            states[relative] = {"kind": "link", "target": target.as_posix()}
        elif path.is_dir():
            states[relative] = {"kind": "directory"}
        elif path.is_file():
            with path.open("rb") as stream:
                digest = hashlib.file_digest(stream, "sha256").hexdigest()
            states[relative] = {
                "kind": "file",
                "size": path.stat().st_size,
                "sha256": digest,
                "executable": bool(path.stat().st_mode & 0o111),
            }
        else:
            raise ValueError(f"Unsupported runtime bundle entry: {path}")
    return states


def write_manifest(directory: pathlib.Path) -> None:
    root = directory.resolve()
    value = {"formatVersion": 2, "buildMode": BUILD_MODE, "layout": BUNDLE_LAYOUT, "files": _file_states(root)}
    (root / MANIFEST_NAME).write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8", newline="\n")


def validate_bundle(directory: pathlib.Path, expected_source_hash: str | None = None) -> None:
    if directory.is_symlink():
        raise ValueError(f"Runtime bundle directory must not be a link: {directory}")
    root = directory.resolve()
    if not root.is_dir():
        raise ValueError(f"Runtime bundle directory was not found: {root}")
    for role in TOOL_ROLES:
        executable = root / executable_name(role)
        if executable.is_symlink() or not executable.is_file() or not os.access(executable, os.X_OK):
            raise ValueError(f"Runtime bundle entry must be a regular executable: {executable}")
    for name in (SOURCE_STAMP_NAME, VERSION_REPORT_NAME, MANIFEST_NAME):
        if not (root / name).is_file():
            raise ValueError(f"Runtime bundle metadata is missing: {root / name}")
    if expected_source_hash is not None and (root / SOURCE_STAMP_NAME).read_text(encoding="utf-8").strip() != expected_source_hash:
        raise ValueError("Runtime bundle was built from different inputs")
    value = json.loads((root / MANIFEST_NAME).read_text(encoding="utf-8"))
    if (not isinstance(value, dict) or value.get("formatVersion") != 2
            or value.get("buildMode") != BUILD_MODE or value.get("layout") != BUNDLE_LAYOUT):
        raise ValueError("Unsupported runtime bundle manifest")
    states = _file_states(root)
    if value.get("files") != states:
        raise ValueError("Runtime bundle files are missing, changed or unexpected")
    entry_hashes: dict[str, str] = {}
    for role in TOOL_ROLES:
        name = executable_name(role)
        digest = str(states[name]["sha256"])
        if digest in entry_hashes:
            raise ValueError(f"Runtime bundle entries must be independent: {entry_hashes[digest]} and {name}")
        entry_hashes[digest] = name
    if sys.platform == "darwin":
        from .runtime_bundle_macos import validate_dependencies

        validate_dependencies(root)


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="ludork-build runtime-bundle")
    parser.add_argument("operation", choices=("validate",))
    parser.add_argument("directory", type=pathlib.Path)
    parsed = parser.parse_args(arguments)
    try:
        validate_bundle(parsed.directory)
        print("Toolchain runtime bundle is complete.")
        return 0
    except (OSError, ValueError, RuntimeError) as error:
        print(str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
