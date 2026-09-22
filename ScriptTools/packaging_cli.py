from __future__ import annotations

import argparse
import json
import pathlib
import sys

from . import packaging_constants as constants
from .pack_error import PackError
from .packaging_names import artifact_name, prepare_output, read_app_name
from .packaging_metadata import (
    add_channel_arguments,
    add_package_arguments,
    load_package_metadata,
    load_release_version,
    read_package_metadata,
    read_release_version,
)
from .resource_constants import RESOURCE_GROUPS


def _write_csharp(path: pathlib.Path) -> None:
    values = {
        "EditorCacheDirectory": constants.EDITOR_CACHE_DIRECTORY,
        "ToolchainExitCode": constants.EXIT_TOOLCHAIN,
        "DeviceExitCode": constants.EXIT_DEVICE,
        "SigningExitCode": constants.EXIT_SIGNING,
        "ProjectExitCode": constants.EXIT_PROJECT,
        "AppNameUnchangedExitCode": constants.EXIT_APP_NAME_UNCHANGED,
        "CompileLuaDirectoriesEnvironment": constants.COMPILE_LUA_DIRECTORIES_ENVIRONMENT,
        "ExcludedFilesEnvironment": constants.EXCLUDED_FILES_ENVIRONMENT,
    }
    lines = ["namespace Ludork.Services;", "", "internal static class ProjectToolConstants", "{"]
    for name, value in values.items():
        kind = "string" if isinstance(value, str) else "int"
        lines.append(f"    public const {kind} {name} = {json.dumps(value)};")
    lines.extend(("}", ""))
    text = "\n".join(lines)
    if not path.is_file() or path.read_text(encoding="utf-8") != text:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")


def main(arguments: list[str] | None = None) -> int:
    lists = {
        "editor-cache-directory": (constants.EDITOR_CACHE_DIRECTORY,),
        "package-cache-directories": constants.PACKAGE_CACHE_DIRECTORIES,
        "resource-groups": RESOURCE_GROUPS,
        "runtime-legal-files": constants.RUNTIME_LEGAL_FILES,
        "template-names": constants.TEMPLATE_NAMES,
        "cpp-template-names": constants.CPP_TEMPLATE_NAMES,
        "standalone-template-names": constants.STANDALONE_TEMPLATE_NAMES,
        "plain-template-names": constants.PLAIN_TEMPLATE_NAMES,
        "ffmpeg-template-names": constants.FFMPEG_TEMPLATE_NAMES,
        "native-lua-files": constants.NATIVE_LUA_FILES,
    }
    parser = argparse.ArgumentParser(prog="ScriptTools packaging-constants")
    operations = parser.add_subparsers(dest="operation", required=True)
    csharp = operations.add_parser("csharp")
    csharp.add_argument("output", type=pathlib.Path)
    listing = operations.add_parser("list")
    listing.add_argument("name", choices=tuple(lists))
    listing.add_argument("--separator", choices=("newline", "space"), default="newline")
    listing.add_argument("--windows", action="store_true")
    checking = operations.add_parser("check-package-metadata")
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
    resolving = operations.add_parser("resolve-metadata")
    resolving.add_argument("project", type=pathlib.Path)
    resolving.add_argument("output", type=pathlib.Path)
    add_package_arguments(resolving)
    naming = operations.add_parser("app-name")
    naming.add_argument("project", type=pathlib.Path)
    naming.add_argument("--artifact", action="store_true")
    preparing = operations.add_parser("prepare-output")
    preparing.add_argument("project", type=pathlib.Path)
    preparing.add_argument("dist", type=pathlib.Path)
    preparing.add_argument("--metadata", type=pathlib.Path, required=True)
    package_name = operations.add_parser("package-name")
    package_name.add_argument("--metadata", type=pathlib.Path, required=True)
    writing = operations.add_parser("write-build-info")
    writing.add_argument("runtime_root", type=pathlib.Path)
    writing.add_argument("--metadata", type=pathlib.Path, required=True)
    parsed = parser.parse_args(arguments)
    try:
        if parsed.operation == "csharp":
            _write_csharp(parsed.output)
        elif parsed.operation == "release-version":
            metadata = read_release_version(parsed.version, parsed.dev if parsed.dev is not None else False)
            print(json.dumps(metadata.as_dict(), ensure_ascii=False))
        elif parsed.operation == "release-build-info":
            release = read_release_version(parsed.version, parsed.dev if parsed.dev is not None else False)
            release.write_build_info(parsed.directory)
            print(release.full_version)
        elif parsed.operation == "build-info":
            value = load_release_version(parsed.file).as_dict()[parsed.field]
            print("true" if value is True else "false" if value is False else value)
        elif parsed.operation in {"check-package-metadata", "resolve-metadata"}:
            metadata = read_package_metadata(parsed.project.expanduser().resolve(), parsed.version, parsed.dev)
            if parsed.operation == "resolve-metadata":
                parsed.output.write_text(json.dumps(metadata.as_dict(), ensure_ascii=False), encoding="utf-8")
        elif parsed.operation == "app-name":
            name = read_app_name(parsed.project.expanduser().resolve())
            print(artifact_name(name) if parsed.artifact else name)
        elif parsed.operation == "prepare-output":
            metadata = load_package_metadata(parsed.metadata)
            print(prepare_output(parsed.project, parsed.dist, metadata.package_name))
        elif parsed.operation == "package-name":
            print(load_package_metadata(parsed.metadata).package_name)
        elif parsed.operation == "write-build-info":
            load_package_metadata(parsed.metadata).write_build_info(parsed.runtime_root)
        else:
            values = lists[parsed.name]
            if parsed.windows:
                values = tuple(value.replace("/", "\\") for value in values)
            print((" " if parsed.separator == "space" else "\n").join(values))
        return 0
    except PackError as error:
        print(str(error), file=sys.stderr)
        return error.exit_code
    except OSError as error:
        print(str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
