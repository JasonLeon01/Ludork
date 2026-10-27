from __future__ import annotations

import argparse
import json
import pathlib
import sys

from .editor_versions import read_editor_versions
from .pack_error import PackError
from .packaging_metadata import load_release_version


def write_csharp(config: pathlib.Path, output: pathlib.Path,
                 build_info: pathlib.Path | None, version: str | None) -> None:
    release = load_release_version(build_info) if build_info is not None else None
    versions = read_editor_versions(config, version)
    if release is not None and release.version != versions.base_version:
        raise PackError("Editor build information does not match the requested assembly version.")
    values = {
        "Ludork.BaseVersion": versions.base_version,
        "Ludork.Development": "true" if release is None or release.dev else "false",
        "Ludork.SupportedGameVersions": json.dumps(versions.supported_game_versions, separators=(",", ":")),
    }
    text = "\n".join(
        f"[assembly: System.Reflection.AssemblyMetadata({json.dumps(key)}, {json.dumps(value)})]"
        for key, value in values.items()
    ) + "\n"
    data = text.encode("utf-8")
    if not output.is_file() or output.read_bytes() != data:
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_bytes(data)


def validate_payload(root: pathlib.Path, build_info: pathlib.Path,
                     content_root: pathlib.Path | None = None) -> None:
    release = load_release_version(build_info)
    if not root.is_dir():
        raise PackError(f"Editor package directory was not found: {root}")
    actual = [path for path in root.rglob("*") if path.name.casefold() == "buildinfo.json"]
    expected = [(content_root if content_root is not None else root) / "BuildInfo.json"] if release.dev else []
    if sorted(actual) != sorted(expected):
        raise PackError(f"Unexpected BuildInfo.json layout for editor package: {root}")
    if release.dev and load_release_version(expected[0]) != release:
        raise PackError(f"Editor package build information does not match its snapshot: {root}")


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="ScriptTools editor-build-metadata")
    operations = parser.add_subparsers(dest="operation", required=True)
    generating = operations.add_parser("csharp")
    generating.add_argument("config", type=pathlib.Path)
    generating.add_argument("output", type=pathlib.Path)
    generating.add_argument("--build-info", type=pathlib.Path)
    generating.add_argument("--version")
    validating = operations.add_parser("validate-payload")
    validating.add_argument("root", type=pathlib.Path)
    validating.add_argument("--build-info", type=pathlib.Path, required=True)
    validating.add_argument("--content-root", type=pathlib.Path)
    parsed = parser.parse_args(arguments)
    try:
        if parsed.operation == "csharp":
            write_csharp(parsed.config, parsed.output, parsed.build_info, parsed.version)
        else:
            validate_payload(parsed.root, parsed.build_info, parsed.content_root)
        return 0
    except (PackError, OSError, ValueError) as error:
        print(str(error), file=sys.stderr)
        return 1
