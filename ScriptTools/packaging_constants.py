from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys

from .pack_error import PackError


EXIT_TOOLCHAIN = 20
EXIT_DEVICE = 21
EXIT_SIGNING = 22
EXIT_PROJECT = 23
EXIT_APP_NAME_UNCHANGED = 24
EDITOR_CACHE_DIRECTORY = "EditorCache"
PACKAGE_CACHE_DIRECTORIES = (EDITOR_CACHE_DIRECTORY, "Cache")
COMPILE_LUA_DIRECTORIES_ENVIRONMENT = "LUDORK_PACK_COMPILE_LUA_DIRECTORIES"
EXCLUDED_FILES_ENVIRONMENT = "LUDORK_PACK_EXCLUDED_FILES"
DEFAULT_APP_NAME_PATTERN = (
    r"^[ \t]*local[ \t]+APP_NAME[ \t]*=[ \t]*[\"']LudorkSample[\"'][ \t]*(?:--[^\r\n]*)?\r?$"
)
TEMPLATE_TOKEN_PATTERN = re.compile(r"__LUDORK_[A-Z0-9_]+__")
FILE_BUFFER_SIZE = 1024 * 1024
SCRIPT_GROUP = "Scripts"
RESOURCE_GROUPS = ("Assets", "Data", SCRIPT_GROUP)
RESOURCE_PACKAGES = tuple(f"{name}.ldpak" for name in RESOURCE_GROUPS)
RUNTIME_LEGAL_FILES = (
    "LICENSE.md",
    "THIRD_PARTY_NOTICES.md",
    "THIRD_PARTY_NOTICES_zh_CN.md",
)
MOBILE_DEPENDENCY_NAMES = (
    "flac", "freetype", "harfbuzz", "libssh2", "mbedtls", "ogg", "sheenbidi", "vorbis",
)
COMMON_DEPENDENCY_CACHE_DIRECTORIES = ("build/_deps", "build/Release/_deps", "build/Debug/_deps")
CPP_TEMPLATE_NAMES = ("Cpp", "Cpp-ffmpeg")
STANDALONE_TEMPLATE_NAMES = ("Standalone", "Standalone-ffmpeg")
TEMPLATE_NAMES = CPP_TEMPLATE_NAMES + STANDALONE_TEMPLATE_NAMES
PLAIN_TEMPLATE_NAMES = (CPP_TEMPLATE_NAMES[0], STANDALONE_TEMPLATE_NAMES[0])
FFMPEG_TEMPLATE_NAMES = (CPP_TEMPLATE_NAMES[1], STANDALONE_TEMPLATE_NAMES[1])
NATIVE_LUA_FILES = (
    "stub/Engine.d.lua", "stub/GlobalCore.d.lua", "stub/GlobalFunctions.d.lua",
    "stub/LuaSF.d.lua", "Engine_meta.lua", "GlobalCore_meta.lua", "GlobalFunctions_meta.lua",
)
ARTIFACT_NAME_PATTERN = re.compile(r'[\x00-\x1f\x7f<>:"/\\|?*;]+')
ARTIFACT_NAME_MAX_LENGTH = 80
ARTIFACT_NAME_FALLBACK = "Ludork Game"


def check_app_name(project: pathlib.Path) -> None:
    entry = project / "Scripts" / "Entry.lua"
    if not entry.is_file():
        raise PackError(f"Lua entry script was not found: {entry}", EXIT_PROJECT)
    try:
        text = entry.read_text(encoding="utf-8")
    except (OSError, UnicodeError) as error:
        raise PackError(f"Unable to read Lua entry script: {error}", EXIT_PROJECT) from error
    if re.search(DEFAULT_APP_NAME_PATTERN, text, re.MULTILINE):
        raise PackError(
            "Change APP_NAME in Scripts/Entry.lua from LudorkSample to a name unique to your game before packaging.",
            EXIT_APP_NAME_UNCHANGED,
        )


def write_csharp(path: pathlib.Path) -> None:
    values = {
        "EditorCacheDirectory": EDITOR_CACHE_DIRECTORY,
        "ToolchainExitCode": EXIT_TOOLCHAIN,
        "DeviceExitCode": EXIT_DEVICE,
        "SigningExitCode": EXIT_SIGNING,
        "ProjectExitCode": EXIT_PROJECT,
        "AppNameUnchangedExitCode": EXIT_APP_NAME_UNCHANGED,
        "DefaultAppNamePattern": DEFAULT_APP_NAME_PATTERN,
        "CompileLuaDirectoriesEnvironment": COMPILE_LUA_DIRECTORIES_ENVIRONMENT,
        "ExcludedFilesEnvironment": EXCLUDED_FILES_ENVIRONMENT,
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
        "editor-cache-directory": (EDITOR_CACHE_DIRECTORY,),
        "package-cache-directories": PACKAGE_CACHE_DIRECTORIES,
        "resource-groups": RESOURCE_GROUPS,
        "runtime-legal-files": RUNTIME_LEGAL_FILES,
        "template-names": TEMPLATE_NAMES,
        "cpp-template-names": CPP_TEMPLATE_NAMES,
        "standalone-template-names": STANDALONE_TEMPLATE_NAMES,
        "plain-template-names": PLAIN_TEMPLATE_NAMES,
        "ffmpeg-template-names": FFMPEG_TEMPLATE_NAMES,
        "native-lua-files": NATIVE_LUA_FILES,
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
    parsed = parser.parse_args(arguments)
    try:
        if parsed.operation == "csharp":
            write_csharp(parsed.output)
        elif parsed.operation == "check-app-name":
            check_app_name(parsed.project.expanduser().resolve())
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
