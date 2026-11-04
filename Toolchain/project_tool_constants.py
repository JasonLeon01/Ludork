from __future__ import annotations

import argparse
import json
import pathlib

from . import packaging_constants as constants


def generate(path: pathlib.Path) -> None:
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
    content = "\n".join(lines).encode("utf-8")
    if not path.is_file() or path.read_bytes() != content:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="ludork-codegen project-tool-constants")
    parser.add_argument("output", type=pathlib.Path)
    args = parser.parse_args(arguments)
    generate(args.output)
    return 0
