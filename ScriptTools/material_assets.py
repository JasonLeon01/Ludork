from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import re
import sys


FORMAT_VERSION = 1
GENERATOR = "Ludork Material Export"
SOURCE_ROOT = pathlib.Path("Data/Materials")
GENERATED_ROOT = pathlib.Path("Assets/Shaders/MaterialGenerated")
EXPORT_RECORD = GENERATED_ROOT / "export.json"
PREVIEW_MANIFEST = pathlib.Path("EditorCache/Preview.json")
PROGRAM_FILES = (
    "vertex.vert", "color.frag", "surface.frag", "transmission.frag", "source.frag", "program.json",
)


class MaterialAssetError(RuntimeError):
    pass


def _is_link(path: pathlib.Path) -> bool:
    return path.is_symlink() or bool(getattr(path, "is_junction", lambda: False)())


def _check_path(root: pathlib.Path, relative: pathlib.Path) -> None:
    current = root
    for part in relative.parts:
        current /= part
        if _is_link(current):
            raise MaterialAssetError(f"Material export does not follow links: {current}")


def _files(root: pathlib.Path, relative: pathlib.Path) -> list[pathlib.Path]:
    _check_path(root, relative)
    directory = root / relative
    if not directory.exists():
        return []
    if not directory.is_dir():
        raise MaterialAssetError(f"Material directory is occupied by a file: {directory}")
    result: list[pathlib.Path] = []
    case_paths: dict[str, str] = {}
    for parent, directories, files in os.walk(directory, followlinks=False):
        for name in sorted([*directories, *files]):
            path = pathlib.Path(parent) / name
            if _is_link(path):
                raise MaterialAssetError(f"Material export does not follow links: {path}")
            key = path.relative_to(root).as_posix()
            if "\\" in key or ":" in key or any(ord(character) < 32 for character in key):
                raise MaterialAssetError(f"Invalid material path: {key!r}")
            folded = key.casefold()
            if folded in case_paths:
                raise MaterialAssetError(
                    f"Material paths conflict on case-insensitive filesystems: {case_paths[folded]}, {key}"
                )
            case_paths[folded] = key
            if name in files:
                if not path.is_file():
                    raise MaterialAssetError(f"Unsupported material filesystem entry: {path}")
                result.append(path.relative_to(root))
    return sorted(result)


def _expected_outputs(materials: list[pathlib.Path]) -> dict[str, tuple[pathlib.Path, str]]:
    entries: dict[str, tuple[pathlib.Path, str]] = {}

    def add(path: pathlib.Path, kind: str) -> None:
        for parent in reversed(path.parents):
            if parent == pathlib.Path("."):
                continue
            claim(parent, "directory")
        claim(path, kind)

    def claim(path: pathlib.Path, kind: str) -> None:
        key = path.as_posix().casefold()
        previous = entries.get(key)
        if previous is not None:
            previous_path, previous_kind = previous
            if previous_path != path or previous_kind == "file" or kind == "file" or (
                previous_kind != "directory" and kind != "directory"
            ):
                raise MaterialAssetError(
                    "Material output paths conflict after removing .json extensions: "
                    f"{previous_path} ({previous_kind}), {path} ({kind})"
                )
            if kind == "directory":
                return
        entries[key] = path, kind

    bases = [GENERATED_ROOT / "Builtin/Default"] + [
        GENERATED_ROOT / "Materials" / path.relative_to(SOURCE_ROOT).with_suffix("") for path in materials
    ]
    layers = [pathlib.Path("single")] + [
        pathlib.Path("layers") / path.relative_to(SOURCE_ROOT).with_suffix("") for path in materials
    ]
    add(EXPORT_RECORD, "file")
    for base in bases:
        add(base, "material directory")
        add(base / "resource.json", "file")
    for base in bases:
        for layer in layers:
            program = base / layer
            add(program, "program directory")
            for name in PROGRAM_FILES:
                add(program / name, "file")
    return entries


def generation_manifest(project_root: pathlib.Path) -> dict[str, object]:
    root = project_root.expanduser().resolve()
    if not root.is_dir():
        raise MaterialAssetError(f"Material project directory does not exist: {root}")
    try:
        materials: list[pathlib.Path] = []
        for relative in _files(root, SOURCE_ROOT):
            if relative.name.lower() == ".json":
                raise MaterialAssetError(f"Material file must have a name before .json: {relative}")
            if relative.suffix.lower() != ".json":
                continue
            if relative.suffix != ".json":
                raise MaterialAssetError(f"Material file extension must be lowercase .json: {relative}")
            materials.append(relative)
        inputs = {
            path.as_posix(): hashlib.sha256((root / path).read_bytes()).hexdigest()
            for path in materials
        }
        _check_path(root, PREVIEW_MANIFEST)
        preview = root / PREVIEW_MANIFEST
        inputs[PREVIEW_MANIFEST.as_posix()] = (
            hashlib.sha256(preview.read_bytes()).hexdigest() if preview.is_file() else "missing"
        )
        entries = _expected_outputs(materials)
        expected = {path for path, kind in entries.values() if kind == "file"}
        existing = set(_files(root, GENERATED_ROOT))
        for path in existing:
            planned = entries.get(path.as_posix().casefold())
            if planned is not None and planned[1] != "file":
                raise MaterialAssetError(f"Material output directory is occupied by a file: {path}")
        for path in expected:
            if (root / path).is_dir():
                raise MaterialAssetError(f"Material output file is occupied by a directory: {path}")
        return {
            "inputs": inputs,
            "materialPaths": [path.as_posix() for path in materials],
            "expectedOutputPaths": [path.as_posix() for path in sorted(expected)],
            "outputPaths": [path.as_posix() for path in sorted(expected | existing)],
        }
    except OSError as exception:
        raise MaterialAssetError(f"Material export could not access project files: {exception}") from exception


def _hashes(value: object, label: str, *, allow_missing: bool = False) -> dict[str, str]:
    if not isinstance(value, dict) or any(
        not isinstance(path, str)
        or not isinstance(digest, str)
        or (re.fullmatch(r"[0-9a-fA-F]{64}", digest) is None and not (allow_missing and digest == "missing"))
        for path, digest in value.items()
    ):
        raise MaterialAssetError(f"Invalid material export {label}; run editor Export again.")
    return {path: digest.lower() for path, digest in value.items()}


def validate_generated(project_root: pathlib.Path) -> None:
    root = project_root.expanduser().resolve()
    manifest = generation_manifest(root)
    record_path = root / EXPORT_RECORD
    try:
        record = json.loads(record_path.read_bytes())
    except (OSError, UnicodeError, ValueError) as exception:
        raise MaterialAssetError(
            f"Material export record is missing or invalid: {record_path}. Run editor Export first."
        ) from exception
    if (
        not isinstance(record, dict)
        or type(record.get("formatVersion")) is not int
        or record["formatVersion"] != FORMAT_VERSION
        or record.get("generator") != GENERATOR
    ):
        raise MaterialAssetError("Unsupported material export format; run editor Export again.")
    inputs = _hashes(record.get("inputs"), "inputs", allow_missing=True)
    expected_inputs = manifest["inputs"]
    source_prefix = SOURCE_ROOT.as_posix() + "/"
    if {
        path: digest for path, digest in inputs.items() if path.startswith(source_prefix)
    } != {
        path: digest for path, digest in expected_inputs.items() if path.startswith(source_prefix)
    }:
        raise MaterialAssetError("Material source files changed; run editor Export again.")
    outputs = _hashes(record.get("outputs"), "outputs")
    expected = set(manifest["expectedOutputPaths"]) - {EXPORT_RECORD.as_posix()}
    if outputs.keys() != expected:
        raise MaterialAssetError("Material export output list is incomplete or obsolete; run editor Export again.")
    if manifest["outputPaths"] != manifest["expectedOutputPaths"]:
        raise MaterialAssetError("Obsolete material export files remain; run editor Export again.")
    for path, digest in outputs.items():
        try:
            actual = hashlib.sha256((root / path).read_bytes()).hexdigest()
        except OSError as exception:
            raise MaterialAssetError(f"Material export file is missing: {path}. Run editor Export again.") from exception
        if actual != digest:
            raise MaterialAssetError(f"Material export file changed: {path}. Run editor Export again.")


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="ScriptTools material-assets")
    parser.add_argument("operation", choices=("manifest", "validate"))
    parser.add_argument("project_root", type=pathlib.Path)
    parsed = parser.parse_args(arguments)
    try:
        if parsed.operation == "manifest":
            print(json.dumps(generation_manifest(parsed.project_root), ensure_ascii=False))
        else:
            validate_generated(parsed.project_root)
            print("Material export is up to date")
    except MaterialAssetError as exception:
        print(str(exception), file=sys.stderr)
        return 1
    return 0
