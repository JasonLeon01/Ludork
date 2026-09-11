from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import sys


BUILD_MODE = "standalone"
MANIFEST_NAME = "runtime-files.json"
SOURCE_STAMP_NAME = "source.sha256"
VERSION_REPORT_NAME = "runtime-versions.txt"


def executable_name() -> str:
    return "ScriptTools.exe" if sys.platform == "win32" else "ScriptTools"


def _file_states(root: pathlib.Path) -> dict[str, dict[str, object]]:
    states: dict[str, dict[str, object]] = {}
    for path in sorted(root.rglob("*")):
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
    value = {"formatVersion": 1, "buildMode": BUILD_MODE, "files": _file_states(root)}
    (root / MANIFEST_NAME).write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def validate_bundle(directory: pathlib.Path, expected_source_hash: str | None = None) -> None:
    if directory.is_symlink():
        raise ValueError(f"Runtime bundle directory must not be a link: {directory}")
    root = directory.resolve()
    if not root.is_dir():
        raise ValueError(f"Runtime bundle directory was not found: {root}")
    executable = root / executable_name()
    if not executable.is_file() or not os.access(executable, os.X_OK):
        raise ValueError(f"Runtime bundle executable is missing: {executable}")
    for name in (SOURCE_STAMP_NAME, VERSION_REPORT_NAME, MANIFEST_NAME):
        if not (root / name).is_file():
            raise ValueError(f"Runtime bundle metadata is missing: {root / name}")
    if expected_source_hash is not None and (root / SOURCE_STAMP_NAME).read_text(encoding="utf-8").strip() != expected_source_hash:
        raise ValueError("Runtime bundle was built from different inputs")
    value = json.loads((root / MANIFEST_NAME).read_text(encoding="utf-8"))
    if not isinstance(value, dict) or value.get("formatVersion") != 1 or value.get("buildMode") != BUILD_MODE:
        raise ValueError("Unsupported runtime bundle manifest")
    if value.get("files") != _file_states(root):
        raise ValueError("Runtime bundle files are missing, changed or unexpected")


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="ScriptTools runtime-bundle")
    parser.add_argument("operation", choices=("validate",))
    parser.add_argument("directory", type=pathlib.Path)
    parsed = parser.parse_args(arguments)
    try:
        validate_bundle(parsed.directory)
        print("ScriptTools runtime bundle is complete.")
        return 0
    except (OSError, ValueError, RuntimeError) as error:
        print(str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
