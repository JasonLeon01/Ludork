from __future__ import annotations

import argparse
import json
import pathlib
import sys

from .pack_error import PackError
from .packaging_names import artifact_name, read_app_name, read_runtime_app_name
from .packaging_metadata import (
    add_channel_arguments,
    add_package_arguments,
    load_package_metadata,
    load_release_version,
    read_package_metadata,
    read_release_version,
)


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="ludork-pack metadata")
    operations = parser.add_subparsers(dest="operation", required=True)
    checking = operations.add_parser("check")
    checking.add_argument("project", type=pathlib.Path)
    add_package_arguments(checking)
    release = operations.add_parser("release-version")
    add_package_arguments(release)
    release_build_info = operations.add_parser("release-build-info")
    release_build_info.add_argument("directory", type=pathlib.Path)
    release_build_info.add_argument("--version", required=True, help="Base release version, such as 1.0.0")
    add_channel_arguments(release_build_info)
    build_info = operations.add_parser("build-info")
    build_info.add_argument("file", type=pathlib.Path)
    build_info.add_argument("--field", required=True,
                            choices=("version", "fullVersion", "dev", "builtAt", "versionCode", "appleBuildVersion"))
    resolving = operations.add_parser("resolve")
    resolving.add_argument("project", type=pathlib.Path)
    resolving.add_argument("output", type=pathlib.Path)
    add_package_arguments(resolving)
    naming = operations.add_parser("app-name")
    naming.add_argument("project", type=pathlib.Path)
    naming.add_argument("--artifact", action="store_true")
    runtime_naming = operations.add_parser("runtime-app-name")
    runtime_naming.add_argument("project", type=pathlib.Path)
    package_name = operations.add_parser("package-name")
    package_name.add_argument("--metadata", type=pathlib.Path, required=True)
    writing = operations.add_parser("write-build-info")
    writing.add_argument("runtime_root", type=pathlib.Path)
    writing.add_argument("--metadata", type=pathlib.Path, required=True)
    parsed = parser.parse_args(arguments)
    try:
        if parsed.operation == "release-version":
            metadata = read_release_version(parsed.version, parsed.dev if parsed.dev is not None else False)
            print(json.dumps(metadata.as_dict(), ensure_ascii=False))
        elif parsed.operation == "release-build-info":
            release = read_release_version(parsed.version, parsed.dev if parsed.dev is not None else False)
            release.write_build_info(parsed.directory)
            print(release.full_version)
        elif parsed.operation == "build-info":
            value = load_release_version(parsed.file).as_dict()[parsed.field]
            print("true" if value is True else "false" if value is False else value)
        elif parsed.operation in {"check", "resolve"}:
            metadata = read_package_metadata(parsed.project.expanduser().resolve(), parsed.version, parsed.dev)
            if parsed.operation == "resolve":
                parsed.output.write_text(json.dumps(metadata.as_dict(), ensure_ascii=False), encoding="utf-8")
        elif parsed.operation == "app-name":
            name = read_app_name(parsed.project.expanduser().resolve())
            print(artifact_name(name) if parsed.artifact else name)
        elif parsed.operation == "runtime-app-name":
            print(read_runtime_app_name(parsed.project.expanduser().resolve()))
        elif parsed.operation == "package-name":
            print(load_package_metadata(parsed.metadata).package_name)
        elif parsed.operation == "write-build-info":
            load_package_metadata(parsed.metadata).write_build_info(parsed.runtime_root)
        return 0
    except PackError as error:
        print(str(error), file=sys.stderr)
        return error.exit_code
    except OSError as error:
        print(str(error), file=sys.stderr)
        return 1

