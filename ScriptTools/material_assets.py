from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import re
import shutil
import sys


FORMAT_VERSION = 1
GENERATOR = "Ludork Material Export"
SOURCE_ROOT = pathlib.Path("Data/Materials")
GENERATED_ROOT = pathlib.Path("Assets/Shaders/MaterialGenerated")
EXPORT_RECORD = GENERATED_ROOT / "export.json"
PREVIEW_MANIFEST = pathlib.Path("EditorCache/Preview.json")
COMBINATIONS_FILE = pathlib.Path("MaterialExport.json")
CONTENT_ROOTS = tuple(pathlib.Path("Data") / name for name in ("Maps", "Tilesets", "AutoTiles"))
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


def _read_object(root: pathlib.Path, path: pathlib.Path, inputs: dict[str, str]) -> dict:
    contents = (root / path).read_bytes()
    inputs[path.as_posix()] = hashlib.sha256(contents).hexdigest()
    try:
        value = json.loads(contents)
    except (ValueError, UnicodeError) as exception:
        raise MaterialAssetError(f"Invalid material export input {path}: {exception}") from exception
    if not isinstance(value, dict):
        raise MaterialAssetError(f"Material export input must be an object: {path}")
    return value


def _material_reference(value: object, materials: set[str], location: str, *, allow_default: bool = True) -> str:
    if not isinstance(value, str) or (not value and not allow_default) or (value and value not in materials):
        raise MaterialAssetError(f"Missing or invalid material reference at {location}: {value!r}")
    return value


def _grid_values(value: object, location: str):
    if not isinstance(value, list):
        raise MaterialAssetError(f"Expected a tile grid at {location}")
    for y, row in enumerate(value):
        if not isinstance(row, list):
            raise MaterialAssetError(f"Expected a tile row at {location}[{y}]")
        for x, cell in enumerate(row):
            if cell is not None:
                yield cell, f"{location}[{y}][{x}]"


def _programs(root: pathlib.Path, materials: list[pathlib.Path], inputs: dict[str, str]) -> list[tuple[str, str]]:
    material_paths = {path.as_posix() for path in materials}
    programs = {(path, "") for path in material_paths | {""}}
    content = {}
    for directory in CONTENT_ROOTS[1:]:
        content[directory.name] = {
            path.relative_to(directory).with_suffix("").as_posix(): (path, _read_object(root, path, inputs))
            for path in _files(root, directory) if path.suffix == ".json"
        }
    for path in _files(root, CONTENT_ROOTS[0]):
        if path.suffix != ".json":
            continue
        document = _read_object(root, path, inputs)
        if path.name == "_world.json":
            continue
        layers = document.get("layers", {})
        if not isinstance(layers, dict):
            raise MaterialAssetError(f"Expected map layers at {path}")
        for name, layer in layers.items():
            location = f"{path.as_posix()}.layers.{name}"
            if not isinstance(layer, dict):
                raise MaterialAssetError(f"Expected a map layer at {location}")
            layer_material = _material_reference(layer.get("material", ""), material_paths, location + ".material")
            if not layer_material:
                continue
            bases: set[str] = set()
            assignments = None
            for index, cell_location in _grid_values(layer.get("tiles", []), location + ".tiles"):
                if type(index) is not int or index < 0:
                    raise MaterialAssetError(f"Invalid tile index at {cell_location}: {index!r}")
                if assignments is None:
                    key = layer.get("layerTileset")
                    if not isinstance(key, str) or key not in content["Tilesets"]:
                        raise MaterialAssetError(f"Missing tileset at {location}.layerTileset: {key!r}")
                    tileset_path, tileset = content["Tilesets"][key]
                    assignments = tileset.get("materials", {})
                    if not isinstance(assignments, dict):
                        raise MaterialAssetError(f"Expected material assignments at {tileset_path}.materials")
                bases.add(_material_reference(
                    assignments.get(str(index), ""), material_paths,
                    f"{tileset_path.as_posix()}.materials.{index} (used by {cell_location})",
                ))
            for key, cell_location in _grid_values(layer.get("autoTiles", []), location + ".autoTiles"):
                if key == "":
                    continue
                if not isinstance(key, str) or key not in content["AutoTiles"]:
                    raise MaterialAssetError(f"Missing autotile at {cell_location}: {key!r}")
                autotile_path, autotile = content["AutoTiles"][key]
                bases.add(_material_reference(
                    autotile.get("material", ""), material_paths,
                    f"{autotile_path.as_posix()}.material (used by {cell_location})",
                ))
            programs.update((base, layer_material) for base in bases)
    _check_path(root, COMBINATIONS_FILE)
    if (root / COMBINATIONS_FILE).exists():
        document = _read_object(root, COMBINATIONS_FILE, inputs)
        combinations = document.get("additionalCombinations")
        if not isinstance(combinations, list):
            raise MaterialAssetError(f"Expected additionalCombinations array in {COMBINATIONS_FILE}")
        for index, pair in enumerate(combinations):
            location = f"{COMBINATIONS_FILE}.additionalCombinations[{index}]"
            if not isinstance(pair, dict):
                raise MaterialAssetError(f"Expected a material combination at {location}")
            programs.add((
                _material_reference(pair.get("material"), material_paths, location + ".material"),
                _material_reference(pair.get("layer"), material_paths, location + ".layer", allow_default=False),
            ))
    else:
        inputs[COMBINATIONS_FILE.as_posix()] = "missing"
    return sorted(programs)


def copy_export_inputs(project_root: pathlib.Path, destination: pathlib.Path) -> None:
    _check_path(project_root, COMBINATIONS_FILE)
    _check_path(destination, COMBINATIONS_FILE)
    source = project_root / COMBINATIONS_FILE
    if source.exists():
        shutil.copy2(source, destination / COMBINATIONS_FILE)
    else:
        (destination / COMBINATIONS_FILE).unlink(missing_ok=True)


def _expected_outputs(
    materials: list[pathlib.Path], programs: list[tuple[str, str]],
) -> dict[str, tuple[pathlib.Path, str]]:
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

    bases = {"": GENERATED_ROOT / "Builtin/Default"} | {
        path.as_posix(): GENERATED_ROOT / "Materials" / path.relative_to(SOURCE_ROOT).with_suffix("")
        for path in materials
    }
    add(EXPORT_RECORD, "file")
    for base in bases.values():
        add(base, "material directory")
        add(base / "resource.json", "file")
    for material, layer in programs:
        suffix = (
            pathlib.Path("layers") / pathlib.Path(layer).relative_to(SOURCE_ROOT).with_suffix("")
            if layer else pathlib.Path("single")
        )
        program = bases[material] / suffix
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
        programs = _programs(root, materials, inputs)
        entries = _expected_outputs(materials, programs)
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
            "programs": [{"material": material, "layer": layer} for material, layer in programs],
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
    if inputs != {path: digest for path, digest in expected_inputs.items() if path != PREVIEW_MANIFEST.as_posix()}:
        raise MaterialAssetError("Material export inputs changed; run editor Export again.")
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
    parser.add_argument("operation", choices=("manifest", "validate", "copy-inputs"))
    parser.add_argument("project_root", type=pathlib.Path)
    parser.add_argument("destination", type=pathlib.Path, nargs="?")
    parsed = parser.parse_args(arguments)
    if (parsed.operation == "copy-inputs") != (parsed.destination is not None):
        parser.error("copy-inputs requires a destination; manifest and validate do not accept one")
    try:
        if parsed.operation == "copy-inputs":
            copy_export_inputs(parsed.project_root.expanduser().resolve(), parsed.destination.expanduser().resolve())
        elif parsed.operation == "manifest":
            print(json.dumps(generation_manifest(parsed.project_root), ensure_ascii=False))
        else:
            validate_generated(parsed.project_root)
            print("Material export is up to date")
    except (MaterialAssetError, OSError) as exception:
        print(str(exception), file=sys.stderr)
        return 1
    return 0
