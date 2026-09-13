from __future__ import annotations

import argparse
import json
import pathlib
import sys

from . import packaging_constants as constants
from .pack_error import PackError
from .packaging_names import artifact_name, prepare_output, read_app_name
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
    checking = operations.add_parser("check-app-name")
    checking.add_argument("project", type=pathlib.Path)
    naming = operations.add_parser("app-name")
    naming.add_argument("project", type=pathlib.Path)
    naming.add_argument("--artifact", action="store_true")
    preparing = operations.add_parser("prepare-output")
    preparing.add_argument("project", type=pathlib.Path)
    preparing.add_argument("dist", type=pathlib.Path)
    parsed = parser.parse_args(arguments)
    try:
        if parsed.operation == "csharp":
            _write_csharp(parsed.output)
        elif parsed.operation == "check-app-name":
            read_app_name(parsed.project.expanduser().resolve())
        elif parsed.operation == "app-name":
            name = read_app_name(parsed.project.expanduser().resolve())
            print(artifact_name(name) if parsed.artifact else name)
        elif parsed.operation == "prepare-output":
            print(prepare_output(parsed.project, parsed.dist))
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
